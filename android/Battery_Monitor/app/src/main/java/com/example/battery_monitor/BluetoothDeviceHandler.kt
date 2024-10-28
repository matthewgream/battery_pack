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
    activity: Activity,
    adapter: AdapterBluetooth,
    private val config: BluetoothDeviceConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    statusCallback: () -> Unit,
    isAvailable: () -> Boolean,
    isPermitted: () -> Boolean
) : ConnectivityDeviceHandler(tag, statusCallback, config.connectionActiveCheck, config.connectionActiveTimeout, isAvailable, isPermitted) {

    //

    private class BluetoothDeviceConnection(
        private val tag: String,
        private val activity: Activity,
        private val config: BluetoothDeviceConfig,
        @Suppress("unused") private val onKeepalive: (BluetoothDeviceConnection) -> Unit,
        private val onConnected: (BluetoothDeviceConnection) -> Unit,
        private val onDisconnected: (BluetoothDeviceConnection) -> Unit,
        private val onError: (BluetoothDeviceConnection) -> Unit,
        private val onDiscovered: (BluetoothDeviceConnection) -> Unit,
        private val onReceived: (BluetoothDeviceConnection, UUID, String) -> Unit
    ) {
        private var gatt: BluetoothGatt? = null
        private inline fun <T> gattOperation(operation: String, block: () -> T?): T? {
            return try {
                block()?.also {
                    Log.d(tag, "GATT $operation: success")
                }
            } catch (e: Exception) {
                Log.e(tag, "GATT $operation: exception", e)
                null
            }
        }
        private val callback = object : BluetoothGattCallback() {
            override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
                when (status) {
                    BluetoothGatt.GATT_SUCCESS -> {
                        when (newState) {
                            BluetoothProfile.STATE_CONNECTED -> onConnected(this@BluetoothDeviceConnection)
                            BluetoothProfile.STATE_DISCONNECTED -> onDisconnected(this@BluetoothDeviceConnection)
                        }
                        return
                    }
                    else -> Log.e(tag, "onConnectionStateChange: error=$status")
                }
                onError(this@BluetoothDeviceConnection)
            }
            override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e(tag, "onServicesDiscovered: error=$status")
                    onError(this@BluetoothDeviceConnection)
                } else onDiscovered(this@BluetoothDeviceConnection)
            }
            override fun onCharacteristicWrite(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?, status: Int) {
                super.onCharacteristicWrite(gatt, characteristic, status)
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e(tag, "onCharacteristicWrite: error=$status")
                    onError(this@BluetoothDeviceConnection)
                }
            }
            override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
                onReceived(this@BluetoothDeviceConnection, characteristic.uuid, String(value))
            }
            override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    Log.e(tag, "onMtuChanged: error=$status")
                    onError(this@BluetoothDeviceConnection)
                } else Log.d(tag, "onMtuChanged: mtu=$mtu")
            }
        }
        fun connect(device: BluetoothDevice): Boolean = gattOperation("connect") {
            device.connectGatt(activity, true, callback).also { gatt = it }
        } != null
        fun discover(): Boolean = gattOperation("discover") {
            gatt?.apply {
                requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER)
                requestMtu(config.connectionMtu)
                discoverServices()
            }
        } != null
        fun enableNotifications(): Boolean = gattOperation("notifications") {
            gatt?.let { gatt ->
                gatt.getService(config.serviceUuid)
                    ?.getCharacteristic(config.characteristicUuid)
                    ?.let { characteristic ->
                        val descriptor = characteristic.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
                        when {
                            !gatt.setCharacteristicNotification(characteristic, true) -> {
                                Log.e(tag, "Failed to set characteristic notification")
                                null
                            }
                            gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) != BluetoothStatusCodes.SUCCESS -> {
                                Log.e(tag, "Failed to write descriptor")
                                null
                            }
                            else -> true
                        }
                    }
            }
        } ?: false
        fun write(value: String): Boolean = gattOperation("write") {
            gatt?.let { gatt ->
                gatt.getService(config.serviceUuid)
                    ?.getCharacteristic(config.characteristicUuid)
                    ?.let { characteristic ->
                        when {
                            gatt.writeCharacteristic(characteristic, value.toByteArray(StandardCharsets.UTF_8), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS -> true
                            else -> {
                                Log.e(tag, "Failed to write characteristic")
                                null
                            }
                        }
                    }
            }
        } ?: false
        fun disconnect() {
            gattOperation("disconnect") {
                gatt?.close()
            }
            gatt = null
        }
    }

    //

    private val connection = BluetoothDeviceConnection(tag, activity,
        config,
        onDiscovered = {
            Log.d(tag, "Device discovered")
            if (it.enableNotifications()) {
                Log.d(tag, "Device notifications enabled on ${config.characteristicUuid}")
                setConnectionIsConnected()
            } else {
                Log.e(tag, "Device service ${config.serviceUuid} or characteristic ${config.characteristicUuid} not found")
                setConnectionDoReconnect()
            }
        },
        onConnected = {
            Log.d(tag, "Device connected")
            setConnectionIsActive()
            it.discover().let { discovered ->
                if (!discovered) setConnectionDoReconnect()
            }
        },
        onReceived = { _, uuid, value ->
            Log.d(tag, "Device received ($uuid): $value")
            setConnectionIsActive()
            dataCallback(value)
        },
        onKeepalive = {
            setConnectionIsActive()
        },
        onDisconnected = {
            Log.d(tag, "Device disconnected")
            setConnectionDoReconnect()
        },
        onError = {
            Log.d(tag, "Device error")
            setConnectionDoReconnect()
        },
    )

    private val scanner = BluetoothDeviceScanner("${tag}Scanner",
        adapter,
        BluetoothDeviceScanner.Config(
            config.deviceName,
            config.connectionScanDelay,
            config.connectionScanPeriod,
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
        onFound = { device ->
            Log.d(tag, "Device located '${device.name}'/${device.address}")
            Log.d(tag, "Device connecting: '${device.name}'/${device.address}")
            setConnectionIsActive()
            if (!connection.connect(device)) setConnectionDoReconnect()
        }
    )

    //

    override fun doConnectionStart(): Boolean {
        Log.d(tag, "Device locate")
        scanner.start()
        return true
    }
    override fun doConnectionStop() {
        Log.d(tag, "Device disconnect")
        connection.disconnect()
        scanner.stop()
    }
    override fun doConnectionIdentify(): Boolean {
        return connection.write(connectivityInfo.toJsonString())
    }
}