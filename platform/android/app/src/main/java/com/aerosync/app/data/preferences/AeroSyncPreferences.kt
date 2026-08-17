package com.aerosync.app.data.preferences

import android.content.Context
import android.content.SharedPreferences
import android.os.Build
import android.os.Environment
import java.io.File

class AeroSyncPreferences(private val context: Context) {

    private val prefs: SharedPreferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    var isDarkTheme: Boolean
        get() = prefs.getBoolean(KEY_IS_DARK_THEME, true)
        set(value) = prefs.edit().putBoolean(KEY_IS_DARK_THEME, value).apply()

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
