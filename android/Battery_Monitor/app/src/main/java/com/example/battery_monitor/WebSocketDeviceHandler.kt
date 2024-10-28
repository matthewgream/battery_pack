package com.example.battery_monitor

import android.app.Activity
import android.net.nsd.NsdServiceInfo
import android.util.Log
import org.java_websocket.WebSocket
import org.java_websocket.client.WebSocketClient
import org.java_websocket.framing.Framedata
import org.java_websocket.handshake.ServerHandshake
import java.net.URI

class WebSocketDeviceHandler(
    tag: String,
    activity: Activity,
    @Suppress("unused") private val adapter: AdapterNetworkWifi,
    private val config: WebSocketDeviceConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    statusCallback: () -> Unit,
    isAvailable: () -> Boolean,
    isPermitted: () -> Boolean
) : ConnectivityDeviceHandler(tag, statusCallback, config.connectionActiveCheck, config.connectionActiveTimeout,  isAvailable, isPermitted) {

    //

    private var websocketClient: WebSocketClient? = null
    private fun websocketCreate(uri: URI): WebSocketClient {
        return object : WebSocketClient(uri) {
            override fun onOpen(handshakedata: ServerHandshake?) {
                Log.d(tag, "onOpen")
                onWebsocketConnected()
            }
            override fun onMessage(message: String) {
                Log.d(tag, "onMessage: message=$message")
                onWebsocketReceived(message)
            }
            override fun onClose(code: Int, reason: String, remote: Boolean) {
                Log.d(tag, "onClose: remote=$remote, code=$code, reason='$reason'")
                if (remote)
                    onWebsocketDisconnected()
            }
            override fun onError(ex: Exception?) {
                Log.e(tag, "onError: error=${ex?.message}")
                onWebsocketError()
            }
            override fun onWebsocketPong(conn: WebSocket?, f: Framedata?) {
                Log.d(tag, "onWebSocketPong")
                onWebsocketKeepalive()
            }
        }
    }
    private fun websocketConnect(url: String): Boolean {
        try {
            websocketClient = websocketCreate(URI(url))
            @Suppress("UsePropertyAccessSyntax")
            websocketClient?.setConnectionLostTimeout(config.connectionActiveCheck)
            websocketClient?.connect()
            return true
        } catch (e: Exception) {
            Log.e(tag, "Device connect failed: exception", e)
        }
        return false
    }
    private fun websocketWrite(value: String) : Boolean {
        try {
            websocketClient?.send(value)
            return true
        } catch (e: Exception) {
            Log.e(tag, "Device send failed: exception", e)
        }
        return false
    }
    private fun websocketDisconnect() {
        try {
            websocketClient?.let {
                it.close()
                websocketClient = null
            }
        } catch (e: Exception) {
            Log.e(tag, "Device close failed: exception", e)
        }
    }

    private val websocketScanner = WebSocketDeviceScanner("${tag}Scanner", activity,
        WebSocketDeviceScanner.Config(config.serviceType, config.serviceName, config.connectionScanDelay, config.connectionScanPeriod), connectivityInfo,
        onFound = { serviceInfo -> onWebsocketLocated(serviceInfo)
    })

    //

    override fun doConnectionStart() : Boolean {
        Log.d(tag, "Device locate")
        websocketScanner.start()
        return true
    }
    override fun doConnectionStop() {
        Log.d(tag, "Device disconnect")
        websocketDisconnect()
        websocketScanner.stop()
    }
    override fun doConnectionIdentify(): Boolean {
        return websocketWrite(connectivityInfo.toJsonString())
    }

    //

    @Suppress("DEPRECATION")
    private fun onWebsocketLocated(serviceInfo: NsdServiceInfo) {
        Log.d(tag, "Device located '${serviceInfo.serviceName}'/${serviceInfo.host.hostAddress}:${serviceInfo.port}")
        val url = "ws://${serviceInfo.host.hostAddress}:${serviceInfo.port}/"
        Log.d(tag, "Device connecting to $url")
        setConnectionIsActive()
        if (!websocketConnect(url))
            setConnectionDoReconnect()
    }
    private fun onWebsocketConnected() {
        Log.d(tag, "Device connected")
        setConnectionIsConnected()
    }
    private fun onWebsocketDisconnected() {
        Log.d(tag, "Device disconnected")
        setConnectionDoReconnect()
    }
    private fun onWebsocketReceived(value: String) {
        Log.d(tag, "Device received: $value")
        setConnectionIsActive()
        dataCallback(value)
    }
    private fun onWebsocketError() {
        Log.d(tag, "Device error")
        setConnectionDoReconnect()
    }
    private fun onWebsocketKeepalive() {
        setConnectionIsActive()
    }
}