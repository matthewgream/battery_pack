package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.util.TypedValue
import android.view.GestureDetector
import android.view.MotionEvent
// import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.core.view.GestureDetectorCompat
import org.json.JSONObject

@SuppressLint("ClickableViewAccessibility")
class DataDiagnosticHandler (private val activity: Activity) {
//    private var scrollView: ScrollView = activity.findViewById (R.id.diagnosticScrollView)
    private var textView: TextView = activity.findViewById (R.id.diagDataTextView)
    private var gestureDetector: GestureDetectorCompat

    init {
        gestureDetector = GestureDetectorCompat (activity, object : GestureDetector.SimpleOnGestureListener () {
            override fun onDoubleTap (e: MotionEvent): Boolean {
                clear ()
                return true
            }
        })
        textView.setOnTouchListener { v, event ->
            val result = gestureDetector.onTouchEvent (event)
            if (!result)
                v.performClick ()
            false
        }
    }

    private fun clear () {
        activity.runOnUiThread {
            textView.text = ""
            Toast.makeText (activity, "Diagnostic data cleared", Toast.LENGTH_SHORT).show ()
        }
    }

    fun render (json: JSONObject) {
        val text = json.toString (2)
        activity.runOnUiThread {
            textView.setTextSize (TypedValue.COMPLEX_UNIT_SP, 10f)
            textView.append (text + "\n\n")
//            // Scroll to the bottom
//            scrollView.post {
//                scrollView.fullScroll(ScrollView.FOCUS_DOWN)
//            }
        }
    }
}
