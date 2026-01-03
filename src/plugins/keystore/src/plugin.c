#include "../../../plug.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <webauthn.h>
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "webauthn.lib")

// We only need access to the host HWND for proper WebAuthN UI parenting.
// The app uses webview-c (WinAPI backend), whose struct layout is defined here.
#ifndef WEBVIEW_WINAPI
#define WEBVIEW_WINAPI
#endif
#include "../../../../thirdparty/webview-c/webview.h"

static HWND g_webauthn_hwnd = NULL;
#endif

#ifdef __ANDROID__
#include <jni.h>
#endif

// Forward declarations for JNI bridge (to be implemented in Android)
#ifdef __ANDROID__
bool android_keystore_invoke(const char *cmd, const char *payload, RespondCallback respond);
#endif

static bool keystore_init(PluginContext *ctx) {
    (void)ctx;
    // Initialization for Android keystore plugin (if needed)
    printf("Keystore plugin initialized\n");
#ifdef _WIN32
    if (ctx != NULL && ctx->webview != NULL) {
        struct webview *wv = (struct webview *)ctx->webview;
        g_webauthn_hwnd = wv->priv.hwnd;
    }
#endif
    return true;
}

#ifdef _WIN32
static HWND webauthn_get_hwnd(void) {
    if (g_webauthn_hwnd != NULL) {
        return g_webauthn_hwnd;
    }
    // Best-effort fallbacks when running without a captured webview window.
    HWND hwnd = GetActiveWindow();
    if (hwnd != NULL) {
        return hwnd;
    }
    return GetForegroundWindow();
}

static char *base64url_encode(const unsigned char *data, size_t len, size_t *out_len) {
    if (out_len) {
        *out_len = 0;
    }

    DWORD base64_len = 0;
    if (!CryptBinaryToStringA((const BYTE *)data, (DWORD)len,
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              NULL, &base64_len)) {
        return NULL;
    }

    char *base64 = (char *)malloc((size_t)base64_len + 1);
    if (base64 == NULL) {
        return NULL;
    }

    if (!CryptBinaryToStringA((const BYTE *)data, (DWORD)len,
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              base64, &base64_len)) {
        free(base64);
        return NULL;
    }
    base64[base64_len] = '\0';

    // Convert to base64url and strip padding.
    // base64_len includes the trailing NUL in some cases; handle safely.
    size_t n = strlen(base64);
    while (n > 0 && base64[n - 1] == '=') {
        n--;
    }
    for (size_t i = 0; i < n; ++i) {
        if (base64[i] == '+') base64[i] = '-';
        else if (base64[i] == '/') base64[i] = '_';
    }
    base64[n] = '\0';
    if (out_len) {
        *out_len = n;
    }
    return base64;
}

