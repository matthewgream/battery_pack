package com.example.battery_monitor

import android.bluetooth.BluetoothAdapter
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.util.Log

class StateManagerBluetooth(
    tag: String,
    private val context: Context,
    private val onDisabled: () -> Unit,
    private val onEnabled: () -> Unit
) : ConnectivityComponent(tag) {

    private val receiver: BroadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                BluetoothAdapter.ACTION_STATE_CHANGED -> {
                    when (intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)) {
                        BluetoothAdapter.STATE_OFF -> {
                            Log.d(tag, "Adapter disabled")
                            onDisabled()
                        }
                        BluetoothAdapter.STATE_ON -> {
                            Log.d(tag, "Adapter enabled")
                            onEnabled()
                        }
                    }
                }
            }
        }
    }

    override fun onStart() {
        context.registerReceiver(receiver, IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED))
    }
    override fun onStop() {
        context.unregisterReceiver(receiver)
    }
}
