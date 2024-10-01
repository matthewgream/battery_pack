package com.example.battery_monitor

import android.util.Log

@Suppress("PropertyName")
class BluetoothDeviceCheckerConfig {
    val CONNECTION_TIMEOUT = 30000L // 30 seconds
    val CONNECTION_CHECK = 5000L // 5 seconds
}

class BluetoothDeviceChecker (
    private val config: BluetoothDeviceCheckerConfig,
    private val isConnected: () -> Boolean,
    private val onTimeout: () -> Unit
) : BluetoothComponent ("BluetoothDeviceChecker") {

    private var checked: Long = 0
    override val timer: Long
        get () = config.CONNECTION_CHECK

    override fun onStart () {
        checked = System.currentTimeMillis ()
    }
    override fun onTimer () : Boolean {
        if (isConnected () && System.currentTimeMillis () - checked > config.CONNECTION_TIMEOUT) {
            Log.d ("Bluetooth", "Device connection checker timeout")
            onTimeout ()
            return false
        }
        return true
    }
    fun ping () {
        checked = System.currentTimeMillis () // even if not active
    }
}

