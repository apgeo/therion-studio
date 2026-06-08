#include "MapEditorReferencedAreaResolver.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <utility>

#include <QLineF>

namespace TherionStudio
{
namespace
{
struct ReferencedAreaSegment
{
    QPointF start;
    QPointF end;
};

struct ReferencedLinePath
{
    QVector<QPointF> points;
    QVector<qreal> cumulativeLengths;
};

struct AdjacentLineIntersection
{
    QPointF point;
    qreal firstLineDistance = 0.0;
    qreal secondLineDistance = 0.0;
};

qreal squaredDistance(const QPointF &a, const QPointF &b)
{
    const QPointF delta = a - b;
    return QPointF::dotProduct(delta, delta);
}

bool pointsNear(const QPointF &a, const QPointF &b, qreal tolerance)
{
    return squaredDistance(a, b) <= tolerance * tolerance;
}

qreal sourceToleranceForPoints(const QVector<QPointF> &points)
{
    if (points.isEmpty()) {
        return 1e-6;
    }

    qreal minX = points.first().x();
    qreal minY = points.first().y();
    qreal maxX = points.first().x();
    qreal maxY = points.first().y();
    for (const QPointF &point : points) {
        minX = qMin(minX, point.x());
        minY = qMin(minY, point.y());
        maxX = qMax(maxX, point.x());
        maxY = qMax(maxY, point.y());
    }
    return qMax<qreal>(1e-6, std::hypot(maxX - minX, maxY - minY) * 1e-7);
}

QPointF cubicPointAt(const QPointF &p0,
                    const QPointF &p1,
                    const QPointF &p2,
                    const QPointF &p3,
                    qreal t)
{
    const qreal u = 1.0 - t;
    return p0 * (u * u * u)
        + p1 * (3.0 * u * u * t)
        + p2 * (3.0 * u * t * t)
        + p3 * (t * t * t);
}

QVector<QPointF> sampledSourceLinePoints(const MapGeometryFeature &feature)
{
    QVector<QPointF> points;
    if (feature.lineVertices.size() < 2) {
        return points;
    }

    points.append(feature.lineVertices.first().anchor);
    for (int index = 1; index < feature.lineVertices.size(); ++index) {
        const MapGeometryFeature::TH2LineVertex &previousVertex = feature.lineVertices.at(index - 1);
        const MapGeometryFeature::TH2LineVertex &currentVertex = feature.lineVertices.at(index);
        const QPointF cp1 = previousVertex.outgoingControl.value_or(previousVertex.anchor);
        const QPointF cp2 = currentVertex.incomingControl.value_or(currentVertex.anchor);
        const bool hasCurveHandle = previousVertex.outgoingControl.has_value() || currentVertex.incomingControl.has_value();
        if (hasCurveHandle) {
            constexpr int kCurveSteps = 24;
            for (int step = 1; step <= kCurveSteps; ++step) {
                points.append(cubicPointAt(previousVertex.anchor,
                                           cp1,
                                           cp2,
                                           currentVertex.anchor,
                                           static_cast<qreal>(step) / static_cast<qreal>(kCurveSteps)));
            }
        } else {
            points.append(currentVertex.anchor);
        }
    }

    return points;
}

ReferencedLinePath referencedLinePath(const MapGeometryFeature &feature)
{
    ReferencedLinePath path;
    path.points = sampledSourceLinePoints(feature);
    path.cumulativeLengths.reserve(path.points.size());

    qreal cumulative = 0.0;
    for (int index = 0; index < path.points.size(); ++index) {
        if (index > 0) {
            cumulative += QLineF(path.points.at(index - 1), path.points.at(index)).length();
        }
        path.cumulativeLengths.append(cumulative);
    }
    return path;
}

qreal segmentParameter(const QPointF &start, const QPointF &end, const QPointF &point)
{
    const QPointF direction = end - start;
    const qreal lengthSquared = QPointF::dotProduct(direction, direction);
    if (lengthSquared <= std::numeric_limits<qreal>::epsilon()) {
        return 0.0;
    }
    return QPointF::dotProduct(point - start, direction) / lengthSquared;
}

bool pointOnSegment(const QPointF &start, const QPointF &end, const QPointF &point, qreal tolerance)
{
    const qreal parameter = segmentParameter(start, end, point);
    if (parameter < -tolerance || parameter > 1.0 + tolerance) {
        return false;
    }

    const QPointF projected = start + (end - start) * qBound<qreal>(0.0, parameter, 1.0);
    return pointsNear(projected, point, tolerance);
}

std::optional<QPointF> segmentIntersectionPoint(const ReferencedAreaSegment &first,
                                                const ReferencedAreaSegment &second,
                                                qreal tolerance)
{
    const QPointF p = first.start;
    const QPointF r = first.end - first.start;
    const QPointF q = second.start;
    const QPointF s = second.end - second.start;
    const qreal denominator = r.x() * s.y() - r.y() * s.x();
    if (std::abs(denominator) <= tolerance) {
        return std::nullopt;
    }

    const QPointF qp = q - p;
    const qreal t = (qp.x() * s.y() - qp.y() * s.x()) / denominator;
    const qreal u = (qp.x() * r.y() - qp.y() * r.x()) / denominator;
    if (t < -tolerance || t > 1.0 + tolerance || u < -tolerance || u > 1.0 + tolerance) {
        return std::nullopt;
    }

    return p + r * qBound<qreal>(0.0, t, 1.0);
}

QPointF pointAtLineDistance(const ReferencedLinePath &path, qreal distance)
{
    if (path.points.isEmpty()) {
        return {};
    }
    if (distance <= 0.0) {
        return path.points.first();
    }
    if (distance >= path.cumulativeLengths.last()) {
        return path.points.last();
    }

    for (int index = 1; index < path.points.size(); ++index) {
        const qreal previousDistance = path.cumulativeLengths.at(index - 1);
        const qreal currentDistance = path.cumulativeLengths.at(index);
        if (distance > currentDistance) {
            continue;
        }
        const qreal span = currentDistance - previousDistance;
        const qreal t = span <= std::numeric_limits<qreal>::epsilon()
            ? 0.0
            : (distance - previousDistance) / span;
        return path.points.at(index - 1) + (path.points.at(index) - path.points.at(index - 1)) * t;
    }

    return path.points.last();
}

void appendUniquePoint(QVector<QPointF> *points, const QPointF &point, qreal tolerance)
{
    if (points == nullptr) {
        return;
    }
    if (!points->isEmpty() && pointsNear(points->last(), point, tolerance)) {
        return;
    }
    points->append(point);
}

void appendLinePathSegment(QVector<QPointF> *points,
                           const ReferencedLinePath &path,
                           qreal startDistance,
                           qreal endDistance,
                           qreal tolerance)
{
    if (points == nullptr || path.points.size() < 2) {
        return;
    }

    const bool forward = startDistance <= endDistance;
    appendUniquePoint(points, pointAtLineDistance(path, startDistance), tolerance);
    if (forward) {
        for (int index = 1; index + 1 < path.points.size(); ++index) {
            const qreal distance = path.cumulativeLengths.at(index);
            if (distance > startDistance + tolerance && distance < endDistance - tolerance) {
                appendUniquePoint(points, path.points.at(index), tolerance);
            }
        }
    } else {
        for (int index = path.points.size() - 2; index > 0; --index) {
            const qreal distance = path.cumulativeLengths.at(index);
            if (distance < startDistance - tolerance && distance > endDistance + tolerance) {
                appendUniquePoint(points, path.points.at(index), tolerance);
            }
        }
    }
    appendUniquePoint(points, pointAtLineDistance(path, endDistance), tolerance);
}

QVector<AdjacentLineIntersection> adjacentLineIntersections(const ReferencedLinePath &first,
                                                           const ReferencedLinePath &second,
                                                           qreal tolerance)
{
    QVector<AdjacentLineIntersection> intersections;
    if (first.points.size() < 2 || second.points.size() < 2) {
        return intersections;
    }

    for (int firstIndex = 1; firstIndex < first.points.size(); ++firstIndex) {
        const ReferencedAreaSegment firstSegment{first.points.at(firstIndex - 1), first.points.at(firstIndex)};
        const qreal firstSegmentLength = first.cumulativeLengths.at(firstIndex) - first.cumulativeLengths.at(firstIndex - 1);
        for (int secondIndex = 1; secondIndex < second.points.size(); ++secondIndex) {
            const ReferencedAreaSegment secondSegment{second.points.at(secondIndex - 1), second.points.at(secondIndex)};
            const qreal secondSegmentLength = second.cumulativeLengths.at(secondIndex) - second.cumulativeLengths.at(secondIndex - 1);
            const std::optional<QPointF> intersection = segmentIntersectionPoint(firstSegment, secondSegment, tolerance);
            if (!intersection.has_value()) {
                continue;
            }

            AdjacentLineIntersection candidate;
            candidate.point = *intersection;
            candidate.firstLineDistance = first.cumulativeLengths.at(firstIndex - 1)
                + segmentParameter(firstSegment.start, firstSegment.end, *intersection) * firstSegmentLength;
            candidate.secondLineDistance = second.cumulativeLengths.at(secondIndex - 1)
                + segmentParameter(secondSegment.start, secondSegment.end, *intersection) * secondSegmentLength;

            bool duplicate = false;
            for (const AdjacentLineIntersection &existing : std::as_const(intersections)) {
                if (pointsNear(existing.point, candidate.point, tolerance * 10.0)) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                intersections.append(candidate);
            }
        }
    }

    return intersections;
}

qreal polygonSignedArea(const QVector<QPointF> &points)
{
    if (points.size() < 3) {
        return 0.0;
    }

    qreal area = 0.0;
    for (int index = 0; index < points.size(); ++index) {
        const QPointF &current = points.at(index);
        const QPointF &next = points.at((index + 1) % points.size());
        area += current.x() * next.y() - next.x() * current.y();
    }
    return area * 0.5;
}

int nodeIndexForPoint(QVector<QPointF> *nodes, const QPointF &point, qreal tolerance)
{
    if (nodes == nullptr) {
        return -1;
    }
    for (int index = 0; index < nodes->size(); ++index) {
        if (pointsNear(nodes->at(index), point, tolerance)) {
            return index;
        }
    }
    nodes->append(point);
    return nodes->size() - 1;
}

QVector<QPointF> orderedAreaVerticesFromReferencedIntersections(const QVector<MapGeometryFeature> &lineFeatures)
{
    const int lineCount = lineFeatures.size();
    if (lineCount < 2) {
        return {};
    }

    QVector<ReferencedLinePath> paths;
    QVector<QPointF> allPoints;
    paths.reserve(lineCount);
    for (const MapGeometryFeature &feature : lineFeatures) {
        ReferencedLinePath path = referencedLinePath(feature);
        if (path.points.size() < 2) {
            return {};
        }
        allPoints += path.points;
        paths.append(path);
    }

    const qreal tolerance = sourceToleranceForPoints(allPoints);
    if (lineCount == 2) {
        const QVector<AdjacentLineIntersection> intersections = adjacentLineIntersections(paths.at(0), paths.at(1), tolerance);
        if (intersections.size() < 2) {
            return {};
        }

        QVector<QPointF> bestPolygon;
        qreal bestArea = 0.0;
        for (int firstIndex = 0; firstIndex < intersections.size(); ++firstIndex) {
            for (int secondIndex = firstIndex + 1; secondIndex < intersections.size(); ++secondIndex) {
                QVector<QPointF> polygon;
                appendLinePathSegment(&polygon,
                                      paths.at(0),
                                      intersections.at(firstIndex).firstLineDistance,
                                      intersections.at(secondIndex).firstLineDistance,
                                      tolerance);
                appendLinePathSegment(&polygon,
                                      paths.at(1),
                                      intersections.at(secondIndex).secondLineDistance,
                                      intersections.at(firstIndex).secondLineDistance,
                                      tolerance);
                const qreal area = std::abs(polygonSignedArea(polygon));
                if (polygon.size() >= 3 && area > bestArea) {
                    bestPolygon = polygon;
                    bestArea = area;
                }
            }
        }
        return bestPolygon;
    }

    QVector<QVector<AdjacentLineIntersection>> pairIntersections;
    pairIntersections.reserve(lineCount);
    for (int index = 0; index < lineCount; ++index) {
        QVector<AdjacentLineIntersection> intersections =
            adjacentLineIntersections(paths.at(index), paths.at((index + 1) % lineCount), tolerance);
        if (intersections.isEmpty()) {
            return {};
        }
        pairIntersections.append(intersections);
    }

    QVector<int> selectedIndexes(lineCount, 0);
    QVector<QPointF> bestPolygon;
    qreal bestArea = 0.0;
    std::function<void(int)> enumerate = [&](int pairIndex) {
        if (pairIndex == lineCount) {
            QVector<QPointF> polygon;
            for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
                const int previousPairIndex = (lineIndex + lineCount - 1) % lineCount;
                const AdjacentLineIntersection &startIntersection =
                    pairIntersections.at(previousPairIndex).at(selectedIndexes.at(previousPairIndex));
                const AdjacentLineIntersection &endIntersection =
                    pairIntersections.at(lineIndex).at(selectedIndexes.at(lineIndex));
                appendLinePathSegment(&polygon,
                                      paths.at(lineIndex),
                                      startIntersection.secondLineDistance,
                                      endIntersection.firstLineDistance,
                                      tolerance);
            }
            if (polygon.size() >= 2 && pointsNear(polygon.first(), polygon.last(), tolerance)) {
                polygon.removeLast();
            }
            const qreal area = std::abs(polygonSignedArea(polygon));
            if (polygon.size() >= 3 && area > bestArea) {
                bestPolygon = polygon;
                bestArea = area;
            }
            return;
        }

        for (int index = 0; index < pairIntersections.at(pairIndex).size(); ++index) {
            selectedIndexes[pairIndex] = index;
            enumerate(pairIndex + 1);
        }
    };
    enumerate(0);

