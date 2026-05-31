#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"
#include "../src/core/TherionDocumentParser.h"

#include <iostream>
#include <cmath>

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

bool expectMoveSetsEqual(const QVector<MapLineSecondaryMove> &expected,
                         const QVector<MapLineSecondaryMove> &actual,
                         const char *message)
{
    auto toMap = [](const QVector<MapLineSecondaryMove> &moves) {
        QHash<int, QPair<QPointF, QPointF>> map;
        for (const MapLineSecondaryMove &move : moves) {
            map.insert(move.sourceVertexIndex, qMakePair(move.oldPoint, move.newPoint));
        }
        return map;
    };

    const QHash<int, QPair<QPointF, QPointF>> expectedMap = toMap(expected);
    const QHash<int, QPair<QPointF, QPointF>> actualMap = toMap(actual);
    if (expectedMap != actualMap) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

const MapGeometryFeature *firstLineFeature(const QVector<MapGeometryFeature> &features)
{
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Line) {
            return &feature;
        }
    }

    return nullptr;
}

const MapGeometryFeature *firstLineFeatureByLabel(const QVector<MapGeometryFeature> &features, const QString &label)
{
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Line
            && feature.label == label) {
            return &feature;
        }
    }

    return nullptr;
}

QVector<const MapGeometryFeature *> pointFeatures(const QVector<MapGeometryFeature> &features)
{
    QVector<const MapGeometryFeature *> points;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Point) {
            points.append(&feature);
        }
    }
    return points;
}

int runPointTypeAndLabelOptionParsingTest()
{
    const QString text =
        QStringLiteral("point 10 20 station -name P1 -orientation 450\n"
                       "point 30 40 label -text \"large<br>dome\" -orient -45\n"
                       "line label -text \"stream<br>label\"\n"
                       "  10 10\n"
                       "  40 10\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const QVector<const MapGeometryFeature *> points = pointFeatures(features);
    if (!expect(points.size() == 2, "Expected two point features to parse.")) {
        return 1;
    }

    const MapGeometryFeature *station = points.at(0);
    if (!expect(station->label == QStringLiteral("station"),
                "Expected standard point-coordinate station syntax to resolve the point type.")) {
        return 1;
    }
    if (!expect(station->stationPoint,
                "Expected standard point-coordinate station syntax to mark the feature as a station point.")) {
        return 1;
    }
    if (!expect(station->optionValues.value(QStringLiteral("name")) == QStringLiteral("P1"),
                "Expected station -name option to be available for style-driven labels.")) {
        return 1;
    }
    if (!expect(station->orientationDegrees.has_value()
                    && std::abs(station->orientationDegrees.value() - 90.0) < 1e-6,
                "Expected point -orientation to parse and normalize into scene degrees.")) {
        return 1;
    }

    const MapGeometryFeature *label = points.at(1);
    if (!expect(label->label == QStringLiteral("label"),
                "Expected standard point-coordinate label syntax to resolve the point type.")) {
        return 1;
    }
    if (!expect(label->optionValues.value(QStringLiteral("text")) == QStringLiteral("large<br>dome"),
                "Expected label -text option to be available for style-driven labels.")) {
        return 1;
    }
    if (!expect(label->orientationDegrees.has_value()
                    && std::abs(label->orientationDegrees.value() - 315.0) < 1e-6,
                "Expected point -orient alias to parse and normalize negative degrees.")) {
        return 1;
    }

    const MapGeometryFeature *lineLabel = firstLineFeatureByLabel(features, QStringLiteral("label"));
    if (!expect(lineLabel != nullptr, "Expected line label feature to parse.")) {
        return 1;
    }
    if (!expect(lineLabel->optionValues.value(QStringLiteral("text")) == QStringLiteral("stream<br>label"),
                "Expected line label -text option to be available for style-driven labels.")) {
        return 1;
    }

    return 0;
}

int runBezierSegmentParsingTest()
{
    const QString text =
        QStringLiteral("line wall\n"
                       "  1174.0 744.5\n"
                       "  1194.0 756.5 1192.5 757.5 1176.0 791.0\n"
                       "  smooth off\n"
                       "  1205.5 788.0 1195.5 832.5 1173.5 879.0\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature.")) {
        return 1;
    }

    if (!expect(line->lineVertices.size() == 3, "Expected three anchor vertices for two Bezier segments.")) {
        return 1;
    }
    if (!expect(line->lineSegments.size() == 2, "Expected two parsed line segments.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(0).type == MapGeometryFeature::LineSegment::Type::Cubic,
                "Expected first segment to be cubic.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(1).type == MapGeometryFeature::LineSegment::Type::Cubic,
                "Expected second segment to be cubic.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(0).control1VertexIndex == 1
                && line->lineSegments.at(0).control2VertexIndex == 2
                && line->lineSegments.at(0).endVertexIndex == 1,
                "Expected first cubic segment indices to map to first control/control/end triplet.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(1).control1VertexIndex == 4
                && line->lineSegments.at(1).control2VertexIndex == 5
                && line->lineSegments.at(1).endVertexIndex == 2,
                "Expected second cubic segment indices to map to second control/control/end triplet.")) {
        return 1;
    }
    if (!expect(line->lineVertices.at(1).isSmooth == false,
                "Expected smooth off to mark the current/last parsed line vertex as non-smooth.")) {
        return 1;
    }
    if (!expect(line->lineVertices.at(0).outgoingControl.has_value()
                && line->lineVertices.at(1).incomingControl.has_value()
                && line->lineVertices.at(1).outgoingControl.has_value()
                && line->lineVertices.at(2).incomingControl.has_value(),
                "Expected parsed Bezier controls to be attached to adjacent vertices.")) {
        return 1;
    }

    return 0;
}

