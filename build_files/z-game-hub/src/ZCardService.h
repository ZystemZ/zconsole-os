#pragma once

#include <QObject>
#include <QVariantList>
#include <QString>
#include <QThread>

class ZCardService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY cardChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY cardChanged)
    Q_PROPERTY(QString currentFormat READ currentFormat NOTIFY cardChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString report READ report NOTIFY reportChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList cards READ cards NOTIFY cardsChanged)
public:
    explicit ZCardService(QObject *parent = nullptr);
    ~ZCardService() override;

    QVariantList entries() const { return m_entries; }
    QString currentPath() const { return m_currentPath; }
    QString currentFormat() const { return m_currentFormat; }
    QString status() const { return m_status; }
    QString report() const { return m_report; }
    bool busy() const { return m_busy; }
    QVariantList cards() const { return m_cards; }

    Q_INVOKABLE bool openCard(const QString &path);
    Q_INVOKABLE void clearCard();
    Q_INVOKABLE bool buildCard(const QString &source, const QString &output, const QString &format);
    Q_INVOKABLE QVariantMap resolveLaunchPlan();
    Q_INVOKABLE void showMessage(const QString &message);
    Q_INVOKABLE void scanCards();
    Q_INVOKABLE bool registerCard(const QString &path);

signals:
    void cardChanged();
    void statusChanged();
    void reportChanged();
    void busyChanged();
    void cardsChanged();

private:
    QVariantList m_entries;
    QString m_currentPath;
    QString m_currentFormat;
    QString m_status;
    QString m_report;
    QThread *m_worker = nullptr;
    bool m_busy = false;
    QVariantList m_cards;

    bool parseDirectory(const QString &path, QString *error);
    bool parseArchive(const QByteArray &bytes, qsizetype baseOffset, qsizetype length, QString *error);
    bool parseZcf(const QByteArray &bytes, QString *error);
    bool parseZuf(const QByteArray &bytes, QString *error);
    bool readCardAsset(const QString &entry, QByteArray *data, QString *error) const;
    QVariantMap cardInfo(const QString &path);
    void setStatus(const QString &value);
    void resetEntries();
};
