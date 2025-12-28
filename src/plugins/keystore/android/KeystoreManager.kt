package com.example.crossweb.plugins.keystore

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import androidx.biometric.BiometricPrompt
import androidx.biometric.BiometricManager
import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec
import android.util.Base64
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors

object KeystoreManager {

    private var activity: AppCompatActivity? = null

    fun setActivity(activity: AppCompatActivity) {
        this.activity = activity
    }

    private val Context.secureDataStore: DataStore<Preferences> by preferencesDataStore(name = "secure_prefs")

    private val PRIVATE_KEY_KEY = stringPreferencesKey("secp256k1_private_key")
    private const val KEY_ALIAS = "aes_key_for_secp256k1"

    @JvmStatic
    fun getOrCreateAesKey(): SecretKey {
        val keyStore = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }

        // Check if key already exists
        if (keyStore.containsAlias(KEY_ALIAS)) {
            return (keyStore.getEntry(KEY_ALIAS, null) as KeyStore.SecretKeyEntry).secretKey
        }

        // Generate AES-256-GCM key (API 23+)
        val keyGenerator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
        val keySpec = KeyGenParameterSpec.Builder(
            KEY_ALIAS,
            KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
        )
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(256)
            // Optional: Require user authentication (biometrics/PIN) for key use
            .setUserAuthenticationRequired(true)
            .setUserAuthenticationValidityDurationSeconds(30)
            .build()

        keyGenerator.init(keySpec)
        return keyGenerator.generateKey()
    }

    @JvmStatic
    fun encrypt(data: ByteArray): String {
        return encryptPrivateKey(data)
    }

    @JvmStatic
    fun decrypt(encryptedData: String): ByteArray {
        return decryptPrivateKey(encryptedData)
    }

    @JvmStatic
    fun save(context: Context, data: String) {
        savePrivateKey(context, data)
    }

    @JvmStatic
    fun load(context: Context): String? {
        return loadPrivateKey(context)
    }

    // --- Helper implementations ---
    private const val GCM_TAG_LENGTH = 128 // bits
    private const val IV_LENGTH = 12 // bytes

    private fun authenticate() {
        val activity = activity ?: throw Exception("Activity not set")
        val latch = CountDownLatch(1)
        var result: Result<Unit> = Result.failure(Exception("Not authenticated"))
        val executor = Executors.newSingleThreadExecutor()
        val biometricPrompt = BiometricPrompt(activity, executor, object : BiometricPrompt.AuthenticationCallback() {
            override fun onAuthenticationSucceeded(result_: BiometricPrompt.AuthenticationResult) {
                result = Result.success(Unit)
                latch.countDown()
            }

            override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
                result = Result.failure(Exception("Auth error: $errString"))
                latch.countDown()
            }

            override fun onAuthenticationFailed() {
                result = Result.failure(Exception("Auth failed"))
                latch.countDown()
            }
        })
        activity.runOnUiThread {
            val promptInfo = BiometricPrompt.PromptInfo.Builder()
                .setTitle("Authenticate")
                .setSubtitle("Authenticate to access keystore")
                .setAllowedAuthenticators(BiometricManager.Authenticators.BIOMETRIC_STRONG or BiometricManager.Authenticators.DEVICE_CREDENTIAL)
                .build()
            biometricPrompt.authenticate(promptInfo)
        }
        latch.await()
        result.getOrThrow()
    }

    private fun encryptPrivateKey(privateKeyBytes: ByteArray): String {
        authenticate()
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, getOrCreateAesKey())
        val encrypted = cipher.doFinal(privateKeyBytes)
        val iv = cipher.getIV()
        return Base64.encodeToString(iv + encrypted, Base64.DEFAULT)
    }

    private fun decryptPrivateKey(encryptedBase64: String): ByteArray {
        authenticate()
        val data = Base64.decode(encryptedBase64, Base64.DEFAULT)
        val iv = data.copyOfRange(0, IV_LENGTH)
        val encrypted = data.copyOfRange(IV_LENGTH, data.size)

        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        val spec = GCMParameterSpec(GCM_TAG_LENGTH, iv)
        cipher.init(Cipher.DECRYPT_MODE, getOrCreateAesKey(), spec)
        return cipher.doFinal(encrypted)
    }

    private fun savePrivateKey(context: Context, encryptedBase64: String) {
        runBlocking {
            context.secureDataStore.edit { prefs ->
                prefs[PRIVATE_KEY_KEY] = encryptedBase64
            }
        }
    }

    private fun loadPrivateKey(context: Context): String? {
        return runBlocking {
            context.secureDataStore.data.first()[PRIVATE_KEY_KEY]
        }
    }
}
