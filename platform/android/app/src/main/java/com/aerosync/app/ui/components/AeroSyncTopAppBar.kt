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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.aerosync.app.data.preferences.ThemeMode
import com.aerosync.app.ui.theme.AeroBrandGradient

import androidx.compose.ui.text.style.TextOverflow

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
        // Left: Project Logo + Brand
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.padding(end = 4.dp)
        ) {
            AeroSyncLogoIcon(size = 30.dp)
            Spacer(modifier = Modifier.width(6.dp))
            Text(
                text = title,
                fontSize = 18.sp,
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
                    val isSelected = selectedTab == tabIndex
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(20.dp))
                            .then(
                                if (isSelected) Modifier.background(AeroBrandGradient)
                                else Modifier.background(Color.Transparent)
                            )
                            .clickable { onSelectTab(tabIndex) }
                            .padding(horizontal = 8.dp, vertical = 5.dp),
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

        // Right Action: Custom Action or Theme Toggle
        if (rightActionContent != null) {
            rightActionContent()
        } else {
            Surface(
                modifier = Modifier
                    .size(36.dp)
                    .clip(CircleShape)
                    .border(1.dp, borderColor, CircleShape)
                    .clickable { onToggleTheme() },
                color = if (isDark) Color(0xFF1E293B) else Color(0xFFFFFFFF),
                shadowElevation = 2.dp
            ) {
                Box(contentAlignment = Alignment.Center) {
                    Icon(
                        imageVector = when (themeMode) {
                            ThemeMode.LIGHT -> Icons.Default.WbSunny
                            ThemeMode.DARK -> Icons.Default.DarkMode
                            ThemeMode.SYSTEM -> Icons.Default.BrightnessAuto
                        },
                        contentDescription = "Toggle Theme",
                        tint = if (isDark) Color(0xFFFBBF24) else Color(0xFF6366F1),
                        modifier = Modifier.size(17.dp)
                    )
                }
            }
        }
    }
}
