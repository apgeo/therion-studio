#include "../src/core/DocumentFile.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
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

int runDirectiveEncodingRoundTripTest(const QString &codecName,
                                      const QString &directiveToken,
                                      const QString &sourceText,
                                      const QString &fileName,
                                      const QString &expectedEncodingFragment,
                                      const char *codecMissingMessage,
                                      const char *encodeFailureMessage,
                                      const char *writeFixtureFailureMessage,
                                      const char *contentMismatchMessage,
                                      const char *encodingMismatchMessage,
                                      const char *saveFailureMessage,
                                      const char *roundTripMismatchMessage)
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(fileName);
    const QString prefixedText = QStringLiteral("encoding %1\n").arg(directiveToken) + sourceText;

    QStringEncoder encoder(codecName, QStringConverter::Flag::Default);
    if (!expect(encoder.isValid(), codecMissingMessage)) {
        return 1;
    }

    const QByteArray encodedBytes = encoder.encode(prefixedText);
    if (!expect(!encoder.hasError(), encodeFailureMessage)) {
        return 1;
    }
    if (!expect(writeRawFile(filePath, encodedBytes), writeFixtureFailureMessage)) {
        return 1;
    }

    QString contents;
    QString encodingName;
    QString encodingLabel;
    QString errorMessage;
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encodingName, &encodingLabel, &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(contents == prefixedText, contentMismatchMessage)) {
        return 1;
    }
    if (!expect(encodingName.contains(expectedEncodingFragment, Qt::CaseInsensitive), encodingMismatchMessage)) {
        return 1;
    }

    if (!expect(DocumentFile::writeTextFile(filePath, contents, encodingName, &errorMessage), saveFailureMessage)) {
        return 1;
    }

    const QByteArray writtenBytes = readRawFile(filePath);
    if (!expect(writtenBytes == encodedBytes, roundTripMismatchMessage)) {
        return 1;
    }

    return 0;
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
    QString encodingName = QStringLiteral("latin1");
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encodingName, &encodingLabel, &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("survey čau\nendsurvey\n"), "UTF-8 fixture decoded contents mismatch.")) {
        return 1;
    }
    if (!expect(encodingName.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0,
                "UTF-8 fixture did not report UTF-8 encoding.")) {
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
    QString encodingName = QStringLiteral("UTF-8");
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encodingName, &encodingLabel, &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("survey café\nendsurvey\n"), "Non-UTF-8 fixture decoded contents mismatch.")) {
        return 1;
    }
    if (!expect(!encodingName.trimmed().isEmpty(), "Non-UTF-8 fixture should report a non-empty encoding name.")) {
        return 1;
    }

    if (!expect(DocumentFile::writeTextFile(filePath, contents, encodingName, &errorMessage), qPrintable(errorMessage))) {
        return 1;
    }

    const QByteArray writtenBytes = readRawFile(filePath);
    if (!expect(writtenBytes == latin1Bytes, "Non-UTF-8 save did not preserve original byte encoding.")) {
        return 1;
    }

    return 0;
}

int runWindows1250DirectiveRoundTripTest()
{
    return runDirectiveEncodingRoundTripTest(QStringLiteral("windows-1250"),
                                             QStringLiteral("windows-1250"),
                                             QStringLiteral("survey žluťoučký\nendsurvey\n"),
                                             QStringLiteral("cp1250.th"),
                                             QStringLiteral("1250"),
                                             "windows-1250 codec is not available in this Qt runtime.",
                                             "Failed to encode windows-1250 fixture text.",
                                             "Failed to write windows-1250 fixture file.",
                                             "windows-1250 fixture decoded contents mismatch.",
                                             "windows-1250 fixture did not preserve codec name from directive.",
                                             "windows-1250 save failed.",
                                             "windows-1250 save did not preserve original byte encoding.");
}

int runCp1250DirectiveAliasRoundTripTest()
{
    return runDirectiveEncodingRoundTripTest(QStringLiteral("windows-1250"),
                                             QStringLiteral("cp1250"),
                                             QStringLiteral("survey žluťoučký\nendsurvey\n"),
                                             QStringLiteral("cp1250-alias.th"),
                                             QStringLiteral("1250"),
                                             "windows-1250 codec is not available in this Qt runtime.",
                                             "Failed to encode cp1250 alias fixture text.",
                                             "Failed to write cp1250 alias fixture file.",
                                             "cp1250 alias fixture decoded contents mismatch.",
                                             "cp1250 alias fixture did not resolve to a 1250-family codec.",
                                             "cp1250 alias save failed.",
                                             "cp1250 alias save did not preserve original byte encoding.");
}

int runWindows1252DirectiveRoundTripTest()
{
    return runDirectiveEncodingRoundTripTest(QStringLiteral("windows-1252"),
                                             QStringLiteral("windows-1252"),
                                             QStringLiteral("survey café déjà vu\nendsurvey\n"),
                                             QStringLiteral("cp1252.th"),
                                             QStringLiteral("1252"),
                                             "windows-1252 codec is not available in this Qt runtime.",
                                             "Failed to encode windows-1252 fixture text.",
                                             "Failed to write windows-1252 fixture file.",
                                             "windows-1252 fixture decoded contents mismatch.",
                                             "windows-1252 fixture did not preserve codec name from directive.",
                                             "windows-1252 save failed.",
                                             "windows-1252 save did not preserve original byte encoding.");
}
}

int main()
{
    if (const int utf8Result = runUtf8DetectionTest(); utf8Result != 0) {
        return utf8Result;
    }

    if (const int nonUtfResult = runNonUtf8RoundTripTest(); nonUtfResult != 0) {
        return nonUtfResult;
    }

    if (const int cp1250Result = runWindows1250DirectiveRoundTripTest(); cp1250Result != 0) {
        return cp1250Result;
    }

    if (const int cp1250AliasResult = runCp1250DirectiveAliasRoundTripTest(); cp1250AliasResult != 0) {
        return cp1250AliasResult;
    }

    return runWindows1252DirectiveRoundTripTest();
}
