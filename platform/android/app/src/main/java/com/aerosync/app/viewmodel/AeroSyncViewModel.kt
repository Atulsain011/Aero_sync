package com.aerosync.app.viewmodel

import android.app.Application
import android.os.Environment
import android.os.StatFs
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.aerosync.app.data.db.AeroSyncDbHelper
import com.aerosync.app.data.preferences.AeroSyncPreferences
import com.aerosync.app.nativebridge.AeroSyncNativeBridge
import com.aerosync.app.service.AeroSyncTransferService
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import java.io.File
import java.util.UUID

enum class QueueItemStatus {
    WAITING,
    CONNECTING,
    QUEUED,
    TRANSFERRING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
}

data class TransferQueueItem(
    val id: String = UUID.randomUUID().toString(),
    val fileName: String,
    val fileSize: Long,
    val filePath: String,
    val status: QueueItemStatus = QueueItemStatus.QUEUED,
    val progressPercent: Int = 0,
    val transferredBytes: Long = 0L,
    val speedMbps: Double = 0.0,
    val etaSeconds: Int = 0,
    val isReceived: Boolean = false,
    val timestamp: Long = System.currentTimeMillis()
)

data class DiscoveredPeer(
    val deviceId: String,
    val deviceName: String,
    val deviceType: String,
    val ipAddress: String,
    val port: Int,
    val isOnline: Boolean = true
)

data class IncomingPairingPrompt(
    val senderId: String,
    val senderName: String,
    val pin: String
)

data class ActiveTransfer(
    val queueItemId: String = "",
    val fileName: String,
    val fileIndex: Int,
    val transferredBytes: Long,
    val totalBytes: Long,
    val speedMbps: Double,
    val etaSeconds: Int,
    val isPaused: Boolean = false,
    val isReceived: Boolean = false,
    val readSpeedMbps: Double = 0.0,
    val writeSpeedMbps: Double = 0.0
)

data class TransferHistoryItem(
    val id: String = UUID.randomUUID().toString(),
    val fileName: String,
    val fileSize: Long,
    val filePath: String,
    val isReceived: Boolean,
    val peerName: String = "",
    val timestamp: Long = System.currentTimeMillis(),
    val status: String = "COMPLETED",
    val durationMs: Long = 0L,
    val avgSpeedBps: Double = 0.0
)

data class SelectedFileItem(
    val id: String = UUID.randomUUID().toString(),
    val uriString: String,
    val fileName: String,
    val fileSize: Long,
    val mimeType: String = "",
    val formattedSize: String = ""
)

data class AeroSyncUiState(
    val deviceName: String = "AeroSync Device",
    val themeMode: com.aerosync.app.data.preferences.ThemeMode = com.aerosync.app.data.preferences.ThemeMode.DARK,
    val isDarkTheme: Boolean = true,
    val selectedTab: Int = 0,
    val storageUsedPercent: Int = 0,
    val freeSpaceText: String = "-- GB",
    val transferRateText: String = "0.0 MB/s",
    val connectionTypeLabel: String = "Wi-Fi / LAN",
    val linkSpeedMbps: Int = 0,
    val isHotspotConnected: Boolean = true,
    val downloadDirectory: String = "",
    val peers: List<DiscoveredPeer> = emptyList(),
    val selectedPeer: DiscoveredPeer? = null,
    val selectedFiles: List<SelectedFileItem> = emptyList(),
    val isWaitingForAcceptance: Boolean = false,
    val waitingPeerName: String = "",
    val activePin: String = "",
    val isPaired: Boolean = false,
    val pairingState: String = "UNPAIRED",
    val incomingPairingPrompt: IncomingPairingPrompt? = null,
    val activeTransfer: ActiveTransfer? = null,
    val isTransferring: Boolean = false,
    val transferQueue: List<TransferQueueItem> = emptyList(),
    val history: List<TransferHistoryItem> = emptyList(),
    val statusMessage: String = "Ready for peer sync"
)

class AeroSyncViewModel(application: Application) : AndroidViewModel(application), AeroSyncNativeBridge.NativeListener {

    private val prefs = AeroSyncPreferences.getInstance(application)
    private val dbHelper = AeroSyncDbHelper.getInstance(application)
    private val deviceId: String get() = prefs.deviceId
    private val nativeBridge = AeroSyncNativeBridge(this)

    private val _uiState = MutableStateFlow(
        AeroSyncUiState(
            deviceName = prefs.deviceName,
            themeMode = prefs.themeMode,
            isDarkTheme = prefs.isDarkTheme,
            downloadDirectory = prefs.downloadDirectory
        )
    )
    val uiState: StateFlow<AeroSyncUiState> = _uiState.asStateFlow()

    private val queueMutex = Mutex()
    private var isQueueWorkerRunning = false
    private var transferStartTimeMs = 0L
    private var lastProgressReportMs = 0L
    private var lastServiceNotificationMs = 0L

