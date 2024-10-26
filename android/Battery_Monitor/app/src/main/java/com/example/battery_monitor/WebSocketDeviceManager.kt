package com.example.battery_monitor

import android.app.Activity

class WebSocketDeviceManager(
    activity: Activity,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<WebSocketDeviceAdapter, WebSocketDeviceHandler, WebSocketDeviceHandlerConfig, StateManagerNetwork>(
    activity,
    "WebSocket",
    arrayOf(
        android.Manifest.permission.INTERNET,
        android.Manifest.permission.ACCESS_NETWORK_STATE
    ),
    connectivityInfo,
    dataCallback,
    statusCallback
) {
    override val adapter: WebSocketDeviceAdapter = WebSocketDeviceAdapter(activity)
    override val device: WebSocketDeviceHandler = WebSocketDeviceHandler(activity,
        adapter,
        WebSocketDeviceHandlerConfig(),
        connectivityInfo,
        dataCallback,
        statusCallback,
        isPermitted = { permissions.allowed },
        isEnabled = { adapter.isEnabled() && connectivityInfo.deviceAddress.isNotEmpty() }
    )
    override val checker: StateManagerNetwork = StateManagerNetwork(activity, "WebSocket",
        onDisabled = { onDisconnected() },
        onEnabled = { onPermitted() }
    )
}