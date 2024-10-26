package com.example.battery_monitor

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Shader
import android.util.AttributeSet
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.View

class DataViewBatteryTemperature @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val paintLine = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = 4f
        color = context.getColor(R.color.graph_line_color)
    }
    private val paintText = Paint().apply {
        textSize = 30f
        color = context.getColor(R.color.graph_text_color)
    }
    private val paintLineDashed = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = 2f
        color = context.getColor(R.color.graph_line_color)
        pathEffect = DashPathEffect(floatArrayOf(10f, 10f), 0f)
    }
    private val paintLineThreshold = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = 2f
        color = context.getColor(android.R.color.holo_red_light)
    }
    private val paintLinePath = Paint().apply {
        style = Paint.Style.STROKE
        strokeWidth = 4f
        color = Color.rgb(64, 64, 64)
        pathEffect = DashPathEffect(floatArrayOf(20f, 10f), 0f)
    }

    private val thresholds = listOf(
        Triple(25f, "25°C", context.getColor(R.color.threshold_min)),
        Triple(35f, "35°C", context.getColor(R.color.threshold_warn)),
        Triple(45f, "45°C", context.getColor(R.color.threshold_max))
    )

    private var temperatureValues: List<Float> = emptyList()
    private var temperatureHistoryValues: MutableList<List<Float>> = mutableListOf()
    private val temperatureHistorySize = 16

    init {
        val detector = GestureDetector(context, object : GestureDetector.SimpleOnGestureListener() {
            override fun onDoubleTap(e: MotionEvent): Boolean {
                clearTemperatureValues()
                return true
            }
        })
        setOnTouchListener { v, event ->
            if (!detector.onTouchEvent(event))
                v.performClick()
            true
        }
    }

    fun clearTemperatureValues() {
        temperatureHistoryValues.clear()
        if (temperatureValues.isNotEmpty())
            temperatureHistoryValues.add(temperatureValues)
        invalidate()
    }

    fun setTemperatureValues(values: List<Float>) {
        temperatureHistoryValues.add(0, values)
        if (temperatureHistoryValues.size > temperatureHistorySize)
            temperatureHistoryValues.removeAt(temperatureHistorySize)
        temperatureValues = values
        invalidate()
    }

    @SuppressLint("DefaultLocale", "DrawAllocation")
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        if (temperatureValues.isEmpty()) return

        val maxTemp = temperatureHistoryValues.flatten().maxOrNull() ?: 0f
        val minTemp = temperatureHistoryValues.flatten().minOrNull() ?: 0f

        val width = width.toFloat()
        val height = height.toFloat()
        val count = temperatureValues.size
        val paddingTop = 50f
        val paddingBottom = 20f
        val paddingHorizontal = width / (count * 2)
        val drawableWidth = width - (2 * paddingHorizontal)
        val drawableHeight = height - paddingTop - paddingBottom

        for (i in temperatureHistoryValues.size - 1 downTo 1) {
            val olderValues = temperatureHistoryValues[i]
            val olderFraction = i.toFloat() / (temperatureHistoryValues.size - 1)
            val newerValues = temperatureHistoryValues[i - 1]
            val newerFraction = (i - 1).toFloat() / (temperatureHistoryValues.size - 1)
            val areaPath = Path()
            olderValues.forEachIndexed { index, temp ->
                val x = paddingHorizontal + (drawableWidth * index / (count - 1))
                val y =
                    paddingTop + drawableHeight - (drawableHeight * (temp - minTemp) / (maxTemp - minTemp))
                if (index == 0) areaPath.moveTo(x, y) else areaPath.lineTo(x, y)
            }
            for (index in newerValues.indices.reversed()) {
                val x = paddingHorizontal + (drawableWidth * index / (count - 1))
                val y =
                    paddingTop + drawableHeight - (drawableHeight * (newerValues[index] - minTemp) / (maxTemp - minTemp))
                areaPath.lineTo(x, y)
            }
            areaPath.close()
            val areaPaint = Paint().apply {
                style = Paint.Style.FILL
                shader = LinearGradient(
                    0f,
                    0f,
                    0f,
                    height,
                    interpolateColor(olderFraction),
                    interpolateColor(newerFraction),
                    Shader.TileMode.CLAMP
                )
            }
            canvas.drawPath(areaPath, areaPaint)
        }

        for (i in temperatureHistoryValues.size - 1 downTo 1) {
            val values = temperatureHistoryValues[i]
            val fraction = i.toFloat() / (temperatureHistoryValues.size - 1)
            val linePath = Path()
            values.forEachIndexed { index, temp ->
                val x = paddingHorizontal + (drawableWidth * index / (count - 1))
                val y =
                    paddingTop + drawableHeight - (drawableHeight * (temp - minTemp) / (maxTemp - minTemp))
                if (index == 0) linePath.moveTo(x, y) else linePath.lineTo(x, y)
            }
            val linePaint = Paint(paintLine).apply { color = interpolateColor(fraction) }
            canvas.drawPath(linePath, linePaint)
        }

        for ((temp, label, color) in thresholds) {
            if (temp in minTemp..maxTemp) {
                val x = paddingHorizontal
                val y =
                    paddingTop + drawableHeight - (drawableHeight * (temp - minTemp) / (maxTemp - minTemp))
                val paint = Paint(paintLineThreshold).apply { this.color = color }
                canvas.drawLine(x, y, width - paddingHorizontal, y, paint)
                canvas.drawText(label, x + 10f, y - 10f, paintText)
            }
        }

        val linePath = Path()
        temperatureValues.forEachIndexed { index, temp ->
            val x = paddingHorizontal + (drawableWidth * index / (count - 1))
            val y =
                paddingTop + drawableHeight - (drawableHeight * (temp - minTemp) / (maxTemp - minTemp))
            if (index == 0) linePath.moveTo(x, y) else linePath.lineTo(x, y)
        }
        canvas.drawPath(linePath, paintLinePath)

        temperatureValues.forEachIndexed { index, temp ->
            val x = paddingHorizontal + (drawableWidth * index / (count - 1))
            val y =
                paddingTop + drawableHeight - (drawableHeight * (temp - minTemp) / (maxTemp - minTemp))
            canvas.drawLine(x, paddingTop, x, height - paddingBottom, paintLineDashed)
            val text = String.format("%.1f°C", temp)
            val textWidth = paintText.measureText(text)
            var textX = x - (textWidth / 2)
            if (textX < paddingHorizontal)
                textX = paddingHorizontal
            else if (textX + textWidth > width - paddingHorizontal)
                textX = width - paddingHorizontal - textWidth
            canvas.drawText(text, textX, y - 20, paintText)
        }
    }

    private fun interpolateColor(fraction: Float): Int {
        return Color.rgb(255, (255 * fraction).toInt(), (255 * fraction).toInt())
    }
}