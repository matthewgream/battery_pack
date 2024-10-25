package com.example.battery_monitor

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.util.Log

class NetworkDeviceState(
    context: Context,
    tag: String,
    private val onDisabled: () -> Unit,
    private val onEnabled: () -> Unit
) : ConnectivityComponent(tag) {

    private val connectivityManager: ConnectivityManager =
        context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

    private val networkCallback = object : ConnectivityManager.NetworkCallback() {
        override fun onAvailable(network: Network) {
            super.onAvailable(network)
            Log.d(tag, "Network available")
            onEnabled()
        }
        override fun onLost(network: Network) {
            super.onLost(network)
            Log.d(tag, "Network lost")
            onDisabled()
        }
    }

    override fun onStart() {
        val networkRequest = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .build()
        connectivityManager.registerNetworkCallback(networkRequest, networkCallback)
    }
    override fun onStop() {
        connectivityManager.unregisterNetworkCallback(networkCallback)
    }
}