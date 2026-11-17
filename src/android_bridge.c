#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <android/log.h>
#include <stdbool.h>
#include <pthread.h>
#include "plug.h"
#include "ipc.h"

// Cache method ids
static jmethodID resolveInvokeMethodId;
jmethodID emitMethodId;

// For callback
__thread JNIEnv *current_env;
__thread jobject current_obj;
__thread jstring current_jid;
static __thread char current_invoke_id[IPC_MAX_ID_LEN];

// Global for emit
JavaVM *global_vm = NULL;
jobject global_obj = NULL;

static JNIEnv *get_jni_env(bool *did_attach) {
    if (did_attach) {
        *did_attach = false;
    }
    if (global_vm == NULL) {
        return NULL;
    }

    JNIEnv *env = NULL;
    jint rs = (*global_vm)->GetEnv(global_vm, (void **)&env, JNI_VERSION_1_6);
    if (rs == JNI_OK) {
        return env;
    }
    if (rs != JNI_EDETACHED) {
        return NULL;
    }
    if ((*global_vm)->AttachCurrentThread(global_vm, &env, NULL) != JNI_OK) {
        return NULL;
    }
    if (did_attach) {
        *did_attach = true;
    }
    return env;
}

static void release_jni_env(bool did_attach) {
    if (did_attach && global_vm != NULL) {
        (*global_vm)->DetachCurrentThread(global_vm);
    }
}

// Helper: emit an event into the JS layer (centralized JNI emit)
void android_emit(const char *event, const char *data) {
    if (global_obj == NULL || emitMethodId == NULL) return;
    bool did_attach = false;
    JNIEnv *env = get_jni_env(&did_attach);
    if (env == NULL) return;

    const char *ev = event ? event : "";
    const char *dt = data ? data : "null";
    jstring jevent = (*env)->NewStringUTF(env, ev);
    jstring jdata = (*env)->NewStringUTF(env, dt);
    if (jevent && jdata) {
        (*env)->CallVoidMethod(env, global_obj, emitMethodId, jevent, jdata);
    }
    if (jevent) (*env)->DeleteLocalRef(env, jevent);
    if (jdata) (*env)->DeleteLocalRef(env, jdata);
    release_jni_env(did_attach);
}

// Helper: send response back to JS via JNI
void android_response(const char *id, const char *response_json) {
    if (global_obj == NULL || resolveInvokeMethodId == NULL) return;
    bool did_attach = false;
    JNIEnv *env = get_jni_env(&did_attach);
    if (env == NULL) return;

    const char *invoke_id = id ? id : "";
    const char *result = response_json ? response_json : "{\"ok\":true}";
    jstring jid = (*env)->NewStringUTF(env, invoke_id);
    jstring jresult = (*env)->NewStringUTF(env, result);
    if (jid && jresult) {
        (*env)->CallVoidMethod(env, global_obj, resolveInvokeMethodId, jid, jresult);
    }
    if (jid) (*env)->DeleteLocalRef(env, jid);
    if (jresult) (*env)->DeleteLocalRef(env, jresult);
    release_jni_env(did_attach);
}

struct InvokeData {
    char *cmd;
    char *payload;
    JavaVM *jvm;
    jobject obj;
    jstring jid;
};

static bool ensure_jni_bindings(JNIEnv *env, jobject obj) {
    if (resolveInvokeMethodId == NULL || emitMethodId == NULL) {
        jclass cls = (*env)->FindClass(env, "__PACKAGE_PATH__/Ipc");
        if (cls == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "{{APP_NAME}}", "Failed to find class");
            return false;
        }

        resolveInvokeMethodId = (*env)->GetMethodID(env, cls, "resolveInvoke", "(Ljava/lang/String;Ljava/lang/String;)V");
        if (resolveInvokeMethodId == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "{{APP_NAME}}", "Failed to find resolveInvoke method");
            return false;
        }

        emitMethodId = (*env)->GetMethodID(env, cls, "emit", "(Ljava/lang/String;Ljava/lang/String;)V");
        if (emitMethodId == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "{{APP_NAME}}", "Failed to find emit method");
            return false;
        }
    }

    if (global_vm == NULL) {
        if ((*env)->GetJavaVM(env, &global_vm) != JNI_OK) {
            __android_log_print(ANDROID_LOG_ERROR, "{{APP_NAME}}", "Failed to get JavaVM");
            return false;
        }
    }

    if (global_obj == NULL) {
        global_obj = (*env)->NewGlobalRef(env, obj);
        if (global_obj == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "{{APP_NAME}}", "Failed to create global object ref");
            return false;
        }
    }

    return true;
}

static void android_set_pending_id(const char *id) {
    if (id == NULL) {
        current_invoke_id[0] = '\0';
        return;
    }
    strncpy(current_invoke_id, id, IPC_MAX_ID_LEN - 1);
    current_invoke_id[IPC_MAX_ID_LEN - 1] = '\0';
}

static void android_clear_pending_id(void) {
    current_invoke_id[0] = '\0';
}

