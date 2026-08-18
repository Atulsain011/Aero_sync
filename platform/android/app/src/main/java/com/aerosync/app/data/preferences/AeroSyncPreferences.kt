package com.aerosync.app.data.preferences

import android.content.Context
import android.content.SharedPreferences
import android.os.Build
import android.os.Environment
import java.io.File

enum class ThemeMode {
    SYSTEM,
    LIGHT,
    DARK
}

class AeroSyncPreferences(private val context: Context) {

    private val prefs: SharedPreferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    var themeMode: ThemeMode
        get() {
            val modeStr = prefs.getString(KEY_THEME_MODE, null)
            return if (modeStr != null) {
                try {
                    ThemeMode.valueOf(modeStr)
                } catch (_: Exception) {
                    ThemeMode.DARK
                }
            } else {
                if (prefs.getBoolean(KEY_IS_DARK_THEME, true)) ThemeMode.DARK else ThemeMode.LIGHT
            }
        }
        set(value) {
            prefs.edit()
                .putString(KEY_THEME_MODE, value.name)
                .putBoolean(KEY_IS_DARK_THEME, value != ThemeMode.LIGHT)
                .apply()
        }

    var isDarkTheme: Boolean
        get() = themeMode != ThemeMode.LIGHT
        set(value) {
            themeMode = if (value) ThemeMode.DARK else ThemeMode.LIGHT
        }

    var downloadDirectory: String
        get() {
            val saved = prefs.getString(KEY_DOWNLOAD_DIRECTORY, null)
            if (!saved.isNullOrBlank()) {
                val f = File(saved)
                if (f.exists() || f.mkdirs()) {
                    if (f.canWrite()) return saved
                }
            }
            // Standard public Downloads/AeroSync
            try {
                val pubDir = File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "AeroSync")
                if (pubDir.exists() || pubDir.mkdirs()) {
                    if (pubDir.canWrite()) return pubDir.absolutePath
                }
            } catch (_: Exception) {}

            // Guaranteed writable fallback in app-specific external files dir
            val extDir = context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS) ?: context.filesDir
            val defDir = File(extDir, "AeroSync")
            if (!defDir.exists()) defDir.mkdirs()
            return defDir.absolutePath
        }
        set(value) {
            if (value.isNotBlank()) {
                prefs.edit().putString(KEY_DOWNLOAD_DIRECTORY, value).apply()
            }
        }

    var deviceName: String
        get() {
            val defaultName = Build.MODEL ?: "AeroSync Device"
            return prefs.getString(KEY_DEVICE_NAME, defaultName) ?: defaultName
        }
        set(value) {
            if (value.isNotBlank()) {
                prefs.edit().putString(KEY_DEVICE_NAME, value).apply()
            }
        }

    companion object {
        private const val PREFS_NAME = "aerosync_user_preferences"
        private const val KEY_THEME_MODE = "key_theme_mode"
        private const val KEY_IS_DARK_THEME = "key_is_dark_theme"
        private const val KEY_DOWNLOAD_DIRECTORY = "key_download_directory"
        private const val KEY_DEVICE_NAME = "key_device_name"

        @Volatile
        private var INSTANCE: AeroSyncPreferences? = null

        fun getInstance(context: Context): AeroSyncPreferences {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: AeroSyncPreferences(context.applicationContext).also { INSTANCE = it }
            }
        }
    }
}
