package com.example.battery_monitor

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context

class BluetoothDeviceAdapter (context: Context): ConnectivityDeviceAdapter () {
    val adapter: BluetoothAdapter = (context.getSystemService (Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    override fun isEnabled(): Boolean {
        return adapter.isEnabled
    }
}