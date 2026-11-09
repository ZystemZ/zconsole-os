#include "Scanner.h"
#include "GameCatalog.h"
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>
#include <QSet>
#include <QVariantMap>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QCoreApplication>
#include <QUrl>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <QSettings>
#endif

namespace {
QString yamlScalar(const QString &text, const QStringList &keys) {
    for (const QString &key : keys) {
        const QRegularExpression rx(QStringLiteral("(?m)^\\s*") + QRegularExpression::escape(key) +
                                    QStringLiteral("\\s*:\\s*([^\\r\\n#]+)"));
        const auto match = rx.match(text);
        if (match.hasMatch()) {
            QString value = match.captured(1).trimmed();
            if (value.size() >= 2 && ((value.startsWith('\"') && value.endsWith('\"')) ||
                                      (value.startsWith('\'') && value.endsWith('\''))))
                value = value.mid(1, value.size() - 2);
            return value;
        }
    }
    return {};
}

QString jsonValue(const QJsonObject &object, const QStringList &keys) {
    for (const QString &key : keys) {
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key().compare(key, Qt::CaseInsensitive) == 0 && it.value().isString())
                return it.value().toString();
        }
    }
    return {};
}

QString existingArtworkPath(QString value) {
    value = value.trimmed();
    if (value.isEmpty()) return {};
    const QUrl url(value);
    if (url.isLocalFile()) value = url.toLocalFile();
    value = QDir::cleanPath(QDir::fromNativeSeparators(value));
    const QFileInfo info(value);
    if (!info.isFile()) return {};
    const QString extension = info.suffix().toLower();
    return QStringList{"png", "jpg", "jpeg", "webp", "bmp", "gif"}.contains(extension) ? info.absoluteFilePath() : QString();
}

QString findSteamArtwork(const QString &appId, const QStringList &steamRoots, const QStringList &steamLibraries) {
    if (!QRegularExpression("^[0-9]{1,10}$").match(appId).hasMatch()) return {};
    QSet<QString> cacheDirs;
    for (const QString &root : steamRoots)
        cacheDirs.insert(QDir(root).filePath("appcache/librarycache"));
    for (const QString &library : steamLibraries)
        cacheDirs.insert(QDir(library).filePath("../appcache/librarycache"));

    const QStringList suffixes = {
        "_library_600x900.jpg", "_library_600x900_2x.jpg",
        "_library_600x900.png", "_library_600x900_2x.png",
        "_library.jpg", "_header.jpg"
    };
    for (const QString &cacheDir : cacheDirs) {
        for (const QString &suffix : suffixes) {
            const QString path = QDir(cacheDir).filePath(appId + suffix);
            const QString artwork = existingArtworkPath(path);
            if (!artwork.isEmpty()) return artwork;
        }
    }
    return {};
}

void collectHeroicEntries(const QJsonValue &value, const QString &source, const QString &runner,
                          const QString &objectKey, bool flatpak,
                          QVariantList *candidates, QSet<QString> *seen) {
    if (value.isArray()) {
        for (const QJsonValue &item : value.toArray()) collectHeroicEntries(item, source, runner, {}, flatpak, candidates, seen);
        return;
    }
    if (!value.isObject()) return;
    const QJsonObject object = value.toObject();
    const QString appName = jsonValue(object, {"app_name", "appName", "appNameId", "id"}).isEmpty()
        ? objectKey : jsonValue(object, {"app_name", "appName", "appNameId", "id"});
    QString resolvedRunner = runner.isEmpty() ? jsonValue(object, {"runner", "store"}).toLower() : runner;
    if (resolvedRunner == "epic") resolvedRunner = "legendary";
    if (resolvedRunner == "amazon") resolvedRunner = "nile";
    if (resolvedRunner != "legendary" && resolvedRunner != "gog" && resolvedRunner != "nile")
        resolvedRunner.clear();
    const QString title = jsonValue(object, {"title", "name", "gameName"}).isEmpty()
        ? appName : jsonValue(object, {"title", "name", "gameName"});
    const QString installPath = jsonValue(object, {"install_path", "installPath", "installDir", "path"});
    if (!title.isEmpty() && !appName.isEmpty() && !installPath.isEmpty()) {
        const QString cleanPath = QDir::cleanPath(installPath);
        const QString dedupeKey = source + QLatin1Char('|') + appName;
        if (!seen->contains(dedupeKey)) {
            seen->insert(dedupeKey);
            QVariantMap c;
            c["title"] = title;
            c["path"] = cleanPath;
            c["platform"] = "PC / Heroic";
            c["kind"] = source + (QFileInfo(cleanPath).isDir() ? " — instalado" : " — caminho indisponível");
            c["source"] = "Heroic";
            c["store"] = source;
            c["appName"] = appName;
            c["runner"] = resolvedRunner;
            const QString artwork = existingArtworkPath(jsonValue(object, {
                "art_cover", "artCover", "art_square", "artSquare", "coverPath", "cover_path", "cover", "image_path", "imagePath"
            }));
            if (!artwork.isEmpty()) c["coverPath"] = artwork;
            const QString encodedApp = QString::fromLatin1(QUrl::toPercentEncoding(appName));
            const QString heroicUri = "heroic://launch?appName=" + encodedApp + "&runner=" + resolvedRunner;
#ifdef Q_OS_WIN
            c["launchCommand"] = !resolvedRunner.isEmpty() ? heroicUri : QString();
#else
            c["launchCommand"] = !resolvedRunner.isEmpty() ? (flatpak
                ? "flatpak run com.heroicgameslauncher.hgl --no-gui " + heroicUri
                : "heroic --no-gui " + heroicUri) : QString();
#endif
            c["importable"] = !resolvedRunner.isEmpty() && !appName.isEmpty();
            c["installed"] = QFileInfo(cleanPath).isDir();
            c["compatPrefix"] = jsonValue(object, {"winePrefix", "wine_prefix", "prefix"});
            candidates->push_back(c);
        }
    }
    for (auto it = object.begin(); it != object.end(); ++it)
        if (it.value().isObject() || it.value().isArray()) collectHeroicEntries(it.value(), source, runner, it.key(), flatpak, candidates, seen);
}

