LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../Flags.mk

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa/src
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa/src/cli
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa/src/compiler
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa/src/runtime
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa/src/shared
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../saynaa/src/utils

LOCAL_MODULE := saynaajava
rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
SRC_FILES := $(call rwildcard,$(LOCAL_PATH)/,*.c)
LOCAL_SRC_FILES := $(SRC_FILES:$(LOCAL_PATH)/%=%)
LOCAL_STATIC_LIBRARIES := saynaa pcre2_8
LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)
