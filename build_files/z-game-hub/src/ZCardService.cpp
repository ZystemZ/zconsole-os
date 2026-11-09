#include "ZCardService.h"
#include "zconsole/emulator/zexe.hpp"
#include "zconsole/zcard/virtual_card.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileDevice>
#include <QCryptographicHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QStandardPaths>
#include <QUrl>
#include <filesystem>
#include <vector>

namespace {
quint16 u16(const QByteArray &b, qsizetype p) {
    return static_cast<quint16>(static_cast<unsigned char>(b.at(p)) | (static_cast<quint16>(static_cast<unsigned char>(b.at(p + 1))) << 8));
}
quint32 u32(const QByteArray &b, qsizetype p) {
    return static_cast<quint32>(static_cast<unsigned char>(b.at(p))) |
           (static_cast<quint32>(static_cast<unsigned char>(b.at(p + 1))) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(b.at(p + 2))) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(b.at(p + 3))) << 24);
}
quint64 u64(const QByteArray &b, qsizetype p) {
    quint64 value = 0;
    for (int i = 0; i < 8; ++i) value |= static_cast<quint64>(static_cast<unsigned char>(b.at(p + i))) << (8 * i);
    return value;
}
bool safeEntry(const QString &name) {
    return !name.isEmpty() && !name.startsWith('/') && !name.contains('\\') && !name.contains(':') && !name.split('/').contains("..") && !name.contains("//");
}
bool imageEntry(const QString &name) {
    const QString lower = name.toLower();
    return (lower.endsWith(".png") || lower.endsWith(".jpg") || lower.endsWith(".jpeg") || lower.endsWith(".webp")) &&
           (lower.contains("cover") || lower.contains("capa") || lower.contains("poster") || lower.contains("boxart") || lower.contains("thumbnail"));
}
}

ZCardService::ZCardService(QObject *parent) : QObject(parent) {}

ZCardService::~ZCardService() {
    if (m_worker) {
        m_worker->quit();
        m_worker->wait();
        m_worker = nullptr;
    }
}

void ZCardService::showMessage(const QString &message) {
    setStatus(message);
}

void ZCardService::setStatus(const QString &value) {
    if (m_status == value) return;
    m_status = value;
    emit statusChanged();
}

void ZCardService::resetEntries() {
    m_entries.clear();
    m_currentFormat.clear();
}

void ZCardService::clearCard() {
    resetEntries();
    m_currentPath.clear();
    m_report.clear();
    emit cardChanged();
    emit reportChanged();
    setStatus("Nenhum cartão montado.");
}

bool ZCardService::parseDirectory(const QString &path, QString *error) {
    const QDir root(path);
    if (!root.exists()) { if (error) *error = "Diretório não encontrado."; return false; }
    const QStringList manifests = {"META/zgame.json", "META/rom-runtime.json", "META/assets.zgf"};
    QString detected;
    for (const auto &manifest : manifests) if (QFileInfo(root.filePath(manifest)).isFile()) { detected = manifest; break; }
    if (detected.isEmpty()) { if (error) *error = "Diretório sem META/zgame.json, META/rom-runtime.json ou META/assets.zgf."; return false; }
    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString absolute = it.next();
        const QString relative = root.relativeFilePath(absolute).replace('\\', '/');
        QVariantMap item;
        item["name"] = relative;
        item["size"] = QFileInfo(absolute).size();
        item["kind"] = relative.startsWith("META/") ? "manifest" : (relative.startsWith("assets/") ? "asset" : "file");
        m_entries.push_back(item);
    }
    m_currentFormat = "Diretório ZCARD / ZGF (" + detected + ")";
    return true;
}

