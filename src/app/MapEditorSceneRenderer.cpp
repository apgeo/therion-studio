#include "MapEditorSceneSupport.h"
#include "MapEditorSceneInternals.h"

#include <QFont>
#include <QGraphicsScene>
#include <QPainterPath>
#include <QRegularExpression>

#include "../core/TherionDocumentParser.h"

namespace TherionStudio {
namespace {

bool tokenLooksNumeric(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    static const QRegularExpression numericPattern(
        QStringLiteral(R"(^[+-]?(?:(?:\d+(?:\.\d*)?)|(?:\.\d+))(?:[eE][+-]?\d+)?$)"));
    return numericPattern.match(token).hasMatch();
}

QVector<QPointF> coordinatePointsFromLine(const TherionParsedLine &parsedLine, int startTokenIndex)
{
    QVector<QPointF> points;
    QVector<int> numericIndices;
    numericIndices.reserve(parsedLine.tokens.size());

    const int firstTokenIndex = qMax(0, startTokenIndex);
    if (firstTokenIndex == 0) {
        int firstNonQuotedIndex = -1;
        for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
            if (index < parsedLine.tokenSpans.size()
                && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
                continue;
            }
            firstNonQuotedIndex = index;
            break;
        }

        if (firstNonQuotedIndex >= 0 && !tokenLooksNumeric(parsedLine.tokens.at(firstNonQuotedIndex))) {
            return points;
        }
    }

    if (firstTokenIndex < parsedLine.tokens.size()) {
        const QString firstToken = parsedLine.tokens.at(firstTokenIndex);
        if (firstTokenIndex == 0
            && firstToken.startsWith(QLatin1Char('-'))
            && !tokenLooksNumeric(firstToken)) {
            return points;
        }
    }

    bool sawCoordinateToken = false;
    bool skipOptionValueToken = false;
    for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
        if (skipOptionValueToken && !sawCoordinateToken) {
            skipOptionValueToken = false;
            continue;
        }

        const QString token = parsedLine.tokens.at(index);
        if (!sawCoordinateToken
            && firstTokenIndex > 0
            && token.startsWith(QLatin1Char('-'))
            && !tokenLooksNumeric(token)) {
            if (index + 1 < parsedLine.tokens.size()) {
                const QString nextToken = parsedLine.tokens.at(index + 1);
                if (!nextToken.startsWith(QLatin1Char('-')) || tokenLooksNumeric(nextToken)) {
                    skipOptionValueToken = true;
                }
            }
            continue;
        }

        const bool numeric = tokenLooksNumeric(token);
        if (!numeric) {
            if (sawCoordinateToken) {
                break;
            }
            continue;
        }

        if (index < parsedLine.tokenSpans.size()
            && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
            continue;
        }

        numericIndices.append(index);
        sawCoordinateToken = true;
    }

    for (int index = 0; index + 1 < numericIndices.size(); index += 2) {
        bool okX = false;
        bool okY = false;
        const qreal x = parsedLine.tokens.at(numericIndices.at(index)).toDouble(&okX);
        const qreal y = parsedLine.tokens.at(numericIndices.at(index + 1)).toDouble(&okY);
        if (okX && okY) {
            points.append(QPointF(x, y));
        }
    }

    return points;
}

void appendLineDataPoints(MapGeometryFeature *feature, const QVector<QPointF> &linePoints)
{
    if (feature == nullptr || linePoints.isEmpty()) {
        return;
    }

    int pointIndex = 0;
    if (feature->vertices.isEmpty()) {
        feature->vertices.append(linePoints.first());
        pointIndex = 1;
    }

    while (pointIndex < linePoints.size()) {
        const int remaining = linePoints.size() - pointIndex;
        if (remaining >= 3) {
            const int control1Index = feature->vertices.size();
            feature->vertices.append(linePoints.at(pointIndex));
            const int control2Index = feature->vertices.size();
            feature->vertices.append(linePoints.at(pointIndex + 1));
            const int endIndex = feature->vertices.size();
            feature->vertices.append(linePoints.at(pointIndex + 2));

            MapGeometryFeature::LineSegment segment;
            segment.type = MapGeometryFeature::LineSegment::Type::Cubic;
            segment.endVertexIndex = endIndex;
            segment.control1VertexIndex = control1Index;
            segment.control2VertexIndex = control2Index;
            feature->lineSegments.append(segment);
            pointIndex += 3;
            continue;
        }

        const int endIndex = feature->vertices.size();
        feature->vertices.append(linePoints.at(pointIndex));
        MapGeometryFeature::LineSegment segment;
        segment.type = MapGeometryFeature::LineSegment::Type::Linear;
        segment.endVertexIndex = endIndex;
        feature->lineSegments.append(segment);
        ++pointIndex;
    }
}

