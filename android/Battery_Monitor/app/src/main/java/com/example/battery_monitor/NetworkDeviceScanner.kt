package com.example.battery_monitor

import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.util.Log
import java.util.concurrent.Executors

@Suppress("PropertyName")
class NetworkDeviceScannerConfig(
    val SERVICE_TYPE: String = "_ws._tcp.",
    val SERVICE_NAME: String = "battery_monitor"
) {
    val SCAN_DELAY = 5000L // 5 seconds
    val SCAN_PERIOD = 30000L // 30 seconds
}

class NetworkDeviceScanner(
    private val nsdManager: NsdManager,
    private val config: NetworkDeviceScannerConfig,
    private val onFound: (NsdServiceInfo) -> Unit
) : ConnectivityComponent("NetworkDeviceScanner") {

    private var isDiscoveryActive = false
    private val executor = Executors.newSingleThreadExecutor()

    private val retryRunnable = Runnable {
        start()
    }

    private val discoveryListener = object : NsdManager.DiscoveryListener {
        override fun onDiscoveryStarted(regType: String) {
            Log.d(tag, "Service discovery started")
            isDiscoveryActive = true
        }

        override fun onServiceFound(serviceInfo: NsdServiceInfo) {
            Log.d(tag, "Service found: ${serviceInfo.serviceName}")
            if (serviceInfo.serviceName == config.SERVICE_NAME)
                resolveService(serviceInfo)
        }

        override fun onServiceLost(serviceInfo: NsdServiceInfo) {
            Log.d(tag, "Service lost: ${serviceInfo.serviceName}")
        }

        override fun onDiscoveryStopped(serviceType: String) {
            Log.d(tag, "Discovery stopped: $serviceType")
            isDiscoveryActive = false
        }

        override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
            Log.e(tag, "Discovery failed to start: Error code: $errorCode")
            isDiscoveryActive = false
            restartAfterDelay()
        }

        override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {
            Log.e(tag, "Discovery failed to stop: Error code: $errorCode")
        }
    }

    private fun resolveService(serviceInfo: NsdServiceInfo) {
        nsdManager.registerServiceInfoCallback(
            serviceInfo,
            executor,
            object : NsdManager.ServiceInfoCallback {
                override fun onServiceInfoCallbackRegistrationFailed(errorCode: Int) {
                    Log.e(tag, "Service info callback registration failed: $errorCode")
                }

                override fun onServiceUpdated(updatedServiceInfo: NsdServiceInfo) {
                    Log.d(tag, "Service resolved: ${updatedServiceInfo.serviceName}")
                    stop()
                    onFound(updatedServiceInfo)
                }

                override fun onServiceLost() {
                    Log.d(tag, "Service lost during resolution")
                }

                override fun onServiceInfoCallbackUnregistered() {
                    Log.d(tag, "Service info callback unregistered")
                }
            }
        )
    }

    private fun restartAfterDelay() {
        stop()
        handler.postDelayed(retryRunnable, config.SCAN_DELAY)
    }

    override val timer: Long
        get() = config.SCAN_PERIOD

    override fun onStart() {
        if (!isDiscoveryActive) {
            try {
                nsdManager.discoverServices(config.SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
            } catch (e: IllegalArgumentException) {
                Log.e(tag, "Error starting discovery: ${e.message}")
                // If we get an IllegalArgumentException, the listener might still be registered
                // Let's try to stop it first and then start again
                try {
                    nsdManager.stopServiceDiscovery(discoveryListener)
                } catch (e: IllegalArgumentException) {
                    Log.e(tag, "Error stopping discovery: ${e.message}")
                }
                // Try to start discovery again after a short delay
                handler.postDelayed({
                    nsdManager.discoverServices(config.SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
                }, 100)
            }
        } else {
            Log.d(tag, "Discovery already active")
        }
    }

    override fun onStop() {
        if (isDiscoveryActive) {
            try {
                nsdManager.stopServiceDiscovery(discoveryListener)
            } catch (e: IllegalArgumentException) {
                Log.e(tag, "Error stopping discovery: ${e.message}")
            }
        }
        handler.removeCallbacks(retryRunnable)
        isDiscoveryActive = false
    }

    override fun onTimer(): Boolean {
        restartAfterDelay()
        return false
    }
}