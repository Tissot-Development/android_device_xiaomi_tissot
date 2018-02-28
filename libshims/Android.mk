LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := libMiCameraHal_shim.c
LOCAL_MODULE := libMiCameraHal_shim
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)