package com.example.crossweb

import android.webkit.*

class Ipc(val webView: WebView) {
    @JavascriptInterface
    fun postMessage(message: String?) {
        message?.let { m ->
            val url = webView.url ?: "unknown"
            this.ipc(url, m)
        }
    }

    @JavascriptInterface
    fun invoke(cmd: String, payload: String): String {
        val id = java.util.UUID.randomUUID().toString()
        nativeInvoke(id, cmd, payload)
        return id
    }

    fun resolveInvoke(id: String, result: String) {
        // Send back to JS
        webView.evaluateJavascript("window.__native__.onMessage($result)", null)
    }

    companion object {
        // Library loaded in MainActivity
    }

    private external fun ipc(url: String, message: String)
    private external fun nativeInvoke(id: String, cmd: String, payload: String)
}