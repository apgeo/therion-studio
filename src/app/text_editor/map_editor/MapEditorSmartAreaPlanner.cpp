#include "MapEditorSmartAreaPlanner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include <QHash>
#include <QLineF>
#include <QPainterPath>
#include <QRectF>
#include <QSet>
#include <QSizeF>

namespace TherionStudio
{
namespace
{
struct SourceSegment
{
    QPointF start;
    QPointF end;
    int lineNumber = 0;
    QVector<qreal> cuts{0.0, 1.0};
};

struct UndirectedEdge
{
    int from = -1;
    int to = -1;
    int lineNumber = 0;
};

struct HalfEdge
{
    int from = -1;
    int to = -1;
    int reverse = -1;
    int lineNumber = 0;
    bool used = false;
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

    QRectF bounds(points.first(), QSizeF());
    for (const QPointF &point : points) {
        bounds |= QRectF(point, QSizeF());
    }
    return qMax<qreal>(1e-6, std::hypot(bounds.width(), bounds.height()) * 1e-7);
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
        const bool curved = previousVertex.outgoingControl.has_value() || currentVertex.incomingControl.has_value();
        if (curved) {
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

    if (feature.closed && !points.isEmpty() && !pointsNear(points.first(), points.last(), 1e-9)) {
        points.append(points.first());
    }
    return points;
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

std::optional<QPointF> segmentIntersectionPoint(const QPointF &firstStart,
                                                const QPointF &firstEnd,
                                                const QPointF &secondStart,
                                                const QPointF &secondEnd,
                                                qreal tolerance)
{
    const QPointF p = firstStart;
    const QPointF r = firstEnd - firstStart;
    const QPointF q = secondStart;
    const QPointF s = secondEnd - secondStart;
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

void appendCut(QVector<qreal> *cuts, qreal value)
{
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

QPainterPath polygonPath(const QVector<QPointF> &points)
{
    QPainterPath path;
    if (points.isEmpty()) {
        return path;
    }
    path.moveTo(points.first());
    for (int index = 1; index < points.size(); ++index) {
        path.lineTo(points.at(index));
    }
    path.closeSubpath();
    return path;
}

QString lineIdentifier(const MapGeometryFeature &feature)
{
    return feature.optionValues.value(QStringLiteral("id")).trimmed();
}

void appendCandidateIfContains(QVector<MapEditorSmartAreaCandidate> *candidates,
                               const QVector<QPointF> &vertices,
                               const QVector<int> &lineNumbers,
                               const QHash<int, QString> &lineIdentifiers,
                               int scrapLineNumber,
                               const QPointF &sourcePoint)
{
    if (candidates == nullptr || vertices.size() < 3 || scrapLineNumber <= 0) {
        return;
    }

    const qreal area = std::abs(polygonSignedArea(vertices));
    if (area <= 1e-8 || !polygonPath(vertices).contains(sourcePoint)) {
        return;
    }

    QSet<int> seenLines;
    MapEditorSmartAreaCandidate candidate;
    candidate.scrapLineNumber = scrapLineNumber;
    candidate.vertices = vertices;
    candidate.area = area;
    for (int lineNumber : lineNumbers) {
        if (lineNumber <= 0 || seenLines.contains(lineNumber)) {
            continue;
        }
        seenLines.insert(lineNumber);
        candidate.boundaryLines.append(MapEditorSmartAreaBoundaryLine{
            lineNumber,
            lineIdentifiers.value(lineNumber)
        });
    }
    if (!candidate.boundaryLines.isEmpty()) {
        candidates->append(candidate);
    }
}

QVector<MapEditorSmartAreaCandidate> arrangementFaceCandidatesForScrap(
    const QVector<MapGeometryFeature> &lineFeatures,
    const QPointF &sourcePoint)
{
    QVector<SourceSegment> sourceSegments;
    QVector<QPointF> allPoints;
    QHash<int, QString> lineIdentifiers;
    int scrapLineNumber = 0;

    for (const MapGeometryFeature &feature : lineFeatures) {
        if (feature.kind != MapGeometryFeature::Kind::Line || feature.lineNumber <= 0) {
            continue;
        }
        if (scrapLineNumber <= 0) {
            scrapLineNumber = feature.scrapLineNumber;
        }
        lineIdentifiers.insert(feature.lineNumber, lineIdentifier(feature));
        const QVector<QPointF> points = sampledSourceLinePoints(feature);
        allPoints += points;
        for (int index = 1; index < points.size(); ++index) {
            if (points.at(index - 1) == points.at(index)) {
                continue;
            }
            SourceSegment segment;
            segment.start = points.at(index - 1);
            segment.end = points.at(index);
            segment.lineNumber = feature.lineNumber;
            sourceSegments.append(segment);
        }
    }
    if (sourceSegments.size() < 3) {
        return {};
    }

    const qreal tolerance = sourceToleranceForPoints(allPoints);
    for (int firstIndex = 0; firstIndex < sourceSegments.size(); ++firstIndex) {
        for (int secondIndex = firstIndex + 1; secondIndex < sourceSegments.size(); ++secondIndex) {
            SourceSegment &first = sourceSegments[firstIndex];
            SourceSegment &second = sourceSegments[secondIndex];
            if (const std::optional<QPointF> intersection =
                    segmentIntersectionPoint(first.start, first.end, second.start, second.end, tolerance)) {
                appendCut(&first.cuts, segmentParameter(first.start, first.end, *intersection));
                appendCut(&second.cuts, segmentParameter(second.start, second.end, *intersection));
            }
            for (const QPointF &point : {first.start, first.end}) {
                if (pointOnSegment(second.start, second.end, point, tolerance)) {
                    appendCut(&second.cuts, segmentParameter(second.start, second.end, point));
                }
            }
            for (const QPointF &point : {second.start, second.end}) {
                if (pointOnSegment(first.start, first.end, point, tolerance)) {
                    appendCut(&first.cuts, segmentParameter(first.start, first.end, point));
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
            const QPointF start = sourceSegment.start + (sourceSegment.end - sourceSegment.start) * startCut;
            const QPointF end = sourceSegment.start + (sourceSegment.end - sourceSegment.start) * endCut;
            if (pointsNear(start, end, tolerance)) {
                continue;
            }
            const int from = nodeIndexForPoint(&nodes, start, tolerance);
            const int to = nodeIndexForPoint(&nodes, end, tolerance);
            if (from >= 0 && to >= 0 && from != to) {
                edges.append(UndirectedEdge{from, to, sourceSegment.lineNumber});
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
        halfEdges.append(HalfEdge{edges.at(edgeIndex).from, edges.at(edgeIndex).to, second, edges.at(edgeIndex).lineNumber, false});
        halfEdges.append(HalfEdge{edges.at(edgeIndex).to, edges.at(edgeIndex).from, first, edges.at(edgeIndex).lineNumber, false});
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

    QVector<MapEditorSmartAreaCandidate> candidates;
    for (int startEdge = 0; startEdge < halfEdges.size(); ++startEdge) {
        if (halfEdges.at(startEdge).used) {
            continue;
        }

        QVector<int> faceNodeIndexes;
        QVector<int> faceLineNumbers;
        int currentEdge = startEdge;
        for (int guard = 0; guard < halfEdges.size() + 1; ++guard) {
            if (halfEdges.at(currentEdge).used) {
                break;
            }
            halfEdges[currentEdge].used = true;
            faceNodeIndexes.append(halfEdges.at(currentEdge).from);
            faceLineNumbers.append(halfEdges.at(currentEdge).lineNumber);
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
                if (polygonSignedArea(facePoints) > tolerance) {
                    appendCandidateIfContains(&candidates,
                                              facePoints,
                                              faceLineNumbers,
                                              lineIdentifiers,
                                              scrapLineNumber,
                                              sourcePoint);
                }
                break;
            }
        }
    }
    return candidates;
}
}

QVector<MapEditorSmartAreaCandidate> mapEditorSmartAreaCandidatesAt(
    const QVector<MapGeometryFeature> &geometryFeatures,
    const QPointF &sourcePoint)
{
    QHash<int, QVector<MapGeometryFeature>> lineFeaturesByScrap;
    for (const MapGeometryFeature &feature : geometryFeatures) {
        if (feature.kind == MapGeometryFeature::Kind::Line
            && feature.scrapLineNumber > 0
            && feature.lineVertices.size() >= 2) {
            lineFeaturesByScrap[feature.scrapLineNumber].append(feature);
        }
    }

    QVector<MapEditorSmartAreaCandidate> candidates;
    for (auto it = lineFeaturesByScrap.constBegin(); it != lineFeaturesByScrap.constEnd(); ++it) {
        QHash<int, QString> lineIdentifiers;
        for (const MapGeometryFeature &feature : it.value()) {
            lineIdentifiers.insert(feature.lineNumber, lineIdentifier(feature));
            if (!feature.closed) {
                continue;
            }
            const QVector<QPointF> closedLinePoints = sampledSourceLinePoints(feature);
            appendCandidateIfContains(&candidates,
                                      closedLinePoints,
                                      QVector<int>{feature.lineNumber},
                                      lineIdentifiers,
                                      feature.scrapLineNumber,
                                      sourcePoint);
        }

        candidates += arrangementFaceCandidatesForScrap(it.value(), sourcePoint);
    }

    std::sort(candidates.begin(), candidates.end(), [](const MapEditorSmartAreaCandidate &left,
                                                       const MapEditorSmartAreaCandidate &right) {
        if (!qFuzzyCompare(left.area, right.area)) {
            return left.area < right.area;
        }
        return left.boundaryLines.size() < right.boundaryLines.size();
    });
    return candidates;
}
}
