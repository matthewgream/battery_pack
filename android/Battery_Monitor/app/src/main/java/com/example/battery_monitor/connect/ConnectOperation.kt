package com.example.battery_monitor.connect

import android.util.Log

inline fun <T> connectOperation(
    tag: String,
    operationType: String,
    operation: String,
    block: () -> T?
): T? {
    return try {
        block()?.also {
            Log.d(tag, "$operationType $operation: success")
        }
    } catch (e: Exception) {
        Log.e(tag, "$operationType $operation: exception", e)
        null
    }
}

inline fun <T> String.withOperation(
    operationType: String,
    operation: String,
    block: () -> T?
): T? = connectOperation(this, operationType, operation, block)