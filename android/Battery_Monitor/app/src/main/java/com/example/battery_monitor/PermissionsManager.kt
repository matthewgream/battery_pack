package com.example.battery_monitor

import android.app.Activity
import android.content.pm.PackageManager
import android.util.Log

class PermissionsManagerFactory (private val activity: Activity) {
    companion object {
        private var requestCode = 1000
    }
    fun create (tag: String, permissionsRequired: Array<String>): PermissionsManager {
        val manager = PermissionsManager (activity, requestCode ++, tag, permissionsRequired)
        if (activity is PermissionsAwareActivity)
            activity.addOnRequestPermissionsResultListener { receivedRequestCode, _, grantResults ->
                if (receivedRequestCode == manager.requestCode) {
                    manager.receivePermissions (grantResults)
                    true
                } else
                    false
            }
        return manager
    }
}

open class PermissionsAwareActivity : Activity () {
    private val permissionsListeners = mutableListOf<(Int, Array<out String>, IntArray) -> Boolean>()
    fun addOnRequestPermissionsResultListener (listener: (Int, Array<out String>, IntArray) -> Boolean) {
        permissionsListeners.add (listener)
    }
    override fun onRequestPermissionsResult (requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        for (listener in permissionsListeners)
            if (listener (requestCode, permissions, grantResults))
                return
        super.onRequestPermissionsResult (requestCode, permissions, grantResults)
    }
}

class PermissionsManager (
    private val activity: Activity,
    val requestCode: Int,
    private val tag: String,
    private val permissionsRequired: Array<String>
) {
    var requested: Boolean = false
        private set
    var obtained: Boolean = false
        private set
    var allowed: Boolean = false
        private set
    private var onAllowed: (() -> Unit)? = null

    fun requestPermissions (onPermissionsAllowed: () -> Unit) {
        this.onAllowed = onPermissionsAllowed
        requested = true
        if (!permissionsRequired.all { activity.checkSelfPermission (it) == PackageManager.PERMISSION_GRANTED }) {
            Log.d (tag, "Permissions request")
            activity.requestPermissions (permissionsRequired, requestCode)
        } else {
            Log.d (tag, "Permissions already granted")
            obtained = true
            allowed = true
            onPermissionsAllowed ()
        }
    }
    fun receivePermissions (grantResults: IntArray) {
        obtained = true
        if (grantResults.isNotEmpty () && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
            Log.d (tag, "Permissions granted")
            allowed = true
            onAllowed?.invoke ()
        } else
            Log.d (tag, "Permissions denied")
    }
}
