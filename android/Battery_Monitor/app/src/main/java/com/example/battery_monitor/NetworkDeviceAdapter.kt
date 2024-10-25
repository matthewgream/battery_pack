package com.example.battery_monitor

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities

class NetworkDeviceAdapter(context: Context): ConnectivityDeviceAdapter () {
    private val connectivityManager: ConnectivityManager =
        context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    override fun isEnabled(): Boolean {
        val network = connectivityManager.activeNetwork
        val capabilities = connectivityManager.getNetworkCapabilities(network)
        return capabilities?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
    }
}