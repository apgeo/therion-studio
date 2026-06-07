#include "../src/core/TherionBackgroundMetadata.h"

#include <QDir>
#include <QFileInfo>
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

QString normalizedPathForCompare(const QString &path)
{
    return QDir::fromNativeSeparators(QDir::cleanPath(path));
}

QString documentPathForTest(const QString &fileName)
{
    return QDir(QDir::tempPath()).filePath(QStringLiteral("therion-studio-test/project/data/") + fileName);
}

int runRasterInsertParsingTest()
{
    const QString documentPath = documentPathForTest(QStringLiteral("clopy01.th2"));
    const QString text =
        QStringLiteral("encoding utf-8\n"
                       "##XTHERION## xth_me_image_insert {0 1 5.011872336272722} {0 {}} clopy01.png 0 {}\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    if (!expect(references.size() == 1, "Expected exactly one parsed raster background reference.")) {
        return 1;
    }

    const TherionBackgroundReference &reference = references.first();
    if (!expect(reference.hasBasePosition, "Expected parsed base position for raster metadata.")) {
        return 1;
    }
    if (!expect(nearlyEqual(reference.basePosition.x(), 0.0) && nearlyEqual(reference.basePosition.y(), 0.0),
                "Expected raster base position to parse as (0,0).")) {
        return 1;
    }
    if (!expect(reference.hasImageScale && nearlyEqual(reference.imageScale, 5.011872336272722),
                "Expected raster image scale to parse from first metadata group.")) {
        return 1;
    }
    if (!expect(reference.hasVisibility && reference.visible,
                "Expected raster visibility to parse from first metadata group.")) {
        return 1;
    }
    if (!expect(reference.lineNumber == 2, "Expected raster metadata source line to be reported.")) {
        return 1;
    }
    if (!expect(!reference.xviReference, "Expected PNG reference to be marked as raster.")) {
        return 1;
    }
    if (!expect(reference.metadataTopEdgeAnchor, "Expected raster metadata to use top-edge anchoring.")) {
        return 1;
    }

    const QString expectedPath = QFileInfo(documentPath).dir().filePath(QStringLiteral("clopy01.png"));
    if (!expect(normalizedPathForCompare(reference.absolutePath) == normalizedPathForCompare(expectedPath),
                "Expected relative raster path to resolve against TH2 document directory.")) {
        return 1;
    }

    return 0;
}

int runBracedRasterPathParsingTest()
{
    const QString documentPath = documentPathForTest(QStringLiteral("map.th2"));
    const QString text =
        QStringLiteral("##XTHERION## xth_me_image_insert {-12.5 0 1.25} {40 {}} {background scans/clopy 01.png} 0 {}\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    if (!expect(references.size() == 1, "Expected braced raster background path to parse.")) {
        return 1;
    }

    const TherionBackgroundReference &reference = references.first();
    if (!expect(reference.hasBasePosition
                && nearlyEqual(reference.basePosition.x(), -12.5)
                && nearlyEqual(reference.basePosition.y(), 40.0),
                "Expected braced raster metadata coordinates to parse.")) {
        return 1;
    }
    if (!expect(reference.hasVisibility && !reference.visible,
                "Expected hidden raster visibility to parse from first metadata group.")) {
        return 1;
    }
    if (!expect(reference.hasImageScale && nearlyEqual(reference.imageScale, 1.25),
                "Expected braced raster gamma/scale value to parse.")) {
        return 1;
    }

    const QString expectedPath = QFileInfo(documentPath).dir().filePath(QStringLiteral("background scans/clopy 01.png"));
    if (!expect(normalizedPathForCompare(reference.absolutePath) == normalizedPathForCompare(expectedPath),
                "Expected braced raster path with spaces to resolve against document directory.")) {
        return 1;
    }

    return 0;
}

int runUnbracedRasterYCoordinateParsingTest()
{
    const QString documentPath = documentPathForTest(QStringLiteral("severna.th2"));
    const QString text =
        QStringLiteral("encoding utf-8\n"
                       "##XTHERION## xth_me_area_adjust 1170.74 0.938 25 1560.0\n"
                       "##XTHERION## xth_me_image_insert {245.0 1.0} 822.0 img/sev1.gif 0 {}\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    if (!expect(references.size() == 1,
                "Expected raster metadata with unbraced y coordinate to parse.")) {
        return 1;
    }

    const TherionBackgroundReference &reference = references.first();
    if (!expect(reference.hasBasePosition
                && nearlyEqual(reference.basePosition.x(), 245.0)
                && nearlyEqual(reference.basePosition.y(), 822.0),
                "Expected unbraced raster metadata coordinates to parse.")) {
        return 1;
    }
    if (!expect(reference.hasVisibility && reference.visible,
                "Expected unbraced raster metadata visibility to parse from first group.")) {
        return 1;
    }
    if (!expect(!reference.xviReference, "Expected GIF reference to be marked as raster.")) {
        return 1;
    }

    const QString expectedPath = QFileInfo(documentPath).dir().filePath(QStringLiteral("img/sev1.gif"));
    if (!expect(normalizedPathForCompare(reference.absolutePath) == normalizedPathForCompare(expectedPath),
                "Expected unbraced raster path to resolve against TH2 document directory.")) {
        return 1;
    }

    return 0;
}

