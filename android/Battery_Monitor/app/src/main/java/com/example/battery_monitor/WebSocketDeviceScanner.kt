package com.example.battery_monitor

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.util.Log

class WebSocketDeviceScanner(
    tag: String,
    private val context: Context,
    private val config: Config,
    private val connectionInfo: ConnectivityInfo,
    private val onFound: (NsdServiceInfo) -> Unit
) : ConnectivityComponent(tag, config.scanPeriod) {

    class Config(
        val type: String,
        val name: String,
        val scanDelay: Int,
        val scanPeriod: Int
    )

    private val nsdManager: NsdManager = context.getSystemService(Context.NSD_SERVICE) as NsdManager
    private var isDiscoveryActive = false

    private val retryRunnable = Runnable {
        start()
    }

    private val discoveryListener = object : NsdManager.DiscoveryListener {
        override fun onDiscoveryStarted(serviceType: String) {
            Log.d(tag, "discoveryListener::onDiscoveryStarted: type=$serviceType")
        }
        override fun onDiscoveryStopped(serviceType: String) {
            Log.d(tag, "discoveryListener::onDiscoveryStopped: type=$serviceType")
        }
        override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
            Log.e(tag, "discoveryListener::onStartDiscoveryFailed: error=$errorCode")
            restartAfterDelay()
        }
        override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {
            Log.e(tag, "discoveryListener::onStopDiscoveryFailed: error=$errorCode")
        }
        override fun onServiceLost(serviceInfo: NsdServiceInfo) {
            Log.d(tag, "discoveryListener::onServiceLost: name=${serviceInfo.serviceName}")
            restartAfterDelay()
        }
        override fun onServiceFound(serviceInfo: NsdServiceInfo) {
            Log.d(tag, "discoveryListener::onServiceFound: name=${serviceInfo.serviceName}")
            if (serviceInfo.serviceName == config.name)
                resolve(serviceInfo)
        }
    }
    private val serviceInfoCallback = object : NsdManager.ServiceInfoCallback {
        override fun onServiceInfoCallbackRegistrationFailed(errorCode: Int) {
            Log.e(tag, "serviceInfo::onServiceInfoCallbackRegistrationFailed: error=$errorCode")
        }
        override fun onServiceInfoCallbackUnregistered() {
            Log.d(tag, "serviceInfo::onServiceInfoCallbackUnregistered")
        }
        override fun onServiceLost() {
            Log.d(tag, "serviceInfo::onServiceLost")
            restartAfterDelay()
        }
        override fun onServiceUpdated(updatedServiceInfo: NsdServiceInfo) {
            val txtRecords = updatedServiceInfo.attributes.map { "${it.key}=${String(it.value)}" }
            val serviceAddr = txtRecords.firstOrNull { it.startsWith("addr=") }?.split("=")?.getOrNull(1)
            if (connectionInfo.deviceAddress.isEmpty()) {
                Log.d(tag, "serviceInfo::onServiceUpdated, address unspecified, but found $serviceAddr, with TXT records: $txtRecords")
                restartAfterDelay()
            } else if (serviceAddr == connectionInfo.deviceAddress) {
                Log.d(tag, "serviceInfo::onServiceUpdated, address matched $serviceAddr, with TXT records $txtRecords")
                stop()
                onFound(updatedServiceInfo)
            } else {
                Log.d(tag, "serviceInfo::onServiceUpdated, address unmatched, expected ${connectionInfo.deviceAddress}, but found $serviceAddr, with TXT records: $txtRecords")
                restartAfterDelay()
            }
        }
    }

    private fun resolve(serviceInfo: NsdServiceInfo) {
        nsdManager.registerServiceInfoCallback(serviceInfo, context.mainExecutor, serviceInfoCallback)
    }

    private fun restartAfterDelay() {
        stop()
        handler.postDelayed(retryRunnable, config.scanDelay*1000L)
    }

    override fun onStart() {
        if (!isDiscoveryActive) {
            try {
                nsdManager.discoverServices(config.type, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
            } catch (e: IllegalArgumentException) {
                Log.e(tag, "Service discovery start: error=${e.message}")
                try {
                    nsdManager.stopServiceDiscovery(discoveryListener)
                } catch (e: IllegalArgumentException) {
                    Log.e(tag, "Service discovery stop: error=${e.message}")
                }
                handler.postDelayed({
                    nsdManager.discoverServices(config.type, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
                }, 100)
            }
            isDiscoveryActive = true
        } else {
            Log.d(tag, "Service discovery already active")
        }
    }

    override fun onStop() {
        if (isDiscoveryActive) {
            try {
                nsdManager.unregisterServiceInfoCallback(serviceInfoCallback)
            } catch (e: IllegalArgumentException) {
                Log.e(tag, "Service discovery unregister: error=${e.message}")
            }
            try {
                nsdManager.stopServiceDiscovery(discoveryListener)
            } catch (e: IllegalArgumentException) {
                Log.e(tag, "Service discovery stop: error=${e.message}")
            }
            isDiscoveryActive = false
        }
        handler.removeCallbacks(retryRunnable)
    }

    override fun onTimer(): Boolean {
        restartAfterDelay()
        return false
    }
}