import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool accent: false
    implicitHeight: 44
    leftPadding: 18
    rightPadding: 18
    contentItem: Text {
        text: control.text
        color: control.accent ? "#03100E" : "#D9F8EB"
        font.pixelSize: 12
        font.bold: control.accent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 18
        color: control.down ? (control.accent ? "#12B887" : "#1A3B31")
                            : control.hovered ? (control.accent ? "#50FFC0" : "#15382D")
                                              : (control.accent ? "#2CF1AA" : "#0D211B")
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#B7FFE8" : (control.accent ? "#46FFC0" : "#275848")
        Behavior on color { ColorAnimation { duration: 160 } }
    }
}