static void android_queue_respond_callback(const char *response) {
    if (current_env == NULL || current_obj == NULL || resolveInvokeMethodId == NULL || current_invoke_id[0] == '\0') {
        return;
    }

    jstring jid = (*current_env)->NewStringUTF(current_env, current_invoke_id);
    jstring jresult = (*current_env)->NewStringUTF(current_env, response ? response : "{\"ok\":true}");
    if (jid && jresult) {
        (*current_env)->CallVoidMethod(current_env, current_obj, resolveInvokeMethodId, jid, jresult);
    }
    if (jid) (*current_env)->DeleteLocalRef(current_env, jid);
    if (jresult) (*current_env)->DeleteLocalRef(current_env, jresult);
}

void respond_callback(const char *response) {
    if (current_env == NULL || current_obj == NULL || current_jid == NULL || resolveInvokeMethodId == NULL) {
        return;
    }
    ipc_log_response(current_invoke_id, response);
    jstring jresult = (*current_env)->NewStringUTF(current_env, response ? response : "{\"ok\":true}");
    if (jresult) {
        (*current_env)->CallVoidMethod(current_env, current_obj, resolveInvokeMethodId, current_jid, jresult);
        (*current_env)->DeleteLocalRef(current_env, jresult);
    }
}

void *async_invoke(void *arg) {
    struct InvokeData *data = (struct InvokeData *)arg;

    // Attach to JVM
    JavaVM *jvm = data->jvm;
    JNIEnv *thread_env;
    if (jvm == NULL || (*jvm)->AttachCurrentThread(jvm, &thread_env, NULL) != JNI_OK) {
        free(data->cmd);
        free(data->payload);
        free(data);
        return NULL;
    }

    // Extract ID from JNI string on this thread
    const char *id = (*thread_env)->GetStringUTFChars(thread_env, data->jid, 0);
    if (id == NULL) {
        (*jvm)->DetachCurrentThread(jvm);
        free(data->cmd);
        free(data->payload);
        free(data);
        return NULL;
    }

    // Set current callback context for this thread
    current_env = thread_env;
    current_obj = data->obj;
    current_jid = data->jid;

    // Set the pending ID before dispatching so the callback can use it
    android_set_pending_id(id);

    if (ipc_dispatch(data->cmd, data->payload, respond_callback)) {
        ipc_process_queue();
    }

    android_clear_pending_id();
    current_env = NULL;
    current_obj = NULL;
    current_jid = NULL;

    // Release the ID string
    (*thread_env)->ReleaseStringUTFChars(thread_env, data->jid, id);

    // Delete global refs
    (*thread_env)->DeleteGlobalRef(thread_env, data->obj);
    (*thread_env)->DeleteGlobalRef(thread_env, data->jid);

    // Detach
    (*jvm)->DetachCurrentThread(jvm);

    // Free
    free(data->cmd);
    free(data->payload);
    free(data);

    return NULL;
}

JNIEXPORT void JNICALL Java___PACKAGE_MANGLED___Ipc_ipc(JNIEnv *env, jobject obj, jstring jurl, jstring jmessage) {
    const char *url = (*env)->GetStringUTFChars(env, jurl, 0);
    const char *message = (*env)->GetStringUTFChars(env, jmessage, 0);

    if (!ensure_jni_bindings(env, obj)) {
        (*env)->ReleaseStringUTFChars(env, jurl, url);
        (*env)->ReleaseStringUTFChars(env, jmessage, message);
        return;
    }

    // For JS-originated messages, queue and process synchronously with a per-invoke id context.
    current_env = env;
    current_obj = obj;
    current_jid = NULL;

    if (ipc_route_external_message(url, message, android_queue_respond_callback)) {
        ipc_process_queue();
    }

    current_env = NULL;
    current_obj = NULL;
    current_jid = NULL;
    android_clear_pending_id();

    // Release strings
    (*env)->ReleaseStringUTFChars(env, jurl, url);
    (*env)->ReleaseStringUTFChars(env, jmessage, message);
}

JNIEXPORT void JNICALL Java___PACKAGE_MANGLED___Ipc_nativeInvoke(JNIEnv *env, jobject obj, jstring jid, jstring jcmd, jstring jpayload) {
    if (!ensure_jni_bindings(env, obj)) {
        return;
    }

    const char *id = (*env)->GetStringUTFChars(env, jid, 0);
    const char *cmd = (*env)->GetStringUTFChars(env, jcmd, 0);
    const char *payload = (*env)->GetStringUTFChars(env, jpayload, 0);
    if (id == NULL || cmd == NULL || payload == NULL) {
        if (id) (*env)->ReleaseStringUTFChars(env, jid, id);
        if (cmd) (*env)->ReleaseStringUTFChars(env, jcmd, cmd);
        if (payload) (*env)->ReleaseStringUTFChars(env, jpayload, payload);
        return;
    }

    // Keep the JNI callback context on the calling thread. The keystore
    // implementation already posts authentication UI work to the main thread.
    current_env = env;
    current_obj = obj;
    current_jid = jid;

    // Set the pending ID before dispatching so the callback can use it
    android_set_pending_id(id);

    if (ipc_dispatch(cmd, payload, respond_callback)) {
        ipc_process_queue();
    }

    android_clear_pending_id();
    current_env = NULL;
    current_obj = NULL;
    current_jid = NULL;

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
    emitMethodId = NULL;

    // Let IPC set per-invoke id context around queue dispatch on Android.
    ipc_register_platform_response_fns(android_set_pending_id, android_clear_pending_id);

    // Initialize plugins (webview is NULL for Android)
    plug_init(NULL);

    return JNI_VERSION_1_6;
}