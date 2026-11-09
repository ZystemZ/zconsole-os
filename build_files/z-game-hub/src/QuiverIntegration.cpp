#include "QuiverIntegration.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QRegularExpression>
#include <QUuid>

QuiverIntegration::QuiverIntegration(QObject *parent)
    : QObject(parent), m_network() {
    m_dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    m_catalogRoot = QDir(m_dataRoot).filePath("quiver-community-catalog");
    QDir().mkpath(m_catalogRoot);
    QDir().mkpath(appsDirectory());
    loadCachedCatalog();
}

QVariantList QuiverIntegration::apps() const { return m_apps; }
QString QuiverIntegration::catalogStatus() const { return m_catalogStatus; }
bool QuiverIntegration::catalogBusy() const { return m_catalogBusy; }
QString QuiverIntegration::operationStatus() const { return m_operationStatus; }
bool QuiverIntegration::operationBusy() const { return m_operationBusy; }
QVariantList QuiverIntegration::releaseAssets() const { return m_releaseAssets; }
QVariantMap QuiverIntegration::pendingApp() const { return m_pendingApp; }
QVariantList QuiverIntegration::installedExecutables() const { return m_installedExecutables; }

QString QuiverIntegration::appsDirectory() const {
    return QDir(m_dataRoot).filePath("QuiverApps");
}

QNetworkRequest QuiverIntegration::makeRequest(const QUrl &url) const {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Z-GAME-HUB/1.3 Quiver integration");
    request.setRawHeader("Accept", "application/vnd.github+json, application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QString QuiverIntegration::cacheFileName(const QString &url) const {
    const QString name = QFileInfo(QUrl(url).path()).fileName();
    if (name.isEmpty() || name == "." || name == "..") return {};
    return QDir(m_catalogRoot).filePath(name);
}

void QuiverIntegration::setCatalogBusy(bool busy) {
    if (m_catalogBusy == busy) return;
    m_catalogBusy = busy;
    emit catalogBusyChanged();
}

void QuiverIntegration::setOperationBusy(bool busy) {
    if (m_operationBusy == busy) return;
    m_operationBusy = busy;
    emit operationBusyChanged();
}

void QuiverIntegration::setCatalogStatus(const QString &status) {
    if (m_catalogStatus == status) return;
    m_catalogStatus = status;
    emit catalogStatusChanged();
}

void QuiverIntegration::setOperationStatus(const QString &status) {
    if (m_operationStatus == status) return;
    m_operationStatus = status;
    emit operationStatusChanged();
}

void QuiverIntegration::setPendingApp(const QVariantMap &app) {
    m_pendingApp = app;
    emit pendingAppChanged();
}

bool QuiverIntegration::loadCatalogFile(const QString &filePath, const QString &listName, bool replace) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return false;
    const QJsonArray entries = document.object().value("apps").toArray();
    if (entries.isEmpty()) return false;

    QVariantList parsed;
    for (const QJsonValue &value : entries) {
        if (!value.isObject()) continue;
        const QJsonObject object = value.toObject();
        const QString name = object.value("name").toString().trimmed();
        if (name.isEmpty()) continue;
        QVariantMap app;
        app.insert("name", name);
        app.insert("project", object.value("project").toString().trimmed());
        app.insert("repository", object.value("repository").toString().trimmed());
        app.insert("repositorySource", object.value("repositorySource").toString().trimmed().toLower() == "gitlab" ? "gitlab" : "github");
        app.insert("folderName", object.value("folderName").toString().trimmed());
        app.insert("releaseAssetFilter", object.value("releaseAssetFilter").toString().trimmed());
        app.insert("appIconUrl", object.value("appIconUrl").toString().trimmed());
        app.insert("customDisplayName", object.value("customDisplayName").toString().trimmed());
        app.insert("catalogName", document.object().value("name").toString(listName.isEmpty() ? "Quiver" : listName));
        app.insert("description", object.value("description").toString());
        QStringList tags;
        for (const QJsonValue &tag : object.value("tags").toArray()) {
            const QString text = tag.toString().trimmed();
            if (!text.isEmpty()) tags << text;
        }
        app.insert("tags", tags);
        QStringList filesToAdd;
        for (const QJsonValue &entry : object.value("filesToAdd").toArray()) {
            const QString text = entry.toString().trimmed();
            if (!text.isEmpty()) filesToAdd << text;
        }
        app.insert("filesToAdd", filesToAdd);
        parsed << app;
    }
    if (parsed.isEmpty()) return false;

    if (replace) m_apps.clear();
    for (const QVariant &entry : parsed) {
        const QVariantMap app = entry.toMap();
        const QString key = app.value("repositorySource").toString() + ":" +
                            app.value("repository").toString().toLower() + ":" +
                            app.value("folderName").toString().toLower();
        bool found = false;
        for (int i = 0; i < m_apps.size(); ++i) {
            const QVariantMap old = m_apps.at(i).toMap();
            const QString oldKey = old.value("repositorySource").toString() + ":" +
                                   old.value("repository").toString().toLower() + ":" +
                                   old.value("folderName").toString().toLower();
            if (key == oldKey) { m_apps[i] = app; found = true; break; }
        }
        if (!found) m_apps << app;
    }
    emit appsChanged();
    return true;
}

