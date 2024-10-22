package com.example.battery_monitor

import android.content.Context
import android.os.Bundle
import android.os.PowerManager
import android.util.Log
import org.json.JSONObject

class MainActivity : PermissionsAwareActivity() {

    private lateinit var connectivityStatusView: DataViewConnectivityStatus
    private var powerSaveState: Boolean = false

    private lateinit var notificationsManager: NotificationsManager
    private lateinit var dataProcessor: DataProcessor
    private lateinit var bluetoothManager: BluetoothManager
    private lateinit var networkManager: NetworkManager
    private lateinit var cloudManager: CloudManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        connectivityStatusView = findViewById(R.id.connectivityStatusView)

        notificationsManager = NotificationsManager(this)
        dataProcessor = DataProcessor(this, notificationsManager)
        bluetoothManager = BluetoothManager(this,
            dataCallback = { data -> dataProcessor.processDataReceived(JSONObject(data)) },
            statusCallback = { updateConnectionStatus() })
        networkManager = NetworkManager(this,
            dataCallback = { data -> dataProcessor.processDataReceived(JSONObject(data)) },
            statusCallback = { updateConnectionStatus() })
        cloudManager = CloudManager(this,
            dataCallback = { data -> dataProcessor.processDataReceived(JSONObject(data)) },
            statusCallback = { updateConnectionStatus() })

        setupConnectionStatusDoubleTap()
        setupPowerSave()
    }

    override fun onDestroy() {
        Log.d("Main", "onDestroy")
        super.onDestroy()
        bluetoothManager.onDestroy()
        networkManager.onDestroy()
        cloudManager.onDestroy()
    }

    override fun onPause() {
        Log.d("Main", "onPause")
        super.onPause()
        bluetoothManager.onSuspend(true)
        networkManager.onSuspend(true)
        cloudManager.onSuspend(true)
    }

    override fun onResume() {
        Log.d("Main", "onResume")
        super.onResume()
        bluetoothManager.onSuspend(false)
        networkManager.onSuspend(false)
        cloudManager.onSuspend(false)
    }

    private fun onPowerSave(enabled: Boolean) {
        Log.d("Main", "onPowerSave($enabled)")
        bluetoothManager.onPowerSave(enabled)
        networkManager.onPowerSave(enabled)
        cloudManager.onPowerSave(enabled)
    }

    private fun onDoubleTap() {
        Log.d("Main", "onDoubleTap")
        bluetoothManager.onDoubleTap()
        networkManager.onDoubleTap()
        cloudManager.onDoubleTap()
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
    private fun setupConnectionStatusDoubleTap() {
        connectivityStatusView.setOnDoubleTapListener {
            onDoubleTap ()
        }
    }
    private fun updateConnectionStatus() {
        connectivityStatusView.updateStatus(
            bluetoothPermitted = bluetoothManager.isPermitted(), bluetoothAvailable = bluetoothManager.isAvailable(), bluetoothConnected = bluetoothManager.isConnected(),
            networkPermitted = networkManager.isPermitted(), networkAvailable = networkManager.isAvailable(), networkConnected = networkManager.isConnected(),
            cloudPermitted = cloudManager.isPermitted() , cloudAvailable = cloudManager.isAvailable(), cloudConnected = cloudManager.isConnected(),
        )
    }
}