static bool create_credential(char *err, size_t err_cap) {
    // Generate user ID
    BYTE user_id[32];
    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (err && err_cap) snprintf(err, err_cap, "Failed to acquire crypto context for user ID");
        return false;
    }
    if (!CryptGenRandom(hProv, sizeof(user_id), user_id)) {
        CryptReleaseContext(hProv, 0);
        if (err && err_cap) snprintf(err, err_cap, "Failed to generate user ID");
        return false;
    }
    CryptReleaseContext(hProv, 0);

    // RP entity
    WEBAUTHN_RP_ENTITY_INFORMATION rp = {0};
    rp.dwVersion = WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION;
    rp.pwszId = L"localhost";
    rp.pwszName = L"Crossweb";

    // User entity
    WEBAUTHN_USER_ENTITY_INFORMATION user = {0};
    user.dwVersion = WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION;
    user.cbId = sizeof(user_id);
    user.pbId = user_id;
    user.pwszName = L"User";
    user.pwszDisplayName = L"User";

    // COSE parameters
    WEBAUTHN_COSE_CREDENTIAL_PARAMETER cose_param = {0};
    cose_param.dwVersion = WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION;
    cose_param.pwszCredentialType = WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY;
    cose_param.lAlg = WEBAUTHN_COSE_ALGORITHM_ECDSA_P256_WITH_SHA256;

    WEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_params = {0};
    cose_params.cCredentialParameters = 1;
    cose_params.pCredentialParameters = &cose_param;

    // Client data
    BYTE challenge[32];
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (err && err_cap) snprintf(err, err_cap, "Failed to acquire crypto context for challenge");
        return false;
    }
    if (!CryptGenRandom(hProv, sizeof(challenge), challenge)) {
        CryptReleaseContext(hProv, 0);
        if (err && err_cap) snprintf(err, err_cap, "Failed to generate challenge");
        return false;
    }
    CryptReleaseContext(hProv, 0);

    size_t b64_len;
    char *b64_challenge = base64url_encode(challenge, sizeof(challenge), &b64_len);
    if (!b64_challenge) {
        if (err && err_cap) snprintf(err, err_cap, "Failed to encode challenge");
        return false;
    }

    char json[1024];
    int json_len = snprintf(json, sizeof(json), "{\"type\":\"webauthn.create\",\"challenge\":\"%s\",\"origin\":\"https://localhost\"}", b64_challenge);
    free(b64_challenge);
    if (json_len < 0 || (size_t)json_len >= sizeof(json)) {
        if (err && err_cap) snprintf(err, err_cap, "JSON too long");
        return false;
    }

    WEBAUTHN_CLIENT_DATA client_data = {0};
    client_data.dwVersion = WEBAUTHN_CLIENT_DATA_CURRENT_VERSION;
    client_data.pbClientDataJSON = (PBYTE)json;
    client_data.cbClientDataJSON = json_len;
    client_data.pwszHashAlgId = WEBAUTHN_HASH_ALGORITHM_SHA_256;

    // Options
    WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options_make = {0};
    options_make.dwVersion = WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_CURRENT_VERSION;
    options_make.dwTimeoutMilliseconds = 60000;
    options_make.dwAuthenticatorAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM;
    options_make.dwUserVerificationRequirement = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
    options_make.dwAttestationConveyancePreference = WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ANY;
    options_make.bPreferResidentKey = TRUE;

    PWEBAUTHN_CREDENTIAL_ATTESTATION attestation = NULL;
    HRESULT hr = WebAuthNAuthenticatorMakeCredential(webauthn_get_hwnd(), &rp, &user, &cose_params, &client_data, &options_make, &attestation);
    if (FAILED(hr)) {
        if (err && err_cap) {
            PCWSTR error_name = WebAuthNGetErrorName(hr);
            if (error_name) {
                char narrow[256];
                WideCharToMultiByte(CP_UTF8, 0, error_name, -1, narrow, sizeof(narrow), NULL, NULL);
                snprintf(err, err_cap, "WebAuthn make credential failed: %s (0x%lx)", narrow, (unsigned long)hr);
            } else {
                snprintf(err, err_cap, "WebAuthn make credential failed: 0x%lx", (unsigned long)hr);
            }
        }
        return false;
    }

    WebAuthNFreeCredentialAttestation(attestation);
    return true;
}

