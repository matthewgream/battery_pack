package com.example.battery_monitor

import android.app.Activity
import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.Handler
import android.os.Looper

class CloudAdapter (context: Context) {
    private val connectivityManager: ConnectivityManager =
        context.getSystemService (Context.CONNECTIVITY_SERVICE) as ConnectivityManager

    fun isEnabled (): Boolean {
        val network = connectivityManager.activeNetwork
        val capabilities = connectivityManager.getNetworkCapabilities (network)
        return capabilities?.hasCapability (NetworkCapabilities.NET_CAPABILITY_INTERNET) == true
    }
}

class CloudManager (
    activity: Activity,
    private val connectionInfo: ConnectionInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) {
    private val handler = Handler (Looper.getMainLooper ())

    private val permissions: PermissionsManager = PermissionsManagerFactory (activity).create (
        tag = "Cloud",
        permissions = arrayOf (
            android.Manifest.permission.INTERNET,
            android.Manifest.permission.ACCESS_NETWORK_STATE
        )
    )

    private val adapter: CloudAdapter = CloudAdapter (activity)
    private val device: CloudDeviceManager = CloudDeviceManager (
        activity,
        adapter,
        CloudDeviceManagerConfig (
            host = SECRET_MQTT_HOST, port = SECRET_MQTT_PORT,
            user = SECRET_MQTT_USER, pass = SECRET_MQTT_PASS
        ),
        connectionInfo,
        dataCallback,
        statusCallback,
        isPermitted = { permissions.allowed },
        isEnabled = { adapter.isEnabled () && connectionInfo.deviceAddress.isNotEmpty() }
    )
    private val checker: NetworkStateReceiver = NetworkStateReceiver (
        context = activity,
        onDisabled = { device.disconnect () },
        onEnabled = { device.connect () }
    )

    init {
        permissions.requestPermissions (
            onPermissionsAllowed = {
                handler.postDelayed ({
                    device.permissionsAllowed ()
                }, 50)
            }
        )
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

    fun isAvailable (): Boolean = adapter.isEnabled ()
    fun isPermitted (): Boolean = permissions.allowed
    fun isConnected (): Boolean = device.isConnected ()

    //

    fun publish (topic: String, message: String) {
        device.publish (topic, message)
    }
    fun subscribe (topic: String) {
        device.subscribe (topic)
    }
}
