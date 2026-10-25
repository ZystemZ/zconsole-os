#pragma once
#include <QObject>
#include <QString>

class Launcher : public QObject {
    Q_OBJECT
public:
    explicit Launcher(QObject *parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE bool openUrl(const QString &url);
    Q_INVOKABLE bool launchCommand(const QString &command);
};
