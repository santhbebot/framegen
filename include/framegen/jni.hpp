#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// JNI entry points expected by Levi Launcher
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved);

// FPS Mod native interface
JNIEXPORT jint JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FpsMod_nativeGetFps(JNIEnv* env, jobject thiz);

// FrameGen native interface
JNIEXPORT jint JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeInitialize(
    JNIEnv* env, jobject thiz, jint width, jint height);

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeShutdown(
    JNIEnv* env, jobject thiz);

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeSetConfig(
    JNIEnv* env, jobject thiz, jstring json_config);

JNIEXPORT jstring JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeGetConfig(
    JNIEnv* env, jobject thiz);

JNIEXPORT jstring JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeGetTelemetry(
    JNIEnv* env, jobject thiz);

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeToggle(
    JNIEnv* env, jobject thiz, jboolean enabled);

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeCyclePreset(
    JNIEnv* env, jobject thiz);

#ifdef __cplusplus
}
#endif