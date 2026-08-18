package com.aerosync.app.nativebridge

import android.util.Log

class AeroSyncNativeBridge(private val listener: NativeListener) {

    interface NativeListener {
        fun onPeersUpdated()
        fun onPairingRequest(senderId: String, senderName: String, pin: String)
        fun onPairingStateChanged(state: String, reason: String)
        fun onProgress(fileName: String, fileIndex: Int, transferred: Long, total: Long, speedBps: Double, speedMbps: Double)
        fun onIncomingTransfer(senderName: String, batchId: String, fileName: String, fileSize: Long, totalFiles: Int, totalBytes: Long)
    }

    companion object {
        init {
            try {
                System.loadLibrary("aerosync_jni")
            } catch (e: UnsatisfiedLinkError) {
                Log.e("AeroSyncNativeBridge", "Failed to load aerosync_jni library: ${e.message}")
            }
        }
    }

    external fun nativeInitialize(deviceId: String, deviceName: String): Boolean
    external fun nativeShutdown()
    external fun nativeGetPeers(): Array<String>?
    external fun nativeConnectToPeer(targetIp: String, targetPort: Int, pin: String): Boolean
    external fun nativeSendFiles(targetIp: String, targetPort: Int, filePaths: Array<String>): Boolean
    external fun nativeCancelTransfer()
    external fun nativeRespondPairing(accept: Boolean)
    external fun nativeSetDownloadDirectory(downloadDir: String)
    external fun nativeAddBroadcastTarget(ip: String)

    // Callbacks from C++ JNI
    fun onNativePeersUpdated() {
        listener.onPeersUpdated()
    }

    fun onNativePairingRequest(senderId: String, senderName: String, pin: String) {
        listener.onPairingRequest(senderId, senderName, pin)
    }

    fun onNativePairingStateChanged(state: String, reason: String) {
        listener.onPairingStateChanged(state, reason)
    }

    fun onNativeProgress(fileName: String, fileIndex: Int, transferred: Long, total: Long, speedBps: Double, speedMbps: Double) {
        listener.onProgress(fileName, fileIndex, transferred, total, speedBps, speedMbps)
    }

    fun onNativeIncomingTransfer(senderName: String, batchId: String, fileName: String, fileSize: Long, totalFiles: Int, totalBytes: Long) {
        listener.onIncomingTransfer(senderName, batchId, fileName, fileSize, totalFiles, totalBytes)
    }
}
