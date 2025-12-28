# Add project specific ProGuard rules here.
# You can control the set of applied configuration files using the
# proguardFiles setting in build.gradle.kts.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
-keepclassmembers class com.example.crossweb.Ipc {
    public *;
}

# Keep native methods
-keepclasseswithmembernames class com.example.crossweb.Ipc {
    native <methods>;
}

# Keep KeystoreManager class and its members since it's accessed via JNI
-keep class com.example.crossweb.plugins.keystore.KeystoreManager {
    *;
}