#include "../src/app/ProjectSearchScanner.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QStringEncoder>
#include <QTemporaryDir>
#include <QTimer>

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

bool writeTextFile(const QString &filePath, const QString &contents)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    const QByteArray bytes = contents.toUtf8();
    return file.write(bytes) == bytes.size();
}

bool writeEncodedTextFile(const QString &filePath, const QString &contents, const QString &encodingName)
{
    QStringEncoder encoder(encodingName.toLatin1().constData());
    if (!encoder.isValid()) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const QByteArray bytes = encoder.encode(contents);
    return file.write(bytes) == bytes.size();
}

QString canonicalOrAbsolutePath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

struct SearchWaitResult
{
    bool received = false;
    ProjectSearchScanner::Result result;
};

SearchWaitResult waitForSearch(ProjectSearchScanner &scanner)
{
    SearchWaitResult waitResult;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);

    QObject::connect(&scanner,
                     &ProjectSearchScanner::searchFinished,
                     &loop,
                     [&](const ProjectSearchScanner::Result &result) {
                         waitResult.received = true;
                         waitResult.result = result;
                         loop.quit();
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start();
    loop.exec();
    return waitResult;
}

int runFilesystemSearchTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString rootFile = QDir(tempDir.path()).filePath(QStringLiteral("root.th"));
    const QString mapFile = QDir(tempDir.path()).filePath(QStringLiteral("map.th2"));
    const QString ignoredFile = QDir(tempDir.path()).filePath(QStringLiteral("notes.md"));
    if (!expect(writeTextFile(rootFile,
                              QStringLiteral("survey Main\n  input map.th2\nendsurvey Main\n")),
                "Temporary .th file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(mapFile,
                              QStringLiteral("scrap s1\n  point 1 2 label -text Main Entrance\nendscrap\n")),
                "Temporary .th2 file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(ignoredFile,
                              QStringLiteral("Main should not be reported from markdown.\n")),
                "Temporary ignored file could not be written.")) {
        return 1;
    }

    ProjectSearchScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestSearch(tempDir.path(), QStringLiteral("main"), false, false, {});

    const SearchWaitResult waitResult = waitForSearch(scanner);
    if (!expect(waitResult.received, "ProjectSearchScanner did not emit searchFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Filesystem search should not report an error.")) {
        return 1;
    }
    if (!expect(waitResult.result.matches.size() == 3, "Case-insensitive search should find three Therion matches.")) {
        return 1;
    }

    for (const ProjectSearchScanner::Match &match : waitResult.result.matches) {
        if (!expect(match.filePath != canonicalOrAbsolutePath(ignoredFile),
                    "Project search should ignore non-Therion text files.")) {
            return 1;
        }
    }

    return 0;
}

int runInMemorySearchTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("unsaved.th"));
    if (!expect(writeTextFile(filePath,
                              QStringLiteral("survey stale\nendsurvey stale\n")),
                "Temporary stale file could not be written.")) {
        return 1;
    }

    QHash<QString, QString> inMemoryContents;
    inMemoryContents.insert(canonicalOrAbsolutePath(filePath),
                            QStringLiteral("survey live\n  centerline\n  station live.1 \"Live Station\"\n  endcenterline\nendsurvey live\n"));

    ProjectSearchScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestSearch(tempDir.path(), QStringLiteral("Live Station"), false, true, inMemoryContents);

    const SearchWaitResult waitResult = waitForSearch(scanner);
    if (!expect(waitResult.received, "In-memory search did not emit searchFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "In-memory search should not report an error.")) {
        return 1;
    }
    if (!expect(waitResult.result.matches.size() == 1, "In-memory search should use the unsaved editor text.")) {
        return 1;
    }
    const ProjectSearchScanner::Match &match = waitResult.result.matches.first();
    if (!expect(canonicalOrAbsolutePath(match.filePath) == canonicalOrAbsolutePath(filePath)
                    && match.lineNumber == 3
                    && match.columnNumber == 19,
                "In-memory search match location is incorrect.")) {
        return 1;
    }

    return 0;
}

int runWholeWordSearchTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("words.th"));
    if (!expect(writeTextFile(filePath,
                              QStringLiteral(
                                  "survey cave\n"
                                  "  station cave.1 \"Cave\"\n"
                                  "  station caves.1 \"Plural\"\n"
                                  "endsurvey cave\n")),
                "Temporary whole-word file could not be written.")) {
        return 1;
    }

    ProjectSearchScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestSearch(tempDir.path(), QStringLiteral("cave"), true, true, {});

    const SearchWaitResult waitResult = waitForSearch(scanner);
    if (!expect(waitResult.received, "Whole-word search did not emit searchFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Whole-word search should not report an error.")) {
        return 1;
    }
    if (!expect(waitResult.result.matches.size() == 3,
                "Whole-word search should skip the embedded 'cave' in 'caves'.")) {
        return 1;
    }

    for (const ProjectSearchScanner::Match &match : waitResult.result.matches) {
        if (!expect(!match.lineText.contains(QStringLiteral("caves.1")),
                    "Whole-word search returned an embedded match.")) {
            return 1;
        }
    }

    return 0;
}

int runDeclaredEncodingSearchTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("latin2.th"));
    const QString contents = QStringLiteral(
        "encoding iso8859-2\n"
        "survey latin2\n"
        "  team \"Stacho Mudrák\"\n"
        "endsurvey latin2\n");
    if (!expect(writeEncodedTextFile(filePath, contents, QStringLiteral("iso-8859-2")),
                "Temporary ISO-8859-2 file could not be written.")) {
        return 1;
    }

    ProjectSearchScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestSearch(tempDir.path(), QStringLiteral("Mudrák"), false, true, {});

    const SearchWaitResult waitResult = waitForSearch(scanner);
    if (!expect(waitResult.received, "Declared-encoding search did not emit searchFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Declared-encoding search should not report an error.")) {
        return 1;
    }
    if (!expect(waitResult.result.matches.size() == 1,
                "Declared-encoding search should find the ISO-8859-2 diacritic match.")) {
        return 1;
    }
    if (!expect(waitResult.result.matches.first().lineText.contains(QStringLiteral("Mudrák")),
                "Declared-encoding search result snippet did not preserve diacritics.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runFilesystemSearchTest() != 0) {
        return 1;
    }
    if (runInMemorySearchTest() != 0) {
        return 1;
    }
    if (runWholeWordSearchTest() != 0) {
        return 1;
    }
    if (runDeclaredEncodingSearchTest() != 0) {
        return 1;
    }

    return 0;
}
