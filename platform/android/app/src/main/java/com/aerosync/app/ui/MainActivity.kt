package com.aerosync.app.ui

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.DocumentsContract
import android.provider.MediaStore
import android.provider.OpenableColumns
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.compose.BackHandler
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import android.widget.Toast
import androidx.core.view.WindowCompat
import com.aerosync.app.service.AeroSyncTransferService
import com.aerosync.app.ui.screens.DevicesScreen
import com.aerosync.app.ui.screens.HomeScreen
import com.aerosync.app.ui.screens.TransfersScreen
import com.aerosync.app.viewmodel.AeroSyncViewModel
import java.io.File
import java.io.FileOutputStream

class MainActivity : ComponentActivity() {

    private val viewModel: AeroSyncViewModel by viewModels()
    private var multicastLock: android.net.wifi.WifiManager.MulticastLock? = null
    private var wifiLock: android.net.wifi.WifiManager.WifiLock? = null

    private fun acquireWifiDiscoveryLocks() {
        try {
            val wifiManager = applicationContext.getSystemService(Context.WIFI_SERVICE) as? android.net.wifi.WifiManager
            if (wifiManager != null) {
                if (multicastLock == null) {
                    multicastLock = wifiManager.createMulticastLock("AeroSync:MulticastLock").apply {
                        setReferenceCounted(true)
                        acquire()
                    }
                }
                if (wifiLock == null) {
                    @Suppress("DEPRECATION")
                    wifiLock = wifiManager.createWifiLock(
                        android.net.wifi.WifiManager.WIFI_MODE_FULL_HIGH_PERF,
                        "AeroSync:WifiDiscoveryLock"
                    ).apply {
                        setReferenceCounted(false)
                        acquire()
                    }
                }
            }
        } catch (_: Exception) {}
    }

    private fun releaseWifiDiscoveryLocks() {
        try {
            if (multicastLock?.isHeld == true) multicastLock?.release()
            multicastLock = null
            if (wifiLock?.isHeld == true) wifiLock?.release()
            wifiLock = null
        } catch (_: Exception) {}
    }