QString encodeArguments(const QStringList &arguments) {
    QJsonArray array;
    for (const QString &argument : arguments) array.append(argument);
    return QStringLiteral("argv:") + QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QByteArray runReadOnlyCommand(const QString &program, const QStringList &arguments, int timeoutMs = 3500) {
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(1000) || !process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(500);
        return {};
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0
        ? process.readAll() : QByteArray();
}
}

Scanner::Scanner(GameCatalog *catalog, QObject *parent) : QObject(parent), m_catalog(catalog) {
    m_deviceTimer.setInterval(2000);
    connect(&m_deviceTimer, &QTimer::timeout, this, &Scanner::refreshExternalDevices);
    m_deviceTimer.start();
    refreshExternalDevices();
}
bool Scanner::busy() const { return m_busy; }
QString Scanner::status() const { return m_status; }
QVariantList Scanner::candidates() const { return m_candidates; }
QVariantList Scanner::externalDevices() const { return m_externalDevices; }
bool Scanner::romSearch() const { return m_romSearch; }
void Scanner::removeCandidate(int index) { if (index >= 0 && index < m_candidates.size()) { m_candidates.removeAt(index); emit candidatesChanged(); } }
void Scanner::setBusy(bool b) { if (m_busy != b) { m_busy=b; emit busyChanged(); } }
void Scanner::setStatus(const QString &s) { m_status=s; emit statusChanged(); }

bool Scanner::isExternalDevicePath(const QString &path) const {
#ifdef Q_OS_WIN
    QString root = QDir::toNativeSeparators(path);
    if (root.size() < 2) return false;
    root = root.left(3);
    const UINT driveType = GetDriveTypeW(reinterpret_cast<LPCWSTR>(root.utf16()));
    const QString systemDrive = qEnvironmentVariable("SystemDrive", "C:").toUpper();
    const bool isSystemDrive = root.left(2).toUpper() == systemDrive;
    return driveType == DRIVE_REMOVABLE || (driveType == DRIVE_FIXED && !isSystemDrive);
#else
    return path.startsWith("/run/media/") || path.startsWith("/media/") || path.startsWith("/mnt/");
#endif
}

void Scanner::refreshExternalDevices() {
    QSet<QString> currentPaths;
    QVariantList currentDevices;
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()) continue;
        const QString path = QDir::cleanPath(storage.rootPath());
        if (!isExternalDevicePath(path) || currentPaths.contains(path)) continue;
        currentPaths.insert(path);
        QVariantMap device;
        device["path"] = path;
        device["label"] = storage.displayName().isEmpty() ? QFileInfo(path).fileName() : storage.displayName();
        device["filesystem"] = QString::fromUtf8(storage.fileSystemType());
        device["bytesAvailable"] = storage.bytesAvailable();
        currentDevices.push_back(device);
        if (!m_knownDevicePaths.contains(path))
            emit deviceConnected(path, device["label"].toString());
    }
    for (const QString &oldPath : m_knownDevicePaths)
        if (!currentPaths.contains(oldPath)) emit deviceDisconnected(oldPath);
    if (currentPaths != m_knownDevicePaths || currentDevices.size() != m_externalDevices.size()) {
        m_externalDevices = currentDevices;
        emit externalDevicesChanged();
    }
    m_knownDevicePaths = currentPaths;
}

void Scanner::scanCommonLocations() {
    QStringList roots;
    const QString home = QDir::homePath();
#ifdef Q_OS_WIN
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString programFiles = qEnvironmentVariable("ProgramFiles", "C:/Program Files");
    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)", "C:/Program Files (x86)");
    roots << home + "/Games" << home + "/ROMs" << home + "/roms"
          << documents + "/Games" << documents + "/ROMs" << documents + "/Emulation"
          << programFiles + "/Steam/steamapps/common"
          << programFilesX86 + "/Steam/steamapps/common";
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady() && isExternalDevicePath(storage.rootPath()))
            roots << storage.rootPath();
    }
