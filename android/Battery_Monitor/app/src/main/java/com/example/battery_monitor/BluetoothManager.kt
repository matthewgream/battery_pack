package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.os.Handler
import android.os.Looper

@SuppressLint("MissingPermission")
class BluetoothManager (
    private val activity: Activity,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) {

    private val handler = Handler (Looper.getMainLooper ())

    private val permissions: PermissionsManager = PermissionsManagerFactory (activity).create (
        tag = "Bluetooth",
        permissions = arrayOf (
            android.Manifest.permission.ACCESS_COARSE_LOCATION,
            android.Manifest.permission.ACCESS_FINE_LOCATION,
            android.Manifest.permission.BLUETOOTH,
            android.Manifest.permission.BLUETOOTH_ADMIN,
            android.Manifest.permission.BLUETOOTH_SCAN,
            android.Manifest.permission.BLUETOOTH_CONNECT
        )
    )
    private val adapter: BluetoothAdapter by lazy {
        val bluetoothManager = activity.getSystemService (Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothManager.adapter
    }
    private val device: BluetoothDeviceManager = BluetoothDeviceManager (activity,
        adapter,
        BluetoothDeviceManagerConfig (),
        connectivityInfo,
        dataCallback,
        statusCallback,
        isPermitted = { permissions.allowed },
        isEnabled = { adapter.isEnabled () }
    )
    private val checker: BluetoothStateReceiver = BluetoothStateReceiver (
        context = activity,
        onDisabled = { device.disconnect () },
        onEnabled = { device.locate () }
    )

    init {
        permissions.requestPermissions (
            onPermissionsAllowed = {
                handler.postDelayed ({
                    device.permissionsAllowed ()
                }, 50)
        })
        checker.start ()
    }

    //

    fun onDestroy () {
        checker.stop ()
        device.disconnect ()
    }
    fun onSuspend (enabled: Boolean) {
    }
    fun onPowerSave (enabled: Boolean) {
    }
    fun onDoubleTap () {
        device.reconnect ()
    }

    //

    fun isAvailable (): Boolean = adapter.isEnabled
    fun isPermitted (): Boolean = permissions.allowed
    fun isConnected (): Boolean = device.isConnected ()
}