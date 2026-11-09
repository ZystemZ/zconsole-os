import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    implicitHeight: 42
    leftPadding: 14
    rightPadding: 38
    font.pixelSize: 12
    contentItem: Text {
        leftPadding: 2
        text: control.displayText
        color: "#E5FFF4"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Canvas {
        x: control.width - width - 14
        y: (control.height - height) / 2
        width: 10
        height: 7
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = "#46FFC0"
            ctx.lineWidth = 1.5
            ctx.beginPath()
            ctx.moveTo(1, 1)
            ctx.lineTo(width / 2, height - 1)
            ctx.lineTo(width - 1, 1)
            ctx.stroke()
        }
    }
    background: Rectangle {
        radius: 12
        color: control.activeFocus || control.popup.visible ? "#0D2820" : "#081914"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#46FFC0" : "#245244"
    }
    popup: Popup {
        y: control.height + 6
        width: control.width
        padding: 6
        implicitHeight: contentItem.implicitHeight + padding * 2
        contentItem: ListView {
            implicitHeight: Math.min(contentHeight, 280)
            model: control.delegateModel
            clip: true
            currentIndex: control.highlightedIndex
            delegate: ItemDelegate {
                width: ListView.view.width
                height: 36
                highlighted: control.highlightedIndex === index
                contentItem: Text {
                    text: modelData
                    color: parent.highlighted ? "#03100E" : "#D9F8EB"
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                background: Rectangle {
                    radius: 8
                    color: parent.highlighted ? "#2CF1AA" : "transparent"
                }
                onClicked: {
                    control.currentIndex = index
                    control.popup.close()
                }
            }
        }
        background: Rectangle {
            radius: 14
            color: "#07140F"
            border.color: "#2B765B"
            border.width: 1
        }
    }
}
