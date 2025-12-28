package com.example.crossweb

import android.os.Bundle
import android.webkit.*
import androidx.appcompat.app.AppCompatActivity
import androidx.webkit.WebViewAssetLoader
import android.app.KeyguardManager
import android.content.Context
import android.content.Intent
import android.provider.Settings
import androidx.appcompat.app.AlertDialog

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Check if device has secure lock screen
        val keyguardManager = getSystemService(Context.KEYGUARD_SERVICE) as KeyguardManager
        if (!keyguardManager.isDeviceSecure) {
            AlertDialog.Builder(this)
                .setTitle("Secure Lock Screen Required")
                .setMessage("This app requires a secure lock screen (PIN, pattern, or password) to protect your data. Please set one up.")
                .setPositiveButton("Go to Settings") { _, _ ->
                    startActivity(Intent(Settings.ACTION_SECURITY_SETTINGS))
                }
                .setNegativeButton("Cancel") { _, _ ->
                    finish()
                }
                .setCancelable(false)
                .show()
            return
        }

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

        // Handle JS dialogs
        webView.webChromeClient = object : WebChromeClient() {
            override fun onJsAlert(view: WebView?, url: String?, message: String?, result: JsResult?): Boolean {
                android.app.AlertDialog.Builder(this@MainActivity)
                    .setTitle("Alert")
                    .setMessage(message)
                    .setPositiveButton("OK") { _, _ -> result?.confirm() }
                    .setCancelable(false)
                    .show()
                return true
            }
        }

        // Load native library
        System.loadLibrary("crossweb")

        // Add JS interface
        val ipc = Ipc(webView)
        ipc.setActivity(this)
        webView.addJavascriptInterface(ipc, "__native__")

        // Set keystore activity
        com.example.crossweb.plugins.keystore.KeystoreManager.setActivity(this)

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