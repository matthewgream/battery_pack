package com.example.battery_monitor

import android.app.Activity

class CloudManager(
    activity: Activity,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : com.example.battery_monitor.ConnectivityManager <CloudDeviceAdapter, CloudDeviceManager, CloudDeviceManagerConfig, NetworkDeviceState> (activity,
    "CloudManager",
    arrayOf(
        android.Manifest.permission.INTERNET,
        android.Manifest.permission.ACCESS_NETWORK_STATE
    ),
    connectivityInfo,
    dataCallback,
    statusCallback
) {
    override val adapter: CloudDeviceAdapter = CloudDeviceAdapter (activity)
    override val device: CloudDeviceManager = CloudDeviceManager (activity,
        adapter,
        CloudDeviceManagerConfig (
            host = SECRET_MQTT_HOST,
            port = SECRET_MQTT_PORT,
            user = SECRET_MQTT_USER,
            pass = SECRET_MQTT_PASS,
            topic = "BatteryMonitor/peer"
        ),
        connectivityInfo,
        dataCallback,
        statusCallback,
        isEnabled = { adapter.isEnabled () && connectivityInfo.deviceAddress.isNotEmpty () },
        isPermitted = { permissions.allowed }
    )
    override val checker: NetworkDeviceState = NetworkDeviceState (activity,
        "CloudDeviceState",
        onDisabled = { onDisconnect () },
        onEnabled = { onPermitted () }
    )

    //

    fun publish(topic: String, message: String) {
        device.publish(topic, message)
    }
    fun subscribe(topic: String) {
        device.subscribe(topic)
    }
}