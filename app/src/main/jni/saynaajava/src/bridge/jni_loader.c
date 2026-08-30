#include "saynaa_exports.h"
#include "saynaa_internal.h"
#include "saynaa_jni.h"

static jclass g_PCallResult = NULL;
static jmethodID g_PCallResult_ctor = NULL;

jclass saynaa_get_pcall_result_class(void) {
  return g_PCallResult;
}

jmethodID saynaa_get_pcall_result_ctor(void) {
  return g_PCallResult_ctor;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  (void) reserved;
  JNIEnv* env = NULL;

  if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }

  jclass localCls = (*env)->FindClass(env, "com/saynaa/saynaajava/PCallResult");
  if (localCls == NULL)
    return JNI_ERR;

  g_PCallResult = (jclass) (*env)->NewGlobalRef(env, localCls);
  (*env)->DeleteLocalRef(env, localCls);

  if (g_PCallResult == NULL)
    return JNI_ERR;

  g_PCallResult_ctor = (*env)->GetMethodID(env, g_PCallResult, "<init>", "(ZLjava/lang/String;Ljava/lang/Object;)V");

  if (g_PCallResult_ctor == NULL)
    return JNI_ERR;

  return JNI_VERSION_1_6;
}
