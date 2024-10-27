package com.example.battery_monitor

import android.app.Activity

class WebSocketDeviceManager(
    tag: String,
    activity: Activity,
    config: WebSocketDeviceConfig,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<AdapterWifi, WebSocketDeviceHandler, WebSocketDeviceConfig>(
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
    override val adapter: AdapterWifi = AdapterWifi(tag, activity,
        onDisabled = { onDisconnected() },
        onEnabled = { onPermitted() }
    )
    override val device: WebSocketDeviceHandler = WebSocketDeviceHandler(tag, activity,
        adapter,
        config,
        connectivityInfo,
        dataCallback,
        statusCallback,
        isPermitted = { permissions.allowed },
        isEnabled = { adapter.isEnabled() && connectivityInfo.deviceAddress.isNotEmpty() }
    )
}