package com.example.battery_monitor

import android.app.Activity

class WebSocketDeviceManager(
    tag: String,
    activity: Activity,
    config: WebSocketDeviceConfig,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<WebSocketDeviceAdapter, WebSocketDeviceHandler, WebSocketDeviceConfig, StateManagerNetwork>(
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
    override val checker: StateManagerNetwork = StateManagerNetwork(tag, activity,
        onDisabled = { onDisconnected() },
        onEnabled = { onPermitted() }
    )
}