package com.example.battery_monitor

import android.content.Context
import android.os.Bundle
import android.os.PowerManager
import android.util.Log
import org.json.JSONObject

enum class ConnectivityType {
    LOCAL,
    NETWORK,
    CLOUD
}

data class ConnectivityStatus(
    val permitted: Boolean,
    val available: Boolean,
    val connected: Boolean,
    val standby: Boolean
)

class MainActivity : PermissionsAwareActivity() {

    private var powerSaveState: Boolean = false

    private val connectivityInfo by lazy { ConnectivityInfo(this) }
    private val connectivityStatusView: DataViewConnectivityStatus by lazy {
        findViewById(R.id.connectivityStatusView)
    }
    private val connectivityManagerLocal: BluetoothDeviceManager by lazy {
        BluetoothDeviceManager(this, connectivityInfo,
            dataCallback = { data ->
                val json = JSONObject(data)
                connectivityManagerAddressUpdate(json)
                dataProcessor.processDataReceived(json)
            },
            statusCallback = { connectivityStatusUpdate() })
    }
    private val connectivityManagerNetwork: WebSocketDeviceManager by lazy {
        WebSocketDeviceManager(this, connectivityInfo,
            dataCallback = { data -> dataProcessor.processDataReceived(JSONObject(data)) },
            statusCallback = { connectivityStatusUpdate() })
    }
    private val connectivityManagerCloud: CloudMqttDeviceManager by lazy {
        CloudMqttDeviceManager(this, connectivityInfo,
            dataCallback = { data -> dataProcessor.processDataReceived(JSONObject(data)) },
            statusCallback = { connectivityStatusUpdate() })
    }
    private val connectivityManagers by lazy {
        listOf(connectivityManagerLocal, connectivityManagerNetwork, connectivityManagerCloud)
    }

    //

    private val notificationsManager: NotificationsManager by lazy {
        NotificationsManager(this)
    }
    private lateinit var dataProcessor: DataProcessor

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        dataProcessor = DataProcessor(this, notificationsManager)
        connectivityManagerOnDoubleTap()
        setupPowerSave()
        connectivityManagers.forEach { it.onCreate() }
    }

    override fun onDestroy() {
        Log.d("Main", "onDestroy")
        super.onDestroy()
        connectivityManagers.forEach { it.onDestroy() }
    }

    override fun onPause() {
        Log.d("Main", "onPause")
        super.onPause()
        connectivityManagers.forEach { it.onPause() }
    }

    override fun onResume() {
        Log.d("Main", "onResume")
        super.onResume()
        connectivityManagers.forEach { it.onResume() }
    }

    private fun onPowerSave() {
        Log.d("Main", "onPowerSave")
        connectivityManagers.forEach { it.onPowerSave() }
    }

    private fun onPowerBack() {
        Log.d("Main", "onPowerBack")
        connectivityManagers.forEach { it.onPowerBack() }
    }

    private fun onDoubleTap() {
        Log.d("Main", "onDoubleTap")
        connectivityManagers.forEach { it.onDoubleTap() }
    }

    private fun connectivityManagerAddressUpdate(json: JSONObject) {
        try {
            if (json.getString("type") == "data" && json.has("addr")) {
                val deviceAddress = json.getString("addr")
                if (deviceAddress != connectivityInfo.deviceAddress) {
                    connectivityInfo.updateDeviceAddress(deviceAddress)
                    Log.d("Main", "Device address changed, triggering network and cloud connection attempts")
                    listOf(connectivityManagerNetwork, connectivityManagerCloud).forEach { it.onDoubleTap() }
                }
            }
        } catch (e: Exception) {
            Log.e("Main", "Error processing bluetooth address: error=${e.message}")
        }
    }

    private fun connectivityManagerOnDoubleTap() {
        connectivityStatusView.setOnDoubleTapListener {
            onDoubleTap()
        }
    }

    private fun connectivityStatusUpdate() {
        val statuses = connectivityManagers.associate { manager ->
            when (manager) {
                is BluetoothDeviceManager -> ConnectivityType.LOCAL
                is WebSocketDeviceManager -> ConnectivityType.NETWORK
                is CloudMqttDeviceManager -> ConnectivityType.CLOUD
                else -> throw IllegalStateException ("Unknown manager type")
            } to ConnectivityStatus(
                permitted = manager.isPermitted(),
                available = manager.isAvailable(),
                connected = manager.isConnected(),
                standby = false // for now
            )
        }
        connectivityStatusView.updateStatus(statuses)
    }

    private fun setupPowerSave() {
        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        powerSaveState = powerManager.isPowerSaveMode
        powerManager.addThermalStatusListener { status ->
            when (status) {
                PowerManager.THERMAL_STATUS_SEVERE,
                PowerManager.THERMAL_STATUS_CRITICAL ->
                    if (!powerSaveState) {
                        powerSaveState = true
                        onPowerSave()
                    }

                else ->
                    if (powerSaveState) {
                        powerSaveState = false
                        onPowerBack()
                    }
            }
        }
    }
}