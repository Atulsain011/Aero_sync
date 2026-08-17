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
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.aerosync.app.ui.components.AeroSyncLogoIcon
import com.aerosync.app.viewmodel.AeroSyncUiState
import com.aerosync.app.viewmodel.QueueItemStatus
import java.text.DecimalFormat
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
    var selectedFilterTab by remember { mutableStateOf(0) } // 0: All, 1: Active, 2: Completed

    val isDark = uiState.isDarkTheme
    val bgColor = if (isDark) Color(0xFF090D16) else Color(0xFFF8FAFC)
    val cardBg = if (isDark) Color(0xFF111827) else Color(0xFFFFFFFF)
    val cardBgAlt = if (isDark) Color(0xFF1F2937) else Color(0xFFF1F5F9)
    val borderColor = if (isDark) Color(0xFF374151) else Color(0xFFE2E8F0)
    val textPrimary = if (isDark) Color(0xFFF9FAFB) else Color(0xFF0F172A)
    val textSecondary = if (isDark) Color(0xFF9CA3AF) else Color(0xFF64748B)
    val textMuted = if (isDark) Color(0xFF6B7280) else Color(0xFF94A3B8)
    val brandBlue = Color(0xFF2563EB)
    val brandGreen = Color(0xFF059669)

    val dateFormat = SimpleDateFormat("MMM d, HH:mm", Locale.getDefault())
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

                    Row(
                        modifier = Modifier
                            .clip(RoundedCornerShape(24.dp))
                            .background(cardBg)
                            .border(1.dp, borderColor, RoundedCornerShape(24.dp))
                            .padding(3.dp),
                        horizontalArrangement = Arrangement.spacedBy(2.dp)
                    ) {
                        listOf("Files", "Devices", "Activity").forEachIndexed { index, tab ->
                            val isSelected = index == 2
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
                }
            }

            // Active Transfer Stream Card (If active)
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
                                    Column {
                                        Text("ACTIVE STREAM", fontSize = 10.sp, fontWeight = FontWeight.Bold, color = brandBlue)
                                        Text(
                                            text = activeName,
                                            fontSize = 13.sp,
                                            fontWeight = FontWeight.Bold,
                                            color = textPrimary,
                                            maxLines = 1,
                                            overflow = TextOverflow.Ellipsis
                                        )
                                    }
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

            // Filter Tabs
            item {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        modifier = Modifier
                            .clip(RoundedCornerShape(16.dp))
                            .background(cardBgAlt)
                            .padding(2.dp)
                    ) {
                        listOf("All", "Active", "Completed").forEachIndexed { index, label ->
                            val isSelected = selectedFilterTab == index
                            Surface(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(14.dp))
                                    .clickable { selectedFilterTab = index },
                                color = if (isSelected) cardBg else Color.Transparent
                            ) {
                                Text(
                                    text = label,
                                    fontSize = 11.sp,
                                    fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                                    color = if (isSelected) brandBlue else textSecondary,
                                    modifier = Modifier.padding(horizontal = 10.dp, vertical = 4.dp)
                                )
                            }
                        }
                    }

                    if (uiState.history.isNotEmpty()) {
                        IconButton(onClick = onClearHistory, modifier = Modifier.size(28.dp)) {
                            Icon(Icons.Default.Delete, contentDescription = "Clear History", tint = textMuted, modifier = Modifier.size(16.dp))
                        }
                    }
                }
            }

            // History & Log List
            val filteredHistory = uiState.history.filter {
                when (selectedFilterTab) {
                    1 -> false
                    2 -> it.status.equals("COMPLETED", ignoreCase = true)
                    else -> true
                }
            }

            if (filteredHistory.isEmpty() && !uiState.isTransferring) {
                item {
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
                                .padding(32.dp),
                            horizontalAlignment = Alignment.CenterHorizontally
                        ) {
                            Surface(
                                shape = CircleShape,
                                color = Color(0xFFEFF6FF),
                                modifier = Modifier.size(48.dp)
                            ) {
                                Box(contentAlignment = Alignment.Center) {
                                    Icon(Icons.Default.History, contentDescription = null, tint = brandBlue, modifier = Modifier.size(24.dp))
                                }
                            }
                            Spacer(modifier = Modifier.height(12.dp))
                            Text(
                                text = "No transfer history yet",
                                fontSize = 14.sp,
                                fontWeight = FontWeight.Bold,
                                color = textPrimary
                            )
                            Spacer(modifier = Modifier.height(4.dp))
                            Text(
                                text = "Files you send or receive across devices will appear here.",
                                fontSize = 11.sp,
                                color = textSecondary
                            )
                        }
                    }
                }
            } else {
                items(filteredHistory.size) { index ->
                    val historyItem = filteredHistory[index]
                    Surface(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(14.dp)),
                        color = cardBg,
                        border = androidx.compose.foundation.BorderStroke(1.dp, borderColor)
                    ) {
                        Row(
                            modifier = Modifier.padding(12.dp),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Row(
                                verticalAlignment = Alignment.CenterVertically,
                                modifier = Modifier.weight(1f)
                            ) {
                                Surface(
                                    shape = CircleShape,
                                    color = if (historyItem.isReceived) Color(0xFFECFDF5) else Color(0xFFEFF6FF),
                                    modifier = Modifier.size(34.dp)
                                ) {
                                    Box(contentAlignment = Alignment.Center) {
                                        Icon(
                                            imageVector = if (historyItem.isReceived) Icons.Default.ArrowDownward else Icons.Default.ArrowUpward,
                                            contentDescription = null,
                                            tint = if (historyItem.isReceived) brandGreen else brandBlue,
                                            modifier = Modifier.size(16.dp)
                                        )
                                    }
                                }
                                Spacer(modifier = Modifier.width(10.dp))
                                Column {
                                    Text(
                                        text = historyItem.fileName,
                                        fontSize = 13.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = textPrimary,
                                        maxLines = 1,
                                        overflow = TextOverflow.Ellipsis
                                    )
                                    Text(
                                        text = "${formatBytes(historyItem.fileSize)} • ${dateFormat.format(Date(historyItem.timestamp))} • ${historyItem.peerName}",
                                        fontSize = 10.sp,
                                        color = textMuted
                                    )
                                }
                            }

                            Text("Completed", fontSize = 11.sp, fontWeight = FontWeight.Bold, color = brandGreen)
                        }
                    }
                }
            }
        }
    }
}
