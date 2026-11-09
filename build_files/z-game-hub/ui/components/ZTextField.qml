import QtQuick
import QtQuick.Controls

TextField {
    id: control
    implicitHeight: 42
    leftPadding: 14
    rightPadding: 14
    color: "#E5FFF4"
    placeholderTextColor: "#668078"
    selectionColor: "#2CF1AA"
    selectedTextColor: "#03100E"
    font.pixelSize: 12
    background: Rectangle {
        radius: 12
        color: control.activeFocus ? "#0D2820" : "#081914"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#46FFC0" : "#245244"
        Behavior on color { ColorAnimation { duration: 140 } }
        Behavior on border.color { ColorAnimation { duration: 140 } }
    }
}