QPainterPath linePathForFeature(const MapGeometryFeature &feature, const QRectF &sourceBounds, const QRectF &previewBounds)
{
    QPainterPath path;
    if (feature.vertices.isEmpty()) {
        return path;
    }

    auto toPreview = [&](int index) {
        if (index < 0 || index >= feature.vertices.size()) {
            return QPointF();
        }
        return mapGeometryPointToPreview(feature.vertices.at(index), sourceBounds, previewBounds);
    };

    path.moveTo(toPreview(0));
    int currentAnchorIndex = 0;
    for (const MapGeometryFeature::LineSegment &segment : feature.lineSegments) {
        if (segment.endVertexIndex < 0 || segment.endVertexIndex >= feature.vertices.size()) {
            continue;
        }

        if (segment.type == MapGeometryFeature::LineSegment::Type::Cubic
            && segment.control1VertexIndex >= 0
            && segment.control2VertexIndex >= 0
            && segment.control1VertexIndex < feature.vertices.size()
            && segment.control2VertexIndex < feature.vertices.size()) {
            path.cubicTo(toPreview(segment.control1VertexIndex),
                         toPreview(segment.control2VertexIndex),
                         toPreview(segment.endVertexIndex));
        } else {
            path.lineTo(toPreview(segment.endVertexIndex));
        }

        currentAnchorIndex = segment.endVertexIndex;
    }

    if (currentAnchorIndex == 0) {
        for (int index = 1; index < feature.vertices.size(); ++index) {
            path.lineTo(toPreview(index));
        }
    }

    return path;
}

} // namespace

QString mapWorkspaceHelpHtml()
{
    return QStringLiteral(
        "<h3>Map Workspace</h3>"
        "<p>The map canvas renders parsed TH2 geometry and supports direct point, line, and area vertex edits where source coordinates are available.</p>"
        "<h4>Toolbar</h4>"
        "<ul>"
        "<li><strong>Select</strong> keeps focus on selecting and moving draft objects.</li>"
        "<li><strong>Point</strong>, <strong>Line</strong>, and <strong>Area</strong> create draft geometry in the scene.</li>"
        "<li><strong>Insert Scrap</strong> appends a new scrap block to the source document and reparses the scene.</li>"
        "<li><strong>Complete Draft</strong> marks the selected draft geometry as finished.</li>"
        "<li><strong>Undo</strong> and <strong>Redo</strong> follow the same command stack as source edits.</li>"
        "<li><strong>Fit</strong> recenters the geometry preview.</li>"
        "<li><strong>Fit + BG</strong> fits the viewport to geometry plus all loaded background image layers.</li>"
        "</ul>"
        "<p>Background image layers are managed from the Map sidebar, including layer order, visibility, position, opacity, and gamma.</p>"
        "<p>When present, <code>##XTHERION## xth_me_image_insert</code> metadata is used to auto-load referenced background images.</p>"
        "<p>Drag parsed geometry handles to rewrite source coordinates. Select a draft item to move or toggle it.</p>");
}

