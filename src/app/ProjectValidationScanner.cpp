#include "ProjectValidationScanner.h"

#include "../core/DocumentFile.h"
#include "../core/ProjectStructureIndex.h"
#include "../core/TherionFileTypes.h"
#include "../core/TherionSourceLogicalDocument.h"
#include "../core/TherionSourceReferenceResolver.h"

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

bool hasValidatableTherionTextFileName(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (isTherionConfigFileName(info.fileName())) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("th2");
}

bool isValidatableTherionTextFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    return info.isFile() && hasValidatableTherionTextFileName(filePath);
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

TherionSourceDiagnostic diagnosticForProjectIndexDiagnostic(const ProjectIndexDiagnostic &indexDiagnostic)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
    diagnostic.lineNumber = indexDiagnostic.lineNumber;
    diagnostic.columnNumber = qMax(1, indexDiagnostic.columnNumber);
    diagnostic.columnLength = indexDiagnostic.columnLength > 0
        ? indexDiagnostic.columnLength
        : indexDiagnostic.referencedName.size();
    diagnostic.currentText = indexDiagnostic.referencedName;

    switch (indexDiagnostic.kind) {
    case ProjectIndexDiagnosticKind::UnknownMapScrapReference:
        diagnostic.code = QStringLiteral("unknown-map-scrap-reference");
        diagnostic.title = QObject::tr("Unknown map scrap reference");
        diagnostic.message = QObject::tr("Map references scrap `%1`, but no matching scrap was found in the project index.")
                                 .arg(indexDiagnostic.referencedName);
        break;
    case ProjectIndexDiagnosticKind::UnknownMapReference:
        diagnostic.code = QStringLiteral("unknown-map-reference");
        diagnostic.title = QObject::tr("Unknown map reference");
        diagnostic.message = QObject::tr("Map references child map `%1`, but no matching map was found in the project index.")
                                 .arg(indexDiagnostic.referencedName);
        break;
    case ProjectIndexDiagnosticKind::AmbiguousMapScrapReference:
        diagnostic.code = QStringLiteral("ambiguous-map-scrap-reference");
        diagnostic.title = QObject::tr("Ambiguous map scrap reference");
        diagnostic.message = QObject::tr("Map scrap reference `%1` matches %2 scraps in the project index.")
                                 .arg(indexDiagnostic.referencedName)
                                 .arg(indexDiagnostic.candidateCount);
        break;
    case ProjectIndexDiagnosticKind::AmbiguousMapReference:
        diagnostic.code = QStringLiteral("ambiguous-map-reference");
        diagnostic.title = QObject::tr("Ambiguous map reference");
        diagnostic.message = QObject::tr("Map reference `%1` matches %2 maps in the project index.")
                                 .arg(indexDiagnostic.referencedName)
                                 .arg(indexDiagnostic.candidateCount);
        break;
    case ProjectIndexDiagnosticKind::MixedMapAndScrapReferences:
        diagnostic.code = QStringLiteral("mixed-map-and-scrap-references");
        diagnostic.severity = TherionSourceDiagnosticSeverity::Warning;
        diagnostic.title = QObject::tr("Mixed map and scrap references");
        diagnostic.message = QObject::tr("Map composition mixes child map and scrap references; `%1` changes the content kind.")
                                 .arg(indexDiagnostic.referencedName);
        break;
    case ProjectIndexDiagnosticKind::UnknownJoinReference:
        diagnostic.code = QStringLiteral("unknown-join-reference");
        diagnostic.title = QObject::tr("Unknown join reference");
        diagnostic.message = QObject::tr("Join references `%1`, but no matching scrap, line, or point was found in the project index.")
                                 .arg(indexDiagnostic.referencedName);
        break;
    case ProjectIndexDiagnosticKind::AmbiguousJoinReference:
        diagnostic.code = QStringLiteral("ambiguous-join-reference");
        diagnostic.title = QObject::tr("Ambiguous join reference");
        diagnostic.message = QObject::tr("Join reference `%1` matches %2 objects in the project index.")
                                 .arg(indexDiagnostic.referencedName)
                                 .arg(indexDiagnostic.candidateCount);
        break;
    }

    return diagnostic;
}

void appendProjectIndexFindings(ProjectValidationScanner::Result *result,
                                const QString &projectRootPath,
                                const QHash<QString, QString> &inMemoryProjectContentsByPath)
{
    if (result == nullptr || result->limitReached) {
        return;
    }

    QString indexErrorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectRootPath,
                                                                                  inMemoryProjectContentsByPath,
                                                                                  &indexErrorMessage);
    if (!indexErrorMessage.isEmpty()) {
        TherionSourceDiagnostic diagnostic;
        diagnostic.code = QStringLiteral("project-index-unavailable");
        diagnostic.severity = TherionSourceDiagnosticSeverity::Warning;
        diagnostic.lineNumber = 1;
        diagnostic.columnNumber = 1;
        diagnostic.columnLength = 1;
        diagnostic.title = QObject::tr("Project index unavailable");
        diagnostic.message = indexErrorMessage;
        diagnostic.currentText = projectRootPath;
        result->findings.append({projectRootPath, diagnostic});
        if (result->findings.size() >= kMaximumProjectValidationFindings) {
            result->limitReached = true;
        }
        return;
    }

    for (const ProjectIndexDiagnostic &indexDiagnostic : snapshot.diagnostics) {
        if (indexDiagnostic.sourceFile.trimmed().isEmpty()) {
            continue;
        }

        result->findings.append({indexDiagnostic.sourceFile, diagnosticForProjectIndexDiagnostic(indexDiagnostic)});
        if (result->findings.size() >= kMaximumProjectValidationFindings) {
            result->limitReached = true;
            return;
        }
    }
}

