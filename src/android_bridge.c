#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <android/log.h>
#include "plug.h"

// Cache method id
static jmethodID resolveInvokeMethodId;

// For callback
static JNIEnv *current_env;
static jobject current_obj;
static jstring current_jid;

void respond_callback(const char *response) {
    jstring jresult = (*current_env)->NewStringUTF(current_env, response);
    (*current_env)->CallVoidMethod(current_env, current_obj, resolveInvokeMethodId, current_jid, jresult);
    (*current_env)->DeleteLocalRef(current_env, jresult);
}

JNIEXPORT void JNICALL Java_com_example_crossweb_Ipc_ipc(JNIEnv *env, jobject obj, jstring jurl, jstring jmessage) {
    const char *url = (*env)->GetStringUTFChars(env, jurl, 0);
    const char *message = (*env)->GetStringUTFChars(env, jmessage, 0);

    // Handle the IPC message
    // For now, stub: print to log
    __android_log_print(ANDROID_LOG_INFO, "CrossWeb", "IPC message: %s from %s", message, url);

    // TODO: Parse message and handle commands, e.g., call plug functions

    // Release strings
    (*env)->ReleaseStringUTFChars(env, jurl, url);
    (*env)->ReleaseStringUTFChars(env, jmessage, message);
}

JNIEXPORT void JNICALL Java_com_example_crossweb_Ipc_nativeInvoke(JNIEnv *env, jobject obj, jstring jid, jstring jcmd, jstring jpayload) {
    // Cache method id if not done
    if (resolveInvokeMethodId == NULL) {
        jclass cls = (*env)->FindClass(env, "com/example/crossweb/Ipc");
        if (cls == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "CrossWeb", "Failed to find class");
            return;
        }
        resolveInvokeMethodId = (*env)->GetMethodID(env, cls, "resolveInvoke", "(Ljava/lang/String;Ljava/lang/String;)V");
        if (resolveInvokeMethodId == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "CrossWeb", "Failed to find method");
            return;
        }
    }

    const char *id = (*env)->GetStringUTFChars(env, jid, 0);
    const char *cmd = (*env)->GetStringUTFChars(env, jcmd, 0);
    const char *payload = (*env)->GetStringUTFChars(env, jpayload, 0);

    // Set globals for callback
    current_env = env;
    current_obj = obj;
    current_jid = jid;

    // Call plugin
    plug_invoke(cmd, payload, respond_callback);

    // Clear globals
    current_env = NULL;
    current_obj = NULL;
    current_jid = NULL;

    // Release strings
    (*env)->ReleaseStringUTFChars(env, jid, id);
    (*env)->ReleaseStringUTFChars(env, jcmd, cmd);
    (*env)->ReleaseStringUTFChars(env, jpayload, payload);
}

// JNI functions for CWebViewClient
JNIEXPORT jstring JNICALL Java_com_example_crossweb_CWebViewClient_assetLoaderDomain(JNIEnv *env, jobject obj) {
    return (*env)->NewStringUTF(env, "appassets.androidplatform.net");
}

JNIEXPORT jboolean JNICALL Java_com_example_crossweb_CWebViewClient_withAssetLoader(JNIEnv *env, jobject obj) {
    return JNI_FALSE;
}

JNIEXPORT jobject JNICALL Java_com_example_crossweb_CWebViewClient_handleRequest(JNIEnv *env, jobject obj, jstring jwebviewId, jobject jrequest, jboolean jisDocumentStartScriptEnabled) {
    // TODO: implement request handling
    return NULL;
}

JNIEXPORT jboolean JNICALL Java_com_example_crossweb_CWebViewClient_shouldOverride(JNIEnv *env, jobject obj, jstring jurl) {
    // TODO: implement URL override logic
    return JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_example_crossweb_CWebViewClient_onPageLoading(JNIEnv *env, jobject obj, jstring jurl) {
    // TODO: handle page loading
}

JNIEXPORT void JNICALL Java_com_example_crossweb_CWebViewClient_onPageLoaded(JNIEnv *env, jobject obj, jstring jurl) {
    // TODO: handle page loaded
}

// JNI functions for CWebView
JNIEXPORT jboolean JNICALL Java_com_example_crossweb_CWebView_shouldOverride(JNIEnv *env, jobject obj, jstring jurl) {
    // TODO: implement URL override logic
    return JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_example_crossweb_CWebView_onEval(JNIEnv *env, jobject obj, jint id, jstring jresult) {
    // TODO: handle eval result
}

// JNI functions for CWebChromeClient
JNIEXPORT void JNICALL Java_com_example_crossweb_CWebChromeClient_handleReceivedTitle(JNIEnv *env, jobject obj, jobject jwebview, jstring jtitle) {
    // TODO: handle received title
}

bool android_keystore_bio_invoke(const char *cmd, const char *payload, RespondCallback respond) {
    // Stub: keystore biometric is handled in Java layer via Ipc.invoke
    respond("{\"ok\":false,\"msg\":\"Keystore handled in Java\"}");
    return true;
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