package com.aerosync.app.viewmodel

import android.app.Application
import android.net.ConnectivityManager
import android.net.LinkProperties
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
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

enum class TransferUiState {
    IDLE,
    FILE_SELECTED,
    PREPARING,
    WAITING_FOR_DEVICE,
    WAITING_FOR_ACCEPT,
    TRANSFERRING,
    COMPLETED,
    FAILED,
    CANCELLED
}

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
    val activeSendTransfer: ActiveTransfer? = null,
    val activeReceiveTransfer: ActiveTransfer? = null,
    val isTransferring: Boolean = false,
    val isPreparing: Boolean = false,
    val transferUiState: TransferUiState = TransferUiState.IDLE,
    val lastCompletedFileName: String = "",
    val lastCompletedFileSize: Long = 0L,
    val lastCompletedFilePath: String = "",
    val transferErrorMessage: String = "",
    val transferQueue: List<TransferQueueItem> = emptyList(),
    val history: List<TransferHistoryItem> = emptyList(),
    val statusMessage: String = "Ready for peer sync"
)

class AeroSyncViewModel(application: Application) : AndroidViewModel(application), AeroSyncNativeBridge.NativeListener {

    private val prefs = AeroSyncPreferences.getInstance(application)
    private val dbHelper = AeroSyncDbHelper.getInstance(application)
    private val deviceId: String get() = prefs.deviceId
    private val nativeBridge = AeroSyncNativeBridge(this)
    private val activeDescriptors = java.util.concurrent.ConcurrentHashMap<String, android.os.ParcelFileDescriptor>()

