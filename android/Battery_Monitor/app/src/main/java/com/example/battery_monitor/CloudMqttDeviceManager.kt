package com.example.battery_monitor

import android.app.Activity

class CloudMqttDeviceManager(
    tag: String,
    activity: Activity,
    config: CloudMqttDeviceConfig,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<CloudMqttDeviceAdapter, CloudMqttDeviceHandler, CloudMqttDeviceConfig, StateManagerNetwork>(
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
    override val adapter: CloudMqttDeviceAdapter = CloudMqttDeviceAdapter(activity)
    override val device: CloudMqttDeviceHandler = CloudMqttDeviceHandler(tag, activity,
        adapter,
        config,
        connectivityInfo,
        dataCallback,
        statusCallback,
        isEnabled = { adapter.isEnabled() && connectivityInfo.deviceAddress.isNotEmpty() },
        isPermitted = { permissions.allowed }
    )
    override val checker: StateManagerNetwork = StateManagerNetwork(tag, activity,
        onDisabled = { onDisconnected() },
        onEnabled = { onPermitted() }
    )

    //

    fun publish(topic: String, message: String) : Boolean {
        return device.publish(topic, message)
    }
    fun subscribe(topic: String) : Boolean {
        return device.subscribe(topic)
    }
}