bool ZCardService::parseArchive(const QByteArray &bytes, qsizetype baseOffset, qsizetype length, QString *error) {
    if (length < 22 || baseOffset < 0 || baseOffset + length > bytes.size()) { if (error) *error = "Arquivo ZIP/ZCARD incompleto."; return false; }
    const QByteArray view = bytes.mid(baseOffset, length);
    const QByteArray signature = QByteArray::fromHex("504b0506");
    qsizetype eocd = view.size() - 22;
    const qsizetype begin = qMax<qsizetype>(0, view.size() - 65557);
    while (eocd >= begin && view.mid(eocd, 4) != signature) --eocd;
    if (eocd < begin) { if (error) *error = "Diretório central ZIP ausente."; return false; }
    const quint16 count = u16(view, eocd + 10);
    const quint32 directorySize = u32(view, eocd + 12);
    const quint32 directoryOffset = u32(view, eocd + 16);
    if (static_cast<quint64>(directoryOffset) + directorySize > static_cast<quint64>(view.size())) { if (error) *error = "Diretório ZIP fora dos limites."; return false; }
    qsizetype p = directoryOffset;
    for (quint16 i = 0; i < count; ++i) {
        if (p + 46 > view.size() || u32(view, p) != 0x02014b50u) { if (error) *error = "Entrada ZIP inválida."; return false; }
        const quint16 nameLength = u16(view, p + 28);
        const quint16 extraLength = u16(view, p + 30);
        const quint16 commentLength = u16(view, p + 32);
        if (p + 46 + nameLength + extraLength + commentLength > view.size()) { if (error) *error = "Nome ZIP inválido."; return false; }
        const QString name = QString::fromUtf8(view.mid(p + 46, nameLength));
        if (!safeEntry(name)) { if (error) *error = "Nome inseguro/path traversal no cartão."; return false; }
        QVariantMap item;
        item["name"] = name;
        item["size"] = static_cast<quint64>(u32(view, p + 24));
        item["compressedSize"] = static_cast<quint64>(u32(view, p + 20));
        item["kind"] = name.startsWith("META/") ? "manifest" : (name.startsWith("assets/") ? "asset" : "file");
        m_entries.push_back(item);
        p += 46 + nameLength + extraLength + commentLength;
    }
    return true;
}

bool ZCardService::parseZcf(const QByteArray &bytes, QString *error) {
    if (bytes.size() < 64 || bytes.mid(0, 4) != "ZCF0") { if (error) *error = "Header ZCF inválido."; return false; }
    const quint32 count = u32(bytes, 24);
    const quint32 tableOffset = u32(bytes, 28);
    if (count == 0 || static_cast<quint64>(tableOffset) + static_cast<quint64>(count) * 48 > static_cast<quint64>(bytes.size())) { if (error) *error = "Tabela ZCF inválida."; return false; }
    const QStringList kinds = {"", "MASTER", "UPDATE", "DLC", "SAVE", "USER"};
    for (quint32 i = 0; i < count; ++i) {
        const qsizetype p = tableOffset + i * 48;
        const quint64 offset = u64(bytes, p);
        const quint64 size = u64(bytes, p + 8);
        const quint64 capacity = u64(bytes, p + 16);
        const quint32 kind = u32(bytes, p + 24);
        QVariantMap item;
        item["name"] = kind < static_cast<quint32>(kinds.size()) ? kinds.at(static_cast<int>(kind)) : "REGION";
        item["size"] = size;
        item["capacity"] = capacity;
        item["kind"] = "region";
        item["offset"] = offset;
        m_entries.push_back(item);
        if (kind == 1 && offset + size <= static_cast<quint64>(bytes.size())) {
            QString nestedError;
            parseArchive(bytes, static_cast<qsizetype>(offset), static_cast<qsizetype>(size), &nestedError);
        }
    }
    m_currentFormat = "Container ZCF0 (MASTER + regiões UPDATE/DLC/SAVE/USER)";
    return true;
}

