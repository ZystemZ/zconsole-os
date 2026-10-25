#include "Launcher.h"
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QRegularExpression>

bool Launcher::openUrl(const QString &url) {
    return QDesktopServices::openUrl(QUrl(url));
}

bool Launcher::launchCommand(const QString &command) {
    // MVP: exige comando explícito; nunca interpreta dados vindos de páginas web.
    if (command.trimmed().isEmpty()) return false;
    QString program = command.trimmed();
    QStringList args;
    // Permite somente um executável sem argumentos neste protótipo.
    if (program.contains(QRegularExpression("\\s"))) return false;
    return QProcess::startDetached(program, args);
}
