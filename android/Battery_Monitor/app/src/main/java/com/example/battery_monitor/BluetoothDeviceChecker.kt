package com.example.battery_monitor

import android.os.Handler
import android.os.Looper
import android.util.Log

@Suppress("PrivatePropertyName")
class BluetoothDeviceChecker (
    private val isConnected: () -> Boolean,
    private val onTimeout: () -> Unit
) {
    private val handler = Handler (Looper.getMainLooper ())

    private val CONNECTION_TIMEOUT = 30000L // 30 seconds
    private val CONNECTION_CHECK = 5000L // 5 seconds

    private var checked: Long = 0
    private var active = false

    private val runner = object : Runnable {
        override fun run () {
            if (isConnected () && System.currentTimeMillis () - checked > CONNECTION_TIMEOUT) {
                Log.d ("Bluetooth", "Device connection checker timeout, restarting")
                onTimeout ()
            }
            handler.postDelayed (this, CONNECTION_CHECK)
        }
    }

    fun start () {
        if (!active) {
            checked = System.currentTimeMillis ()
            handler.post (runner)
            Log.d ("Bluetooth", "Device connection checker started")
            active = true
        }
    }

    fun stop () {
        if (active) {
            handler.removeCallbacksAndMessages (null)
            Log.d ("Bluetooth", "Device connection checker stopped")
            active = false
        }
    }

    fun ping () {
        checked = System.currentTimeMillis () // even if not active
    }
}
