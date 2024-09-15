package com.example.battery_monitor


import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.widget.TextView
import org.json.JSONObject
import java.util.*

class MainActivity : Activity() {

    private val PERMISSION_REQUEST_CODE = 1
    private lateinit var bluetoothAdapter: BluetoothAdapter
    private var bluetoothGatt: BluetoothGatt? = null

    private val DEVICE_NAME = "BatteryMonitor"
    private val SERVICE_UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    private val CHARACTERISTIC_UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")

    private lateinit var statusTextView: TextView
    private lateinit var timeTextView: TextView
    private lateinit var envTempTextView: TextView
    private lateinit var batteryTempTextView: TextView
    private lateinit var fanSpeedTextView: TextView
    private lateinit var alarmsTextView: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_main)
        statusTextView = findViewById(R.id.statusTextView)
        timeTextView = findViewById(R.id.timeTextView)
        envTempTextView = findViewById(R.id.envTempTextView)
        batteryTempTextView = findViewById(R.id.batteryTempTextView)
        fanSpeedTextView = findViewById(R.id.fanSpeedTextView)
        alarmsTextView = findViewById(R.id.alarmsTextView)

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED ||
            checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            ), PERMISSION_REQUEST_CODE)
        } else {
            startBleScan()
        }
    }

    @SuppressLint("MissingPermission")
    private fun startBleScan() {
        bluetoothAdapter.bluetoothLeScanner.startScan(scanCallback)
    }

    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            Log.d("BLEScan", "Found device: ${device.address}")
            if (device.name == DEVICE_NAME) {
                bluetoothAdapter.bluetoothLeScanner.stopScan(this)
                connectToDevice(device)
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun connectToDevice(device: BluetoothDevice) {
        bluetoothGatt = device.connectGatt(this, false, gattCallback)
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                runOnUiThread { statusTextView.text = "Connected" }
                gatt.discoverServices()
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val characteristic = gatt.getService(SERVICE_UUID)?.getCharacteristic(CHARACTERISTIC_UUID)
                if (characteristic != null) {
                    gatt.setCharacteristicNotification(characteristic, true)
                }
            }
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            updateUI(String(value))
        }
    }

    private fun updateUI(jsonString: String) {
        try {
            val json = JSONObject(jsonString)
            runOnUiThread {
                timeTextView.text = "Time: ${json.getLong("time")}"
                envTempTextView.text = "Environment: ${json.getJSONObject("temperatures").getDouble("environment")}"
                batteryTempTextView.text = "Batterypack: ${json.getJSONObject("temperatures").getJSONObject("batterypack").getDouble("avg")}"
                fanSpeedTextView.text = "Fanspeed: ${json.getJSONObject("fan").getInt("speed")}"
                alarmsTextView.text = "Alarms: ${json.getInt("alarms")}"
            }
        } catch (e: Exception) {
            Log.e("JSON", "Error parsing JSON", e)
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (grantResults.isNotEmpty() && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
                startBleScan()
            } else {
                Log.e("Permissions", "Required permissions were denied")
            }
        }
    }
}