QString mapEntryCategoryForLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString directive = parsedLine.tokens.first().toLower();
    const QString secondToken = parsedLine.tokens.value(1).toLower();

    if (directive == QStringLiteral("survey")) {
        return QObject::tr("Survey");
    }
    if (directive == QStringLiteral("map")) {
        return QObject::tr("Map");
    }
    if (directive == QStringLiteral("scrap")) {
        return QObject::tr("Scrap");
    }
    if (directive == QStringLiteral("line")) {
        return QObject::tr("Line");
    }
    if (directive == QStringLiteral("area")) {
        return QObject::tr("Area");
    }
    if (directive == QStringLiteral("station")) {
        return QObject::tr("Station");
    }
    if (directive == QStringLiteral("point") && secondToken == QStringLiteral("station")) {
        return QObject::tr("Station");
    }
    if (directive == QStringLiteral("point")) {
        return QObject::tr("Point");
    }

    return QString();
}

QString mapEntryTitleForLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString directive = parsedLine.tokens.first().toLower();
    if (directive == QStringLiteral("point") && parsedLine.tokens.size() >= 3 && parsedLine.tokens.value(1).toLower() == QStringLiteral("station")) {
        return parsedLine.tokens.value(2);
    }

    if (parsedLine.tokens.size() > 1) {
        return parsedLine.tokens.value(1);
    }

    return parsedLine.tokens.first();
}

QString mapEntrySubtitleForLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.size() <= 1) {
        return parsedLine.rawText.trimmed();
    }

    QStringList remainder = parsedLine.tokens.mid(1);
    if (parsedLine.tokens.first().toLower() == QStringLiteral("point") && parsedLine.tokens.value(1).toLower() == QStringLiteral("station") && parsedLine.tokens.size() > 2) {
        remainder = parsedLine.tokens.mid(2);
    }

    return remainder.join(QStringLiteral(" "));
}

QColor mapEntryAccentForCategory(const QString &category)
{
    if (category == QObject::tr("Survey")) {
        return QColor(QStringLiteral("#5aa9ff"));
    }
    if (category == QObject::tr("Map")) {
        return QColor(QStringLiteral("#8f8bff"));
    }
    if (category == QObject::tr("Scrap")) {
        return QColor(QStringLiteral("#4dd6a8"));
    }
    if (category == QObject::tr("Line")) {
        return QColor(QStringLiteral("#ffb15a"));
    }
    if (category == QObject::tr("Area")) {
        return QColor(QStringLiteral("#ff7f8f"));
    }
    if (category == QObject::tr("Station")) {
        return QColor(QStringLiteral("#ffd86b"));
    }
    if (category == QObject::tr("Point")) {
        return QColor(QStringLiteral("#7ed0ff"));
    }

    return QColor(QStringLiteral("#7f8ca3"));
}

QVector<MapSceneEntry> collectMapSceneEntries(const QVector<TherionParsedLine> &parsedLines)
{
    QVector<MapSceneEntry> entries;
    entries.reserve(parsedLines.size());

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString category = mapEntryCategoryForLine(parsedLine);
        if (category.isEmpty()) {
            continue;
        }

        MapSceneEntry entry;
        entry.lineNumber = parsedLine.lineNumber;
        entry.category = category;
        entry.title = mapEntryTitleForLine(parsedLine);
        entry.subtitle = mapEntrySubtitleForLine(parsedLine);
        entries.append(entry);
    }

    return entries;
}