void QuiverIntegration::loadCachedCatalog() {
    QDir dir(m_catalogRoot);
    const QStringList files = dir.entryList({"*.json"}, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        if (file.compare("index.json", Qt::CaseInsensitive) == 0) continue;
        loadCatalogFile(dir.filePath(file), QFileInfo(file).completeBaseName(), false);
    }
    if (!m_apps.isEmpty())
        setCatalogStatus(QString("%1 apps do catálogo comunitário em cache; conecte-se para atualizar.").arg(m_apps.size()));
    else
        setCatalogStatus("Catálogo Quiver ainda não carregado. Atualize para buscar as listas comunitárias.");
}

void QuiverIntegration::refreshCatalog() {
    if (m_catalogBusy) return;
    setCatalogBusy(true);
    m_apps.clear();
    emit appsChanged();
    m_catalogFiles.clear();
    m_catalogFileIndex = 0;
    setCatalogStatus("Buscando o índice oficial do catálogo comunitário Quiver…");
    requestCatalogIndex();
}

void QuiverIntegration::requestCatalogIndex() {
    const QUrl url("https://raw.githubusercontent.com/tgeorgiadis/quiver-community-app-catalog/main/index.json");
    QNetworkReply *reply = m_network.get(makeRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300 && body.size() < 2 * 1024 * 1024;
        reply->deleteLater();
        QJsonParseError parseError;
        const QJsonDocument document = ok ? QJsonDocument::fromJson(body, &parseError) : QJsonDocument();
        const QJsonArray lists = document.object().value("lists").toArray();
        if (!document.isObject() || parseError.error != QJsonParseError::NoError || lists.isEmpty()) {
            loadCachedCatalog();
            setCatalogStatus(m_apps.isEmpty()
                ? "Não foi possível baixar o catálogo e não existe cache local. Verifique sua conexão."
                : QString("Falha ao atualizar; usando %1 apps já salvos localmente.").arg(m_apps.size()));
            setCatalogBusy(false);
            return;
        }
        QSaveFile cache(QDir(m_catalogRoot).filePath("index.json"));
        if (cache.open(QIODevice::WriteOnly)) { cache.write(body); cache.commit(); }
        for (const QJsonValue &entry : lists) {
            const QString listUrl = entry.toObject().value("remoteLocation").toString();
            const QUrl parsed(listUrl);
            if (parsed.scheme() == "https" && parsed.host() == "raw.githubusercontent.com" && !cacheFileName(listUrl).isEmpty())
                m_catalogFiles << listUrl;
        }
        requestNextCatalogList();
    });
}

