import QtQuick
import QtQuick.Controls

TextArea {
    id: control
    padding: 14
    color: "#BFEBDD"
    placeholderTextColor: "#668078"
    selectionColor: "#2CF1AA"
    selectedTextColor: "#03100E"
    font.pixelSize: 11
    background: Rectangle {
        radius: 12
        color: control.activeFocus ? "#0D2820" : "#07140F"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#46FFC0" : "#1E4C3D"
    }
}