    return bestPolygon;
}

QVector<QPointF> areaVerticesFromReferencedIntersections(const QVector<MapGeometryFeature> &lineFeatures)
{
    struct SourceSegment
    {
        ReferencedAreaSegment segment;
        QVector<qreal> cuts;
    };
    struct UndirectedEdge
    {
        int from = -1;
        int to = -1;
    };
    struct HalfEdge
    {
        int from = -1;
        int to = -1;
        int reverse = -1;
        bool used = false;
    };

    QVector<SourceSegment> sourceSegments;
    QVector<QPointF> allPoints;
    for (const MapGeometryFeature &feature : lineFeatures) {
        const QVector<QPointF> points = sampledSourceLinePoints(feature);
        allPoints += points;
        for (int index = 1; index < points.size(); ++index) {
            if (points.at(index - 1) == points.at(index)) {
                continue;
            }
            sourceSegments.append(SourceSegment{ReferencedAreaSegment{points.at(index - 1), points.at(index)},
                                                QVector<qreal>{0.0, 1.0}});
        }
    }
    if (sourceSegments.size() < 3) {
        return {};
    }

    const qreal tolerance = sourceToleranceForPoints(allPoints);
    auto appendCut = [](QVector<qreal> *cuts, qreal value) {
        if (cuts == nullptr) {
            return;
        }
        const qreal bounded = qBound<qreal>(0.0, value, 1.0);
        for (qreal existing : std::as_const(*cuts)) {
            if (std::abs(existing - bounded) <= 1e-8) {
                return;
            }
        }
        cuts->append(bounded);
    };

    for (int firstIndex = 0; firstIndex < sourceSegments.size(); ++firstIndex) {
        for (int secondIndex = firstIndex + 1; secondIndex < sourceSegments.size(); ++secondIndex) {
            const ReferencedAreaSegment &first = sourceSegments[firstIndex].segment;
            const ReferencedAreaSegment &second = sourceSegments[secondIndex].segment;
            if (const std::optional<QPointF> intersection = segmentIntersectionPoint(first, second, tolerance)) {
                appendCut(&sourceSegments[firstIndex].cuts, segmentParameter(first.start, first.end, *intersection));
                appendCut(&sourceSegments[secondIndex].cuts, segmentParameter(second.start, second.end, *intersection));
            }
            for (const QPointF &point : {first.start, first.end}) {
                if (pointOnSegment(second.start, second.end, point, tolerance)) {
                    appendCut(&sourceSegments[secondIndex].cuts, segmentParameter(second.start, second.end, point));
                }
            }
            for (const QPointF &point : {second.start, second.end}) {
                if (pointOnSegment(first.start, first.end, point, tolerance)) {
                    appendCut(&sourceSegments[firstIndex].cuts, segmentParameter(first.start, first.end, point));
                }
            }
        }
    }

    QVector<QPointF> nodes;
    QVector<UndirectedEdge> edges;
    for (SourceSegment &sourceSegment : sourceSegments) {
        std::sort(sourceSegment.cuts.begin(), sourceSegment.cuts.end());
        for (int index = 1; index < sourceSegment.cuts.size(); ++index) {
            const qreal startCut = sourceSegment.cuts.at(index - 1);
            const qreal endCut = sourceSegment.cuts.at(index);
            if (std::abs(endCut - startCut) <= 1e-8) {
                continue;
            }
            const QPointF start = sourceSegment.segment.start
                + (sourceSegment.segment.end - sourceSegment.segment.start) * startCut;
            const QPointF end = sourceSegment.segment.start
                + (sourceSegment.segment.end - sourceSegment.segment.start) * endCut;
            if (pointsNear(start, end, tolerance)) {
                continue;
            }
            const int from = nodeIndexForPoint(&nodes, start, tolerance);
            const int to = nodeIndexForPoint(&nodes, end, tolerance);
            if (from >= 0 && to >= 0 && from != to) {
                edges.append(UndirectedEdge{from, to});
            }
        }
    }

    QVector<bool> activeEdges(edges.size(), true);
    bool changed = true;
    while (changed) {
        changed = false;
        QVector<int> degrees(nodes.size(), 0);
        for (int edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            if (!activeEdges.at(edgeIndex)) {
                continue;
            }
            ++degrees[edges.at(edgeIndex).from];
            ++degrees[edges.at(edgeIndex).to];
        }
        for (int edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            if (!activeEdges.at(edgeIndex)) {
                continue;
            }
            if (degrees.at(edges.at(edgeIndex).from) <= 1 || degrees.at(edges.at(edgeIndex).to) <= 1) {
                activeEdges[edgeIndex] = false;
                changed = true;
            }
        }
    }

    QVector<HalfEdge> halfEdges;
    QVector<QVector<int>> outgoing(nodes.size());
    for (int edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        if (!activeEdges.at(edgeIndex)) {
            continue;
        }
        const int first = halfEdges.size();
        const int second = first + 1;
        halfEdges.append(HalfEdge{edges.at(edgeIndex).from, edges.at(edgeIndex).to, second, false});
        halfEdges.append(HalfEdge{edges.at(edgeIndex).to, edges.at(edgeIndex).from, first, false});
        outgoing[edges.at(edgeIndex).from].append(first);
        outgoing[edges.at(edgeIndex).to].append(second);
    }
    if (halfEdges.size() < 6) {
        return {};
    }

    for (QVector<int> &nodeEdges : outgoing) {
        std::sort(nodeEdges.begin(), nodeEdges.end(), [&](int left, int right) {
            const QPointF leftVector = nodes.at(halfEdges.at(left).to) - nodes.at(halfEdges.at(left).from);
            const QPointF rightVector = nodes.at(halfEdges.at(right).to) - nodes.at(halfEdges.at(right).from);
            return std::atan2(leftVector.y(), leftVector.x()) < std::atan2(rightVector.y(), rightVector.x());
        });
    }

    QVector<QPointF> selectedFace;
    qreal selectedArea = std::numeric_limits<qreal>::max();
    for (int startEdge = 0; startEdge < halfEdges.size(); ++startEdge) {
        if (halfEdges.at(startEdge).used) {
            continue;
        }

        QVector<int> faceNodeIndexes;
        int currentEdge = startEdge;
        for (int guard = 0; guard < halfEdges.size() + 1; ++guard) {
            if (halfEdges.at(currentEdge).used) {
                break;
            }
            halfEdges[currentEdge].used = true;
            faceNodeIndexes.append(halfEdges.at(currentEdge).from);
            const int atNode = halfEdges.at(currentEdge).to;
            const int reverseEdge = halfEdges.at(currentEdge).reverse;
            const QVector<int> &nodeEdges = outgoing.at(atNode);
            const int reversePosition = nodeEdges.indexOf(reverseEdge);
            if (reversePosition < 0 || nodeEdges.isEmpty()) {
                break;
            }
            currentEdge = nodeEdges.at((reversePosition + nodeEdges.size() - 1) % nodeEdges.size());
            if (currentEdge == startEdge) {
                QVector<QPointF> facePoints;
                for (int nodeIndex : std::as_const(faceNodeIndexes)) {
                    facePoints.append(nodes.at(nodeIndex));
                }
                const qreal area = polygonSignedArea(facePoints);
                const qreal absoluteArea = std::abs(area);
                if (facePoints.size() >= 3 && area > tolerance && absoluteArea < selectedArea) {
                    selectedFace = facePoints;
                    selectedArea = absoluteArea;
                }
                break;
            }
        }
    }

    return selectedFace;
}
}

