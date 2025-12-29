import org.gradle.api.tasks.Copy

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    compileSdk = 36
    namespace = "{{PACKAGE_NAME}}"
    defaultConfig {
        manifestPlaceholders["usesCleartextTraffic"] = "false"
        applicationId = "{{PACKAGE_NAME}}"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"
        externalNativeBuild {
            cmake {
                cFlags("-std=c99")
            }
        }
    }
    buildTypes {
        getByName("debug") {
            manifestPlaceholders["usesCleartextTraffic"] = "true"
            isDebuggable = true
            isJniDebuggable = true
            isMinifyEnabled = false
            packaging {
                jniLibs.keepDebugSymbols.add("*/arm64-v8a/*.so")
                jniLibs.keepDebugSymbols.add("*/armeabi-v7a/*.so")
                jniLibs.keepDebugSymbols.add("*/x86/*.so")
                jniLibs.keepDebugSymbols.add("*/x86_64/*.so")
            }
        }
        getByName("release") {
            isMinifyEnabled = true
            proguardFiles(
                *fileTree(".") { include("**/*.pro") }
                    .plus(getDefaultProguardFile("proguard-android-optimize.txt"))
                    .toList().toTypedArray()
            )
            signingConfig = signingConfigs.getByName("debug")
        }
    }
    kotlinOptions {
        jvmTarget = "1.8"
    }
    buildFeatures {
        buildConfig = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/c/CMakeLists.txt")
        }
    }
}

dependencies {
    implementation("androidx.webkit:webkit:1.14.0")
    implementation("androidx.appcompat:appcompat:1.7.1")
    implementation("androidx.activity:activity-ktx:1.10.1")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.datastore:datastore-preferences:1.1.1")
    implementation("androidx.biometric:biometric:1.1.0")
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.1.4")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.0")
}

// Copy plugin Android sources (Kotlin/Java) into the app module at build time

val cleanPluginSources by tasks.registering(Delete::class) {
    delete(file("$buildDir/generated/pluginSources"))
}

val copyPluginSources by tasks.registering(Copy::class) {
    val pluginsDir = file("../../src/plugins")
    from(pluginsDir) {
        include("**/android/**/*.kt")
        include("**/android/**/*.java")
        includeEmptyDirs = false
    }
    into(file("$buildDir/generated/pluginSources"))
    eachFile {
        val segments = relativePath.segments
        if (segments.size >= 3 && segments[1] == "android") {
            relativePath = org.gradle.api.file.RelativePath(true, "com", "example", "crossweb", "plugins", segments[0], segments[2])
        } else {
            relativePath = org.gradle.api.file.RelativePath(true, relativePath.lastName)
        }
    }
}

android.sourceSets.getByName("main").java.srcDir(file("$buildDir/generated/pluginSources"))
android.sourceSets.getByName("main").kotlin.srcDir(file("$buildDir/generated/pluginSources"))

tasks.named("preBuild") {
    dependsOn(cleanPluginSources, copyPluginSources)
}