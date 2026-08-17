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

data class AeroSyncUiState(
    val deviceName: String = "AeroSync Device",
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
    private val deviceId = UUID.randomUUID().toString().take(8)
    private val nativeBridge = AeroSyncNativeBridge(this)

    private val _uiState = MutableStateFlow(
        AeroSyncUiState(
            deviceName = prefs.deviceName,
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
        // Load persistent data from SQLite and Preferences
        loadPersistentState()
        updateStorageMetrics()
        updateNetworkDiagnostics()
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
        val nextTheme = !_uiState.value.isDarkTheme
        prefs.isDarkTheme = nextTheme
        _uiState.update { it.copy(isDarkTheme = nextTheme) }
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
                status = QueueItemStatus.QUEUED,
                progressPercent = 0,
                isReceived = false
            )
        }

        viewModelScope.launch(Dispatchers.IO) {
            dbHelper.insertQueueItems(newItems)
            val updatedQueue = dbHelper.getAllQueueItems()

            _uiState.update { state ->
                val autoPeer = if (state.selectedPeer == null && state.peers.isNotEmpty()) state.peers.first() else state.selectedPeer
                state.copy(
                    selectedPeer = autoPeer,
                    transferQueue = updatedQueue,
                    statusMessage = "Queued ${newItems.size} file(s) for transfer..."
                )
            }
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
        viewModelScope.launch(Dispatchers.IO) {
            nativeBridge.nativeCancelTransfer()
            val activeId = _uiState.value.activeTransfer?.queueItemId
            if (!activeId.isNullOrEmpty()) {
                val item = dbHelper.getAllQueueItems().firstOrNull { it.id == activeId }
                if (item != null) {
                    dbHelper.insertOrUpdateQueueItem(item.copy(status = QueueItemStatus.CANCELLED))
                }
            }
            val updatedQueue = dbHelper.getAllQueueItems()
            _uiState.update { state ->
                state.copy(
                    isTransferring = false,
                    activeTransfer = null,
                    transferQueue = updatedQueue,
                    transferRateText = "0.0 MB/s",
                    statusMessage = "Transfer cancelled."
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
        _uiState.update {
            it.copy(
                pairingState = state,
                statusMessage = if (reason.isNotEmpty()) reason else "State: $state"
            )
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
        // Receiver UI immediately displays the incoming file in Queue
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

        viewModelScope.launch(Dispatchers.IO) {
            dbHelper.insertOrUpdateQueueItem(newItem)
            val updatedQueue = dbHelper.getAllQueueItems()

            _uiState.update { state ->
                state.copy(
                    transferQueue = updatedQueue,
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

            // Start Foreground Service for background receiving
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

        // Update foreground notification every ~250ms or at completion
        if (now - lastServiceNotificationMs >= 250 || transferred >= total) {
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

        // Throttle progress updates to UI to avoid Compose recomposition flooding
        if (now - lastProgressReportMs < 100 && transferred < total) {
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
        }

        _uiState.update { state ->
            state.copy(peers = liveList.distinctBy { it.deviceId })
        }
    }

    override fun onCleared() {
        nativeBridge.nativeShutdown()
        AeroSyncTransferService.stopTransfer(getApplication())
        super.onCleared()
    }
}
