#include "GameCatalog.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QDirIterator>
#include <QStringList>
#include <QSet>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonParseError>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QJsonValue>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QBuffer>
#include <QTimer>

namespace {
QString manualPlatformForPath(const QString &platform, const QString &path) {
    QString result = platform.trimmed();
#ifdef Q_OS_WIN
    const QString suffix = QFileInfo(path).suffix().toLower();
    if ((result.isEmpty() || result.compare("PC", Qt::CaseInsensitive) == 0) &&
        (suffix == "exe" || suffix == "lnk"))
        return "PC / Aplicativo";
#endif
    return result.isEmpty() ? "PC" : result;
}
}

GameCatalog::GameCatalog(QObject *parent) : QObject(parent), m_network(new QNetworkAccessManager(this)) {}

bool GameCatalog::initialize(const QSqlDatabase &db) {
    m_db = db;
    m_artworkDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/covers";
    QSqlQuery q(m_db);
    const QString gamesSchema = R"SQL(
        CREATE TABLE IF NOT EXISTS games (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            path TEXT NOT NULL DEFAULT '',
            platform TEXT NOT NULL DEFAULT 'PC',
            source TEXT NOT NULL DEFAULT 'manual',
            launch_command TEXT NOT NULL DEFAULT '',
            cover_path TEXT NOT NULL DEFAULT '',
            available INTEGER NOT NULL DEFAULT 1,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(path, source)
        )
    )SQL";
    const QString emulatorSchema = R"SQL(
        CREATE TABLE IF NOT EXISTS emulator_configs (
            platform TEXT PRIMARY KEY,
            executable TEXT NOT NULL,
            arguments TEXT NOT NULL DEFAULT '',
            core TEXT NOT NULL DEFAULT '',
            preset TEXT NOT NULL DEFAULT 'Personalizado',
            aspect_ratio TEXT NOT NULL DEFAULT 'auto',
            enabled INTEGER NOT NULL DEFAULT 1
        )
    )SQL";
    if (!q.exec(gamesSchema) || !q.exec(emulatorSchema)) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    QSet<QString> gameColumns;
    QSqlQuery gameInfo(m_db);
    if (gameInfo.exec("PRAGMA table_info(games)"))
        while (gameInfo.next()) gameColumns.insert(gameInfo.value(1).toString());
    gameInfo.finish();
    if (!gameColumns.contains("cover_path") && !q.exec("ALTER TABLE games ADD COLUMN cover_path TEXT NOT NULL DEFAULT ''")) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    QSet<QString> emulatorColumns;
    if (q.exec("PRAGMA table_info(emulator_configs)")) {
        while (q.next()) emulatorColumns.insert(q.value(1).toString());
    }
    if (!emulatorColumns.contains("preset") && !q.exec("ALTER TABLE emulator_configs ADD COLUMN preset TEXT NOT NULL DEFAULT 'Personalizado'")) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    if (!emulatorColumns.contains("aspect_ratio") && !q.exec("ALTER TABLE emulator_configs ADD COLUMN aspect_ratio TEXT NOT NULL DEFAULT 'auto'")) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    return loadGames() && loadEmulatorConfigs();
}

bool GameCatalog::loadGames() {
    m_games.clear();
    QSqlQuery q(m_db);
    q.prepare("SELECT id,title,path,platform,source,launch_command,available,cover_path FROM games ORDER BY title COLLATE NOCASE");
    if (!q.exec()) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    while (q.next()) {
        QVariantMap g;
        g["id"] = q.value(0);
        g["title"] = q.value(1);
        g["path"] = q.value(2);
        g["platform"] = q.value(3);
        g["source"] = q.value(4);
        g["launchCommand"] = q.value(5);
        g["available"] = q.value(6).toBool();
        const QString coverPath = q.value(7).toString();
        g["coverPath"] = coverPath;
        g["coverUrl"] = !coverPath.isEmpty() && QFileInfo(coverPath).isFile()
            ? QUrl::fromLocalFile(coverPath).toString() : QString();
        m_games.push_back(g);
    }
    emit gamesChanged();
    return true;
}

QVariantList GameCatalog::games() const { return m_games; }

QVariantList GameCatalog::emulatorConfigs() const { return m_emulatorConfigs; }

bool GameCatalog::loadEmulatorConfigs() {
    m_emulatorConfigs.clear();
    QSqlQuery q(m_db);
    if (!q.exec("SELECT platform,executable,arguments,core,preset,aspect_ratio,enabled FROM emulator_configs ORDER BY platform COLLATE NOCASE")) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    while (q.next()) {
        QVariantMap config;
        config["platform"] = q.value(0);
        config["executable"] = q.value(1);
        config["arguments"] = q.value(2);
        config["core"] = q.value(3);
        config["preset"] = q.value(4);
        config["aspectRatio"] = q.value(5);
        config["enabled"] = q.value(6).toBool();
        m_emulatorConfigs.push_back(config);
    }
    emit emulatorConfigsChanged();
    return true;
}

