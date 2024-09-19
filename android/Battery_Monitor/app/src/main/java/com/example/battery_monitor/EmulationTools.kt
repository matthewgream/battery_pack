package com.example.battery_monitor

import android.content.Context
import android.os.Build
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader

object EmulationTools {

    fun isEmulator (): Boolean {
        return Build.DEVICE.contains ("emu")
    }

    fun createTestData (context: Context, resource: Int): JSONObject {
        val jsonObject = JSONObject (BufferedReader (InputStreamReader (context.resources.openRawResource(resource))).use { it.readText() })
        jsonObject.put ("time", System.currentTimeMillis () / 1000)
        return jsonObject
    }

    val createTestDataOperational: (Context) -> JSONObject = { context -> createTestData (context, R.raw.simulated_data_operational) }
    val createTestDataDiagnostic: (Context) -> JSONObject = { context -> createTestData (context, R.raw.simulated_data_diagnostic) }
}
