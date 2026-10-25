import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property string title: "Jogo"
    property string platform: "PC"
    property string artwork: ""
    width: 180
    height: 240
    radius: 10
    color: "#0B1110"
    border.color: "#1C302C"
    Column {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8
        Rectangle {
            width: parent.width
            height: 160
            radius: 7
            color: "#10332E"
            Text { anchors.centerIn: parent; text: "Z"; color: "#16DCC0"; font.pixelSize: 72; font.bold: true }
        }
        Text { text: root.title; color: "#F3F7F6"; font.bold: true; elide: Text.ElideRight; width: parent.width }
        Text { text: root.platform; color: "#8FA49E"; font.pixelSize: 11 }
    }
}
