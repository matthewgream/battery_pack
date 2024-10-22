package com.example.battery_monitor

import android.app.Activity
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.util.Log

@Suppress("PropertyName")
class NetworkDeviceManagerConfig {
    val SERVICE_TYPE = "_ws._tcp."
    val SERVICE_NAME = "battery_monitor"
}

class NetworkDeviceManager(
    activity: Activity,
    private val adapter: NetworkAdapter,
    config: NetworkDeviceManagerConfig,
    private val dataCallback: (String) -> Unit,
    private val statusCallback: () -> Unit,
    private val isEnabled: () -> Boolean,
    private val isPermitted: () -> Boolean
) {
    private val nsdManager: NsdManager = activity.getSystemService(Activity.NSD_SERVICE) as NsdManager
    private var isConnected = false

    private val scanner: NetworkDeviceScanner = NetworkDeviceScanner(
        nsdManager,
        NetworkDeviceScannerConfig(
            SERVICE_TYPE = config.SERVICE_TYPE,
            SERVICE_NAME = config.SERVICE_NAME
        ),
        onFound = { serviceInfo -> handleFoundService(serviceInfo) }
    )

    @Suppress("DEPRECATION")
    private fun handleFoundService(serviceInfo: NsdServiceInfo) {
        val host = serviceInfo.host
        val port = serviceInfo.port
        val txtRecords = serviceInfo.attributes.map { "${it.key}=${String(it.value)}" }

        val id = txtRecords.firstOrNull { it.startsWith("id=") }?.split("=")?.getOrNull(1) ?: "Unknown"

        Log.d("Network", "Device scan located, ${serviceInfo.serviceName} / ${host.hostAddress}:$port [id=$id]")
        Log.d("Network", "  txt records --> $txtRecords")

        isConnected = true
        statusCallback()

        //dataCallback("{\"type\":\"network\",\"data\":\"Connected to $hostname ($ipAddress:$port), Build: $build\"}")
    }

    fun isConnected(): Boolean = isConnected

    fun permissionsAllowed () {
        statusCallback ()
        if (!isConnected)
            locate ()
    }
    fun locate() {
        Log.d("Network", "Device locate initiated")
        statusCallback()
        when {
            !isEnabled() -> Log.e("Network", "Network not enabled or available")
            !isPermitted() -> Log.e("Network", "Network access not permitted")
            isConnected -> Log.d("Network", "Network connection already active, will not locate")
            else -> scanner.start()
        }
    }

    fun disconnect() {
        scanner.stop()
        isConnected = false
        statusCallback()
    }

    fun reconnect() {
        disconnect()
        locate()
    }
}