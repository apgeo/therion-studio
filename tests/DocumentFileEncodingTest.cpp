#include "../src/core/DocumentFile.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool writeRawFile(const QString &filePath, const QByteArray &bytes)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(bytes) == bytes.size();
}

QByteArray readRawFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

int runUtf8DetectionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("utf8.th"));
    const QByteArray utf8Bytes = QStringLiteral("survey čau\nendsurvey\n").toUtf8();
    if (!expect(writeRawFile(filePath, utf8Bytes), "Failed to write UTF-8 fixture file.")) {
        return 1;
    }

    QString contents;
    QString encodingLabel;
    QString errorMessage;
    QStringConverter::Encoding encoding = QStringConverter::Latin1;
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encoding, &encodingLabel, &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("survey čau\nendsurvey\n"), "UTF-8 fixture decoded contents mismatch.")) {
        return 1;
    }
    if (!expect(encoding == QStringConverter::Utf8, "UTF-8 fixture did not report UTF-8 encoding.")) {
        return 1;
    }

    return 0;
}

int runNonUtf8RoundTripTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("latin1.th"));
    const QByteArray latin1Bytes("survey caf\xe9\nendsurvey\n");
    if (!expect(writeRawFile(filePath, latin1Bytes), "Failed to write non-UTF-8 fixture file.")) {
        return 1;
    }

    QString contents;
    QString encodingLabel;
    QString errorMessage;
    QStringConverter::Encoding encoding = QStringConverter::Utf8;
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encoding, &encodingLabel, &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("survey café\nendsurvey\n"), "Non-UTF-8 fixture decoded contents mismatch.")) {
        return 1;
    }
    if (!expect(encoding == QStringConverter::Latin1 || encoding == QStringConverter::System,
                "Non-UTF-8 fixture should decode as Latin1/System.")) {
        return 1;
    }

    if (!expect(DocumentFile::writeTextFile(filePath, contents, encoding, &errorMessage), qPrintable(errorMessage))) {
        return 1;
    }

    const QByteArray writtenBytes = readRawFile(filePath);
    if (!expect(writtenBytes == latin1Bytes, "Non-UTF-8 save did not preserve original byte encoding.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (const int utf8Result = runUtf8DetectionTest(); utf8Result != 0) {
        return utf8Result;
    }

    return runNonUtf8RoundTripTest();
}
