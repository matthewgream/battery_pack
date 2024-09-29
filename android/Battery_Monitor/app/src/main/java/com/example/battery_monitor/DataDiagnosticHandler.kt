package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.util.TypedValue
import android.view.GestureDetector
import android.view.MotionEvent
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.core.view.GestureDetectorCompat
import org.json.JSONObject

@SuppressLint("ClickableViewAccessibility")
class DataDiagnosticHandler (private val activity: Activity) {

    private val scrollView: ScrollView = activity.findViewById (R.id.diagnosticScrollView)
    private val textView: TextView = activity.findViewById (R.id.diagDataTextView)
    private val gestureDetector: GestureDetectorCompat = GestureDetectorCompat (activity, object : GestureDetector.SimpleOnGestureListener () {
        override fun onDoubleTap (e: MotionEvent): Boolean {
            clear ()
            return true
        }
    })
    private var isScrolledToBottom = true

    init {
        textView.setOnTouchListener { v, event ->
            val result = gestureDetector.onTouchEvent (event)
            if (!result)
                v.performClick ()
            false
        }
        scrollView.viewTreeObserver.addOnScrollChangedListener {
            val view = scrollView.getChildAt (scrollView.childCount - 1)
            val diff = (view.bottom - (scrollView.height + scrollView.scrollY))
            isScrolledToBottom = diff <= 0
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
            if (isScrolledToBottom)
                scrollView.post {
                    scrollView.fullScroll (ScrollView.FOCUS_DOWN)
                }
        }
    }
}