void QuiverIntegration::requestNextCatalogList() {
    if (m_catalogFileIndex >= m_catalogFiles.size()) { finishCatalogRefresh(); return; }
    const QString urlText = m_catalogFiles.at(m_catalogFileIndex++);
    const QUrl url(urlText);
    setCatalogStatus(QString("Baixando lista %1 de %2…").arg(m_catalogFileIndex).arg(m_catalogFiles.size()));
    QNetworkReply *reply = m_network.get(makeRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, urlText]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        const QString cachePath = cacheFileName(urlText);
        const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300 && body.size() < 12 * 1024 * 1024;
        reply->deleteLater();
        const QString listName = QFileInfo(QUrl(urlText).path()).completeBaseName();
        if (ok) {
            QSaveFile cache(cachePath);
            if (cache.open(QIODevice::WriteOnly) && cache.write(body) == body.size()) cache.commit();
            loadCatalogFile(cachePath, listName, false);
        } else {
            loadCatalogFile(cachePath, listName, false);
        }
        requestNextCatalogList();
    });
}

void QuiverIntegration::finishCatalogRefresh() {
    setCatalogBusy(false);
    if (m_apps.isEmpty()) setCatalogStatus("As listas foram consultadas, mas nenhuma entrada válida foi encontrada.");
    else setCatalogStatus(QString("Catálogo comunitário legado atualizado: %1 apps em %2 listas.").arg(m_apps.size()).arg(m_catalogFiles.size()));
}

void QuiverIntegration::openOrPrepareApp(const QVariantMap &app) {
    if (m_operationBusy) return;
    const QString repository = app.value("repository").toString().trimmed();
    if (repository.isEmpty() || !QRegularExpression("^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$").match(repository).hasMatch()) {
        emit errorOccurred("Esta entrada não tem um repositório público válido para consultar releases.");
        return;
    }
    const QString folderName = safeFolderName(app);
    if (folderName.isEmpty()) { emit errorOccurred("O catálogo não forneceu um nome de pasta seguro para esta entrada."); return; }
    const QString installedRoot = QDir(appsDirectory()).filePath(folderName);
    const QStringList installed = findExecutables(installedRoot);
    if (!installed.isEmpty()) {
        m_installedExecutables.clear();
        for (const QString &path : installed) m_installedExecutables << path;
        emit installedExecutablesChanged();
        const QString name = app.value("customDisplayName").toString().trimmed().isEmpty()
            ? app.value("name").toString().trimmed() : app.value("customDisplayName").toString().trimmed();
        const QString project = app.value("project").toString().trimmed();
        const QString title = project.isEmpty() || name.contains(project, Qt::CaseInsensitive)
            ? name : name + " — " + project;
        if (installed.size() == 1) {
            launchInstalledApp(installed.first());
            setOperationStatus(QString("%1 aberto.").arg(title));
        } else {
            setOperationStatus(QString("%1 já está instalado; selecione o executável principal.").arg(title));
        }
        emit appInstalled(title, m_installedExecutables, app.value("appIconUrl").toString());
        return;
    }
    setPendingApp(app);
    m_releaseAssets.clear();
    emit releaseAssetsChanged();
    setOperationBusy(true);
    setOperationStatus(QString("Consultando releases de %1…").arg(repository));
    requestLatestRelease(repository, app.value("repositorySource").toString());
}

void QuiverIntegration::requestLatestRelease(const QString &repository, const QString &repositorySource) {
    QUrl url;
    const QString source = repositorySource.trimmed().toLower();
    if (source == "gitlab") {
        url = QUrl("https://gitlab.com/api/v4/projects/" + QString::fromLatin1(QUrl::toPercentEncoding(repository)) + "/releases/permalink/latest");
    } else {
        url = QUrl("https://api.github.com/repos/" + repository + "/releases/latest");
    }
    QNetworkReply *reply = m_network.get(makeRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, repository, source]() {
        handleReleaseReply(reply, repository, source);
    });
}

