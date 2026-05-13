#include "../src/app/MapEditorSceneSupport.h"
#include "../src/core/TherionDocumentParser.h"

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

    if (!expect(line->vertices.size() == 7, "Expected Bezier parsing to keep anchor and control/end points.")) {
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
                && line->lineSegments.at(0).endVertexIndex == 3,
                "Expected first cubic segment indices to map to first control/control/end triplet.")) {
        return 1;
    }
    if (!expect(line->lineSegments.at(1).control1VertexIndex == 4
                && line->lineSegments.at(1).control2VertexIndex == 5
                && line->lineSegments.at(1).endVertexIndex == 6,
                "Expected second cubic segment indices to map to second control/control/end triplet.")) {
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

    if (!expect(line->vertices.size() == 3,
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
}

int main()
{
    if (const int rc = runBezierSegmentParsingTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSmoothAndOptionMetadataIgnoredTest(); rc != 0) {
        return rc;
    }

    return 0;
}
