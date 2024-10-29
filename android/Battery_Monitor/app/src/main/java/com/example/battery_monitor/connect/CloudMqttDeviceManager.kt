package com.example.battery_monitor.connect

import android.app.Activity

class CloudMqttDeviceManager(
    tag: String,
    activity: Activity,
    config: Config,
    connectInfo: ConnectInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectDeviceManager<AdapterNetworkInternet, CloudMqttDeviceHandler>(
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
        val host: String = "mqtt.local",
        val port: Int = 1883,
        val user: String?,
        val pass: String?,
        val root: String = "",
        val connectionActiveCheck: Int = 15,
        val connectionActiveTimeout: Int = 30
    )

    override val adapter: AdapterNetworkInternet = AdapterNetworkInternet("${tag}Adapter", activity,
        onDisabled = { onDisabled() },
        onEnabled = { onEnabled() }
    )
    override val device: CloudMqttDeviceHandler = CloudMqttDeviceHandler("${tag}Device", activity,
        adapter,
        config,
        connectInfo,
        dataCallback,
        statusCallback,
        isAvailable = { adapter.isEnabled() && connectInfo.deviceAddress.isNotEmpty() },
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