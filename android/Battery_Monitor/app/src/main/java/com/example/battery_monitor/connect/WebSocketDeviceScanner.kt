package com.example.battery_monitor.connect

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.util.Log
import com.example.battery_monitor.utility.Activable

class WebSocketDeviceScanner(
    tag: String,
    context: Context,
    private val config: Config,
    connectionInfo: ConnectInfo,
    onFound: (NsdServiceInfo) -> Unit
) : ConnectComponent(tag, config.scanPeriod) {

    class Config(
        val type: String,
        val name: String,
        val scanDelay: Int,
        val scanPeriod: Int
    )

    private class NsdDiscoverer(
        private val tag: String,
        private val context: Context,
        private val config: Config,
        private val connectionInfo: ConnectInfo,
        private val onFound: (NsdDiscoverer, NsdServiceInfo) -> Unit,
        private val onNeedsRestart: (NsdDiscoverer) -> Unit
    ) {
        private val discoveryListener = object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(serviceType: String) {
                Log.d(tag, "discoveryListener::onDiscoveryStarted: type=$serviceType")
            }
            override fun onDiscoveryStopped(serviceType: String) {
                Log.d(tag, "discoveryListener::onDiscoveryStopped: type=$serviceType")
            }
            override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
                Log.e(tag, "discoveryListener::onStartDiscoveryFailed: error=$errorCode")
                onNeedsRestart(this@NsdDiscoverer)
            }
            override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {
                Log.e(tag, "discoveryListener::onStopDiscoveryFailed: error=$errorCode")
            }
            override fun onServiceLost(serviceInfo: NsdServiceInfo) {
                Log.d(tag, "discoveryListener::onServiceLost: name=${serviceInfo.serviceName}")
            }
            override fun onServiceFound(serviceInfo: NsdServiceInfo) {
                Log.d(tag, "discoveryListener::onServiceFound: name=${serviceInfo.serviceName}")
                if (serviceInfo.serviceName == config.name)
                    resolveStart(serviceInfo)
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
                onNeedsRestart(this@NsdDiscoverer)
            }
            override fun onServiceUpdated(updatedServiceInfo: NsdServiceInfo) {
                val txtRecords = updatedServiceInfo.attributes.map { "${it.key}=${String(it.value)}" }
                val serviceAddr = txtRecords.firstOrNull { it.startsWith("addr=") }?.split("=")?.getOrNull(1)
                when {
                    connectionInfo.deviceAddress.isEmpty() -> {
                        Log.d(tag, "serviceInfo::onServiceUpdated, address unspecified, but found $serviceAddr, with TXT records: $txtRecords")
                        onNeedsRestart(this@NsdDiscoverer)
                    }
                    serviceAddr == connectionInfo.deviceAddress -> {
                        Log.d(tag, "serviceInfo::onServiceUpdated, address matched $serviceAddr, with TXT records $txtRecords")
                        onFound(this@NsdDiscoverer, updatedServiceInfo)
                    }
                    else -> {
                        Log.d(tag, "serviceInfo::onServiceUpdated, address unmatched, expected ${connectionInfo.deviceAddress}, but found $serviceAddr, with TXT records: $txtRecords")
                        onNeedsRestart(this@NsdDiscoverer)
                    }
                }
            }
        }
        private val nsdManager: NsdManager = context.getSystemService(Context.NSD_SERVICE) as NsdManager
        private var isDiscovering = Activable()
        private var isResolving = Activable()
        fun discovertyStart() {
            if (isDiscovering.toActive())
                tag.withOperation("NSD","discoveryStart") {
                    nsdManager.discoverServices(config.type, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
                }
        }
        fun discoveryStop() {
            resolveStop()
            if (isDiscovering.toInactive())
                tag.withOperation("NSD","stopDiscovery") {
                    nsdManager.stopServiceDiscovery(discoveryListener)
                }
        }
        private fun resolveStart(serviceInfo: NsdServiceInfo) {
            if (isResolving.toActive())
                tag.withOperation("NSD","startResolving") {
                    nsdManager.registerServiceInfoCallback(serviceInfo, context.mainExecutor, serviceInfoCallback)
                }
        }
        private fun resolveStop() {
            if (isResolving.toInactive())
                tag.withOperation("NSD","stopResolving") {
                    nsdManager.unregisterServiceInfoCallback(serviceInfoCallback)
                }
        }
    }

    //

    private val discoverer = NsdDiscoverer(tag, context,
        config,
        connectionInfo,
        onFound = { _, serviceInfo ->
            stop()
            onFound(serviceInfo)
        },
        onNeedsRestart = {
            doRestartDelayed()
        }
    )

    //

    private val doRestartRunnable = Runnable { start() }
    private fun doRestartDelayed() {
        stop()
        handler.postDelayed(doRestartRunnable, config.scanDelay * 1000L)
    }
    private fun cancelRestartDelayed() {
        handler.removeCallbacks(doRestartRunnable)
    }

    //

    override fun onStart() {
        discoverer.discovertyStart()
    }
    override fun onStop() {
        cancelRestartDelayed()
        discoverer.discoveryStop()
    }
    override fun onTimer(): Boolean {
        doRestartDelayed()
        return false
    }
}