QVariantMap GameCatalog::emulatorConfig(const QString &platform) const {
    for (const QVariant &item : m_emulatorConfigs) {
        const QVariantMap config = item.toMap();
        if (config.value("platform").toString() == platform) return config;
    }
    return {};
}

QString GameCatalog::detectRetroArch() const {
#ifdef Q_OS_WIN
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    const QString programFiles = qEnvironmentVariable("ProgramFiles", "C:/Program Files");
    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)", "C:/Program Files (x86)");
    const QStringList candidates = {
        QStandardPaths::findExecutable("retroarch.exe"),
        programFiles + "/RetroArch/retroarch.exe",
        programFilesX86 + "/RetroArch/retroarch.exe",
        localAppData + "/Programs/RetroArch/retroarch.exe",
        QDir::homePath() + "/RetroArch-Win64/retroarch.exe"
    };
    for (const QString &candidate : candidates)
        if (!candidate.isEmpty() && QFileInfo(candidate).isFile()) return candidate;
    return {};
#else
    const QStringList candidates = {"retroarch", "retroarch-sdl2", "retroarch-wayland"};
    for (const QString &candidate : candidates) {
        const QString path = QStandardPaths::findExecutable(candidate);
        if (!path.isEmpty()) return path;
    }
    if (!QStandardPaths::findExecutable("flatpak").isEmpty())
        return "flatpak run org.libretro.RetroArch";
    return {};
#endif
}

QStringList GameCatalog::detectRetroArchCores() const {
    QStringList roots;
    const QString home = QDir::homePath();
#ifdef Q_OS_WIN
    const QString appData = qEnvironmentVariable("APPDATA", home + "/AppData/Roaming");
    const QString programFiles = qEnvironmentVariable("ProgramFiles", "C:/Program Files");
    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)", "C:/Program Files (x86)");
    roots << appData + "/RetroArch/cores"
          << programFiles + "/RetroArch/cores"
          << programFilesX86 + "/RetroArch/cores";
    const QString retroArchExecutable = detectRetroArch();
    if (!retroArchExecutable.isEmpty())
        roots << QFileInfo(retroArchExecutable).absolutePath() + "/cores";
#else
    roots << home + "/.config/retroarch/cores"
          << home + "/.var/app/org.libretro.RetroArch/config/retroarch/cores"
          << home + "/.var/app/org.libretro.RetroArch/data/retroarch/cores"
          << "/usr/lib/libretro" << "/usr/lib64/libretro" << "/app/lib/retroarch/cores";
#endif

    QStringList cores;
    QSet<QString> seen;
    for (const QString &root : roots) {
        QDir dir(root);
        if (!dir.exists()) continue;
#ifdef Q_OS_WIN
        QDirIterator it(root, {"*_libretro.dll"}, QDir::Files | QDir::Readable,
#else
        QDirIterator it(root, {"*_libretro.so"}, QDir::Files | QDir::Readable,
#endif
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = QFileInfo(it.next()).absoluteFilePath();
            if (!seen.contains(path)) {
                seen.insert(path);
                cores << path;
            }
        }
    }
    cores.sort(Qt::CaseInsensitive);
    return cores;
}

QString GameCatalog::suggestRetroArchCore(const QString &platform) const {
    const QString normalized = platform.toLower();
    QStringList tokens;
    if (normalized.contains("nintendo 64")) tokens = {"mupen64plus_next", "parallel_n64"};
    else if (normalized.contains("playstation 2")) tokens = {"pcsx2", "lrps2"};
    else if (normalized.contains("playstation")) tokens = {"swanstation", "beetle_psx", "pcsx_rearmed"};
    else if (normalized.contains("psp")) tokens = {"ppsspp"};
    else if (normalized.contains("dreamcast")) tokens = {"flycast", "reicast"};
    else if (normalized.contains("game boy advance")) tokens = {"mgba", "vba_next"};
    else if (normalized.contains("game boy")) tokens = {"gambatte", "mgba", "tgbdual"};
    else if (normalized.contains("nintendo ds")) tokens = {"desmume", "melonds"};
    else if (normalized.contains("mega drive") || normalized.contains("master system") ||
             normalized.contains("game gear") || normalized.contains("sega cd"))
        tokens = {"genesis_plus_gx", "picodrive"};
    else if (normalized == "wii" || normalized == "wii u") tokens = {"dolphin"};
    else if (normalized.contains("nintendo 3ds")) tokens = {"citra"};
    else if (normalized.contains("snes")) tokens = {"snes9x", "bsnes"};
    else if (normalized.contains("nes")) tokens = {"mesen", "nestopia", "fceumm"};
    else return {};

    const QStringList cores = detectRetroArchCores();
    for (const QString &token : tokens) {
        for (const QString &core : cores) {
            if (QFileInfo(core).fileName().toLower().contains(token)) return core;
        }
    }
    return {};
}

