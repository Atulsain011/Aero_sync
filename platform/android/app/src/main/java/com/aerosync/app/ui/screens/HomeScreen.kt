package com.aerosync.app.ui.screens

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
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
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.aerosync.app.ui.components.AeroSyncTopAppBar
import com.aerosync.app.viewmodel.AeroSyncUiState
import com.aerosync.app.viewmodel.DiscoveredPeer
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun HomeScreen(
    uiState: AeroSyncUiState,
    onSelectPeer: (DiscoveredPeer) -> Unit,
    onConnectDirectIp: (String) -> Unit,
    onPickFiles: () -> Unit,
    onRemoveSelectedFile: (String) -> Unit,
    onClearSelectedFiles: () -> Unit,
    onSendSelectedFiles: () -> Unit,
    onRespondPairing: (Boolean) -> Unit,
    onToggleTheme: () -> Unit,
    onSelectTab: (Int) -> Unit,
    onTogglePause: () -> Unit,
    onCancelTransfer: () -> Unit,
    onChangeDownloadLocation: () -> Unit
) {
    var showDirectIpDialog by remember { mutableStateOf(false) }
    var directIpInput by remember { mutableStateOf("192.168.43.1") }

    // Collapsible sections - Collapsed by default as requested
    var isSettingsExpanded by remember { mutableStateOf(false) }
    var isPeersExpanded by remember { mutableStateOf(false) }
    var isTransfersExpanded by remember { mutableStateOf(false) }

    val isDark = uiState.isDarkTheme
    val bgColor = if (isDark) Color(0xFF090D16) else Color(0xFFF8FAFC)
    val cardBg = if (isDark) Color(0xFF111827) else Color(0xFFFFFFFF)
    val cardBgAlt = if (isDark) Color(0xFF1F2937) else Color(0xFFF8FAFC)
    val borderColor = if (isDark) Color(0xFF374151) else Color(0xFFE2E8F0)
    val textPrimary = if (isDark) Color(0xFFF9FAFB) else Color(0xFF0F172A)
    val textSecondary = if (isDark) Color(0xFF9CA3AF) else Color(0xFF64748B)

    val primaryGradient = Brush.horizontalGradient(
        listOf(Color(0xFF2563EB), Color(0xFF7C3AED))
    )
    val circleDashedGradient = Brush.sweepGradient(
        listOf(Color(0xFF38BDF8), Color(0xFF6366F1), Color(0xFF8B5CF6), Color(0xFF38BDF8))
    )

    val dateFormat = SimpleDateFormat("MMM d, HH:mm", Locale.getDefault())

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
                .padding(horizontal = 16.dp, vertical = 6.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            // 1. Top Navigation Bar: Brand + Segmented Navigation Tabs (Files, Devices, Activity) + Theme Toggle
            item {
                AeroSyncTopAppBar(
                    title = "AeroSync",
                    selectedTab = uiState.selectedTab,
                    themeMode = uiState.themeMode,
                    isDark = isDark,
                    onSelectTab = onSelectTab,
                    onToggleTheme = onToggleTheme
                )
            }

            // 2. Main Hero Card: Circular Drop Zone + 3 Feature Badges + Browse Files Button
            item {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(22.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor),
                    shadowElevation = if (isDark) 0.dp else 3.dp
                ) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 16.dp, vertical = 20.dp)
                    ) {
                        Canvas(modifier = Modifier.matchParentSize()) {
                            val dotColor = if (isDark) Color(0x1538BDF8) else Color(0x122563EB)
                            val ringColor = if (isDark) Color(0x158B5CF6) else Color(0x157C3AED)
                            drawCircle(dotColor, radius = 5f, center = Offset(size.width * 0.12f, size.height * 0.15f))
                            drawCircle(dotColor, radius = 3f, center = Offset(size.width * 0.88f, size.height * 0.22f))
                            drawCircle(ringColor, radius = 4f, center = Offset(size.width * 0.82f, size.height * 0.72f))
                            drawCircle(dotColor, radius = 3f, center = Offset(size.width * 0.15f, size.height * 0.85f))
                        }

                        Column(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalAlignment = Alignment.CenterHorizontally
                        ) {
                            // Circular Drop & Tap Zone
                            Box(
                                modifier = Modifier
                                    .size(200.dp)
                                    .clickable(
                                        interactionSource = remember { MutableInteractionSource() },
                                        indication = null
                                    ) { onPickFiles() },
                                contentAlignment = Alignment.Center
                            ) {
                                Canvas(modifier = Modifier.fillMaxSize()) {
                                    val strokeWidth = 2.dp.toPx()
                                    val dashLength = 8.dp.toPx()
                                    val gapLength = 6.dp.toPx()
                                    drawCircle(
                                        brush = circleDashedGradient,
                                        radius = (size.minDimension / 2) - strokeWidth,
                                        style = Stroke(
                                            width = strokeWidth,
                                            pathEffect = PathEffect.dashPathEffect(floatArrayOf(dashLength, gapLength), 0f)
                                        )
                                    )
                                }

                                Surface(
                                    modifier = Modifier
                                        .size(164.dp)
                                        .clip(CircleShape)
                                        .shadow(elevation = 6.dp, shape = CircleShape),
                                    color = if (isDark) Color(0xFF1E293B) else Color(0xFFFFFFFF),
                                    border = androidx.compose.foundation.BorderStroke(
                                        1.dp,
                                        if (isDark) Color(0xFF334155) else Color(0xFFEFF6FF)
                                    )
                                ) {
                                    Column(
                                        modifier = Modifier.fillMaxSize(),
                                        horizontalAlignment = Alignment.CenterHorizontally,
                                        verticalArrangement = Arrangement.Center
                                    ) {
                                        Box(
                                            modifier = Modifier
                                                .size(54.dp)
                                                .clip(RoundedCornerShape(16.dp))
                                                .background(
                                                    Brush.linearGradient(
                                                        listOf(Color(0xFF3B82F6).copy(alpha = 0.15f), Color(0xFF8B5CF6).copy(alpha = 0.2f))
                                                    )
                                                ),
                                            contentAlignment = Alignment.Center
                                        ) {
                                            Icon(
                                                imageVector = Icons.Default.CloudUpload,
                                                contentDescription = "Upload",
                                                tint = Color(0xFF3B82F6),
                                                modifier = Modifier.size(32.dp)
                                            )
                                        }

                                        Spacer(modifier = Modifier.height(10.dp))

                                        Text(
                                            text = "Drop files here",
                                            fontSize = 15.sp,
                                            fontWeight = FontWeight.Bold,
                                            color = textPrimary
                                        )
                                        Spacer(modifier = Modifier.height(2.dp))
                                        Text(
                                            text = "or tap to pick files",
                                            fontSize = 11.5.sp,
                                            color = textSecondary
                                        )
                                    }
                                }
                            }

                            Spacer(modifier = Modifier.height(16.dp))

                            // 3 Feature Badges Row
                            Surface(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .clip(RoundedCornerShape(14.dp)),
                                color = cardBgAlt,
                                border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                            ) {
                                Row(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .padding(horizontal = 8.dp, vertical = 10.dp),
                                    horizontalArrangement = Arrangement.SpaceEvenly,
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Row(
                                        verticalAlignment = Alignment.CenterVertically,
                                        horizontalArrangement = Arrangement.spacedBy(5.dp)
                                    ) {
                                        Icon(
                                            imageVector = Icons.Default.Bolt,
                                            contentDescription = null,
                                            tint = Color(0xFF2563EB),
                                            modifier = Modifier.size(15.dp)
                                        )
                                        Column(horizontalAlignment = Alignment.Start) {
                                            Text(
                                                text = "High-speed",
                                                fontSize = 10.5.sp,
                                                fontWeight = FontWeight.Bold,
                                                color = textPrimary,
                                                maxLines = 1,
                                                overflow = TextOverflow.Ellipsis
                                            )
                                            Text(
                                                text = "transfer",
                                                fontSize = 9.5.sp,
                                                color = textSecondary,
                                                maxLines = 1,
                                                overflow = TextOverflow.Ellipsis
                                            )
                                        }
                                    }

                                    Box(
                                        modifier = Modifier
                                            .height(24.dp)
                                            .width(1.dp)
                                            .background(borderColor)
                                    )

                                    Row(
                                        verticalAlignment = Alignment.CenterVertically,
                                        horizontalArrangement = Arrangement.spacedBy(5.dp)
                                    ) {
                                        Icon(
                                            imageVector = Icons.Default.Storage,
                                            contentDescription = null,
                                            tint = Color(0xFF10B981),
                                            modifier = Modifier.size(15.dp)
                                        )
                                        Column(horizontalAlignment = Alignment.Start) {
                                            Text(
                                                text = "Multi-gigabyte",
                                                fontSize = 10.5.sp,
                                                fontWeight = FontWeight.Bold,
                                                color = textPrimary,
                                                maxLines = 1,
                                                overflow = TextOverflow.Ellipsis
                                            )
                                            Text(
                                                text = "support",
                                                fontSize = 9.5.sp,
                                                color = textSecondary,
                                                maxLines = 1,
                                                overflow = TextOverflow.Ellipsis
                                            )
                                        }
                                    }

                                    Box(
                                        modifier = Modifier
                                            .height(24.dp)
                                            .width(1.dp)
                                            .background(borderColor)
                                    )

                                    Row(
                                        verticalAlignment = Alignment.CenterVertically,
                                        horizontalArrangement = Arrangement.spacedBy(5.dp)
                                    ) {
                                        Icon(
                                            imageVector = Icons.Default.Wifi,
                                            contentDescription = null,
                                            tint = Color(0xFF8B5CF6),
                                            modifier = Modifier.size(15.dp)
                                        )
                                        Column(horizontalAlignment = Alignment.Start) {
                                            Text(
                                                text = "Direct",
                                                fontSize = 10.5.sp,
                                                fontWeight = FontWeight.Bold,
                                                color = textPrimary,
                                                maxLines = 1,
                                                overflow = TextOverflow.Ellipsis
                                            )
                                            Text(
                                                text = "Wi-Fi",
                                                fontSize = 9.5.sp,
                                                color = textSecondary,
                                                maxLines = 1,
                                                overflow = TextOverflow.Ellipsis
                                            )
                                        }
                                    }
                                }
                            }

                            Spacer(modifier = Modifier.height(14.dp))

                            // Browse Files Action Button
                            Box(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .height(48.dp)
                                    .clip(RoundedCornerShape(24.dp))
                                    .background(primaryGradient)
                                    .clickable { onPickFiles() },
                                contentAlignment = Alignment.Center
                            ) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.FolderOpen,
                                        contentDescription = "Browse Files",
                                        tint = Color.White,
                                        modifier = Modifier.size(18.dp)
                                    )
                                    Text(
                                        text = "Browse Files",
                                        fontSize = 14.5.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = Color.White,
                                        letterSpacing = 0.2.sp
                                    )
                                }
                            }
                        }
                    }
                }
            }

            // 3. Staged / Selected Files Card (Rendered immediately upon picking files)
            if (uiState.selectedFiles.isNotEmpty()) {
                item {
                    Surface(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(18.dp)),
                        color = cardBg,
                        border = androidx.compose.foundation.BorderStroke(1.5.dp, Color(0xFF2563EB)),
                        shadowElevation = 4.dp
                    ) {
                        Column(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(14.dp),
                            verticalArrangement = Arrangement.spacedBy(10.dp)
                        ) {
                            // Header
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                                ) {
                                    Box(
                                        modifier = Modifier
                                            .size(28.dp)
                                            .clip(CircleShape)
                                            .background(Color(0xFF2563EB).copy(alpha = 0.15f)),
                                        contentAlignment = Alignment.Center
                                    ) {
                                        Icon(
                                            imageVector = Icons.Default.CheckCircle,
                                            contentDescription = null,
                                            tint = Color(0xFF2563EB),
                                            modifier = Modifier.size(16.dp)
                                        )
                                    }
                                    Text(
                                        text = "Selected Files (${uiState.selectedFiles.size})",
                                        fontSize = 14.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = textPrimary
                                    )
                                }
                                TextButton(
                                    onClick = onClearSelectedFiles,
                                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp)
                                ) {
                                    Text(
                                        text = "Clear All",
                                        fontSize = 11.5.sp,
                                        color = Color(0xFFEF4444),
                                        fontWeight = FontWeight.SemiBold
                                    )
                                }
                            }

                            // Selected Files List
                            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                                uiState.selectedFiles.forEach { file ->
                                    Surface(
                                        modifier = Modifier.fillMaxWidth(),
                                        shape = RoundedCornerShape(10.dp),
                                        color = cardBgAlt,
                                        border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                                    ) {
                                        Row(
                                            modifier = Modifier
                                                .fillMaxWidth()
                                                .padding(horizontal = 10.dp, vertical = 8.dp),
                                            horizontalArrangement = Arrangement.SpaceBetween,
                                            verticalAlignment = Alignment.CenterVertically
                                        ) {
                                            Row(
                                                verticalAlignment = Alignment.CenterVertically,
                                                horizontalArrangement = Arrangement.spacedBy(8.dp),
                                                modifier = Modifier.weight(1f)
                                            ) {
                                                Icon(
                                                    imageVector = Icons.Default.InsertDriveFile,
                                                    contentDescription = null,
                                                    tint = Color(0xFF3B82F6),
                                                    modifier = Modifier.size(20.dp)
                                                )
                                                Column(modifier = Modifier.weight(1f)) {
                                                    Text(
                                                        text = file.fileName,
                                                        fontSize = 12.5.sp,
                                                        fontWeight = FontWeight.SemiBold,
                                                        color = textPrimary,
                                                        maxLines = 1,
                                                        overflow = TextOverflow.Ellipsis
                                                    )
                                                    Row(
                                                        verticalAlignment = Alignment.CenterVertically,
                                                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                                                    ) {
                                                        Text(
                                                            text = file.formattedSize,
                                                            fontSize = 10.5.sp,
                                                            color = textSecondary
                                                        )
                                                        val ext = file.fileName.substringAfterLast('.', "").uppercase()
                                                        if (ext.isNotBlank() && ext.length <= 5) {
                                                            Surface(
                                                                shape = RoundedCornerShape(4.dp),
                                                                color = Color(0xFF3B82F6).copy(alpha = 0.12f)
                                                            ) {
                                                                Text(
                                                                    text = ext,
                                                                    fontSize = 9.sp,
                                                                    fontWeight = FontWeight.Bold,
                                                                    color = Color(0xFF3B82F6),
                                                                    modifier = Modifier.padding(horizontal = 4.dp, vertical = 1.dp)
                                                                )
                                                            }
                                                        }
                                                        Surface(
                                                            shape = RoundedCornerShape(4.dp),
                                                            color = Color(0xFF10B981).copy(alpha = 0.12f)
                                                        ) {
                                                            Text(
                                                                text = "Selected",
                                                                fontSize = 9.sp,
                                                                fontWeight = FontWeight.Bold,
                                                                color = Color(0xFF10B981),
                                                                modifier = Modifier.padding(horizontal = 4.dp, vertical = 1.dp)
                                                            )
                                                        }
                                                    }
                                                }
                                            }

                                            IconButton(
                                                onClick = { onRemoveSelectedFile(file.id) },
                                                modifier = Modifier.size(28.dp)
                                            ) {
                                                Icon(
                                                    imageVector = Icons.Default.Close,
                                                    contentDescription = "Remove file",
                                                    tint = Color(0xFFEF4444),
                                                    modifier = Modifier.size(16.dp)
                                                )
                                            }
                                        }
                                    }
                                }
                            }

                            // Target Device Selector Pill
                            val targetPeer = uiState.selectedPeer ?: uiState.peers.firstOrNull()
                            Surface(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .clickable { onSelectTab(1) },
                                shape = RoundedCornerShape(10.dp),
                                color = if (targetPeer != null) Color(0xFF10B981).copy(alpha = 0.1f) else Color(0xFFF59E0B).copy(alpha = 0.1f),
                                border = androidx.compose.foundation.BorderStroke(
                                    1.dp,
                                    if (targetPeer != null) Color(0xFF10B981).copy(alpha = 0.3f) else Color(0xFFF59E0B).copy(alpha = 0.3f)
                                )
                            ) {
                                Row(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .padding(horizontal = 10.dp, vertical = 8.dp),
                                    horizontalArrangement = Arrangement.SpaceBetween,
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Row(
                                        verticalAlignment = Alignment.CenterVertically,
                                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                                    ) {
                                        Icon(
                                            imageVector = if (targetPeer != null) Icons.Default.Devices else Icons.Default.WarningAmber,
                                            contentDescription = null,
                                            tint = if (targetPeer != null) Color(0xFF10B981) else Color(0xFFF59E0B),
                                            modifier = Modifier.size(16.dp)
                                        )
                                        Text(
                                            text = if (targetPeer != null) "Send to: ${targetPeer.deviceName}" else "No target device selected",
                                            fontSize = 11.5.sp,
                                            fontWeight = FontWeight.Bold,
                                            color = if (targetPeer != null) Color(0xFF10B981) else Color(0xFFF59E0B)
                                        )
                                    }
                                    Text(
                                        text = "Change →",
                                        fontSize = 11.sp,
                                        fontWeight = FontWeight.SemiBold,
                                        color = Color(0xFF3B82F6)
                                    )
                                }
                            }

                            // Send Files Button
                            Box(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .height(44.dp)
                                    .clip(RoundedCornerShape(22.dp))
                                    .background(primaryGradient)
                                    .clickable { onSendSelectedFiles() },
                                contentAlignment = Alignment.Center
                            ) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.Send,
                                        contentDescription = "Send",
                                        tint = Color.White,
                                        modifier = Modifier.size(16.dp)
                                    )
                                    Text(
                                        text = "Send ${uiState.selectedFiles.size} File(s) Now",
                                        fontSize = 13.5.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = Color.White
                                    )
                                }
                            }
                        }
                    }
                }
            }

            // 4. Collapsible Card 1: Settings & Status
            item {
                CollapsibleCard(
                    title = "Settings & Status",
                    subtitle = "Download location, network & storage",
                    icon = Icons.Default.Settings,
                    iconBgColor = if (isDark) Color(0xFF1E3A8A).copy(alpha = 0.4f) else Color(0xFFEFF6FF),
                    iconTint = Color(0xFF3B82F6),
                    isExpanded = isSettingsExpanded,
                    onToggle = { isSettingsExpanded = !isSettingsExpanded },
                    cardBg = cardBg,
                    borderColor = borderColor,
                    textPrimary = textPrimary,
                    textSecondary = textSecondary
                ) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(top = 10.dp),
                        verticalArrangement = Arrangement.spacedBy(10.dp)
                    ) {
                        Surface(
                            modifier = Modifier.fillMaxWidth(),
                            shape = RoundedCornerShape(12.dp),
                            color = cardBgAlt,
                            border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                        ) {
                            Row(
                                modifier = Modifier.padding(12.dp),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Column(modifier = Modifier.weight(1f)) {
                                    Text(
                                        text = "Download Location",
                                        fontSize = 12.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = textPrimary
                                    )
                                    Text(
                                        text = if (uiState.downloadDirectory.isNotBlank()) uiState.downloadDirectory else "Downloads/AeroSync",
                                        fontSize = 10.5.sp,
                                        color = textSecondary,
                                        maxLines = 1,
                                        overflow = TextOverflow.Ellipsis
                                    )
                                }
                                Spacer(modifier = Modifier.width(8.dp))
                                Button(
                                    onClick = onChangeDownloadLocation,
                                    modifier = Modifier.height(32.dp),
                                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 0.dp),
                                    shape = RoundedCornerShape(8.dp),
                                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF3B82F6))
                                ) {
                                    Text("Change", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = Color.White)
                                }
                            }
                        }

                        Surface(
                            modifier = Modifier.fillMaxWidth(),
                            shape = RoundedCornerShape(12.dp),
                            color = cardBgAlt,
                            border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                        ) {
                            Column(modifier = Modifier.padding(12.dp)) {
                                Row(
                                    modifier = Modifier.fillMaxWidth(),
                                    horizontalArrangement = Arrangement.SpaceBetween,
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Text("Device Storage", fontSize = 12.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                    Text("${uiState.freeSpaceText} free", fontSize = 10.5.sp, color = textSecondary)
                                }
                                Spacer(modifier = Modifier.height(6.dp))
                                val usedRatio = (uiState.storageUsedPercent / 100f).coerceIn(0f, 1f)
                                LinearProgressIndicator(
                                    progress = usedRatio,
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .height(6.dp)
                                        .clip(RoundedCornerShape(3.dp)),
                                    color = Color(0xFF3B82F6),
                                    trackColor = if (isDark) Color(0xFF334155) else Color(0xFFE2E8F0)
                                )
                            }
                        }

                        Surface(
                            modifier = Modifier.fillMaxWidth(),
                            shape = RoundedCornerShape(12.dp),
                            color = cardBgAlt,
                            border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                        ) {
                            Row(
                                modifier = Modifier.padding(12.dp),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Column {
                                    Text("Network Connection", fontSize = 12.sp, fontWeight = FontWeight.Bold, color = textPrimary)
                                    Text(
                                        text = "${uiState.connectionTypeLabel} • Port 48124",
                                        fontSize = 11.sp,
                                        color = textSecondary
                                    )
                                }
                                Surface(
                                    shape = RoundedCornerShape(6.dp),
                                    color = Color(0xFF10B981).copy(alpha = 0.15f)
                                ) {
                                    Text(
                                        text = if (uiState.isHotspotConnected) "Hotspot Ready" else "Wi-Fi Ready",
                                        fontSize = 10.5.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = Color(0xFF10B981),
                                        modifier = Modifier.padding(horizontal = 8.dp, vertical = 3.dp)
                                    )
                                }
                            }
                        }
                    }
                }
            }

            // 5. Collapsible Card 2: Nearby Peers
            item {
                CollapsibleCard(
                    title = "Nearby Peers",
                    subtitle = "Connected devices & direct IP",
                    icon = Icons.Default.Devices,
                    iconBgColor = if (isDark) Color(0xFF064E3B).copy(alpha = 0.4f) else Color(0xFFECFDF5),
                    iconTint = Color(0xFF10B981),
                    isExpanded = isPeersExpanded,
                    onToggle = { isPeersExpanded = !isPeersExpanded },
                    cardBg = cardBg,
                    borderColor = borderColor,
                    textPrimary = textPrimary,
                    textSecondary = textSecondary
                ) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(top = 10.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        if (uiState.peers.isEmpty()) {
                            Surface(
                                modifier = Modifier.fillMaxWidth(),
                                shape = RoundedCornerShape(12.dp),
                                color = cardBgAlt,
                                border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                            ) {
                                Column(
                                    modifier = Modifier.padding(14.dp),
                                    horizontalAlignment = Alignment.CenterHorizontally
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.WifiTethering,
                                        contentDescription = null,
                                        tint = Color(0xFF10B981),
                                        modifier = Modifier.size(24.dp)
                                    )
                                    Spacer(modifier = Modifier.height(4.dp))
                                    Text(
                                        text = "Scanning for nearby AeroSync peers...",
                                        fontSize = 11.5.sp,
                                        color = textSecondary,
                                        textAlign = TextAlign.Center
                                    )
                                }
                            }
                        } else {
                            uiState.peers.forEach { peer ->
                                Surface(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .clickable { onSelectPeer(peer) },
                                    shape = RoundedCornerShape(12.dp),
                                    color = cardBgAlt,
                                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                                ) {
                                    Row(
                                        modifier = Modifier.padding(12.dp),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Row(verticalAlignment = Alignment.CenterVertically) {
                                            Box(
                                                modifier = Modifier
                                                    .size(34.dp)
                                                    .clip(CircleShape)
                                                    .background(Color(0xFF10B981).copy(alpha = 0.15f)),
                                                contentAlignment = Alignment.Center
                                            ) {
                                                Icon(
                                                    imageVector = if (peer.deviceType.contains("win", ignoreCase = true)) Icons.Default.Computer else Icons.Default.PhoneAndroid,
                                                    contentDescription = null,
                                                    tint = Color(0xFF10B981),
                                                    modifier = Modifier.size(18.dp)
                                                )
                                            }
                                            Spacer(modifier = Modifier.width(10.dp))
                                            Column {
                                                Text(
                                                    text = peer.deviceName,
                                                    fontSize = 13.sp,
                                                    fontWeight = FontWeight.Bold,
                                                    color = textPrimary
                                                )
                                                Text(
                                                    text = "${peer.ipAddress} • ${peer.deviceType}",
                                                    fontSize = 10.5.sp,
                                                    color = textSecondary
                                                )
                                            }
                                        }

                                        Button(
                                            onClick = {
                                                onSelectPeer(peer)
                                                onPickFiles()
                                            },
                                            modifier = Modifier.height(30.dp),
                                            contentPadding = PaddingValues(horizontal = 10.dp, vertical = 0.dp),
                                            shape = RoundedCornerShape(8.dp),
                                            colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF10B981))
                                        ) {
                                            Text("Send", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = Color.White)
                                        }
                                    }
                                }
                            }
                        }

                        OutlinedButton(
                            onClick = { showDirectIpDialog = true },
                            modifier = Modifier.fillMaxWidth().height(36.dp),
                            shape = RoundedCornerShape(10.dp),
                            colors = ButtonDefaults.outlinedButtonColors(contentColor = Color(0xFF10B981))
                        ) {
                            Icon(Icons.Default.AddLink, contentDescription = null, modifier = Modifier.size(14.dp))
                            Spacer(modifier = Modifier.width(6.dp))
                            Text("Direct IP Connect", fontSize = 11.5.sp, fontWeight = FontWeight.Bold)
                        }
                    }
                }
            }

            // 6. Collapsible Card 3: Recent Transfers
            item {
                CollapsibleCard(
                    title = "Recent Transfers",
                    subtitle = "See your recent and active transfers",
                    icon = Icons.Default.History,
                    iconBgColor = if (isDark) Color(0xFF4C1D95).copy(alpha = 0.4f) else Color(0xFFF5F3FF),
                    iconTint = Color(0xFF8B5CF6),
                    isExpanded = isTransfersExpanded,
                    onToggle = { isTransfersExpanded = !isTransfersExpanded },
                    cardBg = cardBg,
                    borderColor = borderColor,
                    textPrimary = textPrimary,
                    textSecondary = textSecondary
                ) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(top = 10.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        val active = uiState.activeTransfer
                        if (active != null) {
                            Surface(
                                modifier = Modifier.fillMaxWidth(),
                                shape = RoundedCornerShape(12.dp),
                                color = cardBgAlt,
                                border = androidx.compose.foundation.BorderStroke(1.dp, Color(0xFF8B5CF6))
                            ) {
                                Column(modifier = Modifier.padding(12.dp)) {
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Text(
                                            text = active.fileName,
                                            fontSize = 13.sp,
                                            fontWeight = FontWeight.Bold,
                                            color = textPrimary,
                                            maxLines = 1,
                                            overflow = TextOverflow.Ellipsis,
                                            modifier = Modifier.weight(1f)
                                        )
                                        Text(
                                            text = "${"%.1f".format(active.speedMbps)} MB/s",
                                            fontSize = 12.sp,
                                            fontWeight = FontWeight.ExtraBold,
                                            color = Color(0xFF8B5CF6)
                                        )
                                    }
                                    Spacer(modifier = Modifier.height(6.dp))
                                    val prog = if (active.totalBytes > 0) (active.transferredBytes.toFloat() / active.totalBytes.toFloat()).coerceIn(0f, 1f) else 0f
                                    LinearProgressIndicator(
                                        progress = prog,
                                        modifier = Modifier
                                            .fillMaxWidth()
                                            .height(5.dp)
                                            .clip(RoundedCornerShape(3.dp)),
                                        color = Color(0xFF8B5CF6),
                                        trackColor = if (isDark) Color(0xFF334155) else Color(0xFFE2E8F0)
                                    )
                                    Spacer(modifier = Modifier.height(6.dp))
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Text(
                                            text = "${active.transferredBytes / (1024 * 1024)} MB / ${active.totalBytes / (1024 * 1024)} MB",
                                            fontSize = 10.5.sp,
                                            color = textSecondary
                                        )
                                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                            Text(
                                                text = if (active.isPaused) "Resume" else "Pause",
                                                fontSize = 11.sp,
                                                fontWeight = FontWeight.Bold,
                                                color = Color(0xFF3B82F6),
                                                modifier = Modifier.clickable { onTogglePause() }
                                            )
                                            Text(
                                                text = "Cancel",
                                                fontSize = 11.sp,
                                                fontWeight = FontWeight.Bold,
                                                color = Color(0xFFEF4444),
                                                modifier = Modifier.clickable { onCancelTransfer() }
                                            )
                                        }
                                    }
                                }
                            }
                        }

                        if (uiState.history.isEmpty() && active == null) {
                            Text(
                                text = "No recent transfers logged yet.",
                                fontSize = 11.5.sp,
                                color = textSecondary,
                                modifier = Modifier.padding(vertical = 4.dp)
                            )
                        } else {
                            uiState.history.take(3).forEach { item ->
                                Surface(
                                    modifier = Modifier.fillMaxWidth(),
                                    shape = RoundedCornerShape(10.dp),
                                    color = cardBgAlt,
                                    border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                                ) {
                                    Row(
                                        modifier = Modifier.padding(10.dp),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically
                                    ) {
                                        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.weight(1f)) {
                                            Icon(
                                                imageVector = if (!item.isReceived) Icons.Default.ArrowOutward else Icons.Default.SouthWest,
                                                contentDescription = null,
                                                tint = if (!item.isReceived) Color(0xFF3B82F6) else Color(0xFF10B981),
                                                modifier = Modifier.size(16.dp)
                                            )
                                            Spacer(modifier = Modifier.width(8.dp))
                                            Column {
                                                Text(
                                                    text = item.fileName,
                                                    fontSize = 12.sp,
                                                    fontWeight = FontWeight.SemiBold,
                                                    color = textPrimary,
                                                    maxLines = 1,
                                                    overflow = TextOverflow.Ellipsis
                                                )
                                                Text(
                                                    text = "${item.fileSize / (1024 * 1024)} MB • ${dateFormat.format(Date(item.timestamp))}",
                                                    fontSize = 10.sp,
                                                    color = textSecondary
                                                )
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        TextButton(
                            onClick = { onSelectTab(2) },
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(
                                text = "View All Activity History →",
                                fontSize = 11.5.sp,
                                fontWeight = FontWeight.Bold,
                                color = Color(0xFF8B5CF6)
                            )
                        }
                    }
                }
            }

            // 7. Security Banner
            item {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(14.dp)),
                    color = if (isDark) Color(0xFF064E3B).copy(alpha = 0.2f) else Color(0xFFECFDF5),
                    border = androidx.compose.foundation.BorderStroke(
                        1.dp,
                        if (isDark) Color(0xFF065F46) else Color(0xFFA7F3D0)
                    )
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 14.dp, vertical = 10.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.weight(1f)
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(32.dp)
                                    .clip(CircleShape)
                                    .background(Color(0xFF10B981).copy(alpha = 0.2f)),
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(
                                    imageVector = Icons.Default.VerifiedUser,
                                    contentDescription = "Secure",
                                    tint = Color(0xFF10B981),
                                    modifier = Modifier.size(17.dp)
                                )
                            }
                            Spacer(modifier = Modifier.width(10.dp))
                            Column {
                                Text(
                                    text = "Secure  •  Private  •  Local Transfer",
                                    fontSize = 12.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = if (isDark) Color(0xFFE2E8F0) else Color(0xFF065F46)
                                )
                                Text(
                                    text = "Your data never leaves your network.",
                                    fontSize = 10.5.sp,
                                    color = if (isDark) Color(0xFF94A3B8) else Color(0xFF047857)
                                )
                            }
                        }

                        Surface(
                            shape = RoundedCornerShape(8.dp),
                            color = if (isDark) Color(0xFF3B0764) else Color(0xFFF3E8FF)
                        ) {
                            Box(modifier = Modifier.padding(6.dp)) {
                                Icon(
                                    imageVector = Icons.Default.Lock,
                                    contentDescription = "Encrypted",
                                    tint = Color(0xFF8B5CF6),
                                    modifier = Modifier.size(15.dp)
                                )
                            }
                        }
                    }
                }
                Spacer(modifier = Modifier.height(10.dp))
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
                        Text("Connect & Send", color = Color.White)
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

        // Incoming Pairing Request Dialog
        val pairing = uiState.incomingPairingPrompt
        if (pairing != null) {
            AlertDialog(
                onDismissRequest = { onRespondPairing(false) },
                title = { Text("Connection Request", fontWeight = FontWeight.Bold, color = textPrimary) },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        Text("Device '${pairing.senderName}' wants to pair with you.", fontSize = 13.sp, color = textPrimary)
                        Text("PIN: ${pairing.pin}", fontSize = 16.sp, fontWeight = FontWeight.ExtraBold, color = Color(0xFF2563EB))
                    }
                },
                confirmButton = {
                    Button(
                        onClick = { onRespondPairing(true) },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF10B981))
                    ) {
                        Text("Accept", color = Color.White)
                    }
                },
                dismissButton = {
                    TextButton(onClick = { onRespondPairing(false) }) {
                        Text("Decline", color = Color(0xFFEF4444))
                    }
                },
                containerColor = cardBg
            )
        }
    }
}

