#include "MapEditorSceneSupport.h"
#include "MapEditorSceneInternals.h"

#include <QFont>
#include <QGraphicsScene>
#include <QPainterPath>
#include <QRegularExpression>

#include "../core/TherionDocumentParser.h"

namespace TherionStudio {
namespace {
constexpr int kMapItemRole = Qt::UserRole + 120;
constexpr int kMapItemGeometryValue = 1;

bool tokenLooksNumeric(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    static const QRegularExpression numericPattern(
        QStringLiteral(R"(^[+-]?(?:(?:\d+(?:\.\d*)?)|(?:\.\d+))(?:[eE][+-]?\d+)?$)"));
    return numericPattern.match(token).hasMatch();
}

std::optional<bool> parseToggleToken(const QString &token)
{
    const QString normalized = token.trimmed().toLower();
    if (normalized == QStringLiteral("on")
        || normalized == QStringLiteral("yes")
        || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("1")) {
        return true;
    }
    if (normalized == QStringLiteral("off")
        || normalized == QStringLiteral("no")
        || normalized == QStringLiteral("false")
        || normalized == QStringLiteral("0")) {
        return false;
    }

    return std::nullopt;
}

std::optional<bool> lineOptionToggleValue(const TherionParsedLine &parsedLine, const QString &optionName)
{
    std::optional<bool> value;
    if (parsedLine.tokens.size() < 2 || optionName.isEmpty()) {
        return value;
    }

    const QString normalizedOption = optionName.toLower();
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed().toLower();
        if (token != normalizedOption) {
            continue;
        }

        if (index + 1 >= parsedLine.tokens.size()) {
            value = true;
            continue;
        }

        const QString nextToken = parsedLine.tokens.at(index + 1).trimmed();
        if (nextToken.startsWith(QLatin1Char('-')) && !tokenLooksNumeric(nextToken)) {
            value = true;
            continue;
        }

        value = parseToggleToken(nextToken).value_or(true);
    }

