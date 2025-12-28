package com.example.crossweb

import android.app.Activity
import android.content.Context
import android.os.Build
import android.util.Log
import androidx.annotation.RequiresApi
import androidx.biometric.BiometricManager
import androidx.biometric.BiometricPrompt
import androidx.core.content.ContextCompat
import java.util.concurrent.Executor

class KeystoreBio(val activity: Activity, val callback: (Boolean, String) -> Unit) {
    private val executor: Executor = ContextCompat.getMainExecutor(activity)

    fun authenticate() {
        val biometricManager = BiometricManager.from(activity)
        if (biometricManager.canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_STRONG) != BiometricManager.BIOMETRIC_SUCCESS) {
            callback(false, "Biometric authentication not available")
            return
        }
        val promptInfo = BiometricPrompt.PromptInfo.Builder()
            .setTitle("Biometric Authentication")
            .setSubtitle("Authenticate to access secure keystore")
            .setNegativeButtonText("Cancel")
            .build()
        val biometricPrompt = BiometricPrompt(activity, executor, object : BiometricPrompt.AuthenticationCallback() {
            override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
                super.onAuthenticationSucceeded(result)
                callback(true, "Authentication succeeded")
            }
            override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
                super.onAuthenticationError(errorCode, errString)
                callback(false, errString.toString())
            }
            override fun onAuthenticationFailed() {
                super.onAuthenticationFailed()
                callback(false, "Authentication failed")
            }
        })
        biometricPrompt.authenticate(promptInfo)
    }
}
