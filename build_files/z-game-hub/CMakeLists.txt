cmake_minimum_required(VERSION 3.21)
project(ZGameHub VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Sql Network)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(z-game-hub
    src/main.cpp
    src/GameCatalog.h
    src/GameCatalog.cpp
    src/Scanner.h
    src/Scanner.cpp
    src/Launcher.h
    src/Launcher.cpp
        src/QuiverIntegration.h
        src/QuiverIntegration.cpp
        src/ZCardService.h
        src/ZCardService.cpp
        src/zconsole/src/asset_runtime.cpp
        src/zconsole/src/runtime/asset_stream.cpp
        src/zconsole/src/zcard/virtual_card.cpp
        src/zconsole/src/zcard/zcard_archive.cpp
        src/zconsole/src/zcard/zcard_container.cpp
        src/zconsole/src/zcard/zcf_hash.cpp
        src/zconsole/src/emulator/console.cpp
        src/zconsole/src/emulator/cpu.cpp
        src/zconsole/src/emulator/zexe.cpp
)

qt_add_qml_module(z-game-hub
    URI ZGameHub
    VERSION 1.0
    QML_FILES
        ui/Main.qml
        ui/components/GameCard.qml
        ui/components/ZButton.qml
        ui/components/ZTextField.qml
        ui/components/ZComboBox.qml
        ui/components/ZTextArea.qml
    RESOURCES
        assets/z-hub-hero.png
        assets/game-card-placeholder.png
        assets/zconsole-mark.svg
)

target_include_directories(z-game-hub PRIVATE src/zconsole/include)

target_link_libraries(z-game-hub PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Sql Qt6::Network
)

if(WIN32)
    set_target_properties(z-game-hub PROPERTIES WIN32_EXECUTABLE TRUE)
    install(TARGETS z-game-hub RUNTIME DESTINATION .)
else()
    install(TARGETS z-game-hub RUNTIME DESTINATION bin)
endif()

if(UNIX AND NOT APPLE)
    install(FILES packaging/z-game-hub.desktop DESTINATION share/applications)
    install(FILES assets/zconsole-mark.svg DESTINATION share/icons/hicolor/scalable/apps)
endif()
