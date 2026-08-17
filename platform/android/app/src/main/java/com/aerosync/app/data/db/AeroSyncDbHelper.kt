package com.aerosync.app.data.db

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import com.aerosync.app.viewmodel.QueueItemStatus
import com.aerosync.app.viewmodel.TransferHistoryItem
import com.aerosync.app.viewmodel.TransferQueueItem

class AeroSyncDbHelper(context: Context) : SQLiteOpenHelper(context, DATABASE_NAME, null, DATABASE_VERSION) {

    override fun onCreate(db: SQLiteDatabase) {
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS $TABLE_QUEUE (
                $COL_ID TEXT PRIMARY KEY,
                $COL_FILE_NAME TEXT NOT NULL,
                $COL_FILE_SIZE INTEGER NOT NULL,
                $COL_FILE_PATH TEXT NOT NULL,
                $COL_STATUS TEXT NOT NULL,
                $COL_PROGRESS INTEGER DEFAULT 0,
                $COL_TRANSFERRED_BYTES INTEGER DEFAULT 0,
                $COL_SPEED_MBPS REAL DEFAULT 0.0,
                $COL_ETA_SECONDS INTEGER DEFAULT 0,
                $COL_IS_RECEIVED INTEGER DEFAULT 0,
                $COL_PEER_NAME TEXT DEFAULT '',
                $COL_TIMESTAMP INTEGER NOT NULL
            )
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS $TABLE_HISTORY (
                $COL_ID TEXT PRIMARY KEY,
                $COL_FILE_NAME TEXT NOT NULL,
                $COL_FILE_SIZE INTEGER NOT NULL,
                $COL_FILE_PATH TEXT NOT NULL,
                $COL_IS_RECEIVED INTEGER DEFAULT 0,
                $COL_PEER_NAME TEXT DEFAULT '',
                $COL_TIMESTAMP INTEGER NOT NULL,
                $COL_STATUS TEXT DEFAULT 'COMPLETED',
                $COL_DURATION_MS INTEGER DEFAULT 0,
                $COL_AVG_SPEED_BPS REAL DEFAULT 0.0
            )
            """.trimIndent()
        )
    }

    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        db.execSQL("DROP TABLE IF EXISTS $TABLE_QUEUE")
        db.execSQL("DROP TABLE IF EXISTS $TABLE_HISTORY")
        onCreate(db)
    }

    // ==========================================
    // Transfer Queue CRUD
    // ==========================================

    @Synchronized
    fun insertOrUpdateQueueItem(item: TransferQueueItem) {
        val db = writableDatabase
        val values = ContentValues().apply {
            put(COL_ID, item.id)
            put(COL_FILE_NAME, item.fileName)
            put(COL_FILE_SIZE, item.fileSize)
            put(COL_FILE_PATH, item.filePath)
            put(COL_STATUS, item.status.name)
            put(COL_PROGRESS, item.progressPercent)
            put(COL_TRANSFERRED_BYTES, item.transferredBytes)
            put(COL_SPEED_MBPS, item.speedMbps)
            put(COL_ETA_SECONDS, item.etaSeconds)
            put(COL_IS_RECEIVED, if (item.isReceived) 1 else 0)
            put(COL_PEER_NAME, "")
            put(COL_TIMESTAMP, item.timestamp)
        }
        db.insertWithOnConflict(TABLE_QUEUE, null, values, SQLiteDatabase.CONFLICT_REPLACE)
    }

    @Synchronized
    fun insertQueueItems(items: List<TransferQueueItem>) {
        val db = writableDatabase
        db.beginTransaction()
        try {
            for (item in items) {
                val values = ContentValues().apply {
                    put(COL_ID, item.id)
                    put(COL_FILE_NAME, item.fileName)
                    put(COL_FILE_SIZE, item.fileSize)
                    put(COL_FILE_PATH, item.filePath)
                    put(COL_STATUS, item.status.name)
                    put(COL_PROGRESS, item.progressPercent)
                    put(COL_TRANSFERRED_BYTES, item.transferredBytes)
                    put(COL_SPEED_MBPS, item.speedMbps)
                    put(COL_ETA_SECONDS, item.etaSeconds)
                    put(COL_IS_RECEIVED, if (item.isReceived) 1 else 0)
                    put(COL_PEER_NAME, "")
                    put(COL_TIMESTAMP, item.timestamp)
                }
                db.insertWithOnConflict(TABLE_QUEUE, null, values, SQLiteDatabase.CONFLICT_REPLACE)
            }
            db.setTransactionSuccessful()
        } finally {
            db.endTransaction()
        }
    }

    @Synchronized
    fun getAllQueueItems(): List<TransferQueueItem> {
        val list = mutableListOf<TransferQueueItem>()
        val db = readableDatabase
        val cursor = db.query(
            TABLE_QUEUE,
            null,
            null,
            null,
            null,
            null,
            "$COL_TIMESTAMP ASC"
        )
        cursor.use {
            val idIdx = it.getColumnIndexOrThrow(COL_ID)
            val nameIdx = it.getColumnIndexOrThrow(COL_FILE_NAME)
            val sizeIdx = it.getColumnIndexOrThrow(COL_FILE_SIZE)
            val pathIdx = it.getColumnIndexOrThrow(COL_FILE_PATH)
            val statusIdx = it.getColumnIndexOrThrow(COL_STATUS)
            val progressIdx = it.getColumnIndexOrThrow(COL_PROGRESS)
            val transferredIdx = it.getColumnIndexOrThrow(COL_TRANSFERRED_BYTES)
            val speedIdx = it.getColumnIndexOrThrow(COL_SPEED_MBPS)
            val etaIdx = it.getColumnIndexOrThrow(COL_ETA_SECONDS)
            val receivedIdx = it.getColumnIndexOrThrow(COL_IS_RECEIVED)
            val timeIdx = it.getColumnIndexOrThrow(COL_TIMESTAMP)

            while (it.moveToNext()) {
                val statusStr = it.getString(statusIdx)
                val status = try {
                    QueueItemStatus.valueOf(statusStr)
                } catch (_: Exception) {
                    QueueItemStatus.QUEUED
                }
                // Convert any uncompleted 'TRANSFERRING' state from crash to PAUSED on load
                val recoveredStatus = if (status == QueueItemStatus.TRANSFERRING) QueueItemStatus.PAUSED else status

                list.add(
                    TransferQueueItem(
                        id = it.getString(idIdx),
                        fileName = it.getString(nameIdx),
                        fileSize = it.getLong(sizeIdx),
                        filePath = it.getString(pathIdx),
                        status = recoveredStatus,
                        progressPercent = it.getInt(progressIdx),
                        transferredBytes = it.getLong(transferredIdx),
                        speedMbps = it.getDouble(speedIdx),
                        etaSeconds = it.getInt(etaIdx),
                        isReceived = it.getInt(receivedIdx) == 1,
                        timestamp = it.getLong(timeIdx)
                    )
                )
            }
        }
        return list
    }

    @Synchronized
    fun deleteQueueItem(id: String) {
        val db = writableDatabase
        db.delete(TABLE_QUEUE, "$COL_ID = ?", arrayOf(id))
    }

    @Synchronized
    fun clearQueue() {
        val db = writableDatabase
        db.delete(TABLE_QUEUE, null, null)
    }

    // ==========================================
    // Transfer History CRUD
    // ==========================================

    @Synchronized
    fun insertHistoryItem(item: TransferHistoryItem) {
        val db = writableDatabase
        val values = ContentValues().apply {
            put(COL_ID, item.id)
            put(COL_FILE_NAME, item.fileName)
            put(COL_FILE_SIZE, item.fileSize)
            put(COL_FILE_PATH, item.filePath)
            put(COL_IS_RECEIVED, if (item.isReceived) 1 else 0)
            put(COL_PEER_NAME, item.peerName)
            put(COL_TIMESTAMP, item.timestamp)
            put(COL_STATUS, item.status)
            put(COL_DURATION_MS, item.durationMs)
            put(COL_AVG_SPEED_BPS, item.avgSpeedBps)
        }
        db.insertWithOnConflict(TABLE_HISTORY, null, values, SQLiteDatabase.CONFLICT_REPLACE)
    }

    @Synchronized
    fun getAllHistoryItems(): List<TransferHistoryItem> {
        val list = mutableListOf<TransferHistoryItem>()
        val db = readableDatabase
        val cursor = db.query(
            TABLE_HISTORY,
            null,
            null,
            null,
            null,
            null,
            "$COL_TIMESTAMP DESC"
        )
        cursor.use {
            val idIdx = it.getColumnIndexOrThrow(COL_ID)
            val nameIdx = it.getColumnIndexOrThrow(COL_FILE_NAME)
            val sizeIdx = it.getColumnIndexOrThrow(COL_FILE_SIZE)
            val pathIdx = it.getColumnIndexOrThrow(COL_FILE_PATH)
            val receivedIdx = it.getColumnIndexOrThrow(COL_IS_RECEIVED)
            val peerIdx = it.getColumnIndexOrThrow(COL_PEER_NAME)
            val timeIdx = it.getColumnIndexOrThrow(COL_TIMESTAMP)
            val statusIdx = it.getColumnIndexOrThrow(COL_STATUS)
            val durationIdx = it.getColumnIndexOrThrow(COL_DURATION_MS)
            val speedIdx = it.getColumnIndexOrThrow(COL_AVG_SPEED_BPS)

            while (it.moveToNext()) {
                list.add(
                    TransferHistoryItem(
                        id = it.getString(idIdx),
                        fileName = it.getString(nameIdx),
                        fileSize = it.getLong(sizeIdx),
                        filePath = it.getString(pathIdx),
                        isReceived = it.getInt(receivedIdx) == 1,
                        peerName = it.getString(peerIdx),
                        timestamp = it.getLong(timeIdx),
                        status = it.getString(statusIdx),
                        durationMs = it.getLong(durationIdx),
                        avgSpeedBps = it.getDouble(speedIdx)
                    )
                )
            }
        }
        return list
    }

    @Synchronized
    fun completeTransferTransaction(queueId: String, historyItem: TransferHistoryItem) {
        val db = writableDatabase
        db.beginTransaction()
        try {
            // Delete from queue
            db.delete(TABLE_QUEUE, "$COL_ID = ?", arrayOf(queueId))
            // Insert into history
            val values = ContentValues().apply {
                put(COL_ID, historyItem.id)
                put(COL_FILE_NAME, historyItem.fileName)
                put(COL_FILE_SIZE, historyItem.fileSize)
                put(COL_FILE_PATH, historyItem.filePath)
                put(COL_IS_RECEIVED, if (historyItem.isReceived) 1 else 0)
                put(COL_PEER_NAME, historyItem.peerName)
                put(COL_TIMESTAMP, historyItem.timestamp)
                put(COL_STATUS, historyItem.status)
                put(COL_DURATION_MS, historyItem.durationMs)
                put(COL_AVG_SPEED_BPS, historyItem.avgSpeedBps)
            }
            db.insertWithOnConflict(TABLE_HISTORY, null, values, SQLiteDatabase.CONFLICT_REPLACE)
            db.setTransactionSuccessful()
        } finally {
            db.endTransaction()
        }
    }

    @Synchronized
    fun clearHistory() {
        val db = writableDatabase
        db.delete(TABLE_HISTORY, null, null)
    }

    companion object {
        private const val DATABASE_NAME = "aerosync.db"
        private const val DATABASE_VERSION = 1

        private const val TABLE_QUEUE = "transfer_queue"
        private const val TABLE_HISTORY = "transfer_history"

        private const val COL_ID = "id"
        private const val COL_FILE_NAME = "file_name"
        private const val COL_FILE_SIZE = "file_size"
        private const val COL_FILE_PATH = "file_path"
        private const val COL_STATUS = "status"
        private const val COL_PROGRESS = "progress_percent"
        private const val COL_TRANSFERRED_BYTES = "transferred_bytes"
        private const val COL_SPEED_MBPS = "speed_mbps"
        private const val COL_ETA_SECONDS = "eta_seconds"
        private const val COL_IS_RECEIVED = "is_received"
        private const val COL_PEER_NAME = "peer_name"
        private const val COL_TIMESTAMP = "timestamp"

        private const val COL_DURATION_MS = "duration_ms"
        private const val COL_AVG_SPEED_BPS = "avg_speed_bps"

        @Volatile
        private var INSTANCE: AeroSyncDbHelper? = null

        fun getInstance(context: Context): AeroSyncDbHelper {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: AeroSyncDbHelper(context.applicationContext).also { INSTANCE = it }
            }
        }
    }
}
