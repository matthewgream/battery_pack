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
import android.os.ParcelUuid
import android.util.Log
import java.nio.charset.StandardCharsets
import java.util.UUID

@SuppressLint("MissingPermission")
class BluetoothDeviceHandler(
    tag: String,
    private val activity: Activity,
    adapter: AdapterBluetooth,
    private val config: BluetoothDeviceConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    statusCallback: () -> Unit,
    isPermitted: () -> Boolean,
    isEnabled: () -> Boolean
) : ConnectivityDeviceHandler(tag, statusCallback, config.connectionActiveCheck, config.connectionActiveTimeout, isPermitted, isEnabled) {

    //

    private var bluetoothGatt: BluetoothGatt? = null
    private fun bluetoothCreate() : BluetoothGattCallback {
        return object : BluetoothGattCallback() {
            override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
                when (status) {
                    BluetoothGatt.GATT_SUCCESS -> {
                        when (newState) {
                            BluetoothProfile.STATE_CONNECTED -> onBluetoothConnected()
                            BluetoothProfile.STATE_DISCONNECTED -> onBluetoothDisconnected()
                        }
                        return
                    }
                    0x3E -> Log.e(tag, "onConnectionStateChange: terminated by local host")
                    0x3B -> Log.e(tag, "onConnectionStateChange: terminated by remote device")
                    0x85 -> Log.e(tag, "onConnectionStateChange: timed out")
                    else -> Log.e(tag, "onConnectionStateChange: error=$status")
                }
                onBluetoothError()
            }
            override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e(tag, "onServicesDiscovered: error=$status")
                    onBluetoothError()
                } else
                    onBluetoothDiscovered()
            }
            override fun onCharacteristicWrite(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?, status: Int) {
                super.onCharacteristicWrite(gatt, characteristic, status)
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e(tag, "onCharacteristicWrite: error=$status")
                    onBluetoothError()
                }
            }
            override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
                onBluetoothReceived(characteristic.uuid, String (value))
            }
            override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e(tag, "onMtuChanged: error=$status")
                    onBluetoothError()
                } else
                    Log.d(tag, "onMtuChanged: mtu=$mtu")
            }
        }
    }
    private fun bluetoothConnect(device: BluetoothDevice) : Boolean {
        try {
            bluetoothGatt = device.connectGatt(activity, true, bluetoothCreate ())
            return true
        } catch (e: Exception) {
            Log.e(tag, "GATT connect: exception", e)
        }
        return false
    }
    private fun bluetoothDiscover() : Boolean {
        val gatt = bluetoothGatt ?: return false
        try {
            gatt.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER)
            gatt.requestMtu(config.connectionMtu)
            gatt.discoverServices()
            return true
        } catch (e: Exception) {
            Log.e(tag, "GATT request priority/MTU or discover services: exception", e)
        }
        return false
    }
    private fun bluetoothNotificationsEnable(): Boolean {
        val gatt = bluetoothGatt ?: return false
        try {
            val service = gatt.getService(config.serviceUuid) ?: return false
            val characteristic = service.getCharacteristic(config.characteristicUuid) ?: return false
            val descriptor = characteristic.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
            if (!gatt.setCharacteristicNotification(characteristic, true)) {
                Log.e(tag, "GATT notifications failed to set")
                return false
            }
            if (gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) != BluetoothStatusCodes.SUCCESS) {
                Log.e(tag, "GATT descriptor failed to write")
                return false
            }
            return true
        } catch (e: Exception) {
            Log.e(tag, "GATT notifications: exception", e)
        }
        return false
    }
    private fun bluetoothWrite(value: String): Boolean {
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
            if (gatt.writeCharacteristic(characteristic, value.toByteArray(StandardCharsets.UTF_8), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) != BluetoothStatusCodes.SUCCESS) {
                Log.e(tag, "GATT characteristic failed to write")
                return false
            }
            return true
        } catch (e: Exception) {
            Log.e(tag, "GATT set characteristic: exception", e)
        }
        return false
    }
    private fun bluetoothDisconnect() {
        val gatt = bluetoothGatt ?: return
        try {
            gatt.close()
        } catch (e: Exception) {
            Log.e(tag, "GATT close: exception", e)
        }
        bluetoothGatt = null
    }

    private val bluetoothScanner: BluetoothDeviceScanner = BluetoothDeviceScanner("${tag}Scanner", adapter,
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
        onFound = { device -> onBluetoothLocated(device) })

    //

    override fun doConnectionStart() : Boolean {
        Log.d(tag, "Device locate")
        bluetoothScanner.start()
        return true
    }
    override fun doConnectionStop() {
        Log.d(tag, "Device disconnect")
        bluetoothDisconnect()
        bluetoothScanner.stop()
    }
    override fun doConnectionIdentify() : Boolean  {
        return bluetoothWrite(connectivityInfo.toJsonString())
    }

    //

    private fun onBluetoothLocated(device: BluetoothDevice) {
        Log.d(tag, "Device located '${device.name}'/${device.address}")
        Log.d(tag, "Device connecting: '${device.name}'/${device.address}")
        setConnectionIsActive()
        if (!bluetoothConnect(device))
            setConnectionDoReconnect()
    }
    private fun onBluetoothConnected() {
        Log.d(tag, "Device connected")
        setConnectionIsActive()
        if (!bluetoothDiscover())
            setConnectionDoReconnect()
    }
    private fun onBluetoothDisconnected() {
        Log.d(tag, "Device disconnected")
        setConnectionDoReconnect()
    }
    private fun onBluetoothDiscovered() {
        Log.d(tag, "Device discovered")
        if (bluetoothNotificationsEnable()) {
            Log.d(tag, "Device notifications enabled on ${config.characteristicUuid}")
            setConnectionIsConnected()
        } else {
            Log.e(tag, "Device service ${config.serviceUuid} or characteristic ${config.characteristicUuid} not found")
            setConnectionDoReconnect()
        }
    }
    private fun onBluetoothError() {
        Log.d(tag, "Device error")
        setConnectionDoReconnect()
    }
    private fun onBluetoothReceived(uuid: UUID, value: String) {
        Log.d(tag, "Device received ($uuid): $value")
        setConnectionIsActive()
        dataCallback(value)
    }
}
