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

@SuppressLint("MissingPermission")
@Suppress("PrivatePropertyName")
class BluetoothDeviceManager (
    private val activity: Activity,
    private val adapter: BluetoothAdapter,
    private val dataCallback: (String) -> Unit,
    private val statusCallback: () -> Unit,
    private val isPermitted: () -> Boolean
) {
    private val handler = Handler (Looper.getMainLooper ())

    private val DEVICE_NAME = "BatteryMonitor"
    private val SERVICE_UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    private val CHARACTERISTIC_UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")

    private var bluetoothGatt: BluetoothGatt? = null
    private var isConnected = false

    private val CONNECTION_TIMEOUT = 30000L // 30 seconds
    private var connectionTimeoutRunnable: Runnable? = null
    private fun connectionTimeoutStart () {
        connectionTimeoutRunnable = Runnable {
            if (!isConnected) {
                Log.e ("Bluetooth", "Device connect timeout")
                disconnect ()
            }
        }
        handler.postDelayed (connectionTimeoutRunnable!!, CONNECTION_TIMEOUT)
    }
    private fun connectionTimeoutCancel () {
        connectionTimeoutRunnable?.let {
            handler.removeCallbacks (it)
            connectionTimeoutRunnable = null
        }
    }
//    private val DISCOVERY_TIMEOUT = 10000L // 10 seconds
    private val DEFAULT_MTU = 512
//    private val discoveryTimeoutToken = 12312

    private val scanner: BluetoothDeviceScanner = BluetoothDeviceScanner (
        adapter.bluetoothLeScanner,
        DEVICE_NAME,
        SERVICE_UUID,
        onFound = { device -> connect (device) }
    )
    private val checker: BluetoothDeviceChecker = BluetoothDeviceChecker (
        isConnected = { isConnected () },
        onTimeout = { reconnect () }
    )

    private val gattCallback = object : BluetoothGattCallback () {
        override fun onConnectionStateChange (gatt: BluetoothGatt, status: Int, newState: Int) {
            when (status) {
                BluetoothGatt.GATT_SUCCESS -> {
                    connectionTimeoutCancel ()
                    when (newState) {
                        BluetoothProfile.STATE_CONNECTED -> connected (gatt)
                        BluetoothProfile.STATE_DISCONNECTED -> disconnected ()
                    }
                }
                0x85 -> Log.e("Bluetooth", "Device connection timed out")
                0x3E -> Log.e("Bluetooth", "Device connection terminated by local host")
                0x3B -> Log.e("Bluetooth", "Device connection terminated by remote device")
                else -> Log.e("Bluetooth", "Device connection state change error: $status")
            }
        }
        override fun onServicesDiscovered (gatt: BluetoothGatt, status: Int) {
//            handler.removeCallbacksAndMessages (discoveryTimeoutToken)
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
            if (status == BluetoothGatt.GATT_SUCCESS)
                Log.d ("Bluetooth", "Device MTU changed to $mtu")
            else
                Log.e ("Bluetooth", "Device MTU change failed: $status")
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
            !adapter.isEnabled -> {
                Log.e ("Bluetooth", "Bluetooth is not enabled")
                // prompt to enable? TBD
            }
            !isPermitted () -> {
                Log.e ("Bluetooth", "Bluetooth is not permitted")
                // re-request permission TBD
            }
            isConnected -> {
                Log.d ("Bluetooth", "Bluetooth is already connected, will not locate")
            }
            else -> scanner.start ()
        }
    }

    private fun connect (device: BluetoothDevice) {
        Log.d ("Bluetooth", "Device connect, to ${device.name} / ${device.address}")
        bluetoothGatt = device.connectGatt (activity, true, gattCallback)
        connectionTimeoutStart ()
    }

    @SuppressLint("ObsoleteSdkInt")
    private fun connected (gatt: BluetoothGatt) {
        Log.d ("Bluetooth", "Device connected (to GATT server)")
        isConnected = true
        statusCallback ()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            gatt.requestConnectionPriority (BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER)
            gatt.requestMtu (DEFAULT_MTU)
        }
//        handler.postDelayed ({
//            Log.e ("Bluetooth", "Device discovery timeout")
//            disconnect ()
//        }, discoveryTimeoutToken, DISCOVERY_TIMEOUT)
        gatt.discoverServices ()
        checker.start ()
    }

    private fun discovered (gatt: BluetoothGatt) {
        Log.d ("Bluetooth", "Device discovery completed")
        val characteristic = gatt.getService (SERVICE_UUID)?.getCharacteristic (CHARACTERISTIC_UUID)
        if (characteristic != null) {
            Log.d ("Bluetooth", "Device notifications enable for ${characteristic.uuid}")
            bluetoothGatt?.setCharacteristicNotification (characteristic, true)
            val descriptor = characteristic.getDescriptor (UUID.fromString ("00002902-0000-1000-8000-00805f9b34fb"))
            descriptor?.let {
                @Suppress("DEPRECATION")
                it.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                bluetoothGatt?.writeDescriptor (it)
            }
        } else {
            Log.e ("Bluetooth", "Device characteristic $CHARACTERISTIC_UUID or service $SERVICE_UUID not found")
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
        connectionTimeoutCancel ()
        handler.removeCallbacksAndMessages (null)
        scanner.stop ()
        checker.stop ()
        if (bluetoothGatt != null) {
            Log.d ("Bluetooth", "Device disconnect")
            bluetoothGatt?.close ()
            bluetoothGatt = null
        }
        isConnected = false
        statusCallback ()
    }

    private fun disconnected () {
        Log.d ("Bluetooth", "Device disconnected (from GATT server)")
        checker.stop ()
        isConnected = false
        statusCallback ()
        reconnect ()
    }

    fun reconnect () {
        Log.d ("Bluetooth", "Device reconnect (forced)")
        disconnect ()
        handler.postDelayed ({
            locate ()
        }, 50)
    }

    fun isConnected (): Boolean = isConnected
}