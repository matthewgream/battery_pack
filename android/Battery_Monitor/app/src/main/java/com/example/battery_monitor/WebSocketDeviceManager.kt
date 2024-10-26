package com.example.battery_monitor

import android.app.Activity
import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities

class WebSocketDeviceAdapter(
    context: Context
) : ConnectivityDeviceAdapter() {
    private val connectivityManager: ConnectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    override fun isEnabled(): Boolean {
        val network = connectivityManager.activeNetwork
        val capabilities = connectivityManager.getNetworkCapabilities(network)
        return capabilities?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
    }
}

class WebSocketDeviceManager(
    tag: String,
    activity: Activity,
    config: WebSocketDeviceConfig,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<WebSocketDeviceAdapter, WebSocketDeviceHandler, WebSocketDeviceConfig, StateManagerWifi>(
    tag,
    activity,
    arrayOf(
        android.Manifest.permission.INTERNET,
        android.Manifest.permission.ACCESS_NETWORK_STATE
    ),
    connectivityInfo,
    dataCallback,
    statusCallback
) {
    override val adapter: WebSocketDeviceAdapter = WebSocketDeviceAdapter(activity)
    override val device: WebSocketDeviceHandler = WebSocketDeviceHandler(tag, activity,
        adapter,
        config,
        connectivityInfo,
        dataCallback,
        statusCallback,
        isPermitted = { permissions.allowed },
        isEnabled = { adapter.isEnabled() && connectivityInfo.deviceAddress.isNotEmpty() }
    )
    override val state: StateManagerWifi = StateManagerWifi(tag, activity,
        onDisabled = { onDisconnected() },
        onEnabled = { onPermitted() }
    )
}