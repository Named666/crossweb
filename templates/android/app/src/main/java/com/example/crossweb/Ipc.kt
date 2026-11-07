package {{PACKAGE_NAME}}

import android.webkit.*
import android.util.Log
import kotlin.jvm.JvmStatic

import androidx.appcompat.app.AppCompatActivity

class Ipc(val webView: WebView) {
        private var activity: AppCompatActivity? = null

        fun setActivity(act: AppCompatActivity) {
            activity = act
            staticActivity = act
        }

        fun getContext(): android.content.Context {
            return activity!!
        }

        fun getActivity(): AppCompatActivity {
            return activity!!
        }
    private fun pageIsTrusted(): Boolean {
        val url = webView.url ?: return false
        return url.startsWith("https://appassets.androidplatform.net/") ||
            url.startsWith("http://localhost:") ||
            url.startsWith("http://127.0.0.1:") ||
            url.startsWith("https://localhost:") ||
            url.startsWith("https://127.0.0.1:")
    }

    private fun identifierOk(value: String): Boolean {
        return value.matches(Regex("^[A-Za-z0-9._:-]+$"))
    }

    @JavascriptInterface
    fun postMessage(message: String?) {
        if (!pageIsTrusted()) {
            Log.e("{{APP_NAME}}", "rejected postMessage from untrusted page")
            return
        }
        message?.let { m ->
            val url = webView.url ?: "unknown"
            this.ipc(url, m)
        }
    }

    @JavascriptInterface
    fun invoke(cmd: String, payload: String): String {
        if (!pageIsTrusted()) {
            Log.e("{{APP_NAME}}", "rejected invoke from untrusted page")
            return ""
        }
        val id = java.util.UUID.randomUUID().toString()
        Log.d("{{APP_NAME}}", "invoke called with cmd: $cmd")
        nativeInvoke(id, cmd, payload)
        return id
    }

    fun resolveInvoke(id: String, result: String) {
        if (!identifierOk(id)) {
            Log.e("{{APP_NAME}}", "rejected resolveInvoke with an unsafe id")
            return
        }
        val encoded = android.util.Base64.encodeToString(
            result.toByteArray(Charsets.UTF_8),
            android.util.Base64.NO_WRAP
        )
        webView.post {
            webView.evaluateJavascript(
                "if(window.external&&window.external.onMessage){window.external.onMessage('$id',JSON.parse(atob('$encoded')));}",
                null
            )
        }
    }

    fun emit(event: String, data: String) {
        if (!identifierOk(event)) {
            Log.e("{{APP_NAME}}", "rejected emit with an unsafe event name")
            return
        }
        val encoded = android.util.Base64.encodeToString(
            data.toByteArray(Charsets.UTF_8),
            android.util.Base64.NO_WRAP
        )
        webView.post {
            webView.evaluateJavascript(
                "if(window.external&&window.external.onEvent){window.external.onEvent('$event',JSON.parse(atob('$encoded')));}",
                null
            )
        }
    }

    public companion object {
        private var staticActivity: AppCompatActivity? = null

        @JvmStatic
        public fun getStaticActivity(): AppCompatActivity? = staticActivity
    }

    private external fun ipc(url: String, message: String)
    private external fun nativeInvoke(id: String, cmd: String, payload: String)
}