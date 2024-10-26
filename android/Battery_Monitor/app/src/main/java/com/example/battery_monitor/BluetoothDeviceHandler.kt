package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.os.Handler
import android.os.Looper
import android.util.Log
import java.nio.charset.StandardCharsets
import java.util.UUID

@Suppress("PropertyName")
class BluetoothDeviceHandlerConfig(
    val DEVICE_NAME: String = "BatteryMonitor",
    val SERVICE_UUID: UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b"),
    val CHARACTERISTIC_UUID: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8"),
    val MTU: Int = 517, // maximum
    val CONNECTION_TIMEOUT: Long = 30000L // 30 seconds
)

@SuppressLint("MissingPermission")
class BluetoothDeviceHandler(
    private val activity: Activity,
    adapter: BluetoothDeviceAdapter,
    private val config: BluetoothDeviceHandlerConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    private val statusCallback: () -> Unit,
    private val isPermitted: () -> Boolean,
    private val isEnabled: () -> Boolean
) : ConnectivityDeviceHandler() {

    private val handler = Handler(Looper.getMainLooper())

    private val scanner: BluetoothDeviceScanner = BluetoothDeviceScanner(adapter.adapter.bluetoothLeScanner, BluetoothDeviceScannerConfig(config.DEVICE_NAME, config.SERVICE_UUID),
        onFound = { device -> located(device) })
    private val checker: ConnectivityChecker = ConnectivityChecker("Bluetooth", ConnectivityCheckerConfig(CONNECTION_TIMEOUT = config.CONNECTION_TIMEOUT, CONNECTION_CHECK = 10000L),
        isConnected = { isConnected() }, onTimeout = { reconnect() })

    //

    private var bluetoothGatt: BluetoothGatt? = null
    private fun bluetoothCreate () : BluetoothGattCallback {
        return object : BluetoothGattCallback() {
            override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
                when (status) {
                    BluetoothGatt.GATT_SUCCESS -> {
                        when (newState) {
                            BluetoothProfile.STATE_CONNECTED -> connected()
                            BluetoothProfile.STATE_DISCONNECTED -> disconnected()
                        }
                        return
                    }

                    0x3E -> Log.e("Bluetooth", "onConnectionStateChange: terminated by local host")
                    0x3B -> Log.e("Bluetooth", "onConnectionStateChange: terminated by remote device")
                    0x85 -> Log.e("Bluetooth", "onConnectionStateChange: timed out")
                    else -> Log.e("Bluetooth", "onConnectionStateChange: state change error: $status")
                }
                disconnect()
            }
            override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e("Bluetooth", "onServicesDiscovered: error=$status")
                    disconnect()
                } else
                    discovered()
            }
            override fun onCharacteristicWrite(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?, status: Int) {
                super.onCharacteristicWrite(gatt, characteristic, status)
                if (status != BluetoothGatt.GATT_SUCCESS)
                    Log.e("Bluetooth", "onCharacteristicWrite: error=$status")
            }
            override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
                received(characteristic.uuid, String (value))
            }
            override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e("Bluetooth", "onMtuChanged: error=$status")
                    disconnect()
                } else
                    Log.d("Bluetooth", "onMtuChanged: mtu=$mtu")
            }
        }
    }
    private fun bluetoothConnect (device: BluetoothDevice) {
        try {
            bluetoothGatt = device.connectGatt(activity, true, bluetoothCreate ())
        } catch (e: Exception) {
            Log.e("Bluetooth", "GATT connect: error=${e.message}")
        }
    }
    private fun bluetoothDiscover () : Boolean {
        val gatt = bluetoothGatt ?: return false
        try {
            gatt.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER)
            gatt.requestMtu(config.MTU)
            gatt.discoverServices()
            return true
        } catch (e: Exception) {
            Log.e("Bluetooth", "GATT request priority/MTU or discover services: error=${e.message}")
            return false
        }
    }
    private fun bluetoothNotificationsEnable (): Boolean {
        val gatt = bluetoothGatt ?: return false
        try {
            val service = gatt.getService(config.SERVICE_UUID) ?: return false
            val characteristic = service.getCharacteristic(config.CHARACTERISTIC_UUID) ?: return false
            val descriptor = characteristic.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
            if (!gatt.setCharacteristicNotification(characteristic, true)) {
                Log.e("Bluetooth", "GATT notifications failed to set")
                return false
            }
            if (gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) !=  BluetoothStatusCodes.SUCCESS) {
                Log.e("Bluetooth", "GATT descriptor failed to write")
                return false
            }
            return true
        } catch (e: Exception) {
            Log.e("Bluetooth", "GATT notifications: error=${e.message}")
            return false
        }
    }
    private fun bluetoothWrite (value: String): Boolean {
        val gatt = bluetoothGatt ?: return false
        try {
            val service = gatt.getService(config.SERVICE_UUID)
            if (service == null) {
                Log.e("Bluetooth", "GATT service not found")
                return false
            }
            val characteristic = service.getCharacteristic(config.CHARACTERISTIC_UUID)
            if (characteristic == null) {
                Log.e("Bluetooth", "GATT characteristic not found")
                return false
            }
            if (gatt.writeCharacteristic(characteristic, value.toByteArray(StandardCharsets.UTF_8), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) !=  BluetoothStatusCodes.SUCCESS) {
                Log.e("Bluetooth", "GATT characteristic failed to write")
                return false
            }
            return true
        } catch (e: Exception) {
            Log.e("Bluetooth", "GATT set characteristic: error=${e.message}")
            return false
        }
    }
    private fun bluetoothDisconnect () {
        val gatt = bluetoothGatt ?: return
        try {
            gatt.close()
        } catch (e: Exception) {
            Log.e("Bluetooth", "GATT close: error=${e.message}")
        }
        bluetoothGatt = null
    }

    //

    private var isConnecting = false
    private var isConnected = false

    override fun permitted() {
        statusCallback()
        if (!isConnecting && !isConnected)
            locate()
    }
    private fun locate() {
        Log.d("Bluetooth", "Device locate initiated")
        statusCallback()
        when {
            !isEnabled() -> Log.e("Bluetooth", "Device not enabled or available")
            !isPermitted() -> Log.e("Bluetooth", "Device access not permitted")
            isConnecting -> Log.d("Bluetooth", "Device connection already in progress")
            isConnected -> Log.d("Bluetooth", "Device connection already active, will not locate")
            else -> {
                isConnecting = true
                scanner.start()
            }
        }
    }
    private fun located(device: BluetoothDevice) {
        Log.d("Bluetooth", "Device located '${device.name}'/${device.address}")
        connect(device)
    }
    private fun connect(device: BluetoothDevice) {
        Log.d("Bluetooth", "Device connect to '${device.name}'/${device.address}")
        statusCallback()
        checker.start()
        bluetoothConnect(device)
    }
    private fun connected() {
        Log.d("Bluetooth", "Device connected")
        checker.ping()
        bluetoothDiscover()
    }
    private fun discovered() {
        Log.d("Bluetooth", "Device discovery completed")
        isConnected = true
        isConnecting = false
        statusCallback()
        if (bluetoothNotificationsEnable ()) {
            Log.d("Bluetooth", "Device notifications enabled on ${config.CHARACTERISTIC_UUID}")
            handler.postDelayed({
                identify()
            }, 1000)
        } else {
            Log.e("Bluetooth", "Device service ${config.SERVICE_UUID} or characteristic ${config.CHARACTERISTIC_UUID} not found")
            disconnect()
        }
    }
    private fun identify() {
        bluetoothWrite (connectivityInfo.toJsonString())
    }
    private fun received(uuid: UUID, value: String) {
        Log.d("Bluetooth", "Device received on $uuid: $value")
        checker.ping()
        dataCallback(value)
    }
    private fun disconnect() {
        Log.d("Bluetooth", "Device disconnect")
        bluetoothDisconnect()
        scanner.stop()
        checker.stop()
        isConnected = false
        isConnecting = false
        statusCallback()
    }
    override fun disconnected() {
        Log.d("Bluetooth", "Device disconnected")
        bluetoothDisconnect()
        scanner.stop()
        checker.stop()
        isConnected = false
        isConnecting = false
        statusCallback()
        handler.postDelayed({
            locate()
        }, 1000)
    }
    override fun reconnect() {
        Log.d("Bluetooth", "Device reconnect")
        disconnect()
        handler.postDelayed({
            locate()
        }, 1000)
    }
    override fun isConnected(): Boolean = isConnected
}
