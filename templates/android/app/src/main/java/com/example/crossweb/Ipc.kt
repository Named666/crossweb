package {{PACKAGE_NAME}}

import android.webkit.*
import android.util.Log
import kotlin.jvm.JvmStatic
import org.json.JSONObject
import androidx.annotation.Keep

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

    @JavascriptInterface
    fun postMessage(message: String?) {
        message?.let { m ->
            val url = webView.url ?: "unknown"
            this.ipc(url, m)
        }
    }

    // Called from JS with JSON string: {id, cmd, payload}
    @JavascriptInterface
    fun invoke(message: String) {
        try {
            val json = JSONObject(message)
            val id = json.getString("id")
            val cmd = json.getString("cmd")
            val payload = if (json.isNull("payload")) "" else json.optString("payload", json.optJSONObject("payload")?.toString() ?: "")
            
            Log.d("{{APP_NAME}}", "invoke called with id: $id, cmd: $cmd, payload: $payload")
            nativeInvoke(id, cmd, payload)
        } catch (e: Exception) {
            Log.e("{{APP_NAME}}", "Failed to parse invoke message: $message", e)
        }
    }

    @Keep
    fun resolveInvoke(id: String, result: String) {
        if (result.contains("\"error\"")) {
            Log.e("{{APP_NAME}}", "IPC/plugin error (id=$id): $result")
        } else {
            Log.d("{{APP_NAME}}", "resolveInvoke called with result: $result")
        }
        // Escape for JS string embedding
        val safeId = org.json.JSONObject.quote(id)
        val safeResult = org.json.JSONObject.quote(result)
        webView.post {
            webView.evaluateJavascript(
                "if(window.external&&window.external.onMessage){window.external.onMessage(JSON.parse($safeId),JSON.parse($safeResult));}",
                null
            )
        }
    }

    @Keep
    fun emit(event: String, data: String) {
        Log.d("{{APP_NAME}}", "emit called with event: $event, data: $data")
        val safeEvent = event.replace("\\", "\\\\").replace("\"", "\\\"")
        val safeData = data.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "\\r")
        webView.post {
            webView.evaluateJavascript(
                "if(window.external&&window.external.onEvent){window.external.onEvent(\"$safeEvent\",JSON.parse(\"$safeData\"));}",
                null
            )
        }
    }

    public companion object {
        private var staticActivity: AppCompatActivity? = null

        @JvmStatic
        public fun getStaticActivity(): AppCompatActivity? = staticActivity
    }

    @Keep
    private external fun ipc(url: String, message: String)
    @Keep
    private external fun nativeInvoke(id: String, cmd: String, payload: String)
}