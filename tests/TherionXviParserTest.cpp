#include "../src/core/TherionXviParser.h"

#include <QTemporaryFile>
#include <QtMath>

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

bool nearlyEqual(qreal a, qreal b, qreal epsilon = 0.0001)
{
    return qAbs(a - b) <= epsilon;
}

int runParseTextTest()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {-10 20 5 0 0 10 4 3}\n"
        "set XVIstations {\n"
        "  {1 2 station.alpha}\n"
        "  {3 4 station.beta}\n"
        "}\n"
        "set XVIshots {\n"
        "  {0 0 10 10}\n"
        "}\n"
        "set XVIsketchlines {\n"
        "  {line 0 0 10 0 10 10}\n"
        "}\n");

    TherionXviDocument document;
    if (!expect(parseTherionXviDocumentText(xviText, &document), "Expected valid XVI text parsing to succeed.")) {
        return 1;
    }
    if (!expect(document.hasGridOrigin, "Expected parsed XVI document to include grid origin.")) {
        return 1;
    }
    if (!expect(nearlyEqual(document.gridOrigin.x(), -10.0) && nearlyEqual(document.gridOrigin.y(), 20.0),
                "Expected parsed XVI grid origin coordinates.")) {
        return 1;
    }
    if (!expect(document.hasGridDefinition, "Expected parsed XVI grid definition to be valid.")) {
        return 1;
    }
    if (!expect(document.gridCountX == 4 && document.gridCountY == 3, "Expected parsed XVI grid counts.")) {
        return 1;
    }
    if (!expect(document.stations.size() == 2, "Expected parsed XVI stations collection size.")) {
        return 1;
    }
    if (!expect(document.shots.size() == 1, "Expected parsed XVI shots collection size.")) {
        return 1;
    }
    if (!expect(document.sketchLines.size() == 1 && document.sketchLines.first().size() == 3,
                "Expected parsed XVI sketchline point count.")) {
        return 1;
    }

    return 0;
}

int runParseFileTest()
{
    QTemporaryFile temporaryFile;
    temporaryFile.setAutoRemove(true);
    if (!expect(temporaryFile.open(), "Expected temporary file creation for XVI parser test.")) {
        return 1;
    }

    const QByteArray xviBytes =
        "set XVIgrid {0 0 1 0 0 1 2 2}\n"
        "set XVIstations {\n"
        "  {0 0 root}\n"
        "}\n"
        "set XVIshots {\n"
        "  {0 0 1 1}\n"
        "}\n";
    if (!expect(temporaryFile.write(xviBytes) == xviBytes.size(), "Expected temporary XVI test content to write fully.")) {
        return 1;
    }
    temporaryFile.flush();

    TherionXviDocument document;
    if (!expect(parseTherionXviDocumentFile(temporaryFile.fileName(), &document), "Expected valid XVI file parsing to succeed.")) {
        return 1;
    }
    if (!expect(document.hasGridOrigin && document.hasGridDefinition, "Expected parsed XVI file to include grid metadata.")) {
        return 1;
    }
    if (!expect(document.stations.contains(QStringLiteral("root")), "Expected parsed XVI file to include root station.")) {
        return 1;
    }
    if (!expect(document.shots.size() == 1, "Expected parsed XVI file to include shot geometry.")) {
        return 1;
    }

    return 0;
}

int runRejectInvalidContentTest()
{
    const QString invalidText = QStringLiteral("set XVIgrid {0 0 1 0}\n");
    TherionXviDocument document;
    if (!expect(!parseTherionXviDocumentText(invalidText, &document),
                "Expected invalid/incomplete XVI content to be rejected.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (const int rc = runParseTextTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runParseFileTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runRejectInvalidContentTest(); rc != 0) {
        return rc;
    }

    return 0;
}