static bool windows_hello_prompt(const wchar_t *caption, const wchar_t *message, char *err, size_t err_cap) {
    (void)caption;
    (void)message;
    if (err && err_cap) err[0] = '\0';

    DWORD api_version = WebAuthNGetApiVersionNumber();
    if (api_version < 1) {
        if (err && err_cap) snprintf(err, err_cap, "WebAuthn API version too low: %lu", api_version);
        return false;
    }

    WINBOOL available = FALSE;
    HRESULT hr = WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable(&available);
    if (FAILED(hr) || !available) {
        if (err && err_cap) snprintf(err, err_cap, "Windows Hello not available");
        return false;
    }

    // Generate random challenge
    BYTE challenge[32];
    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (err && err_cap) snprintf(err, err_cap, "Failed to acquire crypto context");
        return false;
    }
    if (!CryptGenRandom(hProv, sizeof(challenge), challenge)) {
        CryptReleaseContext(hProv, 0);
        if (err && err_cap) snprintf(err, err_cap, "Failed to generate random challenge");
        return false;
    }
    CryptReleaseContext(hProv, 0);

    // Base64url encode challenge
    size_t b64_len;
    char *b64_challenge = base64url_encode(challenge, sizeof(challenge), &b64_len);
    if (!b64_challenge) {
        if (err && err_cap) snprintf(err, err_cap, "Failed to encode challenge");
        return false;
    }

    // Prepare client data
    char json[1024];
    int json_len = snprintf(json, sizeof(json), "{\"type\":\"webauthn.get\",\"challenge\":\"%s\",\"origin\":\"https://localhost\"}", b64_challenge);
    free(b64_challenge);
    if (json_len < 0 || (size_t)json_len >= sizeof(json)) {
        if (err && err_cap) snprintf(err, err_cap, "JSON too long");
        return false;
    }

    WEBAUTHN_CLIENT_DATA client_data = {0};
    client_data.dwVersion = WEBAUTHN_CLIENT_DATA_CURRENT_VERSION;
    client_data.pbClientDataJSON = (PBYTE)json;
    client_data.cbClientDataJSON = json_len;
    client_data.pwszHashAlgId = WEBAUTHN_HASH_ALGORITHM_SHA_256;

    WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options = {0};
    options.dwVersion = WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_CURRENT_VERSION;
    options.dwTimeoutMilliseconds = 60000;
    options.dwAuthenticatorAttachment = WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM;
    options.dwUserVerificationRequirement = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;

    PWEBAUTHN_ASSERTION assertion = NULL;
    hr = WebAuthNAuthenticatorGetAssertion(webauthn_get_hwnd(), L"localhost", &client_data, &options, &assertion);

    if (FAILED(hr)) {
        // If no credentials found, try to create one
        // 0x80090011 = NTE_NOT_FOUND
        // 0x80040001 = NotSupportedError (often returned when no credentials match)
        if (hr == (HRESULT)0x80090011 || hr == (HRESULT)0x80040001) {
             if (create_credential(err, err_cap)) {
                 // Try again after creation
                 hr = WebAuthNAuthenticatorGetAssertion(NULL, L"localhost", &client_data, &options, &assertion);
             }
        }
    }

    if (FAILED(hr)) {
        if (err && err_cap) {
            PCWSTR error_name = WebAuthNGetErrorName(hr);
            if (error_name) {
                // Convert wide to narrow (simplified)
                char narrow[256];
                WideCharToMultiByte(CP_UTF8, 0, error_name, -1, narrow, sizeof(narrow), NULL, NULL);
                snprintf(err, err_cap, "WebAuthn assertion failed: %s (0x%lx)", narrow, (unsigned long)hr);
            } else {
                snprintf(err, err_cap, "WebAuthn assertion failed: 0x%lx", (unsigned long)hr);
            }
        }
        return false;
    }

    WebAuthNFreeAssertion(assertion);
    return true;
}
#endif

