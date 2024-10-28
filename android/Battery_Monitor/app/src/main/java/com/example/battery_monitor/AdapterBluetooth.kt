package com.example.battery_monitor

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.BluetoothLeScanner
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.util.Log

class AdapterBluetooth(
    tag: String,
    private val context: Context,
    private val onDisabled: () -> Unit,
    private val onEnabled: () -> Unit
) : ConnectivityDeviceAdapter(tag) {

    private val adapter: BluetoothAdapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    private val enabled = Activable(isEnabled())

    private val receiver: BroadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                BluetoothAdapter.ACTION_STATE_CHANGED -> {
                    when (intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)) {
                        BluetoothAdapter.STATE_OFF -> {
                            if (enabled.toInactive()) {
                                Log.d(tag, "Bluetooth adapter not available")
                                onDisabled()
                            }
                        }
                        BluetoothAdapter.STATE_ON -> {
                            if (enabled.toActive()) {
                                Log.d(tag, "Bluetooth adapter available")
                                onEnabled()
                            }
                        }
                    }
                }
            }
        }
    }

    fun scanner(): BluetoothLeScanner {
        return adapter.bluetoothLeScanner
    }

    override fun isEnabled(): Boolean {
        return adapter.isEnabled
    }
    override fun onStart() {
        context.registerReceiver(receiver, IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED))
    }
    override fun onStop() {
        context.unregisterReceiver(receiver)
    }
}
