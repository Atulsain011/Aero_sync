package com.aerosync.app.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.aerosync.app.data.preferences.ThemeMode
import com.aerosync.app.ui.theme.AeroBrandGradient

@Composable
fun AeroSyncTopAppBar(
    title: String = "AeroSync",
    selectedTab: Int,
    themeMode: ThemeMode,
    isDark: Boolean,
    onSelectTab: (Int) -> Unit,
    onToggleTheme: () -> Unit,
    rightActionContent: (@Composable () -> Unit)? = null
) {
    val borderColor = if (isDark) Color(0xFF374151) else Color(0xFFE2E8F0)
    val textPrimary = if (isDark) Color(0xFFF9FAFB) else Color(0xFF0F172A)
    val textSecondary = if (isDark) Color(0xFF9CA3AF) else Color(0xFF64748B)

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Left: Project Logo + Brand (with flexible shrink)
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .weight(1f, fill = false)
                .padding(end = 6.dp)
        ) {
            AeroSyncLogoIcon(size = 28.dp)
            Spacer(modifier = Modifier.width(6.dp))
            Text(
                text = title,
                fontSize = 17.sp,
                fontWeight = FontWeight.ExtraBold,
                color = textPrimary,
                letterSpacing = (-0.3).sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }

        // Center: Floating Pill Segmented Navigation (Files, Devices, Activity)
        Surface(
            modifier = Modifier
                .clip(RoundedCornerShape(24.dp))
                .border(1.dp, borderColor, RoundedCornerShape(24.dp)),
            color = if (isDark) Color(0xFF1E293B) else Color(0xFFFFFFFF),
            shadowElevation = if (isDark) 0.dp else 2.dp
        ) {
            Row(
                modifier = Modifier.padding(2.5.dp),
                horizontalArrangement = Arrangement.spacedBy(1.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                val tabs = listOf(
                    Triple("Files", Icons.Default.Description, 0),
                    Triple("Devices", Icons.Default.Devices, 1),
                    Triple("Activity", Icons.Default.ShowChart, 2)
                )

                tabs.forEach { (name, icon, tabIndex) ->
                    val isSelected = selectedTab == tabIndex
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(18.dp))
                            .then(
                                if (isSelected) Modifier.background(AeroBrandGradient)
                                else Modifier.background(Color.Transparent)
                            )
                            .clickable { onSelectTab(tabIndex) }
                            .padding(horizontal = 7.dp, vertical = 4.5.dp),
                        contentAlignment = Alignment.Center
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(3.dp)
                        ) {
                            Icon(
                                imageVector = icon,
                                contentDescription = name,
                                tint = if (isSelected) Color.White else textSecondary,
                                modifier = Modifier.size(12.dp)
                            )
                            Text(
                                text = name,
                                fontSize = 11.sp,
                                fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                                color = if (isSelected) Color.White else textSecondary,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis
                            )
                        }
                    }
                }
            }
        }

        Spacer(modifier = Modifier.width(6.dp))

        // Right Action: Custom Action or Theme Toggle
        if (rightActionContent != null) {
            rightActionContent()
        } else {
            Surface(
                modifier = Modifier
                    .size(34.dp)
                    .clip(CircleShape)
                    .border(1.dp, borderColor, CircleShape)
                    .clickable { onToggleTheme() },
                color = if (isDark) Color(0xFF1E293B) else Color(0xFFFFFFFF),
                shadowElevation = if (isDark) 0.dp else 2.dp
            ) {
                Box(contentAlignment = Alignment.Center) {
                    Icon(
                        imageVector = when (themeMode) {
                            ThemeMode.LIGHT -> Icons.Default.WbSunny
                            ThemeMode.DARK -> Icons.Default.DarkMode
                        },
                        contentDescription = "Toggle Theme",
                        tint = if (isDark) Color(0xFFFBBF24) else Color(0xFF6366F1),
                        modifier = Modifier.size(16.dp)
                    )
                }
            }
        }
    }
}
