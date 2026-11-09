import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property string title: "Jogo"
    property string platform: "PC"
    property string artwork: ""
    width: 206
    height: 288
    radius: 18
    color: "#091511"
    border.color: "#1A4437"
    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10
        Rectangle {
            width: parent.width
            height: 198
            radius: 12
            clip: true
            color: "#0E3028"
            Image {
                anchors.fill: parent
                source: root.artwork.length ? root.artwork : "qrc:/ZGameHub/assets/game-card-placeholder.png"
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
            Text { anchors.centerIn: parent; visible: root.artwork.length === 0; text: "Z"; color: "#16DCC0"; font.pixelSize: 72; font.bold: true }
        }
        Text { text: root.title; color: "#F3F7F6"; font.bold: true; elide: Text.ElideRight; width: parent.width }
        Text { text: root.platform; color: "#8FA49E"; font.pixelSize: 11 }
    }
}
