LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../Flags.mk

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa/cli
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa/compiler
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa/runtime
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa/shared
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa/utils

LOCAL_MODULE := saynaajava
LOCAL_SRC_FILES := saynaajava.c
LOCAL_STATIC_LIBRARIES := saynaa pcre2_8
LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)
