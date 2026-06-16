#include "../src/app/text_editor/map_editor/MapEditorPointSymbolGeometry.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MapEditorPointSymbolGeometryTest : public QObject
{
    Q_OBJECT

private slots:
    void alignsRectToRequestedAnchor();
    void supportsShortAlignAliases();
    void placesLabelsAccordingToAlign();
    void rotatesAlignedPathAroundAnchor();
};

void MapEditorPointSymbolGeometryTest::alignsRectToRequestedAnchor()
{
    const QRectF baseRect(-5.0, -5.0, 10.0, 10.0);

    QCOMPARE(mapEditorPointAlignedRect(baseRect, QStringLiteral("top")),
             QRectF(-5.0, 0.0, 10.0, 10.0));
    QCOMPARE(mapEditorPointAlignedRect(baseRect, QStringLiteral("bottom")),
             QRectF(-5.0, -10.0, 10.0, 10.0));
    QCOMPARE(mapEditorPointAlignedRect(baseRect, QStringLiteral("left")),
             QRectF(0.0, -5.0, 10.0, 10.0));
    QCOMPARE(mapEditorPointAlignedRect(baseRect, QStringLiteral("bottom-right")),
             QRectF(-10.0, -10.0, 10.0, 10.0));
}

void MapEditorPointSymbolGeometryTest::supportsShortAlignAliases()
{
    const QRectF baseRect(-5.0, -5.0, 10.0, 10.0);

    QCOMPARE(mapEditorPointAlignedRect(baseRect, QStringLiteral("tl")),
             mapEditorPointAlignedRect(baseRect, QStringLiteral("top-left")));
    QCOMPARE(mapEditorPointAlignedRect(baseRect, QStringLiteral("br")),
             mapEditorPointAlignedRect(baseRect, QStringLiteral("bottom-right")));
    QCOMPARE(mapEditorPointAlignedRect(baseRect, QStringLiteral("c")),
             mapEditorPointAlignedRect(baseRect, QString()));
}

void MapEditorPointSymbolGeometryTest::placesLabelsAccordingToAlign()
{
    const QPointF anchor(0.0, 0.0);
    const QSizeF symbolSize(10.0, 10.0);
    const QSizeF labelSize(20.0, 8.0);

    QCOMPARE(mapEditorPointLabelBounds(anchor, symbolSize, labelSize, QStringLiteral("left")),
             QRectF(-28.0, -4.0, 20.0, 8.0));
    QCOMPARE(mapEditorPointLabelBounds(anchor, symbolSize, labelSize, QStringLiteral("right")),
             QRectF(8.0, -4.0, 20.0, 8.0));
    QCOMPARE(mapEditorPointLabelBounds(anchor, symbolSize, labelSize, QStringLiteral("top")),
             QRectF(-10.0, -16.0, 20.0, 8.0));
    QCOMPARE(mapEditorPointLabelBounds(anchor, symbolSize, labelSize, QStringLiteral("bottom-right")),
             QRectF(8.0, 8.0, 20.0, 8.0));
    QCOMPARE(mapEditorPointLabelBounds(anchor, symbolSize, labelSize, QString()),
             QRectF(8.0, -4.0, 20.0, 8.0));
}

void MapEditorPointSymbolGeometryTest::rotatesAlignedPathAroundAnchor()
{
    const QRectF baseRect(-5.0, -5.0, 10.0, 10.0);
    const QPainterPath path = mapEditorPointRenderPath(MapEditorPointSymbol::Square,
                                                       {},
                                                       baseRect,
                                                       10.0,
                                                       QStringLiteral("top"),
                                                       90.0);
    const QRectF bounds = path.boundingRect();

    QCOMPARE(bounds, QRectF(-10.0, -5.0, 10.0, 10.0));
}
} // namespace

int runMapEditorPointSymbolGeometryTest(int argc, char **argv)
{
    MapEditorPointSymbolGeometryTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorPointSymbolGeometryTest.moc"
