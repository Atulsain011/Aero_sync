package com.aerosync.app.service

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.wifi.WifiInfo
import android.net.wifi.WifiManager
import android.os.Binder
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import com.aerosync.app.ui.MainActivity

class AeroSyncTransferService : Service() {

    private var multicastLock: WifiManager.MulticastLock? = null
    private var wifiLock: WifiManager.WifiLock? = null
    private var wakeLock: PowerManager.WakeLock? = null
    private var isTransferActive = false
    private val binder = LocalBinder()

    inner class LocalBinder : Binder() {
        fun getService(): AeroSyncTransferService = this@AeroSyncTransferService
    }

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START_TRANSFER -> {
                val fileName = intent.getStringExtra(EXTRA_FILE_NAME) ?: "File"
                val totalBytes = intent.getLongExtra(EXTRA_TOTAL_BYTES, 0L)
                val isSender = intent.getBooleanExtra(EXTRA_IS_SENDER, true)
                handleStartTransfer(fileName, totalBytes, isSender)
            }
            ACTION_UPDATE_PROGRESS -> {
                val fileName = intent.getStringExtra(EXTRA_FILE_NAME) ?: "File"
                val transferred = intent.getLongExtra(EXTRA_TRANSFERRED_BYTES, 0L)
                val total = intent.getLongExtra(EXTRA_TOTAL_BYTES, 0L)
                val speedMbps = intent.getDoubleExtra(EXTRA_SPEED_MBPS, 0.0)
                val etaSeconds = intent.getIntExtra(EXTRA_ETA_SECONDS, 0)
                val isPaused = intent.getBooleanExtra(EXTRA_IS_PAUSED, false)
                handleUpdateProgress(fileName, transferred, total, speedMbps, etaSeconds, isPaused)
            }
            ACTION_STOP_TRANSFER -> {
                handleStopTransfer()
            }
        }
        return START_NOT_STICKY
    }

    override fun onBind(intent: Intent?): IBinder {
        return binder
    }

    override fun onDestroy() {
        releaseLocks()
        super.onDestroy()
    }

    private fun handleStartTransfer(fileName: String, totalBytes: Long, isSender: Boolean) {
        isTransferActive = true
        acquireLocks()

        val actionTitle = if (isSender) "Sending $fileName" else "Receiving $fileName"
        val totalMb = if (totalBytes > 0) totalBytes / (1024 * 1024) else 0
        val contentText = if (totalMb > 0) "Preparing transfer ($totalMb MB)..." else "Connecting..."

        val notification = buildProgressNotification(
            title = actionTitle,
            text = contentText,
            progress = 0,
            indeterminate = totalBytes <= 0
        )

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun handleUpdateProgress(
        fileName: String,
        transferred: Long,
        total: Long,
        speedMbps: Double,
        etaSeconds: Int,
        isPaused: Boolean
    ) {
        if (!isTransferActive) return

        val percent = if (total > 0) ((transferred * 100) / total).toInt().coerceIn(0, 100) else 0
        val transferredMb = transferred / (1024 * 1024)
        val totalMb = total / (1024 * 1024)

        val title = if (isPaused) "Paused: $fileName" else "Transferring $fileName ($percent%)"
        val text = if (isPaused) {
            "$transferredMb MB / $totalMb MB ($percent%) • Paused"
        } else {
            val speedText = if (speedMbps > 0) "%.1f MB/s".format(speedMbps) else "Calculating..."
            val etaText = if (etaSeconds > 0) " • ETA: ${etaSeconds}s" else ""
            "$transferredMb MB / $totalMb MB • $speedText$etaText"
        }

        val notification = buildProgressNotification(
            title = title,
            text = text,
            progress = percent,
            indeterminate = false
        )

        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as? NotificationManager
        manager?.notify(NOTIFICATION_ID, notification)
    }

    private fun handleStopTransfer() {
        isTransferActive = false
        releaseLocks()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            stopForeground(STOP_FOREGROUND_REMOVE)
        } else {
            @Suppress("DEPRECATION")
            stopForeground(true)
        }

        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as? NotificationManager
        manager?.cancel(NOTIFICATION_ID)

        stopSelf()
    }

    private fun buildProgressNotification(
        title: String,
        text: String,
        progress: Int,
        indeterminate: Boolean
    ): android.app.Notification {
        val openAppIntent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP
        }
        val pendingIntent = PendingIntent.getActivity(
            this,
            0,
            openAppIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or (if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) PendingIntent.FLAG_IMMUTABLE else 0)
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(title)
            .setContentText(text)
            .setSmallIcon(android.R.drawable.stat_sys_download)
            .setProgress(100, progress, indeterminate)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val manager = getSystemService(Context.NOTIFICATION_SERVICE) as? NotificationManager
            val channel = NotificationChannel(
                CHANNEL_ID,
                "AeroSync Active Transfers",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Shows progress during active file transfers"
                setShowBadge(false)
                enableVibration(false)
                setSound(null, null)
            }
            manager?.createNotificationChannel(channel)
        }
    }

    private fun acquireLocks() {
        try {
            val wifi = applicationContext.getSystemService(Context.WIFI_SERVICE) as? WifiManager
            
            // Multicast Lock for UDP discovery
            if (multicastLock == null) {
                multicastLock = wifi?.createMulticastLock("AeroSyncMulticastLock")?.apply {
                    setReferenceCounted(true)
                    acquire()
                }
            }

            // High Performance Wi-Fi Lock (prevents 802.11 power saving throttling)
            if (wifiLock == null) {
                val wifiLockMode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    WifiManager.WIFI_MODE_FULL_LOW_LATENCY
                } else {
                    @Suppress("DEPRECATION")
                    WifiManager.WIFI_MODE_FULL_HIGH_PERF
                }
                wifiLock = wifi?.createWifiLock(wifiLockMode, "AeroSyncHighPerfWifiLock")?.apply {
                    setReferenceCounted(false)
                    acquire()
                }
            }

            // Partial WakeLock to keep CPU active during multi-GB transfers
            if (wakeLock == null) {
                val powerManager = getSystemService(Context.POWER_SERVICE) as? PowerManager
                wakeLock = powerManager?.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "AeroSync:TransferWakeLock")?.apply {
                    setReferenceCounted(false)
                    acquire(15 * 60 * 1000L) // 15 min safe timeout
                }
            }
        } catch (e: Exception) {
            android.util.Log.w("AeroSyncService", "Error acquiring locks: ${e.message}")
        }
    }

    private fun releaseLocks() {
        try {
            multicastLock?.let { if (it.isHeld) it.release() }
            multicastLock = null

            wifiLock?.let { if (it.isHeld) it.release() }
            wifiLock = null

            wakeLock?.let { if (it.isHeld) it.release() }
            wakeLock = null
        } catch (e: Exception) {
            android.util.Log.w("AeroSyncService", "Error releasing locks: ${e.message}")
        }
    }

    companion object {
        const val CHANNEL_ID = "aerosync_transfers_channel"
        const val NOTIFICATION_ID = 2001

        const val ACTION_START_TRANSFER = "com.aerosync.app.action.START_TRANSFER"
        const val ACTION_UPDATE_PROGRESS = "com.aerosync.app.action.UPDATE_PROGRESS"
        const val ACTION_STOP_TRANSFER = "com.aerosync.app.action.STOP_TRANSFER"

        const val EXTRA_FILE_NAME = "extra_file_name"
        const val EXTRA_TOTAL_BYTES = "extra_total_bytes"
        const val EXTRA_TRANSFERRED_BYTES = "extra_transferred_bytes"
        const val EXTRA_SPEED_MBPS = "extra_speed_mbps"
        const val EXTRA_ETA_SECONDS = "extra_eta_seconds"
        const val EXTRA_IS_SENDER = "extra_is_sender"
        const val EXTRA_IS_PAUSED = "extra_is_paused"

        fun startTransfer(context: Context, fileName: String, totalBytes: Long, isSender: Boolean) {
            try {
                val intent = Intent(context, AeroSyncTransferService::class.java).apply {
                    action = ACTION_START_TRANSFER
                    putExtra(EXTRA_FILE_NAME, fileName)
                    putExtra(EXTRA_TOTAL_BYTES, totalBytes)
                    putExtra(EXTRA_IS_SENDER, isSender)
                }
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    context.startForegroundService(intent)
                } else {
                    context.startService(intent)
                }
            } catch (_: Exception) {}
        }

        fun updateProgress(
            context: Context,
            fileName: String,
            transferred: Long,
            total: Long,
            speedMbps: Double,
            etaSeconds: Int,
            isPaused: Boolean = false
        ) {
            try {
                val intent = Intent(context, AeroSyncTransferService::class.java).apply {
                    action = ACTION_UPDATE_PROGRESS
                    putExtra(EXTRA_FILE_NAME, fileName)
                    putExtra(EXTRA_TRANSFERRED_BYTES, transferred)
                    putExtra(EXTRA_TOTAL_BYTES, total)
                    putExtra(EXTRA_SPEED_MBPS, speedMbps)
                    putExtra(EXTRA_ETA_SECONDS, etaSeconds)
                    putExtra(EXTRA_IS_PAUSED, isPaused)
                }
                context.startService(intent)
            } catch (_: Exception) {}
        }

        fun stopTransfer(context: Context) {
            try {
                val intent = Intent(context, AeroSyncTransferService::class.java).apply {
                    action = ACTION_STOP_TRANSFER
                }
                context.startService(intent)
            } catch (_: Exception) {}
        }

        fun inspectNetworkDiagnostics(context: Context): NetworkDiagnostics {
            val cm = context.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
            val wifi = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as? WifiManager

            var transport = "Wi-Fi / LAN"
            var linkSpeedMbps = 0
            var isHighSpeed = false

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && cm != null) {
                val activeNetwork: Network? = cm.activeNetwork
                val caps = cm.getNetworkCapabilities(activeNetwork)
                if (caps != null) {
                    when {
                        caps.hasTransport(NetworkCapabilities.TRANSPORT_USB) -> {
                            transport = "USB Tethering"
                            isHighSpeed = true
                            linkSpeedMbps = 480
                        }
                        caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET) -> {
                            transport = "Gigabit Ethernet"
                            isHighSpeed = true
                            linkSpeedMbps = 1000
                        }
                        caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) -> {
                            @Suppress("DEPRECATION")
                            val wifiInfo: WifiInfo? = wifi?.connectionInfo
                            val speed = wifiInfo?.linkSpeed ?: 0
                            val freq = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) wifiInfo?.frequency ?: 0 else 0
                            linkSpeedMbps = if (speed > 0) speed else (caps.linkDownstreamBandwidthKbps / 1000)

                            val bandStr = when {
                                freq >= 5925 -> "6GHz Wi-Fi"
                                freq >= 4900 -> "5GHz Wi-Fi"
                                freq > 0 -> "2.4GHz Wi-Fi"
                                else -> "Wi-Fi"
                            }
                            transport = "$bandStr (${linkSpeedMbps} Mbps)"
                            isHighSpeed = linkSpeedMbps >= 200
                        }
                    }
                }
            }

            // Fallback: Check local network interface names for AP/Tethering
            if (transport == "Wi-Fi / LAN" || linkSpeedMbps == 0) {
                try {
                    val ifaces = java.net.NetworkInterface.getNetworkInterfaces()
                    if (ifaces != null) {
                        for (iface in ifaces.asSequence()) {
                            if (!iface.isUp || iface.isLoopback) continue
                            val name = iface.name.lowercase()
                            if (name.startsWith("rndis") || name.startsWith("usb") || name.startsWith("ncm")) {
                                transport = "USB Tethering (RNDIS)"
                                linkSpeedMbps = 480
                                isHighSpeed = true
                                break
                            } else if (name.startsWith("ap") || name.startsWith("swlan") || name.startsWith("softap")) {
                                transport = "Wi-Fi Hotspot"
                                linkSpeedMbps = 300
                                isHighSpeed = true
                                break
                            }
                        }
                    }
                } catch (_: Exception) {}
            }

            return NetworkDiagnostics(transport, linkSpeedMbps, isHighSpeed)
        }
    }
}

data class NetworkDiagnostics(
    val transportName: String,
    val linkSpeedMbps: Int,
    val isHighSpeed: Boolean
)