bool ZCardService::parseZuf(const QByteArray &bytes, QString *error) {
    if (bytes.size() < 32 || bytes.mid(0, 4) != "ZUF0") { if (error) *error = "Header ZUF inválido."; return false; }
    const quint32 count = u32(bytes, 8);
    qsizetype p = 32;
    for (quint32 i = 0; i < count; ++i) {
        if (p + 12 > bytes.size()) { if (error) *error = "Entrada ZUF truncada."; return false; }
        const quint16 nameLength = u16(bytes, p);
        const quint64 size = u64(bytes, p + 4);
        p += 12;
        if (p + nameLength + static_cast<qsizetype>(size) > bytes.size()) { if (error) *error = "Payload ZUF fora dos limites."; return false; }
        const QString name = QString::fromUtf8(bytes.mid(p, nameLength));
        QVariantMap item;
        item["name"] = name;
        item["size"] = size;
        item["kind"] = name.startsWith("META/") ? "manifest" : "file";
        m_entries.push_back(item);
        p += nameLength + static_cast<qsizetype>(size);
    }
    m_currentFormat = "Pacote ZUF0";
    return true;
}

bool ZCardService::openCard(const QString &path) {
    resetEntries();
    m_currentPath = path;
    QString error;
    QFileInfo info(path);
    bool ok = false;
    if (info.isDir()) ok = parseDirectory(path, &error);
    else if (info.isFile()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = file.readAll();
            if (bytes.startsWith("ZCF0")) ok = parseZcf(bytes, &error);
            else if (bytes.startsWith("ZUF0")) ok = parseZuf(bytes, &error);
            else if (bytes.startsWith("PK")) { m_currentFormat = "Arquivo ZCARD ZIP legado"; ok = parseArchive(bytes, 0, bytes.size(), &error); }
            else error = "Formato não reconhecido. Esperado diretório ZCARD, ZIP/ZCARD, ZCF0 ou ZUF0.";
        } else error = "Não foi possível abrir o arquivo.";
    } else error = "Caminho não encontrado.";
    if (!ok) { m_currentPath.clear(); resetEntries(); setStatus(error); }
    else setStatus(QString("Cartão montado: %1 entrada(s).").arg(m_entries.size()));
    emit cardChanged();
    return ok;
}

bool ZCardService::readCardAsset(const QString &entry, QByteArray *data, QString *error) const {
    if (m_currentPath.isEmpty() || !data) { if (error) *error = "Nenhum cartão montado."; return false; }
    const QFileInfo info(m_currentPath);
    if (info.isDir()) {
        QFile file(QDir(m_currentPath).filePath(entry));
        if (!file.open(QIODevice::ReadOnly)) { if (error) *error = "Asset do cartão não pode ser lido: " + entry; return false; }
        *data = file.readAll();
        return true;
    }
    std::string mountError;
    zconsole::VirtualZCard card(std::filesystem::path(m_currentPath.toStdWString()));
    if (!card.mount(&mountError)) { if (error) *error = QString::fromStdString(mountError); return false; }
    auto stream = card.openStream(entry.toStdString(), &mountError);
    if (!stream) { if (error) *error = QString::fromStdString(mountError); return false; }
    QByteArray result;
    std::vector<char> chunk(64 * 1024);
    std::size_t read = 0;
    while ((read = stream->read(chunk.data(), chunk.size())) > 0) result.append(chunk.data(), static_cast<qsizetype>(read));
    *data = result;
    return true;
}