bool GameCatalog::saveEmulatorConfig(const QString &platform, const QString &executable,
                                     const QString &arguments, const QString &core,
                                     const QString &preset, const QString &aspectRatio, bool enabled) {
    if (platform.trimmed().isEmpty() || executable.trimmed().isEmpty()) {
        emit errorOccurred("Informe a plataforma e o executável do emulador.");
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(R"SQL(INSERT INTO emulator_configs(platform,executable,arguments,core,preset,aspect_ratio,enabled)
                    VALUES(?,?,?,?,?,?,?)
                    ON CONFLICT(platform) DO UPDATE SET
                    executable=excluded.executable, arguments=excluded.arguments,
                    core=excluded.core, preset=excluded.preset,
                    aspect_ratio=excluded.aspect_ratio, enabled=excluded.enabled)SQL");
    q.addBindValue(platform.trimmed());
    q.addBindValue(executable.trimmed());
    q.addBindValue(arguments.trimmed());
    q.addBindValue(core.trimmed());
    q.addBindValue(preset.trimmed().isEmpty() ? "Personalizado" : preset.trimmed());
    q.addBindValue(aspectRatio.trimmed().isEmpty() ? "auto" : aspectRatio.trimmed());
    q.addBindValue(enabled ? 1 : 0);
    if (!q.exec()) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    return loadEmulatorConfigs();
}

bool GameCatalog::removeEmulatorConfig(const QString &platform) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM emulator_configs WHERE platform=?");
    q.addBindValue(platform);
    if (!q.exec()) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    return loadEmulatorConfigs();
}

QString GameCatalog::localPathFromUrl(const QString &url) const {
    const QUrl parsed(url);
    return parsed.isLocalFile() ? parsed.toLocalFile() : url;
}

QString GameCatalog::localFileUrl(const QString &path) const {
    const QString value = path.trimmed();
    if (value.isEmpty()) return {};
    const QUrl parsed(value);
    return parsed.isLocalFile() ? parsed.toString() : QUrl::fromLocalFile(QFileInfo(value).absoluteFilePath()).toString();
}

qint64 GameCatalog::gameIdForPath(const QString &path, const QString &source) const {
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM games WHERE path=? AND source=?");
    query.addBindValue(QDir::cleanPath(path));
    query.addBindValue(source);
    return query.exec() && query.next() ? query.value(0).toLongLong() : -1;
}

bool GameCatalog::storeCoverImage(qint64 id, const QImage &image) {
    if (id <= 0 || image.isNull()) return false;
    QSqlQuery current(m_db);
    current.prepare("SELECT cover_path FROM games WHERE id=?");
    current.addBindValue(id);
    if (!current.exec() || !current.next()) return false;
    const QString oldPath = current.value(0).toString();
    current.finish();
    if (!QDir().mkpath(m_artworkDirectory)) {
        emit errorOccurred("Não foi possível criar a pasta local para capas.");
        return false;
    }
    const QString newPath = QDir(m_artworkDirectory).filePath(QString("game_%1.png").arg(id));
    QSaveFile output(newPath);
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG") || !output.commit()) {
        output.cancelWriting();
        emit errorOccurred("Não foi possível salvar a imagem de capa na biblioteca.");
        return false;
    }
    QSqlQuery update(m_db);
    update.prepare("UPDATE games SET cover_path=? WHERE id=?");
    update.addBindValue(newPath);
    update.addBindValue(id);
    if (!update.exec()) {
        QFile::remove(newPath);
        emit errorOccurred(update.lastError().text());
        return false;
    }
    const QString oldClean = QDir::cleanPath(oldPath);
    const QString newClean = QDir::cleanPath(newPath);
    const QString relativeOld = QDir(m_artworkDirectory).relativeFilePath(oldClean);
    if (!oldPath.isEmpty() && oldClean != newClean && !relativeOld.startsWith("..") &&
        !QDir::isAbsolutePath(relativeOld))
        QFile::remove(oldClean);
    return true;
}

