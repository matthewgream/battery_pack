package com.example.battery_monitor

import android.content.Context
import android.os.Bundle
import android.os.PowerManager
import android.util.Log
import org.json.JSONObject

class MainActivity : PermissionsAwareActivity() {

    private val config: ApplicationConfig = ApplicationConfig()

    private var powermanageState: Boolean = false
    private fun powermanageSetup() {
        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        powermanageState = powerManager.isPowerSaveMode
        powerManager.addThermalStatusListener { status ->
            when (status) {
                PowerManager.THERMAL_STATUS_SEVERE,
                PowerManager.THERMAL_STATUS_CRITICAL ->
                    if (!powermanageState) {
                        powermanageState = true
                        onPowerSave()
                    }
                else ->
                    if (powermanageState) {
                        powermanageState = false
                        onPowerBack()
                    }
            }
        }
    }

    //

    private var processingHandler: DataProcessor? = null
    private fun processingSetup () {
        processingHandler = DataProcessor(this,
            NotificationsManager(this, NotificationsManagerConfig (this)))
    }

    //

    private val connectivityInfo by lazy {
        ConnectivityInfo(this, config.name)
    }
    private val connectivityManagerLocal: BluetoothDeviceManager by lazy {
        BluetoothDeviceManager("Bluetooth", this, connectivityInfo,
            dataCallback = { data ->
                val json = JSONObject(data)
                connectivityManagerAddressBinder(json)
                processingHandler?.processDataReceived(json)
            },
            statusCallback = { connectivityStatusUpdate() })
    }
    private val connectivityManagerNetwork: WebSocketDeviceManager by lazy {
        WebSocketDeviceManager("WebSocket", this, WebSocketDeviceConfig (), connectivityInfo,
            dataCallback = { data -> processingHandler?.processDataReceived(JSONObject(data)) },
            statusCallback = { connectivityStatusUpdate() })
    }
    private val connectivityManagerCloud: CloudMqttDeviceManager by lazy {
        CloudMqttDeviceManager("CloudMqtt", this, CloudMqttDeviceConfig (), connectivityInfo,
            dataCallback = { data -> processingHandler?.processDataReceived(JSONObject(data)) },
            statusCallback = { connectivityStatusUpdate() })
    }
    private val connectivityManagers by lazy {
        listOf(connectivityManagerLocal, connectivityManagerNetwork, connectivityManagerCloud)
    }
    private fun connectivityManagerAddressBinder(json: JSONObject) {
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
    private val connectivityStatusView: DataViewConnectivityStatus by lazy {
        findViewById(R.id.connectivityStatusView)
    }
    private fun connectivityStatusListenerDoubleTap() {
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
    private fun connectivitySetup () {
        connectivityManagers.forEach { it.onCreate() }
        connectivityStatusListenerDoubleTap()
    }

    //

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        processingSetup ()
        connectivitySetup ()
        powermanageSetup()
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
}