package com.example.battery_monitor

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import java.util.UUID

@SuppressLint("MissingPermission")
@Suppress("PrivatePropertyName")
class BluetoothDeviceScanner (
    private val scanner: BluetoothLeScanner,
    private val name: String,
    uuid: UUID,
    private val onFound: (BluetoothDevice) -> Unit
) {
    private val handler = Handler (Looper.getMainLooper ())
    private var scanTimeoutRunnable: Runnable? = null
    private var scanRetryRunnable: Runnable? = null

    private val filter = ScanFilter.Builder ()
        .setDeviceName (name)
        .setServiceUuid (ParcelUuid (uuid))
        .build ()
    private val settings = ScanSettings.Builder ()
        .setLegacy (false)
        .setScanMode (ScanSettings.SCAN_MODE_LOW_LATENCY)
        .setScanMode (ScanSettings.SCAN_MODE_LOW_POWER)
        .setCallbackType (ScanSettings.CALLBACK_TYPE_FIRST_MATCH)
        .setMatchMode (ScanSettings.MATCH_MODE_AGGRESSIVE)
        .setNumOfMatches (ScanSettings.MATCH_NUM_ONE_ADVERTISEMENT)
        .setReportDelay (0L)
        .build ()

    private val SCAN_DELAY = 5000L // 5 seconds
    private val SCAN_PERIOD = 30000L // 30 seconds
//    private val SCAN_PERIOD = 10000L // 10 seconds
//    private val SCAN_INTERVAL = 60000L // 1 minute

    private var isScanning = false

    private val callback = object : ScanCallback () {
        override fun onScanResult (callbackType: Int, result: ScanResult) {
            val device = result.device
            if (device.name == name) {
                val deviceName = result.scanRecord?.deviceName
                val txPower = result.scanRecord?.txPowerLevel
                val rssi = result.rssi
                Log.d ("Bluetooth", "Device scan located, device ${device.name} / ${device.address} [deviceName=$deviceName, txPower=$txPower, rssi=$rssi]")
                stop ()
                onFound (device)
            }
        }
        override fun onScanFailed (errorCode: Int) {
            when (errorCode) {
                SCAN_FAILED_ALREADY_STARTED -> Log.e ("Bluetooth", "Device scan failed: already started")
                SCAN_FAILED_APPLICATION_REGISTRATION_FAILED -> Log.e ("Bluetooth", "Device scan failed: application registration failed")
                SCAN_FAILED_FEATURE_UNSUPPORTED -> Log.e ("Bluetooth", "Device scan failed: feature unsupported")
                SCAN_FAILED_INTERNAL_ERROR -> Log.e ("Bluetooth", "Device scan failed: internal error")
                else -> Log.e ("Bluetooth", "Device scan failed: error $errorCode")
            }
            stop ()
            scanRetryRunnable = Runnable { start() }
            handler.postDelayed (scanRetryRunnable!!, SCAN_DELAY)
        }
    }

//    fun dutyCycleScan () {
//        start ()
//        handler.postDelayed ({
//            stop ()
//            handler.postDelayed ({ dutyCycleScan () }, SCAN_INTERVAL - SCAN_PERIOD)
//        }, SCAN_PERIOD)
//    }

    fun start () {
        if (!isScanning) {
            isScanning = true
            scanner.startScan (listOf (filter), settings, callback)
            scanTimeoutRunnable = Runnable { stop() }
            handler.postDelayed (scanTimeoutRunnable!!, SCAN_PERIOD)
            Log.d ("Bluetooth", "Device scan started, for ${SCAN_PERIOD/1000} seconds")
        }
    }

    fun stop () {
        if (isScanning) {
            isScanning = false
            scanTimeoutRunnable?.let { handler.removeCallbacks (it) }
            scanRetryRunnable?.let { handler.removeCallbacks (it) }
            scanner.stopScan (callback)
            Log.d ("Bluetooth", "Device scan stopped")
        }
    }
}
