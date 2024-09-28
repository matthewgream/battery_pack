package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Handler
import android.os.Looper
import android.util.Log
import java.util.UUID

@SuppressLint("MissingPermission")
class BluetoothManager (private val activity: Activity, private val dataCallback: (String) -> Unit, private val statusCallback: () -> Unit) {

    val PERMISSIONS_CODE = 1
    private var permissionsRequested = false
    private var permissionsObtained = false
    private var permissionsAllowed = false
    @SuppressLint("InlinedApi")
    private val permissionsList = arrayOf (android.Manifest.permission.ACCESS_COARSE_LOCATION, android.Manifest.permission.ACCESS_FINE_LOCATION, android.Manifest.permission.BLUETOOTH, android.Manifest.permission.BLUETOOTH_ADMIN, android.Manifest.permission.BLUETOOTH_SCAN, android.Manifest.permission.BLUETOOTH_CONNECT)

    private val DEVICE_NAME = "BatteryMonitor"
    private val SERVICE_UUID = UUID.fromString ("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    private val CHARACTERISTIC_UUID = UUID.fromString ("beb5483e-36e1-4688-b7f5-ea07361b26a8")

    private val bluetoothAdapter: BluetoothAdapter
    private var bluetoothGatt: BluetoothGatt? = null
    private var isConnected = false
    private var isScanning = false
    private val handler = Handler (Looper.getMainLooper ())
    private val SCAN_PERIOD = 30000L // 30 seconds

    //

    private val CONNECTION_TIMEOUT = 30000L // 30 seconds
    private val CONNECTION_CHECK = 5000L // 5 seconds
    private var connectionChecked: Long = 0
    private var connectionCheckerActive = false
    private val connectionCheckerRunner = object : Runnable {
        override fun run() {
            if (isConnected && System.currentTimeMillis () - connectionChecked  > CONNECTION_TIMEOUT) {
                Log.d ("Bluetooth", "No notifications received for 30 seconds, restarting")
                restart ()
            }
            handler.postDelayed (this, CONNECTION_CHECK)
        }
    }
    private fun connectionCheckerEnable () {
        if (!connectionCheckerActive) {
            handler.removeCallbacks (connectionCheckerRunner)
            handler.post (connectionCheckerRunner)
            Log.d ("Bluetooth", "Connection checker enabled")
            connectionCheckerActive = true
        }
    }
    private fun connectionCheckerDisable () {
        if (connectionCheckerActive) {
            handler.removeCallbacks (connectionCheckerRunner)
            Log.d ("Bluetooth", "Connection checker disabled")
            connectionCheckerActive = false
        }
    }
    private fun connectionCheckerPing () {
        connectionChecked = System.currentTimeMillis () // even if not active
    }

    //

    private lateinit var stateReceiverHandler: BroadcastReceiver
    private var stateReceiverActive = false
    private fun stateReceiverSetup () {
        stateReceiverHandler = object : BroadcastReceiver () {
            override fun onReceive (context: Context?, intent: Intent?) {
                when (intent?.action) {
                    BluetoothAdapter.ACTION_STATE_CHANGED -> {
                        when (intent.getIntExtra (BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)) {
                            BluetoothAdapter.STATE_OFF -> {
                                Log.d ("Bluetooth", "Bluetooth turned off, will start")
                                stop ()
                            }
                            BluetoothAdapter.STATE_ON -> {
                                Log.d ("Bluetooth", "Bluetooth turned on, will stop")
                                start ()
                            }
                        }
                    }
                }
            }
        }
    }
    private fun stateReceiverEnable () {
        if (!stateReceiverActive) {
            activity.registerReceiver (stateReceiverHandler, IntentFilter (BluetoothAdapter.ACTION_STATE_CHANGED))
            stateReceiverActive = true
            Log.d ("Bluetooth", "BluetoothStateReceiver enabled")
        }
    }
    private fun stateReceiverDisable () {
        if (stateReceiverActive) {
            activity.unregisterReceiver (stateReceiverHandler)
            stateReceiverActive = false
            Log.d ("Bluetooth", "BluetoothStateReceiver disabled")
        }
    }

    //

    init {
        val bluetoothManager = activity.getSystemService (Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter
        stateReceiverSetup ()
        permissionsRequest ()
    }
    fun onDestroy () {
        stateReceiverDisable ()
        stop ()
    }
    fun onPause () {
        stateReceiverDisable ()
        stop ()
    }
    fun onResume () {
        stateReceiverEnable ()
        start ()
    }

    //

    fun isAvailable (): Boolean {
        return bluetoothAdapter.isEnabled
    }
    fun isPermitted () : Boolean {
        return permissionsAllowed
    }
    fun isConnected (): Boolean {
        return isConnected
    }
    fun start () {
        deviceLocate ()
    }
    fun restart () {
        deviceReconnect ()
    }
    fun stop () {
        deviceDisconnect ()
    }

    //

    private val scanCallback = object : ScanCallback () {
        override fun onScanResult (callbackType: Int, result: ScanResult) {
            val device = result.device
            if (device.name == DEVICE_NAME) {
                Log.d ("Bluetooth", "Scan located, device ${device.name} / ${device.address}")
                scanStop ()
                deviceConnect (device)
            }
        }
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

    //

    private val gattCallback = object : BluetoothGattCallback () {
        override fun onConnectionStateChange (gatt: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> deviceConnected (gatt)
                BluetoothProfile.STATE_DISCONNECTED -> deviceDisconnected ()
            }
        }
        override fun onServicesDiscovered (gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS)
                deviceDiscoveredServices (gatt)
            else
                Log.d ("Bluetooth", "Device discovery error, status $status")
        }
        override fun onCharacteristicChanged (gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            deviceDataReceived (characteristic.uuid, value)
        }
        override fun onMtuChanged (gatt: BluetoothGatt, mtu: Int, status: Int) {
            Log.d("Bluetooth", "MTU changed to: $mtu, status: $status")
        }
    }

    //

    private fun deviceLocate () {
        Log.d ("Bluetooth", "Device locate")
        statusCallback ()
        if (!bluetoothAdapter.isEnabled)
            Log.e ("Bluetooth", "Bluetooth is not enabled")
        else if (!permissionsAllowed)
            Log.e ("Bluetooth", "Bluetooth is not permitted")
        else if (isConnected)
            Log.d ("Bluetooth", "Bluetooth is already connected, will not locate")
        else
            scanStart ()
    }
    private fun deviceConnect (device: BluetoothDevice) {
        Log.d ("Bluetooth", "Device connect, to ${device.name} / ${device.address}")
        bluetoothGatt = device.connectGatt (activity, true, gattCallback)
    }
    private fun deviceConnected (gatt: BluetoothGatt) {
        Log.d ("Bluetooth", "Device connected (to GATT server)")
        isConnected = true
        statusCallback ()
        connectionCheckerPing ()
        gatt.requestMtu (512)
        gatt.discoverServices ()
        connectionCheckerEnable ()
    }
    private fun deviceDiscoveredServices (gatt: BluetoothGatt) {
        Log.d ("Bluetooth", "Device discovery completed")
        val characteristic = gatt.getService (SERVICE_UUID)?.getCharacteristic (CHARACTERISTIC_UUID)
        if (characteristic != null) {
            Log.d ("Bluetooth", "Device notifications enable for ${characteristic.uuid}")
            bluetoothGatt?.setCharacteristicNotification (characteristic, true)
            val descriptor = characteristic.getDescriptor (UUID.fromString ("00002902-0000-1000-8000-00805f9b34fb"))
            descriptor?.let {
                it.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                bluetoothGatt?.writeDescriptor (it)
            }
        } else {
            Log.e ("Bluetooth", "Device characteristic $CHARACTERISTIC_UUID or service $SERVICE_UUID not found")
            deviceDisconnect ()
        }
    }
    private fun deviceDataReceived (uuid: UUID, value: ByteArray) {
        Log.d ("Bluetooth", "Device characteristic changed on ${uuid}: ${String (value)}")
        connectionCheckerPing ()
        try {
            dataCallback (String (value))
        } catch (e: Exception) {
            Log.e ("Bluetooth", "Error processing received data", e)
        }
    }
    private fun deviceDisconnect () {
        handler.removeCallbacksAndMessages(null)
        connectionCheckerDisable ()
        if (bluetoothGatt != null) {
            Log.d ("Bluetooth", "Device disconnect")
            bluetoothGatt?.close ()
            bluetoothGatt = null
        }
        isConnected = false
        statusCallback ()
    }
    private fun deviceReconnect () {
        Log.d ("Bluetooth", "Device reconnect (forced)")
        deviceDisconnect ()
        handler.postDelayed ({
            deviceLocate ()
        }, 50)
    }
    private fun deviceDisconnected () {
        Log.d ("Bluetooth", "Device disconnected (from GATT server)")
        connectionCheckerDisable ()
        isConnected = false
        statusCallback ()
        deviceReconnect ()
    }

    //

    private fun onPermissionsAllowed () {
        handler.postDelayed ({
            start ()
        }, 50)
    }
    @SuppressLint("InlinedApi")
    private fun permissionsRequest () {
        permissionsRequested = true
        if (!permissionsList.all { activity.checkSelfPermission (it) == PackageManager.PERMISSION_GRANTED }) {
            Log.d ("Bluetooth", "Permissions request")
            activity.requestPermissions (permissionsList, PERMISSIONS_CODE)
        } else {
            Log.d ("Bluetooth", "Permissions granted already")
            permissionsObtained = true
            permissionsAllowed = true
            onPermissionsAllowed ()
        }
    }
    fun permissionsHandler (grantResults: IntArray) {
        permissionsObtained = true
        if (grantResults.isNotEmpty () && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
            Log.d ("Bluetooth", "Permissions granted")
            permissionsAllowed = true
            onPermissionsAllowed ()
        } else {
            Log.d("Bluetooth", "Permissions denied")
        }
    }

}