void appendFindingsForText(ProjectValidationScanner::Result *result,
                           const QString &filePath,
                           const QString &text,
                           const TherionSourceValidationCatalog &validationCatalog,
                           const QSet<QString> &knownProjectFilePaths)
{
    if (result == nullptr) {
        return;
    }

    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = therionSourceDocumentTypeForFilePath(filePath);

    const TherionSourceValidationResult validation =
        TherionSourceValidator::validate(text, validationCatalog, metadata);
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

    const TherionSourceLogicalDocument logicalDocument = TherionSourceLogicalDocument::fromText(text, metadata);
    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        QString commandName = command.metadata.commandName;
        if (commandName.isEmpty()) {
            commandName = command.normalizedDirective;
        }
        if (commandName.isEmpty()) {
            commandName = command.parsed.directive;
        }
        if (commandName != QStringLiteral("input") && commandName != QStringLiteral("source")) {
            continue;
        }

        const QString referencedPath = command.parsed.tokens.value(1).trimmed();
        if (referencedPath.isEmpty()
            || !resolveTherionSourceReferencePath(filePath, referencedPath, knownProjectFilePaths).isEmpty()) {
            continue;
        }

        TherionSourceDiagnostic diagnostic;
        diagnostic.code = QStringLiteral("missing-source-reference");
        diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
        diagnostic.lineNumber = command.startLineNumber;
        diagnostic.columnNumber = 1;
        diagnostic.columnLength = command.text.size();
        diagnostic.title = QObject::tr("Missing referenced source file");
        diagnostic.message = QObject::tr("Command `%1` references `%2`, but no matching project file was found.")
                                 .arg(commandName, referencedPath);
        diagnostic.currentText = command.text;
        TherionSourcePhysicalRange tokenRange;
        if (command.physicalRangeForTokenIndex(1, &tokenRange)) {
            diagnostic.lineNumber = tokenRange.lineNumber;
            diagnostic.columnNumber = tokenRange.columnNumber;
            diagnostic.columnLength = tokenRange.columnLength;
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

    QHash<QString, QString> normalizedInMemoryProjectContentsByPath;
    for (auto it = inMemoryProjectContentsByPath.constBegin();
         it != inMemoryProjectContentsByPath.constEnd();
         ++it) {
        const QString normalizedPath = canonicalOrAbsoluteFilePath(it.key());
        if (!normalizedPath.isEmpty()) {
            normalizedInMemoryProjectContentsByPath.insert(normalizedPath, *it);
        }
    }

    QSet<QString> searchedPaths;
    QVector<QString> filePaths;
    collectValidatableFiles(result.projectRootPath, &filePaths);
    QSet<QString> knownProjectFilePaths;
    for (const QString &candidatePath : filePaths) {
        knownProjectFilePaths.insert(canonicalOrAbsoluteFilePath(candidatePath));
    }
    for (auto it = normalizedInMemoryProjectContentsByPath.constBegin();
         it != normalizedInMemoryProjectContentsByPath.constEnd();
         ++it) {
        knownProjectFilePaths.insert(it.key());
    }

    for (const QString &candidatePath : filePaths) {
        const QString filePath = canonicalOrAbsoluteFilePath(candidatePath);
        searchedPaths.insert(filePath);
        ++result.searchedFileCount;

        const auto memoryIt = normalizedInMemoryProjectContentsByPath.constFind(filePath);
        const QString text = memoryIt != normalizedInMemoryProjectContentsByPath.constEnd()
            ? *memoryIt
            : readValidatableFileText(filePath);
        appendFindingsForText(&result, filePath, text, validationCatalog, knownProjectFilePaths);
        if (result.limitReached) {
            return result;
        }
    }

    for (auto it = normalizedInMemoryProjectContentsByPath.constBegin();
         it != normalizedInMemoryProjectContentsByPath.constEnd();
         ++it) {
        const QString filePath = it.key();
        if (searchedPaths.contains(filePath) || !hasValidatableTherionTextFileName(filePath)) {
            continue;
        }
        searchedPaths.insert(filePath);
        ++result.searchedFileCount;
        appendFindingsForText(&result, filePath, *it, validationCatalog, knownProjectFilePaths);
        if (result.limitReached) {
            return result;
        }
    }

    appendProjectIndexFindings(&result, result.projectRootPath, normalizedInMemoryProjectContentsByPath);

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
    if (!debounceTimer_->isActive()) {
        debounceTimer_->start();
    }
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
    emit validationStarted(generation, request.projectRootPath);

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
    const bool hasSupersedingRequest = queuedScan_ || hasPendingRequest_;
    emit validationFinished(result);

    if (hasSupersedingRequest) {
        queuedScan_ = false;
        debounceTimer_->stop();
        startScan();
    }
}
}