    return value;
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

struct SourceCoordinatePoint
{
    QPointF point;
    int sourceVertexIndex = -1;
};

QVector<SourceCoordinatePoint> sourceCoordinatePointsFromLine(const TherionParsedLine &parsedLine,
                                                              int startTokenIndex,
                                                              int *nextSourceVertexIndex)
{
    QVector<SourceCoordinatePoint> points;
    if (nextSourceVertexIndex == nullptr) {
        return points;
    }

    const QVector<QPointF> rowPoints = coordinatePointsFromLine(parsedLine, startTokenIndex);
    points.reserve(rowPoints.size());
    for (const QPointF &point : rowPoints) {
        SourceCoordinatePoint coordinatePoint;
        coordinatePoint.point = point;
        coordinatePoint.sourceVertexIndex = *nextSourceVertexIndex;
        ++(*nextSourceVertexIndex);
        points.append(coordinatePoint);
    }

    return points;
}

void appendLineDataPoints(MapGeometryFeature *feature, const QVector<SourceCoordinatePoint> &linePoints)
{
    if (feature == nullptr || linePoints.isEmpty()) {
        return;
    }

    int pointIndex = 0;
    if (feature->lineVertices.isEmpty()) {
        MapGeometryFeature::TH2LineVertex firstVertex;
        firstVertex.anchor = linePoints.first().point;
        firstVertex.anchorSourceVertexIndex = linePoints.first().sourceVertexIndex;
        feature->lineVertices.append(firstVertex);
        feature->vertices.append(firstVertex.anchor);
        pointIndex = 1;
    }

    while (pointIndex < linePoints.size()) {
        const int remaining = linePoints.size() - pointIndex;
        if (remaining == 1) {
            const SourceCoordinatePoint anchorPoint = linePoints.at(pointIndex);
            MapGeometryFeature::TH2LineVertex nextVertex;
            nextVertex.anchor = anchorPoint.point;
            nextVertex.anchorSourceVertexIndex = anchorPoint.sourceVertexIndex;
            feature->lineVertices.append(nextVertex);
            feature->vertices.append(nextVertex.anchor);

            MapGeometryFeature::LineSegment segment;
            segment.type = MapGeometryFeature::LineSegment::Type::Linear;
            segment.endVertexIndex = feature->lineVertices.size() - 1;
            feature->lineSegments.append(segment);
            ++pointIndex;
            continue;
        }

        if (remaining < 3) {
            const SourceCoordinatePoint anchorPoint = linePoints.at(pointIndex);
            MapGeometryFeature::TH2LineVertex nextVertex;
            nextVertex.anchor = anchorPoint.point;
            nextVertex.anchorSourceVertexIndex = anchorPoint.sourceVertexIndex;
            feature->lineVertices.append(nextVertex);
            feature->vertices.append(nextVertex.anchor);

            MapGeometryFeature::LineSegment segment;
            segment.type = MapGeometryFeature::LineSegment::Type::Linear;
            segment.endVertexIndex = feature->lineVertices.size() - 1;
            feature->lineSegments.append(segment);
            ++pointIndex;
            continue;
        }

        const SourceCoordinatePoint outControlPoint = linePoints.at(pointIndex);
        const SourceCoordinatePoint inControlPoint = linePoints.at(pointIndex + 1);
        const SourceCoordinatePoint anchorPoint = linePoints.at(pointIndex + 2);

        MapGeometryFeature::TH2LineVertex &previousVertex = feature->lineVertices.last();
        previousVertex.outgoingSourceVertexIndex = outControlPoint.sourceVertexIndex;
        if (mapSourcePointsDiffer(outControlPoint.point, previousVertex.anchor)) {
            previousVertex.outgoingControl = outControlPoint.point;
        } else {
            previousVertex.outgoingControl.reset();
        }
        const bool previousHasOutgoingControl = previousVertex.outgoingControl.has_value();
        const int previousOutgoingSourceIndex = previousVertex.outgoingSourceVertexIndex;

        MapGeometryFeature::TH2LineVertex nextVertex;
        nextVertex.anchor = anchorPoint.point;
        nextVertex.anchorSourceVertexIndex = anchorPoint.sourceVertexIndex;
        nextVertex.incomingSourceVertexIndex = inControlPoint.sourceVertexIndex;
        if (mapSourcePointsDiffer(inControlPoint.point, nextVertex.anchor)) {
            nextVertex.incomingControl = inControlPoint.point;
        } else {
            nextVertex.incomingControl.reset();
        }
        feature->lineVertices.append(nextVertex);
        feature->vertices.append(nextVertex.anchor);

        MapGeometryFeature::LineSegment segment;
        segment.type = (previousHasOutgoingControl || nextVertex.incomingControl.has_value())
            ? MapGeometryFeature::LineSegment::Type::Cubic
            : MapGeometryFeature::LineSegment::Type::Linear;
        segment.endVertexIndex = feature->lineVertices.size() - 1;
        segment.control1VertexIndex = previousOutgoingSourceIndex;
        segment.control2VertexIndex = nextVertex.incomingSourceVertexIndex;
        feature->lineSegments.append(segment);
        pointIndex += 3;
    }
}

QPainterPath linePathForFeature(const MapGeometryFeature &feature, const QRectF &sourceBounds, const QRectF &previewBounds)
{
    QPainterPath path;
    if (feature.lineVertices.size() < 2) {
        return path;
    }

    auto toPreview = [&](const QPointF &point) {
        return mapGeometryPointToPreview(point, sourceBounds, previewBounds);
    };

    path.moveTo(toPreview(feature.lineVertices.first().anchor));
    for (int index = 1; index < feature.lineVertices.size(); ++index) {
        const MapGeometryFeature::TH2LineVertex &previousVertex = feature.lineVertices.at(index - 1);
        const MapGeometryFeature::TH2LineVertex &currentVertex = feature.lineVertices.at(index);
        const QPointF cp1 = previousVertex.outgoingControl.value_or(previousVertex.anchor);
        const QPointF cp2 = currentVertex.incomingControl.value_or(currentVertex.anchor);
        const bool hasCurveHandle = previousVertex.outgoingControl.has_value() || currentVertex.incomingControl.has_value();
        if (hasCurveHandle) {
            path.cubicTo(toPreview(cp1), toPreview(cp2), toPreview(currentVertex.anchor));
        } else {
            path.lineTo(toPreview(currentVertex.anchor));
        }
    }

    if (feature.closed && feature.lineVertices.size() >= 3) {
        path.closeSubpath();
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

    QString subtitle = remainder.join(QStringLiteral(" "));
    if (parsedLine.tokens.first().toLower() == QStringLiteral("line")) {
        const bool closed = lineOptionToggleValue(parsedLine, QStringLiteral("-close")).value_or(false);
        const bool reversed = lineOptionToggleValue(parsedLine, QStringLiteral("-reverse")).value_or(false);
        QStringList flags;
        if (closed) {
            flags.append(QObject::tr("closed"));
        }
        if (reversed) {
            flags.append(QObject::tr("reversed"));
        }
        if (!flags.isEmpty()) {
            if (!subtitle.isEmpty()) {
                subtitle += QStringLiteral(" ");
            }
            subtitle += QStringLiteral("[%1]").arg(flags.join(QStringLiteral(", ")));
        }
    }

    return subtitle;
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

    const QRectF sceneFrame(0, 0, 1200, 900);
    scene->setSceneRect(sceneFrame);
    const QRectF geometryCanvas = sceneFrame.adjusted(24.0, 24.0, -24.0, -24.0);
    scene->addRect(geometryCanvas, QPen(QColor(QStringLiteral("#596477")), 1.2), QBrush(QColor(QStringLiteral("#232833"))));

    const QRectF previewBounds = geometryCanvas.adjusted(20.0, 20.0, -20.0, -20.0);
    const QRectF sourceBounds = geometryBoundsForFeatures(geometryFeatures);
    const qreal mapScale = sceneCoordsScaleFactor(sourceBounds, previewBounds);
    const qreal pointRadius = qBound(3.2, 4.8 * mapScale, 6.2);
    const qreal vertexRadius = qBound(2.4, 3.8 * mapScale, 4.8);
    const qreal thickLineWidth = qBound(1.3, 2.2 * mapScale, 2.9);
    const qreal detailLineWidth = qBound(0.8, 1.4 * mapScale, 1.8);
    const bool compactLabels = geometryFeatures.size() > 24;
    auto markGeometryItem = [](QGraphicsItem *item) {
        if (item != nullptr) {
            item->setData(kMapItemRole, kMapItemGeometryValue);
        }
    };

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
                markGeometryItem(pointItem);

                if (feature.stationPoint) {
                    QPainterPath markerPath;
                    markerPath.moveTo(previewPoint + QPointF(0.0, -12.0));
                    markerPath.lineTo(previewPoint + QPointF(10.0, 6.0));
                    markerPath.lineTo(previewPoint + QPointF(-10.0, 6.0));
                    markerPath.closeSubpath();

                    auto *triangle = scene->addPath(markerPath, QPen(QColor(QStringLiteral("#ff4f3d")), 1.2), QBrush(QColor(QStringLiteral("#ff4f3d"))));
                    triangle->setZValue(3.5);
                    markGeometryItem(triangle);
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
                if (feature.lineVertices.size() < 2) {
                    break;
                }

                const QPainterPath path = linePathForFeature(feature, sourceBounds, previewBounds);

                auto *lineItem = scene->addPath(path, QPen(QColor(QStringLiteral("#222222")), thickLineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                lineItem->setZValue(2.5);
                markGeometryItem(lineItem);
                auto *detailItem = scene->addPath(path, QPen(QColor(QStringLiteral("#2b2b2b")), detailLineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                detailItem->setZValue(3.0);
                markGeometryItem(detailItem);

                for (int vertexIndex = 0; vertexIndex < feature.lineVertices.size(); ++vertexIndex) {
                    const MapGeometryFeature::TH2LineVertex &vertex = feature.lineVertices.at(vertexIndex);
                    auto *vertexItem = new MapEditableGeometryVertexItem(feature.lineNumber,
                                                                         QStringLiteral("line"),
                                                                         vertex.anchorSourceVertexIndex >= 0 ? vertex.anchorSourceVertexIndex : vertexIndex,
                                                                         vertex.anchor,
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
                    markGeometryItem(vertexItem);
                }

                const qreal controlRadius = qBound(1.8, 3.0 * mapScale, 4.0);
                for (int segmentIndex = 1; segmentIndex < feature.lineVertices.size(); ++segmentIndex) {
                    const MapGeometryFeature::TH2LineVertex &previousVertex = feature.lineVertices.at(segmentIndex - 1);
                    const MapGeometryFeature::TH2LineVertex &currentVertex = feature.lineVertices.at(segmentIndex);

                    if (previousVertex.outgoingControl.has_value() && previousVertex.outgoingSourceVertexIndex >= 0) {
                        const QPointF anchorPreview = mapGeometryPointToPreview(previousVertex.anchor, sourceBounds, previewBounds);
                        const QPointF controlPreview = mapGeometryPointToPreview(previousVertex.outgoingControl.value(), sourceBounds, previewBounds);
                        auto *connector = scene->addLine(QLineF(anchorPreview, controlPreview),
                                                         QPen(QColor(48, 128, 220, 160), qBound(0.7, 1.0 * mapScale, 1.4), Qt::DashLine, Qt::RoundCap));
                        connector->setZValue(3.2);
                        markGeometryItem(connector);

                        auto *controlItem = new MapEditableGeometryVertexItem(feature.lineNumber,
                                                                              QStringLiteral("line control"),
                                                                              previousVertex.outgoingSourceVertexIndex,
                                                                              previousVertex.outgoingControl.value(),
                                                                              sourceBounds,
                                                                              previewBounds);
                        controlItem->setRect(QRectF(-controlRadius, -controlRadius, controlRadius * 2.0, controlRadius * 2.0));
                        controlItem->setPen(QPen(QColor(28, 94, 182, 220), 1.0));
                        controlItem->setBrush(QBrush(QColor(104, 188, 255, 205)));
                        controlItem->setMoveCommittedCallback(recordLineAreaVertexMove);
                        scene->addItem(controlItem);
                        controlItem->setZValue(4.2);
                        markGeometryItem(controlItem);
                    }

                    if (currentVertex.incomingControl.has_value() && currentVertex.incomingSourceVertexIndex >= 0) {
                        const QPointF anchorPreview = mapGeometryPointToPreview(currentVertex.anchor, sourceBounds, previewBounds);
                        const QPointF controlPreview = mapGeometryPointToPreview(currentVertex.incomingControl.value(), sourceBounds, previewBounds);
                        auto *connector = scene->addLine(QLineF(anchorPreview, controlPreview),
                                                         QPen(QColor(48, 128, 220, 160), qBound(0.7, 1.0 * mapScale, 1.4), Qt::DashLine, Qt::RoundCap));
                        connector->setZValue(3.2);
                        markGeometryItem(connector);

                        auto *controlItem = new MapEditableGeometryVertexItem(feature.lineNumber,
                                                                              QStringLiteral("line control"),
                                                                              currentVertex.incomingSourceVertexIndex,
                                                                              currentVertex.incomingControl.value(),
                                                                              sourceBounds,
                                                                              previewBounds);
                        controlItem->setRect(QRectF(-controlRadius, -controlRadius, controlRadius * 2.0, controlRadius * 2.0));
                        controlItem->setPen(QPen(QColor(28, 94, 182, 220), 1.0));
                        controlItem->setBrush(QBrush(QColor(104, 188, 255, 205)));
                        controlItem->setMoveCommittedCallback(recordLineAreaVertexMove);
                        scene->addItem(controlItem);
                        controlItem->setZValue(4.2);
                        markGeometryItem(controlItem);
                    }
                }

                if (feature.stationPoint) {
                    const QPointF headPoint = mapGeometryPointToPreview(feature.lineVertices.first().anchor, sourceBounds, previewBounds);
                    QPainterPath markerPath;
                    markerPath.moveTo(headPoint + QPointF(0.0, -12.0));
                    markerPath.lineTo(headPoint + QPointF(10.0, 6.0));
                    markerPath.lineTo(headPoint + QPointF(-10.0, 6.0));
                    markerPath.closeSubpath();

                    auto *triangle = scene->addPath(markerPath, QPen(QColor(QStringLiteral("#ff4f3d")), 1.2), QBrush(QColor(QStringLiteral("#ff4f3d"))));
                    triangle->setZValue(3.5);
                    markGeometryItem(triangle);
                }

                if (!compactLabels) {
                    auto *label = scene->addText(feature.label.isEmpty() ? feature.category : feature.label, QFont(QStringLiteral("Menlo"), 10, QFont::Bold));
                    label->setDefaultTextColor(QColor(QStringLiteral("#9d9d9d")));
                    label->setPos(mapGeometryPointToPreview(feature.lineVertices.first().anchor, sourceBounds, previewBounds) + QPointF(10.0, -18.0));
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
                markGeometryItem(fillItem);

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
                    markGeometryItem(vertexItem);
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
        emptyItem->setPos(previewBounds.left() + 16.0, previewBounds.top() + 44.0);
    }
}

QVector<MapGeometryFeature> collectGeometryFeatures(const QVector<TherionParsedLine> &parsedLines)
{
    QVector<MapGeometryFeature> features;
    MapGeometryFeature currentFeature;
    bool inLineBlock = false;
    bool inAreaBlock = false;
    int lineSourceVertexIndex = 0;

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
        lineSourceVertexIndex = 0;
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
            currentFeature.closed = lineOptionToggleValue(parsedLine, QStringLiteral("-close")).value_or(false);
            currentFeature.reversed = lineOptionToggleValue(parsedLine, QStringLiteral("-reverse")).value_or(false);
            appendLineDataPoints(&currentFeature, sourceCoordinatePointsFromLine(parsedLine, 1, &lineSourceVertexIndex));
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
            if (directive == QStringLiteral("smooth")
                && parsedLine.tokens.size() >= 2
                && parsedLine.tokens.at(1).compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0) {
                if (!currentFeature.lineVertices.isEmpty()) {
                    currentFeature.lineVertices.last().isSmooth = false;
                }
                continue;
            }

            appendLineDataPoints(&currentFeature, sourceCoordinatePointsFromLine(parsedLine, 0, &lineSourceVertexIndex));
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