#else
    roots << home + "/Games" << home + "/ROMs" << home + "/roms"
          << home + "/Emulation" << home + "/emulation"
          << home + "/.local/share/Steam/steamapps/common"
          << home + "/.steam/steam/steamapps/common"
          << home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common"
          << home + "/.local/share/lutris/games"
          << home + "/.var/app/net.lutris.Lutris/data/lutris/games"
          << home + "/.var/app/com.heroicgameslauncher.hgl/config/heroic"
          << home + "/.var/app/com.usebottles.bottles/data/bottles";
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady() && !storage.rootPath().isEmpty()) {
            const QString root = storage.rootPath();
            if (root.startsWith("/run/media/") || root.startsWith("/media/")) {
                roots << root;
            }
        }
    }
#endif
    scanRoots(roots);
}

void Scanner::scanExternalLaunchers() {
    if (m_busy) return;
    if (m_romSearch) { m_romSearch = false; emit romSearchChanged(); }
    setBusy(true);
    m_candidates.clear();
    emit candidatesChanged();
#ifdef Q_OS_WIN
    {
    const QString home = QDir::homePath();
    const QString appData = qEnvironmentVariable("APPDATA", home + "/AppData/Roaming");
    const QString userProfile = qEnvironmentVariable("USERPROFILE", home);
    const QStringList heroicDirs = {
        appData + "/heroic",
        userProfile + "/.config/heroic",
        userProfile + "/.config/legendary"
    };
    QSet<QString> seen;
    int heroicCount = 0;
    for (const QString &heroicDir : heroicDirs) {
        if (!QDir(heroicDir).exists()) continue;
        QDirIterator it(heroicDir, {"installed.json"}, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString installedJson = it.next();
            QFile file(installedJson);
            if (!file.open(QIODevice::ReadOnly)) continue;
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError) continue;
            const QString lowerPath = installedJson.toLower();
            const QString store = lowerPath.contains("legendary") ? "Epic" :
                                  lowerPath.contains("gog") ? "GOG" :
                                  (lowerPath.contains("nile") || lowerPath.contains("amazon")) ? "Amazon" : "Heroic";
            const QString runner = store == "Epic" ? "legendary" :
                                   store == "GOG" ? "gog" :
                                   store == "Amazon" ? "nile" : QString();
            const int before = m_candidates.size();
            collectHeroicEntries(doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object()),
                                 store, runner, {}, false, &m_candidates, &seen);
            heroicCount += m_candidates.size() - before;
        }
    }

    // Epic's per-install .item manifests live under ProgramData by default.
    // Also inspect the launcher's install tree for portable/PCB-mode layouts.
    const QString programData = qEnvironmentVariable("ProgramData", "C:/ProgramData");
    const QString programFiles = qEnvironmentVariable("ProgramFiles", "C:/Program Files");
    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)", "C:/Program Files (x86)");
    const QStringList epicManifestRoots = {
        programData + "/Epic",
        programFilesX86 + "/Epic Games/Launcher",
        programFiles + "/Epic Games/Launcher"
    };
    QSet<QString> seenEpicManifests;
    QSet<QString> seenEpicApps;
    int epicCount = 0;
    for (const QString &manifestRoot : epicManifestRoots) {
        if (!QDir(manifestRoot).exists()) continue;
        QDirIterator it(manifestRoot, {"*.item"}, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString manifestPath = it.next();
            const QString manifestKey = QFileInfo(manifestPath).canonicalFilePath().toCaseFolded();
            if (!manifestKey.isEmpty() && seenEpicManifests.contains(manifestKey)) continue;
            if (!manifestKey.isEmpty()) seenEpicManifests.insert(manifestKey);
            QFile file(manifestPath);
            if (!file.open(QIODevice::ReadOnly)) continue;
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) continue;
            const QJsonObject item = document.object();
            const QString appName = item.value("AppName").toString().trimmed();
            const QString displayName = item.value("DisplayName").toString().trimmed();
            const QString installValue = item.value("InstallLocation").toString().trimmed();
            const QString launchExecutable = item.value("LaunchExecutable").toString().trimmed();
            if (appName.isEmpty() || installValue.isEmpty() || seenEpicApps.contains(appName)) continue;
            const QString installPath = QDir::cleanPath(QDir::fromNativeSeparators(installValue));
            if (!QFileInfo(installPath).isDir()) continue;
            seenEpicApps.insert(appName);
            const QString encodedInstallPath = QString::fromLatin1(
                QUrl::toPercentEncoding(QDir::fromNativeSeparators(installPath)));
            const QString epicUri = "com.epicgames.launcher://apps/" + encodedInstallPath +
                                    "?action=launch&silent=true";
            QVariantMap candidate;
            candidate["title"] = displayName.isEmpty() ? appName : displayName;
            candidate["path"] = installPath;
            candidate["platform"] = "PC / Epic";
            candidate["kind"] = "Epic Games Launcher — instalado";
            candidate["source"] = "Epic";
            candidate["appName"] = appName;
            candidate["launchExecutable"] = launchExecutable;
            candidate["manifest"] = manifestPath;
            const QString epicArtwork = existingArtworkPath(jsonValue(item, {
                "coverPath", "cover_path", "cover", "art_cover", "imagePath", "image_path", "iconPath", "icon_path"
            }));
            if (!epicArtwork.isEmpty()) candidate["coverPath"] = epicArtwork;
            candidate["launchCommand"] = epicUri;
            candidate["installed"] = true;
            candidate["importable"] = true;
            m_candidates.push_back(candidate);
            ++epicCount;
        }
    }

    // Ubisoft Connect's per-game registry key names are the IDs used by its
    // uplay://launch/<id>/0 protocol. Read both common 32/64-bit registry paths.
    const QStringList ubisoftRegistryRoots = {
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Ubisoft\\Launcher\\Installs",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Ubisoft\\Launcher\\Installs"
    };
    const QRegularExpression safeUbisoftId("^[0-9]{1,12}$");
    QSet<QString> seenUbisoftEntries;
    int ubisoftCount = 0;
    for (const QString &registryRoot : ubisoftRegistryRoots) {
        QSettings installs(registryRoot, QSettings::NativeFormat);
        for (const QString &gameId : installs.childGroups()) {
            if (!safeUbisoftId.match(gameId).hasMatch()) continue;
            installs.beginGroup(gameId);
            QString installPath = installs.value("InstallDir").toString().trimmed();
            if (installPath.isEmpty()) installPath = installs.value("InstallLocation").toString().trimmed();
            QString title = installs.value("DisplayName").toString().trimmed();
            if (title.isEmpty()) title = installs.value("GameName").toString().trimmed();
            const QString ubisoftArtwork = existingArtworkPath(installs.value("IconPath", installs.value("ImagePath")).toString());
            installs.endGroup();
            if (installPath.isEmpty()) continue;
            installPath = QDir::cleanPath(QDir::fromNativeSeparators(installPath));
            const QFileInfo installInfo(installPath);
            if (installInfo.isFile()) installPath = installInfo.absolutePath();
            if (!QFileInfo(installPath).isDir()) continue;
            const QString entryKey = gameId + QLatin1Char('|') + installPath.toCaseFolded();
            if (seenUbisoftEntries.contains(entryKey)) continue;
            seenUbisoftEntries.insert(entryKey);
            if (title.isEmpty()) {
                title = QFileInfo(installPath).fileName();
                title.replace('_', ' ');
            }
            QVariantMap candidate;
            candidate["title"] = title.isEmpty() ? "Jogo Ubisoft " + gameId : title;
            candidate["path"] = installPath;
            candidate["platform"] = "PC / Ubisoft";
            candidate["kind"] = "Ubisoft Connect — instalado";
            candidate["source"] = "Ubisoft";
            candidate["appId"] = gameId;
            if (!ubisoftArtwork.isEmpty()) candidate["coverPath"] = ubisoftArtwork;
            candidate["launchCommand"] = "uplay://launch/" + gameId + "/0";
            candidate["installed"] = true;
            candidate["importable"] = true;
            m_candidates.push_back(candidate);
            ++ubisoftCount;
        }
    }

    emit candidatesChanged();
    setStatus(QString("Bibliotecas Windows — Heroic: %1; Epic: %2; Ubisoft Connect: %3 jogo(s). Steam é escaneada separadamente.")
                  .arg(heroicCount).arg(epicCount).arg(ubisoftCount));
    setBusy(false);
    emit scanFinished(m_candidates.size());
    return;
    }
