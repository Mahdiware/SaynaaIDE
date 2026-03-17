#pragma once

VM* vm_from_saynaa(JNIEnv* env, jobject saynaaObject);
void set_vm_ptr_on_saynaa(JNIEnv* env, jobject saynaaObject, jlong ptr);

JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1open(JNIEnv* env, jobject thiz);
JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1pcall(
    JNIEnv* env, jobject thiz, jstring functionName, jobjectArray args);
JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1doFile(
    JNIEnv* env, jobject thiz, jstring fileName);
JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1doString(
    JNIEnv* env, jobject thiz, jstring code);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_executeSnippetWithViewNative(
    JNIEnv* env, jobject thiz, jstring snippet, jobject view);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_execute(JNIEnv* env, jobject thiz, jobject context);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackNative(
    JNIEnv* env, jobject thiz, jint callbackId, jobject arg0);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackMethodNative(
    JNIEnv* env, jobject thiz, jint callbackId, jstring methodName, jobjectArray args);
JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackMethodWithResultNative(
    JNIEnv* env, jobject thiz, jint callbackId, jstring methodName, jobjectArray args);
JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1close(JNIEnv* env, jobject thiz);
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
