package com.example.battery_monitor

import android.app.Activity
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
    config: WebSocketDeviceConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    statusCallback: () -> Unit,
    isAvailable: () -> Boolean,
    isPermitted: () -> Boolean
) : ConnectivityDeviceHandler(tag, statusCallback, config.connectionActiveCheck, config.connectionActiveTimeout,  isAvailable, isPermitted) {

    //

    private class WebSocketDeviceConnection(
        private val tag: String,
        private val config: WebSocketDeviceConfig,
        private val onKeepalive: (WebSocketDeviceConnection) -> Unit,
        private val onConnected: (WebSocketDeviceConnection) -> Unit,
        private val onDisconnected: (WebSocketDeviceConnection) -> Unit,
        private val onError: (WebSocketDeviceConnection) -> Unit,
        private val onReceived: (WebSocketDeviceConnection, String) -> Unit
    ) {
        private var client: WebSocketClient? = null
        private inline fun <T> wsOperation(operation: String, block: () -> T?): T? {
            return try {
                block()?.also {
                    Log.d(tag, "WebSocket $operation: success")
                }
            } catch (e: Exception) {
                Log.e(tag, "WebSocket $operation: exception", e)
                null
            }
        }
        private fun createClient(uri: URI): WebSocketClient =
            object : WebSocketClient(uri) {
                override fun onOpen(handshake: ServerHandshake?) {
                    Log.d(tag, "onOpen")
                    onConnected(this@WebSocketDeviceConnection)
                }
                override fun onMessage(message: String) {
                    Log.d(tag, "onMessage: $message")
                    onReceived(this@WebSocketDeviceConnection, message)
                }
                override fun onClose(code: Int, reason: String, remote: Boolean) {
                    Log.d(tag, "onClose: remote=$remote, code=$code, reason='$reason'")
                    if (remote) onDisconnected(this@WebSocketDeviceConnection)
                }
                override fun onError(ex: Exception?) {
                    Log.e(tag, "onError: ${ex?.message}")
                    onError(this@WebSocketDeviceConnection)
                }
                override fun onWebsocketPong(conn: WebSocket?, f: Framedata?) {
                    Log.d(tag, "onWebSocketPong")
                    onKeepalive(this@WebSocketDeviceConnection)
                }
            }

        fun connect(url: String): Boolean = wsOperation("connect") {
            createClient(URI(url)).also {
                client = it
                it.connectionLostTimeout = config.connectionActiveCheck
                it.connect()
            }
            true
        } ?: false
        fun send(message: String): Boolean = wsOperation("send") {
            client?.send(message)
            true
        } ?: false
        fun disconnect() {
            wsOperation("disconnect") {
                client?.close()
            }
            client = null
        }
    }

    //

    private val connection = WebSocketDeviceConnection(tag,
        config,
        onConnected = {
            Log.d(tag, "Device connected")
            setConnectionIsConnected()
        },
        onReceived = { _, value ->
            Log.d(tag, "Device received: $value")
            setConnectionIsActive()
            dataCallback(value)
        },
        onKeepalive = {
            setConnectionIsActive()
        },
        onDisconnected = {
            Log.d(tag, "Device disconnected")
            setConnectionDoReconnect()
        },
        onError = {
            Log.d(tag, "Device error")
            setConnectionDoReconnect()
        }
    )

    @Suppress("DEPRECATION")
    private val scanner = WebSocketDeviceScanner("${tag}Scanner", activity,
        WebSocketDeviceScanner.Config(config.serviceType, config.serviceName, config.connectionScanDelay, config.connectionScanPeriod),
        connectivityInfo,
        onFound = { serviceInfo ->
            Log.d(tag, "Device located '${serviceInfo.serviceName}'/${serviceInfo.host.hostAddress}:${serviceInfo.port}")
            val url = "ws://${serviceInfo.host.hostAddress}:${serviceInfo.port}/"
            Log.d(tag, "Device connecting to $url")
            setConnectionIsActive()
            if (!connection.connect(url))
                setConnectionDoReconnect()
    })

    //

    override fun doConnectionStart() : Boolean {
        Log.d(tag, "Device locate")
        scanner.start()
        return true
    }
    override fun doConnectionStop() {
        Log.d(tag, "Device disconnect")
        connection.disconnect()
        scanner.stop()
    }
    override fun doConnectionIdentify(): Boolean {
        return connection.send(connectivityInfo.toJsonString())
    }
}