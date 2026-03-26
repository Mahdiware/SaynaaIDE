#pragma once

#include <android/log.h>

#define SAYNAAJAVA_TAG "saynaajava"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, SAYNAAJAVA_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, __VA_ARGS__)