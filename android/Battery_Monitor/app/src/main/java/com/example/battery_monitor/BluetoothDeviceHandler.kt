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
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanSettings
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import java.nio.charset.StandardCharsets
import java.util.UUID

@SuppressLint("MissingPermission")
class BluetoothDeviceHandler(
    private val tag: String,
    private val activity: Activity,
    adapter: AdapterBluetooth,
    private val config: BluetoothDeviceConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    private val statusCallback: () -> Unit,
    private val isPermitted: () -> Boolean,
    private val isEnabled: () -> Boolean
) : ConnectivityDeviceHandler() {

    private val handler = Handler(Looper.getMainLooper())

    private val scanner: BluetoothDeviceScanner = BluetoothDeviceScanner(tag, adapter,
        BluetoothDeviceScanner.Config (config.deviceName, config.connectionScanDelay, config.connectionScanPeriod,
            ScanFilter.Builder()
                .setDeviceName(config.deviceName)
                .setServiceUuid(ParcelUuid(config.serviceUuid))
                .build(),
            ScanSettings.Builder()
                .setLegacy(false)
                .setPhy(ScanSettings.PHY_LE_ALL_SUPPORTED)
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .setScanMode(ScanSettings.SCAN_MODE_LOW_POWER)
                .setCallbackType(ScanSettings.CALLBACK_TYPE_FIRST_MATCH)
                .setMatchMode(ScanSettings.MATCH_MODE_AGGRESSIVE)
                .setNumOfMatches(ScanSettings.MATCH_NUM_ONE_ADVERTISEMENT)
                .setReportDelay(0L)
                .build()
        ),
        onFound = { device -> located(device) })
    private val checker: ConnectivityChecker = ConnectivityChecker(tag, config.connectionActiveCheck, config.connectionActiveTimeout,
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
                    0x3E -> Log.e(tag, "onConnectionStateChange: terminated by local host")
                    0x3B -> Log.e(tag, "onConnectionStateChange: terminated by remote device")
                    0x85 -> Log.e(tag, "onConnectionStateChange: timed out")
                    else -> Log.e(tag, "onConnectionStateChange: error=$status")
                }
                reconnect()
            }
            override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e(tag, "onServicesDiscovered: error=$status")
                    reconnect()
                } else
                    discovered()
            }
            override fun onCharacteristicWrite(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?, status: Int) {
                super.onCharacteristicWrite(gatt, characteristic, status)
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e(tag, "onCharacteristicWrite: error=$status")
                    reconnect()
                }
            }
            override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
                received(characteristic.uuid, String (value))
            }
            override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e(tag, "onMtuChanged: error=$status")
                    reconnect()
                } else
                    Log.d(tag, "onMtuChanged: mtu=$mtu")
            }
        }
    }
    private fun bluetoothConnect (device: BluetoothDevice) {
        try {
            bluetoothGatt = device.connectGatt(activity, true, bluetoothCreate ())
        } catch (e: Exception) {
            Log.e(tag, "GATT connect: error=${e.message}")
        }
    }
    private fun bluetoothDiscover () : Boolean {
        val gatt = bluetoothGatt ?: return false
        try {
            gatt.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER)
            gatt.requestMtu(config.connectionMtu)
            gatt.discoverServices()
            return true
        } catch (e: Exception) {
            Log.e(tag, "GATT request priority/MTU or discover services: error=${e.message}")
            return false
        }
    }
    private fun bluetoothNotificationsEnable (): Boolean {
        val gatt = bluetoothGatt ?: return false
        try {
            val service = gatt.getService(config.serviceUuid) ?: return false
            val characteristic = service.getCharacteristic(config.characteristicUuid) ?: return false
            val descriptor = characteristic.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
            if (!gatt.setCharacteristicNotification(characteristic, true)) {
                Log.e(tag, "GATT notifications failed to set")
                return false
            }
            if (gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) !=  BluetoothStatusCodes.SUCCESS) {
                Log.e(tag, "GATT descriptor failed to write")
                return false
            }
            return true
        } catch (e: Exception) {
            Log.e(tag, "GATT notifications: error=${e.message}")
            return false
        }
    }
    private fun bluetoothWrite (value: String): Boolean {
        val gatt = bluetoothGatt ?: return false
        try {
            val service = gatt.getService(config.serviceUuid)
            if (service == null) {
                Log.e(tag, "GATT service not found")
                return false
            }
            val characteristic = service.getCharacteristic(config.characteristicUuid)
            if (characteristic == null) {
                Log.e(tag, "GATT characteristic not found")
                return false
            }
            if (gatt.writeCharacteristic(characteristic, value.toByteArray(StandardCharsets.UTF_8), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) !=  BluetoothStatusCodes.SUCCESS) {
                Log.e(tag, "GATT characteristic failed to write")
                return false
            }
            return true
        } catch (e: Exception) {
            Log.e(tag, "GATT set characteristic: error=${e.message}")
            return false
        }
    }
    private fun bluetoothDisconnect () {
        val gatt = bluetoothGatt ?: return
        try {
            gatt.close()
        } catch (e: Exception) {
            Log.e(tag, "GATT close: error=${e.message}")
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
        Log.d(tag, "Device locate initiated")
        statusCallback()
        when {
            !isEnabled() -> Log.e(tag, "Device not enabled or available")
            !isPermitted() -> Log.e(tag, "Device access not permitted")
            isConnecting -> Log.d(tag, "Device connection already in progress")
            isConnected -> Log.d(tag, "Device connection already active, will not locate")
            else -> {
                isConnecting = true
                scanner.start()
            }
        }
    }
    private fun located(device: BluetoothDevice) {
        Log.d(tag, "Device located '${device.name}'/${device.address}")
        connect(device)
    }
    private fun connect(device: BluetoothDevice) {
        Log.d(tag, "Device connect to '${device.name}'/${device.address}")
        statusCallback()
        checker.start()
        bluetoothConnect(device)
    }
    private fun connected() {
        Log.d(tag, "Device connected")
        checker.ping()
        bluetoothDiscover()
    }
    private fun discovered() {
        Log.d(tag, "Device discovered")
        checker.ping()
        isConnected = true
        isConnecting = false
        statusCallback()
        if (bluetoothNotificationsEnable ()) {
            Log.d(tag, "Device notifications enabled on ${config.characteristicUuid}")
            handler.postDelayed({
                identify()
            }, 1000)
        } else {
            Log.e(tag, "Device service ${config.serviceUuid} or characteristic ${config.characteristicUuid} not found")
            disconnected()
        }
    }
    private fun identify() {
        bluetoothWrite (connectivityInfo.toJsonString())
    }
    private fun received(uuid: UUID, value: String) {
        Log.d(tag, "Device received ($uuid): $value")
        checker.ping()
        dataCallback(value)
    }
    private fun disconnect() {
        Log.d(tag, "Device disconnect")
        bluetoothDisconnect()
        scanner.stop()
        checker.stop()
        isConnected = false
        isConnecting = false
        statusCallback()
    }
    override fun disconnected() {
        Log.d(tag, "Device disconnected")
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
        Log.d(tag, "Device reconnect")
        disconnect()
        handler.postDelayed({
            locate()
        }, 1000)
    }
    override fun isConnected(): Boolean = isConnected
}
