package com.example.battery_monitor

import android.app.Activity
import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.Handler
import android.os.Looper

class NetworkAdapter (context: Context) {
    private val connectivityManager: ConnectivityManager =
        context.getSystemService (Context.CONNECTIVITY_SERVICE) as ConnectivityManager

    fun isEnabled (): Boolean {
        val network = connectivityManager.activeNetwork
        val capabilities = connectivityManager.getNetworkCapabilities (network)
        return capabilities?.hasTransport (NetworkCapabilities.TRANSPORT_WIFI) == true
    }
}

class NetworkManager (
    activity: Activity,
    private val connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) {
    private val handler = Handler (Looper.getMainLooper ())

    private val permissions: PermissionsManager = PermissionsManagerFactory (activity).create (
        tag = "Network",
        permissions = arrayOf (
            android.Manifest.permission.INTERNET,
            android.Manifest.permission.ACCESS_NETWORK_STATE
        )
    )

    private val adapter: NetworkAdapter = NetworkAdapter (activity)
    private val device: NetworkDeviceManager = NetworkDeviceManager (activity,
        adapter,
        NetworkDeviceManagerConfig (),
        connectivityInfo,
        dataCallback,
        statusCallback,
        isPermitted = { permissions.allowed },
        isEnabled = { adapter.isEnabled () && connectivityInfo.deviceAddress.isNotEmpty() }
    )
    private val checker: NetworkStateReceiver = NetworkStateReceiver (
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
}