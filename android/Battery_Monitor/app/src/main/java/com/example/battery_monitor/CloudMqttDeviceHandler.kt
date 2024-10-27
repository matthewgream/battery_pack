package com.example.battery_monitor

import android.app.Activity
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.hivemq.client.mqtt.MqttClient
import com.hivemq.client.mqtt.mqtt5.Mqtt5AsyncClient
import com.hivemq.client.mqtt.mqtt5.message.connect.connack.Mqtt5ConnAck
import com.hivemq.client.mqtt.mqtt5.message.publish.Mqtt5Publish
import com.hivemq.client.mqtt.mqtt5.message.subscribe.suback.Mqtt5SubAck
import java.nio.charset.StandardCharsets

class CloudMqttDeviceHandler(
    private val tag: String,
    @Suppress("unused") private val activity: Activity,
    @Suppress("unused") private val adapter: AdapterInternet,
    private val config: CloudMqttDeviceConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    private val statusCallback: () -> Unit,
    private val isEnabled: () -> Boolean,
    private val isPermitted: () -> Boolean
) : ConnectivityDeviceHandler() {

    private val handler = Handler (Looper.getMainLooper ())

//    private val checker: ConnectivityChecker = ConnectivityChecker(tag, config.connectionActiveCheck, config.connectionActiveTimeout,
//        isConnected = { isConnected() },
//        onTimeout = { reconnect() })

    //

    private var mqttClient: Mqtt5AsyncClient? = null
    private fun mqttConnect() {
        try {
            mqttClient = MqttClient.builder()
                .useMqttVersion5()
                .identifier(connectivityInfo.identity)
                .serverHost(config.host)
                .serverPort(config.port)
                .automaticReconnectWithDefaultConfig()
                .addConnectedListener { context ->
                    Log.d(tag, "Keep-alive event received")
                    keepalive()
                }
                .build()
                .toAsync()
            val connect = mqttClient?.connectWith()
                ?.cleanStart(true)
                ?.sessionExpiryInterval(config.connectionActiveTimeout.toLong())
                ?.keepAlive(config.connectionActiveCheck)
            if (config.user != null && config.pass != null)
                connect?.simpleAuth()
                    ?.username(config.user)
                    ?.password(config.pass.toByteArray())
                    ?.applySimpleAuth()
            connect?.send()
                ?.whenComplete { connAck: Mqtt5ConnAck, throwable ->
                    if (throwable != null) {
                        Log.e(tag, "MQTT broker connection failure: exception", throwable)
                        disconnected()
                    } else {
                        Log.d(tag, "MQTT broker connection success: ${connAck.reasonString.map { it.toString() }.orElse("Success")}")
                        connected()
                    }
                }
        } catch (e: Exception) {
            Log.e(tag, "MQTT client configuration failure: exception", e)
            disconnected()
        }
    }
    private fun mqttSubscribe(topic: String) : Boolean {
        val client = mqttClient ?: return false
        try {
            client.subscribeWith()
                .topicFilter(topic)
                .retainAsPublished(true)
                .noLocal(true)
                .callback { publish: Mqtt5Publish ->
                    val message = String(publish.payloadAsBytes, StandardCharsets.UTF_8)
                    Log.d(tag, "MQTT broker subscribe topic='${publish.topic}', message='$message'")
                    receive(message)
                }
                .send()
                .whenComplete { subAck: Mqtt5SubAck, throwable ->
                    if (throwable != null) {
                        Log.e(tag, "MQTT broker subscribe topic='$topic' failure: error=${throwable.message}")
                        if (throwable.message?.contains("not connected", ignoreCase = true) == true)
                            disconnected()
                    } else {
                        this.topic = topic
                        Log.d(tag,"MQTT broker subscribe topic='$topic' success: ${subAck.reasonCodes.joinToString(", ")}")
                    }
                }
            return true
        } catch (e: Exception) {
            Log.e(tag, "MQTT broker subscribe topic='$topic' failure: exception", e)
            return false
        }
    }
    private fun mqttUnsubscribe(topic: String) : Boolean {
        val client = mqttClient ?: return false
        try {
            client.unsubscribeWith()
                .topicFilter(topic)
                .send()
                .whenComplete { unsubAck, throwable ->
                    if (throwable != null) {
                        Log.e(tag, "MQTT broker unsubscribe topic='$topic' failure: exception", throwable)
                        if (throwable.message?.contains("not connected", ignoreCase = true) == true)
                            disconnected()
                    } else {
                        this.topic = ""
                        Log.d(tag, "MQTT broker unsubscribe topic='$topic' success: ${unsubAck.reasonCodes.joinToString(", ")}")
                    }
                }
            return true
        } catch (e: Exception) {
            Log.e(tag, "MQTT broker unsubscribe topic='$topic' failure: exception", e)
            return false
        }
    }
    private fun mqttPublish(topic: String, message: String) : Boolean {
        val client = mqttClient ?: return false
        try {
            client.publishWith()
                .topic(topic)
                .payload(message.toByteArray())
                .contentType("application/json")
                .messageExpiryInterval(60)
                .send()
                .whenComplete { _, throwable ->
                    if (throwable != null) {
                        Log.e(tag, "MQTT broker publish topic='$topic' failure: exception", throwable)
                        if (throwable.message?.contains("not connected", ignoreCase = true) == true)
                            disconnected()
                    } else
                        Log.d(tag, "MQTT broker publish topic='$topic', message='$message'")
                }
            return true
        } catch (e: Exception) {
            Log.e(tag, "MQTT broker publish topic='$topic' failure: exception", e)
            return false
        }
    }
    private fun mqttDisconnect() {
        val client = mqttClient ?: return
        try {
            client.disconnectWith()
                .sessionExpiryInterval(0)
                .send()
                .whenComplete { _, throwable ->
                    if (throwable != null)
                        Log.e(tag, "MQTT broker disconnect failure: exception", throwable)
                    else
                        Log.d(tag, "MQTT broker disconnect success")
                }
        } catch (e: Exception) {
            Log.e(tag, "MQTT broker disconnect failure: exception", e)
        }
        mqttClient = null
    }
    //

    private var isConnecting = false
    private var isConnected = false

    private var topic = ""

    override fun permitted() {
        statusCallback()
        if (!isConnecting && !isConnected)
            connect()
    }
    private fun connect() {
        Log.d(tag, "Device connect initiate")
        statusCallback()
//        checker.start()
        when {
            !isEnabled() -> Log.e(tag, "Device not enabled or available")
            !isPermitted() -> Log.e(tag, "Device access not permitted")
            isConnecting -> Log.d(tag, "Device connection already in progress")
            isConnected -> Log.d(tag, "Device connection already active, will not connect")
            else -> {
                isConnecting = true
                mqttConnect()
            }
        }
    }
    private fun connected() {
        Log.d(tag, "Device connected")
//        checker.ping()
        isConnected = true
        isConnecting = false
        statusCallback()
        handler.postDelayed({
            identify()
        }, 1000)
        if (topic.isNotEmpty())
            subscribe()
    }
    private fun identify() {
        val topic = "${config.root}/${connectivityInfo.deviceAddress}/peer"
        publish(topic, connectivityInfo.toJsonString())
    }
    private fun keepalive() {
//        checker.ping()
    }
    fun subscribe(): Boolean {
        val topic = "${config.root}/${connectivityInfo.deviceAddress}/#"
        Log.d(tag, "Device subscribe '${topic}'")
        if (!isConnected) {
            this.topic = topic
            return true
        }
        return mqttSubscribe(topic)
    }
    fun unsubscribe(): Boolean {
        val topic = "${config.root}/${connectivityInfo.deviceAddress}/#"
        Log.d(tag, "Device unsubscribe '${topic}'")
        if (!isConnected) {
            if (this.topic == topic) this.topic = ""
            return true
        }
        return mqttUnsubscribe(topic)
    }
    fun publish(type: String, message: String): Boolean {
        val topic = "${config.root}/${connectivityInfo.deviceAddress}/$type"
        if (!isConnected) {
            Log.e(tag, "Device publish '${topic}' failure: not connected")
            return false
        }
        Log.d(tag, "Device publish '${topic}'")
        return mqttPublish(topic, message)
    }
    private fun receive(value: String) {
        Log.d(tag, "Device received: $value")
//        checker.ping()
        dataCallback(value)
    }
    private fun disconnect() {
        Log.d(tag, "Device disconnect")
        mqttDisconnect()
//        checker.stop()
        isConnected = false
        isConnecting = false
        statusCallback()
    }
    override fun disconnected() {
        Log.d(tag, "Device disconnected")
        mqttDisconnect()
//        checker.stop()
        isConnected = false
        isConnecting = false
        statusCallback()
        handler.postDelayed({
            connect()
        }, 1000)
    }
    override fun reconnect() {
        Log.d(tag, "Device reconnect")
        disconnect()
        handler.postDelayed({
            connect()
        }, 1000)
    }
    override fun isConnected(): Boolean = isConnected
}