int runSmoothAndOptionMetadataIgnoredTest()
{
    const QString text =
        QStringLiteral("line wall\n"
                       "  10 20 30 40\n"
                       "  smooth off 50 60\n"
                       "  -subtype temporary 100 200\n"
                       "  70 80\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for metadata-ignore test.")) {
        return 1;
    }

    if (!expect(line->lineVertices.size() == 3,
                "Expected only coordinate data lines to contribute vertices (smooth/subtype metadata ignored).")) {
        return 1;
    }
    if (!expect(line->lineSegments.size() == 2,
                "Expected two linear segments after metadata lines are ignored.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(0).type == MapGeometryFeature::LineSegment::Type::Linear
                && line->lineSegments.at(1).type == MapGeometryFeature::LineSegment::Type::Linear,
                "Expected both parsed segments to remain linear in metadata-ignore case.")) {
        return 1;
    }

    return 0;
}

int runSlopeLinePointOptionsParsingTest()
{
    const QString text =
        QStringLiteral("line slope\n"
                       "  0 0\n"
                       "  l-size 20\n"
                       "  orientation 0\n"
                       "  10 0\n"
                       "  l-size 40\n"
                       "  orientation 90\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed slope line feature.")) {
        return 1;
    }
    if (!expect(line->lineVertices.size() == 2,
                "Expected slope line to preserve two anchor vertices.")) {
        return 1;
    }
    if (!expect(line->lineVertices.first().leftSize.has_value()
                    && std::abs(line->lineVertices.first().leftSize.value() - 20.0) < 1e-6
                    && line->lineVertices.first().orientationDegrees.has_value()
                    && std::abs(line->lineVertices.first().orientationDegrees.value()) < 1e-6,
                "Expected first slope anchor to retain l-size and orientation metadata.")) {
        return 1;
    }
    if (!expect(line->lineVertices.last().leftSize.has_value()
                    && std::abs(line->lineVertices.last().leftSize.value() - 40.0) < 1e-6
                    && line->lineVertices.last().orientationDegrees.has_value()
                    && std::abs(line->lineVertices.last().orientationDegrees.value() - 90.0) < 1e-6,
                "Expected second slope anchor to retain l-size and orientation metadata.")) {
        return 1;
    }

    return 0;
}

int runAnchorEquivalentControlNormalizationTest()
{
    const QString text =
        QStringLiteral("line wall\n"
                       "  0 0\n"
                       "  0 0 10 0 10 0\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed line feature for control normalization test.")) {
        return 1;
    }
    if (!expect(line->lineVertices.size() == 2, "Expected two anchors in normalization test.")) {
        return 1;
    }
    if (!expect(!line->lineVertices.at(0).outgoingControl.has_value()
                && !line->lineVertices.at(1).incomingControl.has_value(),
                "Expected controls equal to anchor coordinates to normalize back to nil handles in memory.")) {
        return 1;
    }

    return 0;
}

int runLineOptionToggleParsingTest()
{
    const QString text =
        QStringLiteral("line wall -close on -reverse on\n"
                       "  0 0\n"
                       "  10 0\n"
                       "  10 10\n"
                       "endline\n"
                       "line wall -close off -reverse off\n"
                       "  20 20\n"
                       "  30 20\n"
                       "  30 30\n"
                       "endline\n"
                       "line wall -close\n"
                       "  40 40\n"
                       "  50 40\n"
                       "  50 50\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);

    QVector<const MapGeometryFeature *> lines;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Line) {
            lines.append(&feature);
        }
    }

    if (!expect(lines.size() == 3, "Expected three parsed line features for option-toggle parsing test.")) {
        return 1;
    }

    if (!expect(lines.at(0)->closed && lines.at(0)->reversed,
                "Expected first line to parse -close on and -reverse on as enabled.")) {
        return 1;
    }
    if (!expect(!lines.at(1)->closed && !lines.at(1)->reversed,
                "Expected second line to parse -close off and -reverse off as disabled.")) {
        return 1;
    }
    if (!expect(lines.at(2)->closed,
                "Expected bare -close option to enable closed-state parsing.")) {
        return 1;
    }

    return 0;
}

