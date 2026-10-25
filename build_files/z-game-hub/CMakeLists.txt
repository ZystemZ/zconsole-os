cmake_minimum_required(VERSION 3.21)
project(ZGameHub VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Sql)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(z-game-hub
    src/main.cpp
    src/GameCatalog.h
    src/GameCatalog.cpp
    src/Scanner.h
    src/Scanner.cpp
    src/Launcher.h
    src/Launcher.cpp
)

qt_add_qml_module(z-game-hub
    URI ZGameHub
    VERSION 1.0
    QML_FILES
        ui/Main.qml
        ui/components/GameCard.qml
        ui/components/ZButton.qml
    RESOURCES
        assets/z-hub-hero.png
        assets/game-card-placeholder.png
)

target_link_libraries(z-game-hub PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Sql
)

install(TARGETS z-game-hub RUNTIME DESTINATION bin)
install(FILES packaging/z-game-hub.desktop DESTINATION share/applications)
