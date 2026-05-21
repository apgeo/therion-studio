#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
class TherionRunnerService final : public QObject
{
    Q_OBJECT

public:
    enum class StartCode
    {
        Started,
        AlreadyRunning,
        MissingWorkingDirectory,
        WorkingDirectoryNotFound,
        ExecutableNotFound
    };

    struct StartResult
    {
        StartCode code = StartCode::Started;
        QString resolvedExecutablePath;
        bool usedHomebrewFallback = false;
    };

    explicit TherionRunnerService(QObject *parent = nullptr);

    static QString suggestedDefaultExecutablePath();
    bool isRunning() const;
    StartResult start(const QString &executableInput,
                      const QString &workingDirectory,
                      const QStringList &arguments);
    void stop();
    QString errorString() const;

signals:
    void standardOutputReady(const QString &output);
    void standardErrorReady(const QString &output);
    void runFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void runError(const QString &errorText);
    void runningStateChanged(bool running);

private slots:
    void handleReadyReadStandardOutput();
    void handleReadyReadStandardError();
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleError(QProcess::ProcessError error);

private:
    struct ResolvedExecutablePath
    {
        QString path;
        bool usedHomebrewFallback = false;
    };

    ResolvedExecutablePath resolveExecutablePath(const QString &executableInput,
                                                 const QString &workingDirectory) const;
    static QString detectHomebrewTherionExecutablePath();

    QProcess process_;
};
}
