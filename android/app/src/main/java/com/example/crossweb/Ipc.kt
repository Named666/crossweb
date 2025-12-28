package com.example.crossweb

import android.webkit.*
import android.util.Log

class Ipc(val webView: WebView) {
        private var activity: android.app.Activity? = null

        fun setActivity(act: android.app.Activity) {
            activity = act
        }
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
        Log.d("CrossWeb", "invoke called with cmd: $cmd, payload: $payload")
        nativeInvoke(id, cmd, payload)
        return id
    }

    fun resolveInvoke(id: String, result: String) {
        Log.d("CrossWeb", "resolveInvoke called with result: $result")
        // Send back to JS
        val escaped = result.replace("'", "\\'").replace("\"", "\\\"")
        webView.post {
            webView.evaluateJavascript("window.__native__.onMessage('$escaped')", null)
        }
    }

    companion object {
        // Library loaded in MainActivity
    }

    private external fun ipc(url: String, message: String)
    private external fun nativeInvoke(id: String, cmd: String, payload: String)
}