package com.example.battery_monitor

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest

abstract class AdapterNetwork(
    tag: String,
    context: Context,
    private val onDisabled: () -> Unit,
    private val onEnabled: () -> Unit
) : ConnectivityDeviceAdapter(tag) {

    private val manager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    private var networks: MutableSet<Network> = networkLocate()
    private val enabled = Activable(networks.isNotEmpty())

    protected abstract fun matchesCapability(capabilities: NetworkCapabilities): Boolean
    protected abstract fun createNetworkRequest(): NetworkRequest

    private val callback = object : ConnectivityManager.NetworkCallback() {
        override fun onLost(network: Network) {
            super.onLost(network)
            networkRemove(network)
        }
        override fun onAvailable(network: Network) {
            super.onAvailable(network)
            manager.getNetworkCapabilities(network)?.takeIf(::matchesCapability)?.let { networkInsert(network) }
        }
        override fun onCapabilitiesChanged(network: Network, networkCapabilities: NetworkCapabilities) {
            super.onCapabilitiesChanged(network, networkCapabilities)
            if (matchesCapability(networkCapabilities)) { networkInsert(network) } else { networkRemove(network) }
        }
    }

    private fun networkInsert(network: Network) {
        networks.add(network)
        if (enabled.toActive())
            onEnabled()
    }
    private fun networkRemove(network: Network) {
        networks.remove(network)
        if (networks.isEmpty() && enabled.toInactive())
            onDisabled()
    }
    private fun networkLocate(): MutableSet<Network> {
        @Suppress("DEPRECATION")
        return manager.allNetworks.filter { network -> manager.getNetworkCapabilities(network)?.let(::matchesCapability) ?: false }.toMutableSet()
    }

    override fun isEnabled(): Boolean {
        return enabled.isActive
    }
    override fun onStart() {
        if (networks.isEmpty()) {
            networks = networkLocate()
            enabled.isActive = networks.isNotEmpty()
        }
        manager.registerNetworkCallback(createNetworkRequest(), callback)
    }
    override fun onStop() {
        networks.clear()
        manager.unregisterNetworkCallback(callback)
    }
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