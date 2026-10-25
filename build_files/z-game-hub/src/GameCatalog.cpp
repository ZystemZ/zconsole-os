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
#include <QStandardPaths>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QJsonValue>

GameCatalog::GameCatalog(QObject *parent) : QObject(parent) {}

bool GameCatalog::initialize(const QSqlDatabase &db) {
    m_db = db;
    QSqlQuery q(m_db);
    const QString gamesSchema = R"SQL(
        CREATE TABLE IF NOT EXISTS games (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            path TEXT NOT NULL DEFAULT '',
            platform TEXT NOT NULL DEFAULT 'PC',
            source TEXT NOT NULL DEFAULT 'manual',
            launch_command TEXT NOT NULL DEFAULT '',
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
    q.prepare("SELECT id,title,path,platform,source,launch_command,available FROM games ORDER BY title COLLATE NOCASE");
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
    const QStringList candidates = {"retroarch", "retroarch-sdl2", "retroarch-wayland"};
    for (const QString &candidate : candidates) {
        const QString path = QStandardPaths::findExecutable(candidate);
        if (!path.isEmpty()) return path;
    }
    if (!QStandardPaths::findExecutable("flatpak").isEmpty())
        return "flatpak run org.libretro.RetroArch";
    return {};
}

QStringList GameCatalog::detectRetroArchCores() const {
    QStringList roots;
    const QString home = QDir::homePath();
    roots << home + "/.config/retroarch/cores"
          << home + "/.var/app/org.libretro.RetroArch/config/retroarch/cores"
          << home + "/.var/app/org.libretro.RetroArch/data/retroarch/cores"
          << "/usr/lib/libretro" << "/usr/lib64/libretro" << "/app/lib/retroarch/cores";

    QStringList cores;
    QSet<QString> seen;
    for (const QString &root : roots) {
        QDir dir(root);
        if (!dir.exists()) continue;
        QDirIterator it(root, {"*_libretro.so"}, QDir::Files | QDir::Readable,
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
    else if (normalized.contains("playstation")) tokens = {"swanstation", "beetle_psx", "pcsx_rearmed"};
    else if (normalized.contains("game boy advance")) tokens = {"mgba", "vba_next"};
    else if (normalized.contains("game boy")) tokens = {"gambatte", "mgba", "tgbdual"};
    else if (normalized.contains("nintendo ds")) tokens = {"desmume", "melonds"};
    else if (normalized.contains("mega drive")) tokens = {"genesis_plus_gx", "picodrive"};
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
                                const QString &platform, const QString &launchCommand) {
    if (title.trimmed().isEmpty()) {
        emit errorOccurred("Informe o nome do jogo.");
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(R"SQL(INSERT INTO games(title,path,platform,source,launch_command,available)
                    VALUES(?,?,?,'manual',?,1)
                    ON CONFLICT(path,source) DO UPDATE SET
                    title=excluded.title, platform=excluded.platform,
                    launch_command=excluded.launch_command, available=1)SQL");
    q.addBindValue(title.trimmed());
    q.addBindValue(QDir::cleanPath(path.trimmed()));
    q.addBindValue(platform.trimmed().isEmpty() ? "PC" : platform.trimmed());
    q.addBindValue(launchCommand.trimmed());
    if (!q.exec()) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    return loadGames();
}

bool GameCatalog::importCandidate(const QString &title, const QString &path,
                                  const QString &platform, const QString &source,
                                  const QString &launchCommand) {
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
    return loadGames();
}

bool GameCatalog::updateManualGame(qint64 id, const QString &title, const QString &path,
                                   const QString &platform, const QString &launchCommand) {
    if (title.trimmed().isEmpty()) {
        emit errorOccurred("Informe o nome do jogo.");
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(R"SQL(UPDATE games SET title=?, path=?, platform=?, launch_command=?, available=1
                    WHERE id=? AND source='manual')SQL");
    q.addBindValue(title.trimmed());
    q.addBindValue(QDir::cleanPath(path.trimmed()));
    q.addBindValue(platform.trimmed().isEmpty() ? "PC" : platform.trimmed());
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
    return loadGames();
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

bool GameCatalog::removeGame(qint64 id) {
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM games WHERE id=? AND source='manual'");
    q.addBindValue(id);
    if (!q.exec()) {
        emit errorOccurred(q.lastError().text());
        return false;
    }
    return loadGames();
}

void GameCatalog::refresh() { loadGames(); }
