#include "ProjectValidationScanner.h"

#include "../core/DocumentFile.h"
#include "../core/TherionFileTypes.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QSet>
#include <QTimer>
#include <QtConcurrent>

namespace TherionStudio
{
namespace
{
constexpr int kMaximumProjectValidationFindings = 2000;
constexpr qsizetype kMaximumValidatableFileBytes = 4 * 1024 * 1024;

QString canonicalOrAbsolutePath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

bool isValidatableTherionTextFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.isFile()) {
        return false;
    }

    if (isTherionConfigFileName(info.fileName())) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("th2");
}

bool shouldSkipDirectory(const QFileInfo &info)
{
    const QString name = info.fileName();
    return name == QStringLiteral(".git")
        || name == QStringLiteral(".svn")
        || name == QStringLiteral(".hg")
        || name == QStringLiteral("CMakeFiles")
        || name == QStringLiteral("build")
        || name.startsWith(QStringLiteral("cmake-build"));
}

void collectValidatableFiles(const QString &directoryPath, QVector<QString> *filePaths)
{
    if (filePaths == nullptr) {
        return;
    }

    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.isDir() || shouldSkipDirectory(directoryInfo)) {
        return;
    }

    const QFileInfoList entries = QDir(directoryPath).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            collectValidatableFiles(entry.absoluteFilePath(), filePaths);
        } else if (isValidatableTherionTextFile(entry.absoluteFilePath())) {
            filePaths->append(entry.absoluteFilePath());
        }
    }
}

QString readValidatableFileText(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (info.size() > kMaximumValidatableFileBytes) {
        return QString();
    }

    QString contents;
    if (!DocumentFile::readTextFile(filePath, &contents, nullptr, nullptr, nullptr)) {
        return QString();
    }
    return contents;
}

void appendFindingsForText(ProjectValidationScanner::Result *result,
                           const QString &filePath,
                           const QString &text,
                           const TherionSourceValidationCatalog &validationCatalog)
{
    if (result == nullptr) {
        return;
    }

    const TherionSourceValidationResult validation = TherionSourceValidator::validate(text, validationCatalog);
    const bool suppressUnknownCommandWarnings = isTherionConfigFilePath(filePath);
    for (const TherionSourceDiagnostic &diagnostic : validation.diagnostics) {
        if (suppressUnknownCommandWarnings && diagnostic.code == QStringLiteral("unknown-command")) {
            continue;
        }
        result->findings.append({filePath, diagnostic});
        if (result->findings.size() >= kMaximumProjectValidationFindings) {
            result->limitReached = true;
            return;
        }
    }
}

ProjectValidationScanner::Result performProjectValidation(const QString &projectRootPath,
                                                          const TherionSourceValidationCatalog &validationCatalog,
                                                          const QHash<QString, QString> &inMemoryProjectContentsByPath,
                                                          quint64 generation)
{
    ProjectValidationScanner::Result result;
    result.generation = generation;
    result.projectRootPath = projectRootPath;

    if (result.projectRootPath.trimmed().isEmpty() || !QDir(result.projectRootPath).exists()) {
        result.errorMessage = QObject::tr("Open a project before validating.");
        return result;
    }

    QSet<QString> searchedPaths;
    QVector<QString> filePaths;
    collectValidatableFiles(result.projectRootPath, &filePaths);
    for (const QString &candidatePath : filePaths) {
        const QString filePath = canonicalOrAbsolutePath(candidatePath);
        searchedPaths.insert(filePath);
        ++result.searchedFileCount;

        const auto memoryIt = inMemoryProjectContentsByPath.constFind(filePath);
        const QString text = memoryIt != inMemoryProjectContentsByPath.constEnd()
            ? *memoryIt
            : readValidatableFileText(filePath);
        appendFindingsForText(&result, filePath, text, validationCatalog);
        if (result.limitReached) {
            return result;
        }
    }

    for (auto it = inMemoryProjectContentsByPath.constBegin();
         it != inMemoryProjectContentsByPath.constEnd();
         ++it) {
        const QString filePath = canonicalOrAbsolutePath(it.key());
        if (searchedPaths.contains(filePath) || !isValidatableTherionTextFile(filePath)) {
            continue;
        }
        searchedPaths.insert(filePath);
        ++result.searchedFileCount;
        appendFindingsForText(&result, filePath, *it, validationCatalog);
        if (result.limitReached) {
            return result;
        }
    }

    return result;
}
}

ProjectValidationScanner::ProjectValidationScanner(QObject *parent)
    : QObject(parent)
    , debounceTimer_(new QTimer(this))
    , scanWatcher_(new QFutureWatcher<Result>(this))
{
    debounceTimer_->setSingleShot(true);
    debounceTimer_->setInterval(120);
    connect(debounceTimer_, &QTimer::timeout, this, &ProjectValidationScanner::startScan);
    connect(scanWatcher_, &QFutureWatcher<Result>::finished, this, &ProjectValidationScanner::handleScanFinished);
}

void ProjectValidationScanner::requestScan(const QString &projectRootPath,
                                           const TherionSourceValidationCatalog &validationCatalog,
                                           const QHash<QString, QString> &inMemoryProjectContentsByPath)
{
    pendingRequest_.projectRootPath = projectRootPath;
    pendingRequest_.validationCatalog = validationCatalog;
    pendingRequest_.inMemoryProjectContentsByPath = inMemoryProjectContentsByPath;
    hasPendingRequest_ = true;
    debounceTimer_->start();
}

void ProjectValidationScanner::setDebounceIntervalMs(int intervalMs)
{
    debounceTimer_->setInterval(intervalMs);
}

void ProjectValidationScanner::startScan()
{
    if (!hasPendingRequest_) {
        return;
    }

    if (scanWatcher_->isRunning()) {
        queuedScan_ = true;
        return;
    }

    const Request request = pendingRequest_;
    hasPendingRequest_ = false;
    const quint64 generation = ++generation_;

    auto future = QtConcurrent::run([request, generation]() {
        return performProjectValidation(request.projectRootPath,
                                        request.validationCatalog,
                                        request.inMemoryProjectContentsByPath,
                                        generation);
    });
    scanWatcher_->setFuture(future);
}

void ProjectValidationScanner::handleScanFinished()
{
    const Result result = scanWatcher_->result();
    emit validationFinished(result);

    if (queuedScan_) {
        queuedScan_ = false;
        startScan();
    }
}
}