int runClosedLineClosingBezierParsingTest()
{
    const QString text =
        QStringLiteral("line wall -close on\n"
                       "  0 0\n"
                       "  10 0\n"
                       "  12 2 -2 2 0 0\n"
                       "endline\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const MapGeometryFeature *line = firstLineFeature(features);
    if (!expect(line != nullptr, "Expected one parsed closed line feature for closing-bezier test.")) {
        return 1;
    }
    if (!expect(line->closed, "Expected line to remain marked as closed.")) {
        return 1;
    }
    if (!expect(line->lineVertices.size() == 2,
                "Expected closing Bezier row to map into last->first controls without adding duplicate anchor vertex.")) {
        return 1;
    }
    if (!expect(line->lineSegments.size() == 2,
                "Expected closed line to include explicit closing segment metadata.")) {
        return 1;
    }
    if (!expect(line->lineVertices.first().incomingControl.has_value()
                && line->lineVertices.last().outgoingControl.has_value(),
                "Expected closing Bezier controls to attach to first incoming and last outgoing handles.")) {
        return 1;
    }
    if (!expect(std::abs(line->lineVertices.first().incomingControl.value().x() - (-2.0)) < 1e-6
                && std::abs(line->lineVertices.first().incomingControl.value().y() - 2.0) < 1e-6,
                "Expected first incoming control to preserve closing-segment control point.")) {
        return 1;
    }
    if (!expect(std::abs(line->lineVertices.last().outgoingControl.value().x() - 12.0) < 1e-6
                && std::abs(line->lineVertices.last().outgoingControl.value().y() - 2.0) < 1e-6,
                "Expected last outgoing control to preserve closing-segment control point.")) {
        return 1;
    }

    return 0;
}

int runDeCasteljauInsertionTest()
{
    QVector<MapGeometryFeature::TH2LineVertex> vertices;
    MapGeometryFeature::TH2LineVertex start;
    start.anchor = QPointF(0.0, 0.0);
    start.outgoingControl = QPointF(10.0, 0.0);
    MapGeometryFeature::TH2LineVertex end;
    end.anchor = QPointF(10.0, 10.0);
    end.incomingControl = QPointF(12.0, 10.0);
    vertices.append(start);
    vertices.append(end);

    int insertedIndex = -1;
    if (!expect(insertLineVertexByDeCasteljau(&vertices, 0, 0.5, &insertedIndex),
                "Expected de Casteljau insertion to succeed for cubic segment.")) {
        return 1;
    }
    if (!expect(vertices.size() == 3 && insertedIndex == 1,
                "Expected one inserted anchor between the original segment endpoints.")) {
        return 1;
    }

    const MapGeometryFeature::TH2LineVertex &inserted = vertices.at(1);
    if (!expect(std::abs(inserted.anchor.x() - 9.5) < 1e-6 && std::abs(inserted.anchor.y() - 5.0) < 1e-6,
                "Expected inserted anchor to match de Casteljau midpoint for the source cubic.")) {
        return 1;
    }
    if (!expect(vertices.at(0).outgoingControl.has_value(),
                "Expected start outgoing control to remain defined after split.")) {
        return 1;
    }
    if (!expect(vertices.at(2).incomingControl.has_value(),
                "Expected end incoming control to be synthesized by split when original segment was cubic.")) {
        return 1;
    }
    if (!expect(inserted.incomingControl.has_value() && inserted.outgoingControl.has_value(),
                "Expected inserted vertex to carry both incoming and outgoing controls for cubic split.")) {
        return 1;
    }

    return 0;
}

int runLineVertexRemovalReconnectTest()
{
    QVector<MapGeometryFeature::TH2LineVertex> vertices;
    MapGeometryFeature::TH2LineVertex first;
    first.anchor = QPointF(0.0, 0.0);
    first.outgoingControl = QPointF(2.0, 0.0);
    MapGeometryFeature::TH2LineVertex middle;
    middle.anchor = QPointF(5.0, 5.0);
    middle.incomingControl = QPointF(4.0, 5.0);
    middle.outgoingControl = QPointF(6.0, 5.0);
    MapGeometryFeature::TH2LineVertex last;
    last.anchor = QPointF(10.0, 10.0);
    last.incomingControl = QPointF(9.0, 10.0);
    vertices.append(first);
    vertices.append(middle);
    vertices.append(last);

    if (!expect(removeLineVertexWithReconnect(&vertices, 1),
                "Expected middle-vertex removal to succeed for reconnect test.")) {
        return 1;
    }
    if (!expect(vertices.size() == 2, "Expected removal to shrink line vertex sequence by one.")) {
        return 1;
    }
    if (!expect(vertices.at(0).outgoingControl.has_value()
                && vertices.at(0).outgoingControl.value() == QPointF(4.0, 5.0),
                "Expected previous outgoing control to reconnect through removed incoming control.")) {
        return 1;
    }
    if (!expect(vertices.at(1).incomingControl.has_value()
                && vertices.at(1).incomingControl.value() == QPointF(6.0, 5.0),
                "Expected next incoming control to reconnect through removed outgoing control.")) {
        return 1;
    }

    return 0;
}

int runSmoothMirrorHelperTest()
{
    const QPointF anchor(10.0, 20.0);
    const QPointF movedIncoming(16.0, 24.0); // vector length sqrt(52)
    const QPointF oldOutgoing(4.0, 20.0);    // opposite length 6.0

    const std::optional<QPointF> mirroredFromExisting = mirroredSmoothControlPoint(anchor,
                                                                                     movedIncoming,
                                                                                     oldOutgoing);
    if (!expect(mirroredFromExisting.has_value(),
                "Expected mirroredSmoothControlPoint to produce a point for non-zero drag vector.")) {
        return 1;
    }
    const QPointF mirrored = mirroredFromExisting.value();
    const qreal incomingLength = std::hypot(movedIncoming.x() - anchor.x(), movedIncoming.y() - anchor.y());
    const qreal outgoingLength = std::hypot(mirrored.x() - anchor.x(), mirrored.y() - anchor.y());
    if (!expect(std::abs(outgoingLength - 6.0) < 1e-6,
                "Expected mirrored smooth opposite to preserve existing opposite-handle length.")) {
        return 1;
    }
    if (!expect(std::abs(((movedIncoming.x() - anchor.x()) * (mirrored.x() - anchor.x()))
                       + ((movedIncoming.y() - anchor.y()) * (mirrored.y() - anchor.y()))
                       + (incomingLength * outgoingLength)) < 1e-4,
                "Expected mirrored smooth opposite to remain collinear and opposite to moved handle.")) {
        return 1;
    }

    const std::optional<QPointF> mirroredFromMissing = mirroredSmoothControlPoint(anchor,
                                                                                   movedIncoming,
                                                                                   std::nullopt);
    if (!expect(mirroredFromMissing.has_value(),
                "Expected mirroredSmoothControlPoint to create a mirrored point when opposite handle is missing.")) {
        return 1;
    }
    const qreal mirroredFromMissingLength = std::hypot(mirroredFromMissing->x() - anchor.x(),
                                                       mirroredFromMissing->y() - anchor.y());
    if (!expect(std::abs(mirroredFromMissingLength - incomingLength) < 1e-6,
                "Expected missing opposite handle to mirror using moved-handle length.")) {
        return 1;
    }

    const std::optional<QPointF> collapsed = mirroredSmoothControlPoint(anchor, anchor, oldOutgoing);
    if (!expect(!collapsed.has_value(),
                "Expected mirroredSmoothControlPoint to return nullopt for zero-length moved vector.")) {
        return 1;
    }

    return 0;
}

int runPreviewToSourceUnclampedTest()
{
    const QRectF sourceBounds(0.0, -50.0, 100.0, 50.0);
    const QRectF previewBounds(0.0, 0.0, 200.0, 200.0);

    // This point is below the fitted preview rect for source bounds.
    const QPointF previewPoint(100.0, 190.0);
    const QPointF sourcePoint = mapGeometryPreviewToSource(previewPoint, sourceBounds, previewBounds);
    if (!expect(sourcePoint.y() < sourceBounds.top(),
                "Expected preview-to-source conversion to stay unclamped beyond fitted bounds (y below source top).")) {
        return 1;
    }

    // This point is above the fitted preview rect for source bounds.
    const QPointF previewPointAbove(100.0, 10.0);
    const QPointF sourcePointAbove = mapGeometryPreviewToSource(previewPointAbove, sourceBounds, previewBounds);
    if (!expect(sourcePointAbove.y() > sourceBounds.bottom(),
                "Expected preview-to-source conversion to stay unclamped beyond fitted bounds (y above source bottom).")) {
        return 1;
    }

    return 0;
}

int runLineAnchorSecondaryMoveCouplingTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = QPointF(100.0, 200.0);
    vertex.anchorSourceVertexIndex = 10;
    vertex.incomingControl = QPointF(90.0, 190.0);
    vertex.incomingSourceVertexIndex = 11;
    vertex.outgoingControl = QPointF(110.0, 210.0);
    vertex.outgoingSourceVertexIndex = 12;
    lineFeature.lineVertices.append(vertex);

    const QPointF oldAnchor(100.0, 200.0);
    const QPointF newAnchor(104.0, 197.0);
    const QPointF delta = newAnchor - oldAnchor;

    const QVector<MapLineSecondaryMove> secondaryMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                  10,
                                                                                                  oldAnchor,
                                                                                                  newAnchor);
    if (!expect(secondaryMoves.size() == 2,
                "Expected anchor drag to produce two secondary moves (incoming + outgoing control).")) {
        return 1;
    }

    bool incomingVerified = false;
    bool outgoingVerified = false;
    for (const MapLineSecondaryMove &move : secondaryMoves) {
        if (move.sourceVertexIndex == 11) {
            incomingVerified = (move.oldPoint == QPointF(90.0, 190.0))
                && (move.newPoint == QPointF(90.0, 190.0) + delta);
        }
        if (move.sourceVertexIndex == 12) {
            outgoingVerified = (move.oldPoint == QPointF(110.0, 210.0))
                && (move.newPoint == QPointF(110.0, 210.0) + delta);
        }
    }

    if (!expect(incomingVerified,
                "Expected incoming control secondary move to translate by anchor drag delta.")) {
        return 1;
    }
    if (!expect(outgoingVerified,
                "Expected outgoing control secondary move to translate by anchor drag delta.")) {
        return 1;
    }

    return 0;
}

int runLineControlSecondaryMoveSmoothTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = QPointF(100.0, 200.0);
    vertex.anchorSourceVertexIndex = 10;
    vertex.incomingControl = QPointF(90.0, 200.0);
    vertex.incomingSourceVertexIndex = 11;
    vertex.outgoingControl = QPointF(110.0, 200.0);
    vertex.outgoingSourceVertexIndex = 12;
    vertex.isSmooth = true;
    lineFeature.lineVertices.append(vertex);

    const QPointF oldIncoming = vertex.incomingControl.value();
    const QPointF newIncoming(85.0, 195.0);

    const QVector<MapLineSecondaryMove> secondaryMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                  11,
                                                                                                  oldIncoming,
                                                                                                  newIncoming);
    if (!expect(secondaryMoves.size() == 1,
                "Expected smooth incoming-control drag to produce one opposite-control secondary move.")) {
        return 1;
    }

    const MapLineSecondaryMove &move = secondaryMoves.first();
    if (!expect(move.sourceVertexIndex == 12,
                "Expected smooth incoming-control drag to target outgoing control source vertex.")) {
        return 1;
    }
    if (!expect(move.oldPoint == QPointF(110.0, 200.0),
                "Expected opposite-control secondary move to preserve original outgoing control point.")) {
        return 1;
    }

    const qreal oldOppositeLength = std::hypot(move.oldPoint.x() - vertex.anchor.x(),
                                                move.oldPoint.y() - vertex.anchor.y());
    const qreal newOppositeLength = std::hypot(move.newPoint.x() - vertex.anchor.x(),
                                                move.newPoint.y() - vertex.anchor.y());
    if (!expect(std::abs(oldOppositeLength - newOppositeLength) < 1e-6,
                "Expected smooth opposite-control move to preserve prior opposite handle length.")) {
        return 1;
    }

    return 0;
}

int runLineControlSecondaryMoveCornerTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = QPointF(100.0, 200.0);
    vertex.anchorSourceVertexIndex = 10;
    vertex.incomingControl = QPointF(90.0, 200.0);
    vertex.incomingSourceVertexIndex = 11;
    vertex.outgoingControl = QPointF(110.0, 200.0);
    vertex.outgoingSourceVertexIndex = 12;
    vertex.isSmooth = false;
    lineFeature.lineVertices.append(vertex);

    const QVector<MapLineSecondaryMove> secondaryMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                  11,
                                                                                                  QPointF(90.0, 200.0),
                                                                                                  QPointF(85.0, 195.0));
    if (!expect(secondaryMoves.isEmpty(),
                "Expected corner vertex control drag to keep handles independent (no secondary moves).")) {
        return 1;
    }

    return 0;
}

int runLinePreviewSecondaryMoveSelfFilterTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = QPointF(100.0, 200.0);
    vertex.anchorSourceVertexIndex = 10;
    vertex.incomingControl = QPointF(90.0, 200.0);
    vertex.incomingSourceVertexIndex = 11;
    // Malformed but possible in partially rewritten sources: opposite maps to same source index.
    vertex.outgoingControl = QPointF(110.0, 200.0);
    vertex.outgoingSourceVertexIndex = 11;
    vertex.isSmooth = true;
    lineFeature.lineVertices.append(vertex);

    const QVector<MapLineSecondaryMove> commandMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                               11,
                                                                                               QPointF(90.0, 200.0),
                                                                                               QPointF(86.0, 196.0));
    if (!expect(commandMoves.size() == 1,
                "Expected command-time coupling to include malformed self-target move before preview filtering.")) {
        return 1;
    }

    const QVector<MapLineSecondaryMove> previewMoves = collectLinePreviewSecondaryMovesForVertexDrag(lineFeature,
                                                                                                      11,
                                                                                                      QPointF(90.0, 200.0),
                                                                                                      QPointF(86.0, 196.0));
    if (!expect(previewMoves.isEmpty(),
                "Expected preview coupling helper to filter out self-target secondary moves.")) {
        return 1;
    }

    return 0;
}

int runLinePreviewAnchorSequencingNoStaleTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = QPointF(100.0, 200.0);
    vertex.anchorSourceVertexIndex = 10;
    vertex.incomingControl = QPointF(90.0, 190.0);
    vertex.incomingSourceVertexIndex = 11;
    vertex.outgoingControl = QPointF(110.0, 210.0);
    vertex.outgoingSourceVertexIndex = 12;
    lineFeature.lineVertices.append(vertex);

    QHash<int, QPointF> currentControls;
    currentControls.insert(11, QPointF(90.0, 190.0));
    currentControls.insert(12, QPointF(110.0, 210.0));

    const QPointF step0(100.0, 200.0);
    const QPointF step1(103.0, 198.0);
    const QPointF step2(106.0, 197.0);

    const QVector<MapLineSecondaryMove> firstStepMoves = collectLinePreviewCoupledUpdatesForVertexDrag(lineFeature,
                                                                                                        10,
                                                                                                        step0,
                                                                                                        step1,
                                                                                                        currentControls);
    for (const MapLineSecondaryMove &move : firstStepMoves) {
        currentControls.insert(move.sourceVertexIndex, move.newPoint);
    }

    const QVector<MapLineSecondaryMove> secondStepMoves = collectLinePreviewCoupledUpdatesForVertexDrag(lineFeature,
                                                                                                         10,
                                                                                                         step1,
                                                                                                         step2,
                                                                                                         currentControls);
    for (const MapLineSecondaryMove &move : secondStepMoves) {
        currentControls.insert(move.sourceVertexIndex, move.newPoint);
    }

    const QPointF totalDelta = step2 - step0;
    if (!expect(currentControls.value(11) == QPointF(90.0, 190.0) + totalDelta,
                "Expected incoming preview control transport to accumulate drag deltas (no stale absolute reset).")) {
        return 1;
    }
    if (!expect(currentControls.value(12) == QPointF(110.0, 210.0) + totalDelta,
                "Expected outgoing preview control transport to accumulate drag deltas (no stale absolute reset).")) {
        return 1;
    }

    return 0;
}

int runLinePreviewCommitParityTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.kind = MapGeometryFeature::Kind::Line;

    MapGeometryFeature::TH2LineVertex smoothVertex;
    smoothVertex.anchor = QPointF(100.0, 200.0);
    smoothVertex.anchorSourceVertexIndex = 10;
    smoothVertex.incomingControl = QPointF(90.0, 190.0);
    smoothVertex.incomingSourceVertexIndex = 11;
    smoothVertex.outgoingControl = QPointF(110.0, 210.0);
    smoothVertex.outgoingSourceVertexIndex = 12;
    smoothVertex.isSmooth = true;
    lineFeature.lineVertices.append(smoothVertex);

    MapGeometryFeature::TH2LineVertex cornerVertex;
    cornerVertex.anchor = QPointF(300.0, 400.0);
    cornerVertex.anchorSourceVertexIndex = 20;
    cornerVertex.incomingControl = QPointF(290.0, 390.0);
    cornerVertex.incomingSourceVertexIndex = 21;
    cornerVertex.outgoingControl = QPointF(310.0, 410.0);
    cornerVertex.outgoingSourceVertexIndex = 22;
    cornerVertex.isSmooth = false;
    lineFeature.lineVertices.append(cornerVertex);

    QHash<int, QPointF> currentControls;
    currentControls.insert(11, smoothVertex.incomingControl.value());
    currentControls.insert(12, smoothVertex.outgoingControl.value());
    currentControls.insert(21, cornerVertex.incomingControl.value());
    currentControls.insert(22, cornerVertex.outgoingControl.value());

    // 1) Anchor drag parity.
    const QVector<MapLineSecondaryMove> commandAnchorMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                     10,
                                                                                                     smoothVertex.anchor,
                                                                                                     QPointF(104.0, 197.0));
    const QVector<MapLineSecondaryMove> previewAnchorMoves = collectLinePreviewCoupledUpdatesForVertexDrag(lineFeature,
                                                                                                            10,
                                                                                                            smoothVertex.anchor,
                                                                                                            QPointF(104.0, 197.0),
                                                                                                            currentControls);
    if (!expectMoveSetsEqual(commandAnchorMoves,
                             previewAnchorMoves,
                             "Expected preview and commit coupling parity for anchor drag.")) {
        return 1;
    }

    // 2) Smooth control drag parity.
    const QVector<MapLineSecondaryMove> commandSmoothControlMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                            11,
                                                                                                            smoothVertex.incomingControl.value(),
                                                                                                            QPointF(84.0, 196.0));
    const QVector<MapLineSecondaryMove> previewSmoothControlMoves = collectLinePreviewCoupledUpdatesForVertexDrag(lineFeature,
                                                                                                                   11,
                                                                                                                   smoothVertex.incomingControl.value(),
                                                                                                                   QPointF(84.0, 196.0),
                                                                                                                   currentControls);
    if (!expectMoveSetsEqual(commandSmoothControlMoves,
                             previewSmoothControlMoves,
                             "Expected preview and commit coupling parity for smooth control drag.")) {
        return 1;
    }

    // 3) Corner control drag parity (no coupled move).
    const QVector<MapLineSecondaryMove> commandCornerControlMoves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                                            21,
                                                                                                            cornerVertex.incomingControl.value(),
                                                                                                            QPointF(286.0, 388.0));
    const QVector<MapLineSecondaryMove> previewCornerControlMoves = collectLinePreviewCoupledUpdatesForVertexDrag(lineFeature,
                                                                                                                   21,
                                                                                                                   cornerVertex.incomingControl.value(),
                                                                                                                   QPointF(286.0, 388.0),
                                                                                                                   currentControls);
    if (!expectMoveSetsEqual(commandCornerControlMoves,
                             previewCornerControlMoves,
                             "Expected preview and commit coupling parity for corner control drag.")) {
        return 1;
    }

    return 0;
}

int runScrapScaleSourceUnitsPerMeterTest()
{
    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(
        QStringLiteral("scrap clopy01 -projection plan -scale [-128 -1152 851 -1152 0.0 0.0 24.8666 0.0 m]"),
        1);
    const std::optional<qreal> sourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(parsedLine.tokens);
    if (!expect(sourceUnitsPerMeter.has_value(), "Expected source-units-per-meter ratio to parse from scrap scale.")) {
        return 1;
    }
    if (!expect(std::abs(sourceUnitsPerMeter.value() - 39.3698) < 0.01,
                "Expected TH2 source units per meter to account for scrap -scale length.")) {
        return 1;
    }

    const TherionParsedLine centimeterScaleLine = TherionDocumentParser::parseLine(
        QStringLiteral("scrap cm -scale [0 0 100 0 0 0 1000 0 cm]"),
        1);
    const std::optional<qreal> centimeterSourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(centimeterScaleLine.tokens);
    if (!expect(centimeterSourceUnitsPerMeter.has_value(), "Expected centimeter scrap scale unit to parse.")) {
        return 1;
    }
    if (!expect(std::abs(centimeterSourceUnitsPerMeter.value() - 10.0) < 0.001,
                "Expected centimeter unit conversion to produce source units per meter.")) {
        return 1;
    }

    const TherionParsedLine numericScaleLine = TherionDocumentParser::parseLine(
        QStringLiteral("scrap numeric -scale 0.0254"),
        1);
    const std::optional<qreal> numericSourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(numericScaleLine.tokens);
    if (!expect(numericSourceUnitsPerMeter.has_value(), "Expected numeric scrap scale to parse.")) {
        return 1;
    }
    if (!expect(std::abs(numericSourceUnitsPerMeter.value() - 39.3700787) < 0.001,
                "Expected numeric scrap scale to convert meters per source unit.")) {
        return 1;
    }

    const TherionParsedLine unitScaleLine = TherionDocumentParser::parseLine(
        QStringLiteral("scrap unit -scale [2.54 cm]"),
        1);
    const std::optional<qreal> unitSourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(unitScaleLine.tokens);
    if (!expect(unitSourceUnitsPerMeter.has_value(), "Expected two-token unit scrap scale to parse.")) {
        return 1;
    }
    if (!expect(std::abs(unitSourceUnitsPerMeter.value() - 39.3700787) < 0.001,
                "Expected unit scrap scale to convert length units.")) {
        return 1;
    }

    const TherionParsedLine ratioScaleLine = TherionDocumentParser::parseLine(
        QStringLiteral("scrap ratio -scale [100 1 m]"),
        1);
    const std::optional<qreal> ratioSourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(ratioScaleLine.tokens);
    if (!expect(ratioSourceUnitsPerMeter.has_value(), "Expected ratio scrap scale to parse.")) {
        return 1;
    }
    if (!expect(std::abs(ratioSourceUnitsPerMeter.value() - 100.0) < 0.001,
                "Expected ratio scrap scale to convert drawing units per real length.")) {
        return 1;
    }

    const CoordinateTransform coordinateTransform = coordinateTransformFromScrapScale(parsedLine.tokens);
    if (!expect(coordinateTransform.valid, "Expected full scrap scale coordinate transform to parse.")) {
        return 1;
    }
    const QPointF realPoint1 = coordinateTransform.sourceToMap.map(QPointF(-128.0, -1152.0));
    if (!expect(std::abs(realPoint1.x()) < 0.001 && std::abs(realPoint1.y()) < 0.001,
                "Expected scrap scale source point 1 to map to real origin.")) {
        return 1;
    }
    const QPointF sourceAtTenMeters = coordinateTransform.mapToSource.map(QPointF(10.0, 0.0));
    if (!expect(std::abs(sourceAtTenMeters.x() - 265.698) < 0.01
                    && std::abs(sourceAtTenMeters.y() + 1152.0) < 0.01,
                "Expected real 10m grid interval to map back through scrap scale calibration.")) {
        return 1;
    }

    return 0;
}

