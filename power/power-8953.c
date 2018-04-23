/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOG_NDEBUG 1

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdlib.h>

#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)
#define LOG_TAG "Tissot PowerHAL"
#include <utils/Log.h>
#include <cutils/trace.h>
#include <hardware/hardware.h>
#include <hardware/power.h>

#include "utils.h"
#include "metadata-defs.h"
#include "hint-data.h"
#include "performance.h"
#include "power-common.h"

static int display_hint_sent;
int launch_handle = -1;
int launch_mode;

#ifdef EXTRA_POWERHAL_HINTS
static int process_cam_preview_hint(void *metadata)
{
    char governor[80];
    struct cam_preview_metadata_t cam_preview_metadata;

    if (get_scaling_governor(governor, sizeof(governor)) == -1) {
        ALOGE("Can't obtain scaling governor.");

        return HINT_NONE;
    }

    /* Initialize encode metadata struct fields */
    memset(&cam_preview_metadata, 0, sizeof(struct cam_preview_metadata_t));
    cam_preview_metadata.state = -1;
    cam_preview_metadata.hint_id = CAM_PREVIEW_HINT_ID;

    if (metadata) {
        if (parse_cam_preview_metadata((char *)metadata, &cam_preview_metadata) ==
            -1) {
            ALOGE("Error occurred while parsing metadata.");
            return HINT_NONE;
        }
    } else {
        return HINT_NONE;
    }

    if (cam_preview_metadata.state == 1) {
        // EAS resources
        /*
         * lower bus BW to save power
         *   0x41810000: low power ceil mpbs = 2500
         *   0x41814000: low power io percent = 50
         */
        int resource_values[] = {0x41810000, 0x9C4, 0x41814000, 0x32};
        
            perform_hint_action(
            cam_preview_metadata.hint_id, resource_values,
            sizeof(resource_values) / sizeof(resource_values[0]));
        ALOGI("Cam Preview hint start");
        return HINT_HANDLED;
    } else if (cam_preview_metadata.state == 0) {
        undo_hint_action(cam_preview_metadata.hint_id);
        ALOGI("Cam Preview hint stop");
        return HINT_HANDLED;
        }
    }
    return HINT_NONE;
}
#endif

static int process_boost(int boost_handle, int duration)
{
	// EAS resources
    char governor[80];
    int eas_launch_resources[] = {  MAX_FREQ_BIG_CORE_0, 0xFFF, 
                                    MAX_FREQ_LITTLE_CORE_0, 0xFFF,
                                    MIN_FREQ_BIG_CORE_0, 0xFFF, 
                                    MIN_FREQ_LITTLE_CORE_0, 0xFFF,
                                    CPUBW_HWMON_MIN_FREQ, 140,   
                                    ALL_CPUS_PWR_CLPS_DIS_V3, 0x1};

    int* launch_resources;
    size_t launch_resources_size;

    launch_resources = eas_launch_resources;
    launch_resources_size = sizeof(eas_launch_resources) / sizeof(eas_launch_resources[0]);
    boost_handle = interaction_with_handle(
        boost_handle, duration, launch_resources_size, launch_resources);
    return boost_handle;
}

static int process_video_encode_hint(void *metadata)
{
    char governor[80];
    static int boost_handle = -1;

    if (get_scaling_governor(governor, sizeof(governor)) == -1) {
        ALOGE("Can't obtain scaling governor.");

        return HINT_NONE;
    }

    if (metadata) {
        int duration = 2000; // boosts 2s for starting encoding
        boost_handle = process_boost(boost_handle, duration);
        ALOGD("LAUNCH ENCODER-ON: %d MS", duration);
        

           // EAS resources
        /* 1. bus DCVS set to V2 config:
         *    0x41810000: low power ceil mpbs - 2500
         *    0x41814000: low power io percent - 50
         * 2. hysteresis optimization
         *    0x4180C000: bus dcvs hysteresis tuning
         *    0x41820000: sample_ms of 10 ms
         */
        int resource_values[] = {0x41810000, 0x9C4, 0x41814000, 0x32,
                                 0x4180C000, 0x0,   0x41820000, 0xA};

            perform_hint_action(DEFAULT_VIDEO_ENCODE_HINT_ID,
                resource_values, sizeof(resource_values)/sizeof(resource_values[0]));
        ALOGI("Video Encode hint start");
        return HINT_HANDLED;
    } else {
        // boost handle is intentionally not released, release_request(boost_handle);
        undo_hint_action(DEFAULT_VIDEO_ENCODE_HINT_ID);
        ALOGI("Video Encode hint stop");
        return HINT_HANDLED;
        }
    return HINT_NONE;
}

static int process_activity_launch_hint(void *data)
{
    // boost will timeout in 5s
    int duration = 5000;
    ATRACE_BEGIN("launch");
    if (sustained_performance_mode || vr_mode) {
        ATRACE_END();
        return HINT_HANDLED;
    }

    ALOGD("LAUNCH HINT: %s", data ? "ON" : "OFF");
    if (data && launch_mode == 0) {
        launch_handle = process_boost(launch_handle, duration);
        if (launch_handle > 0) {
            launch_mode = 1;
            ALOGD("Activity launch hint handled");
            ATRACE_INT("launch_lock", 1);
            ATRACE_END();
            return HINT_HANDLED;
        } else {
            ATRACE_END();
            return HINT_NONE;
        }
    } else if (data == NULL  && launch_mode == 1) {
        release_request(launch_handle);
        ATRACE_INT("launch_lock", 0);
        launch_mode = 0;
        ATRACE_END();
        return HINT_HANDLED;
    }
    ATRACE_END();
    return HINT_NONE;
}

int power_hint_override(power_hint_t hint, void *data)
{
    int ret_val = HINT_NONE;
    switch(hint) {
#ifdef EXTRA_POWERHAL_HINTS
        case POWER_HINT_CAM_PREVIEW:
            ret_val = process_cam_preview_hint(data);
            break;
#endif
        case POWER_HINT_VIDEO_ENCODE:
            ret_val = process_video_encode_hint(data);
            break;
        case POWER_HINT_LAUNCH:
            ret_val = process_activity_launch_hint(data);
            break;
        default:
            break;
    }
    return ret_val;
}

int set_interactive_override(int on)
{
    return HINT_HANDLED; /* Don't excecute this code path, not in use */
    char governor[80];

    if (get_scaling_governor(governor, sizeof(governor)) == -1) {
        ALOGE("Can't obtain scaling governor.");

        return HINT_NONE;
    }

    if (!on) {
        /* Display off */
        if ((strncmp(governor, INTERACTIVE_GOVERNOR, strlen(INTERACTIVE_GOVERNOR)) == 0) &&
            (strlen(governor) == strlen(INTERACTIVE_GOVERNOR))) {
            int resource_values[] = {}; /* dummy node */
            if (!display_hint_sent) {
                perform_hint_action(DISPLAY_STATE_HINT_ID,
                resource_values, sizeof(resource_values)/sizeof(resource_values[0]));
                display_hint_sent = 1;
                ALOGV("Display Off hint start");
                return HINT_HANDLED;
            }
        }
    } else {
        /* Display on */
        if ((strncmp(governor, INTERACTIVE_GOVERNOR, strlen(INTERACTIVE_GOVERNOR)) == 0) &&
            (strlen(governor) == strlen(INTERACTIVE_GOVERNOR))) {
            undo_hint_action(DISPLAY_STATE_HINT_ID);
            display_hint_sent = 0;
            ALOGV("Display Off hint stop");
            return HINT_HANDLED;
        }
    }
    return HINT_NONE;
}
