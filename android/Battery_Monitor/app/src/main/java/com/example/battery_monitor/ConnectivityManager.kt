package com.example.battery_monitor

import android.app.Activity
import android.os.Handler
import android.os.Looper

abstract class ConnectivityDeviceManager {
    abstract fun isConnected(): Boolean
    abstract fun disconnect()
    abstract fun reconnect()
    abstract fun permitted()
}
abstract class ConnectivityDeviceAdapter {
    abstract fun isEnabled(): Boolean
}

abstract class ConnectivityManager <TAdapter: ConnectivityDeviceAdapter, TDevice: ConnectivityDeviceManager, TConfig, TChecker: ConnectivityComponent>(
    protected val activity: Activity,
    tag: String,
    permissionsRequired: Array <String>,
    protected val connectivityInfo: ConnectivityInfo,
    protected val dataCallback: (String) -> Unit,
    protected val statusCallback: () -> Unit
) {
    private val handler = Handler(Looper.getMainLooper())

    protected val permissions: PermissionsManager = PermissionsManagerFactory(activity).create(tag, permissionsRequired)
    protected abstract val adapter: TAdapter
    protected abstract val device: TDevice
    protected abstract val checker: TChecker

    init {
        permissions.requestPermissions(
            onPermissionsAllowed = {
                handler.postDelayed({
                    onPermitted()
                }, 50)
            }
        )
        handler.postDelayed({
            checker.start()
        }, 50)
    }

    fun onDestroy() {
        checker.stop()
        onDisconnect()
    }
    fun onPause() {}
    fun onResume() {}
    fun onPowerSave() {}
    fun onPowerBack() {}

    protected fun onDisconnect() = device.disconnect ()
    fun onDoubleTap() = device.reconnect ()
    protected fun onPermitted() = device.permitted ()

    fun isAvailable(): Boolean = adapter.isEnabled ()
    fun isPermitted(): Boolean = permissions.allowed
    fun isConnected(): Boolean = device.isConnected ()
}