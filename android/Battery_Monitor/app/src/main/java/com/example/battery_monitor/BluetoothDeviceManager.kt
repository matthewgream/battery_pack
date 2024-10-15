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
import android.content.pm.PackageManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import org.json.JSONObject
import java.nio.charset.StandardCharsets
import java.time.Instant
import java.time.format.DateTimeFormatter
import java.util.UUID


@Suppress("PropertyName")
class BluetoothDeviceManagerConfig {
    val DEVICE_NAME = "BatteryMonitor"
    val SERVICE_UUID: UUID = UUID.fromString ("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    val CHARACTERISTIC_UUID: UUID = UUID.fromString ("beb5483e-36e1-4688-b7f5-ea07361b26a8")
    val MTU = 517 // maximum
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

    private var bluetoothGatt: BluetoothGatt? = null
    private var isConnected = false

    private lateinit var timeoutName: String
    private val timeoutRunnable = Runnable {
        if (!isConnected) {
            Log.e ("Bluetooth", "Device $timeoutName timeout")
            disconnect ()
        }
    }
    private fun timeoutStart (name: String, timeout: Long) {
        timeoutName = name
        handler.postDelayed (timeoutRunnable, timeout)
    }
    private fun timeoutStop () {
        handler.removeCallbacks (timeoutRunnable)
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
        override fun onCharacteristicWrite (gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?, status: Int) {
            super.onCharacteristicWrite (gatt, characteristic, status)
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e ("Bluetooth", "Device write error: $status")
            }
        }
        override fun onCharacteristicChanged (gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            dataReceived (characteristic.uuid, value)
        }
        override fun onMtuChanged (gatt: BluetoothGatt, mtu: Int, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                Log.e ("Bluetooth", "Device MTU change error: $status")
                // disconnect ??
            } else {
                Log.d ("Bluetooth", "Device MTU changed to $mtu")
            }
        }
    }


    @SuppressLint("MissingPermission")
    fun transmitTypeInfo () {
        val characteristic = bluetoothGatt?.getService (config.SERVICE_UUID)?.getCharacteristic (config.CHARACTERISTIC_UUID)
        if (characteristic != null) {
            val appName = "batterymonitor"
            val appVersion = try {
                activity.packageManager.getPackageInfo (activity.packageName, PackageManager.PackageInfoFlags.of (0)).versionName
            } catch (e: PackageManager.NameNotFoundException) {
                "?.?.?"
            }
            val appPlatform = "android${Build.VERSION.SDK_INT}"
            val appDevice = "${Build.MANUFACTURER} ${Build.MODEL}"

            val jsonString = JSONObject ().apply {
                put ("type", "info")
                put ("time", DateTimeFormatter.ISO_INSTANT.format (Instant.now ()))
                put ("info", "$appName-custom-$appPlatform-v$appVersion ($appDevice)")
            }.toString ()

            bluetoothGatt?.writeCharacteristic (characteristic, jsonString.toByteArray (StandardCharsets.UTF_8), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
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
            isConnected -> Log.d ("Bluetooth", "Device is already connected, will not locate")
            else -> scanner.start ()
        }
    }

    private fun connect (device: BluetoothDevice) {
        Log.d ("Bluetooth", "Device connect to ${device.name} / ${device.address}")
        timeoutStart ("connection", config.CONNECTION_TIMEOUT)
        bluetoothGatt = device.connectGatt (activity, true, gattCallback)
    }

    private fun connected (gatt: BluetoothGatt) {
        Log.d ("Bluetooth", "Device connected to GATT server")
        timeoutStop ()
        isConnected = true
        statusCallback ()
        checker.start ()
        gatt.requestConnectionPriority (BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER)
        gatt.requestMtu (config.MTU)
        timeoutStart ("discovery", config.DISCOVERY_TIMEOUT)
        gatt.discoverServices ()
    }

    private fun discovered (gatt: BluetoothGatt) {
        Log.d ("Bluetooth", "Device discovery completed")
        timeoutStop ()
        val characteristic = gatt.getService (config.SERVICE_UUID)?.getCharacteristic (config.CHARACTERISTIC_UUID)
        if (characteristic != null) {
            Log.d ("Bluetooth", "Device notifications enable for ${characteristic.uuid}")
            bluetoothGatt?.setCharacteristicNotification (characteristic, true)
            val descriptor = characteristic.getDescriptor (UUID.fromString ("00002902-0000-1000-8000-00805f9b34fb"))
            bluetoothGatt?.writeDescriptor (descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            handler.postDelayed ({
                transmitTypeInfo ()
            }, 1000)
        } else {
            Log.e ("Bluetooth", "Device service ${config.SERVICE_UUID} or characteristic ${config.CHARACTERISTIC_UUID} not found")
            disconnect ()
        }
    }

    private fun dataReceived (uuid: UUID, value: ByteArray) {
        Log.d ("Bluetooth", "Device characteristic changed on ${uuid}: ${String (value)}")
        checker.ping ()
        try {
            dataCallback (String (value))
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
        timeoutStop ()
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