@Composable
private fun CollapsibleCard(
    title: String,
    subtitle: String,
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    iconBgColor: Color,
    iconTint: Color,
    isExpanded: Boolean,
    onToggle: () -> Unit,
    cardBg: Color,
    borderColor: Color,
    textPrimary: Color,
    textSecondary: Color,
    content: @Composable () -> Unit
) {
    val rotation by animateFloatAsState(if (isExpanded) 90f else 0f, label = "chevronRotation")

    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp)),
        color = cardBg,
        border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(14.dp)
        ) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable(
                        interactionSource = remember { MutableInteractionSource() },
                        indication = null
                    ) { onToggle() },
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.weight(1f)
                ) {
                    Box(
                        modifier = Modifier
                            .size(38.dp)
                            .clip(CircleShape)
                            .background(iconBgColor),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = icon,
                            contentDescription = title,
                            tint = iconTint,
                            modifier = Modifier.size(19.dp)
                        )
                    }
                    Spacer(modifier = Modifier.width(12.dp))
                    Column {
                        Text(
                            text = title,
                            fontSize = 13.5.sp,
                            fontWeight = FontWeight.Bold,
                            color = textPrimary
                        )
                        Text(
                            text = subtitle,
                            fontSize = 11.sp,
                            color = textSecondary
                        )
                    }
                }

                Icon(
                    imageVector = Icons.Default.ChevronRight,
                    contentDescription = if (isExpanded) "Collapse" else "Expand",
                    tint = textSecondary,
                    modifier = Modifier
                        .size(20.dp)
                        .rotate(rotation)
                )
            }

            AnimatedVisibility(
                visible = isExpanded,
                enter = fadeIn() + expandVertically(),
                exit = fadeOut() + shrinkVertically()
            ) {
                content()
            }
        }
    }
}
