package com.aerosync.app.ui.screens

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.aerosync.app.ui.components.AeroSyncLogoIcon
import com.aerosync.app.viewmodel.AeroSyncUiState
import com.aerosync.app.viewmodel.DiscoveredPeer
import com.aerosync.app.viewmodel.QueueItemStatus
import java.text.DecimalFormat

@Composable
fun HomeScreen(
    uiState: AeroSyncUiState,
    onSelectPeer: (DiscoveredPeer) -> Unit,
    onConnectDirectIp: (String) -> Unit,
    onPickFiles: () -> Unit,
    onRespondPairing: (Boolean) -> Unit,
    onToggleTheme: () -> Unit,
    onSelectTab: (Int) -> Unit,
    onTogglePause: () -> Unit,
    onCancelTransfer: () -> Unit,
    onRemoveQueueItem: (String) -> Unit = {},
    onClearHistory: () -> Unit = {},
    onChangeDownloadLocation: () -> Unit
) {
    var isSettingsExpanded by remember { mutableStateOf(false) }
    var isPeersExpanded by remember { mutableStateOf(false) }
    var isHistoryExpanded by remember { mutableStateOf(false) }
    var directIpInput by remember { mutableStateOf("") }

    val isDark = uiState.isDarkTheme
    val bgColor = if (isDark) Color(0xFF090D16) else Color(0xFFF8FAFC)
    val cardBg = if (isDark) Color(0xFF111827) else Color(0xFFFFFFFF)
    val cardBgAlt = if (isDark) Color(0xFF1F2937) else Color(0xFFF1F5F9)
    val borderColor = if (isDark) Color(0xFF374151) else Color(0xFFE2E8F0)
    val textPrimary = if (isDark) Color(0xFFF9FAFB) else Color(0xFF0F172A)
    val textSecondary = if (isDark) Color(0xFF9CA3AF) else Color(0xFF64748B)
    val textMuted = if (isDark) Color(0xFF6B7280) else Color(0xFF94A3B8)
    val brandBlue = Color(0xFF2563EB)
    val brandPurple = Color(0xFF7C3AED)
    val brandGreen = Color(0xFF059669)

    val df = DecimalFormat("#,##0.#")

    fun formatBytes(bytes: Long): String {
        if (bytes <= 0) return "0 B"
        val k = 1024.0
        val sizes = arrayOf("B", "KB", "MB", "GB", "TB")
        val i = (Math.log(bytes.toDouble()) / Math.log(k)).toInt().coerceIn(0, sizes.size - 1)
        return "${df.format(bytes / Math.pow(k, i.toDouble()))} ${sizes[i]}"
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(bgColor)
            .statusBarsPadding()
            .navigationBarsPadding()
    ) {
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 16.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            // 1. Top Header Bar
            item {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    // Logo & App Name
                    Row(
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        AeroSyncLogoIcon(size = 32.dp)
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = "AeroSync",
                            fontSize = 19.sp,
                            fontWeight = FontWeight.ExtraBold,
                            color = textPrimary,
                            letterSpacing = (-0.3).sp
                        )
                    }

                    // Pill Navigation
                    Row(
                        modifier = Modifier
                            .clip(RoundedCornerShape(24.dp))
                            .background(cardBg)
                            .border(1.dp, borderColor, RoundedCornerShape(24.dp))
                            .padding(3.dp),
                        horizontalArrangement = Arrangement.spacedBy(2.dp)
                    ) {
                        listOf("Files", "Devices", "Activity").forEachIndexed { index, tab ->
                            val isSelected = index == 0
                            Surface(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(20.dp))
                                    .clickable { onSelectTab(index) },
                                color = if (isSelected) brandBlue else Color.Transparent
                            ) {
                                Text(
                                    text = tab,
                                    fontSize = 12.sp,
                                    fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                                    color = if (isSelected) Color.White else textSecondary,
                                    modifier = Modifier.padding(horizontal = 12.dp, vertical = 6.dp)
                                )
                            }
                        }
                    }

                    // Theme Toggle
                    IconButton(
                        onClick = onToggleTheme,
                        modifier = Modifier
                            .size(36.dp)
                            .clip(CircleShape)
                            .background(cardBg)
                            .border(1.dp, borderColor, CircleShape)
                    ) {
                        Icon(
                            imageVector = if (isDark) Icons.Default.DarkMode else Icons.Default.LightMode,
                            contentDescription = "Toggle Theme",
                            tint = if (isDark) Color(0xFFFBBF24) else Color(0xFF0284C7),
                            modifier = Modifier.size(17.dp)
                        )
                    }
                }
            }

            // 2. Main Hero Circular Drop Area Card
            item {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(22.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor),
                    shadowElevation = 2.dp
                ) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(24.dp),
                        horizontalAlignment = Alignment.CenterHorizontally
                    ) {
                        // Circular Drop / Upload Zone
                        Box(
                            modifier = Modifier
                                .size(190.dp)
                                .clip(CircleShape)
                                .clickable { onPickFiles() },
                            contentAlignment = Alignment.Center
                        ) {
                            Canvas(modifier = Modifier.fillMaxSize()) {
                                drawCircle(
                                    color = brandBlue.copy(alpha = 0.4f),
                                    style = Stroke(
                                        width = 2.dp.toPx(),
                                        pathEffect = PathEffect.dashPathEffect(floatArrayOf(12f, 8f), 0f)
                                    )
                                )
                            }

                            Column(
                                horizontalAlignment = Alignment.CenterHorizontally,
                                verticalArrangement = Arrangement.Center
                            ) {
                                Surface(
                                    shape = CircleShape,
                                    color = Color(0xFFEFF6FF),
                                    modifier = Modifier.size(56.dp)
                                ) {
                                    Box(contentAlignment = Alignment.Center) {
                                        Icon(
                                            imageVector = Icons.Default.CloudUpload,
                                            contentDescription = "Upload",
                                            tint = brandBlue,
                                            modifier = Modifier.size(32.dp)
                                        )
                                    }
                                }
                                Spacer(modifier = Modifier.height(10.dp))
                                Text(
                                    text = "Drop files here",
                                    fontSize = 16.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = textPrimary
                                )
                                Spacer(modifier = Modifier.height(2.dp))
                                Text(
                                    text = "or tap to pick files",
                                    fontSize = 12.sp,
                                    color = textSecondary
                                )
                            }
                        }

                        Spacer(modifier = Modifier.height(20.dp))

                        // Feature Badges Row
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.Center
                        ) {
                            Surface(
                                shape = RoundedCornerShape(16.dp),
                                color = Color(0xFFEFF6FF),
                                modifier = Modifier.padding(horizontal = 4.dp)
                            ) {
                                Row(
                                    modifier = Modifier.padding(horizontal = 10.dp, vertical = 5.dp),
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Icon(Icons.Default.Bolt, contentDescription = null, tint = brandBlue, modifier = Modifier.size(13.dp))
                                    Spacer(modifier = Modifier.width(4.dp))
                                    Text("High-speed", fontSize = 11.sp, fontWeight = FontWeight.SemiBold, color = brandBlue)
                                }
                            }

                            Surface(
                                shape = RoundedCornerShape(16.dp),
                                color = Color(0xFFECFDF5),
                                modifier = Modifier.padding(horizontal = 4.dp)
                            ) {
                                Row(
                                    modifier = Modifier.padding(horizontal = 10.dp, vertical = 5.dp),
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Icon(Icons.Default.Storage, contentDescription = null, tint = brandGreen, modifier = Modifier.size(13.dp))
                                    Spacer(modifier = Modifier.width(4.dp))
                                    Text("Multi-GB", fontSize = 11.sp, fontWeight = FontWeight.SemiBold, color = brandGreen)
                                }
                            }

                            Surface(
                                shape = RoundedCornerShape(16.dp),
                                color = Color(0xFFF5F3FF),
                                modifier = Modifier.padding(horizontal = 4.dp)
                            ) {
                                Row(
                                    modifier = Modifier.padding(horizontal = 10.dp, vertical = 5.dp),
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Icon(Icons.Default.Wifi, contentDescription = null, tint = brandPurple, modifier = Modifier.size(13.dp))
                                    Spacer(modifier = Modifier.width(4.dp))
                                    Text("Direct Wi-Fi", fontSize = 11.sp, fontWeight = FontWeight.SemiBold, color = brandPurple)
                                }
                            }
                        }

                        Spacer(modifier = Modifier.height(20.dp))

                        // Browse Files Button
                        Button(
                            onClick = onPickFiles,
                            modifier = Modifier
                                .fillMaxWidth(0.75f)
                                .height(46.dp),
                            shape = RoundedCornerShape(23.dp),
                            colors = ButtonDefaults.buttonColors(containerColor = brandBlue, contentColor = Color.White),
                            contentPadding = PaddingValues(horizontal = 20.dp, vertical = 8.dp)
                        ) {
                            Icon(Icons.Default.FolderOpen, contentDescription = null, modifier = Modifier.size(18.dp))
                            Spacer(modifier = Modifier.width(8.dp))
                            Text("Browse Files", fontSize = 14.sp, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }

            // 3. Active Transfer Card (If Active)
            if (uiState.isTransferring || uiState.transferQueue.isNotEmpty()) {
                val activeItem = uiState.transferQueue.firstOrNull { it.status == QueueItemStatus.TRANSFERRING } ?: uiState.transferQueue.firstOrNull()
                val activeName = uiState.activeTransfer?.fileName ?: activeItem?.fileName ?: "Transferring files..."
                val progressPct = if (uiState.activeTransfer != null && uiState.activeTransfer.totalBytes > 0) {
                    ((uiState.activeTransfer.transferredBytes.toDouble() / uiState.activeTransfer.totalBytes.toDouble()) * 100).toInt().coerceIn(0, 100)
                } else {
                    activeItem?.progressPercent ?: 0
                }

                item {
                    Surface(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(16.dp)),
                        color = cardBg,
                        border = androidx.compose.foundation.BorderStroke(1.dp, brandBlue.copy(alpha = 0.5f))
                    ) {
                        Column(modifier = Modifier.padding(16.dp)) {
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    modifier = Modifier.weight(1f)
                                ) {
                                    Box(
                                        modifier = Modifier
                                            .size(8.dp)
                                            .clip(CircleShape)
                                            .background(Color(0xFF10B981))
                                    )
                                    Spacer(modifier = Modifier.width(8.dp))
                                    Text(
                                        text = activeName,
                                        fontSize = 13.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = textPrimary,
                                        maxLines = 1,
                                        overflow = TextOverflow.Ellipsis
                                    )
                                }

                                Surface(
                                    shape = RoundedCornerShape(12.dp),
                                    color = Color(0xFFEFF6FF)
                                ) {
                                    Text(
                                        text = uiState.transferRateText,
                                        fontSize = 12.sp,
                                        fontWeight = FontWeight.ExtraBold,
                                        color = brandBlue,
                                        modifier = Modifier.padding(horizontal = 8.dp, vertical = 3.dp)
                                    )
                                }
                            }

                            Spacer(modifier = Modifier.height(10.dp))

                            LinearProgressIndicator(
                                progress = (progressPct / 100f).coerceIn(0f, 1f),
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .height(6.dp)
                                    .clip(RoundedCornerShape(3.dp)),
                                color = brandBlue,
                                trackColor = borderColor
                            )

                            Spacer(modifier = Modifier.height(8.dp))

                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text(
                                    text = "$progressPct% completed",
                                    fontSize = 11.sp,
                                    color = textSecondary
                                )

                                TextButton(
                                    onClick = onCancelTransfer,
                                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp)
                                ) {
                                    Text("Cancel", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = Color(0xFFEF4444))
                                }
                            }
                        }
                    }
                }
            }

            // 4. Collapsible Accordion 1: Settings & Status
            item {
                val rotationAngle by animateFloatAsState(targetValue = if (isSettingsExpanded) 90f else 0f, label = "rotateSettings")
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                ) {
                    Column {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { isSettingsExpanded = !isSettingsExpanded }
                                .padding(16.dp),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Surface(
                                    shape = CircleShape,
                                    color = Color(0xFFEFF6FF),
                                    modifier = Modifier.size(40.dp)
                                ) {
                                    Box(contentAlignment = Alignment.Center) {
                                        Icon(Icons.Default.Settings, contentDescription = null, tint = brandBlue, modifier = Modifier.size(20.dp))
                                    }
                                }
                                Spacer(modifier = Modifier.width(12.dp))
                                Column {
                                    Text("Settings & Status", fontSize = 14.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                    Text("Download location, network & storage", fontSize = 11.sp, color = textSecondary)
                                }
                            }

                            Icon(
                                Icons.Default.ChevronRight,
                                contentDescription = null,
                                tint = textMuted,
                                modifier = Modifier
                                    .size(18.dp)
                                    .rotate(rotationAngle)
                            )
                        }

                        AnimatedVisibility(visible = isSettingsExpanded) {
                            Column(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(horizontal = 16.dp, vertical = 8.dp),
                                verticalArrangement = Arrangement.spacedBy(10.dp)
                            ) {
                                // Download Location
                                Surface(
                                    modifier = Modifier.fillMaxWidth(),
                                    shape = RoundedCornerShape(10.dp),
                                    color = cardBgAlt
                                ) {
                                    Row(
                                        modifier = Modifier.padding(12.dp),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Column(modifier = Modifier.weight(1f)) {
                                            Text("DOWNLOAD LOCATION", fontSize = 10.sp, fontWeight = FontWeight.Bold, color = textMuted)
                                            Spacer(modifier = Modifier.height(2.dp))
                                            Text(
                                                text = if (uiState.downloadDirectory.isNotBlank()) uiState.downloadDirectory else "/storage/emulated/0/Download/AeroSync",
                                                fontSize = 11.sp,
                                                fontWeight = FontWeight.Medium,
                                                color = textPrimary,
                                                maxLines = 1,
                                                overflow = TextOverflow.Ellipsis
                                            )
                                        }
                                        TextButton(onClick = onChangeDownloadLocation) {
                                            Text("Change", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = brandBlue)
                                        }
                                    }
                                }

                                // Network Status
                                Surface(
                                    modifier = Modifier.fillMaxWidth(),
                                    shape = RoundedCornerShape(10.dp),
                                    color = cardBgAlt
                                ) {
                                    Row(
                                        modifier = Modifier.padding(12.dp),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Column {
                                            Text("NETWORK STATUS", fontSize = 10.sp, fontWeight = FontWeight.Bold, color = textMuted)
                                            Spacer(modifier = Modifier.height(2.dp))
                                            Row(verticalAlignment = Alignment.CenterVertically) {
                                                Box(modifier = Modifier.size(7.dp).clip(CircleShape).background(Color(0xFF10B981)))
                                                Spacer(modifier = Modifier.width(6.dp))
                                                Text(uiState.connectionTypeLabel, fontSize = 12.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                            }
                                        }
                                        Text("Port 48124", fontSize = 11.sp, color = textMuted)
                                    }
                                }

                                // Device Storage
                                Surface(
                                    modifier = Modifier.fillMaxWidth(),
                                    shape = RoundedCornerShape(10.dp),
                                    color = cardBgAlt
                                ) {
                                    Column(modifier = Modifier.padding(12.dp)) {
                                        Row(
                                            modifier = Modifier.fillMaxWidth(),
                                            horizontalArrangement = Arrangement.SpaceBetween
                                        ) {
                                            Text("DEVICE STORAGE", fontSize = 10.sp, fontWeight = FontWeight.Bold, color = textMuted)
                                            Text("${uiState.freeSpaceText} Free", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                        }
                                        Spacer(modifier = Modifier.height(6.dp))
                                        LinearProgressIndicator(
                                            progress = (uiState.storageUsedPercent / 100f).coerceIn(0f, 1f),
                                            modifier = Modifier.fillMaxWidth().height(4.dp).clip(RoundedCornerShape(2.dp)),
                                            color = brandBlue,
                                            trackColor = borderColor
                                        )
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 5. Collapsible Accordion 2: Nearby Peers
            item {
                val rotationAngle by animateFloatAsState(targetValue = if (isPeersExpanded) 90f else 0f, label = "rotatePeers")
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                ) {
                    Column {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { isPeersExpanded = !isPeersExpanded }
                                .padding(16.dp),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Surface(
                                    shape = CircleShape,
                                    color = Color(0xFFECFDF5),
                                    modifier = Modifier.size(40.dp)
                                ) {
                                    Box(contentAlignment = Alignment.Center) {
                                        Icon(Icons.Default.Devices, contentDescription = null, tint = brandGreen, modifier = Modifier.size(20.dp))
                                    }
                                }
                                Spacer(modifier = Modifier.width(12.dp))
                                Column {
                                    Text("Nearby Peers", fontSize = 14.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                    Text(
                                        text = if (uiState.peers.isNotEmpty()) "${uiState.peers.size} active device(s) online" else "Connected devices & direct IP",
                                        fontSize = 11.sp,
                                        color = textSecondary
                                    )
                                }
                            }

                            Icon(
                                Icons.Default.ChevronRight,
                                contentDescription = null,
                                tint = textMuted,
                                modifier = Modifier
                                    .size(18.dp)
                                    .rotate(rotationAngle)
                            )
                        }

                        AnimatedVisibility(visible = isPeersExpanded) {
                            Column(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(horizontal = 16.dp, vertical = 8.dp),
                                verticalArrangement = Arrangement.spacedBy(8.dp)
                            ) {
                                if (uiState.peers.isEmpty()) {
                                    Column(
                                        modifier = Modifier.fillMaxWidth().padding(vertical = 12.dp),
                                        horizontalAlignment = Alignment.CenterHorizontally
                                    ) {
                                        Text("No nearby devices found", fontSize = 13.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                        Spacer(modifier = Modifier.height(2.dp))
                                        Text("Make sure AeroSync is open on your other device.", fontSize = 11.sp, color = textSecondary, textAlign = TextAlign.Center)
                                    }
                                } else {
                                    for (peer in uiState.peers) {
                                        Surface(
                                            modifier = Modifier.fillMaxWidth(),
                                            shape = RoundedCornerShape(10.dp),
                                            color = cardBgAlt
                                        ) {
                                            Row(
                                                modifier = Modifier.padding(10.dp),
                                                horizontalArrangement = Arrangement.SpaceBetween,
                                                verticalAlignment = Alignment.CenterVertically
                                            ) {
                                                Row(verticalAlignment = Alignment.CenterVertically) {
                                                    Box(modifier = Modifier.size(7.dp).clip(CircleShape).background(Color(0xFF10B981)))
                                                    Spacer(modifier = Modifier.width(8.dp))
                                                    Column {
                                                        Text(peer.deviceName, fontSize = 12.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                                        Text("${peer.ipAddress}:${peer.port}", fontSize = 10.sp, color = textMuted)
                                                    }
                                                }

                                                Button(
                                                    onClick = { onSelectPeer(peer) },
                                                    shape = RoundedCornerShape(14.dp),
                                                    colors = ButtonDefaults.buttonColors(containerColor = brandBlue),
                                                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp)
                                                ) {
                                                    Text("Send", fontSize = 11.sp, fontWeight = FontWeight.Bold)
                                                }
                                            }
                                        }
                                    }
                                }

                                // Quick Direct IP
                                Row(
                                    modifier = Modifier.fillMaxWidth(),
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    OutlinedTextField(
                                        value = directIpInput,
                                        onValueChange = { directIpInput = it },
                                        placeholder = { Text("Direct IP (192.168.x.x)", fontSize = 11.sp) },
                                        modifier = Modifier.weight(1f).height(46.dp),
                                        shape = RoundedCornerShape(10.dp),
                                        singleLine = true
                                    )
                                    Spacer(modifier = Modifier.width(6.dp))
                                    Button(
                                        onClick = {
                                            if (directIpInput.isNotBlank()) {
                                                onConnectDirectIp(directIpInput.trim())
                                                directIpInput = ""
                                            }
                                        },
                                        modifier = Modifier.height(46.dp),
                                        shape = RoundedCornerShape(10.dp),
                                        colors = ButtonDefaults.buttonColors(containerColor = cardBgAlt, contentColor = textPrimary)
                                    ) {
                                        Text("Connect", fontSize = 11.sp, fontWeight = FontWeight.Bold)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 6. Collapsible Accordion 3: Recent Transfers
            item {
                val rotationAngle by animateFloatAsState(targetValue = if (isHistoryExpanded) 90f else 0f, label = "rotateHistory")
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                ) {
                    Column {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { isHistoryExpanded = !isHistoryExpanded }
                                .padding(16.dp),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Surface(
                                    shape = CircleShape,
                                    color = Color(0xFFF5F3FF),
                                    modifier = Modifier.size(40.dp)
                                ) {
                                    Box(contentAlignment = Alignment.Center) {
                                        Icon(Icons.Default.History, contentDescription = null, tint = brandPurple, modifier = Modifier.size(20.dp))
                                    }
                                }
                                Spacer(modifier = Modifier.width(12.dp))
                                Column {
                                    Text("Recent Transfers", fontSize = 14.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                    Text("See your recent and active transfers", fontSize = 11.sp, color = textSecondary)
                                }
                            }

                            Icon(
                                Icons.Default.ChevronRight,
                                contentDescription = null,
                                tint = textMuted,
                                modifier = Modifier
                                    .size(18.dp)
                                    .rotate(rotationAngle)
                            )
                        }

                        AnimatedVisibility(visible = isHistoryExpanded) {
                            Column(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(horizontal = 16.dp, vertical = 8.dp),
                                verticalArrangement = Arrangement.spacedBy(8.dp)
                            ) {
                                if (uiState.history.isEmpty()) {
                                    Column(
                                        modifier = Modifier.fillMaxWidth().padding(vertical = 12.dp),
                                        horizontalAlignment = Alignment.CenterHorizontally
                                    ) {
                                        Text("No recent transfers", fontSize = 13.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                        Spacer(modifier = Modifier.height(2.dp))
                                        Text("Files you send or receive will appear here.", fontSize = 11.sp, color = textSecondary)
                                    }
                                } else {
                                    for (history in uiState.history.take(5)) {
                                        Surface(
                                            modifier = Modifier.fillMaxWidth(),
                                            shape = RoundedCornerShape(10.dp),
                                            color = cardBgAlt
                                        ) {
                                            Row(
                                                modifier = Modifier.padding(10.dp),
                                                horizontalArrangement = Arrangement.SpaceBetween,
                                                verticalAlignment = Alignment.CenterVertically
                                            ) {
                                                Row(
                                                    verticalAlignment = Alignment.CenterVertically,
                                                    modifier = Modifier.weight(1f)
                                                ) {
                                                    Icon(
                                                        imageVector = if (history.isReceived) Icons.Default.ArrowDownward else Icons.Default.ArrowUpward,
                                                        contentDescription = null,
                                                        tint = if (history.isReceived) brandGreen else brandBlue,
                                                        modifier = Modifier.size(16.dp)
                                                    )
                                                    Spacer(modifier = Modifier.width(8.dp))
                                                    Column {
                                                        Text(history.fileName, fontSize = 12.sp, fontWeight = FontWeight.Bold, color = textPrimary, maxLines = 1, overflow = TextOverflow.Ellipsis)
                                                        Text("${formatBytes(history.fileSize)} • ${history.peerName}", fontSize = 10.sp, color = textMuted)
                                                    }
                                                }

                                                Text("Completed", fontSize = 10.sp, fontWeight = FontWeight.Bold, color = brandGreen)
                                            }
                                        }
                                    }

                                    TextButton(
                                        onClick = { onSelectTab(2) },
                                        modifier = Modifier.align(Alignment.CenterHorizontally)
                                    ) {
                                        Text("View Full History", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = brandBlue)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 7. Bottom Security & Trust Card
            item {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, Color(0xFF10B981).copy(alpha = 0.25f))
                ) {
                    Row(
                        modifier = Modifier.padding(14.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Surface(
                                shape = RoundedCornerShape(8.dp),
                                color = Color(0xFFECFDF5),
                                modifier = Modifier.size(34.dp)
                            ) {
                                Box(contentAlignment = Alignment.Center) {
                                    Icon(Icons.Default.VerifiedUser, contentDescription = null, tint = brandGreen, modifier = Modifier.size(18.dp))
                                }
                            }
                            Spacer(modifier = Modifier.width(10.dp))
                            Column {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Text("Secure", fontSize = 12.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                    Text(" • ", fontSize = 12.sp, color = brandBlue)
                                    Text("Private", fontSize = 12.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                    Text(" • ", fontSize = 12.sp, color = brandPurple)
                                    Text("Local Transfer", fontSize = 12.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                }
                                Text("Your data never leaves your network.", fontSize = 10.sp, color = textSecondary)
                            }
                        }

                        Surface(
                            shape = CircleShape,
                            color = Color(0xFFEFF6FF),
                            modifier = Modifier.size(28.dp)
                        ) {
                            Box(contentAlignment = Alignment.Center) {
                                Icon(Icons.Default.Lock, contentDescription = null, tint = brandBlue, modifier = Modifier.size(14.dp))
                            }
                        }
                    }
                }
            }
        }
    }
}