int runXviInsertParsingTest()
{
    const QString documentPath = documentPathForTest(QStringLiteral("create.th2"));
    const QString text =
        QStringLiteral("##XTHERION## xth_me_image_insert {456.253 1 1.0} {60.18 0@create.} create.xvi 0 {}\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    if (!expect(references.size() == 1, "Expected exactly one parsed XVI background reference.")) {
        return 1;
    }

    const TherionBackgroundReference &reference = references.first();
    if (!expect(reference.xviReference, "Expected .xvi reference to be marked as vector background.")) {
        return 1;
    }
    if (!expect(!reference.metadataTopEdgeAnchor, "Expected .xvi metadata to disable raster top-edge anchoring.")) {
        return 1;
    }
    if (!expect(reference.rootStationName == QStringLiteral("0@create."),
                "Expected root station token to parse from second metadata group.")) {
        return 1;
    }
    if (!expect(reference.hasBasePosition
                && nearlyEqual(reference.basePosition.x(), 456.253)
                && nearlyEqual(reference.basePosition.y(), 60.18),
                "Expected .xvi base position to parse from both metadata groups.")) {
        return 1;
    }

    return 0;
}

int runAreaAdjustParsingTest()
{
    const QString text =
        QStringLiteral("##XTHERION## xth_me_area_adjust 851 128 -128 -1152\n");

    const TherionAreaAdjust area = parseTherionAreaAdjust(text);
    if (!expect(area.valid, "Expected valid area-adjust metadata rectangle.")) {
        return 1;
    }
    if (!expect(nearlyEqual(area.modelRect.left(), -128.0), "Expected normalized area-adjust left edge.")) {
        return 1;
    }
    if (!expect(nearlyEqual(area.modelRect.top(), -1152.0), "Expected normalized area-adjust top edge.")) {
        return 1;
    }
    if (!expect(nearlyEqual(area.modelRect.right(), 851.0), "Expected normalized area-adjust right edge.")) {
        return 1;
    }
    if (!expect(nearlyEqual(area.modelRect.bottom(), 128.0), "Expected normalized area-adjust bottom edge.")) {
        return 1;
    }

    return 0;
}

int runMixedLineEndingParsingTest()
{
    const QString documentPath = documentPathForTest(QStringLiteral("mixed.th2"));
    const QString text =
        QStringLiteral("encoding utf-8\r"
                       "##XTHERION## xth_me_area_adjust 10 20 30 40\r\n"
                       "##XTHERION## xth_me_image_insert {1 1 2.5} {3 root.station} mixed.xvi 0 {}\n");

    const TherionAreaAdjust area = parseTherionAreaAdjust(text);
    if (!expect(area.valid
                    && nearlyEqual(area.modelRect.left(), 10.0)
                    && nearlyEqual(area.modelRect.top(), 20.0)
                    && nearlyEqual(area.modelRect.right(), 30.0)
                    && nearlyEqual(area.modelRect.bottom(), 40.0),
                "Expected area-adjust metadata to parse across mixed line endings.")) {
        return 1;
    }

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    if (!expect(references.size() == 1, "Expected image metadata to parse across mixed line endings.")) {
        return 1;
    }
    if (!expect(references.first().lineNumber == 3, "Expected mixed line-ending parser to preserve physical line numbers.")) {
        return 1;
    }
    return expect(references.first().rootStationName == QStringLiteral("root.station"),
                  "Expected root station token to parse from mixed line-ending image metadata.")
        ? 0
        : 1;
}

int runMalformedMetadataIgnoredTest()
{
    const QString documentPath = documentPathForTest(QStringLiteral("map.th2"));
    const QString text =
        QStringLiteral("##XTHERION## xth_me_image_insert {broken payload\n"
                       "##XTHERION## xth_me_area_adjust 1 2 3\n");

    const QVector<TherionBackgroundReference> references = parseTherionBackgroundReferences(text, documentPath);
    if (!expect(references.isEmpty(), "Expected malformed image-insert metadata to be ignored.")) {
        return 1;
    }

    const TherionAreaAdjust area = parseTherionAreaAdjust(text);
    if (!expect(!area.valid, "Expected malformed area-adjust metadata to be ignored.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (const int rc = runRasterInsertParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runBracedRasterPathParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runUnbracedRasterYCoordinateParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runXviInsertParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAreaAdjustParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMixedLineEndingParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMalformedMetadataIgnoredTest(); rc != 0) {
        return rc;
    }

    return 0;
}