void renderMapWorkspaceScene(QGraphicsScene *scene,
                             const QString &documentPath,
                             const QVector<MapSceneEntry> &entries,
                             const QVector<MapGeometryFeature> &geometryFeatures,
                             QHash<int, QGraphicsRectItem *> *mapItemsByLine,
                             const std::function<void(int, const QPointF &, const QPointF &)> &recordCardMove,
                             const std::function<void(int, bool, bool)> &recordCardVisibility,
                             const std::function<void(int, const QPointF &, const QPointF &)> &recordPointGeometryMove,
                             const std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> &recordLineAreaVertexMove)
{
    Q_UNUSED(documentPath);
    Q_UNUSED(recordCardMove);
    Q_UNUSED(recordCardVisibility);

    if (scene == nullptr) {
        return;
    }

    if (mapItemsByLine != nullptr) {
        mapItemsByLine->clear();
    }

    const QRectF sceneFrame(0, 0, 980, 760);
    scene->setSceneRect(sceneFrame);
    scene->addRect(QRectF(32, 32, sceneFrame.width() - 64, sceneFrame.height() - 64), QPen(QColor(QStringLiteral("#596477")), 1.5), QBrush(QColor(QStringLiteral("#232833"))));

    QFont headerFont(QStringLiteral("Georgia"), 18, QFont::Bold);
    auto *titleItem = scene->addText(QObject::tr("TH2 Map Workspace"), headerFont);
    titleItem->setDefaultTextColor(QColor(QStringLiteral("#e2e9f3")));
    titleItem->setPos(56, 48);

    auto *summaryItem = scene->addText(QObject::tr("%1 map object(s) and %2 geometry feature(s) parsed from the current TH2 source.").arg(entries.size()).arg(geometryFeatures.size()), QFont(QStringLiteral("Menlo"), 11));
    summaryItem->setDefaultTextColor(QColor(QStringLiteral("#b4bfd0")));
    summaryItem->setPos(56, 84);

    const QRectF geometryCanvas(56.0, 132.0, sceneFrame.width() - 112.0, 260.0);
    scene->addRect(geometryCanvas, QPen(QColor(QStringLiteral("#d8dde5")), 1.2), QBrush(QColor(QStringLiteral("#ffffff"))));

    auto *geometryTitle = scene->addText(QObject::tr("Rendered TH2 geometry preview"), QFont(QStringLiteral("Menlo"), 12, QFont::Bold));
    geometryTitle->setDefaultTextColor(QColor(QStringLiteral("#111418")));
    geometryTitle->setPos(geometryCanvas.left() + 16.0, geometryCanvas.top() + 12.0);

    auto *geometrySubtitle = scene->addText(QObject::tr("Points, lines, and areas are drawn from parsed source geometry when available."), QFont(QStringLiteral("Menlo"), 10));
    geometrySubtitle->setDefaultTextColor(QColor(QStringLiteral("#7b8392")));
    geometrySubtitle->setPos(geometryCanvas.left() + 16.0, geometryCanvas.top() + 36.0);

    const QRectF previewBounds = geometryCanvas.adjusted(24.0, 64.0, -24.0, -24.0);
    const QRectF sourceBounds = geometryBoundsForFeatures(geometryFeatures);
    const qreal mapScale = sceneCoordsScaleFactor(sourceBounds, previewBounds);
    const qreal pointRadius = qBound(3.2, 4.8 * mapScale, 6.2);
    const qreal vertexRadius = qBound(2.4, 3.8 * mapScale, 4.8);
    const qreal thickLineWidth = qBound(1.3, 2.2 * mapScale, 2.9);
    const qreal detailLineWidth = qBound(0.8, 1.4 * mapScale, 1.8);
    const bool compactLabels = geometryFeatures.size() > 24;

    for (int index = 0; index < 6; ++index) {
        const qreal y = previewBounds.top() + (index * previewBounds.height() / 5.0);
        scene->addLine(previewBounds.left(), y, previewBounds.right(), y, QPen(QColor(QStringLiteral("#edf0f4")), 1.0, Qt::SolidLine));
    }
    for (int index = 0; index < 8; ++index) {
        const qreal x = previewBounds.left() + (index * previewBounds.width() / 7.0);
        scene->addLine(x, previewBounds.top(), x, previewBounds.bottom(), QPen(QColor(QStringLiteral("#edf0f4")), 1.0, Qt::SolidLine));
    }

    if (geometryFeatures.isEmpty()) {
        auto *emptyGeometryItem = scene->addText(QObject::tr("No parseable point, line, or area geometry was found in this document yet."), QFont(QStringLiteral("Menlo"), 11));
        emptyGeometryItem->setDefaultTextColor(QColor(QStringLiteral("#92a1b4")));
        emptyGeometryItem->setPos(previewBounds.left() + 16.0, previewBounds.top() + 16.0);
    } else {
        for (const MapGeometryFeature &feature : geometryFeatures) {
            switch (feature.kind) {
            case MapGeometryFeature::Kind::Point: {
                if (!feature.hasAnchor) {
                    break;
                }

                const QPointF previewPoint = mapGeometryPointToPreview(feature.anchor, sourceBounds, previewBounds);
                auto *pointItem = new MapEditablePointItem(feature.lineNumber, feature.anchor, sourceBounds, previewBounds);
                pointItem->setRect(QRectF(-pointRadius, -pointRadius, pointRadius * 2.0, pointRadius * 2.0));
                pointItem->setPen(QPen(QColor(14, 14, 14, 220), 1.1));
                pointItem->setBrush(QBrush(QColor(20, 20, 20, 180)));
                pointItem->setMoveCommittedCallback(recordPointGeometryMove);
                scene->addItem(pointItem);
                pointItem->setZValue(3.0);

                if (feature.stationPoint) {
                    QPainterPath markerPath;
                    markerPath.moveTo(previewPoint + QPointF(0.0, -12.0));
                    markerPath.lineTo(previewPoint + QPointF(10.0, 6.0));
                    markerPath.lineTo(previewPoint + QPointF(-10.0, 6.0));
                    markerPath.closeSubpath();

                    auto *triangle = scene->addPath(markerPath, QPen(QColor(QStringLiteral("#ff4f3d")), 1.2), QBrush(QColor(QStringLiteral("#ff4f3d"))));
                    triangle->setZValue(3.5);
                }

                if (feature.stationPoint || !compactLabels) {
                    auto *label = scene->addText(feature.label.isEmpty() ? feature.category : feature.label, QFont(QStringLiteral("Menlo"), 10, QFont::Bold));
                    label->setDefaultTextColor(QColor(QStringLiteral("#9d9d9d")));
                    label->setPos(previewPoint + QPointF(10.0, -18.0));
                    label->setZValue(4.0);
                }
                break;
            }
            case MapGeometryFeature::Kind::Line: {
                if (feature.vertices.size() < 2) {
                    break;
                }

                const QPainterPath path = linePathForFeature(feature, sourceBounds, previewBounds);

                auto *lineItem = scene->addPath(path, QPen(QColor(QStringLiteral("#222222")), thickLineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                lineItem->setZValue(2.5);
                auto *detailItem = scene->addPath(path, QPen(QColor(QStringLiteral("#2b2b2b")), detailLineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                detailItem->setZValue(3.0);

                for (int vertexIndex = 0; vertexIndex < feature.vertices.size(); ++vertexIndex) {
                    const QPointF vertex = feature.vertices.at(vertexIndex);
                    const QPointF previewPoint = mapGeometryPointToPreview(vertex, sourceBounds, previewBounds);
                    auto *vertexItem = new MapEditableGeometryVertexItem(feature.lineNumber,
                                                                         QStringLiteral("line"),
                                                                         vertexIndex,
                                                                         vertex,
                                                                         sourceBounds,
                                                                         previewBounds);
                    vertexItem->setRect(QRectF(-vertexRadius, -vertexRadius, vertexRadius * 2.0, vertexRadius * 2.0));
                    QColor vertexFill = feature.accent;
                    vertexFill.setAlpha(185);
                    QColor vertexOutline = feature.accent.darker(220);
                    vertexOutline.setAlpha(220);
                    vertexItem->setPen(QPen(vertexOutline, 1.0));
                    vertexItem->setBrush(QBrush(vertexFill));
                    vertexItem->setMoveCommittedCallback(recordLineAreaVertexMove);
                    scene->addItem(vertexItem);
                    vertexItem->setZValue(4.0);
                }

                if (feature.stationPoint) {
                    const QPointF headPoint = mapGeometryPointToPreview(feature.vertices.first(), sourceBounds, previewBounds);
                    QPainterPath markerPath;
                    markerPath.moveTo(headPoint + QPointF(0.0, -12.0));
                    markerPath.lineTo(headPoint + QPointF(10.0, 6.0));
                    markerPath.lineTo(headPoint + QPointF(-10.0, 6.0));
                    markerPath.closeSubpath();

                    auto *triangle = scene->addPath(markerPath, QPen(QColor(QStringLiteral("#ff4f3d")), 1.2), QBrush(QColor(QStringLiteral("#ff4f3d"))));
                    triangle->setZValue(3.5);
                }

                if (!compactLabels) {
                    auto *label = scene->addText(feature.label.isEmpty() ? feature.category : feature.label, QFont(QStringLiteral("Menlo"), 10, QFont::Bold));
                    label->setDefaultTextColor(QColor(QStringLiteral("#9d9d9d")));
                    label->setPos(mapGeometryPointToPreview(feature.vertices.first(), sourceBounds, previewBounds) + QPointF(10.0, -18.0));
                    label->setZValue(4.0);
                }
                break;
            }
            case MapGeometryFeature::Kind::Area: {
                if (feature.vertices.size() < 3) {
                    break;
                }

                QPainterPath path;
                const QPointF firstPoint = mapGeometryPointToPreview(feature.vertices.first(), sourceBounds, previewBounds);
                path.moveTo(firstPoint);
                for (int vertexIndex = 1; vertexIndex < feature.vertices.size(); ++vertexIndex) {
                    path.lineTo(mapGeometryPointToPreview(feature.vertices.at(vertexIndex), sourceBounds, previewBounds));
                }
                path.closeSubpath();

                auto *fillItem = scene->addPath(path, QPen(QColor(QStringLiteral("#222222")), qBound(1.0, 2.0 * mapScale, 2.4), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin), QBrush(QColor(0, 0, 0, 18)));
                fillItem->setZValue(2.0);

                for (int vertexIndex = 0; vertexIndex < feature.vertices.size(); ++vertexIndex) {
                    const QPointF vertex = feature.vertices.at(vertexIndex);
                    auto *vertexItem = new MapEditableGeometryVertexItem(feature.lineNumber,
                                                                         QStringLiteral("area"),
                                                                         vertexIndex,
                                                                         vertex,
                                                                         sourceBounds,
                                                                         previewBounds);
                    vertexItem->setRect(QRectF(-vertexRadius, -vertexRadius, vertexRadius * 2.0, vertexRadius * 2.0));
                    QColor vertexFill = feature.accent;
                    vertexFill.setAlpha(185);
                    QColor vertexOutline = feature.accent.darker(220);
                    vertexOutline.setAlpha(220);
                    vertexItem->setPen(QPen(vertexOutline, 1.0));
                    vertexItem->setBrush(QBrush(vertexFill));
                    vertexItem->setMoveCommittedCallback(recordLineAreaVertexMove);
                    scene->addItem(vertexItem);
                    vertexItem->setZValue(4.0);
                }

                if (!compactLabels) {
                    auto *label = scene->addText(feature.label.isEmpty() ? feature.category : feature.label, QFont(QStringLiteral("Menlo"), 10, QFont::Bold));
                    label->setDefaultTextColor(QColor(QStringLiteral("#9d9d9d")));
                    label->setPos(mapGeometryPointToPreview(feature.vertices.first(), sourceBounds, previewBounds) + QPointF(10.0, -18.0));
                    label->setZValue(4.0);
                }
                break;
            }
            }
        }
    }

    if (entries.isEmpty()) {
        auto *emptyItem = scene->addText(QObject::tr("No Therion map objects were detected in this document."), QFont(QStringLiteral("Menlo"), 12));
        emptyItem->setDefaultTextColor(QColor(QStringLiteral("#9ca7b6")));
        emptyItem->setPos(56, geometryCanvas.bottom() + 28.0);
    }
}

QVector<MapGeometryFeature> collectGeometryFeatures(const QVector<TherionParsedLine> &parsedLines)
{
    QVector<MapGeometryFeature> features;
    MapGeometryFeature currentFeature;
    bool inLineBlock = false;
    bool inAreaBlock = false;

    auto flushCurrentFeature = [&]() {
        if (currentFeature.kind == MapGeometryFeature::Kind::Point) {
            if (currentFeature.hasAnchor) {
                features.append(currentFeature);
            }
        } else if (currentFeature.kind == MapGeometryFeature::Kind::Line) {
            if (currentFeature.vertices.size() >= 2) {
                features.append(currentFeature);
            }
        } else if (currentFeature.kind == MapGeometryFeature::Kind::Area) {
            if (currentFeature.vertices.size() >= 3) {
                features.append(currentFeature);
            }
        }

        currentFeature = MapGeometryFeature{};
        inLineBlock = false;
        inAreaBlock = false;
    };

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive;

        if (directive == QStringLiteral("endline")) {
            if (inLineBlock) {
                flushCurrentFeature();
            }
            continue;
        }

        if (directive == QStringLiteral("endarea")) {
            if (inAreaBlock) {
                flushCurrentFeature();
            }
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("point")) {
            const QVector<QPointF> pointTokens = pointsFromTokens(parsedLine.tokens.mid(1));
            if (pointTokens.isEmpty()) {
                continue;
            }

            MapGeometryFeature feature;
            feature.kind = MapGeometryFeature::Kind::Point;
            feature.lineNumber = parsedLine.lineNumber;
            feature.category = mapEntryCategoryForLine(parsedLine);
            feature.label = mapEntryTitleForLine(parsedLine);
            feature.subtitle = mapEntrySubtitleForLine(parsedLine);
            feature.accent = mapEntryAccentForCategory(feature.category);
            feature.sourceAnchor = pointTokens.first();
            feature.anchor = feature.sourceAnchor;
            feature.hasAnchor = true;
            feature.hasSourceAnchor = true;
            feature.stationPoint = false;
            features.append(feature);
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("station")) {
            const QVector<QPointF> pointTokens = pointsFromTokens(parsedLine.tokens.mid(1));
            if (pointTokens.isEmpty()) {
                continue;
            }

            MapGeometryFeature feature;
            feature.kind = MapGeometryFeature::Kind::Point;
            feature.lineNumber = parsedLine.lineNumber;
            feature.category = mapEntryCategoryForLine(parsedLine);
            feature.label = mapEntryTitleForLine(parsedLine);
            feature.subtitle = mapEntrySubtitleForLine(parsedLine);
            feature.accent = mapEntryAccentForCategory(feature.category);
            feature.sourceAnchor = pointTokens.first();
            feature.anchor = feature.sourceAnchor;
            feature.hasAnchor = true;
            feature.hasSourceAnchor = true;
            feature.stationPoint = true;
            features.append(feature);
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("line")) {
            flushCurrentFeature();
            currentFeature.kind = MapGeometryFeature::Kind::Line;
            currentFeature.lineNumber = parsedLine.lineNumber;
            currentFeature.category = mapEntryCategoryForLine(parsedLine);
            currentFeature.label = mapEntryTitleForLine(parsedLine);
            currentFeature.subtitle = mapEntrySubtitleForLine(parsedLine);
            currentFeature.accent = mapEntryAccentForCategory(currentFeature.category);
            appendLineDataPoints(&currentFeature, coordinatePointsFromLine(parsedLine, 1));
            inLineBlock = true;
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("area")) {
            flushCurrentFeature();
            currentFeature.kind = MapGeometryFeature::Kind::Area;
            currentFeature.lineNumber = parsedLine.lineNumber;
            currentFeature.category = mapEntryCategoryForLine(parsedLine);
            currentFeature.label = mapEntryTitleForLine(parsedLine);
            currentFeature.subtitle = mapEntrySubtitleForLine(parsedLine);
            currentFeature.accent = mapEntryAccentForCategory(currentFeature.category);
            currentFeature.vertices.append(pointsFromTokens(parsedLine.tokens.mid(1)));
            inAreaBlock = true;
            continue;
        }

        if (inLineBlock) {
            appendLineDataPoints(&currentFeature, coordinatePointsFromLine(parsedLine, 0));
            continue;
        }

        if (inAreaBlock) {
            currentFeature.vertices.append(pointsFromTokens(parsedLine.tokens));
            continue;
        }
    }

    if (inLineBlock || inAreaBlock) {
        flushCurrentFeature();
    }

    return features;
}

} // namespace TherionStudio
