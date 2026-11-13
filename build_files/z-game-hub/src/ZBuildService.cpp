#include "ZBuildService.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

ZBuildService::ZBuildService(QObject *parent)
    : QObject(parent),
      m_scriptPath(QStringLiteral("/usr/share/zconsole/sdk/zbuild-godot.py")) {
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        const QString chunk = QString::fromUtf8(m_process.readAllStandardOutput());
        m_report += chunk;
        emit reportChanged();
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        const QString chunk = QString::fromUtf8(m_process.readAllStandardError());
        m_report += chunk;
        emit reportChanged();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setStatus(QStringLiteral("Não foi possível iniciar o ZBuild. Verifique Python 3 e a instalação do SDK."));
            setBusy(false);
        }
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        m_report += QStringLiteral("\n[ZBuild] Processo finalizado: código %1.\n").arg(exitCode);
        emit reportChanged();
        setBusy(false);
        if (exitStatus == QProcess::NormalExit && exitCode == 0)
            setStatus(QStringLiteral("Exportação concluída. O cartucho ZCARD foi gerado."));
        else
            setStatus(QStringLiteral("A exportação falhou. Consulte o relatório para identificar o problema."));
    });
}

void ZBuildService::setBusy(bool value) {
    if (m_busy == value) return;
    m_busy = value;
    emit busyChanged();
}

void ZBuildService::setStatus(const QString &value) {
    if (m_status == value) return;
    m_status = value;
    emit statusChanged();
}

bool ZBuildService::buildGodot(const QString &projectPath,
                               const QString &outputPath,
                               const QString &targets,
                               const QString &godotBinary) {
    if (m_busy) return false;

    const QString projectInput = projectPath.trimmed();
    const QString outputInput = outputPath.trimmed();
    if (projectInput.isEmpty() || outputInput.isEmpty()) {
        setStatus(QStringLiteral("Selecione a pasta do projeto Godot e o arquivo ZCARD de saída."));
        return false;
    }
    const QString project = QDir::cleanPath(projectInput);
    QString output = QDir::cleanPath(outputInput);
    if (!output.endsWith(QStringLiteral(".zcard"), Qt::CaseInsensitive))
        output += QStringLiteral(".zcard");
    if (!QFileInfo(project + QStringLiteral("/project.godot")).isFile()) {
        setStatus(QStringLiteral("A pasta selecionada não contém project.godot."));
        return false;
    }
    if (!QFileInfo(m_scriptPath).isFile()) {
        setStatus(QStringLiteral("ZBuild não está instalado em %1.").arg(m_scriptPath));
        return false;
    }
    const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (python.isEmpty()) {
        setStatus(QStringLiteral("Python 3 não foi encontrado no sistema."));
        return false;
    }

    QStringList arguments{
        m_scriptPath,
        QStringLiteral("--project"), project,
        QStringLiteral("--output"), output,
        QStringLiteral("--targets"), targets.trimmed().isEmpty() ? QStringLiteral("dual") : targets.trimmed()
    };
    if (!godotBinary.trimmed().isEmpty()) {
        arguments << QStringLiteral("--godot") << godotBinary.trimmed();
    }

    m_report.clear();
    emit reportChanged();
    setStatus(QStringLiteral("Preparando exportação Godot…"));
    setBusy(true);
    m_process.setProgram(python);
    m_process.setArguments(arguments);
    m_process.setWorkingDirectory(QFileInfo(project).absoluteFilePath());
    m_process.start();
    return true;
}
