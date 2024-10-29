package com.example.battery_monitor.connect

import android.app.Activity

class WebSocketDeviceManager(
    tag: String,
    activity: Activity,
    config: Config,
    connectInfo: ConnectInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectDeviceManager<AdapterNetworkWifi, WebSocketDeviceHandler>(
    "${tag}Manager",
    activity,
    arrayOf(
        android.Manifest.permission.INTERNET,
        android.Manifest.permission.ACCESS_NETWORK_STATE
    ),
    connectInfo,
    dataCallback,
    statusCallback
) {

    open class Config(
        val serviceName: String,
        val serviceType: String,
        val connectionScanDelay: Int = 5,
        val connectionScanPeriod: Int = 30,
        val connectionActiveCheck: Int = 15,
        val connectionActiveTimeout: Int = 30
    )

    override val adapter: AdapterNetworkWifi = AdapterNetworkWifi("${tag}Adapter", activity,
        onDisabled = { onDisabled() },
        onEnabled = { onEnabled() }
    )
    override val device: WebSocketDeviceHandler = WebSocketDeviceHandler("${tag}Device", activity,
        adapter,
        config,
        connectInfo,
        dataCallback,
        statusCallback,
        isAvailable = { adapter.isEnabled() && connectInfo.deviceAddress.isNotEmpty() },
        isPermitted = { permissions.allowed }
    )
}