package com.example.battery_monitor

import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context

class BluetoothDeviceAdapter(
    context: Context
) : ConnectivityDeviceAdapter() {
    val adapter: BluetoothAdapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    override fun isEnabled(): Boolean {
        return adapter.isEnabled
    }
}

class BluetoothDeviceManager(
    tag: String,
    activity: Activity,
    config: BluetoothDeviceConfig,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<BluetoothDeviceAdapter, BluetoothDeviceHandler, BluetoothDeviceConfig, StateManagerBluetooth>(
    tag,
    activity,
    arrayOf(
        android.Manifest.permission.ACCESS_COARSE_LOCATION,
        android.Manifest.permission.ACCESS_FINE_LOCATION,
        android.Manifest.permission.BLUETOOTH,
        android.Manifest.permission.BLUETOOTH_ADMIN,
        android.Manifest.permission.BLUETOOTH_SCAN,
        android.Manifest.permission.BLUETOOTH_CONNECT
    ),
    connectivityInfo,
    dataCallback,
    statusCallback
) {
    override val adapter: BluetoothDeviceAdapter = BluetoothDeviceAdapter(activity)
    override val device: BluetoothDeviceHandler = BluetoothDeviceHandler(tag, activity,
        adapter,
        config,
        connectivityInfo,
        dataCallback,
        statusCallback,
        isPermitted = { permissions.allowed },
        isEnabled = { adapter.isEnabled() }
    )
    override val state: StateManagerBluetooth = StateManagerBluetooth(tag, activity,
        onDisabled = { onDisconnected() },
        onEnabled = { onPermitted() }
    )
}