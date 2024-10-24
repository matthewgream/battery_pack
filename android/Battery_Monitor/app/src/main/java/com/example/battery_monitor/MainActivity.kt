package com.example.battery_monitor

import android.content.Context
import android.os.Bundle
import android.os.PowerManager
import android.util.Log
import org.json.JSONObject

class MainActivity : PermissionsAwareActivity() {

    private var powerSaveState: Boolean = false

    private val connectivityInfo by lazy { ConnectivityInfo (this) }
    private val connectivityStatusView: DataViewConnectivityStatus by lazy {
        findViewById(R.id.connectivityStatusView)
    }
    private val connectivityManagerBluetooth: BluetoothManager by lazy {
        BluetoothManager(this, connectivityInfo,
            dataCallback = { data ->
                val json = JSONObject(data)
                connectivityManagerAddressUpdate(json)
                dataProcessor.processDataReceived(json)
            },
            statusCallback = { connectivityStatusUpdate() })
    }
    private val connectivityManagerNetwork: NetworkManager by lazy {
        NetworkManager(this, connectivityInfo,
            dataCallback = { data -> dataProcessor.processDataReceived (JSONObject (data)) },
            statusCallback = { connectivityStatusUpdate() })
    }
    private val connectivityManagerCloud: CloudManager by lazy {
        CloudManager(this, connectivityInfo,
            dataCallback = { data -> dataProcessor.processDataReceived (JSONObject (data)) },
            statusCallback = { connectivityStatusUpdate() })
    }
    private val notificationsManager: NotificationsManager by lazy {
        NotificationsManager(this)
    }
    private lateinit var dataProcessor: DataProcessor

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        dataProcessor = DataProcessor(this,
            notificationsManager = notificationsManager)
        connectivityManagerOnDoubleTap()
        setupPowerSave()
    }
    override fun onDestroy() {
        Log.d("Main", "onDestroy")
        super.onDestroy()
        connectivityManagerBluetooth.onDestroy()
        connectivityManagerNetwork.onDestroy()
        connectivityManagerCloud.onDestroy()
    }
    override fun onPause() {
        Log.d("Main", "onPause")
        super.onPause()
        connectivityManagerBluetooth.onSuspend(true)
        connectivityManagerNetwork.onSuspend(true)
        connectivityManagerCloud.onSuspend(true)
    }
    override fun onResume() {
        Log.d("Main", "onResume")
        super.onResume()
        connectivityManagerBluetooth.onSuspend(false)
        connectivityManagerNetwork.onSuspend(false)
        connectivityManagerCloud.onSuspend(false)
    }
    private fun onPowerSave(enabled: Boolean) {
        Log.d("Main", "onPowerSave($enabled)")
        connectivityManagerBluetooth.onPowerSave(enabled)
        connectivityManagerNetwork.onPowerSave(enabled)
        connectivityManagerCloud.onPowerSave(enabled)
    }
    private fun onDoubleTap() {
        Log.d("Main", "onDoubleTap")
        connectivityManagerBluetooth.onDoubleTap()
        connectivityManagerNetwork.onDoubleTap()
        connectivityManagerCloud.onDoubleTap()
    }

    private fun connectivityManagerAddressUpdate (json: JSONObject) {
        try {
            if (json.getString("type") == "data" && json.has("addr")) {
                val deviceAddress = json.getString("addr")
                if (deviceAddress != connectivityInfo.deviceAddress) {
                    connectivityInfo.updateDeviceAddress(deviceAddress)
                    Log.d("MainActivity", "Device address changed, triggering network and cloud connection attempts")
                    connectivityManagerNetwork.onDoubleTap()
                    connectivityManagerCloud.onDoubleTap()
                }
            }
        } catch (e: Exception) {
            Log.e("MainActivity", "Error processing bluetooth address", e)
        }
    }
    private fun connectivityManagerOnDoubleTap () {
        connectivityStatusView.setOnDoubleTapListener {
            onDoubleTap ()
        }
    }
    private fun connectivityStatusUpdate () {
        connectivityStatusView.updateStatus (
            bluetoothPermitted = connectivityManagerBluetooth.isPermitted(), bluetoothAvailable = connectivityManagerBluetooth.isAvailable(), bluetoothConnected = connectivityManagerBluetooth.isConnected(),
            networkPermitted = connectivityManagerNetwork.isPermitted(), networkAvailable = connectivityManagerNetwork.isAvailable(), networkConnected = connectivityManagerNetwork.isConnected(),
            cloudPermitted = connectivityManagerCloud.isPermitted() , cloudAvailable = connectivityManagerCloud.isAvailable(), cloudConnected = connectivityManagerCloud.isConnected(),
        )
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
                        onPowerSave(true)
                    }
                else ->
                    if (powerSaveState) {
                        powerSaveState = false
                        onPowerSave(false)
                    }
            }
        }
    }
}