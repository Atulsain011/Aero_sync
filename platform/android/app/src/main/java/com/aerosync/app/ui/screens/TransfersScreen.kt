package com.aerosync.app.ui.screens

import android.content.Intent
import android.net.Uri
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.aerosync.app.ui.components.AeroSyncLogoIcon
import com.aerosync.app.viewmodel.AeroSyncUiState
import com.aerosync.app.viewmodel.QueueItemStatus
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun TransfersScreen(
    uiState: AeroSyncUiState,
    onTogglePause: () -> Unit,
    onCancelTransfer: () -> Unit,
    onSelectTab: (Int) -> Unit,
    onRemoveQueueItem: (String) -> Unit = {},
    onClearHistory: () -> Unit = {},
    onChangeDownloadLocation: () -> Unit = {}
) {
    val context = LocalContext.current
    var showClearHistoryDialog by remember { mutableStateOf(false) }

    val isDark = uiState.isDarkTheme
    val bgColor = if (isDark) Color(0xFF090D16) else Color(0xFFF8FAFC)
    val cardBg = if (isDark) Color(0xFF111827) else Color(0xFFFFFFFF)
    val cardBgAlt = if (isDark) Color(0xFF1F2937) else Color(0xFFF8FAFC)
    val borderColor = if (isDark) Color(0xFF374151) else Color(0xFFE2E8F0)
    val textPrimary = if (isDark) Color(0xFFF9FAFB) else Color(0xFF0F172A)
    val textSecondary = if (isDark) Color(0xFF9CA3AF) else Color(0xFF64748B)
    val textMuted = if (isDark) Color(0xFF6B7280) else Color(0xFF94A3B8)

    val primaryGradient = Brush.horizontalGradient(
        listOf(Color(0xFF2563EB), Color(0xFF7C3AED))
    )

    val dateFormat = SimpleDateFormat("MMM d, HH:mm", Locale.getDefault())

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(bgColor)
            .statusBarsPadding()
            .navigationBarsPadding()
            .padding(horizontal = 16.dp, vertical = 6.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // 1. Top App Bar: Brand + Segmented Navigation Tabs + Clear Action
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 4.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Left: Official Project Logo + Brand Text
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.padding(end = 4.dp)
                ) {
                    AeroSyncLogoIcon(size = 32.dp)
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = "Activity",
                        fontSize = 20.sp,
                        fontWeight = FontWeight.ExtraBold,
                        color = if (isDark) Color(0xFFF1F5F9) else Color(0xFF0F172A),
                        letterSpacing = (-0.3).sp
                    )
                }

                // Center: Floating Pill Segmented Navigation (Files, Devices, Activity)
                Surface(
                    modifier = Modifier
                        .clip(RoundedCornerShape(24.dp))
                        .border(1.dp, borderColor, RoundedCornerShape(24.dp)),
                    color = if (isDark) Color(0xFF1E293B) else Color(0xFFFFFFFF),
                    shadowElevation = 2.dp
                ) {
                    Row(
                        modifier = Modifier.padding(3.dp),
                        horizontalArrangement = Arrangement.spacedBy(2.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        val tabs = listOf(
                            Triple("Files", Icons.Default.Description, 0),
                            Triple("Devices", Icons.Default.Devices, 1),
                            Triple("Activity", Icons.Default.ShowChart, 2)
                        )

                        tabs.forEach { (name, icon, tabIndex) ->
                            val isSelected = tabIndex == 2 // Activity selected
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(20.dp))
                                    .then(
                                        if (isSelected) Modifier.background(primaryGradient)
                                        else Modifier.background(Color.Transparent)
                                    )
                                    .clickable { onSelectTab(tabIndex) }
                                    .padding(horizontal = 10.dp, vertical = 6.dp),
                                contentAlignment = Alignment.Center
                            ) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(4.dp)
                                ) {
                                    Icon(
                                        imageVector = icon,
                                        contentDescription = name,
                                        tint = if (isSelected) Color.White else textSecondary,
                                        modifier = Modifier.size(13.dp)
                                    )
                                    Text(
                                        text = name,
                                        fontSize = 11.5.sp,
                                        fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                                        color = if (isSelected) Color.White else textSecondary
                                    )
                                }
                            }
                        }
                    }
                }

                // Right: Clear History Action Button
                Surface(
                    modifier = Modifier
                        .size(36.dp)
                        .clip(CircleShape)
                        .border(1.dp, borderColor, CircleShape)
                        .clickable { showClearHistoryDialog = true },
                    color = if (isDark) Color(0xFF1E293B) else Color(0xFFFFFFFF),
                    shadowElevation = 2.dp
                ) {
                    Box(contentAlignment = Alignment.Center) {
                        Icon(
                            imageVector = Icons.Default.DeleteSweep,
                            contentDescription = "Clear History",
                            tint = Color(0xFFEF4444),
                            modifier = Modifier.size(18.dp)
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.height(10.dp))

            // 2. Download Location Quick Card
            Surface(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(14.dp)),
                color = cardBg,
                border = androidx.compose.foundation.BorderStroke(1.dp, borderColor),
                shadowElevation = if (isDark) 0.dp else 1.dp
            ) {
                Row(
                    modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        modifier = Modifier.weight(1f),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(Icons.Default.Folder, contentDescription = null, tint = Color(0xFF3B82F6), modifier = Modifier.size(16.dp))
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = if (uiState.downloadDirectory.isNotBlank()) uiState.downloadDirectory else "Downloads/AeroSync",
                            fontSize = 11.sp,
                            color = textSecondary,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                    Text(
                        text = "Change",
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color(0xFF3B82F6),
                        modifier = Modifier
                            .clip(RoundedCornerShape(6.dp))
                            .clickable { onChangeDownloadLocation() }
                            .padding(4.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(10.dp))

            // 3. Active Transfer Telemetry Card (if transferring or paused)
            val active = uiState.activeTransfer
            if (active != null) {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(bottom = 12.dp)
                        .clip(RoundedCornerShape(16.dp)),
                    color = cardBg,
                    border = androidx.compose.foundation.BorderStroke(1.dp, Color(0xFF8B5CF6)),
                    shadowElevation = if (isDark) 0.dp else 2.dp
                ) {
                    Column(modifier = Modifier.padding(14.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    text = active.fileName,
                                    fontSize = 14.5.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = textPrimary,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis
                                )
                                Spacer(modifier = Modifier.height(2.dp))
                                Text(
                                    text = "${active.transferredBytes / (1024 * 1024)} MB / ${active.totalBytes / (1024 * 1024)} MB",
                                    fontSize = 11.5.sp,
                                    color = textSecondary
                                )
                            }
                            val percent = if (active.totalBytes > 0) ((active.transferredBytes * 100) / active.totalBytes).toInt() else 0
                            Surface(
                                shape = RoundedCornerShape(8.dp),
                                color = Color(0xFF8B5CF6).copy(alpha = 0.15f),
                                border = androidx.compose.foundation.BorderStroke(1.dp, Color(0xFF8B5CF6))
                            ) {
                                Text(
                                    text = "$percent%",
                                    fontSize = 13.sp,
                                    fontWeight = FontWeight.ExtraBold,
                                    color = Color(0xFF8B5CF6),
                                    modifier = Modifier.padding(horizontal = 8.dp, vertical = 2.dp)
                                )
                            }
                        }

                        Spacer(modifier = Modifier.height(10.dp))

                        val progress = if (active.totalBytes > 0) (active.transferredBytes.toFloat() / active.totalBytes.toFloat()).coerceIn(0f, 1f) else 0f
                        LinearProgressIndicator(
                            progress = progress,
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(6.dp)
                                .clip(RoundedCornerShape(3.dp)),
                            color = Color(0xFF8B5CF6),
                            trackColor = if (isDark) Color(0xFF334155) else Color(0xFFE2E8F0)
                        )

                        Spacer(modifier = Modifier.height(10.dp))

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Icon(
                                    imageVector = Icons.Default.Speed,
                                    contentDescription = null,
                                    tint = Color(0xFF10B981),
                                    modifier = Modifier.size(16.dp)
                                )
                                Spacer(modifier = Modifier.width(4.dp))
                                Text(
                                    text = "${"%.1f".format(active.speedMbps)} MB/s",
                                    fontSize = 12.5.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = Color(0xFF10B981)
                                )
                                if (active.etaSeconds > 0) {
                                    Spacer(modifier = Modifier.width(8.dp))
                                    Text(
                                        text = "• ETA: ${active.etaSeconds}s",
                                        fontSize = 10.5.sp,
                                        color = textMuted
                                    )
                                }
                            }

                            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                FilledTonalIconButton(
                                    onClick = onTogglePause,
                                    modifier = Modifier.size(32.dp),
                                    colors = IconButtonDefaults.filledTonalIconButtonColors(
                                        containerColor = if (isDark) Color(0xFF334155) else Color(0xFFF1F5F9)
                                    )
                                ) {
                                    Icon(
                                        imageVector = if (active.isPaused) Icons.Default.PlayArrow else Icons.Default.Pause,
                                        contentDescription = if (active.isPaused) "Resume" else "Pause",
                                        tint = textPrimary,
                                        modifier = Modifier.size(15.dp)
                                    )
                                }
                                FilledTonalIconButton(
                                    onClick = onCancelTransfer,
                                    modifier = Modifier.size(32.dp),
                                    colors = IconButtonDefaults.filledTonalIconButtonColors(
                                        containerColor = Color(0xFFEF4444).copy(alpha = 0.15f)
                                    )
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.Close,
                                        contentDescription = "Cancel",
                                        tint = Color(0xFFEF4444),
                                        modifier = Modifier.size(15.dp)
                                    )
                                }
                            }
                        }
                    }
                }
            }

            // 4. Section Title with Clear History Action
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 4.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "Transfer Queue & Activity (${uiState.transferQueue.size + uiState.history.size})",
                    fontSize = 13.5.sp,
                    fontWeight = FontWeight.Bold,
                    color = textPrimary
                )

                if (uiState.history.isNotEmpty()) {
                    Surface(
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .clickable { showClearHistoryDialog = true },
                        color = Color(0xFFEF4444).copy(alpha = 0.12f),
                        border = androidx.compose.foundation.BorderStroke(1.dp, Color(0xFFEF4444).copy(alpha = 0.25f))
                    ) {
                        Row(
                            modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(4.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Default.DeleteSweep,
                                contentDescription = "Clear History",
                                tint = Color(0xFFEF4444),
                                modifier = Modifier.size(13.dp)
                            )
                            Text(
                                text = "Clear History",
                                fontSize = 11.sp,
                                fontWeight = FontWeight.Bold,
                                color = Color(0xFFEF4444)
                            )
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(6.dp))

            // 5. Scrollable List: Queued Items + History Records
            LazyColumn(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                // Pending & Paused Queue items
                val activeQueue = uiState.transferQueue.filter {
                    it.status == QueueItemStatus.QUEUED || it.status == QueueItemStatus.PAUSED || it.status == QueueItemStatus.FAILED
                }
                items(activeQueue, key = { it.id }) { item ->
                    Surface(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(12.dp)),
                        color = cardBg,
                        border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                    ) {
                        Row(
                            modifier = Modifier.padding(10.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Icon(
                                imageVector = when (item.status) {
                                    QueueItemStatus.PAUSED -> Icons.Default.PauseCircle
                                    QueueItemStatus.FAILED -> Icons.Default.Error
                                    else -> Icons.Default.Schedule
                                },
                                contentDescription = null,
                                tint = when (item.status) {
                                    QueueItemStatus.PAUSED -> Color(0xFFF59E0B)
                                    QueueItemStatus.FAILED -> Color(0xFFEF4444)
                                    else -> textSecondary
                                },
                                modifier = Modifier.size(20.dp)
                            )
                            Spacer(modifier = Modifier.width(10.dp))
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    text = item.fileName,
                                    fontSize = 12.5.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = textPrimary,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis
                                )
                                Text(
                                    text = "${item.fileSize / (1024 * 1024)} MB • ${item.status.name}",
                                    fontSize = 10.5.sp,
                                    color = textMuted
                                )
                            }
                            IconButton(onClick = { onRemoveQueueItem(item.id) }, modifier = Modifier.size(26.dp)) {
                                Icon(
                                    imageVector = Icons.Default.Delete,
                                    contentDescription = "Remove",
                                    tint = Color(0xFFEF4444),
                                    modifier = Modifier.size(15.dp)
                                )
                            }
                        }
                    }
                }

                // Completed History items
                items(uiState.history, key = { it.id }) { item ->
                    Surface(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(12.dp))
                            .clickable {
                                try {
                                    val file = File(item.filePath)
                                    if (file.exists()) {
                                        val uri = Uri.fromFile(file)
                                        val viewIntent = Intent(Intent.ACTION_VIEW).apply {
                                            setDataAndType(uri, "*/*")
                                            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_GRANT_READ_URI_PERMISSION
                                        }
                                        context.startActivity(viewIntent)
                                    }
                                } catch (_: Exception) {}
                            },
                        color = cardBg,
                        border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                    ) {
                        Row(
                            modifier = Modifier.padding(10.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Icon(
                                imageVector = if (!item.isReceived) Icons.Default.ArrowOutward else Icons.Default.SouthWest,
                                contentDescription = null,
                                tint = if (!item.isReceived) Color(0xFF3B82F6) else Color(0xFF10B981),
                                modifier = Modifier.size(20.dp)
                            )
                            Spacer(modifier = Modifier.width(10.dp))
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    text = item.fileName,
                                    fontSize = 12.5.sp,
                                    fontWeight = FontWeight.SemiBold,
                                    color = textPrimary,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis
                                )
                                Text(
                                    text = "${item.fileSize / (1024 * 1024)} MB • ${item.peerName} • ${dateFormat.format(Date(item.timestamp))}",
                                    fontSize = 10.5.sp,
                                    color = textMuted
                                )
                            }
                            Surface(
                                shape = RoundedCornerShape(6.dp),
                                color = Color(0xFF10B981).copy(alpha = 0.12f)
                            ) {
                                Row(
                                    modifier = Modifier.padding(horizontal = 6.dp, vertical = 2.dp),
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.CheckCircle,
                                        contentDescription = null,
                                        tint = Color(0xFF10B981),
                                        modifier = Modifier.size(11.dp)
                                    )
                                    Spacer(modifier = Modifier.width(3.dp))
                                    Text(
                                        text = "Verified",
                                        fontSize = 9.5.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = Color(0xFF10B981)
                                    )
                                }
                            }
                        }
                    }
                }

                // Empty State if no history or queue
                if (uiState.history.isEmpty() && activeQueue.isEmpty()) {
                    item {
                        Surface(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(vertical = 20.dp),
                            shape = RoundedCornerShape(14.dp),
                            color = cardBgAlt,
                            border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                        ) {
                            Column(
                                modifier = Modifier.padding(20.dp),
                                horizontalAlignment = Alignment.CenterHorizontally
                            ) {
                                Icon(
                                    imageVector = Icons.Default.HourglassEmpty,
                                    contentDescription = null,
                                    tint = textMuted,
                                    modifier = Modifier.size(28.dp)
                                )
                                Spacer(modifier = Modifier.height(8.dp))
                                Text(
                                    text = "No transfer history yet",
                                    fontSize = 13.sp,
                                    fontWeight = FontWeight.Bold,
                                    color = textPrimary
                                )
                                Spacer(modifier = Modifier.height(2.dp))
                                Text(
                                    text = "Files sent and received with AeroSync will be logged here.",
                                    fontSize = 11.sp,
                                    color = textSecondary
                                )
                            }
                        }
                    }
                }
            }
        }

        // 6. Confirmation Alert Dialog for Clearing History
        if (showClearHistoryDialog) {
            AlertDialog(
                onDismissRequest = { showClearHistoryDialog = false },
                title = {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = Icons.Default.Warning,
                            contentDescription = null,
                            tint = Color(0xFFEF4444),
                            modifier = Modifier.size(20.dp)
                        )
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = "Clear Transfer History?",
                            fontWeight = FontWeight.Bold,
                            fontSize = 16.sp,
                            color = textPrimary
                        )
                    }
                },
                text = {
                    Text(
                        text = "Are you sure you want to permanently clear all completed transfer records from history? This action cannot be undone.",
                        fontSize = 12.5.sp,
                        color = textSecondary
                    )
                },
                confirmButton = {
                    Button(
                        onClick = {
                            onClearHistory()
                            showClearHistoryDialog = false
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFEF4444))
                    ) {
                        Text("Clear", fontWeight = FontWeight.Bold, color = Color.White)
                    }
                },
                dismissButton = {
                    TextButton(onClick = { showClearHistoryDialog = false }) {
                        Text("Cancel", color = textSecondary)
                    }
                },
                containerColor = cardBg
            )
        }
    }
}
