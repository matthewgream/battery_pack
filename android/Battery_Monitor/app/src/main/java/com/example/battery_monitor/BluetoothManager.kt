package com.example.battery_monitor

import android.app.Activity

class BluetoothManager(
    activity: Activity,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : com.example.battery_monitor.ConnectivityManager <BluetoothDeviceAdapter, BluetoothDeviceManager, BluetoothDeviceManagerConfig, BluetoothDeviceState> (activity,
    "Bluetooth",
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
    override val adapter: BluetoothDeviceAdapter = BluetoothDeviceAdapter (activity)
    override val device: BluetoothDeviceManager = BluetoothDeviceManager (activity,
        adapter,
        BluetoothDeviceManagerConfig (),
        connectivityInfo,
        dataCallback,
        statusCallback,
        isPermitted = { permissions.allowed },
        isEnabled = { adapter.isEnabled () }
    )
    override val checker: BluetoothDeviceState = BluetoothDeviceState (activity,
        onDisabled = { onDisconnect () },
        onEnabled = { onPermitted () }
    )
}