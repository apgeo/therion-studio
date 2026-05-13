#include "../src/core/MapBackgroundPlacement.h"

#include <iostream>
#include <QtMath>

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

int runClopyFallbackAnchorTest()
{
    RasterPlacementMetadata metadata{};
    metadata.basePosition = QPointF(0.0, 0.0);
    metadata.hasBasePosition = true;
    metadata.topEdgeAnchor = true;

    AreaAdjustMetadata areaAdjust{};
    areaAdjust.valid = true;
    areaAdjust.modelRect = QRectF(QPointF(-128.0, -1152.0), QPointF(851.0, 128.0));

    const QRectF rect = resolveRasterModelRect(QSizeF(723.0, 1024.0), metadata, areaAdjust);
    if (!expect(rect.isValid(), "Expected a valid model rect for clopy fallback test.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.left(), 0.0), "Expected clopy fallback left edge to be anchored at x=0.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.top(), -1024.0), "Expected clopy fallback top edge to be y=-1024.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.right(), 723.0), "Expected clopy fallback right edge to be x=723.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.bottom(), 0.0), "Expected clopy fallback bottom edge to be y=0.")) {
        return 1;
    }

    return 0;
}

int runAreaAdjustMatchTest()
{
    RasterPlacementMetadata metadata{};
    metadata.basePosition = QPointF(0.0, 0.0);
    metadata.hasBasePosition = true;
    metadata.topEdgeAnchor = true;

    AreaAdjustMetadata areaAdjust{};
    areaAdjust.valid = true;
    areaAdjust.modelRect = QRectF(QPointF(-128.0, -860.0), QPointF(736.0, 132.0));

    const QSizeF paddedImageSize(860.0, 988.0);
    const QRectF rect = resolveRasterModelRect(paddedImageSize, metadata, areaAdjust);
    if (!expect(rect.isValid(), "Expected a valid model rect when area-adjust matches image dimensions.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.left(), -128.0), "Expected area-adjust match to preserve left edge.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.top(), -860.0), "Expected area-adjust match to preserve top edge.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.right(), 736.0), "Expected area-adjust match to preserve right edge.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.bottom(), 132.0), "Expected area-adjust match to preserve bottom edge.")) {
        return 1;
    }

    return 0;
}

int runOffsetAnchorTest()
{
    RasterPlacementMetadata metadata{};
    metadata.basePosition = QPointF(232.0, -145.0);
    metadata.hasBasePosition = true;
    metadata.topEdgeAnchor = true;

    AreaAdjustMetadata areaAdjust{};
    areaAdjust.valid = false;

    const QRectF rect = resolveRasterModelRect(QSizeF(772.0, 337.0), metadata, areaAdjust);
    if (!expect(rect.isValid(), "Expected a valid model rect for offset-anchor test.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.left(), 232.0), "Expected offset-anchor left edge at x=232.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.top(), -482.0), "Expected offset-anchor top edge at y=-482.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.right(), 1004.0), "Expected offset-anchor right edge at x=1004.")) {
        return 1;
    }
    if (!expect(nearlyEqual(rect.bottom(), -145.0), "Expected offset-anchor bottom edge at y=-145.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (const int rc = runClopyFallbackAnchorTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAreaAdjustMatchTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runOffsetAnchorTest(); rc != 0) {
        return rc;
    }

    return 0;
}
