package com.example.battery_monitor

import android.app.Activity

class BluetoothDeviceManager(
    tag: String,
    activity: Activity,
    config: BluetoothDeviceConfig,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<AdapterBluetooth, BluetoothDeviceHandler, BluetoothDeviceConfig>(
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
    override val adapter: AdapterBluetooth = AdapterBluetooth(tag, activity,
        onDisabled = { onDisconnected() },
        onEnabled = { onPermitted() }
    )
    override val device: BluetoothDeviceHandler = BluetoothDeviceHandler(tag, activity,
        adapter,
        config,
        connectivityInfo,
        dataCallback,
        statusCallback,
        isPermitted = { permissions.allowed },
        isEnabled = { adapter.isEnabled() }
    )
}