#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>

class QuiverIntegration : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(QString catalogStatus READ catalogStatus NOTIFY catalogStatusChanged)
    Q_PROPERTY(bool catalogBusy READ catalogBusy NOTIFY catalogBusyChanged)
    Q_PROPERTY(QString operationStatus READ operationStatus NOTIFY operationStatusChanged)
    Q_PROPERTY(bool operationBusy READ operationBusy NOTIFY operationBusyChanged)
    Q_PROPERTY(QVariantList releaseAssets READ releaseAssets NOTIFY releaseAssetsChanged)
    Q_PROPERTY(QVariantMap pendingApp READ pendingApp NOTIFY pendingAppChanged)
    Q_PROPERTY(QVariantList installedExecutables READ installedExecutables NOTIFY installedExecutablesChanged)

public:
    explicit QuiverIntegration(QObject *parent = nullptr);

    QVariantList apps() const;
    QString catalogStatus() const;
    bool catalogBusy() const;
    QString operationStatus() const;
    bool operationBusy() const;
    QVariantList releaseAssets() const;
    QVariantMap pendingApp() const;
    QVariantList installedExecutables() const;

    Q_INVOKABLE void refreshCatalog();
    Q_INVOKABLE void openOrPrepareApp(const QVariantMap &app);
    Q_INVOKABLE void installSelectedRelease(const QVariantMap &asset);
    Q_INVOKABLE bool launchInstalledApp(const QString &executablePath);
    Q_INVOKABLE QString appsDirectory() const;

signals:
    void appsChanged();
    void catalogStatusChanged();
    void catalogBusyChanged();
    void operationStatusChanged();
    void operationBusyChanged();
    void releaseAssetsChanged();
    void pendingAppChanged();
    void installedExecutablesChanged();
    void errorOccurred(const QString &message);
    void appInstalled(const QString &title, const QVariantList &executables, const QString &iconUrl);

private:
    QNetworkAccessManager m_network;
    QVariantList m_apps;
    QVariantList m_releaseAssets;
    QVariantMap m_pendingApp;
    QVariantList m_installedExecutables;
    QString m_dataRoot;
    QString m_catalogRoot;
    QString m_catalogStatus;
    QString m_operationStatus;
    QStringList m_catalogFiles;
    int m_catalogFileIndex = 0;
    bool m_catalogBusy = false;
    bool m_operationBusy = false;
    QString m_downloadFileName;
    QString m_downloadArchivePath;
    QSaveFile *m_downloadOutput = nullptr;
    qint64 m_downloadBytesReceived = 0;
    bool m_downloadFailed = false;
    QString m_expectedSha256;
    QCryptographicHash m_downloadHash{QCryptographicHash::Sha256};

    void setCatalogBusy(bool busy);
    void setOperationBusy(bool busy);
    void setCatalogStatus(const QString &status);
    void setOperationStatus(const QString &status);
    void setPendingApp(const QVariantMap &app);
    void loadCachedCatalog();
    bool loadCatalogFile(const QString &filePath, const QString &listName, bool replace);
    void requestCatalogIndex();
    void requestNextCatalogList();
    void finishCatalogRefresh();
    void requestLatestRelease(const QString &repository, const QString &repositorySource);
    void handleReleaseReply(QNetworkReply *reply, const QString &repository,
                            const QString &repositorySource);
    void beginDownload(const QString &url, const QString &fileName);
    void finishCatalogAppInstall(const QString &archivePath);
    bool extractZip(const QString &archivePath, const QString &destination, QString *error) const;
    QStringList findExecutables(const QString &root) const;
    bool createQuiverHelperFiles(const QVariantMap &app, const QString &root, QString *error) const;
    QString safeFolderName(const QVariantMap &app) const;
    QString cacheFileName(const QString &url) const;
    QNetworkRequest makeRequest(const QUrl &url) const;
};
