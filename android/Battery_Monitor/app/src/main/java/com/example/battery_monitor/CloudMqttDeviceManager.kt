package com.example.battery_monitor

import android.app.Activity

class CloudMqttDeviceManager(
    activity: Activity,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<CloudMqttDeviceAdapter, CloudMqttDeviceHandler, CloudMqttDeviceHandlerConfig, StateManagerNetwork>(
    activity,
    "CloudMqtt",
    arrayOf(
        android.Manifest.permission.INTERNET,
        android.Manifest.permission.ACCESS_NETWORK_STATE
    ),
    connectivityInfo,
    dataCallback,
    statusCallback
) {
    override val adapter: CloudMqttDeviceAdapter = CloudMqttDeviceAdapter(activity)
    override val device: CloudMqttDeviceHandler = CloudMqttDeviceHandler(activity,
        adapter,
        CloudMqttDeviceHandlerConfig(
            host = SECRET_MQTT_HOST,
            port = SECRET_MQTT_PORT,
            user = SECRET_MQTT_USER,
            pass = SECRET_MQTT_PASS,
            topic = "BatteryMonitor"
        ),
        connectivityInfo,
        dataCallback,
        statusCallback,
        isEnabled = { adapter.isEnabled() && connectivityInfo.deviceAddress.isNotEmpty() },
        isPermitted = { permissions.allowed }
    )
    override val checker: StateManagerNetwork = StateManagerNetwork(activity, "CloudMqtt",
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