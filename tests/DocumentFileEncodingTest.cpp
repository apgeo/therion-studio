#include "../src/core/DocumentFile.h"
#include "../src/core/TherionDocumentEditor.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QTemporaryDir>

#include <functional>
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

bool applyPlannerEdits(QString *contents,
                       const std::function<bool(const QString &, QVector<TherionSourceTextEdit> *, QString *)> &planner,
                       QString *errorMessage)
{
    if (contents == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No document contents are available.");
        }
        return false;
    }

    QVector<TherionSourceTextEdit> edits;
    if (!planner(*contents, &edits, errorMessage)) {
        return false;
    }
    return TherionDocumentEditor::applySourceTextEdits(contents, edits, errorMessage);
}

bool rewriteStructureEntryName(QString *contents,
                               int lineNumber,
                               const QString &category,
                               const QString &newName,
                               QString *errorMessage)
{
    return applyPlannerEdits(contents, [&](const QString &source, QVector<TherionSourceTextEdit> *edits, QString *plannerError) {
        return TherionDocumentEditor::structureEntryNameRewriteEdits(source, lineNumber, category, newName, edits, plannerError);
    }, errorMessage);
}

bool rewriteLineOptionToggle(QString *contents, int lineNumber, const QString &optionName, bool enabled, QString *errorMessage)
{
    return applyPlannerEdits(contents, [&](const QString &source, QVector<TherionSourceTextEdit> *edits, QString *plannerError) {
        return TherionDocumentEditor::lineOptionToggleRewriteEdits(source, lineNumber, optionName, enabled, edits, plannerError);
    }, errorMessage);
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

QByteArray encodingNameBytes(const QString &encodingName)
{
    return encodingName.trimmed().toLatin1();
}

QStringEncoder makeEncoder(const QString &encodingName,
                           QStringConverter::Flags flags = QStringConverter::Flag::Default)
{
    const QByteArray nameBytes = encodingNameBytes(encodingName);
    return QStringEncoder(nameBytes.constData(), flags);
}

void printOptionalCodecSkip(const char *message)
{
    std::cerr << "Skipping optional codec test: " << message << '\n';
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
                                      const char *roundTripMismatchMessage,
                                      QStringConverter::Flags encoderFlags = QStringConverter::Flag::Default)
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(fileName);
    const QString prefixedText = QStringLiteral("encoding %1\n").arg(directiveToken) + sourceText;

    QStringEncoder encoder = makeEncoder(codecName, encoderFlags);
    if (!encoder.isValid()) {
        printOptionalCodecSkip(codecMissingMessage);
        return 0;
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
    QString errorMessage;
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encodingName, nullptr, &errorMessage),
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
                                             "windows-1250 save did not preserve original byte encoding.",
                                             QStringConverter::Flag::Default);
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
                                             "cp1250 alias save did not preserve original byte encoding.",
                                             QStringConverter::Flag::Default);
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
                                             "windows-1252 save did not preserve original byte encoding.",
                                             QStringConverter::Flag::Default);
}

int runCp1252DirectiveAliasRoundTripTest()
{
    return runDirectiveEncodingRoundTripTest(QStringLiteral("windows-1252"),
                                             QStringLiteral("cp1252"),
                                             QStringLiteral("survey café déjà vu\nendsurvey\n"),
                                             QStringLiteral("cp1252-alias.th"),
                                             QStringLiteral("1252"),
                                             "windows-1252 codec is not available in this Qt runtime.",
                                             "Failed to encode cp1252 alias fixture text.",
                                             "Failed to write cp1252 alias fixture file.",
                                             "cp1252 alias fixture decoded contents mismatch.",
                                             "cp1252 alias fixture did not resolve to a 1252-family codec.",
                                             "cp1252 alias save failed.",
                                             "cp1252 alias save did not preserve original byte encoding.",
                                             QStringConverter::Flag::Default);
}