    init {
        // Load persistent data from SQLite and Preferences in background
        loadPersistentState()
        viewModelScope.launch(Dispatchers.IO) {
            updateStorageMetrics()
            updateNetworkDiagnostics()
            discoverAndRegisterBroadcastTargets()
        }
    }

    private fun loadPersistentState() {
        viewModelScope.launch(Dispatchers.IO) {
            val savedQueue = dbHelper.getAllQueueItems()
            val savedHistory = dbHelper.getAllHistoryItems()

            // Check if there are active queue items needing resumption
            val activeItem = savedQueue.firstOrNull { it.status == QueueItemStatus.TRANSFERRING || it.status == QueueItemStatus.PAUSED }
            val activeTransferObj = if (activeItem != null && activeItem.status == QueueItemStatus.TRANSFERRING) {
                ActiveTransfer(
                    queueItemId = activeItem.id,
                    fileName = activeItem.fileName,
                    fileIndex = 0,
                    transferredBytes = activeItem.transferredBytes,
                    totalBytes = activeItem.fileSize,
                    speedMbps = activeItem.speedMbps,
                    etaSeconds = activeItem.etaSeconds,
                    isPaused = activeItem.status == QueueItemStatus.PAUSED,
                    isReceived = activeItem.isReceived
                )
            } else null

            _uiState.update {
                it.copy(
                    transferQueue = savedQueue,
                    history = savedHistory,
                    isDarkTheme = prefs.isDarkTheme,
                    downloadDirectory = prefs.downloadDirectory,
                    deviceName = prefs.deviceName,
                    activeTransfer = activeTransferObj,
                    isTransferring = activeTransferObj != null
                )
            }
        }
    }

    fun toggleTheme() {
        val nextMode = when (_uiState.value.themeMode) {
            com.aerosync.app.data.preferences.ThemeMode.DARK -> com.aerosync.app.data.preferences.ThemeMode.LIGHT
            com.aerosync.app.data.preferences.ThemeMode.LIGHT -> com.aerosync.app.data.preferences.ThemeMode.DARK
        }
        prefs.themeMode = nextMode
        _uiState.update { it.copy(themeMode = nextMode, isDarkTheme = prefs.isDarkTheme) }
    }

    fun discoverAndRegisterBroadcastTargets() {
        viewModelScope.launch(Dispatchers.IO) {
            try {
                // Register common hotspot & USB tethering fallback targets
                val defaultTargets = listOf(
                    "192.168.42.255", "192.168.42.1", "192.168.42.129",
                    "192.168.43.255", "192.168.43.1",
                    "192.168.49.255", "192.168.49.1",
                    "192.168.137.255", "192.168.137.1",
                    "255.255.255.255"
                )
                for (t in defaultTargets) {
                    nativeBridge.nativeAddBroadcastTarget(t)
                }

                val ifaces = java.net.NetworkInterface.getNetworkInterfaces() ?: return@launch
                for (iface in ifaces.asSequence()) {
                    if (!iface.isUp || iface.isLoopback) continue
                    for (addr in iface.interfaceAddresses) {
                        val ip = addr.address
                        if (ip is java.net.Inet4Address) {
                            val bcast = addr.broadcast
                            if (bcast != null) {
                                val ipStr = bcast.hostAddress
                                if (!ipStr.isNullOrBlank()) {
                                    nativeBridge.nativeAddBroadcastTarget(ipStr)
                                }
                            } else {
                                // Compute subnet broadcast dynamically when bcast is null (common on Android AP/USB tethering)
                                val rawBytes = ip.address
                                val prefixLen = addr.networkPrefixLength.toInt().coerceIn(1, 31)
                                val maskInt = (0xFFFFFFFF.toLong() shl (32 - prefixLen)).toInt()
                                val ipInt = ((rawBytes[0].toInt() and 0xFF) shl 24) or
                                        ((rawBytes[1].toInt() and 0xFF) shl 16) or
                                        ((rawBytes[2].toInt() and 0xFF) shl 8) or
                                        (rawBytes[3].toInt() and 0xFF)
                                val bcastInt = ipInt or maskInt.inv()
                                val bcastBytes = byteArrayOf(
                                    ((bcastInt ushr 24) and 0xFF).toByte(),
                                    ((bcastInt ushr 16) and 0xFF).toByte(),
                                    ((bcastInt ushr 8) and 0xFF).toByte(),
                                    (bcastInt and 0xFF).toByte()
                                )
                                val calculatedBcast = java.net.InetAddress.getByAddress(bcastBytes).hostAddress
                                if (!calculatedBcast.isNullOrBlank()) {
                                    nativeBridge.nativeAddBroadcastTarget(calculatedBcast)
                                }
                                // Also add gateway .1 host IP
                                val gwBytes = byteArrayOf(rawBytes[0], rawBytes[1], rawBytes[2], 1.toByte())
                                val calculatedGw = java.net.InetAddress.getByAddress(gwBytes).hostAddress
                                if (!calculatedGw.isNullOrBlank()) {
                                    nativeBridge.nativeAddBroadcastTarget(calculatedGw)
                                }
                            }
                        }
                    }
                }
            } catch (_: Exception) {}
        }
    }

