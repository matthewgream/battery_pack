package com.example.battery_monitor

import android.annotation.SuppressLint
import android.app.Activity
import android.util.TypedValue
import android.view.GestureDetector
import android.view.MotionEvent
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import org.json.JSONObject

@SuppressLint("ClickableViewAccessibility")
class DataProcessorDiagnostic (private val activity: Activity) {

    private val scrollView: ScrollView = activity.findViewById (R.id.diagnosticScrollView)
    private val textView: TextView = activity.findViewById (R.id.diagDataTextView)
    private var isScrolledToBottom = true

    init {
        val detector = GestureDetector (activity, object : GestureDetector.SimpleOnGestureListener () {
            override fun onDoubleTap (e: MotionEvent): Boolean {
                clear ()
                return true
            }
        })
        textView.setOnTouchListener { v, event ->
            if (!detector.onTouchEvent (event))
                v.performClick ()
            true
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