int runLatin2DirectiveRoundTripTest()
{
    return runDirectiveEncodingRoundTripTest(QStringLiteral("iso-8859-2"),
                                             QStringLiteral("latin2"),
                                             QStringLiteral("survey žluťoučký\nendsurvey\n"),
                                             QStringLiteral("latin2.th"),
                                             QStringLiteral("8859"),
                                             "iso-8859-2 codec is not available in this Qt runtime.",
                                             "Failed to encode latin2 fixture text.",
                                             "Failed to write latin2 fixture file.",
                                             "latin2 fixture decoded contents mismatch.",
                                             "latin2 fixture did not resolve to an 8859-family codec.",
                                             "latin2 save failed.",
                                             "latin2 save did not preserve original byte encoding.",
                                             QStringConverter::Flag::Default);
}

int runUndeclaredLatin2RoundTripTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("undeclared-latin2.th2"));
    const QString sourceText = QStringLiteral(
        "scrap žluťoučký\n"
        "endscrap\n");

    QStringEncoder encoder = makeEncoder(QStringLiteral("iso-8859-2"));
    if (!encoder.isValid()) {
        printOptionalCodecSkip("iso-8859-2 codec is not available in this Qt runtime.");
        return 0;
    }

    const QByteArray encodedBytes = encoder.encode(sourceText);
    if (!expect(!encoder.hasError(), "Failed to encode undeclared latin2 fixture text.")) {
        return 1;
    }
    if (!expect(writeRawFile(filePath, encodedBytes), "Failed to write undeclared latin2 fixture file.")) {
        return 1;
    }

    QString contents;
    QString encodingName;
    QString errorMessage;
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encodingName, nullptr, &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }
    if (!expect(contents == sourceText, "Undeclared latin2 fixture decoded contents mismatch.")) {
        return 1;
    }
    if (!expect(encodingName.contains(QStringLiteral("8859"), Qt::CaseInsensitive),
                "Undeclared latin2 fixture did not resolve to an ISO-8859-family codec.")) {
        return 1;
    }

    if (!expect(DocumentFile::writeTextFile(filePath, contents, encodingName, &errorMessage), qPrintable(errorMessage))) {
        return 1;
    }

    const QByteArray writtenBytes = readRawFile(filePath);
    if (!expect(writtenBytes == encodedBytes, "Undeclared latin2 save did not preserve original byte encoding.")) {
        return 1;
    }

    return 0;
}

int runUtf8DirectiveAliasRoundTripTest()
{
    return runDirectiveEncodingRoundTripTest(QStringLiteral("utf-8"),
                                             QStringLiteral("utf8"),
                                             QStringLiteral("survey žluťoučký kůň\nendsurvey\n"),
                                             QStringLiteral("utf8-alias.th"),
                                             QStringLiteral("utf"),
                                             "utf-8 codec is not available in this Qt runtime.",
                                             "Failed to encode utf8 alias fixture text.",
                                             "Failed to write utf8 alias fixture file.",
                                             "utf8 alias fixture decoded contents mismatch.",
                                             "utf8 alias fixture did not resolve to a UTF-family codec.",
                                             "utf8 alias save failed.",
                                             "utf8 alias save did not preserve original byte encoding.",
                                             QStringConverter::Flag::Default);
}

int runUtf16DirectiveAliasRoundTripTest()
{
    return runDirectiveEncodingRoundTripTest(QStringLiteral("utf-16"),
                                             QStringLiteral("utf16"),
                                             QStringLiteral("survey žluťoučký kůň\nendsurvey\n"),
                                             QStringLiteral("utf16-alias.th"),
                                             QStringLiteral("utf-16"),
                                             "utf-16 codec is not available in this Qt runtime.",
                                             "Failed to encode utf16 alias fixture text.",
                                             "Failed to write utf16 alias fixture file.",
                                             "utf16 alias fixture decoded contents mismatch.",
                                             "utf16 alias fixture did not resolve to a UTF-16 codec.",
                                             "utf16 alias save failed.",
                                             "utf16 alias save did not preserve original byte encoding.",
                                             QStringConverter::Flag::WriteBom);
}

