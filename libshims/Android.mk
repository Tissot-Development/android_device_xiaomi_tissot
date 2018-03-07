LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
    external/skia/include
LOCAL_SRC_FILES := libshims_skia.c
LOCAL_MODULE := libshims_skia
LOCAL_SHARED_LIBRARIES := libskia
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)