package com.example.battery_monitor

import android.app.Activity

class NetworkManager(
    activity: Activity,
    connectivityInfo: ConnectivityInfo,
    dataCallback: (String) -> Unit,
    statusCallback: () -> Unit
) : com.example.battery_monitor.ConnectivityManager <NetworkDeviceAdapter, NetworkDeviceManager, NetworkDeviceManagerConfig, NetworkDeviceState>(activity,
    "Network",
    arrayOf (
        android.Manifest.permission.INTERNET,
        android.Manifest.permission.ACCESS_NETWORK_STATE
    ),
    connectivityInfo,
    dataCallback,
    statusCallback
) {
    override val adapter: NetworkDeviceAdapter = NetworkDeviceAdapter (activity)
    override val device: NetworkDeviceManager = NetworkDeviceManager (activity,
        adapter,
        NetworkDeviceManagerConfig (),
        connectivityInfo,
        dataCallback,
        statusCallback,
        isPermitted = { permissions.allowed },
        isEnabled = { adapter.isEnabled () && connectivityInfo.deviceAddress.isNotEmpty () }
    )
    override val checker: NetworkDeviceState = NetworkDeviceState (activity,
        "NetworkDeviceState",
        onDisabled = { onDisconnect () },
        onEnabled = { onPermitted () }
    )
}