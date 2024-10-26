package com.example.battery_monitor

import android.util.Log

class ConnectivityChecker(
    override val tag: String,
    private val periodTimeout : Long,
    private val periodCheck: Long,
    private val isConnected: () -> Boolean,
    private val onTimeout: () -> Unit
) : ConnectivityComponent(tag) {

    private var checked: Long = 0
    override val timer: Long
        get() = periodCheck

    override fun onStart() {
        checked = System.currentTimeMillis()
    }

    override fun onTimer(): Boolean {
        if (isConnected() && System.currentTimeMillis() - checked > periodTimeout) {
            Log.d(tag, "Connection checker timeout")
            onTimeout()
            return false
        }
        return true
    }

    fun ping() {
        checked = System.currentTimeMillis() // even if not active
    }
}

