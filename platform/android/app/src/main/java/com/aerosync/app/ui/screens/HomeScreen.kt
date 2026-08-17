package com.aerosync.app.ui.screens

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
import androidx.compose.ui.geometry.Offset
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
    var showDirectIpDialog by remember { mutableStateOf(false) }
    var directIpInput by remember { mutableStateOf("192.168.43.1") }

    val isDark = uiState.isDarkTheme
    val bgColor = if (isDark) Color(0xFF090D16) else Color(0xFFF8FAFC)
    val cardBg = if (isDark) Color(0xFF111827) else Color(0xFFFFFFFF)
    val cardBgAlt = if (isDark) Color(0xFF1F2937) else Color(0xFFF1F5F9)
    val borderColor = if (isDark) Color(0xFF374151) else Color(0xFFE2E8F0)
    val borderDashedColor = if (isDark) Color(0xFF4B5563) else Color(0xFF94A3B8)
    val textPrimary = if (isDark) Color(0xFFF9FAFB) else Color(0xFF0F172A)
    val textSecondary = if (isDark) Color(0xFF9CA3AF) else Color(0xFF64748B)
    val textMuted = if (isDark) Color(0xFF6B7280) else Color(0xFF94A3B8)
    val brandBlue = if (isDark) Color(0xFF38BDF8) else Color(0xFF0284C7)
    val activeDotColor = Color(0xFF10B981)

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
            // Header: Official Logo, Navigation Tabs, Theme Toggle
            item {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        AeroSyncLogoIcon(size = 34.dp)
                        Spacer(modifier = Modifier.width(10.dp))
                        Text(
                            text = "AeroSync",
                            fontSize = 21.sp,
                            fontWeight = FontWeight.ExtraBold,
                            color = brandBlue,
                            letterSpacing = 0.5.sp
                        )
                    }

                    Row(
                        modifier = Modifier
                            .clip(RoundedCornerShape(20.dp))
                            .background(cardBgAlt)
                            .border(1.dp, borderColor, RoundedCornerShape(20.dp))
                            .padding(3.dp),
                        horizontalArrangement = Arrangement.spacedBy(2.dp)
                    ) {
                        listOf("Files", "Devices", "Activity").forEachIndexed { index, tab ->
                            val isSelected = index == 0
                            Surface(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(16.dp))
                                    .clickable { onSelectTab(index) },
                                color = if (isSelected) (if (isDark) Color(0xFF374151) else Color(0xFFE2E8F0)) else Color.Transparent
                            ) {
                                Text(
                                    text = tab,
                                    fontSize = 12.sp,
                                    fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                                    color = if (isSelected) textPrimary else textSecondary,
                                    modifier = Modifier.padding(horizontal = 10.dp, vertical = 5.dp)
                                )
                            }
                        }
                    }

                    IconButton(
                        onClick = onToggleTheme,
                        modifier = Modifier
                            .size(38.dp)
                            .clip(CircleShape)
                            .background(cardBgAlt)
                            .border(1.dp, borderColor, CircleShape)
                    ) {
                        Icon(
                            imageVector = if (isDark) Icons.Default.DarkMode else Icons.Default.LightMode,
                            contentDescription = "Toggle Theme",
                            tint = if (isDark) Color(0xFFFBBF24) else Color(0xFF0284C7),
                            modifier = Modifier.size(18.dp)
                        )
                    }
                }
            }

            // Upload Center Drop Zone Card
            item {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp))
                        .background(cardBg)
                        .border(1.dp, borderColor, RoundedCornerShape(16.dp))
                ) {
                    Canvas(modifier = Modifier.matchParentSize()) {
                        val stripeWidth = 24.dp.toPx()
                        val stripeSpacing = 24.dp.toPx()
                        val totalW = size.width
                        val totalH = size.height
                        val strokeColor = if (isDark) Color(0xFF374151).copy(alpha = 0.2f) else Color(0xFFE2E8F0).copy(alpha = 0.4f)
                        var x = -totalH
                        while (x < totalW + totalH) {
                            drawLine(color = strokeColor, start = Offset(x, 0f), end = Offset(x + totalH, totalH), strokeWidth = 6f)
                            x += stripeWidth + stripeSpacing
                        }
                    }

                    Box(modifier = Modifier.fillMaxWidth().padding(10.dp)) {
                        Canvas(modifier = Modifier.matchParentSize()) {
                            drawRoundRect(
                                color = borderDashedColor.copy(alpha = 0.5f),
                                size = size,
                                cornerRadius = androidx.compose.ui.geometry.CornerRadius(12.dp.toPx(), 12.dp.toPx()),
                                style = Stroke(width = 1.5.dp.toPx(), pathEffect = PathEffect.dashPathEffect(floatArrayOf(10f, 8f), 0f))
                            )
                        }

                        Column(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 16.dp, vertical = 20.dp),
                            horizontalAlignment = Alignment.CenterHorizontally
                        ) {
                            Surface(
                                shape = CircleShape,
                                color = if (isDark) Color(0xFF0C4A6E) else Color(0xFFE0F2FE),
                                modifier = Modifier.size(52.dp)
                            ) {
                                Box(contentAlignment = Alignment.Center) {
                                    Icon(
                                        imageVector = Icons.Default.CloudUpload,
                                        contentDescription = "Upload",
                                        tint = brandBlue,
                                        modifier = Modifier.size(28.dp)
                                    )
                                }
                            }
                            Spacer(modifier = Modifier.height(10.dp))
                            Text(
                                text = "Drop or Pick Files to Transfer",
                                fontSize = 16.sp,
                                fontWeight = FontWeight.Bold,
                                color = textPrimary
                            )
                            Spacer(modifier = Modifier.height(4.dp))
                            Text(
                                text = "High-speed streaming • Multi-gigabyte support • Direct Wi-Fi",
                                fontSize = 11.sp,
                                color = textMuted,
                                textAlign = TextAlign.Center
                            )
                            Spacer(modifier = Modifier.height(14.dp))
                            Button(
                                onClick = onPickFiles,
                                shape = RoundedCornerShape(20.dp),
                                colors = ButtonDefaults.buttonColors(containerColor = brandBlue, contentColor = Color.White),
                                contentPadding = PaddingValues(horizontal = 24.dp, vertical = 8.dp)
                            ) {
                                Icon(Icons.Default.Add, contentDescription = null, modifier = Modifier.size(16.dp))
                                Spacer(modifier = Modifier.width(6.dp))
                                Text("Browse Files", fontSize = 13.sp, fontWeight = FontWeight.Bold)
                            }
                        }
                    }
                }
            }

            // Download Location Card
            item {
                Surface(
                    modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                ) {
                    Column(modifier = Modifier.padding(14.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                text = "DOWNLOAD LOCATION",
                                fontSize = 11.sp,
                                fontWeight = FontWeight.Bold,
                                color = textSecondary,
                                letterSpacing = 0.8.sp
                            )
                            Text(
                                text = "Change Folder",
                                fontSize = 11.sp,
                                fontWeight = FontWeight.Bold,
                                color = brandBlue,
                                modifier = Modifier
                                    .clip(RoundedCornerShape(8.dp))
                                    .clickable { onChangeDownloadLocation() }
                                    .padding(4.dp)
                            )
                        }
                        Spacer(modifier = Modifier.height(8.dp))
                        Surface(
                            modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(10.dp)),
                            color = cardBgAlt,
                            border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                        ) {
                            Row(
                                modifier = Modifier.padding(horizontal = 10.dp, vertical = 8.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Icon(Icons.Default.Folder, contentDescription = null, tint = brandBlue, modifier = Modifier.size(18.dp))
                                Spacer(modifier = Modifier.width(8.dp))
                                Text(
                                    text = if (uiState.downloadDirectory.isNotBlank()) uiState.downloadDirectory else "Default: Downloads/AeroSync",
                                    fontSize = 11.sp,
                                    fontWeight = FontWeight.Medium,
                                    color = textPrimary,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis
                                )
                            }
                        }
                    }
                }
            }

            // Real-Time System Status & Live Diagnostics Card
            item {
                Surface(
                    modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                ) {
                    Column(modifier = Modifier.padding(14.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text("NETWORK & STORAGE STATUS", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = textSecondary, letterSpacing = 0.8.sp)
                            Surface(
                                shape = RoundedCornerShape(12.dp),
                                color = cardBgAlt,
                                border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                            ) {
                                Row(
                                    modifier = Modifier.padding(horizontal = 8.dp, vertical = 3.dp),
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Box(modifier = Modifier.size(6.dp).clip(CircleShape).background(activeDotColor))
                                    Spacer(modifier = Modifier.width(5.dp))
                                    Text(
                                        text = uiState.connectionTypeLabel,
                                        fontSize = 11.sp,
                                        fontWeight = FontWeight.SemiBold,
                                        color = textSecondary
                                    )
                                }
                            }
                        }
                        Spacer(modifier = Modifier.height(10.dp))
                        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                            Text("Storage Used", fontSize = 13.sp, fontWeight = FontWeight.SemiBold, color = textPrimary)
                            Text("${uiState.storageUsedPercent}%", fontSize = 13.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                        }
                        Spacer(modifier = Modifier.height(6.dp))
                        LinearProgressIndicator(
                            progress = (uiState.storageUsedPercent / 100f).coerceIn(0f, 1f),
                            modifier = Modifier.fillMaxWidth().height(6.dp).clip(RoundedCornerShape(3.dp)),
                            color = brandBlue,
                            trackColor = if (isDark) Color(0xFF374151) else Color(0xFFE2E8F0)
                        )
                        Spacer(modifier = Modifier.height(12.dp))
                        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                            Surface(
                                modifier = Modifier.weight(1f).clip(RoundedCornerShape(10.dp)),
                                color = cardBgAlt,
                                border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                            ) {
                                Column(modifier = Modifier.padding(10.dp)) {
                                    Text("Free Space", fontSize = 11.sp, color = textSecondary)
                                    Text(uiState.freeSpaceText, fontSize = 16.sp, fontWeight = FontWeight.ExtraBold, color = textPrimary)
                                }
                            }
                            Surface(
                                modifier = Modifier.weight(1f).clip(RoundedCornerShape(10.dp)),
                                color = cardBgAlt,
                                border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                            ) {
                                Column(modifier = Modifier.padding(10.dp)) {
                                    Text("Throughput", fontSize = 11.sp, color = textSecondary)
                                    Text(uiState.transferRateText, fontSize = 16.sp, fontWeight = FontWeight.ExtraBold, color = activeDotColor)
                                }
                            }
                        }
                    }
                }
            }

            // Connected Devices Quick List Card
            item {
                Surface(
                    modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                ) {
                    Column(modifier = Modifier.padding(14.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text("NEARBY PEERS", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = textSecondary, letterSpacing = 0.8.sp)
                            Surface(shape = RoundedCornerShape(10.dp), color = brandBlue) {
                                Text(
                                    text = "${uiState.peers.size} Online",
                                    fontSize = 11.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = Color.White,
                                    modifier = Modifier.padding(horizontal = 8.dp, vertical = 2.dp)
                                )
                            }
                        }
                        Spacer(modifier = Modifier.height(10.dp))

                        if (uiState.peers.isEmpty()) {
                            Column(
                                modifier = Modifier.fillMaxWidth().padding(vertical = 10.dp),
                                horizontalAlignment = Alignment.CenterHorizontally
                            ) {
                                Text(
                                    text = "Scanning local network for AeroSync peers...",
                                    fontSize = 12.sp,
                                    color = textMuted,
                                    textAlign = TextAlign.Center
                                )
                            }
                        } else {
                            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                uiState.peers.forEach { peer ->
                                    val isSelected = uiState.selectedPeer?.deviceId == peer.deviceId
                                    Surface(
                                        modifier = Modifier
                                            .fillMaxWidth()
                                            .clip(RoundedCornerShape(10.dp))
                                            .clickable { onSelectPeer(peer) },
                                        color = if (isSelected) (if (isDark) Color(0xFF0C4A6E).copy(alpha = 0.3f) else Color(0xFFE0F2FE)) else cardBgAlt,
                                        border = if (isSelected) androidx.compose.foundation.BorderStroke(1.dp, brandBlue) else androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                                    ) {
                                        Row(
                                            modifier = Modifier.padding(horizontal = 10.dp, vertical = 8.dp),
                                            verticalAlignment = Alignment.CenterVertically
                                        ) {
                                            Icon(Icons.Default.Devices, contentDescription = null, tint = brandBlue, modifier = Modifier.size(20.dp))
                                            Spacer(modifier = Modifier.width(10.dp))
                                            Column(modifier = Modifier.weight(1f)) {
                                                Text(peer.deviceName, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, color = textPrimary)
                                                Text(peer.ipAddress, fontSize = 11.sp, color = textSecondary)
                                            }
                                            Box(modifier = Modifier.size(7.dp).clip(CircleShape).background(activeDotColor))
                                        }
                                    }
                                }
                            }
                        }

                        Spacer(modifier = Modifier.height(10.dp))
                        Surface(
                            shape = RoundedCornerShape(10.dp),
                            color = cardBgAlt,
                            border = androidx.compose.foundation.BorderStroke(1.dp, borderColor),
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { onSelectTab(1) }
                        ) {
                            Text(
                                text = "Manage Devices & Direct IP",
                                fontSize = 12.sp,
                                fontWeight = FontWeight.SemiBold,
                                color = brandBlue,
                                textAlign = TextAlign.Center,
                                modifier = Modifier.padding(vertical = 8.dp)
                            )
                        }
                    }
                }
            }

            // Recent Activity & Active Transfer Card
            item {
                Surface(
                    modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                ) {
                    Column(modifier = Modifier.padding(14.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text("RECENT TRANSFERS", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = textSecondary, letterSpacing = 0.8.sp)
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                if (uiState.history.isNotEmpty()) {
                                    Text(
                                        text = "Clear",
                                        fontSize = 11.sp,
                                        fontWeight = FontWeight.SemiBold,
                                        color = Color(0xFFEF4444),
                                        modifier = Modifier.clickable { onClearHistory() }.padding(4.dp)
                                    )
                                    Spacer(modifier = Modifier.width(6.dp))
                                }
                                Text(
                                    text = "View All",
                                    fontSize = 11.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = brandBlue,
                                    modifier = Modifier.clickable { onSelectTab(2) }.padding(4.dp)
                                )
                            }
                        }
                        Spacer(modifier = Modifier.height(10.dp))

                        val active = uiState.activeTransfer
                        if (active != null) {
                            Surface(
                                modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(10.dp)),
                                color = cardBgAlt,
                                border = androidx.compose.foundation.BorderStroke(1.dp, brandBlue)
                            ) {
                                Column(modifier = Modifier.padding(10.dp)) {
                                    Text(active.fileName, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, color = textPrimary)
                                    Spacer(modifier = Modifier.height(6.dp))
                                    val pct = if (active.totalBytes > 0) ((active.transferredBytes * 100) / active.totalBytes).toInt() else 0
                                    LinearProgressIndicator(
                                        progress = (pct / 100f).coerceIn(0f, 1f),
                                        modifier = Modifier.fillMaxWidth().height(5.dp).clip(RoundedCornerShape(3.dp)),
                                        color = brandBlue,
                                        trackColor = if (isDark) Color(0xFF374151) else Color(0xFFE2E8F0)
                                    )
                                    Row(
                                        modifier = Modifier.fillMaxWidth().padding(top = 6.dp),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Text(
                                            text = "${active.transferredBytes / (1024 * 1024)} MB / ${active.totalBytes / (1024 * 1024)} MB (${pct}%)",
                                            fontSize = 11.sp,
                                            color = textSecondary
                                        )
                                        Row {
                                            Text(
                                                text = if (active.isPaused) "Resume" else "Pause",
                                                fontSize = 11.sp,
                                                fontWeight = FontWeight.Bold,
                                                color = brandBlue,
                                                modifier = Modifier.clickable { onTogglePause() }.padding(4.dp)
                                            )
                                            Spacer(modifier = Modifier.width(6.dp))
                                            Text(
                                                text = "Cancel",
                                                fontSize = 11.sp,
                                                fontWeight = FontWeight.Bold,
                                                color = Color(0xFFEF4444),
                                                modifier = Modifier.clickable { onCancelTransfer() }.padding(4.dp)
                                            )
                                        }
                                    }
                                }
                            }
                            Spacer(modifier = Modifier.height(8.dp))
                        }

                        if (active == null && uiState.history.isEmpty() && uiState.transferQueue.isEmpty()) {
                            Text(
                                text = "No active or recent transfers.",
                                fontSize = 12.sp,
                                color = textMuted,
                                textAlign = TextAlign.Center,
                                modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)
                            )
                        } else {
                            // Show queued items
                            uiState.transferQueue.filter { it.status == QueueItemStatus.QUEUED || it.status == QueueItemStatus.PAUSED }.take(2).forEach { qItem ->
                                Surface(
                                    modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(8.dp)),
                                    color = cardBgAlt
                                ) {
                                    Row(
                                        modifier = Modifier.padding(horizontal = 10.dp, vertical = 6.dp),
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Icon(Icons.Default.Schedule, contentDescription = null, tint = textMuted, modifier = Modifier.size(16.dp))
                                        Spacer(modifier = Modifier.width(8.dp))
                                        Column(modifier = Modifier.weight(1f)) {
                                            Text(qItem.fileName, fontSize = 12.sp, fontWeight = FontWeight.Medium, color = textPrimary, maxLines = 1, overflow = TextOverflow.Ellipsis)
                                            Text("${qItem.fileSize / (1024 * 1024)} MB • ${qItem.status.name}", fontSize = 10.sp, color = textMuted)
                                        }
                                        IconButton(onClick = { onRemoveQueueItem(qItem.id) }, modifier = Modifier.size(24.dp)) {
                                            Icon(Icons.Default.Close, contentDescription = "Remove", tint = Color(0xFFEF4444), modifier = Modifier.size(14.dp))
                                        }
                                    }
                                }
                                Spacer(modifier = Modifier.height(6.dp))
                            }

                            // Show recent history
                            uiState.history.take(2).forEach { hItem ->
                                Surface(
                                    modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(8.dp)),
                                    color = cardBgAlt
                                ) {
                                    Row(
                                        modifier = Modifier.padding(horizontal = 10.dp, vertical = 6.dp),
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Icon(Icons.Default.CheckCircle, contentDescription = null, tint = activeDotColor, modifier = Modifier.size(16.dp))
                                        Spacer(modifier = Modifier.width(8.dp))
                                        Column(modifier = Modifier.weight(1f)) {
                                            Text(hItem.fileName, fontSize = 12.sp, fontWeight = FontWeight.Medium, color = textPrimary, maxLines = 1, overflow = TextOverflow.Ellipsis)
                                            Text("${hItem.fileSize / (1024 * 1024)} MB • ${if (hItem.isReceived) "Received" else "Sent"}", fontSize = 10.sp, color = textMuted)
                                        }
                                    }
                                }
                                Spacer(modifier = Modifier.height(6.dp))
                            }
                        }
                    }
                }
            }
        }

        // Direct IP Dialog
        if (showDirectIpDialog) {
            AlertDialog(
                onDismissRequest = { showDirectIpDialog = false },
                containerColor = cardBg,
                title = { Text("Connect via Direct IP", fontSize = 17.sp, fontWeight = FontWeight.Bold, color = textPrimary) },
                text = {
                    Column {
                        Text("Enter remote IP address (e.g. 192.168.43.1):", fontSize = 12.sp, color = textSecondary)
                        Spacer(modifier = Modifier.height(8.dp))
                        OutlinedTextField(
                            value = directIpInput,
                            onValueChange = { directIpInput = it },
                            singleLine = true,
                            colors = OutlinedTextFieldDefaults.colors(
                                focusedTextColor = textPrimary,
                                unfocusedTextColor = textPrimary,
                                focusedBorderColor = brandBlue,
                                unfocusedBorderColor = borderColor
                            ),
                            modifier = Modifier.fillMaxWidth()
                        )
                    }
                },
                confirmButton = {
                    Button(
                        onClick = {
                            showDirectIpDialog = false
                            onConnectDirectIp(directIpInput)
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = brandBlue)
                    ) {
                        Text("Connect", color = Color.White, fontWeight = FontWeight.Bold)
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showDirectIpDialog = false }) {
                        Text("Cancel", color = textMuted)
                    }
                }
            )
        }

        // Pairing PIN Prompt Dialog
        val prompt = uiState.incomingPairingPrompt
        if (prompt != null) {
            AlertDialog(
                onDismissRequest = { onRespondPairing(false) },
                containerColor = cardBg,
                title = { Text("Pairing Request", fontSize = 17.sp, fontWeight = FontWeight.Bold, color = textPrimary) },
                text = {
                    Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.fillMaxWidth()) {
                        Text("${prompt.senderName} wants to sync with this device.", fontSize = 13.sp, color = textSecondary, textAlign = TextAlign.Center)
                        Spacer(modifier = Modifier.height(12.dp))
                        Surface(
                            shape = RoundedCornerShape(12.dp),
                            color = cardBgAlt,
                            border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                        ) {
                            Text(
                                text = "PIN: ${prompt.pin}",
                                fontSize = 26.sp,
                                fontWeight = FontWeight.ExtraBold,
                                color = brandBlue,
                                modifier = Modifier.padding(horizontal = 20.dp, vertical = 10.dp)
                            )
                        }
                        Spacer(modifier = Modifier.height(10.dp))
                        Text("Verify this PIN matches the sender screen before accepting.", fontSize = 11.sp, color = textMuted, textAlign = TextAlign.Center)
                    }
                },
                confirmButton = {
                    Button(
                        onClick = { onRespondPairing(true) },
                        colors = ButtonDefaults.buttonColors(containerColor = activeDotColor)
                    ) {
                        Text("Confirm & Pair", color = Color.White, fontWeight = FontWeight.Bold)
                    }
                },
                dismissButton = {
                    TextButton(onClick = { onRespondPairing(false) }) {
                        Text("Decline", color = Color(0xFFEF4444))
                    }
                }
            )
        }
    }
}