QVariantMap ZCardService::cardInfo(const QString &path) {
    QVariantMap info;
    if (!openCard(path)) return info;
    QString error;
    QString manifestEntry;
    for (const QVariant &value : m_entries) {
        const QString name = value.toMap().value("name").toString();
        if (name == "META/zgame.json" || name == "META/rom-runtime.json") { manifestEntry = name; break; }
    }
    QByteArray manifestBytes;
    QJsonObject manifest;
    if (!manifestEntry.isEmpty() && readCardAsset(manifestEntry, &manifestBytes, &error)) {
        const QJsonDocument doc = QJsonDocument::fromJson(manifestBytes);
        if (doc.isObject()) manifest = doc.object();
    }
    QString coverEntry;
    for (const QVariant &value : m_entries) {
        const QString name = value.toMap().value("name").toString();
        if (imageEntry(name)) { coverEntry = name; break; }
    }
    QString coverPath;
    if (!coverEntry.isEmpty()) {
        QByteArray coverBytes;
        if (readCardAsset(coverEntry, &coverBytes, &error)) {
            QImage cover;
            if (cover.loadFromData(coverBytes)) {
                const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/covers/zcards";
                QDir().mkpath(dir);
                const QString hash = QString::fromLatin1(QCryptographicHash::hash((path + coverEntry).toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
                coverPath = dir + "/" + hash + ".png";
                if (!QFileInfo(coverPath).isFile()) cover.save(coverPath, "PNG");
            }
        }
    }
    const bool native = manifest.value("format").toString().startsWith("zgame-native-");
    QString platform = native ? manifest.value("platform").toString() : manifest.value("system").toString();
    if (platform.isEmpty()) platform = native ? "native" : "ROM";
    info["title"] = manifest.value("title").toString().isEmpty() ? QFileInfo(path).completeBaseName() : manifest.value("title").toString();
    info["path"] = path;
    info["cardPath"] = path;
    info["platform"] = "ZCARD / " + platform;
    info["source"] = "zcard";
    info["available"] = true;
    info["coverPath"] = coverPath;
    info["coverUrl"] = coverPath.isEmpty() ? QString() : QUrl::fromLocalFile(coverPath).toString();
    info["format"] = m_currentFormat;
    return info;
}

bool ZCardService::registerCard(const QString &path) {
    const QString clean = QFileInfo(path).absoluteFilePath();
    for (const QVariant &value : m_cards) if (value.toMap().value("path").toString() == clean) return true;
    const QVariantMap info = cardInfo(clean);
    if (info.isEmpty()) return false;
    m_cards.push_back(info);
    emit cardsChanged();
    return true;
}

void ZCardService::scanCards() {
    const QStringList roots = {
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/ZCards",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/ZCards",
        QDir::homePath() + "/ZCards"
    };
    for (const QString &root : roots) {
        if (!QDir(root).exists()) continue;
        QDirIterator it(root, {"*.zcard", "*.zip", "*.zcf", "*.zuf"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) registerCard(it.next());
        QDirIterator dirs(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (dirs.hasNext()) {
            const QString dir = dirs.next();
            if (QFileInfo(dir + "/META/zgame.json").isFile() || QFileInfo(dir + "/META/rom-runtime.json").isFile()) registerCard(dir);
        }
    }
    clearCard();
    setStatus(m_cards.isEmpty() ? "Nenhum ZCARD encontrado nas pastas padrão." : QString("%1 ZCARD(s) disponível(is) na biblioteca.").arg(m_cards.size()));
}

QVariantMap ZCardService::resolveLaunchPlan() {
    QVariantMap plan;
    if (m_currentPath.isEmpty()) { plan["ok"] = false; plan["message"] = "Monte um ZCARD antes de executar seu conteúdo."; return plan; }
    QString manifestEntry;
    for (const QVariant &value : m_entries) {
        const QString name = value.toMap().value("name").toString();
        if (name == "META/zgame.json" || name == "META/rom-runtime.json") { manifestEntry = name; break; }
    }
    if (manifestEntry.isEmpty()) { plan["ok"] = false; plan["message"] = "O cartão não possui manifesto de execução."; return plan; }
    QByteArray manifestBytes;
    QString error;
    if (!readCardAsset(manifestEntry, &manifestBytes, &error)) { plan["ok"] = false; plan["message"] = error; return plan; }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) { plan["ok"] = false; plan["message"] = "Manifesto ZCARD inválido: " + parseError.errorString(); return plan; }
    const QJsonObject object = document.object();
    const QString format = object.value("format").toString();
    const bool native = manifestEntry.endsWith("zgame.json") && format.startsWith("zgame-native-");
    const QString entry = native ? object.value("executable").toString() : object.value("rom").toString();
    if (entry.isEmpty() || !safeEntry(entry)) { plan["ok"] = false; plan["message"] = "Manifesto sem executável ou ROM seguro."; return plan; }
    QString platform = native ? object.value("platform").toString().toLower() : object.value("system").toString().toLower();
    if (platform.isEmpty()) platform = native ? "native" : QFileInfo(entry).suffix().toLower();

    QString launchPath;
    const QFileInfo cardInfoFile(m_currentPath);
    if (cardInfoFile.isDir()) {
        launchPath = QDir(m_currentPath).filePath(entry);
        if (!QFileInfo(launchPath).isFile()) { plan["ok"] = false; plan["message"] = "O conteúdo indicado pelo manifesto não existe: " + entry; return plan; }
    } else {
        const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/mounted-zcards/" + QString::fromLatin1(QCryptographicHash::hash(m_currentPath.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
        QDir().mkpath(cacheRoot);
        launchPath = QDir(cacheRoot).filePath(QFileInfo(entry).fileName());
        QByteArray payload;
        if (!readCardAsset(entry, &payload, &error)) { plan["ok"] = false; plan["message"] = error; return plan; }
        QFile output(launchPath);
        if (!output.open(QIODevice::WriteOnly) || output.write(payload) != payload.size()) { plan["ok"] = false; plan["message"] = "Não foi possível materializar o conteúdo do cartão."; return plan; }
        output.close();
#ifndef Q_OS_WIN
        output.setPermissions(output.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
#endif
    }
    if (native) {
#if defined(Q_OS_WIN)
        const QString host = "windows";
#elif defined(Q_OS_MACOS)
        const QString host = "macos";
#elif defined(Q_OS_ANDROID)
        const QString host = "android";
#elif defined(Q_OS_LINUX)
        const QString host = "linux";
#else
        const QString host = "unknown";
#endif
        const bool compatible = platform == host || (host == "macos" && platform == "darwin");
        if (!compatible) { plan["ok"] = false; plan["message"] = "Este executável nativo é para " + platform + ", mas o HUB está rodando em " + host + "."; return plan; }
    }
    plan["ok"] = true;
    plan["path"] = launchPath;
    plan["platform"] = native ? "native:" + platform : platform;
    plan["entry"] = entry;
    plan["native"] = native;
    plan["title"] = object.value("title").toString();
    plan["message"] = native ? "Executável nativo pronto para o sistema operacional." : "ROM pronta para o emulador configurado para " + platform + ".";
    return plan;
}

bool ZCardService::buildCard(const QString &source, const QString &output, const QString &format) {
    if (source.trimmed().isEmpty() || output.trimmed().isEmpty()) { setStatus("Informe a pasta fonte e o arquivo de saída."); return false; }
    if (format != "legacy" && format != "zcf" && format != "zuf") { setStatus("Formato de ZCARD inválido."); return false; }
    if (m_busy) return false;
    m_busy = true;
    m_report.clear();
    emit reportChanged();
    emit busyChanged();
    m_worker = QThread::create([this, source, output, format] {
        std::string report;
        std::string error;
        const std::filesystem::path sourcePath(source.toStdWString());
        const std::filesystem::path outputPath(output.toStdWString());
        bool ok = false;
        if (format == "zcf") ok = zconsole::ZCardBuilder::buildZcf(sourcePath, outputPath, &report, &error);
        else if (format == "zuf") ok = zconsole::ZCardBuilder::buildZuf(sourcePath, outputPath, &report, &error);
        else ok = zconsole::ZCardBuilder::build(sourcePath, outputPath, &report, &error);
        QMetaObject::invokeMethod(this, [this, ok, report, error] {
            m_busy = false;
            m_report = QString::fromStdString(report.empty() ? error : report);
            emit reportChanged();
            emit busyChanged();
            setStatus(ok ? "ZCARD gerado e validado dentro do HUB." : "Falha ao gerar ZCARD: " + QString::fromStdString(error));
            if (m_worker) { m_worker->deleteLater(); m_worker = nullptr; }
        }, Qt::QueuedConnection);
    });
    m_worker->start();
    setStatus("Gerando ZCARD dentro do HUB…");
    return true;
}