#else
    const QString home = QDir::homePath();
    QSet<QString> seen;

    // Lutris: read its own PGA SQLite library. Query columns by name so older/newer
    // schemas that add fields do not break discovery. Database is opened read-only.
    const QStringList lutrisDirs = {
        home + "/.local/share/lutris",
        home + "/.var/app/net.lutris.Lutris/data/lutris"
    };
    int lutrisCount = 0;
    for (const QString &lutrisDir : lutrisDirs) {
        const QString dbPath = lutrisDir + "/pga.db";
        if (!QFileInfo::exists(dbPath)) continue;
        const QString connection = QStringLiteral("lutris_%1_%2").arg(QCoreApplication::applicationPid()).arg(lutrisCount);
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(dbPath);
            db.setConnectOptions("QSQLITE_OPEN_READONLY");
            if (db.open()) {
                QSqlQuery query(db);
                if (query.exec("SELECT * FROM games")) {
                    const QSqlRecord record = query.record();
                    auto column = [&record](const QStringList &names) {
                        for (const QString &name : names) {
                            const int index = record.indexOf(name);
                            if (index >= 0) return index;
                        }
                        return -1;
                    };
                    const int nameCol = column({"name", "title"});
                    const int slugCol = column({"slug", "game_slug"});
                    const int runnerCol = column({"runner"});
                    const int installedCol = column({"installed"});
                    const int directoryCol = column({"directory", "install_dir"});
                    const int idCol = column({"id"});
                    const int configCol = column({"configpath", "config_path", "config_id"});
                    while (query.next()) {
                        const auto value = [&query](int index) { return index >= 0 ? query.value(index).toString() : QString(); };
                        const QString title = value(nameCol).trimmed();
                        const QString slug = value(slugCol).trimmed();
                        const QString runner = value(runnerCol).trimmed();
                        if (title.isEmpty() || slug.isEmpty()) continue;
                        const QString installedValue = value(installedCol).toLower();
                        const bool installed = installedCol < 0 || installedValue == "1" || installedValue == "true" || installedValue == "yes";
                        if (!installed) continue;
                        QString configPath = value(configCol);
                        if (configPath.isEmpty() || !QFileInfo::exists(configPath))
                            configPath = lutrisDir + "/games/" + slug + ".yml";
                        QString exe, prefix, workingDir;
                        QFile config(configPath);
                        if (config.open(QIODevice::ReadOnly | QIODevice::Text)) {
                            const QString yaml = QString::fromUtf8(config.readAll());
                            exe = yamlScalar(yaml, {"exe", "executable"});
                            prefix = yamlScalar(yaml, {"prefix"});
                            workingDir = yamlScalar(yaml, {"working_dir", "working-directory"});
                        }
                        const QString installDir = value(directoryCol);
                        QString gamePath = !installDir.isEmpty() ? installDir : workingDir;
                        if (gamePath.isEmpty() && !exe.isEmpty()) gamePath = QFileInfo(exe).absolutePath();
                        if (gamePath.isEmpty()) gamePath = configPath;
                        gamePath = QDir::cleanPath(gamePath);
                        const QString key = QStringLiteral("Lutris|") + slug;
                        if (seen.contains(key)) continue;
                        seen.insert(key);
                        QVariantMap c;
                        c["title"] = title;
                        c["path"] = gamePath;
                        c["platform"] = "PC / Lutris";
                        c["kind"] = runner.isEmpty() ? "Lutris — instalado" : "Lutris — " + runner;
                        c["source"] = "Lutris";
                        c["runner"] = runner;
                        c["slug"] = slug;
                        c["lutrisId"] = value(idCol);
                        c["compatPrefix"] = prefix;
                        c["executable"] = exe;
                        c["installed"] = true;
                        // Accept only a conservative slug alphabet before passing the official URI
                        // as one launcher argument; the database is local but still user-controlled.
                        const bool safeSlug = QRegularExpression("^[A-Za-z0-9._-]+$").match(slug).hasMatch();
                        c["launchCommand"] = safeSlug
                            ? (lutrisDir.contains("/.var/app/")
                                   ? "flatpak run net.lutris.Lutris lutris:" + slug
                                   : "lutris lutris:" + slug)
                            : QString();
                        c["importable"] = safeSlug;
                        m_candidates.push_back(c);
                        ++lutrisCount;
                    }
                }
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connection);
    }

    // Heroic: read installed-game JSON only from the documented native and Flatpak
    // configuration trees. Never interpret commands or execute content from JSON.
    const QStringList heroicDirs = {
        home + "/.config/heroic", home + "/.config/legendary",
        home + "/.var/app/com.heroicgameslauncher.hgl/config/heroic",
        home + "/.var/app/com.heroicgameslauncher.hgl/config/legendary"
    };
    int heroicCount = 0;
    for (const QString &heroicDir : heroicDirs) {
        if (!QDir(heroicDir).exists()) continue;
        QDirIterator it(heroicDir, {"installed.json"}, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString installedJson = it.next();
            QFile file(installedJson);
            if (!file.open(QIODevice::ReadOnly)) continue;
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError) continue;
            const QString lowerPath = installedJson.toLower();
            const QString store = lowerPath.contains("legendary") ? "Epic" :
                                  lowerPath.contains("gog") ? "GOG" :
                                  (lowerPath.contains("nile") || lowerPath.contains("amazon")) ? "Amazon" : "Heroic";
            const QString runner = store == "Epic" ? "legendary" :
                                   store == "GOG" ? "gog" :
                                   store == "Amazon" ? "nile" : QString();
            const bool flatpak = heroicDir.contains("/.var/app/");
            const int before = m_candidates.size();
            collectHeroicEntries(doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object()),
                                 store, runner, {}, flatpak, &m_candidates, &seen);
            heroicCount += m_candidates.size() - before;
        }
    }

    // Bottles: ask its documented CLI for registered programs, then translate the
    // returned Windows path to the local bottle drive so stale entries are excluded.
    const QStringList bottlesRoots = {
        home + "/.local/share/bottles/bottles",
        home + "/.var/app/com.usebottles.bottles/data/bottles/bottles"
    };
    int bottleCount = 0;
    for (const QString &root : bottlesRoots) {
        QDir bottles(root);
        if (!bottles.exists()) continue;
        const bool flatpak = root.contains("/.var/app/");
        const QString flatpakExecutable = QStandardPaths::findExecutable("flatpak");
        const QString bottlesExecutable = QStandardPaths::findExecutable("bottles-cli");
        if ((flatpak && flatpakExecutable.isEmpty()) || (!flatpak && bottlesExecutable.isEmpty())) continue;
        const QStringList names = bottles.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &bottleName : names) {
            const QString bottlePath = bottles.filePath(bottleName);
            QString runner;
            QFile bottleConfig(bottlePath + "/bottle.yml");
            if (bottleConfig.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QString yaml = QString::fromUtf8(bottleConfig.readAll());
                runner = yamlScalar(yaml, {"Runner", "runner"});
            }
            const QStringList cliPrefix = flatpak
                ? QStringList{"run", "--command=bottles-cli", "com.usebottles.bottles", "--json", "programs", "-b", bottleName}
                : QStringList{"--json", "programs", "-b", bottleName};
            const QByteArray output = runReadOnlyCommand(flatpak ? flatpakExecutable : bottlesExecutable, cliPrefix);
            QJsonParseError parseError;
            const QJsonDocument programsDoc = QJsonDocument::fromJson(output, &parseError);
            if (parseError.error != QJsonParseError::NoError) continue;
            QJsonArray programRecords;
            if (programsDoc.isArray()) programRecords = programsDoc.array();
            else if (programsDoc.isObject() && programsDoc.object().value("programs").isArray())
                programRecords = programsDoc.object().value("programs").toArray();
            for (const QJsonValue &programValue : programRecords) {
                if (!programValue.isObject()) continue;
                const QJsonObject programRecord = programValue.toObject();
                const QString title = jsonValue(programRecord, {"name", "title"}).trimmed();
                const QString windowsPath = jsonValue(programRecord, {"path", "executable"}).trimmed();
                if (title.isEmpty() || windowsPath.isEmpty() || programRecord.value("removed").toBool()) continue;
                QString resolvedExe = windowsPath;
                const QRegularExpression drivePath("^([A-Za-z]):[/\\\\](.*)$");
                const auto driveMatch = drivePath.match(windowsPath);
                if (driveMatch.hasMatch()) {
                    const QString drive = driveMatch.captured(1).toLower();
                    const QString relative = driveMatch.captured(2).replace(QChar(92), QChar('/'));
                    const QString driveRoot = drive == "c"
                        ? bottlePath + "/drive_c"
                        : bottlePath + "/dosdevices/" + drive + ":";
                    resolvedExe = QDir(driveRoot).filePath(relative);
                }
                resolvedExe = QDir::cleanPath(resolvedExe);
                const bool executableExists = QFileInfo(resolvedExe).isFile();
                const QString key = "Bottles|" + bottleName + "|" + title;
                if (seen.contains(key)) continue;
                seen.insert(key);
                QVariantMap c;
                c["title"] = title;
                c["path"] = resolvedExe;
                c["platform"] = "PC / Bottles";
                c["kind"] = runner.isEmpty() ? "Bottles — " + bottleName : "Bottles — " + runner;
                c["source"] = "Bottles";
                c["runner"] = runner;
                c["compatPrefix"] = bottlePath;
                c["installed"] = executableExists;
                const QStringList launchArgs = flatpak
                    ? QStringList{"flatpak", "run", "--command=bottles-cli", "com.usebottles.bottles", "run", "-b", bottleName, "-p", title}
                    : QStringList{"bottles-cli", "run", "-b", bottleName, "-p", title};
                c["launchCommand"] = executableExists ? encodeArguments(launchArgs) : QString();
                c["importable"] = executableExists;
                m_candidates.push_back(c);
                ++bottleCount;
            }
        }
    }
    emit candidatesChanged();
    setStatus(QString("Bibliotecas — Lutris: %1; Heroic: %2; Bottles: %3 programa(s). Revise antes de importar.")
                  .arg(lutrisCount).arg(heroicCount).arg(bottleCount));
    setBusy(false);
    emit scanFinished(m_candidates.size());