int runAreaScrapClipMetadataParsingTest()
{
    const QString text =
        QStringLiteral("scrap s1\n"
                       "line wall -id w1\n"
                       "  0 0\n"
                       "  10 0\n"
                       "endline\n"
                       "line wall -id w2\n"
                       "  0 10\n"
                       "  10 10\n"
                       "endline\n"
                       "area sand\n"
                       "  w1\n"
                       "  w2\n"
                       "endarea\n"
                       "area clay -clip off -place top\n"
                       "  w1\n"
                       "  w2\n"
                       "endarea\n"
                       "area bedrock -place bottom\n"
                       "  w1\n"
                       "  w2\n"
                       "endarea\n"
                       "endscrap\n");

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(text);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);

    QVector<const MapGeometryFeature *> areas;
    QVector<const MapGeometryFeature *> lines;
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Area) {
            areas.append(&feature);
        } else if (feature.kind == MapGeometryFeature::Kind::Line) {
            lines.append(&feature);
        }
    }

    if (!expect(lines.size() == 2, "Expected two wall line features for scrap clip metadata test.")) {
        return 1;
    }
    if (!expect(areas.size() == 3, "Expected three area features for scrap clip metadata test.")) {
        return 1;
    }
    if (!expect(lines.first()->scrapLineNumber == 1 && areas.first()->scrapLineNumber == 1,
                "Expected line and area features to retain their owning scrap source line.")) {
        return 1;
    }
    if (!expect(areas.first()->clipToScrap,
                "Expected area without -clip to default to clip-on rendering metadata.")) {
        return 1;
    }
    if (!expect(!areas.at(1)->clipToScrap,
                "Expected area -clip off to disable scrap clipping metadata.")) {
        return 1;
    }
    if (!expect(areas.at(0)->areaPlace == MapGeometryAreaPlace::Default
                    && areas.at(1)->areaPlace == MapGeometryAreaPlace::Top
                    && areas.at(2)->areaPlace == MapGeometryAreaPlace::Bottom,
                "Expected area -place metadata to parse as default, top, and bottom.")) {
        return 1;
    }
    if (!expect(!areas.first()->verticesEditable && areas.first()->vertices.size() >= 3,
                "Expected referenced area border lines to resolve into non-editable area geometry.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (const int rc = runPointTypeAndLabelOptionParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runBezierSegmentParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSmoothAndOptionMetadataIgnoredTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSlopeLinePointOptionsParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAnchorEquivalentControlNormalizationTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineOptionToggleParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runClosedLineClosingBezierParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runDeCasteljauInsertionTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineVertexRemovalReconnectTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSmoothMirrorHelperTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runPreviewToSourceUnclampedTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineAnchorSecondaryMoveCouplingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineControlSecondaryMoveSmoothTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineControlSecondaryMoveCornerTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLinePreviewSecondaryMoveSelfFilterTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLinePreviewAnchorSequencingNoStaleTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLinePreviewCommitParityTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runScrapScaleSourceUnitsPerMeterTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAreaScrapClipMetadataParsingTest(); rc != 0) {
        return rc;
    }

    return 0;
}
