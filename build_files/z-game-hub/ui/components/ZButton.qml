import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool accent: false
    implicitHeight: 38
    leftPadding: 14
    rightPadding: 14
    contentItem: Text {
        text: control.text
        color: control.accent ? "#03100E" : "#DCEBE7"
        font.pixelSize: 12
        font.bold: control.accent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 8
        color: control.down ? (control.accent ? "#0DB796" : "#18312D")
                            : control.hovered ? (control.accent ? "#29E8C6" : "#142923")
                                              : (control.accent ? "#12D9BA" : "#101B19")
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#8AFFF0" : (control.accent ? "#1EE8C6" : "#294840")
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