#endif
}


void Scanner::scanSteamLibraries() {
    if (m_busy) return;
    if (m_romSearch) { m_romSearch = false; emit romSearchChanged(); }
    setBusy(true);
    m_candidates.clear();
    emit candidatesChanged();
    QStringList steamApps;
    const QString home = QDir::homePath();
#ifdef Q_OS_WIN
    QStringList steamRoots;
    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)", "C:/Program Files (x86)");
    steamRoots << programFilesX86 + "/Steam"
               << qEnvironmentVariable("ProgramFiles", "C:/Program Files") + "/Steam"
               << qEnvironmentVariable("LOCALAPPDATA") + "/Steam";
    const QStringList registryKeys = {
        "HKEY_CURRENT_USER\\Software\\Valve\\Steam",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam"
    };
    for (const QString &key : registryKeys) {
        QSettings registry(key, QSettings::NativeFormat);
        const QString registeredPath = registry.value("SteamPath", registry.value("InstallPath")).toString();
        if (!registeredPath.isEmpty()) steamRoots << QDir::fromNativeSeparators(registeredPath);
    }
#else
    const QStringList steamRoots = {
        home + "/.local/share/Steam", home + "/.steam/steam",
        home + "/.steam/root", home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam",
        home + "/.var/app/com.valvesoftware.Steam/.steam/steam",
        "/var/lib/flatpak/app/com.valvesoftware.Steam/current/active/files/steam"
    };
