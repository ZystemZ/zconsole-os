#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>

class GameCatalog : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    Q_PROPERTY(QVariantList emulatorConfigs READ emulatorConfigs NOTIFY emulatorConfigsChanged)
public:
    explicit GameCatalog(QObject *parent = nullptr);
    bool initialize(const QSqlDatabase &db);
    QVariantList games() const;
    QVariantList emulatorConfigs() const;

    Q_INVOKABLE bool addManualGame(const QString &title, const QString &path,
                                   const QString &platform, const QString &launchCommand);
    Q_INVOKABLE bool addPath(const QString &path, const QString &platform = QStringLiteral("PC"));
    Q_INVOKABLE QString localPathFromUrl(const QString &url) const;
    Q_INVOKABLE bool importCandidate(const QString &title, const QString &path,
                                     const QString &platform, const QString &source,
                                     const QString &launchCommand);
    Q_INVOKABLE bool updateManualGame(qint64 id, const QString &title, const QString &path,
                                      const QString &platform, const QString &launchCommand);
    Q_INVOKABLE QVariantMap emulatorConfig(const QString &platform) const;
    Q_INVOKABLE QString detectRetroArch() const;
    Q_INVOKABLE QStringList detectRetroArchCores() const;
    Q_INVOKABLE QString suggestRetroArchCore(const QString &platform) const;
    Q_INVOKABLE bool saveEmulatorConfig(const QString &platform, const QString &executable,
                                        const QString &arguments, const QString &core,
                                        const QString &preset = QStringLiteral("Personalizado"),
                                        const QString &aspectRatio = QStringLiteral("auto"),
                                        bool enabled = true);
    Q_INVOKABLE bool removeEmulatorConfig(const QString &platform);
    Q_INVOKABLE bool launchGame(qint64 id);
    Q_INVOKABLE bool removeGame(qint64 id);
    Q_INVOKABLE void refresh();

signals:
    void gamesChanged();
    void emulatorConfigsChanged();
    void errorOccurred(const QString &message);

private:
    QSqlDatabase m_db;
    QVariantList m_games;
    QVariantList m_emulatorConfigs;
    bool loadGames();
    bool loadEmulatorConfigs();
};
