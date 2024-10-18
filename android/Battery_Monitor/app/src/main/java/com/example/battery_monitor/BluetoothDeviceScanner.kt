package com.example.battery_monitor

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.os.ParcelUuid
import android.util.Log
import java.util.UUID

/*
class AdaptiveBluetoothScanner(private val bluetoothAdapter: BluetoothAdapter) {
    private var scanInterval = 5000L // Start with 5 seconds
    private var scanDuration = 10000L // Start with 10 seconds
    private val maxInterval = 5 * 60 * 1000L // 5 minutes
    private val minInterval = 1000L // 1 second
    private var deviceFoundRecently = false

    private val handler = Handler(Looper.getMainLooper())
    private var isScanning = false

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            // Device found logic
            deviceFoundRecently = true
            decreaseScanInterval()
        }
    }

    fun startAdaptiveScanning() {
        handler.post(object : Runnable {
            override fun run() {
                if (isScanning) {
                    stopScan()
                    if (!deviceFoundRecently) {
                        increaseScanInterval()
                    }
                    deviceFoundRecently = false
                    handler.postDelayed(this, scanInterval)
                } else {
                    startScan()
                    handler.postDelayed(this, scanDuration)
                }
            }
        })
    }

    private fun startScan() {
        isScanning = true
        bluetoothAdapter.bluetoothLeScanner.startScan(scanCallback)
    }

    private fun stopScan() {
        isScanning = false
        bluetoothAdapter.bluetoothLeScanner.stopScan(scanCallback)
    }

    private fun increaseScanInterval() {
        scanInterval = (scanInterval * 1.5).toLong().coerceAtMost(maxInterval)
    }

    private fun decreaseScanInterval() {
        scanInterval = (scanInterval * 0.7).toLong().coerceAtLeast(minInterval)
    }
}

class DutyCycleScanner(private val bluetoothAdapter: BluetoothAdapter) {
    private val SCAN_PERIOD_ACTIVE = 10000L // 10 seconds
    private val SCAN_PERIOD_IDLE = 50000L // 50 seconds
    private val handler = Handler(Looper.getMainLooper())

    private val scanCallback = object : ScanCallback() {
        // Implement callback methods
    }

    fun startDutyCycleScanning() {
        scanCycle()
    }

    private fun scanCycle() {
        startScan()
        handler.postDelayed({
            stopScan()
            handler.postDelayed({ scanCycle() }, SCAN_PERIOD_IDLE)
        }, SCAN_PERIOD_ACTIVE)
    }

    private fun startScan() {
        bluetoothAdapter.bluetoothLeScanner.startScan(scanCallback)
    }

    private fun stopScan() {
        bluetoothAdapter.bluetoothLeScanner.stopScan(scanCallback)
    }
}

*/

@Suppress("PropertyName")
class BluetoothDeviceScannerConfig (
    val name: String,
    uuid: UUID,
) {
    val SCAN_DELAY = 5000L // 5 seconds
    val SCAN_PERIOD = 30000L // 30 seconds

    val filter: ScanFilter = ScanFilter.Builder ()
        .setDeviceName (name)
        .setServiceUuid (ParcelUuid (uuid))
        .build ()

    val settings: ScanSettings = ScanSettings.Builder ()
        .setLegacy (false)
        .setPhy (ScanSettings.PHY_LE_ALL_SUPPORTED)
        .setScanMode (ScanSettings.SCAN_MODE_LOW_LATENCY)
        .setScanMode (ScanSettings.SCAN_MODE_LOW_POWER)
        .setCallbackType (ScanSettings.CALLBACK_TYPE_FIRST_MATCH)
        .setMatchMode (ScanSettings.MATCH_MODE_AGGRESSIVE)
        .setNumOfMatches (ScanSettings.MATCH_NUM_ONE_ADVERTISEMENT)
        .setReportDelay (0L)
        .build ()
}

@SuppressLint("MissingPermission")
class BluetoothDeviceScanner (
    private val scanner: BluetoothLeScanner,
    private val config: BluetoothDeviceScannerConfig,
    private val onFound: (BluetoothDevice) -> Unit
) : ConnectivityComponent ("BluetoothDeviceScanner") {

    private val retryRunnable = Runnable {
        start ()
    }

    private val callback = object : ScanCallback () {
        override fun onScanResult (callbackType: Int, result: ScanResult) {
            if (result.scanRecord?.deviceName == config.name) {
                stop ()
                Log.d (tag, "Device scan located, device ${result.scanRecord?.deviceName} / ${result.device.address} [txPower=${result.scanRecord?.txPowerLevel}, rssi=${result.rssi}]")
                onFound (result.device)
            }
        }
        override fun onScanFailed (errorCode: Int) {
            when (errorCode) {
                SCAN_FAILED_ALREADY_STARTED -> Log.e (tag, "Device scan failed: already started")
                SCAN_FAILED_APPLICATION_REGISTRATION_FAILED -> Log.e ("Bluetooth", "Device scan failed: application registration failed")
                SCAN_FAILED_FEATURE_UNSUPPORTED -> Log.e (tag, "Device scan failed: feature unsupported")
                SCAN_FAILED_INTERNAL_ERROR -> Log.e (tag, "Device scan failed: internal error")
                else -> Log.e (tag, "Device scan failed: error $errorCode")
            }
            restartAfterDelay ()
        }
    }

//    private val SCAN_PERIOD = 10000L // 10 seconds
//    private val SCAN_INTERVAL = 60000L // 1 minute
//    fun dutyCycleScan () {
//        start ()
//        handler.postDelayed ({
//            stop ()
//            handler.postDelayed ({ dutyCycleScan () }, SCAN_INTERVAL - SCAN_PERIOD)
//        }, SCAN_PERIOD)
//    }

    private fun restartAfterDelay () {
        stop ()
        handler.postDelayed (retryRunnable, config.SCAN_DELAY)
    }

    override val timer: Long
        get () = config.SCAN_PERIOD

    override fun onStart () {
        scanner.startScan (listOf (config.filter), config.settings, callback)
    }
    override fun onStop () {
        handler.removeCallbacks (retryRunnable)
        scanner.stopScan (callback)
    }
    override fun onTimer () : Boolean {
        restartAfterDelay ()
        return false
    }
}
