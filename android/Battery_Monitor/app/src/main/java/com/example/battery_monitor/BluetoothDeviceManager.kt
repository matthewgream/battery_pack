package com.example.battery_monitor

import android.app.Activity

class BluetoothDeviceManager(
    tag: String,
    activity: Activity,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<BluetoothDeviceAdapter, BluetoothDeviceHandler, BluetoothDeviceHandlerConfig, StateManagerBluetooth>(
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
        BluetoothDeviceHandlerConfig(),
        connectivityInfo,
        dataCallback,
        statusCallback,
        isPermitted = { permissions.allowed },
        isEnabled = { adapter.isEnabled() }
    )
    override val checker: StateManagerBluetooth = StateManagerBluetooth(tag, activity,
        onDisabled = { onDisconnected() },
        onEnabled = { onPermitted() }
    )
}