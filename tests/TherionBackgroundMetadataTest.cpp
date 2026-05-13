#include "../src/core/TherionBackgroundMetadata.h"

#include <QDir>
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

int runRasterInsertParsingTest()
{
    const QString documentPath = QDir::cleanPath(QStringLiteral("/tmp/project/data/clopy01.th2"));
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
    if (!expect(!reference.xviReference, "Expected PNG reference to be marked as raster.")) {
        return 1;
    }
    if (!expect(reference.metadataTopEdgeAnchor, "Expected raster metadata to use top-edge anchoring.")) {
        return 1;
    }

    const QString expectedPath = QDir::cleanPath(QStringLiteral("/tmp/project/data/clopy01.png"));
    if (!expect(QDir::cleanPath(reference.absolutePath) == expectedPath,
                "Expected relative raster path to resolve against TH2 document directory.")) {
        return 1;
    }

    return 0;
}

int runXviInsertParsingTest()
{
    const QString documentPath = QDir::cleanPath(QStringLiteral("/tmp/project/data/create.th2"));
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

int runMalformedMetadataIgnoredTest()
{
    const QString documentPath = QDir::cleanPath(QStringLiteral("/tmp/project/data/map.th2"));
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
    if (const int rc = runXviInsertParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAreaAdjustParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMalformedMetadataIgnoredTest(); rc != 0) {
        return rc;
    }

    return 0;
}