int runUnknownDirectiveFallsBackToUtf8Test()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("unknown-encoding-directive.th"));
    const QString sourceText = QStringLiteral(
        "encoding madeup-xyz\n"
        "survey žluťoučký kůň\n"
        "endsurvey\n");
    const QByteArray utf8Bytes = sourceText.toUtf8();

    if (!expect(writeRawFile(filePath, utf8Bytes), "Failed to write unknown-directive UTF-8 fixture file.")) {
        return 1;
    }

    QString contents;
    QString encodingName;
    QString errorMessage;
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encodingName, nullptr, &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(contents == sourceText, "Unknown-directive UTF-8 fixture decoded contents mismatch.")) {
        return 1;
    }

    if (!expect(encodingName.contains(QStringLiteral("utf"), Qt::CaseInsensitive),
                "Unknown-directive UTF-8 fixture should resolve to a UTF-family encoding.")) {
        return 1;
    }

    if (!expect(DocumentFile::writeTextFile(filePath, contents, encodingName, &errorMessage), qPrintable(errorMessage))) {
        return 1;
    }

    const QByteArray writtenBytes = readRawFile(filePath);
    if (!expect(writtenBytes == utf8Bytes, "Unknown-directive UTF-8 fallback save did not preserve original bytes.")) {
        return 1;
    }

    return 0;
}

int runUnknownDirectiveFallsBackToLatin1Test()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("unknown-encoding-directive-latin1.th"));
    const QByteArray latin1Bytes("encoding madeup-xyz\nsurvey caf\xe9\nendsurvey\n");

    if (!expect(writeRawFile(filePath, latin1Bytes), "Failed to write unknown-directive Latin1 fixture file.")) {
        return 1;
    }

    QString contents;
    QString encodingName;
    QString errorMessage;
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encodingName, nullptr, &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(contents == QStringLiteral("encoding madeup-xyz\nsurvey café\nendsurvey\n"),
                "Unknown-directive Latin1 fixture decoded contents mismatch.")) {
        return 1;
    }

    if (!expect(!encodingName.trimmed().isEmpty(),
                "Unknown-directive Latin1 fixture should resolve to a non-empty fallback encoding.")) {
        return 1;
    }

    if (!expect(DocumentFile::writeTextFile(filePath, contents, encodingName, &errorMessage), qPrintable(errorMessage))) {
        return 1;
    }

    const QByteArray writtenBytes = readRawFile(filePath);
    if (!expect(writtenBytes == latin1Bytes, "Unknown-directive Latin1 fallback save did not preserve original bytes.")) {
        return 1;
    }

    return 0;
}

int runUnknownDirectiveFallsBackToUtf16Test()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("unknown-encoding-directive-utf16.th"));
    const QString sourceText = QStringLiteral(
        "encoding madeup-xyz\n"
        "survey žluťoučký kůň\n"
        "endsurvey\n");

    QStringEncoder encoder = makeEncoder(QStringLiteral("utf-16"), QStringConverter::Flag::WriteBom);
    if (!expect(encoder.isValid(), "utf-16 codec is not available in this Qt runtime.")) {
        return 1;
    }

    const QByteArray utf16Bytes = encoder.encode(sourceText);
    if (!expect(!encoder.hasError(), "Failed to encode unknown-directive UTF-16 fixture text.")) {
        return 1;
    }

    if (!expect(writeRawFile(filePath, utf16Bytes), "Failed to write unknown-directive UTF-16 fixture file.")) {
        return 1;
    }

    QString contents;
    QString encodingName;
    QString errorMessage;
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encodingName, nullptr, &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(contents == sourceText, "Unknown-directive UTF-16 fixture decoded contents mismatch.")) {
        return 1;
    }

    if (!expect(encodingName.contains(QStringLiteral("utf-16"), Qt::CaseInsensitive),
                "Unknown-directive UTF-16 fixture should resolve to a UTF-16-family encoding.")) {
        return 1;
    }

    if (!expect(DocumentFile::writeTextFile(filePath, contents, encodingName, &errorMessage), qPrintable(errorMessage))) {
        return 1;
    }

    const QByteArray writtenBytes = readRawFile(filePath);
    if (!expect(writtenBytes == utf16Bytes, "Unknown-directive UTF-16 fallback save did not preserve original bytes.")) {
        return 1;
    }

    return 0;
}

int runInspectorFallbackEncodingPreservationTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("inspector-fallback-cp1250.th2"));
    const QString originalText = QStringLiteral(
        "encoding cp1250\n"
        "line wall -id puvodni-linka -close off\n"
        "map puvodni-mapa\n");

    QStringEncoder encoder = makeEncoder(QStringLiteral("windows-1250"));
    if (!encoder.isValid()) {
        printOptionalCodecSkip("windows-1250 codec is not available in this Qt runtime.");
        return 0;
    }

    const QByteArray originalBytes = encoder.encode(originalText);
    if (!expect(!encoder.hasError(), "Failed to encode inspector fallback fixture text.")) {
        return 1;
    }

    if (!expect(writeRawFile(filePath, originalBytes), "Failed to write inspector fallback fixture file.")) {
        return 1;
    }

    QString contents;
    QString encodingName;
    QString errorMessage;
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encodingName, nullptr, &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(encodingName.contains(QStringLiteral("1250"), Qt::CaseInsensitive),
                "Inspector fallback fixture did not resolve to a 1250-family encoding.")) {
        return 1;
    }

    if (!expect(rewriteLineOptionToggle(&contents,
                                                                2,
                                                                QStringLiteral("-close"),
                                                                true,
                                                                &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(rewriteStructureEntryName(&contents,
                                                                  3,
                                                                  QStringLiteral("Maps"),
                                                                  QStringLiteral("žlutá-mapa"),
                                                                  &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(DocumentFile::writeTextFile(filePath, contents, encodingName, &errorMessage), qPrintable(errorMessage))) {
        return 1;
    }

    QStringEncoder expectedEncoder = makeEncoder(encodingName);
    if (!expect(expectedEncoder.isValid(), "Resolved encoding is not writable for expected byte validation.")) {
        return 1;
    }

    const QString expectedText = QStringLiteral(
        "encoding cp1250\n"
        "line wall -id puvodni-linka -close on\n"
        "map žlutá-mapa\n");
    const QByteArray expectedBytes = expectedEncoder.encode(expectedText);
    if (!expect(!expectedEncoder.hasError(), "Failed to encode expected inspector fallback output.")) {
        return 1;
    }

    const QByteArray writtenBytes = readRawFile(filePath);
    if (!expect(writtenBytes == expectedBytes,
                "Inspector fallback rewrite path did not preserve source encoding semantics.")) {
        return 1;
    }

    return 0;
}

int runInspectorFallbackEncodingPreservationWindows1252Test()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("inspector-fallback-cp1252.th2"));
    const QString originalText = QStringLiteral(
        "encoding windows-1252\n"
        "line wall -id ligne-originale -close off\n"
        "map cafe-plan\n");

    QStringEncoder encoder = makeEncoder(QStringLiteral("windows-1252"));
    if (!encoder.isValid()) {
        printOptionalCodecSkip("windows-1252 codec is not available in this Qt runtime.");
        return 0;
    }

    const QByteArray originalBytes = encoder.encode(originalText);
    if (!expect(!encoder.hasError(), "Failed to encode inspector fallback windows-1252 fixture text.")) {
        return 1;
    }

    if (!expect(writeRawFile(filePath, originalBytes), "Failed to write inspector fallback windows-1252 fixture file.")) {
        return 1;
    }

    QString contents;
    QString encodingName;
    QString errorMessage;
    if (!expect(DocumentFile::readTextFile(filePath, &contents, &encodingName, nullptr, &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(encodingName.contains(QStringLiteral("1252"), Qt::CaseInsensitive),
                "Inspector fallback windows-1252 fixture did not resolve to a 1252-family encoding.")) {
        return 1;
    }

    if (!expect(rewriteLineOptionToggle(&contents,
                                                                2,
                                                                QStringLiteral("-close"),
                                                                true,
                                                                &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(rewriteStructureEntryName(&contents,
                                                                  3,
                                                                  QStringLiteral("Maps"),
                                                                  QStringLiteral("café-plan"),
                                                                  &errorMessage),
                qPrintable(errorMessage))) {
        return 1;
    }

    if (!expect(DocumentFile::writeTextFile(filePath, contents, encodingName, &errorMessage), qPrintable(errorMessage))) {
        return 1;
    }

    QStringEncoder expectedEncoder = makeEncoder(encodingName);
    if (!expect(expectedEncoder.isValid(), "Resolved windows-1252 encoding is not writable for expected byte validation.")) {
        return 1;
    }

    const QString expectedText = QStringLiteral(
        "encoding windows-1252\n"
        "line wall -id ligne-originale -close on\n"
        "map café-plan\n");
    const QByteArray expectedBytes = expectedEncoder.encode(expectedText);
    if (!expect(!expectedEncoder.hasError(), "Failed to encode expected inspector fallback windows-1252 output.")) {
        return 1;
    }

    const QByteArray writtenBytes = readRawFile(filePath);
    if (!expect(writtenBytes == expectedBytes,
                "Inspector fallback windows-1252 rewrite path did not preserve source encoding semantics.")) {
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

    if (const int nonUtfResult = runNonUtf8RoundTripTest(); nonUtfResult != 0) {
        return nonUtfResult;
    }

    if (const int cp1250Result = runWindows1250DirectiveRoundTripTest(); cp1250Result != 0) {
        return cp1250Result;
    }

    if (const int cp1250AliasResult = runCp1250DirectiveAliasRoundTripTest(); cp1250AliasResult != 0) {
        return cp1250AliasResult;
    }

    if (const int cp1252Result = runWindows1252DirectiveRoundTripTest(); cp1252Result != 0) {
        return cp1252Result;
    }

    if (const int cp1252AliasResult = runCp1252DirectiveAliasRoundTripTest(); cp1252AliasResult != 0) {
        return cp1252AliasResult;
    }

    if (const int latin2Result = runLatin2DirectiveRoundTripTest(); latin2Result != 0) {
        return latin2Result;
    }

    if (const int undeclaredLatin2Result = runUndeclaredLatin2RoundTripTest(); undeclaredLatin2Result != 0) {
        return undeclaredLatin2Result;
    }

    if (const int utf8AliasResult = runUtf8DirectiveAliasRoundTripTest(); utf8AliasResult != 0) {
        return utf8AliasResult;
    }

    if (const int utf16AliasResult = runUtf16DirectiveAliasRoundTripTest(); utf16AliasResult != 0) {
        return utf16AliasResult;
    }

    if (const int unknownDirectiveResult = runUnknownDirectiveFallsBackToUtf8Test(); unknownDirectiveResult != 0) {
        return unknownDirectiveResult;
    }

    if (const int unknownDirectiveUtf16Result = runUnknownDirectiveFallsBackToUtf16Test(); unknownDirectiveUtf16Result != 0) {
        return unknownDirectiveUtf16Result;
    }

    if (const int unknownDirectiveLatin1Result = runUnknownDirectiveFallsBackToLatin1Test(); unknownDirectiveLatin1Result != 0) {
        return unknownDirectiveLatin1Result;
    }

    if (const int inspector1250Result = runInspectorFallbackEncodingPreservationTest(); inspector1250Result != 0) {
        return inspector1250Result;
    }

    return runInspectorFallbackEncodingPreservationWindows1252Test();
}