    fun addSelectedFiles(files: List<SelectedFileItem>) {
        if (files.isEmpty()) return
        _uiState.update { state ->
            val updated = (state.selectedFiles + files).distinctBy { it.uriString }
            state.copy(
                selectedFiles = updated,
                statusMessage = "${updated.size} file(s) staged for transfer"
            )
        }
    }

    fun removeSelectedFile(id: String) {
        _uiState.update { state ->
            val updated = state.selectedFiles.filter { it.id != id }
            state.copy(
                selectedFiles = updated,
                statusMessage = if (updated.isEmpty()) "Selected files cleared" else "${updated.size} file(s) staged"
            )
        }
    }

    fun clearSelectedFiles() {
        _uiState.update { state ->
            state.copy(
                selectedFiles = emptyList(),
                statusMessage = "Selected files cleared"
            )
        }
    }

    fun startTransferOfSelectedFiles() {
        val staged = _uiState.value.selectedFiles
        if (staged.isEmpty()) return

        val peer = _uiState.value.selectedPeer ?: _uiState.value.peers.firstOrNull()
        if (peer == null) {
            _uiState.update { it.copy(statusMessage = "Please select a target device in the Devices tab first.") }
            return
        }

        viewModelScope.launch(Dispatchers.IO) {
            val resolvedPaths = mutableListOf<String>()
            val queueItems = mutableListOf<TransferQueueItem>()

            val context = getApplication<Application>()
            val stagingDir = File(context.externalCacheDir ?: context.cacheDir, "transfer_staging").apply {
                if (!exists()) mkdirs()
            }

            for (item in staged) {
                val uri = android.net.Uri.parse(item.uriString)
                val path = com.aerosync.app.ui.MainActivity.resolveUriToFilePath(context, uri) ?: run {
                    val tempFile = File(stagingDir, item.fileName)
                    try {
                        context.contentResolver.openInputStream(uri)?.use { input ->
                            java.io.FileOutputStream(tempFile).use { output ->
                                input.copyTo(output)
                            }
                        }
                        tempFile.absolutePath
                    } catch (_: Exception) {
                        null
                    }
                }

                if (!path.isNullOrBlank()) {
                    resolvedPaths.add(path)
                    queueItems.add(
                        TransferQueueItem(
                            id = item.id,
                            fileName = item.fileName,
                            fileSize = if (item.fileSize > 0) item.fileSize else File(path).length(),
                            filePath = path,
                            status = QueueItemStatus.TRANSFERRING,
                            progressPercent = 0,
                            isReceived = false
                        )
                    )
                }
            }

            if (queueItems.isEmpty()) {
                _uiState.update { it.copy(statusMessage = "Unable to read selected files.") }
                return@launch
            }

            // Clear staged files and switch to Transfers
            _uiState.update { state ->
                state.copy(
                    selectedFiles = emptyList(),
                    selectedTab = 2,
                    selectedPeer = peer,
                    transferQueue = queueItems + state.transferQueue,
                    isTransferring = true,
                    activeTransfer = ActiveTransfer(
                        queueItemId = queueItems.first().id,
                        fileName = if (queueItems.size == 1) queueItems.first().fileName else "${queueItems.size} files batch",
                        fileIndex = 0,
                        transferredBytes = 0L,
                        totalBytes = queueItems.sumOf { it.fileSize },
                        speedMbps = 0.0,
                        etaSeconds = 0,
                        isReceived = false
                    ),
                    statusMessage = "Starting transfer of ${queueItems.size} file(s) to ${peer.deviceName}..."
                )
            }

            dbHelper.insertQueueItems(queueItems)
            startQueueProcessor()
        }
    }

    fun setSelectedTab(tab: Int) {
        _uiState.update { it.copy(selectedTab = tab) }
        updateStorageMetrics()
        updateNetworkDiagnostics()
    }

    fun setDownloadDirectory(path: String) {
        if (path.isBlank()) return
        prefs.downloadDirectory = path
        _uiState.update { it.copy(downloadDirectory = path, statusMessage = "Download folder: $path") }
        viewModelScope.launch(Dispatchers.IO) {
            nativeBridge.nativeSetDownloadDirectory(path)
            updateStorageMetrics()
        }
    }

