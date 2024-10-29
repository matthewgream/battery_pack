package com.example.battery_monitor.connect

import android.app.Activity
import java.util.UUID

class BluetoothDeviceManager(
    tag: String,
    activity: Activity,
    config: Config,
    connectInfo: ConnectInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectDeviceManager<AdapterBluetooth, BluetoothDeviceHandler>(
    "${tag}Manager",
    activity,
    arrayOf(
        android.Manifest.permission.ACCESS_COARSE_LOCATION,
        android.Manifest.permission.ACCESS_FINE_LOCATION,
        android.Manifest.permission.BLUETOOTH,
        android.Manifest.permission.BLUETOOTH_ADMIN,
        android.Manifest.permission.BLUETOOTH_SCAN,
        android.Manifest.permission.BLUETOOTH_CONNECT
    ),
    connectInfo,
    dataCallback,
    statusCallback
) {

    open class Config(
        val deviceName: String,
        val serviceUuid: UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b"),
        val characteristicUuid: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8"),
        val connectionScanDelay : Int = 5,
        val connectionScanPeriod : Int = 30,
        val connectionActiveCheck: Int = 15,
        val connectionActiveTimeout: Int = 30,
        val connectionMtu: Int = 517
    )

    override val adapter: AdapterBluetooth = AdapterBluetooth("${tag}Adapter", activity,
        onDisabled = { onDisabled() },
        onEnabled = { onEnabled() }
    )
    override val device: BluetoothDeviceHandler = BluetoothDeviceHandler("${tag}Device", activity,
        adapter,
        config,
        connectInfo,
        dataCallback,
        statusCallback,
        isAvailable = { adapter.isEnabled() },
        isPermitted = { permissions.allowed },
    )
}