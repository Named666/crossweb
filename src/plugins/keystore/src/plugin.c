#include "plugin.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __ANDROID__
#include <jni.h>
#endif

// Forward declarations for JNI bridge (to be implemented in Android)
#ifdef __ANDROID__
bool android_keystore_invoke(const char *cmd, const char *payload, RespondCallback respond);
#endif

static bool keystore_init(PluginContext *ctx) {
    // Initialization for Android keystore plugin (if needed)
    printf("Keystore plugin initialized\n");
    return true;
}

static bool keystore_invoke(const char *cmd, const char *payload, RespondCallback respond) {
#ifdef __ANDROID__
    // Forward to JNI/Android implementation
    return android_keystore_invoke(cmd, payload, respond);
#else
    respond("{\"ok\":false,\"msg\":\"Keystore not supported on this platform\"}");
    return false;
#endif
}

static void keystore_event(const char *event, const char *data) {
    // No-op for now
}

static void keystore_cleanup(void) {
    // Cleanup if needed
}

Plugin keystore_plugin = {
    .name = "keystore",
    .version = 100,
    .init = keystore_init,
    .invoke = keystore_invoke,
    .event = keystore_event,
    .cleanup = keystore_cleanup
};

#ifdef __ANDROID__
bool android_keystore_invoke(const char *cmd, const char *payload, RespondCallback respond) {
    extern __thread JNIEnv *current_env;
    extern __thread jobject current_obj;

    if (current_env == NULL || current_obj == NULL) {
        respond("{\"ok\":false,\"msg\":\"JNI not initialized\"}");
        return false;
    }

    // Get Context from current_obj (Ipc instance)
    jclass ipcClass = (*current_env)->GetObjectClass(current_env, current_obj);
    jmethodID getContextMethod = (*current_env)->GetMethodID(current_env, ipcClass, "getContext", "()Landroid/content/Context;");
    if (getContextMethod == NULL) {
        respond("{\"ok\":false,\"msg\":\"getContext method not found\"}");
        return false;
    }
    jobject context = (*current_env)->CallObjectMethod(current_env, current_obj, getContextMethod);
    if (context == NULL) {
        respond("{\"ok\":false,\"msg\":\"Failed to get context\"}");
        return false;
    }

    // Clear any pending Java exception from earlier GetMethodID/GetStaticMethodID calls
    if ((*current_env)->ExceptionCheck(current_env)) {
        (*current_env)->ExceptionClear(current_env);
    }

    // Lookup KeystoreManager via the app ClassLoader to avoid FindClass issues on native threads
    // Obtain ClassLoader via current_obj.getClass().getClassLoader()
    jobject classLoader = NULL;
    jmethodID getClassMethod = (*current_env)->GetMethodID(current_env, ipcClass, "getClass", "()Ljava/lang/Class;");
    if (getClassMethod != NULL) {
        jobject classObj = (*current_env)->CallObjectMethod(current_env, current_obj, getClassMethod);
        if (classObj != NULL) {
            jclass classClass = (*current_env)->FindClass(current_env, "java/lang/Class");
            if (classClass != NULL) {
                jmethodID getClassLoaderMethod = (*current_env)->GetMethodID(current_env, classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
                if (getClassLoaderMethod != NULL) {
                    classLoader = (*current_env)->CallObjectMethod(current_env, classObj, getClassLoaderMethod);
                }
                (*current_env)->DeleteLocalRef(current_env, classClass);
            }
            (*current_env)->DeleteLocalRef(current_env, classObj);
        }
    }

    jclass keystoreClass = NULL;
    if (classLoader != NULL) {
        jclass classLoaderClass = (*current_env)->FindClass(current_env, "java/lang/ClassLoader");
        if (classLoaderClass != NULL) {
            jmethodID loadClassMethod = (*current_env)->GetMethodID(current_env, classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
            if (loadClassMethod != NULL) {
                jstring className = (*current_env)->NewStringUTF(current_env, "__PACKAGE__.plugins.keystore.KeystoreManager");
                keystoreClass = (jclass)(*current_env)->CallObjectMethod(current_env, classLoader, loadClassMethod, className);
                (*current_env)->DeleteLocalRef(current_env, className);
            }
            (*current_env)->DeleteLocalRef(current_env, classLoaderClass);
        }
        (*current_env)->DeleteLocalRef(current_env, classLoader);
    }
    // Fallback to FindClass if classLoader approach failed
    if (keystoreClass == NULL) {
        keystoreClass = (*current_env)->FindClass(current_env, "__PACKAGE_PATH__/plugins/keystore/KeystoreManager");
    }
    if (keystoreClass == NULL) {
        respond("{\"ok\":false,\"msg\":\"KeystoreManager class not found\"}");
        return false;
    }

    // Prefer a Context-based setter: call setActivity(Context) if present.
    // This avoids relying on exact Activity return-type signatures from Ipc.getActivity().
    jmethodID setActivityMethod = (*current_env)->GetStaticMethodID(current_env, keystoreClass, "setActivity", "(Landroid/content/Context;)V");
    if (setActivityMethod != NULL) {
        (*current_env)->CallStaticVoidMethod(current_env, keystoreClass, setActivityMethod, context);
        if ((*current_env)->ExceptionCheck(current_env)) {
            (*current_env)->ExceptionClear(current_env);
        }
    }

    jmethodID encryptMethod = (*current_env)->GetStaticMethodID(current_env, keystoreClass, "encrypt", "([B)Ljava/lang/String;");
    jmethodID decryptMethod = (*current_env)->GetStaticMethodID(current_env, keystoreClass, "decrypt", "(Ljava/lang/String;)[B");
    jmethodID saveMethod = (*current_env)->GetStaticMethodID(current_env, keystoreClass, "save", "(Landroid/content/Context;Ljava/lang/String;)V");
    jmethodID loadMethod = (*current_env)->GetStaticMethodID(current_env, keystoreClass, "load", "(Landroid/content/Context;)Ljava/lang/String;");

    if (strcmp(cmd, "encrypt") == 0) {
        size_t len = strlen(payload) / 2;
        jbyteArray bytes = (*current_env)->NewByteArray(current_env, (jsize)len);
        jbyte *tmp = malloc(len);
        for (size_t i = 0; i < len; i++) {
            unsigned int byte;
            sscanf(payload + 2 * i, "%2x", &byte);
            tmp[i] = (jbyte)byte;
        }
        (*current_env)->SetByteArrayRegion(current_env, bytes, 0, (jsize)len, tmp);
        free(tmp);

        jstring encrypted = (jstring)(*current_env)->CallStaticObjectMethod(current_env, keystoreClass, encryptMethod, bytes);
        (*current_env)->DeleteLocalRef(current_env, bytes);
        if (encrypted == NULL) {
            respond("{\"ok\":false,\"msg\":\"Encryption failed\"}");
            (*current_env)->DeleteLocalRef(current_env, keystoreClass);
            return false;
        }
        const char *encryptedStr = (*current_env)->GetStringUTFChars(current_env, encrypted, NULL);
        char response[2048];
        snprintf(response, sizeof(response), "{\"ok\":true,\"encrypted\":\"%s\"}", encryptedStr);
        (*current_env)->ReleaseStringUTFChars(current_env, encrypted, encryptedStr);
        (*current_env)->DeleteLocalRef(current_env, encrypted);
        respond(response);
    } else if (strcmp(cmd, "decrypt") == 0) {
        jstring encrypted = (*current_env)->NewStringUTF(current_env, payload);
        jbyteArray decryptedBytes = (jbyteArray)(*current_env)->CallStaticObjectMethod(current_env, keystoreClass, decryptMethod, encrypted);
        (*current_env)->DeleteLocalRef(current_env, encrypted);
        if (decryptedBytes == NULL) {
            respond("{\"ok\":false,\"msg\":\"Decryption failed\"}");
            (*current_env)->DeleteLocalRef(current_env, keystoreClass);
            return false;
        }
        jsize len = (*current_env)->GetArrayLength(current_env, decryptedBytes);
        jbyte *buf = malloc(len);
        (*current_env)->GetByteArrayRegion(current_env, decryptedBytes, 0, len, buf);
        char hex[2048];
        for (jsize i = 0; i < len; i++) {
            sprintf(hex + 2 * i, "%02x", (unsigned char)buf[i]);
        }
        hex[2 * len] = '\0';
        free(buf);
        (*current_env)->DeleteLocalRef(current_env, decryptedBytes);
        char response[2048];
        snprintf(response, sizeof(response), "{\"ok\":true,\"privateKey\":\"%s\"}", hex);
        respond(response);
    } else if (strcmp(cmd, "save") == 0) {
        jstring encrypted = (*current_env)->NewStringUTF(current_env, payload);
        (*current_env)->CallStaticVoidMethod(current_env, keystoreClass, saveMethod, context, encrypted);
        (*current_env)->DeleteLocalRef(current_env, encrypted);
        respond("{\"ok\":true}");
    } else if (strcmp(cmd, "load") == 0) {
        jstring loaded = (jstring)(*current_env)->CallStaticObjectMethod(current_env, keystoreClass, loadMethod, context);
        if (loaded != NULL) {
            const char *loadedStr = (*current_env)->GetStringUTFChars(current_env, loaded, NULL);
            char response[2048];
            snprintf(response, sizeof(response), "{\"ok\":true,\"encrypted\":\"%s\"}", loadedStr);
            (*current_env)->ReleaseStringUTFChars(current_env, loaded, loadedStr);
            (*current_env)->DeleteLocalRef(current_env, loaded);
            respond(response);
        } else {
            respond("{\"ok\":false,\"msg\":\"No key stored\"}");
        }
    } else if (strcmp(cmd, "get") == 0) {
        jstring loaded = (jstring)(*current_env)->CallStaticObjectMethod(current_env, keystoreClass, loadMethod, context);
        if (loaded != NULL) {
            const char *loadedStr = (*current_env)->GetStringUTFChars(current_env, loaded, NULL);
            char response[2048];
            snprintf(response, sizeof(response), "{\"ok\":true,\"encrypted\":\"%s\"}", loadedStr);
            (*current_env)->ReleaseStringUTFChars(current_env, loaded, loadedStr);
            (*current_env)->DeleteLocalRef(current_env, loaded);
            respond(response);
        } else {
            respond("{\"ok\":false,\"msg\":\"No key stored\"}");
        }
    } else {
        respond("{\"ok\":false,\"msg\":\"Unknown command\"}");
    }

    (*current_env)->DeleteLocalRef(current_env, keystoreClass);
    (*current_env)->DeleteLocalRef(current_env, ipcClass);
    return true;
}
#endif
