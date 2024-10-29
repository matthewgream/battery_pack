package com.example.battery_monitor.connect

import android.app.Activity
import android.util.Log
import com.example.battery_monitor.utility.Activable
import org.json.JSONObject

class ConnectManager(
    private val tag: String,
    private val activity: Activity,
    private val name: String,
    private val config: Config,
    private val statusView: ConnectStatusView,
    statusConfig: ConnectStatusView.Config,
    private val onReceiveData: (JSONObject) -> Unit,
    private val addressExtractor: (JSONObject) -> String?
) {

    private var subscribedToCloud = Activable()

    open class Config (
        val bluetooth : BluetoothDeviceManager.Config,
        val websocket : WebSocketDeviceManager.Config,
        val cloudmqtt : CloudMqttDeviceManager.Config,
    )

    private val info by lazy {
        ConnectInfo(activity, name)
    }

    private val managerDirect: BluetoothDeviceManager by lazy {
        BluetoothDeviceManager("Bluetooth", activity, config.bluetooth, info,
            dataCallback = { data ->
                val json = JSONObject(data)
                addressBinder(json)
                if (!subscribedToCloud.isActive)
                    onReceiveData(json)
            },
            statusCallback = { updateStatus() }
        )
    }
    private val managerLocal: WebSocketDeviceManager by lazy {
        WebSocketDeviceManager("WebSocket", activity, config.websocket, info,
            dataCallback = { data ->
                if (!subscribedToCloud.isActive)
                    onReceiveData(JSONObject(data))
            },
            statusCallback = { updateStatus() }
        )
    }
    private val managerCloud: CloudMqttDeviceManager by lazy {
        CloudMqttDeviceManager("CloudMqtt", activity, config.cloudmqtt, info,
            dataCallback = { data ->
                if (subscribedToCloud.isActive)
                    onReceiveData(JSONObject(data))
            },
            statusCallback = { updateStatus() }
        )
    }
    private val managers by lazy {
        listOf(managerDirect, managerLocal, managerCloud)
    }
    private fun addressBinder(json: JSONObject) {
        try {
            addressExtractor(json)?.let { deviceAddress ->
                if (deviceAddress != info.deviceAddress) {
                    info.updateDeviceAddress(deviceAddress)
                    Log.d(tag, "Device address changed, triggering network and cloud connection attempts")
                    listOf(managerLocal, managerCloud).forEach { it.onDoubleTap() }
                }
            }
        } catch (e: Exception) {
            Log.e(tag, "Error processing bluetooth address: exception", e)
        }
    }
    private fun subscriberUpdate(directOrLocal: Boolean) {
        if ((!directOrLocal && info.deviceAddress.isNotEmpty()) && subscribedToCloud.toActive())
            managerCloud.subscribe()
        else if (directOrLocal && subscribedToCloud.toInactive())
            managerCloud.unsubscribe()
    }
    private fun updateStatus() {
        val statuses = managers.associate { manager ->
            when (manager) {
                is BluetoothDeviceManager -> ConnectType.DIRECT
                is WebSocketDeviceManager -> ConnectType.LOCAL
                is CloudMqttDeviceManager -> ConnectType.CLOUD
                else -> throw IllegalStateException("Unknown manager type")
            } to ConnectStatus(
                available = manager.isAvailable(),
                permitted = manager.isPermitted(),
                connected = manager.isConnected()
            )
        }
        val directOrLocal = statuses[ConnectType.DIRECT]?.connected!! || statuses[ConnectType.LOCAL]?.connected!!
        subscriberUpdate(directOrLocal)
        statusView.updateStatus(statuses)
    }

    init {
        statusView.initialize(statusConfig)
    }

    fun onCreate() {
        managers.forEach { it.onCreate() }
        statusView.setOnDoubleTapListener { onDoubleTap() }
    }
    fun onDestroy() {
        managers.forEach { it.onDestroy() }
    }
    fun onPause() {
        managers.forEach { it.onPause() }
    }
    fun onResume() {
        managers.forEach { it.onResume() }
    }
    fun onPowerSave() {
        managers.forEach { it.onPowerSave() }
    }
    fun onPowerBack() {
        managers.forEach { it.onPowerBack() }
    }
    private fun onDoubleTap() {
        managers.forEach { it.onDoubleTap() }
    }
}