#endif
    for (const QString &root : steamRoots) {
        if (QDir(root).exists()) steamApps << QDir(root).filePath("steamapps");
    }
    // Collect secondary Steam libraries declared by each discovered Steam root.
    QStringList roots = steamApps;
    QSet<QString> seenApps;
    for (int i = 0; i < roots.size(); ++i) {
        const QString apps = roots.at(i);
        const QString canonical = QFileInfo(apps).canonicalFilePath();
        const QString normalized = canonical.isEmpty() ? QDir::cleanPath(apps) : canonical;
        if (seenApps.contains(normalized)) continue;
        seenApps.insert(normalized);
        QFile vf(apps + "/libraryfolders.vdf");
        if (!vf.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        const QString content = QString::fromUtf8(vf.readAll());
        QRegularExpression re("\\\"path\\\"\\s+\\\"([^\\\"]+)\\\"");
        auto it = re.globalMatch(content);
        while (it.hasNext()) {
            QString lib = it.next().captured(1);
#ifdef Q_OS_WIN
            lib.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
#endif
            const QString discovered = QDir::cleanPath(lib + "/steamapps");
            if (!roots.contains(discovered)) roots << discovered;
        }
    }
    QSet<QString> manifests;
    for (const QString &apps : roots) {
        QDir d(apps);
        if (!d.exists()) continue;
        const QStringList files = d.entryList({"appmanifest_*.acf"}, QDir::Files);
        for (const QString &fileName : files) {
            const QString manifestPath = d.filePath(fileName);
            if (manifests.contains(manifestPath)) continue;
            manifests.insert(manifestPath);
            QFile f(manifestPath);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QString data = QString::fromUtf8(f.readAll());
            auto capture = [&data](const QString &key) -> QString {
                QRegularExpression rx("\\\"" + key + "\\\"\\s+\\\"([^\\\"]*)\\\"", QRegularExpression::CaseInsensitiveOption);
                auto m = rx.match(data); return m.hasMatch() ? m.captured(1) : QString();
            };
            const QString appId = capture("appid");
            const QString name = capture("name");
            const QString installDir = capture("installdir");
            if (appId.isEmpty() || name.isEmpty() || installDir.isEmpty()) continue;
            const QString gamePath = QDir::cleanPath(d.filePath("common/" + installDir));
            const QString osList = capture("oslist").toLower();
            const QString compatPrefix = QDir::cleanPath(d.filePath("compatdata/" + appId + "/pfx"));
#ifdef Q_OS_WIN
            const bool proton = false;
#else
            const bool proton = osList.contains("windows") || QDir(compatPrefix).exists();
#endif
            const bool installed = QFileInfo(gamePath).isDir();
            QVariantMap c;
            c["title"] = name;
            c["path"] = gamePath;
#ifdef Q_OS_WIN
            c["platform"] = "PC / Steam";
            c["kind"] = "Steam para Windows";
#else
            c["platform"] = proton ? "PC / Steam (Proton)" : "PC / Steam (Linux)";
            c["kind"] = proton ? "Steam / Proton" : "Steam / Linux nativo";
#endif
            c["source"] = "Steam";
            c["launchCommand"] = "steam://rungameid/" + appId;
            c["appId"] = appId;
            c["library"] = d.absolutePath();
            const QString steamArtwork = findSteamArtwork(appId, steamRoots, roots);
            if (!steamArtwork.isEmpty()) c["coverPath"] = steamArtwork;
            c["compatPrefix"] = QDir(compatPrefix).exists() ? compatPrefix : QString();
            c["installed"] = installed;
#ifdef Q_OS_WIN
            c["launcher"] = "Steam para Windows";
#else
            c["launcher"] = apps.startsWith(home + "/.var/app/") ? "Steam Flatpak" : "Steam nativa";
#endif
            m_candidates.push_back(c);
        }
    }
    emit candidatesChanged();
    setStatus(QString("Steam: %1 manifestos encontrados. Revise antes de importar.").arg(m_candidates.size()));
    setBusy(false);
    emit scanFinished(m_candidates.size());
}

