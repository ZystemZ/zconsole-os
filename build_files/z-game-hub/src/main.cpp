#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>
#include "GameCatalog.h"
#include "Scanner.h"
#include "Launcher.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Z-GAME HUB");
    QGuiApplication::setApplicationVersion("1.0.0");
    QGuiApplication::setOrganizationName("Zystem");

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDir);
    const QString dbPath = dataDir + "/zgames.sqlite";

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qCritical() << "Não foi possível abrir o catálogo:" << db.lastError().text();
        return 2;
    }

    GameCatalog catalog;
    if (!catalog.initialize(db)) {
        qCritical() << "Não foi possível inicializar o catálogo.";
        return 3;
    }

    Scanner scanner(&catalog);
    Launcher launcher;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("gameCatalog", &catalog);
    engine.rootContext()->setContextProperty("gameScanner", &scanner);
    engine.rootContext()->setContextProperty("gameLauncher", &launcher);
    engine.loadFromModule("ZGameHub", "Main");
    if (engine.rootObjects().isEmpty()) return 4;
    return app.exec();
}