MapEditorReferencedAreaResolution resolveMapEditorReferencedArea(
    const QStringList &identifiers,
    const QHash<QString, MapGeometryFeature> &lineFeaturesByIdentifier)
{
    MapEditorReferencedAreaResolution resolution;
    QVector<MapGeometryFeature> referencedLineFeatures;
    QVector<QPointF> tolerancePoints;
    for (const QString &identifier : identifiers) {
        const QString normalizedIdentifier = identifier.trimmed().toLower();
        if (normalizedIdentifier.isEmpty()) {
            continue;
        }

        const auto featureIt = lineFeaturesByIdentifier.constFind(normalizedIdentifier);
        if (featureIt == lineFeaturesByIdentifier.constEnd()) {
            continue;
        }
        referencedLineFeatures.append(featureIt.value());
        for (const QPointF &point : std::as_const(featureIt.value().vertices)) {
            tolerancePoints.append(point);
        }
    }

    const qreal tolerance = sourceToleranceForPoints(tolerancePoints);
    if (referencedLineFeatures.size() == 1) {
        const MapGeometryFeature &lineFeature = referencedLineFeatures.first();
        if (lineFeature.closed && lineFeature.lineVertices.size() >= 2) {
            resolution.vertices = lineFeature.vertices;
            resolution.lineVertices = lineFeature.lineVertices;
            return resolution;
        }
    }

    bool closedByReferencedLines = false;
    for (const QString &identifier : identifiers) {
        const QString normalizedIdentifier = identifier.trimmed().toLower();
        if (normalizedIdentifier.isEmpty()) {
            continue;
        }

        const auto featureIt = lineFeaturesByIdentifier.constFind(normalizedIdentifier);
        if (featureIt == lineFeaturesByIdentifier.constEnd()) {
            continue;
        }

        const MapGeometryFeature &lineFeature = featureIt.value();
        if (lineFeature.lineVertices.size() < 2) {
            continue;
        }

        const bool appendReversed = !resolution.vertices.isEmpty()
            && pointsNear(resolution.vertices.last(), lineFeature.lineVertices.last().anchor, tolerance)
            && !pointsNear(resolution.vertices.last(), lineFeature.lineVertices.first().anchor, tolerance);
        const int existingVertexCount = resolution.vertices.size();
        for (int offset = 0; offset < lineFeature.lineVertices.size(); ++offset) {
            const int index = appendReversed ? (lineFeature.lineVertices.size() - 1 - offset) : offset;
            const QPointF anchor = lineFeature.lineVertices.at(index).anchor;
            if (!resolution.vertices.isEmpty() && pointsNear(resolution.vertices.last(), anchor, tolerance)) {
                continue;
            }
            resolution.vertices.append(anchor);
            MapGeometryFeature::TH2LineVertex vertex = lineFeature.lineVertices.at(index);
            if (appendReversed) {
                std::swap(vertex.incomingControl, vertex.outgoingControl);
                std::swap(vertex.incomingSourceVertexIndex, vertex.outgoingSourceVertexIndex);
            }
            resolution.lineVertices.append(vertex);
        }

        if (existingVertexCount > 0
            && !resolution.vertices.isEmpty()
            && pointsNear(resolution.vertices.first(), resolution.vertices.last(), tolerance)) {
            closedByReferencedLines = true;
            resolution.vertices.removeLast();
            if (!resolution.lineVertices.isEmpty()) {
                resolution.lineVertices.removeLast();
            }
        }
    }

    if (resolution.vertices.size() >= 3 && closedByReferencedLines) {
        return resolution;
    }

    QVector<QPointF> intersectionFace = orderedAreaVerticesFromReferencedIntersections(referencedLineFeatures);
    if (intersectionFace.size() < 3) {
        intersectionFace = areaVerticesFromReferencedIntersections(referencedLineFeatures);
    }
    if (intersectionFace.size() >= 3) {
        resolution.vertices = intersectionFace;
        resolution.lineVertices.clear();
        for (const QPointF &point : intersectionFace) {
            MapGeometryFeature::TH2LineVertex vertex;
            vertex.anchor = point;
            resolution.lineVertices.append(vertex);
        }
        return resolution;
    }

    resolution.vertices.clear();
    resolution.lineVertices.clear();
    return resolution;
}
}
