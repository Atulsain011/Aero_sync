package com.aerosync.app.ui.screens

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
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.aerosync.app.ui.components.AeroSyncTopAppBar
import com.aerosync.app.viewmodel.AeroSyncUiState
import com.aerosync.app.viewmodel.DiscoveredPeer

@Composable
fun DevicesScreen(
    uiState: AeroSyncUiState,
    onSelectPeer: (DiscoveredPeer) -> Unit,
    onConnectDirectIp: (String) -> Unit,
    onUpdateDeviceName: (String) -> Unit,
    onRefreshPeers: () -> Unit,
    onSelectTab: (Int) -> Unit,
    onToggleTheme: () -> Unit = {}
) {
    var showDirectIpDialog by remember { mutableStateOf(false) }
    var directIpInput by remember { mutableStateOf("192.168.43.1") }
    var showRenameDialog by remember { mutableStateOf(false) }
    var renameInput by remember { mutableStateOf(uiState.deviceName) }

    val isDark = uiState.isDarkTheme
    val bgColor = if (isDark) Color(0xFF090D16) else Color(0xFFF8FAFC)
    val cardBg = if (isDark) Color(0xFF111827) else Color(0xFFFFFFFF)
    val cardBgAlt = if (isDark) Color(0xFF1F2937) else Color(0xFFF8FAFC)
    val borderColor = if (isDark) Color(0xFF374151) else Color(0xFFE2E8F0)
    val textPrimary = if (isDark) Color(0xFFF9FAFB) else Color(0xFF0F172A)
    val textSecondary = if (isDark) Color(0xFF9CA3AF) else Color(0xFF64748B)

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(bgColor)
            .statusBarsPadding()
            .navigationBarsPadding()
            .padding(horizontal = 16.dp, vertical = 6.dp)
    ) {
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            // 1. Top Navigation Bar: Brand + Segmented Navigation Tabs + Refresh/Theme
            item {
                AeroSyncTopAppBar(
                    title = "Devices",
                    selectedTab = uiState.selectedTab,
                    themeMode = uiState.themeMode,
                    isDark = isDark,
                    onSelectTab = onSelectTab,
                    onToggleTheme = onToggleTheme,
                    rightActionContent = {
                        Surface(
                            modifier = Modifier
                                .size(36.dp)
                                .clip(CircleShape)
                                .border(1.dp, borderColor, CircleShape)
                                .clickable { onRefreshPeers() },
                            color = if (isDark) Color(0xFF1E293B) else Color(0xFFFFFFFF),
                            shadowElevation = 2.dp
                        ) {
                            Box(contentAlignment = Alignment.Center) {
                                Icon(
                                    imageVector = Icons.Default.Refresh,
                                    contentDescription = "Scan Devices",
                                    tint = Color(0xFF3B82F6),
                                    modifier = Modifier.size(18.dp)
                                )
                            }
                        }
                    }
                )
            }

            // 2. Local Device Info Card
            item {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor),
                    shadowElevation = if (isDark) 0.dp else 2.dp
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(14.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.weight(1f)) {
                            Box(
                                modifier = Modifier
                                    .size(42.dp)
                                    .clip(CircleShape)
                                    .background(Color(0xFF3B82F6).copy(alpha = 0.15f)),
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(Icons.Default.Smartphone, contentDescription = null, tint = Color(0xFF3B82F6), modifier = Modifier.size(22.dp))
                            }
                            Spacer(modifier = Modifier.width(12.dp))
                            Column(modifier = Modifier.weight(1f)) {
                                Text(uiState.deviceName, fontSize = 14.5.sp, fontWeight = FontWeight.Bold, color = textPrimary, maxLines = 1, overflow = TextOverflow.Ellipsis)
                                Text("${uiState.connectionTypeLabel} • Port 48124 • Beacon Active", fontSize = 10.5.sp, color = textSecondary, maxLines = 1, overflow = TextOverflow.Ellipsis)
                            }
                        }
                        IconButton(
                            onClick = {
                                renameInput = uiState.deviceName
                                showRenameDialog = true
                            },
                            modifier = Modifier.size(32.dp)
                        ) {
                            Icon(Icons.Default.Edit, contentDescription = "Rename", tint = Color(0xFF3B82F6), modifier = Modifier.size(16.dp))
                        }
                    }
                }
            }

            // 3. Discovered Peers Section
            item {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor),
                    shadowElevation = if (isDark) 0.dp else 2.dp
                ) {
                    Column(modifier = Modifier.padding(14.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text("NEARBY DISCOVERED PEERS", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = textSecondary, letterSpacing = 0.6.sp)
                                Spacer(modifier = Modifier.width(8.dp))
                                Surface(shape = RoundedCornerShape(8.dp), color = Color(0xFF10B981).copy(alpha = 0.15f)) {
                                    Text(
                                        text = "${uiState.peers.size} Online",
                                        fontSize = 10.5.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = Color(0xFF10B981),
                                        modifier = Modifier.padding(horizontal = 7.dp, vertical = 2.dp)
                                    )
                                }
                            }
                        }

                        Spacer(modifier = Modifier.height(12.dp))

                        if (uiState.peers.isEmpty()) {
                            Column(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(vertical = 20.dp),
                                horizontalAlignment = Alignment.CenterHorizontally
                            ) {
                                CircularProgressIndicator(
                                    modifier = Modifier.size(24.dp),
                                    color = Color(0xFF3B82F6),
                                    strokeWidth = 2.dp
                                )
                                Spacer(modifier = Modifier.height(10.dp))
                                Text(
                                    text = "Listening for nearby AeroSync devices...",
                                    fontSize = 12.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = textPrimary
                                )
                                Spacer(modifier = Modifier.height(2.dp))
                                Text(
                                    text = "Ensure both devices are connected to the same Wi-Fi or Hotspot.",
                                    fontSize = 10.5.sp,
                                    color = textSecondary,
                                    textAlign = TextAlign.Center
                                )
                            }
                        } else {
                            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                uiState.peers.forEach { peer ->
                                    val isWindows = peer.deviceType.contains("win", ignoreCase = true)
                                    val isSelected = uiState.selectedPeer?.deviceId == peer.deviceId

                                    Surface(
                                        modifier = Modifier
                                            .fillMaxWidth()
                                            .clip(RoundedCornerShape(12.dp))
                                            .clickable { onSelectPeer(peer) },
                                        color = if (isSelected) Color(0xFF2563EB).copy(alpha = 0.08f) else cardBgAlt,
                                        border = androidx.compose.foundation.BorderStroke(
                                            if (isSelected) 1.5.dp else 1.dp,
                                            if (isSelected) Color(0xFF2563EB) else borderColor
                                        )
                                    ) {
                                        Row(
                                            modifier = Modifier.padding(12.dp),
                                            horizontalArrangement = Arrangement.SpaceBetween,
                                            verticalAlignment = Alignment.CenterVertically
                                        ) {
                                            Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.weight(1f)) {
                                                Box(
                                                    modifier = Modifier
                                                        .size(36.dp)
                                                        .clip(CircleShape)
                                                        .background(if (isWindows) Color(0xFF3B82F6).copy(alpha = 0.15f) else Color(0xFF10B981).copy(alpha = 0.15f)),
                                                    contentAlignment = Alignment.Center
                                                ) {
                                                    Icon(
                                                        imageVector = if (isWindows) Icons.Default.Computer else Icons.Default.PhoneAndroid,
                                                        contentDescription = null,
                                                        tint = if (isWindows) Color(0xFF3B82F6) else Color(0xFF10B981),
                                                        modifier = Modifier.size(18.dp)
                                                    )
                                                }
                                                Spacer(modifier = Modifier.width(10.dp))
                                                Column(modifier = Modifier.weight(1f)) {
                                                    Row(verticalAlignment = Alignment.CenterVertically) {
                                                        Text(peer.deviceName, fontSize = 13.sp, fontWeight = FontWeight.Bold, color = textPrimary, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f, fill = false))
                                                        if (isSelected) {
                                                            Spacer(modifier = Modifier.width(6.dp))
                                                            Surface(
                                                                shape = RoundedCornerShape(6.dp),
                                                                color = Color(0xFF2563EB).copy(alpha = 0.15f)
                                                            ) {
                                                                Text("Selected", fontSize = 9.sp, fontWeight = FontWeight.Bold, color = Color(0xFF2563EB), modifier = Modifier.padding(horizontal = 4.dp, vertical = 1.dp))
                                                            }
                                                        }
                                                    }
                                                    Text("${peer.ipAddress} • ${peer.deviceType}", fontSize = 10.5.sp, color = textSecondary, maxLines = 1, overflow = TextOverflow.Ellipsis)
                                                }
                                            }

                                            Button(
                                                onClick = {
                                                    onSelectPeer(peer)
                                                    onSelectTab(0)
                                                },
                                                modifier = Modifier.height(30.dp),
                                                contentPadding = PaddingValues(horizontal = 10.dp, vertical = 0.dp),
                                                shape = RoundedCornerShape(8.dp),
                                                colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF2563EB))
                                            ) {
                                                Text("Send Files", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = Color.White)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Spacer(modifier = Modifier.height(10.dp))

                        // Direct IP Connect Button
                        OutlinedButton(
                            onClick = { showDirectIpDialog = true },
                            modifier = Modifier.fillMaxWidth().height(40.dp),
                            shape = RoundedCornerShape(10.dp),
                            colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFF2563EB))
                        ) {
                            Icon(Icons.Default.AddLink, contentDescription = null, modifier = Modifier.size(15.dp))
                            Spacer(modifier = Modifier.width(6.dp))
                            Text("Connect via Direct IP Address", fontSize = 12.sp, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }
        }

        // Direct IP Dialog
        if (showDirectIpDialog) {
            AlertDialog(
                onDismissRequest = { showDirectIpDialog = false },
                title = { Text("Direct IP Connection", fontWeight = FontWeight.Bold, color = textPrimary) },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("Enter the IP address of the target device on your Wi-Fi or Hotspot:", fontSize = 12.sp, color = textSecondary)
                        OutlinedTextField(
                            value = directIpInput,
                            onValueChange = { directIpInput = it },
                            label = { Text("Target IP Address") },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth()
                        )
                    }
                },
                confirmButton = {
                    Button(
                        onClick = {
                            if (directIpInput.isNotBlank()) {
                                onConnectDirectIp(directIpInput.trim())
                                showDirectIpDialog = false
                            }
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF2563EB))
                    ) {
                        Text("Connect", color = Color.White)
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showDirectIpDialog = false }) {
                        Text("Cancel", color = textSecondary)
                    }
                },
                containerColor = cardBg
            )
        }

        // Rename Device Dialog
        if (showRenameDialog) {
            AlertDialog(
                onDismissRequest = { showRenameDialog = false },
                title = { Text("Rename Device", fontWeight = FontWeight.Bold, color = textPrimary) },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("Set the broadcast name visible to other AeroSync peers:", fontSize = 12.sp, color = textSecondary)
                        OutlinedTextField(
                            value = renameInput,
                            onValueChange = { renameInput = it },
                            label = { Text("Device Name") },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth()
                        )
                    }
                },
                confirmButton = {
                    Button(
                        onClick = {
                            if (renameInput.isNotBlank()) {
                                onUpdateDeviceName(renameInput.trim())
                                showRenameDialog = false
                            }
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF2563EB))
                    ) {
                        Text("Save", color = Color.White)
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showRenameDialog = false }) {
                        Text("Cancel", color = textSecondary)
                    }
                },
                containerColor = cardBg
            )
        }
    }
}
