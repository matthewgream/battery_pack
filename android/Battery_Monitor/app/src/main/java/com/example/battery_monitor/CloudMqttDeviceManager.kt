package com.example.battery_monitor

import android.app.Activity
import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities

class CloudMqttDeviceAdapter(
    context: Context
) : ConnectivityDeviceAdapter() {
    private val connectivityManager: ConnectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    override fun isEnabled(): Boolean {
        val network = connectivityManager.activeNetwork
        val capabilities = connectivityManager.getNetworkCapabilities(network)
        return capabilities?.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET) == true
    }
}

class CloudMqttDeviceManager(
    tag: String,
    activity: Activity,
    config: CloudMqttDeviceConfig,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : ConnectivityDeviceManager<CloudMqttDeviceAdapter, CloudMqttDeviceHandler, CloudMqttDeviceConfig, StateManagerInternet>(
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
    override val state: StateManagerInternet = StateManagerInternet(tag, activity,
        onDisabled = { onDisconnected() },
        onEnabled = { onPermitted() }
    )

    //

    @Suppress("unused")
    fun publish(topic: String, message: String) : Boolean {
        return device.publish(topic, message)
    }
    @Suppress("unused")
    fun subscribe(topic: String) : Boolean {
        return device.subscribe(topic)
    }
}