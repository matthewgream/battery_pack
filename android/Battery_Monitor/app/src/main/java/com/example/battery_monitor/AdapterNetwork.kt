package com.example.battery_monitor

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest

object NetworkRegistry {
    private val networks = mutableMapOf<Network, NetworkCapabilities>()
    private val listeners = mutableSetOf<Pair<(NetworkCapabilities) -> Boolean, (Boolean) -> Unit>>()
    private var initialized = Activable()
    private lateinit var manager: ConnectivityManager

    private val callback = object : ConnectivityManager.NetworkCallback() {
        override fun onLost(network: Network) {
            super.onLost(network)
            networks.remove(network)
            notifyListeners()
        }
        override fun onAvailable(network: Network) {
            super.onAvailable(network)
            manager.getNetworkCapabilities(network)?.let { capabilities ->
                networks[network] = capabilities
                notifyListeners()
            }
        }
        override fun onCapabilitiesChanged(network: Network, networkCapabilities: NetworkCapabilities) {
            super.onCapabilitiesChanged(network, networkCapabilities)
            networks[network] = networkCapabilities
            notifyListeners()
        }
    }

    fun initialize(context: Context) {
        if (initialized.toActive()) {
            manager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            @Suppress("DEPRECATION")
            manager.allNetworks.forEach { network ->
                manager.getNetworkCapabilities(network)?.let { capabilities ->
                    networks[network] = capabilities
                }
            }
            manager.registerNetworkCallback(NetworkRequest.Builder().build(), callback)
        }
    }

    private fun notifyListeners() {
        listeners.forEach { (matcher, callback) ->
            callback(networks.any { (_, caps) -> matcher(caps) })
        }
    }
    fun addListener(matcher: (NetworkCapabilities) -> Boolean, callback: (Boolean) -> Unit) {
        listeners.add(matcher to callback)
        callback(networks.any { (_, caps) -> matcher(caps) })
    }
    fun removeListener(matcher: (NetworkCapabilities) -> Boolean, callback: (Boolean) -> Unit) {
        listeners.remove(matcher to callback)
    }
}

abstract class AdapterNetwork(
    tag: String,
    context: Context,
    private val onDisabled: () -> Unit,
    private val onEnabled: () -> Unit
) : ConnectivityDeviceAdapter(tag) {

    private val enabled = Activable()

    init { NetworkRegistry.initialize(context) }

    protected abstract fun matchesCapability(capabilities: NetworkCapabilities): Boolean
    protected abstract fun createNetworkRequest(): NetworkRequest

    private val networkCallback: (Boolean) -> Unit = { hasNetwork ->
        if (hasNetwork) { if (enabled.toActive()) onEnabled()
        } else { if (enabled.toInactive()) onDisabled() }
    }

    override fun isEnabled(): Boolean = enabled.isActive
    override fun onStart() { NetworkRegistry.addListener(::matchesCapability, networkCallback) }
    override fun onStop() { NetworkRegistry.removeListener(::matchesCapability, networkCallback) }
}

class AdapterNetworkInternet(
    tag: String,
    context: Context,
    onDisabled: () -> Unit,
    onEnabled: () -> Unit
) : AdapterNetwork(tag, context, onDisabled, onEnabled) {

    override fun matchesCapability(capabilities: NetworkCapabilities): Boolean = capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
    override fun createNetworkRequest(): NetworkRequest = NetworkRequest.Builder().addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET).build()
}

class AdapterNetworkWifi(
    tag: String,
    context: Context,
    onDisabled: () -> Unit,
    onEnabled: () -> Unit
) : AdapterNetwork(tag, context, onDisabled, onEnabled) {

    override fun matchesCapability(capabilities: NetworkCapabilities): Boolean = capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)
    override fun createNetworkRequest(): NetworkRequest = NetworkRequest.Builder().addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build()
}