void QuiverIntegration::handleReleaseReply(QNetworkReply *reply, const QString &repository,
                                           const QString &repositorySource) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const QString networkError = reply->errorString();
    reply->deleteLater();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || status < 200 || status >= 300) {
        const QString message = QString("Não foi possível consultar a release de %1 (HTTP %2): %3")
                                    .arg(repository).arg(status).arg(networkError);
        setOperationStatus(message);
        setOperationBusy(false);
        emit errorOccurred(message);
        return;
    }
    const QJsonObject release = document.object();
    const QString version = release.value("tag_name").toString();
    QVariantList candidates;
    const QString source = repositorySource.toLower();
    if (source == "gitlab") {
        const QJsonArray links = release.value("assets").toObject().value("links").toArray();
        for (const QJsonValue &value : links) {
            const QJsonObject asset = value.toObject();
            QVariantMap item;
            item.insert("name", asset.value("name").toString());
            item.insert("url", asset.value("direct_asset_url").toString(asset.value("url").toString()));
            item.insert("size", -1);
            item.insert("version", version);
            item.insert("digest", QString());
            candidates << item;
        }
    } else {
        for (const QJsonValue &value : release.value("assets").toArray()) {
            const QJsonObject asset = value.toObject();
            QVariantMap item;
            item.insert("name", asset.value("name").toString());
            item.insert("url", asset.value("browser_download_url").toString());
            item.insert("size", asset.value("size").toVariant());
            item.insert("version", version);
            item.insert("digest", asset.value("digest").toString());
            candidates << item;
        }
    }
    const QString filter = m_pendingApp.value("releaseAssetFilter").toString().trimmed().toLower();
    QVariantList filtered;
    for (const QVariant &value : candidates) {
        const QVariantMap item = value.toMap();
        const QString name = item.value("name").toString();
        const QString lower = name.toLower();
        if (!lower.endsWith(".zip")) continue;
        if (!filter.isEmpty()) {
            bool matches = false;
            if (filter.contains('*') || filter.contains('?')) {
                QString pattern = QRegularExpression::escape(filter.left(256));
                pattern.replace("\\*", ".*");
                pattern.replace("\\?", ".");
                const QRegularExpression expression("^" + pattern + "$", QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
                matches = expression.isValid() && expression.match(name).hasMatch();
            } else {
                matches = lower.contains(filter);
            }
            if (!matches) continue;
        }
        if (item.value("url").toString().isEmpty()) continue;
        filtered << item;
    }
    if (filtered.isEmpty()) {
        setOperationStatus("Esta release não oferece um pacote .zip compatível com a instalação automática do HUB.");
        setOperationBusy(false);
        emit errorOccurred(m_operationStatus);
        return;
    }
    bool hasWindows = false;
    for (const QVariant &value : filtered) {
        const QString name = value.toMap().value("name").toString().toLower();
        if (name.contains("win") || name.contains("windows")) hasWindows = true;
    }
    if (hasWindows) {
        QVariantList windowsOnly;
        for (const QVariant &value : filtered) {
            const QString name = value.toMap().value("name").toString().toLower();
            if (name.contains("win") || name.contains("windows")) windowsOnly << value;
        }
        filtered = windowsOnly;
    }
    m_releaseAssets = filtered;
    emit releaseAssetsChanged();
    setOperationBusy(false);
    setOperationStatus(QString("%1 pacote(s) .zip disponível(is) em %2. Escolha uma release para baixar.")
                            .arg(filtered.size()).arg(version));
}

