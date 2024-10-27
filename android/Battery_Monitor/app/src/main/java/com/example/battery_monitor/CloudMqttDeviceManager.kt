package com.example.battery_monitor

import android.app.Activity

class CloudMqttDeviceManager(
    tag: String,
    activity: Activity,
    config: CloudMqttDeviceConfig,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<AdapterInternet, CloudMqttDeviceHandler, CloudMqttDeviceConfig>(
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
    override val adapter: AdapterInternet = AdapterInternet("${tag}Adapter", activity,
        onDisabled = { onDisconnected() },
        onEnabled = { onPermitted() }
    )
    override val device: CloudMqttDeviceHandler = CloudMqttDeviceHandler("${tag}Device", activity,
        adapter,
        config,
        connectivityInfo,
        dataCallback,
        statusCallback,
        isEnabled = { adapter.isEnabled() && connectivityInfo.deviceAddress.isNotEmpty() },
        isPermitted = { permissions.allowed }
    )

    //

    @Suppress("unused")
    fun publish(type: String, message: String) : Boolean {
        return device.publish(type, message)
    }
    fun subscribe() : Boolean {
        return device.subscribe()
    }
    fun unsubscribe() : Boolean {
        return device.unsubscribe()
    }
}