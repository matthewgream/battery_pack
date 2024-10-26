package com.example.battery_monitor

import android.util.Log

class ConnectivityChecker(
    override val tag: String,
    periodCheck : Int,
    private val periodTimeout : Int,
    private val isConnected: () -> Boolean,
    private val onTimeout: () -> Unit
) : ConnectivityComponent(tag, periodCheck) {

    private var checked: Long = 0

    override fun onStart() {
        checked = System.currentTimeMillis()
    }
    override fun onTimer(): Boolean {
        if (isConnected() && System.currentTimeMillis() - checked > (periodTimeout*1000L)) {
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

