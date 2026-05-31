import QtQuick
import STM32AiDeployer

// Crisp, vector-drawn line icons for metric cards.
// name ∈ "speed" | "memory" | "target" | "thermometer" | "wave"
Canvas {
    id: root

    property string name: "speed"
    property color color: Theme.primary
    property real lineWidth: 2

    implicitWidth: 22
    implicitHeight: 22

    onColorChanged: requestPaint()
    onNameChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        ctx.strokeStyle = root.color
        ctx.fillStyle = root.color
        ctx.lineWidth = root.lineWidth
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        var w = width, h = height
        var cx = w / 2, cy = h / 2

        if (name === "speed") {
            // gauge arc + needle
            var r = w * 0.36
            ctx.beginPath()
            ctx.arc(cx, cy + r * 0.25, r, Math.PI * 0.85, Math.PI * 0.15, false)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(cx, cy + r * 0.25)
            ctx.lineTo(cx + r * 0.55, cy - r * 0.35)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(cx, cy + r * 0.25, 1.6, 0, Math.PI * 2, false)
            ctx.fill()
        } else if (name === "memory") {
            // chip body + pins
            var s = w * 0.40
            ctx.strokeRect(cx - s, cy - s, s * 2, s * 2)
            // inner square
            ctx.strokeRect(cx - s * 0.45, cy - s * 0.45, s * 0.9, s * 0.9)
            // pins
            var pin = s * 0.4
            for (var i = -1; i <= 1; i++) {
                var off = i * s * 0.7
                // top
                ctx.beginPath(); ctx.moveTo(cx + off, cy - s); ctx.lineTo(cx + off, cy - s - pin); ctx.stroke()
                // bottom
                ctx.beginPath(); ctx.moveTo(cx + off, cy + s); ctx.lineTo(cx + off, cy + s + pin); ctx.stroke()
                // left
                ctx.beginPath(); ctx.moveTo(cx - s, cy + off); ctx.lineTo(cx - s - pin, cy + off); ctx.stroke()
                // right
                ctx.beginPath(); ctx.moveTo(cx + s, cy + off); ctx.lineTo(cx + s + pin, cy + off); ctx.stroke()
            }
        } else if (name === "target") {
            // concentric circles + center dot
            ctx.beginPath(); ctx.arc(cx, cy, w * 0.38, 0, Math.PI * 2, false); ctx.stroke()
            ctx.beginPath(); ctx.arc(cx, cy, w * 0.22, 0, Math.PI * 2, false); ctx.stroke()
            ctx.beginPath(); ctx.arc(cx, cy, 1.8, 0, Math.PI * 2, false); ctx.fill()
        } else if (name === "thermometer") {
            var bx = cx, top = h * 0.16, bulbR = w * 0.16
            ctx.beginPath()
            ctx.moveTo(bx - w * 0.07, top)
            ctx.lineTo(bx - w * 0.07, cy + h * 0.12)
            ctx.arc(bx, cy + h * 0.18, bulbR, Math.PI, Math.PI * 2 + 0.0001, true)
            ctx.lineTo(bx + w * 0.07, top)
            ctx.arc(bx, top, w * 0.07, 0, Math.PI, true)
            ctx.stroke()
            ctx.beginPath(); ctx.arc(bx, cy + h * 0.18, bulbR * 0.55, 0, Math.PI * 2, false); ctx.fill()
        } else if (name === "wave") {
            // signal/UART wave
            ctx.beginPath()
            var steps = 5
            for (var k = 0; k <= steps; k++) {
                var x = w * 0.15 + (w * 0.7) * (k / steps)
                var y = cy + Math.sin(k * 1.6) * h * 0.22
                if (k === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
            }
            ctx.stroke()
        }
    }
}
