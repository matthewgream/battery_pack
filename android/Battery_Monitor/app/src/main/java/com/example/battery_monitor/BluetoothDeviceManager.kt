package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import java.util.UUID

@Suppress("PropertyName")
class BluetoothDeviceManagerConfig {
    val DEVICE_NAME = "BatteryMonitor"
    val SERVICE_UUID: UUID = UUID.fromString ("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    val CHARACTERISTIC_UUID: UUID = UUID.fromString ("beb5483e-36e1-4688-b7f5-ea07361b26a8")
    val MTU = 512
    val CONNECTION_TIMEOUT = 30000L // 30 seconds
    val DISCOVERY_TIMEOUT = 10000L // 10 seconds
}

@SuppressLint("MissingPermission")
class BluetoothDeviceManager (
    private val activity: Activity,
    private val adapter: BluetoothAdapter,
    private val config: BluetoothDeviceManagerConfig,
    private val dataCallback: (String) -> Unit,
    private val statusCallback: () -> Unit,
    private val isPermitted: () -> Boolean
) {
    private val handler = Handler (Looper.getMainLooper ())

    private var connectionTimeoutRunnable: Runnable? = null
    private var discoveryTimeoutRunnable: Runnable? = null

    private var bluetoothGatt: BluetoothGatt? = null
    private var isConnected = false

    private fun connectionTimeoutStart () {
        connectionTimeoutRunnable = Runnable {
            if (!isConnected) {
                Log.e ("Bluetooth", "Device connect timeout")
                disconnect ()
            }
        }
        handler.postDelayed (connectionTimeoutRunnable!!, config.CONNECTION_TIMEOUT)
    }
    private fun connectionTimeoutCancel () {
        connectionTimeoutRunnable?.let {
            handler.removeCallbacks (it)
            connectionTimeoutRunnable = null
        }
    }
    private fun discoveryTimeoutStart () {
        discoveryTimeoutRunnable = Runnable {
            if (!isConnected) {
                Log.e ("Bluetooth", "Device discovery timeout")
                disconnect ()
            }
        }
        handler.postDelayed (discoveryTimeoutRunnable!!, config.DISCOVERY_TIMEOUT)
    }
    private fun discoveryTimeoutCancel () {
        discoveryTimeoutRunnable?.let {
            handler.removeCallbacks (it)
            discoveryTimeoutRunnable = null
        }
    }

    private val scanner: BluetoothDeviceScanner = BluetoothDeviceScanner (
        adapter.bluetoothLeScanner,
        BluetoothDeviceScannerConfig (config.DEVICE_NAME, config.SERVICE_UUID),
        onFound = { device -> connect (device) }
    )
    private val checker: BluetoothDeviceChecker = BluetoothDeviceChecker (
        BluetoothDeviceCheckerConfig (),
        isConnected = { isConnected () },
        onTimeout = { reconnect () }
    )

    private val gattCallback = object : BluetoothGattCallback () {
        override fun onConnectionStateChange (gatt: BluetoothGatt, status: Int, newState: Int) {
            when (status) {
                BluetoothGatt.GATT_SUCCESS -> {
                    when (newState) {
                        BluetoothProfile.STATE_CONNECTED -> connected (gatt)
                        BluetoothProfile.STATE_DISCONNECTED -> disconnected ()
                    }
                    return
                }
                0x3E -> Log.e ("Bluetooth", "Device connection terminated by local host")
                0x3B -> Log.e ("Bluetooth", "Device connection terminated by remote device")
                0x85 -> Log.e ("Bluetooth", "Device connection timed out")
                else -> Log.e ("Bluetooth", "Device connection state change error: $status")
            }
            disconnect ()
        }
        override fun onServicesDiscovered (gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e ("Bluetooth", "Device discovery error: $status")
                disconnect ()
            } else {
                discovered (gatt)
            }
        }
        override fun onCharacteristicChanged (gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            dataReceived (characteristic.uuid, value)
        }
        override fun onMtuChanged (gatt: BluetoothGatt, mtu: Int, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e ("Bluetooth", "Device MTU change failed: $status")
                // disconnect ??
            } else {
                Log.d ("Bluetooth", "Device MTU changed to $mtu")
            }
        }
    }

    fun permissionsAllowed () {
        statusCallback ()
        if (!isConnected)
            locate ()
    }
    fun locate () {
        Log.d ("Bluetooth", "Device locate initiated")
        statusCallback ()
        when {
            !adapter.isEnabled -> Log.e ("Bluetooth", "Bluetooth is not enabled")
            !isPermitted () -> Log.e ("Bluetooth", "Bluetooth is not permitted")
            isConnected -> Log.d ("Bluetooth", "Bluetooth device is already connected, will not locate")
            else -> scanner.start ()
        }
    }

    private fun connect (device: BluetoothDevice) {
        Log.d ("Bluetooth", "Device connect to ${device.name} / ${device.address}")
        connectionTimeoutStart ()
        bluetoothGatt = device.connectGatt (activity, true, gattCallback)
    }

    @SuppressLint("ObsoleteSdkInt")
    private fun connected (gatt: BluetoothGatt) {
        Log.d ("Bluetooth", "Device connected to GATT server")
        connectionTimeoutCancel ()
        isConnected = true
        statusCallback ()
        checker.start ()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            gatt.requestConnectionPriority (BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER)
            gatt.requestMtu (config.MTU)
        }
        discoveryTimeoutStart ()
        gatt.discoverServices ()
    }

    private fun discovered (gatt: BluetoothGatt) {
        Log.d ("Bluetooth", "Device discovery completed")
        discoveryTimeoutCancel ()
        val characteristic = gatt.getService (config.SERVICE_UUID)?.getCharacteristic (config.CHARACTERISTIC_UUID)
        if (characteristic != null) {
            Log.d ("Bluetooth", "Device notifications enable for ${characteristic.uuid}")
            bluetoothGatt?.setCharacteristicNotification (characteristic, true)
            val descriptor = characteristic.getDescriptor (UUID.fromString ("00002902-0000-1000-8000-00805f9b34fb")) //  "Client Characteristic Configuration"
            descriptor?.let {
                @Suppress("DEPRECATION")
                it.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                bluetoothGatt?.writeDescriptor (it)
            }
        } else {
            Log.e ("Bluetooth", "Device characteristic ${config.CHARACTERISTIC_UUID} or service ${config.SERVICE_UUID} not found")
            disconnect ()
        }
    }

    private fun dataReceived (uuid: UUID, value: ByteArray) {
        Log.d ("Bluetooth", "Device characteristic changed on ${uuid}: ${String(value)}")
        checker.ping ()
        try {
            dataCallback (String(value))
        } catch (e: Exception) {
            Log.e ("Bluetooth", "Device data processing error", e)
        }
    }

    fun disconnect () {
        if (bluetoothGatt != null) {
            Log.d ("Bluetooth", "Device disconnect")
            bluetoothGatt?.close ()
            bluetoothGatt = null
        }
        discoveryTimeoutCancel ()
        connectionTimeoutCancel ()
        handler.removeCallbacksAndMessages (null)
        scanner.stop ()
        checker.stop ()
        isConnected = false
        statusCallback ()
    }

    private fun disconnected () {
        Log.d ("Bluetooth", "Device disconnected from GATT server")
        if (bluetoothGatt != null) {
            bluetoothGatt?.close ()
            bluetoothGatt = null
        }
        checker.stop ()
        isConnected = false
        statusCallback ()
        reconnect ()
    }

    fun reconnect () {
        Log.d ("Bluetooth", "Device reconnect")
        disconnect ()
        handler.postDelayed ({
            locate ()
        }, 50)
    }

    fun isConnected (): Boolean = isConnected
}