bool GameCatalog::applyCoverPath(qint64 id, const QString &coverPath) {
    if (id <= 0) return false;
    const QString requested = coverPath.trimmed();
    if (requested.isEmpty()) {
        QSqlQuery current(m_db);
        current.prepare("SELECT cover_path FROM games WHERE id=?");
        current.addBindValue(id);
        if (!current.exec() || !current.next()) return false;
        const QString previousPath = current.value(0).toString();
        const QString oldPath = previousPath.isEmpty() ? QString() : QDir::cleanPath(previousPath);
        current.finish();
        QSqlQuery update(m_db);
        update.prepare("UPDATE games SET cover_path='' WHERE id=?");
        update.addBindValue(id);
        if (!update.exec()) { emit errorOccurred(update.lastError().text()); return false; }
        const QString relativeOld = QDir(m_artworkDirectory).relativeFilePath(oldPath);
        if (!oldPath.isEmpty() && !relativeOld.startsWith("..") && !QDir::isAbsolutePath(relativeOld))
            QFile::remove(oldPath);
        return true;
    }

    const QUrl parsed(requested);
    const QString localPath = parsed.isLocalFile() ? parsed.toLocalFile() : requested;
    if (!QFileInfo(localPath).isFile()) {
        emit errorOccurred("O arquivo de capa selecionado não foi encontrado.");
        return false;
    }
    QImageReader reader(localPath);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        emit errorOccurred("O arquivo escolhido não é uma imagem compatível. Use PNG, JPG, JPEG, WEBP, BMP ou GIF.");
        return false;
    }
    const QSize dimensions = reader.size();
    if (dimensions.isValid() && (dimensions.width() > 12000 || dimensions.height() > 12000 ||
        qint64(dimensions.width()) * dimensions.height() > 80000000)) {
        emit errorOccurred("A imagem é grande demais para usar como capa. Escolha uma imagem menor.");
        return false;
    }
    const QImage image = reader.read();
    if (image.isNull()) {
        emit errorOccurred("Não foi possível decodificar a imagem de capa.");
        return false;
    }
    return storeCoverImage(id, image);
}

bool GameCatalog::addPath(const QString &path, const QString &platform) {
    const QString requestedPath = path.trimmed();
    if (requestedPath.isEmpty()) {
        emit errorOccurred("Selecione um arquivo ou uma pasta antes de adicionar.");
        return false;
    }
    const QString cleanPath = QDir::cleanPath(requestedPath);
    const QFileInfo info(cleanPath);
    if (!info.exists()) {
        emit errorOccurred("O caminho selecionado não existe ou não está disponível.");
        return false;
    }

    QString title = info.isDir() ? info.fileName() : info.completeBaseName();
    if (title.isEmpty()) title = info.fileName();
    return addManualGame(title, cleanPath, platform, QString());
}

