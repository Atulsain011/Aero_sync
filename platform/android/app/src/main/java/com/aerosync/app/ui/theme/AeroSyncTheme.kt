package com.aerosync.app.ui.theme

import android.app.Activity
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat
import com.aerosync.app.data.preferences.ThemeMode

// Theme Colors
val AeroDarkBackground = Color(0xFF090D16)
val AeroDarkCard = Color(0xFF111827)
val AeroDarkCardAlt = Color(0xFF1F2937)
val AeroDarkBorder = Color(0xFF374151)
val AeroDarkTextPrimary = Color(0xFFF9FAFB)
val AeroDarkTextSecondary = Color(0xFF9CA3AF)
val AeroDarkTextMuted = Color(0xFF6B7280)

val AeroLightBackground = Color(0xFFF8FAFC)
val AeroLightCard = Color(0xFFFFFFFF)
val AeroLightCardAlt = Color(0xFFF1F5F9)
val AeroLightBorder = Color(0xFFE2E8F0)
val AeroLightTextPrimary = Color(0xFF0F172A)
val AeroLightTextSecondary = Color(0xFF64748B)
val AeroLightTextMuted = Color(0xFF94A3B8)

val AeroPrimaryBlue = Color(0xFF2563EB)
val AeroPrimaryPurple = Color(0xFF7C3AED)
val AeroAccentSky = Color(0xFF38BDF8)
val AeroSuccessGreen = Color(0xFF10B981)
val AeroWarningAmber = Color(0xFFF59E0B)
val AeroDangerRed = Color(0xFFEF4444)

val AeroBrandGradient = Brush.horizontalGradient(
    listOf(AeroPrimaryBlue, AeroPrimaryPurple)
)

private val DarkColorScheme = darkColorScheme(
    primary = AeroPrimaryBlue,
    secondary = AeroPrimaryPurple,
    tertiary = AeroAccentSky,
    background = AeroDarkBackground,
    surface = AeroDarkCard,
    onPrimary = Color.White,
    onSecondary = Color.White,
    onBackground = AeroDarkTextPrimary,
    onSurface = AeroDarkTextPrimary
)

private val LightColorScheme = lightColorScheme(
    primary = AeroPrimaryBlue,
    secondary = AeroPrimaryPurple,
    tertiary = AeroAccentSky,
    background = AeroLightBackground,
    surface = AeroLightCard,
    onPrimary = Color.White,
    onSecondary = Color.White,
    onBackground = AeroLightTextPrimary,
    onSurface = AeroLightTextPrimary
)

@Composable
fun AeroSyncTheme(
    themeMode: ThemeMode = ThemeMode.DARK,
    content: @Composable () -> Unit
) {
    val isDark = when (themeMode) {
        ThemeMode.SYSTEM -> isSystemInDarkTheme()
        ThemeMode.DARK -> true
        ThemeMode.LIGHT -> false
    }

    val colorScheme = if (isDark) DarkColorScheme else LightColorScheme
    val view = LocalView.current

    if (!view.isInEditMode) {
        SideEffect {
            val window = (view.context as? Activity)?.window ?: return@SideEffect
            window.statusBarColor = (if (isDark) AeroDarkBackground else AeroLightBackground).toArgb()
            window.navigationBarColor = (if (isDark) AeroDarkBackground else AeroLightBackground).toArgb()
            val insetsController = WindowCompat.getInsetsController(window, view)
            insetsController.isAppearanceLightStatusBars = !isDark
            insetsController.isAppearanceLightNavigationBars = !isDark
        }
    }

    MaterialTheme(
        colorScheme = colorScheme,
        content = content
    )
}
