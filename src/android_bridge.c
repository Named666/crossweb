#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <android/log.h>
#include <stdbool.h>
#include <pthread.h>
#include "plug.h"

// Cache method ids
static jmethodID resolveInvokeMethodId;
jmethodID emitMethodId;

// For callback
__thread JNIEnv *current_env;
__thread jobject current_obj;
__thread jstring current_jid;

// Global for emit
JNIEnv *global_env = NULL;
jobject global_obj = NULL;

// Helper: emit an event into the JS layer (centralized JNI emit)
void android_emit(const char *event, const char *data) {
    if (global_env == NULL || global_obj == NULL || emitMethodId == NULL) return;
    const char *ev = event ? event : "";
    const char *dt = data ? data : "null";
    jstring jevent = (*global_env)->NewStringUTF(global_env, ev);
    jstring jdata = (*global_env)->NewStringUTF(global_env, dt);
    (*global_env)->CallVoidMethod(global_env, global_obj, emitMethodId, jevent, jdata);
    (*global_env)->DeleteLocalRef(global_env, jevent);
    (*global_env)->DeleteLocalRef(global_env, jdata);
}

// Helper: send response back to JS via JNI
void android_response(const char *id, const char *response_json) {
    if (global_env == NULL || global_obj == NULL || resolveInvokeMethodId == NULL) return;
    jstring jid = (*global_env)->NewStringUTF(global_env, id);
    jstring jresult = (*global_env)->NewStringUTF(global_env, response_json);
    (*global_env)->CallVoidMethod(global_env, global_obj, resolveInvokeMethodId, jid, jresult);
    (*global_env)->DeleteLocalRef(global_env, jid);
    (*global_env)->DeleteLocalRef(global_env, jresult);
}

struct InvokeData {
    char *id;
    char *cmd;
    char *payload;
    JNIEnv *env;
    jobject obj;
    jstring jid;
};

void respond_callback(const char *response) {
    jstring jresult = (*current_env)->NewStringUTF(current_env, response);
    (*current_env)->CallVoidMethod(current_env, current_obj, resolveInvokeMethodId, current_jid, jresult);
    (*current_env)->DeleteLocalRef(current_env, jresult);
}

void *async_invoke(void *arg) {
    struct InvokeData *data = (struct InvokeData *)arg;

    // Attach to JVM
    JavaVM *jvm;
    (*data->env)->GetJavaVM(data->env, &jvm);
    JNIEnv *thread_env;
    (*jvm)->AttachCurrentThread(jvm, &thread_env, NULL);

    // Set current for callback
    current_env = thread_env;
    current_obj = data->obj;
    current_jid = data->jid;

    // Call plugin
    plug_invoke(data->cmd, data->payload, respond_callback);

    // Delete global refs
    (*thread_env)->DeleteGlobalRef(thread_env, data->obj);
    (*thread_env)->DeleteGlobalRef(thread_env, data->jid);

    // Detach
    (*jvm)->DetachCurrentThread(jvm);

    // Free
    free(data->id);
    free(data->cmd);
    free(data->payload);
    free(data);

    return NULL;
}

JNIEXPORT void JNICALL Java___PACKAGE_MANGLED___Ipc_ipc(JNIEnv *env, jobject obj, jstring jurl, jstring jmessage) {
    const char *url = (*env)->GetStringUTFChars(env, jurl, 0);
    const char *message = (*env)->GetStringUTFChars(env, jmessage, 0);

    // Handle the IPC message
    // For now, stub: print to log
    __android_log_print(ANDROID_LOG_INFO, "{{APP_NAME}}", "IPC message: %s from %s", message, url);

    // TODO: Parse message and handle commands, e.g., call plug functions

    // Release strings
    (*env)->ReleaseStringUTFChars(env, jurl, url);
    (*env)->ReleaseStringUTFChars(env, jmessage, message);
}

JNIEXPORT void JNICALL Java___PACKAGE_MANGLED___Ipc_nativeInvoke(JNIEnv *env, jobject obj, jstring jid, jstring jcmd, jstring jpayload) {
    // Cache method ids if not done
    if (resolveInvokeMethodId == NULL) {
        jclass cls = (*env)->FindClass(env, "__PACKAGE_PATH__/Ipc");
        if (cls == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "{{APP_NAME}}", "Failed to find class");
            return;
        }
        resolveInvokeMethodId = (*env)->GetMethodID(env, cls, "resolveInvoke", "(Ljava/lang/String;Ljava/lang/String;)V");
        if (resolveInvokeMethodId == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "{{APP_NAME}}", "Failed to find resolveInvoke method");
            return;
        }
        emitMethodId = (*env)->GetMethodID(env, cls, "emit", "(Ljava/lang/String;Ljava/lang/String;)V");
        if (emitMethodId == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "{{APP_NAME}}", "Failed to find emit method");
            return;
        }
    }

    // Set global for emit
    if (global_env == NULL) {
        global_env = env;
        global_obj = (*env)->NewGlobalRef(env, obj);
    }

    const char *id = (*env)->GetStringUTFChars(env, jid, 0);
    const char *cmd = (*env)->GetStringUTFChars(env, jcmd, 0);
    const char *payload = (*env)->GetStringUTFChars(env, jpayload, 0);

    // Create global refs for thread safety
    jobject global_obj_ref = (*env)->NewGlobalRef(env, obj);
    jstring global_jid_ref = (*env)->NewGlobalRef(env, jid);

    // Create async invoke
    struct InvokeData *data = malloc(sizeof(struct InvokeData));
    data->id = strdup(id);
    data->cmd = strdup(cmd);
    data->payload = strdup(payload);
    data->env = env;
    data->obj = global_obj_ref;
    data->jid = global_jid_ref;

    pthread_t thread;
    pthread_create(&thread, NULL, async_invoke, data);
    pthread_detach(thread);

    // Release strings
    (*env)->ReleaseStringUTFChars(env, jid, id);
    (*env)->ReleaseStringUTFChars(env, jcmd, cmd);
    (*env)->ReleaseStringUTFChars(env, jpayload, payload);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    // Cache method id later
    resolveInvokeMethodId = NULL;

    // Initialize plugins (webview is NULL for Android)
    plug_init(NULL);

    return JNI_VERSION_1_6;
}