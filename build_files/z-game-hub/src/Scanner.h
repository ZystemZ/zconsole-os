#pragma once
#include <QObject>
#include <QFutureWatcher>
#include <QStringList>
#include <QVariantList>
#include <QSet>
#include <QTimer>

class GameCatalog;

class Scanner : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY candidatesChanged)
    Q_PROPERTY(QVariantList externalDevices READ externalDevices NOTIFY externalDevicesChanged)
public:
    explicit Scanner(GameCatalog *catalog, QObject *parent = nullptr);
    bool busy() const;
    QString status() const;
    QVariantList candidates() const;
    QVariantList externalDevices() const;
    Q_INVOKABLE void removeCandidate(int index);
    Q_INVOKABLE void scanCommonLocations();
    Q_INVOKABLE void scanDirectory(const QString &path);
    Q_INVOKABLE void scanExternalLaunchers();
    Q_INVOKABLE void scanSteamLibraries();
    Q_INVOKABLE void refreshExternalDevices();

signals:
    void busyChanged();
    void statusChanged();
    void scanFinished(int filesFound);
    void candidatesChanged();
    void externalDevicesChanged();
    void deviceConnected(const QString &path, const QString &label);
    void deviceDisconnected(const QString &path);

private:
    GameCatalog *m_catalog;
    bool m_busy = false;
    QString m_status = "Pronto";
    QVariantList m_candidates;
    QVariantList m_externalDevices;
    QSet<QString> m_knownDevicePaths;
    QTimer m_deviceTimer;
    void setStatus(const QString &s);
    void setBusy(bool b);
    void scanRoots(const QStringList &roots);
    bool isExternalDevicePath(const QString &path) const;
};
