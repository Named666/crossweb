package com.example.crossweb

import android.os.Bundle
import android.webkit.*
import androidx.appcompat.app.AppCompatActivity
import androidx.webkit.WebViewAssetLoader

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val webView: WebView = findViewById(R.id.webview)

        // Important: enables JavaScript (if your HTML needs it)
        webView.settings.javaScriptEnabled = true

        // Set up asset loader for local assets
        val assetLoader = WebViewAssetLoader.Builder()
            .setDomain("appassets.androidplatform.net")
            .addPathHandler("/", WebViewAssetLoader.AssetsPathHandler(this))
            .build()

        // Makes sure links open inside the WebView instead of external browser
        webView.webViewClient = object : WebViewClient() {
            override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse? {
                return assetLoader.shouldInterceptRequest(request.url)
            }
        }

        // Load native library
        System.loadLibrary("crossweb")

        // Add JS interface
        webView.addJavascriptInterface(Ipc(webView), "__native__")

        // Load the local HTML file from assets folder via asset loader
        webView.loadUrl("https://appassets.androidplatform.net/index.html")
    }

    // Optional: handle back button to navigate web history
    override fun onBackPressed() {
        val webView: WebView = findViewById(R.id.webview)
        if (webView.canGoBack()) {
            webView.goBack()
        } else {
            super.onBackPressed()
        }
    }
}