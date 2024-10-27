package com.example.battery_monitor

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.util.Log

class AdapterWifi(
    tag: String,
    context: Context,
    private val onDisabled: () -> Unit,
    private val onEnabled: () -> Unit
) : ConnectivityDeviceAdapter(tag) {

    private val manager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

    private val callback = object : ConnectivityManager.NetworkCallback() {
        override fun onLost(network: Network) {
            super.onLost(network)
            Log.d(tag, "Wifi lost")
            onDisabled()
        }
        override fun onAvailable(network: Network) {
            super.onAvailable(network)
            Log.d(tag, "Wifi available")
            onEnabled()
        }
    }

    override fun isEnabled(): Boolean {
        return manager.getNetworkCapabilities(manager.activeNetwork)
            ?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
    }
    override fun onStart() {
        manager.registerNetworkCallback(
            NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .build(),
            callback)
    }
    override fun onStop() {
        manager.unregisterNetworkCallback(callback)
    }
}