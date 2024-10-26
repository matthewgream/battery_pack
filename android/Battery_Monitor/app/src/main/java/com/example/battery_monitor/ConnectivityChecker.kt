package com.example.battery_monitor

import android.util.Log

@Suppress("PropertyName")
class ConnectivityCheckerConfig(
    val CONNECTION_TIMEOUT: Long = 30000L, // 30 seconds
    val CONNECTION_CHECK: Long = 5000L // 5 seconds
)

class ConnectivityChecker(
    override val tag: String,
    private val config: ConnectivityCheckerConfig,
    private val isConnected: () -> Boolean,
    private val onTimeout: () -> Unit
) : ConnectivityComponent(tag) {

    private var checked: Long = 0
    override val timer: Long
        get() = config.CONNECTION_CHECK

    override fun onStart() {
        checked = System.currentTimeMillis()
    }

    override fun onTimer(): Boolean {
        if (isConnected() && System.currentTimeMillis() - checked > config.CONNECTION_TIMEOUT) {
            Log.d(tag, "Device connection checker timeout")
            onTimeout()
            return false
        }
        return true
    }

    fun ping() {
        checked = System.currentTimeMillis() // even if not active
    }
}