void QuiverIntegration::installSelectedRelease(const QVariantMap &asset) {
    if (m_operationBusy) return;
    const QString urlText = asset.value("url").toString();
    const QUrl url(urlText);
    const QString source = m_pendingApp.value("repositorySource").toString();
    const QString allowedHost = source == "gitlab" ? "gitlab.com" : "github.com";
    if (url.scheme() != "https" || (url.host() != allowedHost && !(source != "gitlab" && url.host().endsWith("githubusercontent.com")))) {
        emit errorOccurred("A URL do pacote não pertence ao host oficial esperado; download bloqueado.");
        return;
    }
    if (!url.path().toLower().endsWith(".zip")) {
        emit errorOccurred("Por segurança, esta versão do instalador aceita somente arquivos .zip.");
        return;
    }
    const QString folder = safeFolderName(m_pendingApp);
    const QString target = QDir(appsDirectory()).filePath(folder);
    if (QDir(target).exists()) {
        const QStringList existing = findExecutables(target);
        if (existing.isEmpty()) {
            emit errorOccurred("Já existe uma pasta com esse nome, mas sem executável reconhecido. Renomeie ou remova a pasta antes de instalar.");
            return;
        }
        m_installedExecutables.clear();
        for (const QString &path : existing) m_installedExecutables << path;
        emit installedExecutablesChanged();
        emit appInstalled(m_pendingApp.value("name").toString(), m_installedExecutables,
                          m_pendingApp.value("appIconUrl").toString());
        return;
    }
    m_expectedSha256 = asset.value("digest").toString();
    beginDownload(urlText, QFileInfo(url.path()).fileName());
}

void QuiverIntegration::beginDownload(const QString &url, const QString &fileName) {
    if (m_downloadOutput) {
        m_downloadOutput->cancelWriting();
        delete m_downloadOutput;
        m_downloadOutput = nullptr;
    }
    const QString downloadDir = QDir(m_dataRoot).filePath("downloads");
    if (!QDir().mkpath(downloadDir)) {
        setOperationBusy(false);
        emit errorOccurred("Não foi possível criar a pasta temporária de downloads.");
        return;
    }
    m_downloadFileName = QFileInfo(QUrl(url).path()).fileName();
    if (m_downloadFileName.isEmpty()) m_downloadFileName = fileName;
    m_downloadArchivePath = QDir(downloadDir).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + "-" + QFileInfo(m_downloadFileName).fileName());
    m_downloadOutput = new QSaveFile(m_downloadArchivePath);
    if (!m_downloadOutput->open(QIODevice::WriteOnly)) {
        delete m_downloadOutput;
        m_downloadOutput = nullptr;
        setOperationBusy(false);
        emit errorOccurred("Não foi possível abrir o arquivo temporário para o download.");
        return;
    }
    m_downloadBytesReceived = 0;
    m_downloadFailed = false;
    m_downloadHash.reset();
    setOperationBusy(true);
    setOperationStatus(QString("Baixando %1…").arg(fileName));
    QNetworkReply *reply = m_network.get(makeRequest(QUrl(url)));
    reply->setReadBufferSize(4 * 1024 * 1024);
    connect(reply, &QIODevice::readyRead, this, [this, reply]() {
        if (m_downloadFailed || !m_downloadOutput) return;
        const QByteArray chunk = reply->readAll();
        m_downloadBytesReceived += chunk.size();
        if (m_downloadBytesReceived > 1024LL * 1024 * 1024 || m_downloadOutput->write(chunk) != chunk.size()) {
            m_downloadFailed = true;
            reply->abort();
        } else {
            m_downloadHash.addData(chunk);
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, fileName](qint64 received, qint64 total) {
        if (total > 0) setOperationStatus(QString("Baixando %1 — %2% (%3 / %4 MiB)…")
            .arg(fileName).arg((received * 100) / total).arg(received / (1024 * 1024)).arg(total / (1024 * 1024)));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray remainder = reply->readAll();
        if (!remainder.isEmpty() && m_downloadOutput && !m_downloadFailed) {
            m_downloadBytesReceived += remainder.size();
            if (m_downloadBytesReceived > 1024LL * 1024 * 1024 || m_downloadOutput->write(remainder) != remainder.size())
                m_downloadFailed = true;
            else
                m_downloadHash.addData(remainder);
        }
        const QString networkError = reply->errorString();
        const bool transferOk = !m_downloadFailed && reply->error() == QNetworkReply::NoError &&
                                status >= 200 && status < 300 && m_downloadBytesReceived > 0 && m_downloadOutput;
        QString expectedHash = m_expectedSha256.trimmed().toLower();
        if (expectedHash.startsWith("sha256:")) expectedHash.remove(0, 7);
        const bool hashOk = expectedHash.isEmpty() || m_downloadHash.result().toHex() == expectedHash.toLatin1();
        const bool committed = transferOk && hashOk && m_downloadOutput && m_downloadOutput->commit();
        reply->deleteLater();
        if (!committed) {
            if (m_downloadOutput) { m_downloadOutput->cancelWriting(); delete m_downloadOutput; m_downloadOutput = nullptr; }
            QFile::remove(m_downloadArchivePath);
            if (!hashOk)
                setOperationStatus("A verificação SHA-256 do pacote falhou; o arquivo foi descartado.");
            else if (m_downloadFailed)
                setOperationStatus("Download bloqueado: o pacote ultrapassou 1 GiB ou não pôde ser gravado.");
            else if (transferOk)
                setOperationStatus("Não foi possível finalizar a gravação do pacote baixado.");
            else
                setOperationStatus(QString("Download falhou (HTTP %1): %2").arg(status).arg(networkError));
            setOperationBusy(false);
            emit errorOccurred(m_operationStatus);
            return;
        }
        delete m_downloadOutput;
        m_downloadOutput = nullptr;
        const QString archivePath = m_downloadArchivePath;
        finishCatalogAppInstall(archivePath);
        QFile::remove(archivePath);
    });
}

