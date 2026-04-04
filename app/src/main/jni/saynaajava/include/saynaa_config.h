#pragma once

#include <android/log.h>

#define SAYNAAJAVA_TAG "saynaajava"

#ifdef DEBUG
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, SAYNAAJAVA_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, SAYNAAJAVA_TAG, __VA_ARGS__)
#else
#define LOGD(...) ((void)0)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, __VA_ARGS__)
#define LOGI(...) ((void)0)
#endif