void Scanner::scanDirectory(const QString &path) {
    if (!QDir(path).exists()) { setStatus("Pasta não encontrada: " + path); return; }
    scanRoots({path});
}

void Scanner::scanRomsDirectory(const QString &path) {
    if (!QDir(path).exists()) { setStatus("Pasta de ROMs não encontrada: " + path); return; }
    scanRoots({path}, true);
}

void Scanner::scanRoots(const QStringList &roots, bool romMode) {
    if (m_busy) return;
    if (m_romSearch != romMode) {
        m_romSearch = romMode;
        emit romSearchChanged();
    }
    setBusy(true);
    m_candidates.clear();
    emit candidatesChanged();
    QSet<QString> seen;
    const QStringList patterns = {"*.rom","*.nes","*.sfc","*.smc","*.gb","*.gbc","*.gba","*.nds",
                                  "*.iso","*.chd","*.cue","*.cdi","*.gdi","*.z64","*.n64","*.v64",
                                  "*.md","*.gen","*.exe","*.3ds","*.cia","*.pbp","*.cso","*.ecm",
                                  "*.wbfs","*.wud","*.wux","*.nsp","*.xci","*.sms","*.gg",
                                  "*.zip","*.7z","*.rar","*.AppImage","*.sh"};
    const QSet<QString> romExtensions = {"rom","nes","sfc","smc","gb","gbc","gba","nds",
        "iso","chd","cue","cdi","gdi","z64","n64","v64","md","gen","3ds","cia",
        "pbp","cso","ecm","wbfs","wud","wux","nsp","xci","sms","gg","zip","7z","rar"};
    const QSet<QString> supportExtensions = {"bin","bios","key","dat","rom","firmware"};
    const QSet<QString> knownFirmware = {"scph5500.bin","scph5501.bin","scph5502.bin","scph1001.bin",
        "scph10000.bin","scph39001.bin","psx.bin","ps1.bin","gba_bios.bin","gb_bios.bin",
        "dmg_boot.bin","cgb_boot.bin","bios7.bin","bios9.bin","firmware.bin","bios_cd_u.bin",
        "bios_cd_e.bin","bios_cd_j.bin","dc_boot.bin","dc_flash.bin","dc_bios.bin",
        "sega_101.bin","mpr-17933.bin","saturn_bios.bin"};
    int count = 0;
    int supportCount = 0;
    const int maxCandidates = 1500;
    for (const QString &root : roots) {
        QDir dir(root);
        if (!dir.exists()) continue;
        const QString canonical = dir.canonicalPath();
        if (canonical.isEmpty() || seen.contains(canonical)) continue;
        seen.insert(canonical);
        setStatus("Procurando em " + canonical);
        QDirIterator it(canonical, romMode ? QStringList{"*"} : patterns,
                        QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext() && m_candidates.size() < maxCandidates) {
            const QString file = it.next();
            QFileInfo fi(file);
            if (!fi.isReadable()) continue;
            const QString absPath = fi.absoluteFilePath();
            if (seen.contains(absPath)) continue;
            seen.insert(absPath);
            const QString ext = fi.suffix().toLower();
            const QString fileName = fi.fileName().toLower();
            const QStringList pathParts = QDir::fromNativeSeparators(absPath).toLower().split('/', Qt::SkipEmptyParts);
            const bool inFirmwareFolder = pathParts.contains("bios") || pathParts.contains("firmware") ||
                                          pathParts.contains("system");
            const bool isKnownFirmware = knownFirmware.contains(fileName);
            const bool isSupportFile = romMode && (isKnownFirmware ||
                (inFirmwareFolder && supportExtensions.contains(ext)));
            bool isRom = romExtensions.contains(ext);
            if (romMode && ext == "bin") {
                const QDir parentDir(fi.absolutePath());
                const bool hasCue = QFileInfo(parentDir.filePath(fi.completeBaseName() + ".cue")).isFile() ||
                                    QFileInfo(parentDir.filePath(fi.completeBaseName() + ".CUE")).isFile();
                if (hasCue && !isSupportFile) continue;
                isRom = false;
            }
            if (romMode && !isSupportFile && !isRom) continue;

            QString platform = "PC";
            if (ext == "nes") platform = "NES";
            else if (ext == "sfc" || ext == "smc") platform = "SNES";
            else if (ext == "gb" || ext == "gbc") platform = "Game Boy";
            else if (ext == "gba") platform = "Game Boy Advance";
            else if (ext == "nds") platform = "Nintendo DS";
            else if (ext == "md" || ext == "gen") platform = "Mega Drive";
            else if (ext == "z64" || ext == "n64" || ext == "v64") platform = "Nintendo 64";
            else if (ext == "3ds" || ext == "cia") platform = "Nintendo 3DS";
            else if (ext == "pbp" || ext == "ecm") platform = "PlayStation";
            else if (ext == "cso") platform = "PSP";
            else if (ext == "gdi" || ext == "cdi") platform = "Dreamcast";
            else if (ext == "wbfs") platform = "Wii";
            else if (ext == "wud" || ext == "wux") platform = "Wii U";
            else if (ext == "nsp" || ext == "xci") platform = "Nintendo Switch";
            else if (ext == "sms") platform = "Master System";
            else if (ext == "gg") platform = "Game Gear";
            else if (ext == "zip" || ext == "7z" || ext == "rar") platform = "ROM compactada";
            else if (QStringList{"iso","chd","cue"}.contains(ext)) {
                const QString lowerPath = QDir::fromNativeSeparators(absPath).toLower();
                if (lowerPath.contains("ps1") || lowerPath.contains("psx") || lowerPath.contains("playstation")) platform = "PlayStation";
                else if (lowerPath.contains("ps2") || lowerPath.contains("playstation 2")) platform = "PlayStation 2";
                else platform = "Disco/Imagem";
            }
            QVariantMap candidate;
            candidate["title"] = fi.completeBaseName();
            candidate["path"] = absPath;
            candidate["platform"] = isSupportFile ? "BIOS / Firmware" : platform;
            candidate["kind"] = isSupportFile ? "BIOS/firmware detectado — não é um jogo" :
                (QStringList{"exe","AppImage","sh"}.contains(ext) ? "Executável" :
                 (QStringList{"zip","7z","rar"}.contains(ext) ? "ROM compactada — compatibilidade depende do emulador" : "ROM/Imagem"));
            candidate["source"] = romMode ? "romsearch" : "scanner";
            candidate["launchCommand"] = "";
            candidate["sizeBytes"] = fi.size();
            candidate["importable"] = !isSupportFile && (romMode ? isRom : true);
            candidate["requiresExtractor"] = QStringList{"zip","7z","rar"}.contains(ext);
            m_candidates.push_back(candidate);
            if (isSupportFile) ++supportCount;
            else ++count;
        }
        if (m_candidates.size() >= maxCandidates) break;
    }
    emit candidatesChanged();
    if (romMode)
        setStatus(QString("Busca local concluída: %1 ROM(s)/imagem(ns) e %2 arquivo(s) de BIOS/firmware. Nenhum conteúdo foi baixado.").arg(count).arg(supportCount));
    else
        setStatus(QString("Busca concluída: %1 candidatos. Revise a lista antes de importar.").arg(count));
    setBusy(false);
    emit scanFinished(count);
}