    fun updateStorageMetrics() {
        try {
            val downloadDir = _uiState.value.downloadDirectory.ifBlank {
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).path
            }
            val targetDir = File(downloadDir)
            if (!targetDir.exists()) targetDir.mkdirs()

            val stat = StatFs(if (targetDir.exists()) targetDir.path else Environment.getDataDirectory().path)
            val bytesAvailable = stat.availableBytes
            val bytesTotal = stat.totalBytes
            val usedBytes = (bytesTotal - bytesAvailable).coerceAtLeast(0)
            val usedPct = if (bytesTotal > 0) ((usedBytes * 100) / bytesTotal).toInt() else 0
            val freeGb = bytesAvailable / (1024L * 1024L * 1024L)
            val freeText = if (freeGb >= 1) "$freeGb GB" else "${bytesAvailable / (1024L * 1024L)} MB"

            _uiState.update {
                it.copy(
                    storageUsedPercent = usedPct.coerceIn(0, 100),
                    freeSpaceText = freeText
                )
            }
        } catch (_: Exception) {
            // Keep existing metrics if unavailable
        }
    }

    fun updateNetworkDiagnostics() {
        try {
            val diag = AeroSyncTransferService.inspectNetworkDiagnostics(getApplication())
            _uiState.update {
                it.copy(
                    connectionTypeLabel = diag.transportName,
                    linkSpeedMbps = diag.linkSpeedMbps
                )
            }
        } catch (_: Exception) {}
    }

    fun initializeNativeEngine(customDownloadDir: String? = null) {
        val downloadDir = customDownloadDir ?: prefs.downloadDirectory
        viewModelScope.launch(Dispatchers.IO) {
            val name = prefs.deviceName
            nativeBridge.nativeInitialize(deviceId, name)
            nativeBridge.nativeSetDownloadDirectory(downloadDir)
            discoverAndRegisterBroadcastTargets()
            refreshPeers()
        }
    }

    fun updateDeviceName(newName: String) {
        if (newName.isBlank()) return
        prefs.deviceName = newName
        _uiState.update { it.copy(deviceName = newName, statusMessage = "Device name updated to $newName") }
        viewModelScope.launch(Dispatchers.IO) {
            nativeBridge.nativeShutdown()
            nativeBridge.nativeInitialize(deviceId, newName)
            nativeBridge.nativeSetDownloadDirectory(prefs.downloadDirectory)
            refreshPeers()
        }
    }

    fun selectPeer(peer: DiscoveredPeer) {
        val pin = (100000..999999).random().toString()
        _uiState.update {
            it.copy(
                selectedPeer = peer,
                isWaitingForAcceptance = true,
                waitingPeerName = peer.deviceName,
                activePin = pin,
                statusMessage = "Pairing with ${peer.deviceName} (PIN: $pin)..."
            )
        }
        viewModelScope.launch(Dispatchers.IO) {
            val paired = nativeBridge.nativeConnectToPeer(peer.ipAddress, peer.port, pin)
            _uiState.update {
                it.copy(
                    isWaitingForAcceptance = false,
                    isPaired = paired || peer.deviceId.startsWith("peer-") || peer.deviceId.startsWith("direct-"),
                    statusMessage = if (paired) "Authenticated with ${peer.deviceName}" else "Connected to ${peer.deviceName}"
                )
            }
        }
    }

    fun connectToDirectIp(ipAddress: String, port: Int = 48124, customName: String = "Direct Device") {
        val cleanIp = ipAddress.trim()
        val directPeer = DiscoveredPeer(
            deviceId = "direct-$cleanIp",
            deviceName = if (cleanIp == "192.168.43.1") "Hotspot Gateway ($cleanIp)" else customName,
            deviceType = "windows",
            ipAddress = cleanIp,
            port = port,
            isOnline = true
        )
        _uiState.update { state ->
            val updated = (listOf(directPeer) + state.peers).distinctBy { it.deviceId }
            state.copy(peers = updated, selectedPeer = directPeer)
        }
        selectPeer(directPeer)
    }

    // ==========================================
    // Persistent Transfer Queue Processing
    // ==========================================

    fun enqueueFiles(filePaths: List<String>) {
        if (filePaths.isEmpty()) return

        val newItems = filePaths.map { path ->
            val f = File(path)
            TransferQueueItem(
                id = UUID.randomUUID().toString(),
                fileName = f.name.ifEmpty { "file_${System.currentTimeMillis()}" },
                fileSize = if (f.exists()) f.length() else 1024L * 1024L,
                filePath = path,
                status = QueueItemStatus.TRANSFERRING,
                progressPercent = 0,
                isReceived = false
            )
        }

        val totalBatchBytes = newItems.sumOf { it.fileSize }
        val firstFileName = newItems.first().fileName

        // 1. INSTANT OPTIMISTIC UI: Immediately switch to Transfers screen and show item in queue
        _uiState.update { state ->
            val autoPeer = if (state.selectedPeer == null && state.peers.isNotEmpty()) state.peers.first() else state.selectedPeer
            state.copy(
                selectedTab = 2, // INSTANTLY switch to Transfers tab!
                selectedPeer = autoPeer,
                transferQueue = newItems + state.transferQueue,
                isTransferring = true,
                activeTransfer = ActiveTransfer(
                    queueItemId = newItems.first().id,
                    fileName = if (newItems.size == 1) firstFileName else "${newItems.size} files batch",
                    fileIndex = 0,
                    transferredBytes = 0L,
                    totalBytes = totalBatchBytes,
                    speedMbps = 0.0,
                    etaSeconds = 0,
                    isReceived = false
                ),
                statusMessage = "Transferring ${newItems.size} file(s)..."
            )
        }

        // 2. Persist to SQLite and start transfer in background
        viewModelScope.launch(Dispatchers.IO) {
            dbHelper.insertQueueItems(newItems)
            startQueueProcessor()
        }
    }

    private fun startQueueProcessor() {
        viewModelScope.launch(Dispatchers.IO) {
            queueMutex.withLock {
                if (isQueueWorkerRunning) return@withLock
                isQueueWorkerRunning = true
            }

            try {
                val peer = _uiState.value.selectedPeer
                if (peer == null) {
                    _uiState.update { state ->
                        state.copy(
                            statusMessage = "Please select a connected device or enter Direct IP to start transfer."
                        )
                    }
                    return@launch
                }

                val queuedItems = dbHelper.getAllQueueItems().filter {
                    it.status == QueueItemStatus.QUEUED || it.status == QueueItemStatus.PAUSED
                }
                if (queuedItems.isEmpty()) {
                    checkAndStopServiceIfQueueEmpty()
                    return@launch
                }

                val validPaths = queuedItems.map { it.filePath }
                val totalBatchBytes = queuedItems.sumOf { it.fileSize }
                val firstFileName = queuedItems.first().fileName

                // Mark items as TRANSFERRING in SQLite & State
                for (item in queuedItems) {
                    dbHelper.insertOrUpdateQueueItem(item.copy(status = QueueItemStatus.TRANSFERRING))
                }

                // 1. Immediately start Foreground Service for background execution & notification
                AeroSyncTransferService.startTransfer(
                    context = getApplication(),
                    fileName = if (queuedItems.size == 1) firstFileName else "${queuedItems.size} files",
                    totalBytes = totalBatchBytes,
                    isSender = true
                )

                transferStartTimeMs = System.currentTimeMillis()

                _uiState.update { state ->
                    state.copy(
                        transferQueue = dbHelper.getAllQueueItems(),
                        isTransferring = true,
                        activeTransfer = ActiveTransfer(
                            queueItemId = queuedItems.first().id,
                            fileName = if (queuedItems.size == 1) firstFileName else "${queuedItems.size} files batch",
                            fileIndex = 0,
                            transferredBytes = 0L,
                            totalBytes = totalBatchBytes,
                            speedMbps = 0.0,
                            etaSeconds = 0,
                            isReceived = false
                        ),
                        transferRateText = "0.0 MB/s",
                        statusMessage = "Transferring ${queuedItems.size} file(s) to ${peer.deviceName}..."
                    )
                }

                val success = nativeBridge.nativeSendFiles(
                    peer.ipAddress,
                    peer.port,
                    validPaths.toTypedArray()
                )

                val duration = (System.currentTimeMillis() - transferStartTimeMs).coerceAtLeast(1)
                val avgSpeed = if (duration > 0) (totalBatchBytes * 1000.0) / duration else 0.0

                if (success) {
                    for (item in queuedItems) {
                        val historyItem = TransferHistoryItem(
                            id = item.id,
                            fileName = item.fileName,
                            fileSize = item.fileSize,
                            filePath = item.filePath,
                            isReceived = false,
                            peerName = peer.deviceName,
                            timestamp = System.currentTimeMillis(),
                            status = "COMPLETED",
                            durationMs = duration,
                            avgSpeedBps = avgSpeed
                        )
                        dbHelper.completeTransferTransaction(item.id, historyItem)
                    }
                } else {
                    for (item in queuedItems) {
                        dbHelper.insertOrUpdateQueueItem(item.copy(status = QueueItemStatus.FAILED))
                    }
                }

                val updatedQueue = dbHelper.getAllQueueItems()
                val updatedHistory = dbHelper.getAllHistoryItems()

                _uiState.update { state ->
                    state.copy(
                        transferQueue = updatedQueue,
                        history = updatedHistory,
                        activeTransfer = null,
                        isTransferring = false,
                        transferRateText = "0.0 MB/s",
                        statusMessage = if (success) "Completed ${queuedItems.size} file(s)!" else "Transfer interrupted"
                    )
                }

                updateStorageMetrics()
                checkAndStopServiceIfQueueEmpty()
            } finally {
                queueMutex.withLock {
                    isQueueWorkerRunning = false
                }
            }
        }
    }

    private fun checkAndStopServiceIfQueueEmpty() {
        val activeOrQueued = dbHelper.getAllQueueItems().filter {
            it.status == QueueItemStatus.TRANSFERRING || it.status == QueueItemStatus.QUEUED
        }
        if (activeOrQueued.isEmpty()) {
            AeroSyncTransferService.stopTransfer(getApplication())
        }
    }

    fun removeQueueItem(id: String) {
        val activeId = _uiState.value.activeTransfer?.queueItemId
        if (id == activeId) {
            cancelActiveTransfer()
        }
        viewModelScope.launch(Dispatchers.IO) {
            dbHelper.deleteQueueItem(id)
            val updatedQueue = dbHelper.getAllQueueItems()
            _uiState.update { state ->
                state.copy(
                    transferQueue = updatedQueue,
                    statusMessage = "Item removed from queue."
                )
            }
            checkAndStopServiceIfQueueEmpty()
        }
    }

    fun clearCompletedQueue() {
        viewModelScope.launch(Dispatchers.IO) {
            val activeId = _uiState.value.activeTransfer?.queueItemId
            val pendingItems = dbHelper.getAllQueueItems().filter { it.id != activeId && it.status != QueueItemStatus.TRANSFERRING }
            pendingItems.forEach { dbHelper.deleteQueueItem(it.id) }
            val updated = dbHelper.getAllQueueItems()
            _uiState.update { state ->
                state.copy(
                    transferQueue = updated,
                    statusMessage = "Queue cleared."
                )
            }
            checkAndStopServiceIfQueueEmpty()
        }
    }

    fun clearHistory() {
        viewModelScope.launch(Dispatchers.IO) {
            dbHelper.clearHistory()
            _uiState.update { state ->
                state.copy(
                    history = emptyList(),
                    statusMessage = "Transfer history cleared."
                )
            }
        }
    }

    fun togglePauseTransfer() {
        val active = _uiState.value.activeTransfer ?: return
        val nextPaused = !active.isPaused

        if (nextPaused) {
            // Pause transfer safely without losing data
            viewModelScope.launch(Dispatchers.IO) {
                nativeBridge.nativeCancelTransfer()
                active.queueItemId.let { qId ->
                    val item = dbHelper.getAllQueueItems().firstOrNull { it.id == qId }
                    if (item != null) {
                        dbHelper.insertOrUpdateQueueItem(item.copy(status = QueueItemStatus.PAUSED))
                    }
                }
                _uiState.update { state ->
                    state.copy(
                        activeTransfer = active.copy(isPaused = true),
                        transferQueue = dbHelper.getAllQueueItems(),
                        isTransferring = false,
                        statusMessage = "Transfer paused."
                    )
                }
                AeroSyncTransferService.updateProgress(
                    context = getApplication(),
                    fileName = active.fileName,
                    transferred = active.transferredBytes,
                    total = active.totalBytes,
                    speedMbps = 0.0,
                    etaSeconds = 0,
                    isPaused = true
                )
            }
        } else {
            // Resume transfer seamlessly from checkpoint
            _uiState.update { state ->
                state.copy(
                    activeTransfer = active.copy(isPaused = false),
                    statusMessage = "Resuming transfer from verified chunk..."
                )
            }
            startQueueProcessor()
        }
    }

    fun cancelActiveTransfer() {
        // 1. Instant UI update - Clear active transfer and remove transferring items immediately
        _uiState.update { state ->
            val activeId = state.activeTransfer?.queueItemId
            val updatedQueue = state.transferQueue.filter {
                it.id != activeId && it.status != QueueItemStatus.TRANSFERRING
            }
            state.copy(
                isTransferring = false,
                activeTransfer = null,
                selectedPeer = null,
                isPaired = false,
                isWaitingForAcceptance = false,
                activePin = "",
                pairingState = "UNPAIRED",
                transferQueue = updatedQueue,
                transferRateText = "0.0 MB/s",
                statusMessage = "Transfer cancelled."
            )
        }

        // 2. Immediate Native Cancel & Service Stop in background
        viewModelScope.launch(Dispatchers.IO) {
            AeroSyncTransferService.stopTransfer(getApplication())
            nativeBridge.nativeCancelTransfer()
            val activeId = _uiState.value.activeTransfer?.queueItemId
            if (!activeId.isNullOrEmpty()) {
                dbHelper.deleteQueueItem(activeId)
            }
            val transferring = dbHelper.getAllQueueItems().filter { it.status == QueueItemStatus.TRANSFERRING }
            transferring.forEach { dbHelper.deleteQueueItem(it.id) }
            val updatedQueue = dbHelper.getAllQueueItems()
            _uiState.update { state ->
                state.copy(
                    transferQueue = updatedQueue
                )
            }
            checkAndStopServiceIfQueueEmpty()
        }
    }

    fun respondToPairingRequest(accept: Boolean) {
        val prompt = _uiState.value.incomingPairingPrompt
        viewModelScope.launch(Dispatchers.IO) {
            nativeBridge.nativeRespondPairing(accept)
        }
        _uiState.update { state ->
            val matchedPeer = if (accept && prompt != null) {
                state.peers.firstOrNull { it.deviceId == prompt.senderId || it.deviceName == prompt.senderName } ?: state.selectedPeer
            } else {
                state.selectedPeer
            }

            state.copy(
                incomingPairingPrompt = null,
                selectedPeer = matchedPeer,
                isPaired = accept,
                statusMessage = if (accept) "Pairing accepted & authenticated" else "Pairing declined"
            )
        }
    }

    // ==========================================
    // Native Bridge Callbacks
    // ==========================================

    override fun onPeersUpdated() {
        refreshPeers()
    }

    override fun onPairingRequest(senderId: String, senderName: String, pin: String) {
        _uiState.update {
            it.copy(
                incomingPairingPrompt = IncomingPairingPrompt(senderId, senderName, pin),
                statusMessage = "Incoming pairing request from $senderName with PIN $pin"
            )
        }
    }

    override fun onPairingStateChanged(state: String, reason: String) {
        _uiState.update { current ->
            if (state == "DISCONNECTED" || state == "UNPAIRED") {
                val updatedQueue = current.transferQueue.filter {
                    it.status != QueueItemStatus.TRANSFERRING
                }
                current.copy(
                    pairingState = state,
                    selectedPeer = null,
                    isPaired = false,
                    isWaitingForAcceptance = false,
                    activePin = "",
                    isTransferring = false,
                    activeTransfer = null,
                    transferQueue = updatedQueue,
                    transferRateText = "0.0 MB/s",
                    statusMessage = if (reason.isNotEmpty()) reason else "Disconnected"
                )
            } else {
                current.copy(
                    pairingState = state,
                    statusMessage = if (reason.isNotEmpty()) reason else "State: $state"
                )
            }
        }
        if (state == "DISCONNECTED" || state == "UNPAIRED") {
            AeroSyncTransferService.stopTransfer(getApplication())
        }
    }

    override fun onIncomingTransfer(
        senderName: String,
        batchId: String,
        fileName: String,
        fileSize: Long,
        totalFiles: Int,
        totalBytes: Long
    ) {
        // Receiver UI immediately displays the incoming file in Transfers section with 0ms delay
        val itemId = batchId.ifEmpty { UUID.randomUUID().toString() }
        val targetPath = File(prefs.downloadDirectory, fileName).absolutePath
        val newItem = TransferQueueItem(
            id = itemId,
            fileName = fileName,
            fileSize = if (fileSize > 0) fileSize else totalBytes,
            filePath = targetPath,
            status = QueueItemStatus.TRANSFERRING,
            progressPercent = 0,
            isReceived = true,
            timestamp = System.currentTimeMillis()
        )

        // 1. INSTANT UI UPDATE: Show item immediately in active queue and switch to Transfers view
        _uiState.update { state ->
            state.copy(
                selectedTab = 2, // Switch directly to Transfers section so user sees it instantly
                transferQueue = listOf(newItem) + state.transferQueue.filter { it.id != itemId },
                isTransferring = true,
                activeTransfer = ActiveTransfer(
                    queueItemId = itemId,
                    fileName = fileName,
                    fileIndex = 0,
                    transferredBytes = 0L,
                    totalBytes = if (fileSize > 0) fileSize else totalBytes,
                    speedMbps = 0.0,
                    etaSeconds = 0,
                    isReceived = true
                ),
                statusMessage = "Receiving $fileName from $senderName..."
            )
        }

        // 2. Persist to SQLite and start Foreground Service concurrently in background
        viewModelScope.launch(Dispatchers.IO) {
            dbHelper.insertOrUpdateQueueItem(newItem)
            AeroSyncTransferService.startTransfer(
                context = getApplication(),
                fileName = fileName,
                totalBytes = if (fileSize > 0) fileSize else totalBytes,
                isSender = false
            )
        }
    }

    override fun onProgress(
        fileName: String,
        fileIndex: Int,
        transferred: Long,
        total: Long,
        speedBps: Double,
        speedMbps: Double
    ) {
        val now = System.currentTimeMillis()
        val mbpsVal = if (speedMbps > 0) speedMbps else (speedBps * 8.0) / 1000000.0
        val mbPerSec = if (speedBps > 0) speedBps / (1024.0 * 1024.0) else (speedMbps / 8.0)
        val remainingBytes = if (total > transferred) total - transferred else 0L
        val etaSec = if (speedBps > 0) (remainingBytes / speedBps).toInt() else 0
        val rateText = if (mbPerSec >= 0.1) "%.1f MB/s".format(mbPerSec) else "${"%.1f".format(mbpsVal)} Mbps"
        val pct = if (total > 0) ((transferred * 100) / total).toInt().coerceIn(0, 100) else 0

        // Update foreground notification periodically (~800ms) or at completion to minimize IPC load
        if (now - lastServiceNotificationMs >= 800 || transferred >= total) {
            lastServiceNotificationMs = now
            AeroSyncTransferService.updateProgress(
                context = getApplication(),
                fileName = fileName,
                transferred = transferred,
                total = total,
                speedMbps = mbPerSec,
                etaSeconds = etaSec,
                isPaused = false
            )
        }

        // Smooth high-refresh UI progress updates (~30-60fps)
        if (now - lastProgressReportMs < 33 && transferred < total) {
            return
        }
        lastProgressReportMs = now

        val activeId = _uiState.value.activeTransfer?.queueItemId ?: ""

        _uiState.update { state ->
            val updatedQueue = if (activeId.isNotEmpty()) {
                state.transferQueue.map {
                    if (it.id == activeId || it.fileName == fileName) it.copy(
                        status = if (transferred >= total && total > 0) QueueItemStatus.COMPLETED else QueueItemStatus.TRANSFERRING,
                        progressPercent = pct,
                        transferredBytes = transferred,
                        speedMbps = mbPerSec,
                        etaSeconds = etaSec
                    ) else it
                }
            } else state.transferQueue

            state.copy(
                activeTransfer = ActiveTransfer(
                    queueItemId = activeId,
                    fileName = fileName,
                    fileIndex = fileIndex,
                    transferredBytes = transferred,
                    totalBytes = total,
                    speedMbps = mbPerSec,
                    etaSeconds = etaSec,
                    isReceived = state.activeTransfer?.isReceived ?: false
                ),
                transferQueue = updatedQueue,
                isTransferring = transferred < total,
                transferRateText = rateText,
                statusMessage = if (transferred >= total && total > 0) "Completed $fileName!" else "Streaming $fileName ($rateText, ETA: ${etaSec}s)"
            )
        }

        // Check if receiver just finished receiving the file
        if (transferred >= total && total > 0) {
            viewModelScope.launch(Dispatchers.IO) {
                updateStorageMetrics()

                val isRecv = _uiState.value.activeTransfer?.isReceived ?: false
                if (isRecv) {
                    val matchingItem = dbHelper.getAllQueueItems().firstOrNull {
                        it.id == activeId || it.fileName == fileName
                    }
                    if (matchingItem != null) {
                        val duration = (System.currentTimeMillis() - matchingItem.timestamp).coerceAtLeast(1)
                        val avgSpeed = (total * 1000.0) / duration
                        val historyItem = TransferHistoryItem(
                            id = matchingItem.id,
                            fileName = matchingItem.fileName,
                            fileSize = total,
                            filePath = matchingItem.filePath,
                            isReceived = true,
                            peerName = _uiState.value.selectedPeer?.deviceName ?: "Sender",
                            timestamp = System.currentTimeMillis(),
                            status = "COMPLETED",
                            durationMs = duration,
                            avgSpeedBps = avgSpeed
                        )
                        dbHelper.completeTransferTransaction(matchingItem.id, historyItem)

                        val updatedQueue = dbHelper.getAllQueueItems()
                        val updatedHistory = dbHelper.getAllHistoryItems()

                        _uiState.update { state ->
                            state.copy(
                                transferQueue = updatedQueue,
                                history = updatedHistory,
                                activeTransfer = null,
                                isTransferring = false,
                                transferRateText = "0.0 MB/s",
                                statusMessage = "Received $fileName successfully!"
                            )
                        }
                    }
                }
                checkAndStopServiceIfQueueEmpty()
            }
        }
    }

    private fun refreshPeers() {
        val peerStrs = nativeBridge.nativeGetPeers() ?: return
        val myId = deviceId
        val liveList = peerStrs.mapNotNull { str ->
            val parts = str.split("|")
            if (parts.size >= 5) {
                DiscoveredPeer(
                    deviceId = parts[0],
                    deviceName = parts[1],
                    deviceType = parts[2],
                    ipAddress = parts[3],
                    port = parts[4].toIntOrNull() ?: 48124,
                    isOnline = true
                )
            } else null
        }.filter {
            it.deviceId.isNotBlank() && it.deviceId != myId && it.ipAddress != "127.0.0.1" && it.ipAddress.isNotBlank()
        }.distinctBy { it.deviceId }

        _uiState.update { state ->
            state.copy(peers = liveList)
        }
    }

    override fun onCleared() {
        nativeBridge.nativeShutdown()
        AeroSyncTransferService.stopTransfer(getApplication())
        super.onCleared()
    }
}
