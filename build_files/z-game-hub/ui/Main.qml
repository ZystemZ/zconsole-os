import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import ZGameHub

ApplicationWindow {
    id: window
    width: 1440
    height: 900
    minimumWidth: 1120
    minimumHeight: 720
    visible: true
    title: "Z-GAME HUB"
    color: "#020706"

    property color neon: "#34F6AE"
    property color neonSoft: "#12CFA0"
    property color ink: "#F4FFF9"
    property color muted: "#8AA69B"
    property color panel: "#091513"
    property color panelRaised: "#0D211D"

    property string activePage: "Início"
    property string searchText: ""
    property string romSearchText: ""
    property string platformFilter: "Todas"
    property int editingId: -1
    property int coverEditingId: -1
    property string pendingUsbPath: ""
    property string pendingQuiverTitle: ""
    property var pendingQuiverExecutables: []
    property var pendingQuiverAsset: ({})
    property string quiverSearchText: ""
    property string quiverCategory: "Todas"
    property var platformSummaries: []
    property var platformFilterOptions: ["Todas"]

    Shortcut {
        sequence: "F11"
        onActivated: {
            if (window.visibility === Window.FullScreen) window.showNormal()
            else window.showFullScreen()
        }
    }

    function libraryItems() {
        return gameCatalog.games.concat(zcardService.cards)
    }

    function launchLibraryItem(item) {
        if (item.source === "zcard") {
            if (zcardService.openCard(item.path)) launchMountedZCard()
            return
        }
        gameCatalog.launchGame(item.id)
    }

    function refreshPlatformSummaries() {
        var counts = ({})
        var games = libraryItems()
        for (var i = 0; i < games.length; ++i) {
            var platform = games[i].platform || "PC"
            counts[platform] = (counts[platform] || 0) + 1
        }
        var names = Object.keys(counts).sort()
        var summaries = []
        var options = ["Todas"]
        for (var j = 0; j < names.length; ++j) {
            summaries.push({ platform: names[j], count: counts[names[j]] })
            options.push(names[j])
        }
        platformSummaries = summaries
        platformFilterOptions = options
        if (platformFilter !== "Todas" && options.indexOf(platformFilter) < 0)
            platformFilter = "Todas"
    }

    function pageDescription() {
        if (activePage === "Minha biblioteca") return "Pesquise, filtre e inicie seus jogos e aplicativos."
        if (activePage === "Plataformas") return "Sua coleção organizada pelas plataformas encontradas."
        if (activePage === "Dispositivos") return "Unidades removíveis detectadas e prontas para varredura."
        if (activePage === "Loja e descoberta") return "Encontre jogos instalados nos launchers conectados ao computador."
        if (activePage === "Scanner") return "Controle as buscas e revise os candidatos encontrados."
        if (activePage === "ROMs e BIOS") return "Encontre arquivos de jogos e confira arquivos comuns de suporte nas suas pastas locais."
        if (activePage === "Quiver") return "Pesquise o catálogo e instale ou inicie ports sem sair do Z-GAME HUB."
        if (activePage === "ZCARD / arquivos") return "Monte, inspecione e gere cartões compatíveis com o ecossistema ZConsole."
        if (activePage === "Configurações") return "Preferências da biblioteca e dos emuladores."
        return "Sua coleção. Seu universo. Do seu jeito."
    }

    Component.onCompleted: {
        window.showFullScreen()
        zcardService.scanCards()
        refreshPlatformSummaries()
    }
    onActivePageChanged: {
        if (activePage === "Quiver" && quiverIntegration.apps.length === 0 && !quiverIntegration.catalogBusy)
            quiverIntegration.refreshCatalog()
    }

    MessageDialog {
        id: exitHubDialog
        title: "Voltar ao desktop?"
        text: "O Z-GAME HUB será fechado. A sessão do desktop continuará aberta."
        buttons: MessageDialog.Yes | MessageDialog.No
        onAccepted: Qt.quit()
    }

    background: Rectangle {
        color: "#020706"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#061814" }
            GradientStop { position: 0.38; color: "#020807" }
            GradientStop { position: 1.0; color: "#010303" }
        }
        Rectangle {
            width: 520; height: 520; radius: 260; x: -160; y: -220; color: "#0CE0A0"; opacity: 0.055
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { to: 0.095; duration: 2600; easing.type: Easing.InOutSine }
                NumberAnimation { to: 0.045; duration: 2600; easing.type: Easing.InOutSine }
            }
        }
        Rectangle { width: 620; height: 620; radius: 310; anchors.right: parent.right; y: 110; color: "#1E7BFF"; opacity: 0.035 }
        Rectangle { anchors.fill: parent; color: "transparent"; border.color: "#0E342A"; border.width: 1; opacity: 0.7 }
    }

    function addSelectedFiles(urls) {
        var added = 0
        for (var i = 0; i < urls.length; ++i) {
            var path = gameCatalog.localPathFromUrl(urls[i].toString())
            if (gameCatalog.addPath(path, "PC")) added++
        }
        if (added > 0) gameCatalog.refresh()
    }

    function openEditor(game) {
        editingId = game.id
        gameTitle.text = game.title
        gamePath.text = game.path
        gamePlatform.text = game.platform
        gameCommand.text = game.launchCommand
        gameCover.text = game.coverPath || ""
        addDialog.open()
    }

    function openCoverEditor(game) {
        coverEditingId = game.id
        coverEditPath.text = game.coverPath || ""
        coverDialog.open()
    }

    function comboIndex(items, value) {
        for (var i = 0; i < items.length; ++i)
            if (items[i] === value) return i
        return 0
    }

    function hasEmulator(platform) {
        var configs = gameCatalog.emulatorConfigs
        for (var i = 0; i < configs.length; ++i)
            if (configs[i].platform === platform && configs[i].enabled && configs[i].executable)
                return true
        return false
    }

    function applyEmulatorPreset() {
        if (emulatorPreset.currentText === "Tela cheia") emulatorArguments.text = "--fullscreen {ROM}"
        else if (emulatorPreset.currentText === "Janela") emulatorArguments.text = "--windowed {ROM}"
        else if (emulatorPreset.currentText === "Tela cheia 4:3") emulatorArguments.text = "--fullscreen --aspect-ratio 4:3 {ROM}"
        else if (emulatorPreset.currentText === "Tela cheia 16:9") emulatorArguments.text = "--fullscreen --aspect-ratio 16:9 {ROM}"
    }

    function launchMountedZCard() {
        var plan = zcardService.resolveLaunchPlan()
        if (!plan.ok) {
            zcardService.showMessage(plan.message)
            return
        }
        gameCatalog.launchExternalFile(plan.path, plan.platform)
    }

    header: Rectangle {
        height: 88
        color: "#06100E"
        opacity: 0.96
        border.color: "#174237"
        border.width: 1
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 30
            anchors.rightMargin: 30
            spacing: 18
            Rectangle {
                width: 48; height: 48; radius: 16
                rotation: -7
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#39FFAA" }
                    GradientStop { position: 1.0; color: "#0AAE89" }
                }
                Image { anchors.centerIn: parent; width: 34; height: 34; source: "qrc:/ZGameHub/assets/zconsole-mark.svg"; fillMode: Image.PreserveAspectFit; rotation: 7 }
                SequentialAnimation on scale {
                    loops: Animation.Infinite
                    NumberAnimation { to: 1.05; duration: 1800; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 1800; easing.type: Easing.InOutSine }
                }
            }
            Column {
                spacing: 1
                Text { text: "Z-GAME HUB"; color: ink; font.pixelSize: 21; font.bold: true; font.letterSpacing: 3 }
                Text { text: "JOGUE. DESCUBRA. EXPLORE."; color: neon; font.pixelSize: 9; font.bold: true; font.letterSpacing: 2 }
            }
            Item { Layout.fillWidth: true }
            ZTextField {
                id: search
                Layout.preferredWidth: 310
                Layout.preferredHeight: 44
                placeholderText: "Buscar na biblioteca..."
                color: ink
                placeholderTextColor: "#6D8980"
                leftPadding: 18
                background: Rectangle { radius: 22; color: "#0B1B17"; border.width: 1; border.color: search.activeFocus ? neon : "#1C4338"; Behavior on border.color { ColorAnimation { duration: 180 } } }
                onTextChanged: window.searchText = text.toLowerCase()
            }
                ZButton {
                    text: "＋  Adicionar jogo/app"
                    accent: true
                    onClicked: addDialog.open()
                }
                ZButton { text: "Emuladores"; onClicked: emulatorDialog.open() }
            }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 24

        Rectangle {
            Layout.preferredWidth: 248
            Layout.fillHeight: true
            radius: 24
            color: "#07120F"
            border.color: "#12382F"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 9
                Text { text: "CENTRAL DE JOGOS"; color: "#5F887A"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2; leftPadding: 10; bottomPadding: 8 }
                Repeater {
                    model: [
                        {label:"Início", icon:"⌂"},
                        {label:"Minha biblioteca", icon:"▦"},
                        {label:"Plataformas", icon:"◈"},
                        {label:"Dispositivos", icon:"▤"},
                        {label:"Loja e descoberta", icon:"◇"},
                        {label:"Scanner", icon:"⌕"},
                        {label:"ROMs e BIOS", icon:"▣"},
                        {label:"Quiver", icon:"↗"},
                        {label:"ZCARD / arquivos", icon:"▤"},
                        {label:"Configurações", icon:"⚙"}
                    ]
                    delegate: ZButton {
                        id: navButton
                        Layout.fillWidth: true
                        text: modelData.icon + "    " + modelData.label
                        onClicked: window.activePage = modelData.label
                        implicitHeight: 46
                        background: Rectangle { radius: 16; color: window.activePage === modelData.label ? "#123A2F" : (navButton.hovered ? "#0C241D" : "transparent"); border.width: window.activePage === modelData.label ? 1 : 0; border.color: neon; Behavior on color { ColorAnimation { duration: 160 } } }
                        contentItem: Text { text: navButton.text; color: window.activePage === modelData.label ? neon : "#A6C1B6"; verticalAlignment: Text.AlignVCenter; leftPadding: 14; font.pixelSize: 13; font.bold: window.activePage === modelData.label }
                    }
                }
                Item { Layout.fillHeight: true }
                Rectangle {
                    Layout.fillWidth: true
                    radius: 10
                    color: "#0B211B"
                    border.color: "#1B5B47"
                    implicitHeight: 112
                    Column {
                        anchors.fill: parent; anchors.margins: 12; spacing: 6
                        Text { text: "ZYSTEM SCANNER"; color: "#14DCC0"; font.bold: true; font.pixelSize: 11 }
                        Text { text: gameScanner.status; color: "#A8B9B5"; wrapMode: Text.Wrap; width: parent.width; font.pixelSize: 11 }
                        RowLayout {
                            spacing: 6
                            ZButton { Layout.fillWidth: true; implicitHeight: 34; text: gameScanner.busy ? "Buscando..." : "Procurar"; enabled: !gameScanner.busy; onClicked: gameScanner.scanCommonLocations() }
                            ZButton { Layout.fillWidth: true; implicitHeight: 34; text: "Sair"; onClicked: exitHubDialog.open() }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 18
            RowLayout {
                Layout.fillWidth: true
                Column {
                    spacing: 4
                    Text { text: window.activePage.toUpperCase(); color: ink; font.pixelSize: 30; font.bold: true; font.letterSpacing: 2 }
                    Text { text: window.pageDescription(); color: muted; font.pixelSize: 13 }
                }
                Item { Layout.fillWidth: true }
                ZComboBox {
                    id: platformFilterBox
                    Layout.preferredWidth: 150
                    visible: window.activePage === "Início" || window.activePage === "Minha biblioteca"
                    model: window.platformFilterOptions
                    onCurrentTextChanged: window.platformFilter = currentText
                }
                ZButton { visible: window.activePage === "Scanner" || window.activePage === "Loja e descoberta"; text: "Steam"; onClicked: gameScanner.scanSteamLibraries() }
                ZButton { visible: window.activePage === "Scanner" || window.activePage === "Loja e descoberta"; text: "Epic / Ubisoft / Heroic"; onClicked: gameScanner.scanExternalLaunchers() }
                ZButton { text: "⟳ Atualizar"; onClicked: { gameCatalog.refresh(); gameScanner.scanCommonLocations() } }
                ZButton { visible: window.activePage === "Loja e descoberta"; text: "Z-GameStore V2"; onClicked: gameLauncher.launchCommand("/usr/bin/zgamestore-gui") }
                ZButton { visible: window.activePage === "Loja e descoberta"; text: "Navegador Web"; onClicked: gameLauncher.launchCommand("/usr/bin/chromium-browser") }
            }

            Rectangle {
                visible: window.activePage === "Início"
                Layout.fillWidth: true
                Layout.preferredHeight: 232
                radius: 24
                clip: true
                Image {
                    anchors.fill: parent
                    source: "qrc:/ZGameHub/assets/z-hub-hero.png"
                    fillMode: Image.PreserveAspectCrop
                    opacity: 0.48
                    asynchronous: true
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.56; duration: 3000; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 0.42; duration: 3000; easing.type: Easing.InOutSine }
                    }
                }
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#0B3D2D" }
                    GradientStop { position: 0.46; color: "#09251F" }
                    GradientStop { position: 1.0; color: "#07100E" }
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 28
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "EM DESTAQUE AGORA"; color: neon; font.pixelSize: 11; font.bold: true; font.letterSpacing: 3 }
                        Text { text: "Todos os seus jogos.\nEm um só lugar."; color: ink; font.pixelSize: 34; font.bold: true }
                        Text { text: "Steam • PC • Windows • Retro • Emuladores"; color: "#A5C5BE"; font.pixelSize: 13 }
                    }
                    Rectangle {
                        Layout.preferredWidth: 154; Layout.preferredHeight: 154; radius: 30
                        color: "#10372D"; border.color: neon; border.width: 2
                        rotation: 4
                        Text { anchors.centerIn: parent; text: "Z"; color: neon; font.pixelSize: 108; font.bold: true; rotation: -4 }
                    }
                }
            }

            RowLayout {
                visible: window.activePage === "Início"
                Layout.fillWidth: true
                Text { text: "SUA BIBLIOTECA"; color: "#F0F6F4"; font.pixelSize: 17; font.bold: true; font.letterSpacing: 1 }
                Item { Layout.fillWidth: true }
                Text { text: window.libraryItems().length + " jogos, apps e ZCARDs"; color: "#7F9690"; font.pixelSize: 12 }
            }

            ListView {
                id: gameCarousel
                visible: window.activePage === "Início"
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: ListView.Horizontal
                spacing: 14
                clip: true
                snapMode: ListView.SnapToItem
                highlightRangeMode: ListView.StrictlyEnforceRange
                preferredHighlightBegin: width * 0.04
                preferredHighlightEnd: width * 0.36
                highlightMoveDuration: 160
                keyNavigationEnabled: true
                activeFocusOnTab: true
                focus: true
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                        if (currentItem) launchLibraryItem(currentItem.modelData)
                        event.accepted = true
                    }
                }
                model: window.libraryItems().filter(function(g) {
                    return g.title.toLowerCase().indexOf(window.searchText) >= 0
                        && (window.platformFilter === "Todas" || g.platform === window.platformFilter)
                })
                delegate: Rectangle {
                    required property var modelData
                    width: 226; height: 326; radius: 20
                    color: ListView.isCurrentItem ? "#102D24" : "#091310"; border.width: ListView.isCurrentItem ? 2 : 1; border.color: ListView.isCurrentItem ? neon : "#1C3A31"
                    scale: ListView.isCurrentItem ? 1.0 : 0.88
                    opacity: ListView.isCurrentItem ? 1.0 : 0.72
                    Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 220 } }
                    Column {
                        anchors.fill: parent; anchors.margins: 12; spacing: 10
                        Rectangle {
                            width: parent.width; height: 222; radius: 14
                            clip: true
                            Image {
                                anchors.fill: parent
                                source: modelData.coverUrl ? modelData.coverUrl : "qrc:/ZGameHub/assets/game-card-placeholder.png"
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                opacity: modelData.coverUrl ? 1.0 : 0.84
                            }
                            Rectangle { anchors.fill: parent; color: "#06110D"; opacity: 0.18 }
                            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 100; gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: "#050B08" } } }
                            Text { anchors.centerIn: parent; visible: !modelData.coverUrl; text: "Z"; color: "#18DCC0"; font.pixelSize: 70; font.bold: true; opacity: 0.65 }
                            Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 8; text: modelData.platform; color: "#B9F8EB"; font.pixelSize: 10 }
                        }
                        Text { text: modelData.title; color: "#F3F7F6"; font.pixelSize: 13; font.bold: true; elide: Text.ElideRight; width: parent.width }
                        Text { text: modelData.available ? "● Disponível" : "○ Indisponível"; color: modelData.available ? "#17DDBB" : "#E5B76A"; font.pixelSize: 10 }
                        RowLayout {
                            width: parent.width
                            spacing: 5
                            ZButton {
                                Layout.fillWidth: true
                                text: "▶ Iniciar"
                                accent: true
                                onClicked: window.launchLibraryItem(modelData)
                            }
                            ZButton {
                                text: "▧"
                                onClicked: openCoverEditor(modelData)
                                ToolTip.visible: hovered
                                ToolTip.text: "Alterar capa"
                            }
                            ZButton {
                                visible: modelData.source === "manual"
                                text: "✎"
                                onClicked: openEditor(modelData)
                                ToolTip.visible: hovered
                                ToolTip.text: "Editar"
                            }
                            ZButton {
                                visible: modelData.source === "manual"
                                text: "×"
                                onClicked: { removeDialog.pendingId = modelData.id; removeDialog.open() }
                                ToolTip.visible: hovered
                                ToolTip.text: "Excluir"
                            }
                        }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: gameCarousel.count === 0
                    text: "Sua biblioteca está esperando por jogos e apps.\nUse “Adicionar jogo/app” ou “Procurar jogos”."
                    color: "#81928D"; horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5
                }
            }
            Loader {
                id: pageLoader
                visible: window.activePage !== "Início"
                active: visible
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceComponent: window.activePage === "Minha biblioteca" ? libraryPage
                    : window.activePage === "Plataformas" ? platformsPage
                    : window.activePage === "Dispositivos" ? devicesPage
                    : window.activePage === "Loja e descoberta" ? discoveryPage
                    : window.activePage === "Scanner" ? scannerPage
                    : window.activePage === "ROMs e BIOS" ? romsPage
                    : window.activePage === "Quiver" ? quiverPage
                    : window.activePage === "ZCARD / arquivos" ? zcardPage
                    : window.activePage === "Configurações" ? settingsPage
                    : null
            }
        }
    }

    Component {
        id: libraryPage
        ColumnLayout {
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Text { text: "TODOS OS JOGOS E APLICATIVOS"; color: "#F0F6F4"; font.pixelSize: 16; font.bold: true }
                Item { Layout.fillWidth: true }
                Text {
                    text: gameCatalog.games.filter(function(g) {
                        return g.title.toLowerCase().indexOf(window.searchText) >= 0
                            && (window.platformFilter === "Todas" || g.platform === window.platformFilter)
                    }).length + " item(ns)"
                    color: "#7F9690"; font.pixelSize: 12
                }
            }
            ListView {
                id: libraryList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: window.libraryItems().filter(function(g) {
                    return g.title.toLowerCase().indexOf(window.searchText) >= 0
                        && (window.platformFilter === "Todas" || g.platform === window.platformFilter)
                })
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 76
                    radius: 9
                    color: "#0B1110"
                    border.color: "#1C302C"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 12
                        Rectangle {
                            Layout.preferredWidth: 42
                            Layout.preferredHeight: 56
                            radius: 5
                            clip: true
                            color: "#101918"
                            Image {
                                anchors.fill: parent
                                source: modelData.coverUrl ? modelData.coverUrl : "qrc:/ZGameHub/assets/game-card-placeholder.png"
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text { text: modelData.title; color: "#F3F7F6"; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                            Text { text: modelData.platform + " • " + modelData.source + " • " + modelData.path; color: "#829A93"; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        }
                        Text { text: modelData.available ? "Disponível" : "Indisponível"; color: modelData.available ? "#17DDBB" : "#E5B76A"; font.pixelSize: 10 }
                        ZButton { text: modelData.source === "zcard" ? "▶ Montar / iniciar" : "▶ Iniciar"; accent: true; onClicked: window.launchLibraryItem(modelData) }
                        ZButton { visible: modelData.source !== "zcard"; text: "Capa"; onClicked: window.openCoverEditor(modelData) }
                        ZButton { visible: modelData.source === "manual"; text: "Editar"; onClicked: window.openEditor(modelData) }
                        ZButton { visible: modelData.source === "manual"; text: "Excluir"; onClicked: { removeDialog.pendingId = modelData.id; removeDialog.open() } }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: libraryList.count === 0
                    text: "Nenhum item corresponde à busca e ao filtro atuais."
                    color: "#81928D"
                }
            }
        }
    }

    Component {
        id: platformsPage
        ColumnLayout {
            spacing: 12
            Text { text: "Selecione uma plataforma para abrir sua coleção filtrada."; color: "#9BAEA8"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            GridView {
                id: platformGrid
                Layout.fillWidth: true
                Layout.fillHeight: true
                cellWidth: 250
                cellHeight: 132
                clip: true
                model: window.platformSummaries
                delegate: ZButton {
                    id: platformCard
                    required property var modelData
                    width: 234
                    height: 112
                    onClicked: {
                        window.platformFilter = modelData.platform
                        platformFilterBox.currentIndex = window.platformFilterOptions.indexOf(modelData.platform)
                        window.activePage = "Minha biblioteca"
                    }
                    background: Rectangle { radius: 18; color: platformCard.down ? "#164B3A" : (platformCard.hovered ? "#102B22" : "#091511"); border.color: "#1D6A52"; Behavior on color { ColorAnimation { duration: 160 } } }
                    contentItem: Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8
                        Text { text: modelData.platform; color: "#20E0C0"; font.pixelSize: 15; font.bold: true; elide: Text.ElideRight; width: parent.width }
                        Text { text: modelData.count + (modelData.count === 1 ? " item" : " itens"); color: "#A8B9B5"; font.pixelSize: 12 }
                        Text { text: "Abrir biblioteca  →"; color: "#7F9690"; font.pixelSize: 10 }
                    }
                }
                Text { anchors.centerIn: parent; visible: platformGrid.count === 0; text: "As plataformas aparecerão aqui quando você adicionar ou importar itens."; color: "#81928D"; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; width: parent.width * 0.8 }
            }
        }
    }

    Component {
        id: devicesPage
        ColumnLayout {
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Text { text: gameScanner.externalDevices.length + " unidade(s) externa(s) detectada(s)"; color: "#DCEBE7"; font.pixelSize: 14 }
                Item { Layout.fillWidth: true }
                ZButton { text: "Atualizar dispositivos"; onClicked: gameScanner.refreshExternalDevices() }
            }
            ListView {
                id: devicesList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: gameScanner.externalDevices
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 82
                    radius: 9
                    color: "#0B1110"
                    border.color: "#1C302C"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text { text: modelData.label; color: "#F3F7F6"; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { text: modelData.path + " • " + modelData.filesystem + " • " + (modelData.bytesAvailable / 1073741824).toFixed(1) + " GB livres"; color: "#829A93"; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideMiddle }
                        }
                        ZButton { text: "Procurar jogos"; accent: true; onClicked: gameScanner.scanDirectory(modelData.path) }
                    }
                }
                Text { anchors.centerIn: parent; visible: devicesList.count === 0; text: "Nenhum dispositivo externo detectado. Conecte uma unidade USB e aguarde a atualização automática."; color: "#81928D"; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; width: parent.width * 0.8 }
            }
        }
    }

    Component {
        id: discoveryPage
        ColumnLayout {
            spacing: 16
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 168
                radius: 12
                color: "#0B1715"
                border.color: "#176B5D"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 10
                    Text { text: "DESCOBERTA LOCAL"; color: "#20E0C0"; font.pixelSize: 12; font.bold: true; font.letterSpacing: 1 }
                    Text { text: "Localize jogos que já estão instalados no computador."; color: "#F0F6F4"; font.pixelSize: 20; font.bold: true; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    Text { text: "Esta versão não exibe catálogos online das lojas. Ela pesquisa bibliotecas locais do Steam, Epic Games, Ubisoft Connect e Heroic; os resultados ficam para revisão antes da importação."; color: "#A8B9B5"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                }
            }
            Text { text: "ESCOLHA ONDE PROCURAR"; color: "#F0F6F4"; font.pixelSize: 14; font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                ZButton { Layout.fillWidth: true; text: "Steam"; onClicked: { window.activePage = "Scanner"; gameScanner.scanSteamLibraries() } }
                ZButton { Layout.fillWidth: true; text: "Epic / Ubisoft / Heroic"; onClicked: { window.activePage = "Scanner"; gameScanner.scanExternalLaunchers() } }
                ZButton { Layout.fillWidth: true; text: "Pastas comuns"; onClicked: { window.activePage = "Scanner"; gameScanner.scanCommonLocations() } }
            }
            Item { Layout.fillHeight: true }
        }
    }

    Component {
        id: scannerPage
        ColumnLayout {
            spacing: 12
            Text { text: gameScanner.status; color: "#A8B9B5"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                ZButton { Layout.fillWidth: true; text: "Steam"; onClicked: gameScanner.scanSteamLibraries() }
                ZButton { Layout.fillWidth: true; text: "Epic / Ubisoft / Heroic"; onClicked: gameScanner.scanExternalLaunchers() }
                ZButton { Layout.fillWidth: true; text: "Pastas comuns"; onClicked: gameScanner.scanCommonLocations() }
            }
            ProgressBar { Layout.fillWidth: true; visible: gameScanner.busy; indeterminate: true }
            RowLayout {
                Layout.fillWidth: true
                Text { text: gameScanner.candidates.length + " candidato(s) aguardando revisão"; color: "#DCEBE7"; font.pixelSize: 13 }
                Item { Layout.fillWidth: true }
                ZButton { text: "Revisar resultados"; accent: true; enabled: gameScanner.candidates.length > 0; onClicked: candidateDialog.open() }
            }
            ListView {
                id: scannerCandidatesList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                model: gameScanner.candidates
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 58
                    radius: 8
                    color: "#0B1110"
                    border.color: "#1C302C"
                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4
                        Text { text: modelData.title; color: "#F3F7F6"; font.bold: true; elide: Text.ElideRight; width: parent.width }
                        Text { text: modelData.platform + " • " + modelData.kind + " • " + modelData.path; color: "#829A93"; font.pixelSize: 10; elide: Text.ElideMiddle; width: parent.width }
                    }
                }
                Text { anchors.centerIn: parent; visible: scannerCandidatesList.count === 0; text: "Inicie uma busca para listar candidatos nesta tela."; color: "#81928D" }
            }
        }
    }

    Component {
        id: romsPage
        ColumnLayout {
            spacing: 12
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 112
                radius: 10
                color: "#0B1715"
                border.color: "#176B5D"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 6
                    Text { text: "BUSCA LOCAL DE ROMs E SUPORTE"; color: "#20E0C0"; font.pixelSize: 12; font.bold: true; font.letterSpacing: 1 }
                    Text { text: "Pesquise arquivos que já estão em uma pasta sua."; color: "#F0F6F4"; font.pixelSize: 18; font.bold: true; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    Text { text: "O HUB não baixa ROMs, jogos, BIOS ou firmware. Arquivos de suporte detectados servem apenas como referência e nunca são importados como jogos."; color: "#A8B9B5"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11 }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ZButton { text: "Escolher pasta e buscar"; accent: true; enabled: !gameScanner.busy; onClicked: romFolderPicker.open() }
                ZButton { text: "Configurar emuladores"; onClicked: emulatorDialog.open() }
                Item { Layout.fillWidth: true }
                Text { text: gameScanner.busy ? "Buscando..." : gameScanner.status; color: "#9BAEA8"; wrapMode: Text.Wrap; Layout.maximumWidth: 420; font.pixelSize: 11 }
            }
            ProgressBar { Layout.fillWidth: true; visible: gameScanner.busy; indeterminate: true }
            RowLayout {
                Layout.fillWidth: true
                ZTextField {
                    id: romSearchFilter
                    Layout.fillWidth: true
                    placeholderText: "Filtrar resultados por nome, plataforma ou caminho..."
                    onTextChanged: window.romSearchText = text
                }
                Text { text: gameScanner.candidates.filter(function(item) { return item.source === "romsearch" }).length + " resultado(s)"; color: "#829A93"; font.pixelSize: 11 }
            }
            ListView {
                id: romResults
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                model: gameScanner.candidates.filter(function(item) {
                    if (item.source !== "romsearch") return false
                    var query = window.romSearchText.toLowerCase()
                    return query.length === 0 || String(item.title).toLowerCase().indexOf(query) >= 0
                        || String(item.platform).toLowerCase().indexOf(query) >= 0
                        || String(item.path).toLowerCase().indexOf(query) >= 0
                })
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 78
                    radius: 8
                    color: "#0B1110"
                    border.color: modelData.importable === false ? "#594D35" : "#1C302C"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text { text: modelData.title; color: "#F3F7F6"; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                            Text { text: modelData.platform + " • " + modelData.kind + " • " + (Number(modelData.sizeBytes || 0) / 1048576).toFixed(1) + " MB"; color: modelData.importable === false ? "#D7B77A" : "#829A93"; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
                            Text { text: modelData.path; color: "#6F827C"; font.pixelSize: 9; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        }
                        Text {
                            visible: modelData.importable !== false
                            text: window.hasEmulator(modelData.platform) ? "Emulador configurado" : "Configure o emulador"
                            color: window.hasEmulator(modelData.platform) ? "#20E0C0" : "#D7B77A"
                            font.pixelSize: 10
                        }
                        ZButton {
                            visible: modelData.importable !== false
                            text: "＋ Adicionar"
                            onClicked: {
                                if (gameCatalog.importCandidate(modelData.title, modelData.path, modelData.platform, "romsearch", "", "", "")) {
                                    for (var i = 0; i < gameScanner.candidates.length; ++i) {
                                        if (gameScanner.candidates[i].path === modelData.path) {
                                            gameScanner.removeCandidate(i)
                                            break
                                        }
                                    }
                                }
                            }
                        }
                        Text { visible: modelData.importable === false; text: "Suporte — não importar"; color: "#D7B77A"; font.pixelSize: 10 }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: romResults.count === 0
                    text: gameScanner.candidates.length === 0 ? "Escolha uma pasta para procurar ROMs e arquivos de suporte." : "Nenhum resultado corresponde ao filtro."
                    color: "#81928D"
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    Component {
        id: quiverPage
        ColumnLayout {
            spacing: 10
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 112
                radius: 10
                color: "#0B1715"
                border.color: "#176B5D"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 6
                    Text { text: "CATÁLOGO QUIVER PORTADO PARA O HUB"; color: "#20E0C0"; font.pixelSize: 12; font.bold: true; font.letterSpacing: 1 }
                    Text { text: "Navegue, instale e inicie seus ports no mesmo launcher"; color: "#F0F6F4"; font.pixelSize: 18; font.bold: true; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    Text { text: "O HUB busca as quatro listas comunitárias Quiver e instala ports direto na sua biblioteca, sem abrir o Quiver separado. Jogos originais, ROMs e BIOS não são fornecidos; clientes de lojas podem ser necessários em segundo plano para login/DRM."; color: "#A8B9B5"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11 }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ZButton { text: quiverIntegration.catalogBusy ? "Atualizando catálogo…" : "Atualizar catálogo"; enabled: !quiverIntegration.catalogBusy; onClicked: quiverIntegration.refreshCatalog() }
                Item { Layout.fillWidth: true }
            }
            Text { Layout.fillWidth: true; text: quiverIntegration.catalogStatus + (quiverIntegration.operationStatus.length ? "  •  " + quiverIntegration.operationStatus : ""); color: quiverIntegration.operationBusy || quiverIntegration.catalogBusy ? "#20E0C0" : "#A8B9B5"; wrapMode: Text.Wrap; font.pixelSize: 11 }
            RowLayout {
                Layout.fillWidth: true
                ZTextField { id: quiverSearchField; Layout.fillWidth: true; placeholderText: "Buscar jogo, projeto, console ou tag…"; text: window.quiverSearchText; onTextChanged: window.quiverSearchText = text.trim().toLowerCase() }
                ZComboBox { id: quiverCategoryBox; Layout.preferredWidth: 190; model: ["Todas", "Nintendo", "PlayStation", "Xbox", "Other Platforms"]; currentIndex: 0; onCurrentTextChanged: window.quiverCategory = currentText }
            }
            ListView {
                id: quiverAppsList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 7
                model: quiverIntegration.apps.filter(function(app) {
                    var query = window.quiverSearchText
                    var searchable = (app.name + " " + app.project + " " + app.repository + " " + app.tags.join(" ")).toLowerCase()
                    return (window.quiverCategory === "Todas" || app.catalogName === window.quiverCategory)
                        && (!query.length || searchable.indexOf(query) >= 0)
                })
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 104
                    radius: 9
                    color: "#0B1110"
                    border.color: "#1C302C"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 11
                        Rectangle {
                            Layout.preferredWidth: 62
                            Layout.preferredHeight: 82
                            radius: 6
                            color: "#101918"
                            clip: true
                            Image { anchors.fill: parent; source: modelData.appIconUrl || "qrc:/ZGameHub/assets/game-card-placeholder.png"; fillMode: Image.PreserveAspectFit; asynchronous: true }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text { text: modelData.name + (modelData.project ? " — " + modelData.project : ""); color: "#F0F6F4"; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                            Text { text: modelData.catalogName + " • " + modelData.repositorySource + ":" + modelData.repository; color: "#9AB0AA"; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
                            Text { text: modelData.tags.join(" • "); color: "#829A93"; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
                            Text { visible: modelData.filesToAdd.length > 0; text: "Arquivos auxiliares Quiver: " + modelData.filesToAdd.join(", "); color: "#D7B77A"; font.pixelSize: 9; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                        ZButton { text: quiverIntegration.operationBusy ? "Aguarde…" : "Instalar / abrir"; accent: true; enabled: !quiverIntegration.operationBusy && !!modelData.repository; onClicked: quiverIntegration.openOrPrepareApp(modelData) }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: quiverAppsList.count === 0
                    text: quiverIntegration.catalogBusy ? "Carregando listas comunitárias…" : (quiverIntegration.apps.length === 0 ? "Use Atualizar catálogo para baixar as listas publicadas." : "Nenhum app corresponde à busca e ao filtro atuais.")
                    color: "#81928D"
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    Component {
        id: zcardPage
        ScrollView {
            clip: true
            ColumnLayout {
                width: pageLoader.width
                spacing: 14
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 188
                    radius: 22
                    color: "#0A1E18"
                    border.color: neon
                    border.width: 1
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 22
                        spacing: 8
                        Text { text: "ZCARD CONTROL CENTER"; color: neon; font.pixelSize: 11; font.bold: true; font.letterSpacing: 3 }
                        Text { text: "Monte e empacote seus projetos ZConsole."; color: ink; font.pixelSize: 25; font.bold: true }
                        Text { text: "O HUB carrega diretórios ZGF, cartões ZIP/legacy, containers ZCF0 e pacotes ZUF0. A geração usa o código oficial ZCardBuilder incorporado ao próprio executável; nenhuma emulação é executada nesta tela."; color: "#A5C5BE"; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        RowLayout {
                            Layout.fillWidth: true
                            ZButton { text: "Abrir arquivo ZCARD"; accent: true; onClicked: zcardOpenFileDialog.open() }
                            ZButton { text: "Montar diretório ZGF"; onClicked: zcardOpenFolderDialog.open() }
                            Item { Layout.fillWidth: true }
                            Text { text: zcardService.busy ? "GERANDO…" : "BUILDER INTEGRADO"; color: zcardService.busy ? "#F2CB73" : neon; font.pixelSize: 10; font.bold: true }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 92
                    radius: 18
                    color: "#07130F"
                    border.color: "#1D6A52"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 14
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: zcardService.currentPath.length ? zcardService.currentPath : "Nenhum ZCARD montado"; color: ink; font.bold: true; elide: Text.ElideMiddle; Layout.fillWidth: true }
                            Text { text: zcardService.currentFormat.length ? zcardService.currentFormat : "Escolha um arquivo ou diretório para inspecionar"; color: muted; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                        Text { text: zcardService.entries.length + " entrada(s)"; color: neon; font.bold: true }
                        ZButton { visible: zcardService.currentPath.length > 0; text: "Executar conteúdo"; accent: true; onClicked: window.launchMountedZCard() }
                        ZButton { visible: zcardService.currentPath.length > 0; text: "Desmontar"; onClicked: zcardService.clearCard() }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 340
                    spacing: 14
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 18
                        color: "#07130F"
                        border.color: "#12382F"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 16; spacing: 9
                            Text { text: "CONTEÚDO DO CARTÃO"; color: ink; font.pixelSize: 13; font.bold: true; font.letterSpacing: 1 }
                            ListView {
                                id: zcardEntriesList
                                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 5
                                model: zcardService.entries
                                delegate: Rectangle {
                                    required property var modelData
                                    width: ListView.view.width; height: 42; radius: 10
                                    color: zcardEntryMouse.containsMouse ? "#102B22" : "#091A15"
                                    border.color: zcardEntryMouse.containsMouse ? neon : "#173A30"
                                    Behavior on color { ColorAnimation { duration: 150 } }
                                    RowLayout {
                                        anchors.fill: parent; anchors.margins: 9; spacing: 10
                                        Text { text: modelData.kind === "manifest" ? "▧" : (modelData.kind === "region" ? "◈" : "•"); color: neon; font.pixelSize: 14 }
                                        Text { text: modelData.name; color: "#E6F7F0"; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                        Text { text: (modelData.size || 0) + " bytes"; color: muted; font.pixelSize: 10 }
                                    }
                                    MouseArea { id: zcardEntryMouse; anchors.fill: parent; hoverEnabled: true }
                                }
                                Text { anchors.centerIn: parent; visible: zcardEntriesList.count === 0; text: "Nenhum conteúdo montado."; color: muted }
                            }
                        }
                    }
                    Rectangle {
                        Layout.preferredWidth: 390; Layout.fillHeight: true; radius: 18; color: "#07130F"; border.color: "#12382F"
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 16; spacing: 9
                            Text { text: "GERAR ZCARD"; color: ink; font.pixelSize: 13; font.bold: true; font.letterSpacing: 1 }
                            Text { text: "Cada opção chama o builder oficial incorporado no programa."; color: muted; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            ZComboBox { id: zcardFormatBox; Layout.fillWidth: true; model: ["legacy", "zcf", "zuf"] }
                            ZTextField { id: zcardSourcePath; Layout.fillWidth: true; placeholderText: "Pasta fonte do projeto (META/CODE/assets)" }
                            RowLayout {
                                Layout.fillWidth: true
                                ZButton { text: "Escolher fonte"; onClicked: zcardBuildSourceDialog.open() }
                                Item { Layout.fillWidth: true }
                            }
                            ZTextField { id: zcardOutputPath; Layout.fillWidth: true; placeholderText: "Arquivo de saída .zcard / .zcf / .zuf" }
                            RowLayout {
                                Layout.fillWidth: true
                                ZButton { text: "Escolher saída"; onClicked: zcardBuildOutputDialog.open() }
                                Item { Layout.fillWidth: true }
                            }
                            ZButton { Layout.fillWidth: true; accent: true; enabled: !zcardService.busy; text: zcardService.busy ? "Gerando…" : "Gerar e validar cartão"; onClicked: zcardService.buildCard(zcardSourcePath.text, zcardOutputPath.text, zcardFormatBox.currentText) }
                            Text { text: zcardService.status; color: zcardService.status.indexOf("erro") >= 0 || zcardService.status.indexOf("inválido") >= 0 ? "#F09C88" : "#91E9C5"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11 }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 126; radius: 18; color: "#050D0A"; border.color: "#12382F"
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 5
                        Text { text: "RELATÓRIO DO BUILDER"; color: neon; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2 }
                        ZTextArea {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            readOnly: true
                            text: zcardService.report
                            color: "#9CD4BD"
                            wrapMode: Text.Wrap
                            background: Rectangle { color: "transparent" }
                            font.family: "Consolas"
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }
    }

    Component {
        id: settingsPage
        ColumnLayout {
            spacing: 14
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 112
                radius: 10
                color: "#0B1110"
                border.color: "#1C302C"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 6
                    Text { text: "Z-GAME HUB para Windows"; color: "#F3F7F6"; font.pixelSize: 16; font.bold: true }
                    Text { text: "Versão 1.3.0 • Catálogo local: " + window.libraryItems().length + " jogo(s), aplicativo(s) e ZCARD(s)"; color: "#A8B9B5"; font.pixelSize: 12 }
                    Text { text: gameCatalog.emulatorConfigs.length + " configuração(ões) de emulador salva(s)"; color: "#829A93"; font.pixelSize: 11 }
                }
            }
            Text { text: "Ajustes disponíveis"; color: "#F0F6F4"; font.pixelSize: 15; font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                ZButton { text: "Configurar emuladores"; accent: true; onClicked: emulatorDialog.open() }
                ZButton { text: "Adicionar jogo ou aplicativo"; onClicked: { window.editingId = -1; gameCover.text = ""; addDialog.open() } }
                ZButton { text: "Atualizar catálogo"; onClicked: gameCatalog.refresh() }
                Item { Layout.fillWidth: true }
            }
            Text { text: "As capas são copiadas para os dados locais do HUB. Jogos Steam tentam importar a cápsula oficial; qualquer item da biblioteca pode receber uma capa própria."; color: "#829A93"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            Item { Layout.fillHeight: true }
        }
    }

    Dialog {
        id: addDialog
        title: editingId >= 0 ? "Editar jogo ou aplicativo" : "Adicionar jogo ou aplicativo"
        modal: true
        anchors.centerIn: parent
        width: Math.min(520, window.width - 40)
        standardButtons: Dialog.Save | Dialog.Cancel
        onAccepted: {
            var saved = editingId >= 0
                ? gameCatalog.updateManualGame(editingId, gameTitle.text, gamePath.text, gamePlatform.text, gameCommand.text, gameCover.text)
                : gameCatalog.addManualGame(gameTitle.text, gamePath.text, gamePlatform.text, gameCommand.text, gameCover.text)
            if (saved) {
                editingId = -1
                gameTitle.text = ""; gamePath.text = ""; gamePlatform.text = "PC"; gameCommand.text = ""; gameCover.text = ""
            }
        }
        onRejected: {
            editingId = -1
            gameTitle.text = ""; gamePath.text = ""; gamePlatform.text = "PC"; gameCommand.text = ""; gameCover.text = ""
        }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: "Cadastre manualmente um jogo, programa ou atalho que o scanner não encontrou. O registro será preservado nas próximas buscas."; color: "#9BAEA8"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            ZTextField { id: gameTitle; Layout.fillWidth: true; placeholderText: "Nome do jogo ou aplicativo *" }
            RowLayout {
                Layout.fillWidth: true
                ZTextField { id: gamePath; Layout.fillWidth: true; placeholderText: "Caminho de .exe, atalho .lnk ou ROM" }
                ZButton { text: "Procurar"; onClicked: manualFilePicker.open() }
            }
            ZTextField { id: gamePlatform; Layout.fillWidth: true; text: "PC"; placeholderText: "Plataforma (PC, SNES, PS1...)" }
            ZTextField { id: gameCommand; Layout.fillWidth: true; placeholderText: "Comando alternativo com argumentos (opcional)" }
            RowLayout {
                Layout.fillWidth: true
                Rectangle {
                    Layout.preferredWidth: 70; Layout.preferredHeight: 90; radius: 6; clip: true; color: "#101918"
                    Image { anchors.fill: parent; source: gameCover.text.trim().length ? gameCatalog.localFileUrl(gameCover.text) : "qrc:/ZGameHub/assets/game-card-placeholder.png"; fillMode: Image.PreserveAspectCrop; asynchronous: true }
                    Text { anchors.centerIn: parent; visible: gameCover.text.trim().length === 0; text: "SEM\nCAPA"; color: "#18DCC0"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    ZTextField { id: gameCover; Layout.fillWidth: true; placeholderText: "Imagem de capa (opcional)" }
                    RowLayout {
                        ZButton { text: "Escolher imagem"; onClicked: manualCoverPicker.open() }
                        ZButton { text: "Remover"; onClicked: gameCover.text = "" }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ZButton { text: "＋ Adicionar arquivos..."; onClicked: filePicker.open() }
                ZButton { text: "＋ Adicionar pasta..."; onClicked: folderPicker.open() }
                Item { Layout.fillWidth: true }
            }
            Text { text: "Executáveis .exe e atalhos .lnk podem ser iniciados diretamente. ROMs precisam de um emulador configurado."; color: "#9BAEA8"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11 }
        }
        background: Rectangle { color: "#081713"; radius: 20; border.width: 1; border.color: "#1D6A52" }
    }

    Dialog {
        id: coverDialog
        title: "Editar capa do jogo"
        modal: true
        anchors.centerIn: parent
        width: Math.min(440, window.width - 40)
        standardButtons: Dialog.Save | Dialog.Cancel
        onAccepted: {
            if (coverEditingId >= 0) gameCatalog.setGameCover(coverEditingId, coverEditPath.text)
            coverEditingId = -1
            coverEditPath.text = ""
        }
        onRejected: { coverEditingId = -1; coverEditPath.text = "" }
        contentItem: ColumnLayout {
            spacing: 10
            Text { text: "A imagem será copiada para a pasta de dados do Z-GAME HUB para continuar disponível mesmo se o arquivo original for movido."; color: "#9BAEA8"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                Rectangle {
                    Layout.preferredWidth: 90; Layout.preferredHeight: 120; radius: 6; clip: true; color: "#101918"
                    Image { anchors.fill: parent; source: coverEditPath.text.trim().length ? gameCatalog.localFileUrl(coverEditPath.text) : "qrc:/ZGameHub/assets/game-card-placeholder.png"; fillMode: Image.PreserveAspectCrop; asynchronous: true }
                    Text { anchors.centerIn: parent; visible: coverEditPath.text.trim().length === 0; text: "SEM\nCAPA"; color: "#18DCC0"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    ZTextField { id: coverEditPath; Layout.fillWidth: true; placeholderText: "Caminho da imagem" }
                    RowLayout {
                        ZButton { text: "Escolher imagem"; onClicked: coverEditPicker.open() }
                        ZButton { text: "Remover capa"; onClicked: coverEditPath.text = "" }
                    }
                }
            }
        }
        background: Rectangle { color: "#081713"; radius: 20; border.width: 1; border.color: "#1D6A52" }
    }

    Dialog {
        id: removeDialog
        property int pendingId: -1
        title: "Excluir registro manual"
        modal: true
        anchors.centerIn: parent
        width: Math.min(420, window.width - 40)
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            if (pendingId >= 0) gameCatalog.removeGame(pendingId)
            pendingId = -1
        }
        contentItem: Text {
            text: "Remover este registro de jogo ou aplicativo da biblioteca? O arquivo original não será apagado."
            color: "#EAF4F1"
            wrapMode: Text.Wrap
            width: 360
        }
        background: Rectangle { color: "#081713"; radius: 20; border.width: 1; border.color: "#1D6A52" }
    }

    Dialog {
        id: emulatorDialog
        title: "Emuladores por plataforma"
        modal: true
        anchors.centerIn: parent
        width: Math.min(620, window.width - 40)
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 10
            Text {
                text: "Configure o RetroArch ou outro emulador. Use {ROM} nos argumentos para indicar a posição da ROM; se não usar o marcador, a ROM será anexada ao final."
                color: "#A8B9B5"; wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Plataforma"; color: "#D9E8E4" }
                ZComboBox {
                    id: emulatorPlatform
                    Layout.fillWidth: true
                    model: ["NES", "SNES", "Game Boy", "Game Boy Advance", "Nintendo DS", "Nintendo 64", "Mega Drive", "Master System", "Game Gear", "PlayStation", "PlayStation 2", "PSP", "Dreamcast", "Wii", "Wii U", "Nintendo 3DS", "Nintendo Switch", "Sega CD", "Disco/Imagem", "ROM compactada", "PC / Retro"]
                    onCurrentTextChanged: {
                        var config = gameCatalog.emulatorConfig(currentText)
                        retroarchExecutable.text = config.executable || ""
                        emulatorArguments.text = config.arguments || ""
                        emulatorCore.text = config.core || ""
                        emulatorPreset.currentIndex = comboIndex(emulatorPreset.model, config.preset || "Personalizado")
                        aspectRatio.currentIndex = comboIndex(aspectRatio.model, config.aspectRatio || "auto")
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ZTextField { id: retroarchExecutable; Layout.fillWidth: true; placeholderText: "Executável (ex.: C:/Program Files/RetroArch/retroarch.exe)" }
                ZButton { text: "Escolher"; onClicked: emulatorExecutablePicker.open() }
                ZButton { text: "Detectar RetroArch"; onClicked: retroarchExecutable.text = gameCatalog.detectRetroArch() }
            }
            ZTextField { id: emulatorArguments; Layout.fillWidth: true; placeholderText: "Argumentos (ex.: --fullscreen {ROM})" }
            RowLayout {
                Layout.fillWidth: true
                ZComboBox {
                    id: emulatorPreset
                    Layout.fillWidth: true
                    model: ["Personalizado", "Tela cheia", "Janela", "Tela cheia 4:3", "Tela cheia 16:9"]
                }
                ZButton { text: "Aplicar preset"; onClicked: applyEmulatorPreset() }
                ZComboBox {
                    id: aspectRatio
                    Layout.preferredWidth: 130
                    model: ["auto", "4:3", "16:9", "16:10", "21:9"]
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ZTextField { id: emulatorCore; Layout.fillWidth: true; placeholderText: "Core opcional (ex.: snes9x_libretro.so)" }
                ZButton {
                    text: "Sugerir core"
                    onClicked: emulatorCore.text = gameCatalog.suggestRetroArchCore(emulatorPlatform.currentText)
                }
            }
            Text {
                text: gameCatalog.detectRetroArchCores().length + " core(s) RetroArch encontrado(s) nas pastas padrão do Windows"
                color: "#829A93"; font.pixelSize: 10; Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                ZButton {
                    text: "Salvar configuração"
                    onClicked: {
                        if (gameCatalog.saveEmulatorConfig(emulatorPlatform.currentText, retroarchExecutable.text, emulatorArguments.text, emulatorCore.text, emulatorPreset.currentText, aspectRatio.currentText, true))
                            emulatorDialog.close()
                    }
                }
                ZButton {
                    text: "Remover plataforma"
                    onClicked: {
                        gameCatalog.removeEmulatorConfig(emulatorPlatform.currentText)
                        retroarchExecutable.text = ""; emulatorArguments.text = ""; emulatorCore.text = ""
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }
        background: Rectangle { color: "#081713"; radius: 20; border.width: 1; border.color: "#1D6A52" }
    }

    FileDialog {
        id: emulatorExecutablePicker
        title: "Escolher executável do emulador"
        fileMode: FileDialog.OpenFile
        onAccepted: retroarchExecutable.text = gameCatalog.localPathFromUrl(selectedFile.toString())
    }

    FileDialog {
        id: manualCoverPicker
        title: "Escolher imagem de capa"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Imagens (*.png *.jpg *.jpeg *.webp *.bmp *.gif)", "Todos os arquivos (*)"]
        onAccepted: gameCover.text = gameCatalog.localPathFromUrl(selectedFile.toString())
    }

    FileDialog {
        id: coverEditPicker
        title: "Escolher imagem de capa"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Imagens (*.png *.jpg *.jpeg *.webp *.bmp *.gif)", "Todos os arquivos (*)"]
        onAccepted: coverEditPath.text = gameCatalog.localPathFromUrl(selectedFile.toString())
    }

    FileDialog {
        id: manualFilePicker
        title: "Escolher aplicativo, atalho ou ROM"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Aplicativos, atalhos e ROMs (*.exe *.lnk *.rom *.nes *.sfc *.smc *.gb *.gbc *.gba *.nds *.iso *.chd *.cue *.cdi *.gdi *.z64 *.n64 *.v64 *.md *.gen *.3ds *.cia *.pbp *.cso *.ecm *.wbfs *.wud *.wux *.nsp *.xci *.sms *.gg *.zip *.7z *.rar)", "Todos os arquivos (*)"]
        onAccepted: {
            var path = gameCatalog.localPathFromUrl(selectedFile.toString())
            gamePath.text = path
            var separator = Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\"))
            if (gameTitle.text.trim().length === 0)
                gameTitle.text = path.substring(separator + 1).replace(/\.[^.]+$/, "")
        }
    }

    FileDialog {
        id: filePicker
        title: "Adicionar aplicativos ou jogos"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Aplicativos, atalhos e ROMs (*.exe *.lnk *.rom *.nes *.sfc *.smc *.gb *.gbc *.gba *.nds *.iso *.chd *.cue *.cdi *.gdi *.z64 *.n64 *.v64 *.md *.gen *.3ds *.cia *.pbp *.cso *.ecm *.wbfs *.wud *.wux *.nsp *.xci *.sms *.gg *.zip *.7z *.rar)", "Todos os arquivos (*)"]
        onAccepted: addSelectedFiles(selectedFiles)
    }

    FolderDialog {
        id: folderPicker
        title: "Adicionar uma pasta de jogos"
        onAccepted: {
            var path = gameCatalog.localPathFromUrl(selectedFolder.toString())
            if (gameCatalog.addPath(path, "PC / Pasta")) gameCatalog.refresh()
        }
    }

    FolderDialog {
        id: romFolderPicker
        title: "Escolher pasta com ROMs e arquivos de suporte"
        onAccepted: gameScanner.scanRomsDirectory(gameCatalog.localPathFromUrl(selectedFolder.toString()))
    }

    FileDialog {
        id: zcardOpenFileDialog
        title: "Abrir ZCARD / ZCF / ZUF"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Cartões ZConsole (*.zcard *.zcf *.zuf *.zip)", "Todos os arquivos (*)"]
        onAccepted: {
            var path = gameCatalog.localPathFromUrl(selectedFile.toString())
            zcardService.registerCard(path)
            zcardService.openCard(path)
        }
    }

    FolderDialog {
        id: zcardOpenFolderDialog
        title: "Montar diretório ZGF / projeto ZConsole"
        onAccepted: {
            var path = gameCatalog.localPathFromUrl(selectedFolder.toString())
            zcardService.registerCard(path)
            zcardService.openCard(path)
        }
    }

    FolderDialog {
        id: zcardBuildSourceDialog
        title: "Escolher pasta fonte do projeto ZCARD"
        onAccepted: zcardSourcePath.text = gameCatalog.localPathFromUrl(selectedFolder.toString())
    }

    FileDialog {
        id: zcardBuildOutputDialog
        title: "Escolher arquivo de saída do cartão"
        fileMode: FileDialog.SaveFile
        nameFilters: ["ZCARD legacy (*.zcard)", "Container ZCF (*.zcf)", "Pacote ZUF (*.zuf)", "Todos os arquivos (*)"]
        onAccepted: zcardOutputPath.text = gameCatalog.localPathFromUrl(selectedFile.toString())
    }

    Dialog {
        id: quiverReleaseDialog
        title: "Pacotes publicados pelo projeto"
        modal: true
        anchors.centerIn: parent
        width: Math.min(720, window.width - 40)
        height: Math.min(560, window.height - 40)
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 10
            Text {
                text: "Escolha um ZIP da release. O HUB baixa e extrai o pacote no diretório local QuiverApps. Confirme que o pacote e os requisitos pertencem ao projeto escolhido."
                color: "#A8B9B5"; wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
                model: quiverIntegration.releaseAssets
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width; height: 66; radius: 7
                    color: "#101918"; border.color: "#1D3832"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 9; spacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 3
                            Text { text: modelData.name; color: "#F0F6F4"; font.bold: true; elide: Text.ElideMiddle; Layout.fillWidth: true }
                            Text { text: modelData.version + (modelData.size > 0 ? " • " + Math.max(1, Math.round(modelData.size / 1048576)) + " MiB" : ""); color: "#829A93"; font.pixelSize: 10 }
                        }
                        ZButton {
                            text: "Baixar e instalar"
                            accent: true
                            onClicked: {
                                window.pendingQuiverAsset = modelData
                                quiverReleaseDialog.close()
                                quiverInstallConfirmDialog.open()
                            }
                        }
                    }
                }
            }
        }
        background: Rectangle { color: "#081713"; radius: 20; border.width: 1; border.color: "#1D6A52" }
    }

    Dialog {
        id: quiverInstallConfirmDialog
        title: "Confirmar download do projeto"
        modal: true
        anchors.centerIn: parent
        width: Math.min(520, window.width - 40)
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: quiverIntegration.installSelectedRelease(window.pendingQuiverAsset)
        contentItem: Text {
            text: "O HUB vai baixar “" + window.pendingQuiverAsset.name + "” e extrair a release de “" + quiverIntegration.pendingApp.name + "” na sua pasta local. Ports/recompilações podem exigir dados do jogo original, que não são incluídos. Deseja continuar?"
            color: "#EAF4F1"; wrapMode: Text.Wrap; width: 450
        }
        background: Rectangle { color: "#081713"; radius: 20; border.width: 1; border.color: "#1D6A52" }
    }

    Dialog {
        id: quiverExecutableDialog
        title: "Escolher executável principal"
        modal: true
        anchors.centerIn: parent
        width: Math.min(620, window.width - 40)
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 10
            Text { text: "Selecione o executável do app instalado. Se o projeto trouxer ferramentas auxiliares, escolha o jogo principal."; color: "#A8B9B5"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            ZComboBox { id: quiverExecutableBox; Layout.fillWidth: true; model: window.pendingQuiverExecutables; currentIndex: 0 }
            RowLayout {
                Layout.fillWidth: true
                ZButton {
                    text: "Adicionar à biblioteca"
                    onClicked: {
                        if (quiverExecutableBox.currentText.length && gameCatalog.addManualGame(window.pendingQuiverTitle, quiverExecutableBox.currentText, "PC / Quiver", "", ""))
                            quiverExecutableDialog.close()
                    }
                }
                ZButton {
                    text: "Abrir agora"
                    onClicked: {
                        if (quiverIntegration.launchInstalledApp(quiverExecutableBox.currentText)) quiverExecutableDialog.close()
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }
        background: Rectangle { color: "#081713"; radius: 20; border.width: 1; border.color: "#1D6A52" }
    }

    Connections {
        target: gameScanner
        function onScanFinished(filesFound) { if (!gameScanner.romSearch) candidateDialog.open() }
        function onDeviceConnected(path, label) {
            window.pendingUsbPath = path
            usbDialog.deviceLabel = label
            usbDialog.open()
        }
    }

    Dialog {
        id: usbDialog
        property string deviceLabel: "dispositivo externo"
        title: "Dispositivo externo detectado"
        modal: true
        anchors.centerIn: parent
        width: Math.min(460, window.width - 40)
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            if (window.pendingUsbPath.length > 0) gameScanner.scanDirectory(window.pendingUsbPath)
            window.pendingUsbPath = ""
        }
        contentItem: Text {
            text: "O dispositivo ‘" + usbDialog.deviceLabel + "’ está disponível. Deseja pesquisar jogos nele agora?"
            color: "#EAF4F1"
            wrapMode: Text.Wrap
            width: 380
        }
        background: Rectangle { color: "#081713"; radius: 20; border.width: 1; border.color: "#1D6A52" }
    }

    Dialog {
        id: candidateDialog
        title: "Resultados do scanner — revisar antes de importar"
        modal: true
        anchors.centerIn: parent
        width: Math.min(900, window.width - 40)
        height: Math.min(650, window.height - 40)
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 10
            Text {
                text: gameScanner.candidates.length + " arquivos candidatos encontrados. Importar apenas os jogos que você reconhece. ROMs e executáveis não serão iniciados automaticamente."
                color: "#A8B9B5"; wrapMode: Text.Wrap; Layout.fillWidth: true
            }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
                model: gameScanner.candidates
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: ListView.view.width; height: 66; radius: 7
                    color: "#101918"; border.color: "#1D3832"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 12
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 3
                            Text { text: modelData.title; color: "#F0F6F4"; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                            Text { text: modelData.platform + " • " + modelData.kind + " • " + modelData.path; color: "#829A93"; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
                        }
                        ZButton {
                            text: modelData.importable === false ? "Executável indisponível" : (modelData.source === "Steam" ? "＋ Importar Steam" : "＋ Importar")
                            enabled: modelData.importable !== false
                            onClicked: {
                                if (gameCatalog.importCandidate(modelData.title, modelData.path, modelData.platform,
                                    modelData.source || "scanner", modelData.launchCommand || "",
                                    modelData.coverPath || "", modelData.appId || ""))
                                    gameScanner.removeCandidate(index)
                            }
                        }
                        ZButton { text: "Ignorar"; onClicked: gameScanner.removeCandidate(index) }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: gameScanner.candidates.length === 0
                    text: "Nenhum candidato pendente. Faça uma nova busca para verificar outras pastas."
                    color: "#81928D"
                }
            }
        }
        background: Rectangle { color: "#081713"; radius: 20; border.width: 1; border.color: "#1D6A52" }
    }

    Connections {
        target: gameCatalog
        function onErrorOccurred(message) {
            errorDialog.text = message
            errorDialog.open()
        }
        function onGamesChanged() { window.refreshPlatformSummaries() }
    }
    Connections {
        target: quiverIntegration
        function onErrorOccurred(message) {
            errorDialog.text = message
            errorDialog.open()
        }
        function onReleaseAssetsChanged() {
            if (quiverIntegration.releaseAssets.length > 0) quiverReleaseDialog.open()
        }
        function onAppInstalled(title, executables, iconUrl) {
            if (executables.length === 1) {
                gameCatalog.addManualGame(title, executables[0], "PC / Quiver", "", "")
            } else if (executables.length > 1) {
                window.pendingQuiverTitle = title
                window.pendingQuiverExecutables = executables
                quiverExecutableBox.currentIndex = 0
                quiverExecutableDialog.open()
            }
        }
    }
    Dialog {
        id: errorDialog
        property string text: ""
        title: "Z-GAME HUB"
        standardButtons: Dialog.Ok
        modal: true
        anchors.centerIn: parent
        contentItem: Text { text: errorDialog.text; color: "#EAF4F1"; wrapMode: Text.Wrap; width: 360 }
        background: Rectangle { color: "#081713"; radius: 20; border.width: 1; border.color: "#1D6A52" }
    }
}
