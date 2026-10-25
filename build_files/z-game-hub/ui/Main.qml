import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import ZGameHub

ApplicationWindow {
    id: window
    width: 1440
    height: 900
    visible: true
    title: "Z-GAME HUB"
    color: "#050707"
    Component.onCompleted: window.showFullScreen()

    property string activePage: "Início"
    property string searchText: ""
    property string platformFilter: "Todas"
    property int editingId: -1
    property string pendingUsbPath: ""

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
        addDialog.open()
    }

    function comboIndex(items, value) {
        for (var i = 0; i < items.length; ++i)
            if (items[i] === value) return i
        return 0
    }

    function applyEmulatorPreset() {
        if (emulatorPreset.currentText === "Tela cheia") emulatorArguments.text = "--fullscreen {ROM}"
        else if (emulatorPreset.currentText === "Janela") emulatorArguments.text = "--windowed {ROM}"
        else if (emulatorPreset.currentText === "Tela cheia 4:3") emulatorArguments.text = "--fullscreen --aspect-ratio 4:3 {ROM}"
        else if (emulatorPreset.currentText === "Tela cheia 16:9") emulatorArguments.text = "--fullscreen --aspect-ratio 16:9 {ROM}"
    }

    header: Rectangle {
        height: 76
        color: "#090D0D"
        border.color: "#15332F"
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 30
            anchors.rightMargin: 30
            spacing: 22
            Rectangle {
                width: 42; height: 42; radius: 8
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#16E0B8" }
                    GradientStop { position: 1.0; color: "#08AFC5" }
                }
                Text { anchors.centerIn: parent; text: "Z"; color: "#03100E"; font.pixelSize: 30; font.bold: true }
            }
            Column {
                spacing: 1
                Text { text: "Z-GAME HUB"; color: "#F4F8F7"; font.pixelSize: 20; font.bold: true; font.letterSpacing: 2 }
                Text { text: "DESENVOLVIDO POR ZYSTEM"; color: "#12D9BA"; font.pixelSize: 9; font.letterSpacing: 2 }
            }
            Item { Layout.fillWidth: true }
            TextField {
                id: search
                Layout.preferredWidth: 300
                placeholderText: "Buscar na biblioteca..."
                color: "#F4F8F7"
                placeholderTextColor: "#71817E"
                background: Rectangle { radius: 8; color: "#111918"; border.color: search.activeFocus ? "#12D9BA" : "#243532" }
                onTextChanged: window.searchText = text.toLowerCase()
            }
                ZButton {
                    text: "＋  Adicionar jogo"
                    accent: true
                    onClicked: addDialog.open()
                }
                ZButton { text: "Emuladores"; onClicked: emulatorDialog.open() }
                ZButton {
                    text: "⌂  Voltar ao desktop"
                    onClicked: returnDesktopDialog.open()
                    ToolTip.visible: hovered
                    ToolTip.text: "Fechar o Hub e voltar ao desktop"
                }
            }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 22

        Rectangle {
            Layout.preferredWidth: 230
            Layout.fillHeight: true
            radius: 14
            color: "#090D0D"
            border.color: "#142522"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8
                Text { text: "MENU"; color: "#627873"; font.pixelSize: 11; font.bold: true; leftPadding: 8; bottomPadding: 8 }
                Repeater {
                    model: [
                        {label:"Início", icon:"⌂"},
                        {label:"Minha biblioteca", icon:"▦"},
                        {label:"Plataformas", icon:"◈"},
                        {label:"Dispositivos", icon:"▤"},
                        {label:"Loja e descoberta", icon:"◇"},
                        {label:"Scanner", icon:"⌕"},
                        {label:"Configurações", icon:"⚙"}
                    ]
                    delegate: Button {
                        Layout.fillWidth: true
                        text: modelData.icon + "    " + modelData.label
                        onClicked: window.activePage = modelData.label
                        background: Rectangle { radius: 8; color: window.activePage === modelData.label ? "#10312C" : "transparent"; border.color: window.activePage === modelData.label ? "#176B5D" : "transparent" }
                        contentItem: Text { text: parent.text; color: window.activePage === modelData.label ? "#20E0C0" : "#B8C7C3"; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 14 }
                    }
                }
                Item { Layout.fillHeight: true }
                Rectangle {
                    Layout.fillWidth: true
                    radius: 10
                    color: "#0D1715"
                    border.color: "#17443B"
                    implicitHeight: 112
                    Column {
                        anchors.fill: parent; anchors.margins: 12; spacing: 6
                        Text { text: "ZYSTEM SCANNER"; color: "#14DCC0"; font.bold: true; font.pixelSize: 11 }
                        Text { text: gameScanner.status; color: "#A8B9B5"; wrapMode: Text.Wrap; width: parent.width; font.pixelSize: 11 }
                        Text { text: gameScanner.externalDevices.length + " dispositivo(s) externo(s)"; color: "#6FE8D1"; font.pixelSize: 10 }
                        Button { text: gameScanner.busy ? "Buscando..." : "Procurar jogos"; enabled: !gameScanner.busy; onClicked: gameScanner.scanCommonLocations() }
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
                    Text { text: window.activePage.toUpperCase(); color: "#F4F8F7"; font.pixelSize: 28; font.bold: true; font.letterSpacing: 1 }
                    Text { text: "Sua coleção. Seu universo. Do seu jeito."; color: "#7F9690"; font.pixelSize: 13 }
                }
                Item { Layout.fillWidth: true }
                ComboBox {
                    id: platformFilterBox
                    Layout.preferredWidth: 150
                    model: ["Todas", "PC", "PC / Steam", "PC / Steam (Linux)", "PC / Steam (Proton)", "PC / Lutris", "PC / Heroic", "PC / Bottles", "NES", "SNES", "Game Boy", "Game Boy Advance", "Nintendo DS", "Mega Drive", "Nintendo 64", "Disco/Imagem"]
                    onCurrentTextChanged: window.platformFilter = currentText
                }
                ZButton { text: "Steam"; onClicked: gameScanner.scanSteamLibraries() }
                ZButton { text: "Launchers"; onClicked: gameScanner.scanExternalLaunchers() }
                ZButton { text: "⟳ Atualizar"; onClicked: { gameCatalog.refresh(); gameScanner.scanCommonLocations() } }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
                radius: 14
                clip: true
                Image {
                    anchors.fill: parent
                    source: "qrc:/qt/qml/ZGameHub/assets/z-hub-hero.png"
                    fillMode: Image.PreserveAspectCrop
                    opacity: 0.72
                    asynchronous: true
                }
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#103C35" }
                    GradientStop { position: 0.6; color: "#0A2423" }
                    GradientStop { position: 1.0; color: "#101719" }
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 28
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "BEM-VINDO À Z CONSOLE"; color: "#22E3C1"; font.pixelSize: 11; font.bold: true; font.letterSpacing: 2 }
                        Text { text: "Todos os seus jogos.\nEm um só lugar."; color: "#F4FAF8"; font.pixelSize: 30; font.bold: true }
                        Text { text: "Steam • PC • Windows • Retro • Emuladores"; color: "#A5C5BE"; font.pixelSize: 13 }
                    }
                    Rectangle {
                        Layout.preferredWidth: 140; Layout.preferredHeight: 140; radius: 20
                        color: "#112E2B"; border.color: "#1C8C78"
                        Text { anchors.centerIn: parent; text: "Z"; color: "#19DCC0"; font.pixelSize: 100; font.bold: true }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "SUA BIBLIOTECA"; color: "#F0F6F4"; font.pixelSize: 17; font.bold: true; font.letterSpacing: 1 }
                Item { Layout.fillWidth: true }
                Text { text: gameCatalog.games.length + " jogos adicionados"; color: "#7F9690"; font.pixelSize: 12 }
            }

            ListView {
                id: gameCarousel
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
                        if (currentItem) gameCatalog.launchGame(currentItem.modelData.id)
                        event.accepted = true
                    }
                }
                model: gameCatalog.games.filter(function(g) {
                    return g.title.toLowerCase().indexOf(window.searchText) >= 0
                        && (window.platformFilter === "Todas" || g.platform === window.platformFilter)
                })
                delegate: Rectangle {
                    required property var modelData
                    width: 210; height: 292; radius: 12
                    color: "#0B1110"; border.color: "#1C302C"
                    Column {
                        anchors.fill: parent; anchors.margins: 12; spacing: 10
                        Rectangle {
                            width: parent.width; height: 188; radius: 8
                            clip: true
                            Image {
                                anchors.fill: parent
                                source: "qrc:/qt/qml/ZGameHub/assets/game-card-placeholder.png"
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                opacity: 0.84
                            }
                            Rectangle { anchors.fill: parent; color: "#071110"; opacity: 0.24 }
                            Text { anchors.centerIn: parent; text: "Z"; color: "#18DCC0"; font.pixelSize: 70; font.bold: true; opacity: 0.65 }
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
                                onClicked: gameCatalog.launchGame(modelData.id)
                            }
                            Button {
                                visible: modelData.source === "manual"
                                text: "✎"
                                onClicked: openEditor(modelData)
                                ToolTip.visible: hovered
                                ToolTip.text: "Editar"
                            }
                            Button {
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
                    text: "Sua biblioteca está esperando pelos jogos.\nUse “Adicionar jogo” ou “Procurar jogos”."
                    color: "#81928D"; horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5
                }
            }
        }
    }

    Dialog {
        id: addDialog
        title: editingId >= 0 ? "Editar jogo" : "Adicionar jogo à biblioteca"
        modal: true
        anchors.centerIn: parent
        width: Math.min(520, window.width - 40)
        standardButtons: Dialog.Save | Dialog.Cancel
        onAccepted: {
            var saved = editingId >= 0
                ? gameCatalog.updateManualGame(editingId, gameTitle.text, gamePath.text, gamePlatform.text, gameCommand.text)
                : gameCatalog.addManualGame(gameTitle.text, gamePath.text, gamePlatform.text, gameCommand.text)
            if (saved) {
                editingId = -1
                gameTitle.text = ""; gamePath.text = ""; gamePlatform.text = "PC"; gameCommand.text = ""
            }
        }
        onRejected: {
            editingId = -1
            gameTitle.text = ""; gamePath.text = ""; gamePlatform.text = "PC"; gameCommand.text = ""
        }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: "Adicione um jogo que o scanner não encontrou. O registro será preservado nas próximas buscas."; color: "#9BAEA8"; wrapMode: Text.Wrap; Layout.fillWidth: true }
            TextField { id: gameTitle; Layout.fillWidth: true; placeholderText: "Nome do jogo *" }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: gamePath; Layout.fillWidth: true; placeholderText: "Caminho do executável ou ROM" }
                Button { text: "Escolher arquivo"; onClicked: manualFilePicker.open() }
            }
            TextField { id: gamePlatform; Layout.fillWidth: true; text: "PC"; placeholderText: "Plataforma (PC, SNES, PS1...)" }
            TextField { id: gameCommand; Layout.fillWidth: true; placeholderText: "Comando de inicialização (opcional)" }
            RowLayout {
                Layout.fillWidth: true
                Button { text: "＋ Adicionar arquivos..."; onClicked: filePicker.open() }
                Button { text: "＋ Adicionar pasta..."; onClicked: folderPicker.open() }
                Item { Layout.fillWidth: true }
            }
            Text { text: "Arquivos selecionados entram como registros PC e podem ser configurados depois com um launcher ou emulador."; color: "#9BAEA8"; wrapMode: Text.Wrap; Layout.fillWidth: true; font.pixelSize: 11 }
        }
        background: Rectangle { color: "#0A1110"; radius: 12; border.color: "#176B5D" }
    }

    Dialog {
        id: removeDialog
        property int pendingId: -1
        title: "Excluir jogo manual"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            if (pendingId >= 0) gameCatalog.removeGame(pendingId)
            pendingId = -1
        }
        contentItem: Text {
            text: "Remover este registro da biblioteca? O arquivo original não será apagado."
            color: "#EAF4F1"
            wrapMode: Text.Wrap
            width: 360
        }
        background: Rectangle { color: "#0A1110"; radius: 12; border.color: "#176B5D" }
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
                ComboBox {
                    id: emulatorPlatform
                    Layout.fillWidth: true
                    model: ["NES", "SNES", "Game Boy", "Game Boy Advance", "Nintendo DS", "Mega Drive", "Nintendo 64", "PlayStation", "PC / Retro"]
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
                TextField { id: retroarchExecutable; Layout.fillWidth: true; placeholderText: "Executável (ex.: /usr/bin/retroarch)" }
                Button { text: "Escolher"; onClicked: emulatorExecutablePicker.open() }
                Button { text: "Detectar RetroArch"; onClicked: retroarchExecutable.text = gameCatalog.detectRetroArch() }
            }
            TextField { id: emulatorArguments; Layout.fillWidth: true; placeholderText: "Argumentos (ex.: --fullscreen {ROM})" }
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: emulatorPreset
                    Layout.fillWidth: true
                    model: ["Personalizado", "Tela cheia", "Janela", "Tela cheia 4:3", "Tela cheia 16:9"]
                }
                Button { text: "Aplicar preset"; onClicked: applyEmulatorPreset() }
                ComboBox {
                    id: aspectRatio
                    Layout.preferredWidth: 130
                    model: ["auto", "4:3", "16:9", "16:10", "21:9"]
                }
            }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: emulatorCore; Layout.fillWidth: true; placeholderText: "Core opcional (ex.: snes9x_libretro.so)" }
                Button {
                    text: "Sugerir core"
                    onClicked: emulatorCore.text = gameCatalog.suggestRetroArchCore(emulatorPlatform.currentText)
                }
            }
            Text {
                text: gameCatalog.detectRetroArchCores().length + " core(s) RetroArch encontrado(s) nas pastas padrão"
                color: "#829A93"; font.pixelSize: 10; Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "Salvar configuração"
                    onClicked: {
                        if (gameCatalog.saveEmulatorConfig(emulatorPlatform.currentText, retroarchExecutable.text, emulatorArguments.text, emulatorCore.text, emulatorPreset.currentText, aspectRatio.currentText, true))
                            emulatorDialog.close()
                    }
                }
                Button {
                    text: "Remover plataforma"
                    onClicked: {
                        gameCatalog.removeEmulatorConfig(emulatorPlatform.currentText)
                        retroarchExecutable.text = ""; emulatorArguments.text = ""; emulatorCore.text = ""
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }
        background: Rectangle { color: "#0A1110"; radius: 12; border.color: "#176B5D" }
    }

    FileDialog {
        id: emulatorExecutablePicker
        title: "Escolher executável do emulador"
        fileMode: FileDialog.OpenFile
        onAccepted: retroarchExecutable.text = gameCatalog.localPathFromUrl(selectedFile.toString())
    }

    FileDialog {
        id: manualFilePicker
        title: "Escolher executável ou ROM"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Executáveis e ROMs (*.exe *.AppImage *.sh *.rom *.nes *.sfc *.smc *.gb *.gbc *.gba *.nds *.iso *.chd *.cue *.cdi *.gdi *.z64 *.n64 *.v64 *.md *.gen)", "Todos os arquivos (*)"]
        onAccepted: {
            var path = gameCatalog.localPathFromUrl(selectedFile.toString())
            gamePath.text = path
            if (gameTitle.text.trim().length === 0)
                gameTitle.text = path.substring(path.lastIndexOf("/") + 1).replace(/\.[^.]+$/, "")
        }
    }

    FileDialog {
        id: filePicker
        title: "Adicionar executáveis ou ROMs"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Executáveis e ROMs (*.exe *.AppImage *.sh *.rom *.nes *.sfc *.smc *.gb *.gbc *.gba *.nds *.iso *.chd *.cue *.cdi *.gdi *.z64 *.n64 *.v64 *.md *.gen)", "Todos os arquivos (*)"]
        onAccepted: addSelectedFiles(selectedFiles)
    }

    FileDialog {
        id: folderPicker
        title: "Adicionar uma pasta de jogos"
        fileMode: FileDialog.OpenDirectory
        onAccepted: {
            var path = gameCatalog.localPathFromUrl(selectedFile.toString())
            if (gameCatalog.addPath(path, "PC / Pasta")) gameCatalog.refresh()
        }
    }

    Connections {
        target: gameScanner
        function onScanFinished(filesFound) { candidateDialog.open() }
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
        background: Rectangle { color: "#0A1110"; radius: 12; border.color: "#176B5D" }
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
                        Button {
                            text: modelData.importable === false ? "Executável indisponível" : (modelData.source === "Steam" ? "＋ Importar Steam" : "＋ Importar")
                            enabled: modelData.importable !== false
                            onClicked: {
                                if (gameCatalog.importCandidate(modelData.title, modelData.path, modelData.platform,
                                    modelData.source || "scanner", modelData.launchCommand || ""))
                                    gameScanner.removeCandidate(index)
                            }
                        }
                        Button { text: "Ignorar"; onClicked: gameScanner.removeCandidate(index) }
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
        background: Rectangle { color: "#080F0E"; radius: 12; border.color: "#176B5D" }
    }

    Dialog {
        id: returnDesktopDialog
        title: "Voltar ao desktop?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: Qt.quit()
        contentItem: Text {
            text: "O Hub será fechado; a sessão gráfica continuará aberta no desktop."
            color: "#EAF4F1"
            wrapMode: Text.Wrap
            width: 380
        }
        background: Rectangle { color: "#0A1110"; radius: 12; border.color: "#176B5D" }
    }

    Connections {
        target: gameCatalog
        function onErrorOccurred(message) {
            errorDialog.text = message
            errorDialog.open()
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
        background: Rectangle { color: "#0A1110"; radius: 12; border.color: "#176B5D" }
    }
}