    private val folderPickerLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri ->
        if (uri != null) {
            try {
                contentResolver.takePersistableUriPermission(
                    uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                )
            } catch (_: Exception) {}

            val path = uri.path ?: ""
            val resolved = if (path.contains("primary:")) {
                Environment.getExternalStorageDirectory().absolutePath + "/" + path.substringAfter("primary:")
            } else {
                val docId = DocumentsContract.getTreeDocumentId(uri)
                if (docId.contains("primary:")) {
                    Environment.getExternalStorageDirectory().absolutePath + "/" + docId.substringAfter("primary:")
                } else {
                    Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).absolutePath + "/AeroSync"
                }
            }
            val targetFolder = File(resolved)
            if (!targetFolder.exists()) targetFolder.mkdirs()
            viewModel.setDownloadDirectory(targetFolder.absolutePath)
        }
    }

    private val filePickerLauncher = registerForActivityResult(
        ActivityResultContracts.GetMultipleContents()
    ) { uris ->
        if (uris.isNotEmpty()) {
            val selectedItems = uris.mapNotNull { uri ->
                var fileName = "file_${System.currentTimeMillis()}"
                var fileSize = 0L
                val mimeType = contentResolver.getType(uri) ?: ""
                try {
                    contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                        val nameIdx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                        val sizeIdx = cursor.getColumnIndex(OpenableColumns.SIZE)
                        if (cursor.moveToFirst()) {
                            if (nameIdx >= 0) fileName = cursor.getString(nameIdx) ?: fileName
                            if (sizeIdx >= 0) fileSize = cursor.getLong(sizeIdx)
                        }
                    }
                } catch (_: Exception) {}

                val formatted = when {
                    fileSize >= 1024 * 1024 * 1024 -> String.format(java.util.Locale.US, "%.2f GB", fileSize.toDouble() / (1024 * 1024 * 1024))
                    fileSize >= 1024 * 1024 -> String.format(java.util.Locale.US, "%.1f MB", fileSize.toDouble() / (1024 * 1024))
                    fileSize >= 1024 -> "${fileSize / 1024} KB"
                    fileSize > 0 -> "$fileSize B"
                    else -> "Ready"
                }

                com.aerosync.app.viewmodel.SelectedFileItem(
                    uriString = uri.toString(),
                    fileName = fileName,
                    fileSize = fileSize,
                    mimeType = mimeType,
                    formattedSize = formatted
                )
            }
            if (selectedItems.isNotEmpty()) {
                viewModel.addSelectedFiles(selectedItems)
            }
        }
    }

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { _ ->
        // Permissions granted
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, false)

        // Asynchronously initialize background locks and native engine without blocking UI mount
        lifecycleScope.launch(Dispatchers.IO) {
            acquireWifiDiscoveryLocks()
            viewModel.initializeNativeEngine()
        }

        requestPermissions()

        setContent {
            val uiState by viewModel.uiState.collectAsState()
            var lastBackPressTime by remember { mutableStateOf(0L) }

            com.aerosync.app.ui.theme.AeroSyncTheme(themeMode = uiState.themeMode) {
                BackHandler {
                    if (uiState.selectedTab != 0) {
                        viewModel.setSelectedTab(0)
                    } else {
                        val currentTime = System.currentTimeMillis()
                        if (currentTime - lastBackPressTime < 2000) {
                            finish()
                        } else {
                            lastBackPressTime = currentTime
                            Toast.makeText(this@MainActivity, "Press back again to exit AeroSync", Toast.LENGTH_SHORT).show()
                        }
                    }
                }

                when (uiState.selectedTab) {
                    1 -> DevicesScreen(
                        uiState = uiState,
                        onSelectPeer = { viewModel.selectPeer(it) },
                        onConnectDirectIp = { viewModel.connectToDirectIp(it) },
                        onUpdateDeviceName = { viewModel.updateDeviceName(it) },
                        onRefreshPeers = { viewModel.initializeNativeEngine() },
                        onSelectTab = { viewModel.setSelectedTab(it) },
                        onToggleTheme = { viewModel.toggleTheme() }
                    )
                    2 -> TransfersScreen(
                        uiState = uiState,
                        onTogglePause = { viewModel.togglePauseTransfer() },
                        onCancelTransfer = { viewModel.cancelActiveTransfer() },
                        onSelectTab = { viewModel.setSelectedTab(it) },
                        onRemoveQueueItem = { viewModel.removeQueueItem(it) },
                        onClearQueue = { viewModel.clearCompletedQueue() },
                        onClearHistory = { viewModel.clearHistory() },
                        onChangeDownloadLocation = { folderPickerLauncher.launch(null) },
                        onToggleTheme = { viewModel.toggleTheme() }
                    )
                    else -> HomeScreen(
                        uiState = uiState,
                        onSelectPeer = { viewModel.selectPeer(it) },
                        onConnectDirectIp = { viewModel.connectToDirectIp(it) },
                        onPickFiles = { filePickerLauncher.launch("*/*") },
                        onRemoveSelectedFile = { viewModel.removeSelectedFile(it) },
                        onClearSelectedFiles = { viewModel.clearSelectedFiles() },
                        onSendSelectedFiles = { viewModel.startTransferOfSelectedFiles() },
                        onRespondPairing = { viewModel.respondToPairingRequest(it) },
                        onToggleTheme = { viewModel.toggleTheme() },
                        onSelectTab = { viewModel.setSelectedTab(it) },
                        onTogglePause = { viewModel.togglePauseTransfer() },
                        onCancelTransfer = { viewModel.cancelActiveTransfer() },
                        onChangeDownloadLocation = { folderPickerLauncher.launch(null) },
                        onResetTransfer = { viewModel.resetTransferState() },
                        onRetryTransfer = { viewModel.retryTransfer() }
                    )
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        acquireWifiDiscoveryLocks()
        // Always refresh storage metrics and network info dynamically when returning to app
        viewModel.updateStorageMetrics()
        viewModel.updateNetworkDiagnostics()
    }

    override fun onDestroy() {
        releaseWifiDiscoveryLocks()
        super.onDestroy()
    }

    private fun requestPermissions() {
        val permissions = mutableListOf(
            android.Manifest.permission.ACCESS_FINE_LOCATION,
            android.Manifest.permission.ACCESS_COARSE_LOCATION
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissions.add("android.permission.NEARBY_WIFI_DEVICES")
            permissions.add(android.Manifest.permission.POST_NOTIFICATIONS)
            permissions.add(android.Manifest.permission.READ_MEDIA_IMAGES)
            permissions.add(android.Manifest.permission.READ_MEDIA_VIDEO)
            permissions.add(android.Manifest.permission.READ_MEDIA_AUDIO)
        }
        permissionLauncher.launch(permissions.toTypedArray())
    }

    companion object {
        data class UriMetadata(val fileName: String, val fileSize: Long)

        fun getUriMetadata(context: Context, uri: Uri): UriMetadata {
            var fileName = "file_${System.currentTimeMillis()}"
            var fileSize = 0L
            if (uri.scheme == "file") {
                val f = File(uri.path ?: "")
                if (f.exists()) {
                    return UriMetadata(f.name, f.length())
                }
            }
            try {
                context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                    val nameIdx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                    val sizeIdx = cursor.getColumnIndex(OpenableColumns.SIZE)
                    if (cursor.moveToFirst()) {
                        if (nameIdx >= 0) {
                            val n = cursor.getString(nameIdx)
                            if (!n.isNullOrBlank()) fileName = n
                        }
                        if (sizeIdx >= 0) {
                            fileSize = cursor.getLong(sizeIdx)
                        }
                    }
                }
            } catch (_: Exception) {}
            return UriMetadata(fileName, fileSize)
        }

        fun resolveUriToFilePath(context: Context, uri: Uri): String? {
            if (uri.scheme == "file") {
                val p = uri.path
                if (!p.isNullOrBlank() && File(p).exists()) return p
                return null
            }

            // Direct SAF / Storage Document resolution without copying files
            try {
                if (DocumentsContract.isDocumentUri(context, uri)) {
                    val docId = DocumentsContract.getDocumentId(uri)
                    val authority = uri.authority
                    if ("com.android.externalstorage.documents" == authority) {
                        val split = docId.split(":")
                        val type = split[0]
                        if ("primary".equals(type, ignoreCase = true)) {
                            val path = Environment.getExternalStorageDirectory().absolutePath + "/" + split.getOrNull(1).orEmpty()
                            if (File(path).exists()) return path
                        }
                    } else if ("com.android.providers.downloads.documents" == authority) {
                        if (docId.startsWith("raw:")) {
                            val path = docId.removePrefix("raw:")
                            if (File(path).exists()) return path
                        }
                    } else if ("com.android.providers.media.documents" == authority) {
                        val split = docId.split(":")
                        val id = split.getOrNull(1) ?: docId
                        val contentUri = MediaStore.Files.getContentUri("external")
                        context.contentResolver.query(
                            contentUri,
                            arrayOf(MediaStore.MediaColumns.DATA),
                            "${MediaStore.MediaColumns._ID}=?",
                            arrayOf(id),
                            null
                        )?.use { cursor ->
                            if (cursor.moveToFirst()) {
                                val colIdx = cursor.getColumnIndex(MediaStore.MediaColumns.DATA)
                                if (colIdx >= 0) {
                                    val directPath = cursor.getString(colIdx)
                                    if (!directPath.isNullOrBlank() && File(directPath).exists()) {
                                        return directPath
                                    }
                                }
                            }
                        }
                    }
                }

                // Direct query on general content URI
                context.contentResolver.query(uri, arrayOf(MediaStore.MediaColumns.DATA), null, null, null)?.use { cursor ->
                    if (cursor.moveToFirst()) {
                        val colIdx = cursor.getColumnIndex(MediaStore.MediaColumns.DATA)
                        if (colIdx >= 0) {
                            val directPath = cursor.getString(colIdx)
                            if (!directPath.isNullOrBlank() && File(directPath).exists()) {
                                return directPath
                            }
                        }
                    }
                }
            } catch (_: Exception) {}

            return null
        }
    }
}
