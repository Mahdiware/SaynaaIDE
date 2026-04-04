#pragma once

#include <jni.h>

JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1open(JNIEnv* env, jobject thiz);
JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1pcall(
    JNIEnv* env, jobject thiz, jstring functionName, jobjectArray args);
JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1doFile(
    JNIEnv* env, jobject thiz, jstring fileName);
JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1doString(
    JNIEnv* env, jobject thiz, jstring code);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_execute(JNIEnv* env, jobject thiz, jobject context);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackNative(
    JNIEnv* env, jobject thiz, jint callbackId, jobject arg0);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackMethodNative(
    JNIEnv* env, jobject thiz, jint callbackId, jstring methodName, jobjectArray args);
JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackMethodWithResultNative(
    JNIEnv* env, jobject thiz, jint callbackId, jstring methodName, jobjectArray args);
JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getGlobal(
    JNIEnv* env, jobject thiz, jstring name);
JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setGlobal(
    JNIEnv* env, jobject thiz, jstring name, jobject value);
JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setGlobalFromSlot(
    JNIEnv* env, jobject thiz, jstring name, jint slot);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1reserveSlots(
    JNIEnv* env, jobject thiz, jint count);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setSlotNull(
    JNIEnv* env, jobject thiz, jint slot);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setSlotBool(
    JNIEnv* env, jobject thiz, jint slot, jboolean value);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setSlotNumber(
    JNIEnv* env, jobject thiz, jint slot, jdouble value);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setSlotString(
    JNIEnv* env, jobject thiz, jint slot, jstring value);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1newList(
    JNIEnv* env, jobject thiz, jint slot);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1newMap(
    JNIEnv* env, jobject thiz, jint slot);
JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1listInsert(
    JNIEnv* env, jobject thiz, jint listSlot, jint index, jint valueSlot);
JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1mapSet(
    JNIEnv* env, jobject thiz, jint mapSlot, jint keySlot, jint valueSlot);
JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1wrapJavaObject(
    JNIEnv* env, jobject thiz, jint slot, jobject value);
JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getSlotType(
    JNIEnv* env, jobject thiz, jint slot);
JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getSlotBool(
    JNIEnv* env, jobject thiz, jint slot);
JNIEXPORT jdouble JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getSlotNumber(
    JNIEnv* env, jobject thiz, jint slot);
JNIEXPORT jstring JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getSlotString(
    JNIEnv* env, jobject thiz, jint slot);
JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getSlotJavaObject(
    JNIEnv* env, jobject thiz, jint slot);
JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getListSize(
    JNIEnv* env, jobject thiz, jint listSlot);
JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1listGetToSlot(
    JNIEnv* env, jobject thiz, jint listSlot, jint index, jint valueSlot);
JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getMapSize(
    JNIEnv* env, jobject thiz, jint mapSlot);
JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1mapEntryToSlots(
    JNIEnv* env, jobject thiz, jint mapSlot, jint entryIndex, jint keySlot, jint valueSlot);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1close(JNIEnv* env, jobject thiz);
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
