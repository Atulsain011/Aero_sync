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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.aerosync.app.ui.components.AeroSyncLogoIcon
import com.aerosync.app.viewmodel.AeroSyncUiState
import com.aerosync.app.viewmodel.DiscoveredPeer

@Composable
fun DevicesScreen(
    uiState: AeroSyncUiState,
    onSelectPeer: (DiscoveredPeer) -> Unit,
    onConnectDirectIp: (String) -> Unit,
    onUpdateDeviceName: (String) -> Unit,
    onRefreshPeers: () -> Unit,
    onSelectTab: (Int) -> Unit
) {
    var showDirectIpDialog by remember { mutableStateOf(false) }
    var directIpInput by remember { mutableStateOf("192.168.43.1") }
    var showRenameDialog by remember { mutableStateOf(false) }
    var renameInput by remember { mutableStateOf(uiState.deviceName) }

    val isDark = uiState.isDarkTheme
    val bgColor = if (isDark) Color(0xFF090D16) else Color(0xFFF8FAFC)
    val cardBg = if (isDark) Color(0xFF111827) else Color(0xFFFFFFFF)
    val cardBgAlt = if (isDark) Color(0xFF1F2937) else Color(0xFFF1F5F9)
    val borderColor = if (isDark) Color(0xFF374151) else Color(0xFFE2E8F0)
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
            .padding(horizontal = 16.dp, vertical = 8.dp)
    ) {
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            // Header Tabs
            item {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        AeroSyncLogoIcon(size = 34.dp)
                        Spacer(modifier = Modifier.width(10.dp))
                        Text(
                            text = "Devices",
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
                            val isSelected = index == 1
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
                }
            }

            // Local Device Broadcast Card
            item {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(14.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Surface(
                                shape = CircleShape,
                                color = if (isDark) Color(0xFF0C4A6E) else Color(0xFFE0F2FE),
                                modifier = Modifier.size(42.dp)
                            ) {
                                Box(contentAlignment = Alignment.Center) {
                                    Icon(Icons.Default.Smartphone, contentDescription = null, tint = brandBlue, modifier = Modifier.size(22.dp))
                                }
                            }
                            Spacer(modifier = Modifier.width(12.dp))
                            Column {
                                Text(uiState.deviceName, fontSize = 15.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                Text("Broadcast Active • Port 48124", fontSize = 11.sp, color = textSecondary)
                            }
                        }
                        IconButton(
                            onClick = {
                                renameInput = uiState.deviceName
                                showRenameDialog = true
                            },
                            modifier = Modifier.size(32.dp)
                        ) {
                            Icon(Icons.Default.Edit, contentDescription = "Rename", tint = brandBlue, modifier = Modifier.size(18.dp))
                        }
                    }
                }
            }

            // Discovered Devices Section Card
            item {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                ) {
                    Column(modifier = Modifier.padding(14.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text("DISCOVERED PEERS", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = textSecondary, letterSpacing = 0.8.sp)
                                Spacer(modifier = Modifier.width(8.dp))
                                Surface(shape = RoundedCornerShape(10.dp), color = brandBlue) {
                                    Text(
                                        text = "${uiState.peers.size} Found",
                                        fontSize = 11.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = Color.White,
                                        modifier = Modifier.padding(horizontal = 7.dp, vertical = 2.dp)
                                    )
                                }
                            }
                            IconButton(onClick = onRefreshPeers, modifier = Modifier.size(28.dp)) {
                                Icon(Icons.Default.Refresh, contentDescription = "Refresh", tint = brandBlue, modifier = Modifier.size(18.dp))
                            }
                        }

                        Spacer(modifier = Modifier.height(12.dp))

                        if (uiState.peers.isEmpty()) {
                            Column(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(vertical = 16.dp),
                                horizontalAlignment = Alignment.CenterHorizontally
                            ) {
                                CircularProgressIndicator(
                                    modifier = Modifier.size(28.dp),
                                    color = brandBlue,
                                    strokeWidth = 2.5.dp
                                )
                                Spacer(modifier = Modifier.height(12.dp))
                                Text(
                                    text = "Searching for nearby devices on Wi-Fi / Hotspot...",
                                    fontSize = 13.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = textPrimary,
                                    textAlign = TextAlign.Center
                                )
                                Spacer(modifier = Modifier.height(4.dp))
                                Text(
                                    text = "Ensure other devices have AeroSync open on the same local network.",
                                    fontSize = 11.sp,
                                    color = textMuted,
                                    textAlign = TextAlign.Center,
                                    modifier = Modifier.padding(horizontal = 12.dp)
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
                                        border = androidx.compose.foundation.BorderStroke(1.dp, if (isSelected) brandBlue else borderColor)
                                    ) {
                                        Row(
                                            modifier = Modifier.padding(12.dp),
                                            verticalAlignment = Alignment.CenterVertically
                                        ) {
                                            val icon = when (peer.deviceType.lowercase()) {
                                                "windows", "desktop" -> Icons.Default.Computer
                                                "macos", "mac" -> Icons.Default.LaptopMac
                                                "ios", "iphone", "ipad" -> Icons.Default.PhoneIphone
                                                else -> Icons.Default.Devices
                                            }
                                            Icon(icon, contentDescription = null, tint = brandBlue, modifier = Modifier.size(24.dp))
                                            Spacer(modifier = Modifier.width(12.dp))
                                            Column(modifier = Modifier.weight(1f)) {
                                                Text(peer.deviceName, fontSize = 14.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                                Text("${peer.ipAddress}:${peer.port} • ${peer.deviceType.uppercase()}", fontSize = 11.sp, color = textSecondary)
                                            }
                                            Box(modifier = Modifier.size(8.dp).clip(CircleShape).background(activeDotColor))
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Direct IP Connection Card
            item {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                ) {
                    Column(modifier = Modifier.padding(14.dp)) {
                        Text("DIRECT IP CONNECTION", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = textSecondary, letterSpacing = 0.8.sp)
                        Spacer(modifier = Modifier.height(8.dp))
                        Text(
                            text = "Connect directly using target IP (e.g. 192.168.43.1 for Mobile Hotspot / USB Tethering gateway).",
                            fontSize = 11.sp,
                            color = textMuted
                        )
                        Spacer(modifier = Modifier.height(12.dp))
                        Button(
                            onClick = { showDirectIpDialog = true },
                            shape = RoundedCornerShape(12.dp),
                            colors = ButtonDefaults.buttonColors(containerColor = brandBlue, contentColor = Color.White),
                            modifier = Modifier.fillMaxWidth(),
                            contentPadding = PaddingValues(vertical = 8.dp)
                        ) {
                            Icon(Icons.Default.Cable, contentDescription = null, modifier = Modifier.size(16.dp))
                            Spacer(modifier = Modifier.width(8.dp))
                            Text("Connect via IP Address", fontSize = 13.sp, fontWeight = FontWeight.Bold)
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
                title = { Text("Direct IP Connection", fontSize = 17.sp, fontWeight = FontWeight.Bold, color = textPrimary) },
                text = {
                    Column {
                        Text("Enter the remote device IP address:", fontSize = 12.sp, color = textSecondary)
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
                        Text("Connect & Pair", color = Color.White, fontWeight = FontWeight.Bold)
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showDirectIpDialog = false }) {
                        Text("Cancel", color = textMuted)
                    }
                }
            )
        }

        // Rename Dialog
        if (showRenameDialog) {
            AlertDialog(
                onDismissRequest = { showRenameDialog = false },
                containerColor = cardBg,
                title = { Text("Change Device Name", fontSize = 17.sp, fontWeight = FontWeight.Bold, color = textPrimary) },
                text = {
                    Column {
                        Text("Enter a new broadcast name for this device:", fontSize = 12.sp, color = textSecondary)
                        Spacer(modifier = Modifier.height(8.dp))
                        OutlinedTextField(
                            value = renameInput,
                            onValueChange = { renameInput = it },
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
                            showRenameDialog = false
                            onUpdateDeviceName(renameInput)
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = brandBlue)
                    ) {
                        Text("Save", color = Color.White, fontWeight = FontWeight.Bold)
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showRenameDialog = false }) {
                        Text("Cancel", color = textMuted)
                    }
                }
            )
        }
    }
}
