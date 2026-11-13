#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class ZBuildService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString report READ report NOTIFY reportChanged)
    Q_PROPERTY(QString scriptPath READ scriptPath CONSTANT)

public:
    explicit ZBuildService(QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QString report() const { return m_report; }
    QString scriptPath() const { return m_scriptPath; }

    Q_INVOKABLE bool buildGodot(const QString &projectPath,
                                const QString &outputPath,
                                const QString &targets,
                                const QString &godotBinary = QString());

signals:
    void busyChanged();
    void statusChanged();
    void reportChanged();

private:
    void setBusy(bool value);
    void setStatus(const QString &value);

    QProcess m_process;
    QString m_scriptPath;
    QString m_status;
    QString m_report;
    bool m_busy = false;
};
