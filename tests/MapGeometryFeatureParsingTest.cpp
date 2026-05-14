#include "../src/app/MapEditorSceneSupport.h"
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

const MapGeometryFeature *firstLineFeature(const QVector<MapGeometryFeature> &features)
{
    for (const MapGeometryFeature &feature : features) {
        if (feature.kind == MapGeometryFeature::Kind::Line) {
            return &feature;
        }
    }

    return nullptr;
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
}

int main()
{
    if (const int rc = runBezierSegmentParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSmoothAndOptionMetadataIgnoredTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runAnchorEquivalentControlNormalizationTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineOptionToggleParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runDeCasteljauInsertionTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runLineVertexRemovalReconnectTest(); rc != 0) {
        return rc;
    }

    return 0;
}