bool QuiverIntegration::extractZip(const QString &archivePath, const QString &destination, QString *error) const {
    if (!QDir().mkpath(destination)) { if (error) *error = "Não foi possível criar a pasta de instalação temporária."; return false; }
#ifdef Q_OS_WIN
    QString quotedArchive = archivePath;
    QString quotedDestination = destination;
    quotedArchive.replace("'", "''");
    quotedDestination.replace("'", "''");
    const QString command = "$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath '" + quotedArchive + "' -DestinationPath '" + quotedDestination + "' -Force";
    QProcess process;
    process.start("powershell.exe", {"-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", command});
#else
    QProcess process;
    process.start("unzip", {"-q", "-o", archivePath, "-d", destination});
#endif
    if (!process.waitForStarted(10000)) { if (error) *error = "Não foi possível iniciar o extrator de arquivos ZIP."; return false; }
    if (!process.waitForFinished(180000)) { process.kill(); process.waitForFinished(); if (error) *error = "A extração do arquivo passou do tempo limite."; return false; }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) *error = QString("A extração do arquivo ZIP falhou: %1").arg(QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
        return false;
    }
    return true;
}

QStringList QuiverIntegration::findExecutables(const QString &root) const {
    QStringList results;
    if (!QDir(root).exists()) return results;
#ifdef Q_OS_WIN
    const QStringList patterns = {"*.exe"};
#else
    const QStringList patterns = {"*.AppImage", "*.sh", "*.x86_64", "*.bin"};
#endif
    QDirIterator it(root, patterns, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext()) results << QFileInfo(it.next()).absoluteFilePath();
    results.sort(Qt::CaseInsensitive);
    return results;
}

QString QuiverIntegration::safeFolderName(const QVariantMap &app) const {
    QString folder = app.value("folderName").toString().trimmed();
    if (folder.isEmpty()) folder = app.value("name").toString() + "-" + app.value("project").toString();
    folder.replace(QRegularExpression("[^A-Za-z0-9._-]+"), "-");
    folder.replace(QRegularExpression("-+"), "-");
    while (folder.startsWith('.')) folder.remove(0, 1);
    if (folder == ".." || folder.isEmpty()) return {};
    return folder.left(100);
}

