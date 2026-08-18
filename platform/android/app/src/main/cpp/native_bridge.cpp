#include <jni.h>
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <android/log.h>
#include "aerosync/aerosync_app.hpp"
#include "aerosync/pairing_state_machine.hpp"

#define LOG_TAG "AeroSyncJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static std::mutex g_appMutex;
static std::shared_ptr<aerosync::AeroSyncApp> g_app;
static JavaVM* g_jvm = nullptr;
static jobject g_bridgeObj = nullptr;
static std::mutex g_bridgeMutex;

static std::mutex g_pairingMutex;
static std::function<void(bool)> g_pendingPairingCallback;

struct JniThreadAttacher {
    JNIEnv* env{nullptr};
    bool attached{false};

    JniThreadAttacher() {
        if (!g_jvm) return;
        jint res = g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (res == JNI_EDETACHED) {
            if (g_jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attached = true;
            }
        }
    }

    ~JniThreadAttacher() {
        if (attached && g_jvm) {
            g_jvm->DetachCurrentThread();
        }
    }
};

static jobject getSafeBridgeRef(JNIEnv* env) {
    std::lock_guard<std::mutex> lock(g_bridgeMutex);
    if (!g_bridgeObj) return nullptr;
    return env->NewLocalRef(g_bridgeObj);
}

