#include "DocumentFile.h"

#include <QFile>
#include <QFileInfo>

namespace TherionStudio
{
bool DocumentFile::readUtf8TextFile(const QString &filePath, QString *contents, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to open %1 for reading.").arg(QFileInfo(filePath).fileName());
        }
        return false;
    }

    const QByteArray data = file.readAll();
    if (contents != nullptr) {
        *contents = QString::fromUtf8(data);
    }

    return true;
}

bool DocumentFile::writeUtf8TextFile(const QString &filePath, const QString &contents, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to open %1 for writing.").arg(QFileInfo(filePath).fileName());
        }
        return false;
    }

    if (file.write(contents.toUtf8()) == -1) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to write %1.").arg(QFileInfo(filePath).fileName());
        }
        return false;
    }

    return true;
}
}
