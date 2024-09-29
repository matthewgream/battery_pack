package com.example.battery_monitor

import android.bluetooth.BluetoothAdapter
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.util.Log

class BluetoothStateReceiver(
    private val context: Context,
    private val onDisabled: () -> Unit,
    private val onEnabled: () -> Unit
) {
    private val handler: BroadcastReceiver = object : BroadcastReceiver () {
        override fun onReceive (context: Context?, intent: Intent?) {
            when (intent?.action) {
                BluetoothAdapter.ACTION_STATE_CHANGED -> {
                    when (intent.getIntExtra (BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)) {
                        BluetoothAdapter.STATE_OFF -> {
                            Log.d ("Bluetooth", "Adapter disabled")
                            onDisabled ()
                        }
                        BluetoothAdapter.STATE_ON -> {
                            Log.d ("Bluetooth", "Adapter enabled")
                            onEnabled ()
                        }
                    }
                }
            }
        }
    }

    private var active = false

    init {
        start ()
    }

    private fun start () {
        if (!active) {
            context.registerReceiver (handler, IntentFilter (BluetoothAdapter.ACTION_STATE_CHANGED))
            active = true
            Log.d ("Bluetooth", "Adapter state receiver started")
        }
    }

    fun stop () {
        if (active) {
            context.unregisterReceiver (handler)
            active = false
            Log.d ("Bluetooth", "Adapter state receiver stopped")
        }
    }
}
