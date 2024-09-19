package com.example.battery_monitor

import android.annotation.SuppressLint
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
import android.os.Handler
import android.os.Looper
import android.util.Log
import java.util.UUID

@SuppressLint ("MissingPermission")
class BluetoothManager (
    private val context: Context,
    private val dataCallback: (String) -> Unit,
    private val statusCallback: (Boolean) -> Unit
) {

    private val DEVICE_NAME = "BatteryMonitor"
    private val SERVICE_UUID = UUID.fromString ("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    private val CHARACTERISTIC_UUID = UUID.fromString ("beb5483e-36e1-4688-b7f5-ea07361b26a8")

    private val bluetoothAdapter: BluetoothAdapter
    private var bluetoothGatt: BluetoothGatt? = null
    private var isConnected = false
    private var isScanning = false
    private val handler = Handler (Looper.getMainLooper ())
    private var reconnectAttempts = 0
    private val MAX_RECONNECT_ATTEMPTS = 5
    private val RECONNECT_INTERVAL = 5000L // 5 seconds
    private val SCAN_PERIOD = 10000L // 10 seconds

    init {
        val bluetoothManager = context.getSystemService (Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter
    }

    //

    fun isAvailable (): Boolean {
        return bluetoothAdapter.isEnabled
    }

    //

    fun scan () {
        if (bluetoothAdapter.isEnabled && !isConnected && !isScanning)
            scanStart ()
    }
    private fun scanStart () {
        if (!isScanning) {
            isScanning = true
            bluetoothAdapter.bluetoothLeScanner.startScan (scanCallback)
            handler.postDelayed ({ scanStop() }, SCAN_PERIOD)
            Log.d ("Bluetooth", "Scan started, for ${SCAN_PERIOD/1000} seconds")
        }
    }
    private fun scanStop () {
        if (isScanning) {
            isScanning = false
            bluetoothAdapter.bluetoothLeScanner.stopScan (scanCallback)
            Log.d ("Bluetooth", "Scan stopped")
        }
    }
    private val scanCallback = object : ScanCallback () {
        override fun onScanResult (callbackType: Int, result: ScanResult) {
            val device = result.device
            Log.d ("Bluetooth", "Scan located, device ${device.name} / ${device.address}")
            if (device.name == DEVICE_NAME) {
                scanStop ()
                deviceConnect (device)
            }
        }
    }

    //

    private val gattCallback = object : BluetoothGattCallback () {
        override fun onConnectionStateChange (gatt: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    Log.d ("Bluetooth", "Device connected, to GATT server")
                    isConnected = true
                    reconnectAttempts = 0
                    gatt.discoverServices ()
                    statusCallback (true)
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    Log.d ("Bluetooth", "Device disconnected, from GATT server")
                    isConnected = false
                    statusCallback (false)
                    deviceReconnect ()
                }
            }
        }
        override fun onServicesDiscovered (gatt: BluetoothGatt, status: Int) {
            Log.d ("Bluetooth", "Device discovery, with status $status")
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val characteristic = gatt.getService (SERVICE_UUID)?.getCharacteristic (CHARACTERISTIC_UUID)
                if (characteristic != null)
                    gatt.setCharacteristicNotification (characteristic, true)
            }
        }
        override fun onCharacteristicChanged (gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            Log.d ("Bluetooth", "Device received data, of size ${value.size}")
            dataCallback (String (value))
        }
    }
    private fun deviceConnect (device: BluetoothDevice) {
        Log.d ("Bluetooth", "Device connect, to ${device.name} / ${device.address}")
        bluetoothGatt = device.connectGatt (context, false, gattCallback)
    }
    private fun deviceReconnect () {
        if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
            reconnectAttempts ++
            Log.d ("Bluetooth", "Device reconnect, attempt $reconnectAttempts of $MAX_RECONNECT_ATTEMPTS")
            handler.postDelayed ({ if (!isConnected) scanStart () }, RECONNECT_INTERVAL)
        } else {
            Log.e ("Bluetooth", "Device reconnect, failed after $MAX_RECONNECT_ATTEMPTS attempts")
            statusCallback (false)
        }
    }
    fun deviceDisconnect () {
        handler.removeCallbacksAndMessages (null)
        if (bluetoothGatt != null) {
            Log.d ("Bluetooth", "Device disconnect")
            bluetoothGatt?.close ()
            bluetoothGatt = null
        }
        isConnected = false
    }
}
