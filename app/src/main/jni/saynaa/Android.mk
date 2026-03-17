LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../Flags.mk

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
SRC_FILES := $(call rwildcard,$(LOCAL_PATH)/src/,*.c)
LOCAL_SRC_FILES := $(SRC_FILES:$(LOCAL_PATH)/%=%)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/src
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../pcre2/src
LOCAL_STATIC_LIBRARIES += pcre2_8

LOCAL_MODULE := saynaa

include $(BUILD_STATIC_LIBRARY)
