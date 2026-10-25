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
            const QString encodedApp = QString::fromLatin1(QUrl::toPercentEncoding(appName));
            const QString heroicUri = "heroic://launch?appName=" + encodedApp + "&runner=" + resolvedRunner;
            c["launchCommand"] = !resolvedRunner.isEmpty() ? (flatpak
                ? "flatpak run com.heroicgameslauncher.hgl --no-gui " + heroicUri
                : "heroic --no-gui " + heroicUri) : QString();
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
void Scanner::removeCandidate(int index) { if (index >= 0 && index < m_candidates.size()) { m_candidates.removeAt(index); emit candidatesChanged(); } }
void Scanner::setBusy(bool b) { if (m_busy != b) { m_busy=b; emit busyChanged(); } }
void Scanner::setStatus(const QString &s) { m_status=s; emit statusChanged(); }

bool Scanner::isExternalDevicePath(const QString &path) const {
    return path.startsWith("/run/media/") || path.startsWith("/media/") || path.startsWith("/mnt/");
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
    scanRoots(roots);
}

void Scanner::scanExternalLaunchers() {
    if (m_busy) return;
    setBusy(true);
    m_candidates.clear();
    emit candidatesChanged();
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
}


void Scanner::scanSteamLibraries() {
    if (m_busy) return;
    setBusy(true);
    m_candidates.clear();
    emit candidatesChanged();
    QStringList steamApps;
    const QString home = QDir::homePath();
    const QStringList steamRoots = {
        home + "/.local/share/Steam", home + "/.steam/steam",
        home + "/.steam/root", home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam",
        home + "/.var/app/com.valvesoftware.Steam/.steam/steam",
        "/var/lib/flatpak/app/com.valvesoftware.Steam/current/active/files/steam"
    };
    for (const QString &root : steamRoots) {
        if (QDir(root).exists()) steamApps << QDir(root).filePath("steamapps");
    }
    // Collect secondary libraries from every discovered native/Flatpak Steam root.
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
            const QString lib = it.next().captured(1);
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
            const bool proton = osList.contains("windows") || QDir(compatPrefix).exists();
            const bool installed = QFileInfo(gamePath).isDir();
            QVariantMap c;
            c["title"] = name;
            c["path"] = gamePath;
            c["platform"] = proton ? "PC / Steam (Proton)" : "PC / Steam (Linux)";
            c["kind"] = proton ? "Steam / Proton" : "Steam / Linux nativo";
            c["source"] = "Steam";
            c["launchCommand"] = "steam://rungameid/" + appId;
            c["appId"] = appId;
            c["library"] = d.absolutePath();
            c["compatPrefix"] = QDir(compatPrefix).exists() ? compatPrefix : QString();
            c["installed"] = installed;
            c["launcher"] = apps.startsWith(home + "/.var/app/") ? "Steam Flatpak" : "Steam nativa";
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

void Scanner::scanRoots(const QStringList &roots) {
    if (m_busy) return;
    setBusy(true);
    m_candidates.clear();
    emit candidatesChanged();
    QSet<QString> seen;
    const QStringList patterns = {"*.rom","*.nes","*.sfc","*.smc","*.gb","*.gbc","*.gba","*.nds",
                                  "*.iso","*.chd","*.cue","*.cdi","*.gdi","*.z64","*.n64","*.v64",
                                  "*.md","*.gen","*.exe","*.AppImage","*.sh"};
    int count = 0;
    const int maxCandidates = 1500;
    for (const QString &root : roots) {
        QDir dir(root);
        if (!dir.exists()) continue;
        const QString canonical = dir.canonicalPath();
        if (canonical.isEmpty() || seen.contains(canonical)) continue;
        seen.insert(canonical);
        setStatus("Procurando em " + canonical);
        QDirIterator it(canonical, patterns, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext() && m_candidates.size() < maxCandidates) {
            const QString file = it.next();
            QFileInfo fi(file);
            if (!fi.isReadable()) continue;
            const QString absPath = fi.absoluteFilePath();
            if (seen.contains(absPath)) continue;
            seen.insert(absPath);
            const QString ext = fi.suffix().toLower();
            QString platform = "PC";
            if (QStringList{"nes"}.contains(ext)) platform = "NES";
            else if (QStringList{"sfc","smc"}.contains(ext)) platform = "SNES";
            else if (QStringList{"gb","gbc"}.contains(ext)) platform = "Game Boy";
            else if (ext == "gba") platform = "Game Boy Advance";
            else if (ext == "nds") platform = "Nintendo DS";
            else if (QStringList{"md","gen"}.contains(ext)) platform = "Mega Drive";
            else if (QStringList{"z64","n64","v64"}.contains(ext)) platform = "Nintendo 64";
            else if (QStringList{"iso","chd","cue","cdi","gdi"}.contains(ext)) platform = "Disco/Imagem";
            QVariantMap candidate;
            candidate["title"] = fi.completeBaseName();
            candidate["path"] = absPath;
            candidate["platform"] = platform;
            candidate["kind"] = (ext == "exe" || ext == "AppImage" || ext == "sh") ? "Executável" : "ROM/Imagem";
            candidate["source"] = "scanner";
            candidate["launchCommand"] = "";
            m_candidates.push_back(candidate);
            ++count;
        }
        if (m_candidates.size() >= maxCandidates) break;
    }
    emit candidatesChanged();
    setStatus(QString("Busca concluída: %1 candidatos. Revise a lista antes de importar.").arg(count));
    setBusy(false);
    emit scanFinished(count);
}