bool GameCatalog::addManualGame(const QString &title, const QString &path,
                                const QString &platform, const QString &launchCommand,
                                const QString &coverPath) {
    if (title.trimmed().isEmpty()) {
        emit errorOccurred("Informe o nome do jogo ou aplicativo.");
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(R"SQL(INSERT INTO games(title,path,platform,source,launch_command,available)
                    VALUES(?,?,?,'manual',?,1)
                    ON CONFLICT(path,source) DO UPDATE SET
                    title=excluded.title, platform=excluded.platform,
                    launch_command=excluded.launch_command, available=1)SQL");
    q.addBindValue(title.trimmed());
    const QString cleanPath = QDir::cleanPath(path.trimmed());
    q.addBindValue(cleanPath);
    q.addBindValue(manualPlatformForPath(platform, cleanPath));
    q.addBindValue(launchCommand.trimmed());
    if (!q.exec()) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    q.finish();
    const qint64 id = gameIdForPath(cleanPath, "manual");
    if (id > 0 && !applyCoverPath(id, coverPath)) return false;
    return loadGames();
}

bool GameCatalog::importCandidate(const QString &title, const QString &path,
                                  const QString &platform, const QString &source,
                                  const QString &launchCommand, const QString &coverPath,
                                  const QString &appId) {
    if (title.trimmed().isEmpty() || path.trimmed().isEmpty()) {
        emit errorOccurred("O candidato precisa ter nome e caminho.");
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(R"SQL(INSERT INTO games(title,path,platform,source,launch_command,available)
                    VALUES(?,?,?,?,?,1)
                    ON CONFLICT(path,source) DO UPDATE SET
                    title=excluded.title, platform=excluded.platform,
                    launch_command=excluded.launch_command, available=1)SQL");
    q.addBindValue(title.trimmed());
    q.addBindValue(QDir::cleanPath(path.trimmed()));
    q.addBindValue(platform.trimmed().isEmpty() ? "PC" : platform.trimmed());
    q.addBindValue(source.trimmed().isEmpty() ? "scanner" : source.trimmed());
    q.addBindValue(launchCommand.trimmed());
    if (!q.exec()) { emit errorOccurred(q.lastError().text()); return false; }
    q.finish();
    const QString cleanPath = QDir::cleanPath(path.trimmed());
    const QString cleanSource = source.trimmed().isEmpty() ? "scanner" : source.trimmed();
    const qint64 id = gameIdForPath(cleanPath, cleanSource);
    bool hasCover = false;
    if (id > 0 && !coverPath.trimmed().isEmpty()) hasCover = applyCoverPath(id, coverPath);
    if (id > 0 && cleanSource == "Steam" && !hasCover)
        fetchSteamCover(id, appId);
    return loadGames();
}

bool GameCatalog::updateManualGame(qint64 id, const QString &title, const QString &path,
                                   const QString &platform, const QString &launchCommand,
                                   const QString &coverPath) {
    if (title.trimmed().isEmpty()) {
        emit errorOccurred("Informe o nome do jogo ou aplicativo.");
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(R"SQL(UPDATE games SET title=?, path=?, platform=?, launch_command=?, available=1
                    WHERE id=? AND source='manual')SQL");
    q.addBindValue(title.trimmed());
    const QString cleanPath = QDir::cleanPath(path.trimmed());
    q.addBindValue(cleanPath);
    q.addBindValue(manualPlatformForPath(platform, cleanPath));
    q.addBindValue(launchCommand.trimmed());
    q.addBindValue(id);
    if (!q.exec()) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    if (q.numRowsAffected() == 0) {
        emit errorOccurred("Somente jogos adicionados manualmente podem ser editados.");
        return false;
    }
    q.finish();
    if (!applyCoverPath(id, coverPath)) return false;
    return loadGames();
}

bool GameCatalog::setGameCover(qint64 id, const QString &coverPath) {
    m_pendingSteamCovers.remove(id);
    if (id <= 0 || !applyCoverPath(id, coverPath)) return false;
    return loadGames();
}

void GameCatalog::fetchSteamCover(qint64 id, const QString &appId, bool headerFallback) {
    if (id <= 0 || !QRegularExpression("^[0-9]{1,10}$").match(appId).hasMatch()) return;
    if (!headerFallback && m_pendingSteamCovers.contains(id)) return;
    if (!headerFallback) m_pendingSteamCovers.insert(id);

    QSqlQuery current(m_db);
    current.prepare("SELECT cover_path FROM games WHERE id=? AND source='Steam'");
    current.addBindValue(id);
    if (!current.exec() || !current.next()) { m_pendingSteamCovers.remove(id); return; }
    const QString savedPath = current.value(0).toString();
    current.finish();
    if (!savedPath.isEmpty() && QFileInfo(savedPath).isFile()) { m_pendingSteamCovers.remove(id); return; }

    const QString assetName = headerFallback ? "header.jpg" : "library_600x900.jpg";
    const QUrl url(QString("https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/%1/%2")
                       .arg(appId, assetName));
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_network->get(request);
    auto *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
    timeout->start(15000);
    connect(reply, &QNetworkReply::finished, this, [this, reply, id, appId, headerFallback]() {
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray bytes = reply->readAll();
        const bool requestOk = reply->error() == QNetworkReply::NoError && httpStatus >= 200 &&
                               httpStatus < 300 && bytes.size() <= 15 * 1024 * 1024;
        reply->deleteLater();
        if (!m_pendingSteamCovers.contains(id)) return;

        QImage image;
        if (requestOk) {
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer);
            reader.setAutoTransform(true);
            const QSize dimensions = reader.size();
            if (!dimensions.isValid() || (dimensions.width() <= 12000 && dimensions.height() <= 12000 &&
                qint64(dimensions.width()) * dimensions.height() <= 80000000))
                image = reader.read();
        }
        if (image.isNull()) {
            if (!headerFallback) fetchSteamCover(id, appId, true);
            else m_pendingSteamCovers.remove(id);
            return;
        }

        QSqlQuery currentPath(m_db);
        currentPath.prepare("SELECT cover_path FROM games WHERE id=? AND source='Steam'");
        currentPath.addBindValue(id);
        if (!currentPath.exec() || !currentPath.next()) {
            m_pendingSteamCovers.remove(id);
            return;
        }
        const QString currentCover = currentPath.value(0).toString();
        currentPath.finish();
        if (!currentCover.isEmpty() && QFileInfo(currentCover).isFile()) {
            m_pendingSteamCovers.remove(id);
            return;
        }
        if (storeCoverImage(id, image)) loadGames();
        m_pendingSteamCovers.remove(id);
    });
}

bool GameCatalog::launchGame(qint64 id) {
    QSqlQuery q(m_db);
    q.prepare("SELECT launch_command,path,source,platform FROM games WHERE id=?");
    q.addBindValue(id);
    if (!q.exec() || !q.next()) { emit errorOccurred("Jogo não encontrado no catálogo."); return false; }
    const QString command = q.value(0).toString().trimmed();
    const QString path = q.value(1).toString();
    const QString source = q.value(2).toString();
    const QString platform = q.value(3).toString();
    if (!command.isEmpty()) {
        if (command.startsWith("steam://", Qt::CaseInsensitive)) {
            const bool ok = QDesktopServices::openUrl(QUrl(command));
            if (!ok) emit errorOccurred("Não foi possível abrir o protocolo Steam. Verifique se a Steam está instalada.");
            return ok;
        }
#ifdef Q_OS_WIN
        if (source == "Epic") {
            const QUrl epicUrl(command, QUrl::StrictMode);
            const QUrlQuery query(epicUrl);
            QString uriPath = epicUrl.path(QUrl::FullyDecoded);
            if (uriPath.startsWith('/')) uriPath.remove(0, 1);
            uriPath = QDir::cleanPath(QDir::fromNativeSeparators(uriPath));
            const QString installedPath = QDir::cleanPath(QDir::fromNativeSeparators(path));
            if (epicUrl.scheme() != "com.epicgames.launcher" || epicUrl.host() != "apps" ||
                query.queryItemValue("action") != "launch" || query.queryItemValue("silent") != "true" ||
                uriPath.compare(installedPath, Qt::CaseInsensitive) != 0 || !QFileInfo(path).isDir()) {
                emit errorOccurred("Link de lançamento Epic inválido ou instalação indisponível.");
                return false;
            }
            const bool ok = QDesktopServices::openUrl(epicUrl);
            if (!ok) emit errorOccurred("Não foi possível abrir Epic Games Launcher. Verifique se está instalado.");
            return ok;
        }
        if (source == "Ubisoft") {
            const QUrl ubisoftUrl(command, QUrl::StrictMode);
            if (ubisoftUrl.scheme() != "uplay" || ubisoftUrl.host() != "launch" ||
                !QRegularExpression("^/[0-9]{1,12}/0$").match(ubisoftUrl.path()).hasMatch() ||
                !QFileInfo(path).isDir()) {
                emit errorOccurred("Link Ubisoft Connect inválido ou instalação indisponível.");
                return false;
            }
            const bool ok = QDesktopServices::openUrl(ubisoftUrl);
            if (!ok) emit errorOccurred("Não foi possível abrir Ubisoft Connect. Verifique se está instalado.");
            return ok;
        }
        if (source == "Lutris" || source == "Bottles") {
            emit errorOccurred("Esta edição Windows não usa launchers da camada Linux (Lutris/Bottles).");
            return false;
        }
        if (source == "Heroic" && command.startsWith("heroic://", Qt::CaseInsensitive)) {
            const QUrl heroicUrl(command);
            const QUrlQuery query(heroicUrl);
            const QString runner = query.queryItemValue("runner");
            if (heroicUrl.scheme() != "heroic" || heroicUrl.host() != "launch" ||
                query.queryItemValue("appName").isEmpty() ||
                (runner != "legendary" && runner != "gog" && runner != "nile")) {
                emit errorOccurred("Link de lançamento Heroic inválido.");
                return false;
            }
            if (path.isEmpty() || !QFileInfo(path).isDir()) {
                emit errorOccurred("A instalação Heroic deste jogo não está disponível.");
                return false;
            }
            const bool ok = QDesktopServices::openUrl(heroicUrl);
            if (!ok) emit errorOccurred("Não foi possível abrir Heroic. Instale ou repare o Heroic Games Launcher para Windows.");
            return ok;
        }
#endif
        // argv: stores exact arguments as JSON; legacy commands are tokenized and
        // passed to QProcess directly, never through a shell.
        QStringList parts;
        if (command.startsWith("argv:")) {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(command.mid(5).toUtf8(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
                emit errorOccurred("Argumentos de inicialização armazenados são inválidos.");
                return false;
            }
            for (const QJsonValue &value : doc.array()) {
                if (!value.isString()) { emit errorOccurred("Argumentos de inicialização armazenados são inválidos."); return false; }
                parts << value.toString();
            }
        } else {
            parts = QProcess::splitCommand(command);
        }
        if (parts.isEmpty()) { emit errorOccurred("Comando de inicialização vazio."); return false; }
        const QString program = parts.first();
        const QStringList args = parts.mid(1);
        const auto executableAvailable = [](const QString &name) {
            return !QStandardPaths::findExecutable(name).isEmpty() ||
                   (QFileInfo(name).isFile() && QFileInfo(name).isExecutable());
        };
        if (source == "Heroic") {
            if (path.isEmpty() || !QFileInfo(path).isDir()) {
                emit errorOccurred("A instalação Heroic deste jogo não está disponível no momento.");
                return false;
            }
            if (args.isEmpty()) { emit errorOccurred("Link de lançamento Heroic ausente."); return false; }
            const QUrl heroicUrl(args.last());
            const QUrlQuery query(heroicUrl);
            const QString runner = query.queryItemValue("runner");
            if (heroicUrl.scheme() != "heroic" || heroicUrl.host() != "launch" ||
                query.queryItemValue("appName").isEmpty() ||
                (runner != "legendary" && runner != "gog" && runner != "nile")) {
                emit errorOccurred("Link Heroic inválido ou incompatível.");
                return false;
            }
            const bool nativeCommand = program == "heroic" && args.size() == 2 && args.first() == "--no-gui";
            const bool flatpakCommand = program == "flatpak" && args.size() == 4 && args.at(0) == "run" &&
                args.at(1) == "com.heroicgameslauncher.hgl" && args.at(2) == "--no-gui";
            if ((!nativeCommand && !flatpakCommand) || !executableAvailable(program)) {
                emit errorOccurred("Heroic não está instalado ou o comando de lançamento não é válido.");
                return false;
            }
        } else if (source == "Bottles") {
            if (path.isEmpty() || !QFileInfo(path).isFile()) {
                emit errorOccurred("O executável deste programa Bottles não está disponível.");
                return false;
            }
            const bool nativeCommand = program == "bottles-cli" && args.size() == 5 && args.at(0) == "run" &&
                args.at(1) == "-b" && args.at(3) == "-p";
            const bool flatpakCommand = program == "flatpak" && args.size() == 8 && args.at(0) == "run" &&
                args.at(1) == "--command=bottles-cli" && args.at(2) == "com.usebottles.bottles" &&
                args.at(3) == "run" && args.at(4) == "-b" && args.at(6) == "-p";
            if ((!nativeCommand && !flatpakCommand) || !executableAvailable(program)) {
                emit errorOccurred("Bottles não está instalado ou o comando de lançamento não é válido.");
                return false;
            }
        } else if (source == "Lutris") {
            const QString lutrisUri = args.isEmpty() ? QString() : args.last();
            const QString slug = lutrisUri.startsWith("lutris:") ? lutrisUri.mid(7) : QString();
            const bool safeSlug = QRegularExpression("^[A-Za-z0-9._-]+$").match(slug).hasMatch();
            const bool nativeCommand = program == "lutris" && args.size() == 1;
            const bool flatpakCommand = program == "flatpak" && args.size() == 3 && args.at(0) == "run" &&
                args.at(1) == "net.lutris.Lutris";
            if ((!nativeCommand && !flatpakCommand) || !safeSlug || !executableAvailable(program)) {
                emit errorOccurred("Lutris não está instalado ou o identificador do jogo é inválido.");
                return false;
            }
        } else if (!executableAvailable(program)) {
            emit errorOccurred("O executável configurado não foi encontrado ou não pode ser iniciado: " + program);
            return false;
        }
        const bool ok = QProcess::startDetached(program, args);
        if (!ok) emit errorOccurred("Falha ao iniciar o comando configurado: " + program);
        return ok;
    }
#ifdef Q_OS_WIN
    if (source == "manual" && !path.isEmpty()) {
        const QFileInfo manualInfo(path);
        if (manualInfo.isFile() && manualInfo.suffix().compare("exe", Qt::CaseInsensitive) == 0) {
            const bool ok = QProcess::startDetached(manualInfo.absoluteFilePath(), {});
            if (!ok) emit errorOccurred("Não foi possível iniciar o aplicativo: " + path);
            return ok;
        }
        if (manualInfo.isFile() && manualInfo.suffix().compare("lnk", Qt::CaseInsensitive) == 0) {
            const bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(manualInfo.absoluteFilePath()));
            if (!ok) emit errorOccurred("Não foi possível abrir o atalho: " + path);
            return ok;
        }
    }
#endif
    const QVariantMap emulator = emulatorConfig(platform);
    if (emulator.value("enabled").toBool() && !emulator.value("executable").toString().trimmed().isEmpty()) {
        if (path.isEmpty() || !QFileInfo::exists(path)) {
            emit errorOccurred("A ROM configurada não está disponível neste momento: " + path);
            return false;
        }
        const QString executable = emulator.value("executable").toString().trimmed();
        QString program;
        QStringList args;
        if (QFileInfo::exists(executable) && !QFileInfo(executable).isDir()) {
            program = executable;
        } else {
            const QStringList executableParts = QProcess::splitCommand(executable);
            if (executableParts.isEmpty()) {
                emit errorOccurred("O executável do emulador está vazio ou inválido.");
                return false;
            }
            program = executableParts.first();
            args = executableParts.mid(1);
        }
        if (program.isEmpty()) {
            emit errorOccurred("O executável do emulador está vazio ou inválido.");
            return false;
        }
        args += QProcess::splitCommand(emulator.value("arguments").toString());
        const QString preset = emulator.value("preset").toString();
        const bool hasFullscreen = args.contains("--fullscreen");
        const bool hasWindowed = args.contains("--windowed");
        if (!hasFullscreen && !hasWindowed &&
            (preset == "Tela cheia" || preset == "Tela cheia 4:3" || preset == "Tela cheia 16:9"))
            args << "--fullscreen";
        else if (!hasFullscreen && !hasWindowed && preset == "Janela")
            args << "--windowed";
        const QString aspectRatio = emulator.value("aspectRatio").toString();
        bool hasAspectArgument = false;
        for (const QString &arg : args) {
            if (arg == "--aspect-ratio" || arg.startsWith("--aspect-ratio=")) {
                hasAspectArgument = true;
                break;
            }
        }
        const QString presetAspect = preset == "Tela cheia 4:3" ? "4:3" :
                                     preset == "Tela cheia 16:9" ? "16:9" : aspectRatio;
        if (!hasAspectArgument && !presetAspect.isEmpty() && presetAspect != "auto")
            args << "--aspect-ratio" << presetAspect;
        const QString core = emulator.value("core").toString().trimmed();
        bool hasRomMarker = false;
        bool hasCoreMarker = false;
        for (QString &arg : args) {
            if (arg.contains("{ROM}")) {
                arg.replace("{ROM}", path);
                hasRomMarker = true;
            }
            if (arg.contains("{CORE}")) {
                arg.replace("{CORE}", core);
                hasCoreMarker = true;
            }
        }
        if (!core.isEmpty() && !hasCoreMarker) args << "-L" << core;
        if (!hasRomMarker) args << path;
        const bool ok = QProcess::startDetached(program, args);
        if (!ok) emit errorOccurred("Não foi possível iniciar o emulador configurado: " + program);
        return ok;
    }
    if (source == "Steam") {
        emit errorOccurred("Este jogo Steam não tem um AppID de inicialização salvo.");
        return false;
    }
    if (!path.isEmpty() && QFileInfo(path).isDir()) {
        emit errorOccurred("Este registro aponta para uma pasta. Configure um launcher ou executável antes de iniciar.");
        return false;
    }
    emit errorOccurred("Configure um comando de inicialização para este jogo antes de executar.");
    return false;
}

bool GameCatalog::launchExternalFile(const QString &path, const QString &platform) {
    const QFileInfo file(path);
    if (!file.isFile()) { emit errorOccurred("O conteúdo do ZCARD não está disponível: " + path); return false; }
    if (platform.startsWith("native:", Qt::CaseInsensitive)) {
#ifdef Q_OS_WIN
        if (file.suffix().compare("lnk", Qt::CaseInsensitive) == 0) return QDesktopServices::openUrl(QUrl::fromLocalFile(file.absoluteFilePath()));
        const bool ok = QProcess::startDetached(file.absoluteFilePath(), {});
#elif defined(Q_OS_MACOS)
        const bool ok = file.suffix().compare("app", Qt::CaseInsensitive) == 0
            ? QProcess::startDetached("open", {file.absoluteFilePath()})
            : QProcess::startDetached(file.absoluteFilePath(), {});
#else
        const bool ok = QProcess::startDetached(file.absoluteFilePath(), {});
#endif
        if (!ok) emit errorOccurred("Não foi possível iniciar o executável nativo do ZCARD.");
        return ok;
    }
    const QString canonicalPlatform = platform.trimmed().toLower();
    QVariantMap emulator = emulatorConfig(canonicalPlatform);
    if (!emulator.value("enabled").toBool() || emulator.value("executable").toString().trimmed().isEmpty()) {
        emit errorOccurred("Nenhum emulador externo está configurado para o sistema " + canonicalPlatform + ". Abra Configurações e informe o executável.");
        return false;
    }
    const QString executable = emulator.value("executable").toString().trimmed();
    QString program;
    QStringList args;
    if (QFileInfo::exists(executable) && !QFileInfo(executable).isDir()) {
        program = executable;
    } else {
        const QStringList parts = QProcess::splitCommand(executable);
        if (parts.isEmpty()) { emit errorOccurred("O executável do emulador configurado é inválido."); return false; }
        program = parts.first();
        args = parts.mid(1);
    }
    args += QProcess::splitCommand(emulator.value("arguments").toString());
    const QString core = emulator.value("core").toString().trimmed();
    bool hasRom = false;
    bool hasCore = false;
    for (QString &arg : args) {
        if (arg.contains("{ROM}") || arg.contains("{rom}")) { arg.replace("{ROM}", path); arg.replace("{rom}", path); hasRom = true; }
        if (arg.contains("{CORE}") || arg.contains("{core}")) { arg.replace("{CORE}", core); arg.replace("{core}", core); hasCore = true; }
    }
    if (!core.isEmpty() && !hasCore) args << "-L" << core;
    if (!hasRom) args << path;
    const bool ok = QProcess::startDetached(program, args);
    if (!ok) emit errorOccurred("Não foi possível iniciar o emulador externo: " + program);
    return ok;
}

bool GameCatalog::removeGame(qint64 id) {
    QSqlQuery current(m_db);
    current.prepare("SELECT cover_path FROM games WHERE id=? AND source='manual'");
    current.addBindValue(id);
    const QString coverPath = current.exec() && current.next() ? current.value(0).toString() : QString();
    current.finish();
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM games WHERE id=? AND source='manual'");
    q.addBindValue(id);
    if (!q.exec()) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    const QString relativeCover = QDir(m_artworkDirectory).relativeFilePath(QDir::cleanPath(coverPath));
    if (!coverPath.isEmpty() && !relativeCover.startsWith("..") && !QDir::isAbsolutePath(relativeCover))
        QFile::remove(coverPath);
    return loadGames();
}

void GameCatalog::refresh() { loadGames(); }