static bool keystore_invoke(const char *cmd, const char *payload, RespondCallback respond) {
#ifdef __ANDROID__
    // Forward to JNI/Android implementation
    return android_keystore_invoke(cmd, payload, respond);
#elif defined(_WIN32)
    if (respond == NULL) {
        return false;
    }

    const char *in = payload ? payload : "";

    if (strcmp(cmd, "encrypt") == 0) {
        {
            char err[256] = {0};
            if (!windows_hello_prompt(L"Crossweb", L"Authenticate with Windows Hello to encrypt", err, sizeof(err))) {
                char msg[512];
                snprintf(msg, sizeof(msg), "windows hello cancelled or failed: %s", err[0] ? err : "unknown error");
                size_t resp_len = strlen(msg) + 32;
                char *resp = (char *)malloc(resp_len);
                if (resp) {
                    snprintf(resp, resp_len, "{\"ok\":false,\"msg\":\"%s\"}", msg);
                    respond(resp);
                    free(resp);
                } else {
                    respond("{\"ok\":false,\"msg\":\"windows hello failed\"}");
                }
                return false;
            }
        }
        // Input: hex string
        size_t in_len = strlen(in);
        if ((in_len % 2) != 0) {
            respond("{\"ok\":false,\"msg\":\"invalid hex length\"}");
            return false;
        }

        size_t byte_len = in_len / 2;
        BYTE *bytes = (BYTE *)malloc(byte_len ? byte_len : 1);
        if (bytes == NULL) {
            respond("{\"ok\":false,\"msg\":\"out of memory\"}");
            return false;
        }
        for (size_t i = 0; i < byte_len; ++i) {
            unsigned int v = 0;
            if (sscanf(in + (i * 2), "%2x", &v) != 1) {
                free(bytes);
                respond("{\"ok\":false,\"msg\":\"invalid hex\"}");
                return false;
            }
            bytes[i] = (BYTE)v;
        }

        DATA_BLOB in_blob = {0};
        in_blob.pbData = bytes;
        in_blob.cbData = (DWORD)byte_len;
        DATA_BLOB out_blob = {0};

        // DPAPI: encrypt tied to current user. If user signed-in with Windows Hello,
        // DPAPI ultimately benefits from that protection.
        if (!CryptProtectData(&in_blob, L"crossweb.keystore", NULL, NULL, NULL, 0, &out_blob)) {
            free(bytes);
            respond("{\"ok\":false,\"msg\":\"CryptProtectData failed\"}");
            return false;
        }
        free(bytes);

        DWORD b64_len = 0;
        if (!CryptBinaryToStringA(out_blob.pbData, out_blob.cbData,
                                 CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &b64_len)) {
            LocalFree(out_blob.pbData);
            respond("{\"ok\":false,\"msg\":\"base64 encode failed\"}");
            return false;
        }

        char *b64 = (char *)malloc((size_t)b64_len + 1);
        if (b64 == NULL) {
            LocalFree(out_blob.pbData);
            respond("{\"ok\":false,\"msg\":\"out of memory\"}");
            return false;
        }
        if (!CryptBinaryToStringA(out_blob.pbData, out_blob.cbData,
                                 CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64, &b64_len)) {
            free(b64);
            LocalFree(out_blob.pbData);
            respond("{\"ok\":false,\"msg\":\"base64 encode failed\"}");
            return false;
        }
        b64[b64_len] = '\0';
        LocalFree(out_blob.pbData);

        // JSON response
        size_t resp_cap = strlen(b64) + 64;
        char *resp = (char *)malloc(resp_cap);
        if (resp == NULL) {
            free(b64);
            respond("{\"ok\":false,\"msg\":\"out of memory\"}");
            return false;
        }
        snprintf(resp, resp_cap, "{\"ok\":true,\"encrypted\":\"%s\"}", b64);
        free(b64);
        respond(resp);
        free(resp);
        return true;
    }

    if (strcmp(cmd, "decrypt") == 0) {
        {
            char err[256] = {0};
            if (!windows_hello_prompt(L"Crossweb", L"Authenticate with Windows Hello to decrypt", err, sizeof(err))) {
                char msg[512];
                snprintf(msg, sizeof(msg), "windows hello cancelled or failed: %s", err[0] ? err : "unknown error");
                size_t resp_len = strlen(msg) + 32;
                char *resp = (char *)malloc(resp_len);
                if (resp) {
                    snprintf(resp, resp_len, "{\"ok\":false,\"msg\":\"%s\"}", msg);
                    respond(resp);
                    free(resp);
                } else {
                    respond("{\"ok\":false,\"msg\":\"windows hello failed\"}");
                }
                return false;
            }
        }
        // Input: base64 string (DPAPI blob)
        DWORD bin_len = 0;
        if (!CryptStringToBinaryA(in, 0, CRYPT_STRING_BASE64, NULL, &bin_len, NULL, NULL)) {
            respond("{\"ok\":false,\"msg\":\"invalid base64\"}");
            return false;
        }
        BYTE *bin = (BYTE *)malloc((size_t)bin_len ? (size_t)bin_len : 1);
        if (bin == NULL) {
            respond("{\"ok\":false,\"msg\":\"out of memory\"}");
            return false;
        }
        if (!CryptStringToBinaryA(in, 0, CRYPT_STRING_BASE64, bin, &bin_len, NULL, NULL)) {
            free(bin);
            respond("{\"ok\":false,\"msg\":\"invalid base64\"}");
            return false;
        }

        DATA_BLOB in_blob = {0};
        in_blob.pbData = bin;
        in_blob.cbData = bin_len;
        DATA_BLOB out_blob = {0};

        if (!CryptUnprotectData(&in_blob, NULL, NULL, NULL, NULL, 0, &out_blob)) {
            free(bin);
            respond("{\"ok\":false,\"msg\":\"CryptUnprotectData failed\"}");
            return false;
        }
        free(bin);

        // Convert decrypted bytes to hex string
        size_t hex_len = (size_t)out_blob.cbData * 2;
        char *hex = (char *)malloc(hex_len + 1);
        if (hex == NULL) {
            LocalFree(out_blob.pbData);
            respond("{\"ok\":false,\"msg\":\"out of memory\"}");
            return false;
        }
        static const char *digits = "0123456789abcdef";
        for (DWORD i = 0; i < out_blob.cbData; ++i) {
            BYTE b = out_blob.pbData[i];
            hex[i * 2] = digits[(b >> 4) & 0xF];
            hex[i * 2 + 1] = digits[b & 0xF];
        }
        hex[hex_len] = '\0';
        LocalFree(out_blob.pbData);

        size_t resp_cap = hex_len + 64;
        char *resp = (char *)malloc(resp_cap);
        if (resp == NULL) {
            free(hex);
            respond("{\"ok\":false,\"msg\":\"out of memory\"}");
            return false;
        }
        snprintf(resp, resp_cap, "{\"ok\":true,\"privateKey\":\"%s\"}", hex);
        free(hex);
        respond(resp);
        free(resp);
        return true;
    }

    if (strcmp(cmd, "save") == 0) {
        // Input: encrypted base64 string
        const char *appdata = getenv("APPDATA");
        char folder[MAX_PATH] = {0};
        char path[MAX_PATH] = {0};
        if (appdata && appdata[0] != '\0') {
            snprintf(folder, sizeof(folder), "%s\\crossweb", appdata);
            CreateDirectoryA(folder, NULL);
            snprintf(path, sizeof(path), "%s\\keystore.dat", folder);
        } else {
            snprintf(path, sizeof(path), ".\\keystore.dat");
        }

        FILE *f = fopen(path, "wb");
        if (!f) {
            respond("{\"ok\":false,\"msg\":\"failed to open keystore file\"}");
            return false;
        }
        size_t n = fwrite(in, 1, strlen(in), f);
        fclose(f);
        if (n != strlen(in)) {
            respond("{\"ok\":false,\"msg\":\"failed to write keystore file\"}");
            return false;
        }
        respond("{\"ok\":true}");
        return true;
    }

    if (strcmp(cmd, "load") == 0) {
        const char *appdata = getenv("APPDATA");
        char path[MAX_PATH] = {0};
        if (appdata && appdata[0] != '\0') {
            snprintf(path, sizeof(path), "%s\\crossweb\\keystore.dat", appdata);
        } else {
            snprintf(path, sizeof(path), ".\\keystore.dat");
        }

        FILE *f = fopen(path, "rb");
        if (!f) {
            respond("{\"ok\":false,\"msg\":\"no saved value\"}");
            return false;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size <= 0 || size > 1024 * 1024) {
            fclose(f);
            respond("{\"ok\":false,\"msg\":\"invalid keystore file\"}");
            return false;
        }
        char *buf = (char *)malloc((size_t)size + 1);
        if (!buf) {
            fclose(f);
            respond("{\"ok\":false,\"msg\":\"out of memory\"}");
            return false;
        }
        size_t readn = fread(buf, 1, (size_t)size, f);
        fclose(f);
        buf[readn] = '\0';
        if (readn == 0) {
            free(buf);
            respond("{\"ok\":false,\"msg\":\"no saved value\"}");
            return false;
        }
        size_t resp_cap = readn + 64;
        char *resp = (char *)malloc(resp_cap);
        if (!resp) {
            free(buf);
            respond("{\"ok\":false,\"msg\":\"out of memory\"}");
            return false;
        }
        snprintf(resp, resp_cap, "{\"ok\":true,\"encrypted\":\"%s\"}", buf);
        free(buf);
        respond(resp);
        free(resp);
        return true;
    }

    respond("{\"ok\":false,\"msg\":\"Unknown command\"}");
    return false;
#else
    respond("{\"ok\":false,\"msg\":\"Keystore not supported on this platform\"}");
    return false;
#endif
}

static void keystore_event(const char *event, const char *data) {
    (void)event;
    (void)data;
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