static std::shared_ptr<aerosync::AeroSyncApp> getAppInstance() {
    std::lock_guard<std::mutex> lock(g_appMutex);
    return g_app;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_aerosync_app_nativebridge_AeroSyncNativeBridge_nativeInitialize(
        JNIEnv* env, jobject thiz, jstring deviceId, jstring deviceName) {
    
    {
        std::lock_guard<std::mutex> lock(g_bridgeMutex);
        if (g_bridgeObj) {
            env->DeleteGlobalRef(g_bridgeObj);
        }
        g_bridgeObj = env->NewGlobalRef(thiz);
    }

    const char* idStr = env->GetStringUTFChars(deviceId, nullptr);
    const char* nameStr = env->GetStringUTFChars(deviceName, nullptr);

    std::shared_ptr<aerosync::AeroSyncApp> app;
    {
        std::lock_guard<std::mutex> lock(g_appMutex);
        if (g_app) {
            g_app->shutdown();
            g_app.reset();
        }
        g_app = std::make_shared<aerosync::AeroSyncApp>(idStr ? idStr : "android-dev", 
                                                        nameStr ? nameStr : "Android Device", 
                                                        aerosync::DeviceType::DEVICE_ANDROID);
        app = g_app;
    }

    if (idStr) env->ReleaseStringUTFChars(deviceId, idStr);
    if (nameStr) env->ReleaseStringUTFChars(deviceName, nameStr);

    if (!app) return JNI_FALSE;

    app->setPeerDiscoveredCallback([](const std::vector<aerosync::PeerInfo>& peers) {
        JniThreadAttacher attacher;
        JNIEnv* env = attacher.env;
        if (!env) return;
        jobject bridge = getSafeBridgeRef(env);
        if (bridge) {
            jclass clazz = env->GetObjectClass(bridge);
            if (clazz) {
                jmethodID mid = env->GetMethodID(clazz, "onNativePeersUpdated", "()V");
                if (mid) {
                    env->CallVoidMethod(bridge, mid);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                }
                env->DeleteLocalRef(clazz);
            }
            env->DeleteLocalRef(bridge);
        }
    });

    app->setIncomingConnectCallback([](const aerosync::ConnectRequest& req, std::function<void(bool accept)> respondCb) {
        {
            std::lock_guard<std::mutex> lock(g_pairingMutex);
            g_pendingPairingCallback = respondCb;
        }
        JniThreadAttacher attacher;
        JNIEnv* env = attacher.env;
        if (!env) return;
        jobject bridge = getSafeBridgeRef(env);
        if (bridge) {
            jclass clazz = env->GetObjectClass(bridge);
            if (clazz) {
                jmethodID mid = env->GetMethodID(clazz, "onNativePairingRequest", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
                if (mid) {
                    jstring sId = env->NewStringUTF(req.senderId.c_str());
                    jstring sName = env->NewStringUTF(req.senderName.c_str());
                    jstring sPin = env->NewStringUTF(req.pairingPin.c_str());
                    env->CallVoidMethod(bridge, mid, sId, sName, sPin);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    env->DeleteLocalRef(sId);
                    env->DeleteLocalRef(sName);
                    env->DeleteLocalRef(sPin);
                }
                env->DeleteLocalRef(clazz);
            }
            env->DeleteLocalRef(bridge);
        }
    });

    app->setIncomingTransferProgressCallback([](const aerosync::TransferProgress& prog) {
        JniThreadAttacher attacher;
        JNIEnv* env = attacher.env;
        if (!env) return;
        jobject bridge = getSafeBridgeRef(env);
        if (bridge) {
            jclass clazz = env->GetObjectClass(bridge);
            if (clazz) {
                jmethodID mid = env->GetMethodID(clazz, "onNativeProgress", "(Ljava/lang/String;IJJDD)V");
                if (mid) {
                    jstring nameJ = env->NewStringUTF(prog.currentFileName.c_str());
                    env->CallVoidMethod(bridge, mid, nameJ, prog.currentFileIndex,
                                        prog.batchBytesTransferred, prog.batchTotalBytes,
                                        prog.speedBytesPerSec, prog.speedMbps);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    env->DeleteLocalRef(nameJ);
                }
                env->DeleteLocalRef(clazz);
            }
            env->DeleteLocalRef(bridge);
        }
    });

    app->setPairingStateChangedCallback([](aerosync::PairingState state, const std::string& reason) {
        JniThreadAttacher attacher;
        JNIEnv* env = attacher.env;
        if (!env) return;
        jobject bridge = getSafeBridgeRef(env);
        if (bridge) {
            jclass clazz = env->GetObjectClass(bridge);
            if (clazz) {
                jmethodID mid = env->GetMethodID(clazz, "onNativePairingStateChanged", "(Ljava/lang/String;Ljava/lang/String;)V");
                if (mid) {
                    jstring stateStr = env->NewStringUTF(aerosync::pairingStateToString(state).c_str());
                    jstring reasonStr = env->NewStringUTF(reason.c_str());
                    env->CallVoidMethod(bridge, mid, stateStr, reasonStr);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    env->DeleteLocalRef(stateStr);
                    env->DeleteLocalRef(reasonStr);
                }
                env->DeleteLocalRef(clazz);
            }
            env->DeleteLocalRef(bridge);
        }
    });

    app->setIncomingTransferCallback([](const aerosync::TransferManifest& manifest, std::function<void(bool accept)> respondCb) {
        LOGI("Incoming transfer auto-accepted for batch %s (%zu files, %llu bytes)",
             manifest.batchId.c_str(), manifest.files.size(), (unsigned long long)manifest.totalBytes);

        JniThreadAttacher attacher;
        JNIEnv* env = attacher.env;
        if (env) {
            jobject bridge = getSafeBridgeRef(env);
            if (bridge) {
                jclass clazz = env->GetObjectClass(bridge);
                if (clazz) {
                    jmethodID mid = env->GetMethodID(clazz, "onNativeIncomingTransfer", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JIJ)V");
                    if (mid) {
                        jstring senderJ = env->NewStringUTF(manifest.senderName.c_str());
                        jstring batchIdJ = env->NewStringUTF(manifest.batchId.c_str());
                        std::string firstFileName = manifest.files.empty() ? "incoming_file" : manifest.files[0].relativePath;
                        uint64_t firstFileSize = manifest.files.empty() ? manifest.totalBytes : manifest.files[0].fileSize;
                        jstring fileNameJ = env->NewStringUTF(firstFileName.c_str());

                        env->CallVoidMethod(bridge, mid, senderJ, batchIdJ, fileNameJ, (jlong)firstFileSize, (jint)manifest.files.size(), (jlong)manifest.totalBytes);
                        if (env->ExceptionCheck()) env->ExceptionClear();

                        env->DeleteLocalRef(senderJ);
                        env->DeleteLocalRef(batchIdJ);
                        env->DeleteLocalRef(fileNameJ);
                    }
                    env->DeleteLocalRef(clazz);
                }
                env->DeleteLocalRef(bridge);
            }
        }

        respondCb(true);
    });

    return app->initialize() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_aerosync_app_nativebridge_AeroSyncNativeBridge_nativeRespondPairing(JNIEnv* env, jobject thiz, jboolean accept) {
    std::function<void(bool)> cb;
    {
        std::lock_guard<std::mutex> lock(g_pairingMutex);
        cb = g_pendingPairingCallback;
        g_pendingPairingCallback = nullptr;
    }
    if (cb) {
        cb(accept == JNI_TRUE);
    }
}

JNIEXPORT void JNICALL
Java_com_aerosync_app_nativebridge_AeroSyncNativeBridge_nativeSetDownloadDirectory(JNIEnv* env, jobject thiz, jstring downloadDir) {
    auto app = getAppInstance();
    if (!app || !downloadDir) return;
    const char* pathStr = env->GetStringUTFChars(downloadDir, nullptr);
    if (pathStr) {
        app->setDownloadDirectory(pathStr);
        env->ReleaseStringUTFChars(downloadDir, pathStr);
    }
}

JNIEXPORT void JNICALL
Java_com_aerosync_app_nativebridge_AeroSyncNativeBridge_nativeShutdown(JNIEnv* env, jobject thiz) {
    std::shared_ptr<aerosync::AeroSyncApp> app;
    {
        std::lock_guard<std::mutex> lock(g_appMutex);
        app = g_app;
        g_app.reset();
    }
    if (app) {
        app->shutdown();
    }
    {
        std::lock_guard<std::mutex> lock(g_bridgeMutex);
        if (g_bridgeObj) {
            env->DeleteGlobalRef(g_bridgeObj);
            g_bridgeObj = nullptr;
        }
    }
}

JNIEXPORT jobjectArray JNICALL
Java_com_aerosync_app_nativebridge_AeroSyncNativeBridge_nativeGetPeers(JNIEnv* env, jobject thiz) {
    auto app = getAppInstance();
    if (!app) return nullptr;

    auto peers = app->getPeers();
    jclass stringClazz = env->FindClass("java/lang/String");
    jobjectArray result = env->NewObjectArray(static_cast<jsize>(peers.size()), stringClazz, nullptr);

    for (size_t i = 0; i < peers.size(); ++i) {
        std::string info = peers[i].deviceId + "|" + peers[i].deviceName + "|" +
                           aerosync::deviceTypeToString(peers[i].deviceType) + "|" +
                           peers[i].ipAddress + "|" + std::to_string(peers[i].port);
        jstring jStr = env->NewStringUTF(info.c_str());
        env->SetObjectArrayElement(result, static_cast<jsize>(i), jStr);
        env->DeleteLocalRef(jStr);
    }
    env->DeleteLocalRef(stringClazz);

    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_aerosync_app_nativebridge_AeroSyncNativeBridge_nativeSendFiles(
        JNIEnv* env, jobject thiz, jstring targetIp, jint targetPort, jobjectArray filePaths) {
    if (!targetIp || !filePaths) return JNI_FALSE;

    const char* ipStr = env->GetStringUTFChars(targetIp, nullptr);
    aerosync::PeerInfo target;
    target.ipAddress = ipStr ? ipStr : "";
    target.port = static_cast<uint16_t>(targetPort);
    if (ipStr) env->ReleaseStringUTFChars(targetIp, ipStr);

    jsize count = env->GetArrayLength(filePaths);
    std::vector<std::string> paths;
    for (jsize i = 0; i < count; ++i) {
        jstring pathStr = (jstring)env->GetObjectArrayElement(filePaths, i);
        if (pathStr) {
            const char* p = env->GetStringUTFChars(pathStr, nullptr);
            if (p) {
                paths.push_back(p);
                env->ReleaseStringUTFChars(pathStr, p);
            }
            env->DeleteLocalRef(pathStr);
        }
    }

    auto app = getAppInstance();
    if (!app) return JNI_FALSE;

    bool result = app->sendFiles(target, paths, [](const aerosync::TransferProgress& prog) {
        JniThreadAttacher attacher;
        JNIEnv* env = attacher.env;
        if (!env) return;
        jobject bridge = getSafeBridgeRef(env);
        if (bridge) {
            jclass clazz = env->GetObjectClass(bridge);
            if (clazz) {
                jmethodID mid = env->GetMethodID(clazz, "onNativeProgress", "(Ljava/lang/String;IJJDD)V");
                if (mid) {
                    jstring nameJ = env->NewStringUTF(prog.currentFileName.c_str());
                    env->CallVoidMethod(bridge, mid, nameJ, prog.currentFileIndex,
                                        prog.batchBytesTransferred, prog.batchTotalBytes,
                                        prog.speedBytesPerSec, prog.speedMbps);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    env->DeleteLocalRef(nameJ);
                }
                env->DeleteLocalRef(clazz);
            }
            env->DeleteLocalRef(bridge);
        }
    });

    return result ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_aerosync_app_nativebridge_AeroSyncNativeBridge_nativeConnectToPeer(
        JNIEnv* env, jobject thiz, jstring targetIp, jint targetPort, jstring pin) {
    if (!targetIp) return JNI_FALSE;

    const char* ipStr = env->GetStringUTFChars(targetIp, nullptr);
    const char* pinStr = pin ? env->GetStringUTFChars(pin, nullptr) : "";

    aerosync::PeerInfo target;
    target.ipAddress = ipStr ? ipStr : "";
    target.port = static_cast<uint16_t>(targetPort);

    auto app = getAppInstance();
    if (!app) {
        if (ipStr) env->ReleaseStringUTFChars(targetIp, ipStr);
        if (pin && pinStr) env->ReleaseStringUTFChars(pin, pinStr);
        return JNI_FALSE;
    }

    bool res = app->connectToPeer(target, pinStr ? pinStr : "");

    if (ipStr) env->ReleaseStringUTFChars(targetIp, ipStr);
    if (pin && pinStr) env->ReleaseStringUTFChars(pin, pinStr);

    return res ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_aerosync_app_nativebridge_AeroSyncNativeBridge_nativeCancelTransfer(JNIEnv* env, jobject thiz) {
    auto app = getAppInstance();
    if (app) {
        app->cancelTransfer();
    }
}

JNIEXPORT void JNICALL
Java_com_aerosync_app_nativebridge_AeroSyncNativeBridge_nativeAddBroadcastTarget(JNIEnv* env, jobject thiz, jstring ipStr) {
    if (!ipStr) return;
    auto app = getAppInstance();
    if (!app) return;
    const char* ipChars = env->GetStringUTFChars(ipStr, nullptr);
    if (ipChars) {
        app->addBroadcastTarget(ipChars);
        env->ReleaseStringUTFChars(ipStr, ipChars);
    }
}

} // extern "C"
