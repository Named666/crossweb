package {{PACKAGE_NAME}}

import android.os.Build
import android.os.Bundle
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.appcompat.app.AppCompatActivity
import androidx.webkit.WebViewAssetLoader
import android.util.Log

class MainActivity : AppCompatActivity() {
    private var devOrigin: String? = null
    private var devUrl: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        readDevUrl()

        val webView: WebView = findViewById(R.id.webview)
        webView.settings.javaScriptEnabled = true
        webView.settings.allowFileAccess = false
        webView.settings.allowContentAccess = false
        webView.settings.allowFileAccessFromFileURLs = false
        webView.settings.allowUniversalAccessFromFileURLs = false
        webView.settings.mixedContentMode = WebSettings.MIXED_CONTENT_NEVER_ALLOW
        if (Build.VERSION.SDK_INT >= 26) {
            webView.settings.safeBrowsingEnabled = true
        }
        if (BuildConfig.DEBUG) {
            WebView.setWebContentsDebuggingEnabled(true)
        }

        val assetLoader = WebViewAssetLoader.Builder()
            .setDomain("appassets.androidplatform.net")
            .addPathHandler("/", WebViewAssetLoader.AssetsPathHandler(this))
            .build()

        webView.webViewClient = object : WebViewClient() {
            override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean {
                val url = request.url?.toString() ?: return true
                // Applies to main-frame and subframe navigations. This stops an
                // untrusted document from loading. It does not make the
                // JavascriptInterface able to tell frames apart.
                if (!isTrusted(url)) {
                    Log.e(TAG, "blocked navigation")
                    return true
                }
                return false
            }

            override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse? {
                return assetLoader.shouldInterceptRequest(request.url)
            }
        }

        System.loadLibrary("crossweb")
        val ipc = Ipc(webView)
        ipc.setActivity(this)
        ipc.setAppDataRoot(filesDir.absolutePath)
        webView.addJavascriptInterface(ipc, "external")

        val start = devUrl ?: "https://appassets.androidplatform.net/index.html"
        webView.loadUrl(start)
    }

    private fun readDevUrl() {
        try {
            val text = assets.open("dev-url.txt").bufferedReader().use { it.readText() }.trim()
            val origin = devOriginOrNull(text)
            if (origin == null) {
                Log.e(TAG, "dev-url.txt was rejected; staying on bundled content")
                return
            }
            devUrl = text
            devOrigin = origin
        } catch (_: Exception) {
            devUrl = null
            devOrigin = null
        }
    }

    private fun devOriginOrNull(url: String): String? {
        val match = Regex("^(https?)://(localhost|127\\.0\\.0\\.1):([0-9]{1,5})(/.*)?$").matchEntire(url)
            ?: return null
        val port = match.groupValues[3].toIntOrNull() ?: return null
        if (port !in 1..65535) return null
        return "${match.groupValues[1]}://${match.groupValues[2]}:$port"
    }

    private fun isTrusted(url: String): Boolean {
        if (url == APP_ORIGIN || url.startsWith("$APP_ORIGIN/")) return true
        val dev = devOrigin ?: return false
        return url == dev || url.startsWith("$dev/")
    }

    companion object {
        private const val TAG = "Crossweb"
        private const val APP_ORIGIN = "https://appassets.androidplatform.net"
    }
}
