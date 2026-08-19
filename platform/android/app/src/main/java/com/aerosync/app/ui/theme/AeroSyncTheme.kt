package com.aerosync.app.ui.theme

import android.app.Activity
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.sp
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

// Standard AeroSync Typography Definition
val AeroTypography = Typography(
    headlineLarge = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.ExtraBold,
        fontSize = 24.sp,
        lineHeight = 30.sp,
        letterSpacing = (-0.5).sp
    ),
    headlineMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Bold,
        fontSize = 18.sp,
        lineHeight = 24.sp,
        letterSpacing = (-0.3).sp
    ),
    titleMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.SemiBold,
        fontSize = 15.sp,
        lineHeight = 20.sp
    ),
    bodyLarge = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Normal,
        fontSize = 14.sp,
        lineHeight = 20.sp
    ),
    bodyMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Normal,
        fontSize = 13.sp,
        lineHeight = 18.sp
    ),
    bodySmall = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Normal,
        fontSize = 11.sp,
        lineHeight = 16.sp
    ),
    labelMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.SemiBold,
        fontSize = 11.sp,
        lineHeight = 14.sp
    ),
    labelSmall = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Bold,
        fontSize = 10.sp,
        lineHeight = 12.sp
    )
)

@Composable
fun AeroSyncTheme(
    themeMode: ThemeMode = ThemeMode.DARK,
    content: @Composable () -> Unit
) {
    val isDark = themeMode == ThemeMode.DARK

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

    val currentDensity = LocalDensity.current
    val clampedDensity = Density(
        density = currentDensity.density,
        fontScale = currentDensity.fontScale.coerceIn(0.85f, 1.15f)
    )

    CompositionLocalProvider(LocalDensity provides clampedDensity) {
        MaterialTheme(
            colorScheme = colorScheme,
            typography = AeroTypography,
            content = content
        )
    }
}
