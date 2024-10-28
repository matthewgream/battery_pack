package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.util.Log
import com.hivemq.client.mqtt.MqttClient
import com.hivemq.client.mqtt.mqtt5.Mqtt5AsyncClient
import com.hivemq.client.mqtt.mqtt5.message.connect.connack.Mqtt5ConnAck
import com.hivemq.client.mqtt.mqtt5.message.publish.Mqtt5Publish
import com.hivemq.client.mqtt.mqtt5.message.subscribe.suback.Mqtt5SubAck
import java.nio.charset.StandardCharsets

class CloudMqttDeviceHandler(
    tag: String,
    @Suppress("unused") private val activity: Activity,
    @Suppress("unused") private val adapter: AdapterNetworkInternet,
    val config: CloudMqttDeviceConfig,
    private val connectivityInfo: ConnectivityInfo,
    private val dataCallback: (String) -> Unit,
    statusCallback: () -> Unit,
    isAvailable: () -> Boolean,
    isPermitted: () -> Boolean
) : ConnectivityDeviceHandler(tag, statusCallback, 0, 0, isAvailable, isPermitted) {

    //

    private class MqttDeviceConnection(
        private val tag: String,
        private val config: CloudMqttDeviceConfig,
        private val identity: String,
        private val onKeepalive: (MqttDeviceConnection) -> Unit,
        private val onConnected: (MqttDeviceConnection) -> Unit,
        private val onDisconnected: (MqttDeviceConnection) -> Unit,
        private val onError: (MqttDeviceConnection) -> Unit,
        private val onReceived: (MqttDeviceConnection, String) -> Unit
    ) {
        private var client: Mqtt5AsyncClient? = null
        private inline fun <T> mqttOperation(operation: String, block: () -> T?): T? {
            return try {
                block()?.also {
                    Log.d(tag, "MQTT $operation: success")
                }
            } catch (e: Exception) {
                Log.e(tag, "MQTT $operation: exception", e)
                null
            }
        }
        @SuppressLint("CheckResult")
        fun connect(): Boolean = mqttOperation("connect") {
            MqttClient.builder()
                .useMqttVersion5()
                .identifier(identity)
                .serverHost(config.host)
                .serverPort(config.port)
                .automaticReconnectWithDefaultConfig()
                .addConnectedListener { _ ->
                    Log.d(tag, "MQTT broker Keep-alive received")
                    onKeepalive(this@MqttDeviceConnection)
                }
                .build()
                .toAsync()
                .also { client = it }
                .connectWith()
                .apply {
                    cleanStart(true)
                    sessionExpiryInterval(config.connectionActiveTimeout.toLong())
                    keepAlive(config.connectionActiveCheck)
                    config.user?.let { user ->
                        config.pass?.let { pass ->
                            simpleAuth()
                                .username(user)
                                .password(pass.toByteArray())
                                .applySimpleAuth()
                        }
                    }
                }
                .send()
                .whenComplete { connAck: Mqtt5ConnAck, throwable ->
                    when {
                        throwable != null -> {
                            Log.e(tag, "MQTT broker connection failure", throwable)
                            onError(this@MqttDeviceConnection)
                        }
                        else -> {
                            Log.d(tag, "MQTT broker connection success: ${connAck.reasonString.map { it.toString() }.orElse("Success")}")
                            onConnected(this@MqttDeviceConnection)
                        }
                    }
                }
            true
        } ?: false
        fun subscribe(topic: String): Boolean = mqttOperation("subscribe") {
            client?.subscribeWith()
                ?.topicFilter(topic)
                ?.retainAsPublished(true)
                ?.noLocal(true)
                ?.callback { publish: Mqtt5Publish ->
                    val message = String(publish.payloadAsBytes, StandardCharsets.UTF_8)
                    Log.d(tag, "MQTT broker subscribe topic='${publish.topic}', message='$message'")
                    onReceived(this@MqttDeviceConnection, message)
                }
                ?.send()
                ?.whenComplete { subAck: Mqtt5SubAck, throwable ->
                    when {
                        throwable?.message?.contains("not connected", true) == true -> {
                            Log.e(tag, "MQTT broker subscribe topic='$topic' failure: not connected")
                            onDisconnected(this@MqttDeviceConnection)
                        }
                        throwable != null -> {
                            Log.e(tag, "MQTT broker subscribe topic='$topic' failure", throwable)
                            onError(this@MqttDeviceConnection)
                        }
                        else -> {
                            Log.d(tag, "MQTT broker subscribe topic='$topic' success: ${subAck.reasonCodes.joinToString(", ")}")
                        }
                    }
                }
            true
        } ?: false
        fun unsubscribe(topic: String): Boolean = mqttOperation("unsubscribe") {
            client?.unsubscribeWith()
                ?.topicFilter(topic)
                ?.send()
                ?.whenComplete { unsubAck, throwable ->
                    when {
                        throwable?.message?.contains("not connected", true) == true -> {
                            Log.e(tag, "MQTT broker unsubscribe topic='$topic' failure: not connected")
                            onDisconnected(this@MqttDeviceConnection)
                        }
                        throwable != null -> {
                            Log.e(tag, "MQTT broker unsubscribe topic='$topic' failure", throwable)
                            onError(this@MqttDeviceConnection)
                        }
                        else -> {
                            Log.d(tag, "MQTT broker unsubscribe topic='$topic' success: ${unsubAck.reasonCodes.joinToString(", ")}")
                        }
                    }
                }
            true
        } ?: false
        fun publish(topic: String, message: String): Boolean = mqttOperation("publish") {
            client?.publishWith()
                ?.topic(topic)
                ?.payload(message.toByteArray())
                ?.contentType("application/json")
                ?.messageExpiryInterval(60)
                ?.send()
                ?.whenComplete { _, throwable ->
                    when {
                        throwable?.message?.contains("not connected", true) == true -> {
                            Log.e(tag, "MQTT broker publish topic='$topic' failure: not connected")
                            onDisconnected(this@MqttDeviceConnection)
                        }
                        throwable != null -> {
                            Log.e(tag, "MQTT broker publish topic='$topic' failure", throwable)
                            onError(this@MqttDeviceConnection)
                        }
                        else -> {
                            Log.d(tag, "MQTT broker publish topic='$topic', message='$message'")
                        }
                    }
                }
            true
        } ?: false
        fun disconnect() = mqttOperation("disconnect") {
            client?.disconnectWith()
                ?.sessionExpiryInterval(0)
                ?.send()
                ?.whenComplete { _, throwable ->
                    if (throwable != null) {
                        Log.e(tag, "MQTT broker disconnect failure", throwable)
                    } else {
                        Log.d(tag, "MQTT broker disconnect success")
                    }
                }
        }.also {
            client = null
        }
    }

    //

    private var root = "${config.root}/${connectivityInfo.deviceAddress}"
    private var topic = ""

    private val connection = MqttDeviceConnection(tag,
        config,
        connectivityInfo.identity,
        onConnected = {
            Log.d(tag, "Device connected")
            setConnectionIsConnected()
            if (topic.isNotEmpty())
                if (!it.subscribe(topic))
                    setConnectionDoReconnect()
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

    //

    override fun doConnectionStart(): Boolean {
        Log.d(tag, "Device connect")
        return connection.connect()
    }
    override fun doConnectionStop() {
        Log.d(tag, "Device disconnect")
        connection.disconnect()
    }
    override fun doConnectionIdentify(): Boolean {
        return connection.publish("${root}/peer", connectivityInfo.toJsonString())
    }

    //

    fun subscribe(): Boolean {
        if (topic.isNotEmpty()) {
            Log.e(tag, "Device subscribe: failure, already subscribed")
            return false
        }
        topic = "${root}/#"
        return if (!isConnected()) {
            Log.d(tag, "Device subscribe: pending, upon connection")
            true
        } else {
            Log.d(tag, "Device subscribe")
            connection.subscribe(topic)
        }
    }
    fun unsubscribe(): Boolean {
        if (topic.isEmpty()) {
            Log.e(tag, "Device unsubscribe: failure, not subscribed")
            return false
        }
        val topicOld = topic
        topic = ""
        return if (!isConnected()) {
            Log.d(tag, "Device unsubscribe: cleared, not connected")
            true
        } else {
            Log.d(tag, "Device unsubscribe")
            connection.unsubscribe(topicOld)
        }
    }
    fun publish(type: String, message: String): Boolean {
        return if (!isConnected()) {
            Log.e(tag, "Device publish failure: not connected")
            false
        } else {
            Log.d(tag, "Device publish")
            connection.publish("${root}/${type}", message)
        }
    }
}