bool QuiverIntegration::createQuiverHelperFiles(const QVariantMap &app, const QString &root, QString *error) const {
    const QStringList files = app.value("filesToAdd").toStringList();
    for (const QString &requested : files) {
        const QString name = requested.trimmed();
        if (name.isEmpty() || name == "." || name == ".." || name.contains('/') || name.contains('\\') ||
            name.contains(':') || name.contains(QChar::Null)) {
            if (error) *error = "A entrada contém um arquivo auxiliar inválido; a instalação foi cancelada.";
            return false;
        }
        const QString path = QDir(root).filePath(name);
        if (QFileInfo::exists(path)) continue;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            if (error) *error = QString("Não foi possível criar o arquivo auxiliar %1.").arg(name);
            return false;
        }
        file.close();
    }
    return true;
}

void QuiverIntegration::finishCatalogAppInstall(const QString &archivePath) {
    const QString folder = safeFolderName(m_pendingApp);
    const QString appRoot = QDir(appsDirectory()).filePath(folder);
    if (QDir(appRoot).exists()) {
        setOperationBusy(false);
        emit errorOccurred("A pasta do app já existe. Para evitar sobrescrever seus arquivos, faça uma cópia de segurança ou escolha outro folderName.");
        return;
    }
    const QString stage = QDir(appsDirectory()).filePath(".staging-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    QString extractError;
    if (!extractZip(archivePath, stage, &extractError)) {
        QDir(stage).removeRecursively();
        setOperationBusy(false);
        setOperationStatus(extractError);
        emit errorOccurred(extractError);
        return;
    }
    if (!createQuiverHelperFiles(m_pendingApp, stage, &extractError)) {
        QDir(stage).removeRecursively();
        setOperationBusy(false);
        emit errorOccurred(extractError);
        return;
    }
    if (!QDir().rename(stage, appRoot)) {
        QDir(stage).removeRecursively();
        setOperationBusy(false);
        emit errorOccurred("Não foi possível mover os arquivos para a pasta final do app.");
        return;
    }
    m_installedExecutables.clear();
    for (const QString &path : findExecutables(appRoot)) m_installedExecutables << path;
    emit installedExecutablesChanged();
    if (m_installedExecutables.isEmpty()) {
        setOperationBusy(false);
        setOperationStatus(QString("Arquivos instalados em %1; não encontrei executável automaticamente.").arg(appRoot));
        emit errorOccurred("O arquivo foi extraído. Não encontrei um executável reconhecido; confira a pasta e as instruções do projeto.");
        return;
    }
    const QString name = m_pendingApp.value("customDisplayName").toString().trimmed().isEmpty()
        ? m_pendingApp.value("name").toString().trimmed()
        : m_pendingApp.value("customDisplayName").toString().trimmed();
    const QString project = m_pendingApp.value("project").toString().trimmed();
    const QString title = project.isEmpty() || name.contains(project, Qt::CaseInsensitive)
        ? name : name + " — " + project;
    setOperationBusy(false);
    setOperationStatus(QString("%1 instalado em %2. Selecione o executável principal para adicioná-lo à biblioteca.").arg(title, appRoot));
    emit appInstalled(title, m_installedExecutables, m_pendingApp.value("appIconUrl").toString());
}

bool QuiverIntegration::launchInstalledApp(const QString &executablePath) {
    const QString path = QDir::cleanPath(executablePath.trimmed());
    if (path.isEmpty() || !QFileInfo(path).isFile()) {
        emit errorOccurred("O executável selecionado não existe mais.");
        return false;
    }
#ifdef Q_OS_UNIX
    QFile::setPermissions(path, QFile::permissions(path) | QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
#endif
    const bool ok = QProcess::startDetached(path, {}, QFileInfo(path).absolutePath());
    if (!ok) emit errorOccurred("Não foi possível abrir o app instalado.");
    return ok;
}