    private fun closeDescriptors() {
        activeDescriptors.values.forEach { pfd ->
            try { pfd.close() } catch (_: Exception) {}
        }
        activeDescriptors.clear()
    }

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
        initializeNativeEngine()
        viewModelScope.launch(Dispatchers.IO) {
            updateStorageMetrics()
            updateNetworkDiagnostics()
            
            // Continuous high-frequency peer refresh ticker (350ms) for sub-second UI updates
            while (true) {
                refreshPeers()
                kotlinx.coroutines.delay(350)
            }
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

            val cleanQueue = savedQueue.filter { it.status != QueueItemStatus.COMPLETED && it.id !in savedHistory.map { h -> h.id }.toSet() }

            _uiState.update {
                it.copy(
                    transferQueue = cleanQueue,
                    history = savedHistory.distinctBy { h -> h.id },
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
            val nextState = if (state.transferUiState == TransferUiState.IDLE || state.transferUiState == TransferUiState.CANCELLED || state.transferUiState == TransferUiState.COMPLETED || state.transferUiState == TransferUiState.FAILED) {
                TransferUiState.FILE_SELECTED
            } else state.transferUiState

            state.copy(
                selectedFiles = updated,
                transferUiState = nextState,
                statusMessage = "${updated.size} file(s) ready to send"
            )
        }
    }

    fun removeSelectedFile(id: String) {
        _uiState.update { state ->
            val updated = state.selectedFiles.filter { it.id != id }
            val nextState = if (updated.isEmpty() && state.transferUiState == TransferUiState.FILE_SELECTED) TransferUiState.IDLE else state.transferUiState
            state.copy(
                selectedFiles = updated,
                transferUiState = nextState,
                statusMessage = if (updated.isEmpty()) "Selected files cleared" else "${updated.size} file(s) staged"
            )
        }
    }

    fun clearSelectedFiles() {
        _uiState.update { state ->
            val nextState = if (state.transferUiState == TransferUiState.FILE_SELECTED) TransferUiState.IDLE else state.transferUiState
            state.copy(
                selectedFiles = emptyList(),
                transferUiState = nextState,
                statusMessage = "Selected files cleared"
            )
        }
    }

    fun resetTransferState() {
        _uiState.update { state ->
            state.copy(
                transferUiState = TransferUiState.IDLE,
                selectedFiles = emptyList(),
                activeTransfer = null,
                isTransferring = false,
                isPreparing = false,
                transferErrorMessage = "",
                statusMessage = "Ready for peer sync"
            )
        }
    }

    fun retryTransfer() {
        val state = _uiState.value
        if (state.selectedFiles.isNotEmpty()) {
            startTransferOfSelectedFiles()
        } else if (state.transferQueue.isNotEmpty()) {
            startQueueProcessor()
        } else {
            resetTransferState()
        }
    }

    fun startTransferOfSelectedFiles() {
        val staged = _uiState.value.selectedFiles
        if (staged.isEmpty()) return

        val peer = _uiState.value.selectedPeer ?: _uiState.value.peers.firstOrNull()
        if (peer == null) {
            _uiState.update { it.copy(statusMessage = "Please select a target device first.") }
            return
        }

        // Set PREPARING state immediately upon tapping Send
        _uiState.update { state ->
            state.copy(
                transferUiState = TransferUiState.PREPARING,
                isPreparing = true,
                selectedPeer = peer,
                statusMessage = "Preparing file..."
            )
        }

        viewModelScope.launch(Dispatchers.IO) {
            val resolvedPaths = mutableListOf<String>()
            val queueItems = mutableListOf<TransferQueueItem>()
            val context = getApplication<Application>()

            for (item in staged) {
                val uri = android.net.Uri.parse(item.uriString)
                val resolved = com.aerosync.app.ui.MainActivity.resolveFileForTransfer(context, uri)

                if (resolved.pfd != null) {
                    activeDescriptors[item.id] = resolved.pfd
                }

                val finalName = if (item.fileName.isNotBlank()) item.fileName else resolved.fileName
                val finalSize = if (resolved.fileSize > 0) resolved.fileSize else item.fileSize

                resolvedPaths.add(resolved.filePath)
                queueItems.add(
                    TransferQueueItem(
                        id = item.id,
                        fileName = finalName,
                        fileSize = finalSize,
                        filePath = resolved.filePath,
                        status = QueueItemStatus.QUEUED,
                        progressPercent = 0,
                        isReceived = false
                    )
                )
            }

            if (queueItems.isEmpty()) {
                _uiState.update { it.copy(transferUiState = TransferUiState.FAILED, transferErrorMessage = "Unable to read selected files.", statusMessage = "Unable to read selected files.") }
                return@launch
            }

            val firstItem = queueItems.first()

            _uiState.update { state ->
                val firstActive = ActiveTransfer(
                    queueItemId = firstItem.id,
                    fileName = firstItem.fileName,
                    fileIndex = 0,
                    transferredBytes = 0L,
                    totalBytes = firstItem.fileSize,
                    speedMbps = 0.0,
                    etaSeconds = 0,
                    isReceived = false
                )
                state.copy(
                    isPreparing = false,
                    transferUiState = TransferUiState.WAITING_FOR_DEVICE,
                    selectedPeer = peer,
                    transferQueue = queueItems + state.transferQueue,
                    isTransferring = true,
                    activeSendTransfer = firstActive,
                    activeTransfer = firstActive,
                    statusMessage = "Waiting for device ${peer.deviceName}..."
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
        _uiState.update {
            it.copy(
                selectedPeer = peer,
                isWaitingForAcceptance = false,
                waitingPeerName = "",
                activePin = "",
                isPaired = true,
                pairingState = "PAIRED",
                statusMessage = "Connected to ${peer.deviceName}"
            )
        }
        viewModelScope.launch(Dispatchers.IO) {
            nativeBridge.nativeConnectToPeer(peer.ipAddress, peer.port, "")
        }
    }

    fun connectToDirectIp(ipAddress: String, port: Int = 48124, customName: String = "Direct Device") {
        val cleanIp = ipAddress.trim()
        if (cleanIp.isBlank()) return
        nativeBridge.nativeAddBroadcastTarget(cleanIp)
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

        val firstItem = newItems.first()

        // 1. INSTANT OPTIMISTIC UI: Immediately switch to Transfers screen and show item in queue
        _uiState.update { state ->
            val autoPeer = if (state.selectedPeer == null && state.peers.isNotEmpty()) state.peers.first() else state.selectedPeer
            val firstActive = ActiveTransfer(
                queueItemId = firstItem.id,
                fileName = firstItem.fileName,
                fileIndex = 0,
                transferredBytes = 0L,
                totalBytes = firstItem.fileSize,
                speedMbps = 0.0,
                etaSeconds = 0,
                isReceived = false
            )
            state.copy(
                selectedTab = 2, // INSTANTLY switch to Transfers tab!
                selectedPeer = autoPeer,
                transferQueue = newItems + state.transferQueue,
                isTransferring = true,
                activeSendTransfer = firstActive,
                activeTransfer = firstActive,
                statusMessage = "Transferring ${firstItem.fileName}..."
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
                while (true) {
                    val peer = _uiState.value.selectedPeer ?: _uiState.value.peers.firstOrNull()
                    if (peer == null) {
                        _uiState.update { state ->
                            state.copy(
                                statusMessage = "Please select a connected device or enter Direct IP to start transfer."
                            )
                        }
                        break
                    }

                    val queuedItems = dbHelper.getAllQueueItems().filter {
                        (!it.isReceived) && (it.status == QueueItemStatus.QUEUED || it.status == QueueItemStatus.PAUSED || it.status == QueueItemStatus.TRANSFERRING)
                    }
                    if (queuedItems.isEmpty()) {
                        break
                    }

                    val nextItem = queuedItems.first()

                    // Mark current single item as TRANSFERRING in SQLite & State
                    dbHelper.insertOrUpdateQueueItem(nextItem.copy(status = QueueItemStatus.TRANSFERRING))

                    AeroSyncTransferService.startTransfer(
                        context = getApplication(),
                        fileName = nextItem.fileName,
                        totalBytes = nextItem.fileSize,
                        isSender = true
                    )

                    transferStartTimeMs = System.currentTimeMillis()

                    val currentActiveSend = ActiveTransfer(
                        queueItemId = nextItem.id,
                        fileName = nextItem.fileName,
                        fileIndex = 0,
                        transferredBytes = 0L,
                        totalBytes = nextItem.fileSize,
                        speedMbps = 0.0,
                        etaSeconds = 0,
                        isReceived = false
                    )

                    _uiState.update { state ->
                        state.copy(
                            transferQueue = dbHelper.getAllQueueItems(),
                            isTransferring = true,
                            transferUiState = TransferUiState.TRANSFERRING,
                            activeSendTransfer = currentActiveSend,
                            activeTransfer = currentActiveSend,
                            transferRateText = "0.0 MB/s",
                            statusMessage = "Sending ${nextItem.fileName} to ${peer.deviceName}..."
                        )
                    }

                    var success = false
                    var targetIp = peer.ipAddress
                    var targetPort = peer.port

                    for (attempt in 1..3) {
                        success = nativeBridge.nativeSendFiles(
                            targetIp,
                            targetPort,
                            arrayOf(nextItem.filePath)
                        )
                        if (success || _uiState.value.transferUiState == TransferUiState.CANCELLED) {
                            break
                        }
                        val currentPeer = _uiState.value.selectedPeer
                        _uiState.update { it.copy(statusMessage = "Network/IP changed. Reconnecting peer (attempt $attempt/3)...") }
                        kotlinx.coroutines.delay(1200)
                        refreshPeers()
                        val livePeer = _uiState.value.peers.firstOrNull { p ->
                            p.deviceId == peer.deviceId || (currentPeer != null && p.deviceId == currentPeer.deviceId)
                        }
                        if (livePeer != null) {
                            targetIp = livePeer.ipAddress
                            targetPort = livePeer.port
                            _uiState.update { it.copy(selectedPeer = livePeer) }
                            nativeBridge.nativeConnectToPeer(targetIp, targetPort, _uiState.value.activePin)
                        }
                    }

                    val duration = (System.currentTimeMillis() - transferStartTimeMs).coerceAtLeast(1)
                    val avgSpeed = if (duration > 0) (nextItem.fileSize * 1000.0) / duration else 0.0

                    val historyItem = TransferHistoryItem(
                        id = nextItem.id,
                        fileName = nextItem.fileName,
                        fileSize = nextItem.fileSize,
                        filePath = nextItem.filePath,
                        isReceived = false,
                        peerName = peer.deviceName,
                        timestamp = System.currentTimeMillis(),
                        status = if (success) "COMPLETED" else (if (_uiState.value.transferUiState == TransferUiState.CANCELLED) "CANCELLED" else "FAILED"),
                        durationMs = duration,
                        avgSpeedBps = if (success) avgSpeed else 0.0
                    )

                    dbHelper.completeTransferTransaction(nextItem.id, historyItem)

                    val updatedQueue = dbHelper.getAllQueueItems()
                    val updatedHistory = dbHelper.getAllHistoryItems().distinctBy { it.id }
                    val historyIds = updatedHistory.map { it.id }.toSet()
                    val cleanQueue = updatedQueue.filter { it.id !in historyIds && it.status != QueueItemStatus.COMPLETED }

                    _uiState.update { state ->
                        val nextRecv = state.activeReceiveTransfer
                        val isStillTransferring = nextRecv != null
                        state.copy(
                            transferQueue = cleanQueue,
                            history = updatedHistory,
                            activeSendTransfer = null,
                            activeTransfer = nextRecv,
                            isTransferring = isStillTransferring,
                            transferUiState = if (isStillTransferring) TransferUiState.TRANSFERRING else TransferUiState.IDLE,
                            lastCompletedFileName = if (success) nextItem.fileName else state.lastCompletedFileName,
                            lastCompletedFileSize = if (success) nextItem.fileSize else state.lastCompletedFileSize,
                            statusMessage = if (success) "Completed ${nextItem.fileName}" else "Transfer interrupted"
                        )
                    }

                    updateStorageMetrics()
                    if (_uiState.value.transferUiState == TransferUiState.CANCELLED) {
                        break
                    }
                }

                checkAndStopServiceIfQueueEmpty()
            } finally {
                closeDescriptors()
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

    init {
        registerNetworkCallback()
    }

    private fun registerNetworkCallback() {
        try {
            val connectivityManager = getApplication<Application>().getSystemService(android.content.Context.CONNECTIVITY_SERVICE) as? android.net.ConnectivityManager
            connectivityManager?.registerDefaultNetworkCallback(object : android.net.ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: android.net.Network) {
                    onNetworkOrIpChanged()
                }
                override fun onLinkPropertiesChanged(network: android.net.Network, linkProperties: android.net.LinkProperties) {
                    onNetworkOrIpChanged()
                }
                override fun onLost(network: android.net.Network) {
                    onNetworkOrIpChanged()
                }
            })
        } catch (_: Exception) {}
    }

    private fun onNetworkOrIpChanged() {
        viewModelScope.launch(Dispatchers.IO) {
            updateNetworkDiagnostics()
            val state = _uiState.value
            val isBusy = state.isTransferring || state.activeTransfer != null
            if (!isBusy) {
                val name = prefs.deviceName
                nativeBridge.nativeInitialize(deviceId, name)
                nativeBridge.nativeSetDownloadDirectory(prefs.downloadDirectory)
            }
            discoverAndRegisterBroadcastTargets()
            refreshPeers()

            val peer = state.selectedPeer
            if (peer != null) {
                val livePeer = state.peers.firstOrNull { it.deviceId == peer.deviceId || it.deviceName == peer.deviceName }
                if (livePeer != null && livePeer.ipAddress != peer.ipAddress) {
                    _uiState.update { it.copy(selectedPeer = livePeer, statusMessage = "Peer IP updated to ${livePeer.ipAddress}. Reconnecting...") }
                    nativeBridge.nativeConnectToPeer(livePeer.ipAddress, livePeer.port, state.activePin)
                }
            }
        }
    }

    fun removeHistoryItem(id: String) {
        _uiState.update { state ->
            val updated = state.history.filter { it.id != id }
            state.copy(history = updated, statusMessage = "History item removed.")
        }
        viewModelScope.launch(Dispatchers.IO) {
            dbHelper.deleteHistoryItem(id)
        }
    }

    fun clearHistory() {
        _uiState.update { state ->
            state.copy(
                history = emptyList(),
                statusMessage = "Transfer history cleared."
            )
        }
        viewModelScope.launch(Dispatchers.IO) {
            dbHelper.clearHistory()
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
        val active = _uiState.value.activeTransfer
        val activeId = active?.queueItemId
        val cancelledHistoryItem = if (active != null) {
            TransferHistoryItem(
                id = activeId?.ifEmpty { UUID.randomUUID().toString() } ?: UUID.randomUUID().toString(),
                fileName = active.fileName,
                fileSize = active.totalBytes,
                filePath = "",
                isReceived = active.isReceived,
                peerName = _uiState.value.selectedPeer?.deviceName ?: "Peer",
                timestamp = System.currentTimeMillis(),
                status = "CANCELLED"
            )
        } else null

        // 1. Instant UI update - Clear active transfer immediately and show CANCELLED item in history
        _uiState.update { state ->
            val updatedQueue = state.transferQueue.filter {
                it.id != activeId && it.status != QueueItemStatus.TRANSFERRING
            }
            val updatedHistory = if (cancelledHistoryItem != null) {
                (listOf(cancelledHistoryItem) + state.history).distinctBy { it.id }
            } else state.history

            state.copy(
                isTransferring = false,
                isPreparing = false,
                transferUiState = TransferUiState.IDLE,
                activeTransfer = null,
                selectedPeer = null,
                isPaired = false,
                isWaitingForAcceptance = false,
                activePin = "",
                pairingState = "UNPAIRED",
                transferQueue = updatedQueue,
                history = updatedHistory,
                transferRateText = "0.0 MB/s",
                statusMessage = "Transfer cancelled."
            )
        }

        // 2. Immediate Native Cancel & Service Stop in background
        viewModelScope.launch(Dispatchers.IO) {
            closeDescriptors()
            AeroSyncTransferService.stopTransfer(getApplication())
            nativeBridge.nativeCancelTransfer()
            if (cancelledHistoryItem != null) {
                dbHelper.completeTransferTransaction(cancelledHistoryItem.id, cancelledHistoryItem)
            }
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
        // Auto-accept incoming pairing on same network without showing PIN modal
        respondToPairingRequest(true)
    }

    override fun onPairingStateChanged(state: String, reason: String) {
        _uiState.update { current ->
            if (state == "DISCONNECTED" || state == "UNPAIRED") {
                val updatedQueue = current.transferQueue.filter {
                    it.status != QueueItemStatus.TRANSFERRING
                }
                val isCancelled = reason.contains("cancel", ignoreCase = true) || current.isTransferring
                val cancelledHistoryItem = if (isCancelled && current.activeTransfer != null) {
                    val active = current.activeTransfer
                    TransferHistoryItem(
                        id = active.queueItemId.ifEmpty { UUID.randomUUID().toString() },
                        fileName = active.fileName,
                        fileSize = active.totalBytes,
                        filePath = "",
                        isReceived = active.isReceived,
                        peerName = current.selectedPeer?.deviceName ?: "Peer",
                        timestamp = System.currentTimeMillis(),
                        status = "CANCELLED"
                    )
                } else null

                val updatedHistory = if (cancelledHistoryItem != null) {
                    (listOf(cancelledHistoryItem) + current.history).distinctBy { it.id }
                } else current.history

                current.copy(
                    pairingState = state,
                    selectedPeer = null,
                    isPaired = false,
                    isWaitingForAcceptance = false,
                    activePin = "",
                    isTransferring = false,
                    activeTransfer = null,
                    transferUiState = TransferUiState.IDLE,
                    transferErrorMessage = if (isCancelled) "Transfer cancelled by peer" else (if (reason.isNotEmpty()) reason else "Device disconnected"),
                    transferQueue = updatedQueue,
                    history = updatedHistory,
                    transferRateText = "0.0 MB/s",
                    statusMessage = if (isCancelled) "Transfer cancelled by peer." else (if (reason.isNotEmpty()) reason else "Disconnected")
                )
            } else {
                current.copy(
                    pairingState = state,
                    statusMessage = if (reason.isNotEmpty()) reason else "State: $state"
                )
            }
        }
        AeroSyncTransferService.stopTransfer(getApplication())
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

        val activeRecv = ActiveTransfer(
            queueItemId = itemId,
            fileName = fileName,
            fileIndex = 0,
            transferredBytes = 0L,
            totalBytes = if (fileSize > 0) fileSize else totalBytes,
            speedMbps = 0.0,
            etaSeconds = 0,
            isReceived = true
        )

        // 1. INSTANT UI UPDATE: Set transferUiState to TRANSFERRING and create activeReceiveTransfer card
        _uiState.update { state ->
            state.copy(
                transferQueue = listOf(newItem) + state.transferQueue.filter { it.id != itemId },
                isTransferring = true,
                transferUiState = TransferUiState.TRANSFERRING,
                activeReceiveTransfer = activeRecv,
                activeTransfer = state.activeSendTransfer ?: activeRecv,
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

        val activeId = _uiState.value.activeTransfer?.queueItemId ?: ""
        val isDone = (total > 0 && transferred >= total) || (total > 0 && pct >= 100) || (total > 0 && transferred >= (total - 1024) && speedBps == 0.0)

        // Update foreground notification periodically (~800ms) or at completion to minimize IPC load
        if (now - lastServiceNotificationMs >= 800 || isDone) {
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

        // Smooth high-refresh UI progress updates (~30-60fps) - bypass throttle if completion reached
        if (!isDone && now - lastProgressReportMs < 33 && transferred < total) {
            return
        }
        lastProgressReportMs = now

        _uiState.update { state ->
            val isRecv = (state.activeReceiveTransfer != null && (state.activeReceiveTransfer.fileName == fileName || state.activeReceiveTransfer.queueItemId == activeId))
            val activePath = File(prefs.downloadDirectory, fileName).absolutePath
            val completedItem = if (isDone) {
                TransferHistoryItem(
                    id = (if (isRecv) state.activeReceiveTransfer?.queueItemId else state.activeSendTransfer?.queueItemId) ?: activeId.ifEmpty { UUID.randomUUID().toString() },
                    fileName = fileName,
                    fileSize = total,
                    filePath = activePath,
                    isReceived = isRecv,
                    peerName = state.selectedPeer?.deviceName ?: if (isRecv) "Sender" else "Receiver",
                    timestamp = System.currentTimeMillis(),
                    status = "COMPLETED",
                    avgSpeedBps = speedBps
                )
            } else null

            val updatedHistory = if (completedItem != null) {
                (listOf(completedItem) + state.history).distinctBy { it.id }
            } else state.history

            val updatedQueue = if (isDone) {
                state.transferQueue.filter { it.id != activeId && it.fileName != fileName && it.status != QueueItemStatus.COMPLETED }
            } else if (activeId.isNotEmpty()) {
                state.transferQueue.map {
                    if (it.id == activeId || it.fileName == fileName) it.copy(
                        status = QueueItemStatus.TRANSFERRING,
                        progressPercent = pct,
                        transferredBytes = transferred,
                        speedMbps = mbPerSec,
                        etaSeconds = etaSec
                    ) else it
                }
            } else state.transferQueue

            val nextSend = if (isDone && !isRecv) null else (if (!isRecv && state.activeSendTransfer != null) state.activeSendTransfer.copy(
                transferredBytes = transferred,
                totalBytes = total,
                speedMbps = mbPerSec,
                etaSeconds = etaSec
            ) else state.activeSendTransfer)

            val nextRecv = if (isDone && isRecv) null else (if (isRecv && state.activeReceiveTransfer != null) state.activeReceiveTransfer.copy(
                transferredBytes = transferred,
                totalBytes = total,
                speedMbps = mbPerSec,
                etaSeconds = etaSec
            ) else state.activeReceiveTransfer)

            val isStillTransferring = nextSend != null || nextRecv != null
            val nextState = if (isStillTransferring) TransferUiState.TRANSFERRING else TransferUiState.IDLE

            state.copy(
                activeSendTransfer = nextSend,
                activeReceiveTransfer = nextRecv,
                activeTransfer = nextSend ?: nextRecv,
                transferQueue = updatedQueue,
                history = updatedHistory,
                isTransferring = isStillTransferring,
                transferUiState = nextState,
                lastCompletedFileName = if (isDone) fileName else state.lastCompletedFileName,
                lastCompletedFileSize = if (isDone) total else state.lastCompletedFileSize,
                lastCompletedFilePath = if (isDone) activePath else state.lastCompletedFilePath,
                transferRateText = if (!isStillTransferring) "0.0 MB/s" else rateText,
                statusMessage = if (isDone) "Completed $fileName!" else "Streaming $fileName ($rateText, ETA: ${etaSec}s)"
            )
        }

        if (isDone) {
            viewModelScope.launch(Dispatchers.IO) {
                updateStorageMetrics()
                val matchingItem = dbHelper.getAllQueueItems().firstOrNull {
                    it.id == activeId || it.fileName == fileName
                }
                val duration = if (matchingItem != null) (System.currentTimeMillis() - matchingItem.timestamp).coerceAtLeast(1) else 1000L
                val avgSpeed = if (duration > 0) (total * 1000.0) / duration else 0.0
                val historyItem = TransferHistoryItem(
                    id = activeId.ifEmpty { UUID.randomUUID().toString() },
                    fileName = fileName,
                    fileSize = total,
                    filePath = File(prefs.downloadDirectory, fileName).absolutePath,
                    isReceived = _uiState.value.activeTransfer?.isReceived ?: false,
                    peerName = _uiState.value.selectedPeer?.deviceName ?: "Peer",
                    timestamp = System.currentTimeMillis(),
                    status = "COMPLETED",
                    durationMs = duration,
                    avgSpeedBps = avgSpeed
                )
                dbHelper.completeTransferTransaction(activeId, historyItem)
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
                val rawType = parts[2].trim()
                val devType = when (rawType.lowercase()) {
                    "android" -> "Android Device"
                    "windows" -> "Windows PC"
                    "linux" -> "Linux PC"
                    "macos" -> "Mac"
                    "ios" -> "iPhone"
                    else -> rawType.ifBlank { "Remote Device" }
                }
                val ip = parts[3].ifBlank { "Unknown IP" }
                val rawName = parts[1].trim()
                val name = if (rawName.isBlank() || rawName.equals("Unknown Device", ignoreCase = true)) {
                    "$devType ($ip)"
                } else {
                    rawName
                }
                DiscoveredPeer(
                    deviceId = parts[0],
                    deviceName = name,
                    deviceType = devType,
                    ipAddress = ip,
                    port = parts[4].toIntOrNull() ?: 48124,
                    isOnline = true
                )
            } else null
        }.filter {
            it.deviceId.isNotBlank() && it.deviceId != myId && it.ipAddress != "127.0.0.1" && it.ipAddress.isNotBlank()
        }.distinctBy { it.deviceId }

        _uiState.update { state ->
            val currentSelected = state.selectedPeer
            val updatedSelected = if (currentSelected != null) {
                liveList.firstOrNull { it.deviceId == currentSelected.deviceId } ?: currentSelected
            } else null
            state.copy(peers = liveList, selectedPeer = updatedSelected)
        }
    }

    override fun onCleared() {
        nativeBridge.nativeShutdown()
        AeroSyncTransferService.stopTransfer(getApplication())
        super.onCleared()
    }
}
