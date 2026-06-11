#include "ProjectTemplateService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace TherionStudio
{
namespace
{
struct TemplateManifest
{
    QString targetConfig;
    QStringList openFiles;
    QStringList directories;
    QStringList files;
};

ProjectTemplateService::CreateProjectResult failedResult(const QString &projectRootPath,
                                                         const QString &errorMessage)
{
    ProjectTemplateService::CreateProjectResult result;
    result.projectRootPath = projectRootPath;
    result.errorMessage = errorMessage;
    return result;
}

QString templatePath(const QString &rootPath, const QString &relativePath)
{
    return rootPath + QLatin1Char('/') + relativePath;
}

bool isSafeRelativePath(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return false;
    }
    const QString cleaned = QDir::cleanPath(path);
    return cleaned == path
        && !QDir::isAbsolutePath(cleaned)
        && cleaned != QStringLiteral(".")
        && cleaned != QStringLiteral("..")
        && !cleaned.startsWith(QStringLiteral("../"))
        && !cleaned.contains(QStringLiteral("/../"));
}

QStringList stringArray(const QJsonObject &object, const QString &key)
{
    QStringList values;
    const QJsonArray array = object.value(key).toArray();
    for (const QJsonValue &value : array) {
        if (value.isString()) {
            values.append(value.toString());
        }
    }
    return values;
}

bool readManifest(const QString &templateRootPath, TemplateManifest *manifest, QString *errorMessage)
{
    QFile manifestFile(templatePath(templateRootPath, QStringLiteral("template.json")));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        *errorMessage = QStringLiteral("Template manifest could not be opened.");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *errorMessage = QStringLiteral("Template manifest is not valid JSON.");
        return false;
    }

    const QJsonObject rootObject = document.object();
    if (rootObject.value(QStringLiteral("schemaVersion")).toInt() != 1) {
        *errorMessage = QStringLiteral("Template manifest has an unsupported schema version.");
        return false;
    }

    manifest->targetConfig = rootObject.value(QStringLiteral("targetConfig")).toString();
    manifest->openFiles = stringArray(rootObject, QStringLiteral("openFiles"));
    manifest->directories = stringArray(rootObject, QStringLiteral("directories"));
    manifest->files = stringArray(rootObject, QStringLiteral("files"));
    if (manifest->files.isEmpty()) {
        *errorMessage = QStringLiteral("Template manifest does not list any files.");
        return false;
    }

    const QStringList paths = manifest->files
        + manifest->openFiles
        + manifest->directories
        + QStringList{manifest->targetConfig};
    for (const QString &path : paths) {
        if (!isSafeRelativePath(path)) {
            *errorMessage = QStringLiteral("Template manifest contains an unsafe relative path.");
            return false;
        }
    }

    if (!manifest->files.contains(manifest->targetConfig)) {
        *errorMessage = QStringLiteral("Template target config is not listed as a template file.");
        return false;
    }
    for (const QString &openFile : manifest->openFiles) {
        if (!manifest->files.contains(openFile)) {
            *errorMessage = QStringLiteral("Template open file is not listed as a template file.");
            return false;
        }
    }

    return true;
}

bool hasDirectoryEntries(const QDir &directory)
{
    return !directory.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty();
}

bool copyTemplateFile(const QString &sourcePath, const QString &targetPath, QString *errorMessage)
{
    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        *errorMessage = QStringLiteral("Template file could not be opened: %1").arg(sourcePath);
        return false;
    }

    const QFileInfo targetInfo(targetPath);
    if (!QDir().mkpath(targetInfo.absolutePath())) {
        *errorMessage = QStringLiteral("Target directory could not be created: %1")
                            .arg(targetInfo.absolutePath());
        return false;
    }

    if (QFileInfo::exists(targetPath)) {
        *errorMessage = QStringLiteral("Target file already exists: %1").arg(targetPath);
        return false;
    }

    QSaveFile targetFile(targetPath);
    if (!targetFile.open(QIODevice::WriteOnly)) {
        *errorMessage = QStringLiteral("Target file could not be opened: %1").arg(targetPath);
        return false;
    }
    if (targetFile.write(sourceFile.readAll()) < 0 || !targetFile.commit()) {
        *errorMessage = QStringLiteral("Target file could not be written: %1").arg(targetPath);
        return false;
    }

    return true;
}
}

QString ProjectTemplateService::defaultTemplateResourcePath()
{
    return QStringLiteral(":/resources/project_templates/default");
}

ProjectTemplateService::CreateProjectResult
ProjectTemplateService::createProjectFromTemplate(const QString &templateRootPath,
                                                  const QString &targetProjectPath)
{
    const QString cleanTargetPath = QDir::cleanPath(targetProjectPath.trimmed());
    if (cleanTargetPath.isEmpty() || cleanTargetPath == QStringLiteral(".")) {
        return failedResult(cleanTargetPath, QStringLiteral("Choose a target project folder."));
    }

    TemplateManifest manifest;
    QString errorMessage;
    if (!readManifest(templateRootPath, &manifest, &errorMessage)) {
        return failedResult(cleanTargetPath, errorMessage);
    }

    QDir targetDirectory(cleanTargetPath);
    if (targetDirectory.exists() && hasDirectoryEntries(targetDirectory)) {
        return failedResult(cleanTargetPath, QStringLiteral("The target project folder must be empty."));
    }
    if (!targetDirectory.exists() && !QDir().mkpath(cleanTargetPath)) {
        return failedResult(cleanTargetPath, QStringLiteral("The target project folder could not be created."));
    }

    QStringList createdFiles;
    QStringList createdDirectories;
    for (const QString &relativePath : manifest.directories) {
        const QString targetPath = targetDirectory.filePath(relativePath);
        if (!QDir().mkpath(targetPath)) {
            return failedResult(cleanTargetPath,
                                QStringLiteral("Target directory could not be created: %1")
                                    .arg(targetPath));
        }
        createdDirectories.prepend(targetPath);
    }

    for (const QString &relativePath : manifest.files) {
        const QString targetPath = targetDirectory.filePath(relativePath);
        if (!copyTemplateFile(templatePath(templateRootPath, relativePath), targetPath, &errorMessage)) {
            for (const QString &createdFile : createdFiles) {
                QFile::remove(createdFile);
            }
            for (const QString &createdDirectory : createdDirectories) {
                QDir().rmdir(createdDirectory);
            }
            return failedResult(cleanTargetPath, errorMessage);
        }
        createdFiles.append(targetPath);
    }

    CreateProjectResult result;
    result.success = true;
    result.projectRootPath = QFileInfo(cleanTargetPath).absoluteFilePath();
    result.targetConfigPath = targetDirectory.filePath(manifest.targetConfig);
    for (const QString &openFile : manifest.openFiles) {
        result.openFilePaths.append(targetDirectory.filePath(openFile));
    }
    return result;
}
}
