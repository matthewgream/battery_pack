package com.example.battery_monitor

import android.app.Activity

class WebSocketDeviceManager(
    tag: String,
    activity: Activity,
    config: WebSocketDeviceConfig,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<AdapterNetworkWifi, WebSocketDeviceHandler, WebSocketDeviceConfig>(
    "${tag}Manager",
    activity,
    arrayOf(
        android.Manifest.permission.INTERNET,
        android.Manifest.permission.ACCESS_NETWORK_STATE
    ),
    connectivityInfo,
    dataCallback,
    statusCallback
) {
    override val adapter: AdapterNetworkWifi = AdapterNetworkWifi("${tag}Adapter", activity,
        onDisabled = { onDisabled() },
        onEnabled = { onEnabled() }
    )
    override val device: WebSocketDeviceHandler = WebSocketDeviceHandler("${tag}Device", activity,
        adapter,
        config,
        connectivityInfo,
        dataCallback,
        statusCallback,
        isAvailable = { adapter.isEnabled() && connectivityInfo.deviceAddress.isNotEmpty() },
        isPermitted = { permissions.allowed }
    )
}