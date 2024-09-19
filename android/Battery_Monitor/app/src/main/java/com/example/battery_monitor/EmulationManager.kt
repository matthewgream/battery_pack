package com.example.battery_monitor

import android.content.Context
import android.os.Build
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader

object EmulationManager {

    fun isEmulator (): Boolean {
        return (Build.BRAND.startsWith("generic") && Build.DEVICE.startsWith("generic")
                || Build.FINGERPRINT.startsWith("generic")
                || Build.FINGERPRINT.startsWith("unknown")
                || Build.HARDWARE.contains("goldfish")
                || Build.HARDWARE.contains("ranchu")
                || Build.MODEL.contains("google_sdk")
                || Build.MODEL.contains("Emulator")
                || Build.MODEL.contains("Android SDK built for x86")
                || Build.MANUFACTURER.contains("Genymotion")
                || Build.PRODUCT.contains("sdk_google")
                || Build.PRODUCT.contains("google_sdk")
                || Build.PRODUCT.contains("sdk")
                || Build.PRODUCT.contains("sdk_x86")
                || Build.PRODUCT.contains("sdk_gphone64_arm64")
                || Build.PRODUCT.contains("vbox86p")
                || Build.PRODUCT.contains("emulator")
                || Build.PRODUCT.contains("simulator"))
    }

    fun createTestData (context: Context, resource: Int): JSONObject {
        val jsonObject = JSONObject (BufferedReader (InputStreamReader (context.resources.openRawResource(resource))).use { it.readText() })
        jsonObject.put ("time", System.currentTimeMillis () / 1000)
        return jsonObject
    }

    val createTestDataOperational: (Context) -> JSONObject = { context -> createTestData (context, R.raw.simulated_data_operational) }
    val createTestDataDiagnostic: (Context) -> JSONObject = { context -> createTestData (context, R.raw.simulated_data_diagnostic) }
}
