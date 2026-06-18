#include "ThreeDViewerViewportItem.h"

#include "ThreeDViewerProjection.h"

#include "../../core/ThreeDViewerSceneStatistics.h"

#include <QCoreApplication>
#include <QHoverEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QRectF>
#include <QMutexLocker>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>
#include <QSGTextNode>
#include <QTextLayout>
#include <QWheelEvent>
#include <QLocale>

#include <algorithm>
#include <cmath>
#include <utility>

namespace TherionStudio
{
namespace
{
constexpr int kCenterlineLayer = 0;
constexpr int kStationsLayer = 1;
constexpr int kLabelsLayer = 2;
constexpr int kMeshesLayer = 3;
constexpr int kSurfacesLayer = 4;
constexpr double kPi = 3.14159265358979323846;
constexpr qreal kStationLabelOffsetX = 8.0;
constexpr qreal kStationLabelOffsetY = -8.0;
constexpr qreal kStationLabelPadding = 4.0;
constexpr qreal kStationMarkerDeclutterRadius = 5.5;

QColor translucentColor(const QColor &color, int alpha)
{
    QColor copy = color;
    copy.setAlpha(alpha);
    return copy;
}

QSGGeometryNode *createGeometryNode(const QVector<QPointF> &points,
                                    QSGGeometry::DrawingMode drawingMode,
                                    const QColor &color,
                                    qreal lineWidth = 1.0)
{
    if (points.isEmpty()) {
        return nullptr;
    }

    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), points.size());
    geometry->setDrawingMode(drawingMode);
    geometry->setLineWidth(lineWidth);
    auto *vertexData = geometry->vertexDataAsPoint2D();
    for (qsizetype index = 0; index < points.size(); ++index) {
        vertexData[index].set(points.at(index).x(), points.at(index).y());
    }

    auto *material = new QSGFlatColorMaterial();
    material->setColor(color);

    auto *node = new QSGGeometryNode();
    node->setGeometry(geometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

void appendGeometryNode(QSGNode *root,
                        const QVector<QPointF> &points,
                        QSGGeometry::DrawingMode drawingMode,
                        const QColor &color,
                        qreal lineWidth = 1.0)
{
    if (QSGGeometryNode *node = createGeometryNode(points, drawingMode, color, lineWidth)) {
        root->appendChildNode(node);
    }
}

void appendSolidRect(QSGNode *root, const QRectF &rect, const QColor &color)
{
    QVector<QPointF> points;
    points.reserve(4);
    points << rect.topLeft()
           << rect.bottomLeft()
           << rect.topRight()
           << rect.bottomRight();
    appendGeometryNode(root, points, QSGGeometry::DrawTriangleStrip, color, 1.0);
}

QColor altitudeColor(double normalized)
{
    const double clamped = std::clamp(normalized, 0.0, 1.0);
    const double hue = 0.78 - clamped * 0.78;
    const double value = 0.95 - clamped * 0.15;
    return QColor::fromHsvF(hue, 1.0, value, 0.95);
}

ThreeDViewerVec3 boundsCenter(const ThreeDViewerBounds &bounds)
{
    return {
        (bounds.minimum.x + bounds.maximum.x) * 0.5,
        (bounds.minimum.y + bounds.maximum.y) * 0.5,
        (bounds.minimum.z + bounds.maximum.z) * 0.5,
    };
}

void appendClosedPolyline(QSGNode *root,
                          const QVector<QPointF> &points,
                          const QColor &color,
                          qreal lineWidth = 1.0)
{
    if (points.size() < 2) {
        return;
    }

    QVector<QPointF> closedPoints = points;
    closedPoints.append(points.front());
    appendGeometryNode(root, closedPoints, QSGGeometry::DrawLineStrip, color, lineWidth);
}

void appendTextNode(QSGNode *root,
                    QQuickWindow *window,
                    const QString &text,
                    const QPointF &position,
                    const QColor &color,
                    const QFont &font)
{
    if (window == nullptr || text.isEmpty()) {
        return;
    }

    QSGTextNode *textNode = window->createTextNode();
    if (textNode == nullptr) {
        return;
    }

    textNode->setColor(color);
    textNode->setTextStyle(QSGTextNode::Normal);
    textNode->setRenderType(QSGTextNode::QtRendering);

    QTextLayout layout(text, font);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    line.setLineWidth(10000.0);
    layout.endLayout();
    textNode->addTextLayout(position, &layout);
    root->appendChildNode(textNode);
}

void appendTextNodeWithShadow(QSGNode *root,
                              QQuickWindow *window,
                              const QString &text,
                              const QPointF &position,
                              const QColor &color,
                              const QFont &font,
                              const QPointF &shadowOffset = QPointF(1.0, 1.0))
{
    appendTextNode(root,
                   window,
                   text,
                   position + shadowOffset,
                   QColor(0, 0, 0, 220),
                   font);
    appendTextNode(root, window, text, position, color, font);
}

QRectF textBoundsAt(const QString &text, const QFont &font, const QPointF &position)
{
    const QFontMetricsF metrics(font);
    QRectF bounds = metrics.boundingRect(text);
    bounds.moveTopLeft(position);
    return bounds.adjusted(-kStationLabelPadding,
                           -kStationLabelPadding,
                           kStationLabelPadding,
                           kStationLabelPadding);
}

bool intersectsAny(const QRectF &candidate, const QVector<QRectF> &occupied)
{
    for (const QRectF &rect : occupied) {
        if (candidate.intersects(rect)) {
            return true;
        }
    }
    return false;
}

QRectF markerBoundsAt(const QPointF &center)
{
    return QRectF(center.x() - kStationMarkerDeclutterRadius,
                  center.y() - kStationMarkerDeclutterRadius,
                  kStationMarkerDeclutterRadius * 2.0,
                  kStationMarkerDeclutterRadius * 2.0);
}

QPointF centeredTextOrigin(const QFont &font, const QString &text, const QPointF &center)
{
    const QFontMetricsF metrics(font);
    const QRectF bounds = metrics.boundingRect(text);
    return QPointF(center.x() - bounds.width() * 0.5,
                   center.y() - bounds.height() * 0.5);
}

QVector<QPointF> projectPolyline(const ThreeDViewerCamera &camera,
                                 const QVector<ThreeDViewerVec3> &points,
                                 int viewportWidth,
                                 int viewportHeight)
{
    QVector<QPointF> projectedPoints;
    projectedPoints.reserve(points.size());
    for (const ThreeDViewerVec3 &point : points) {
        const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera, point, viewportWidth, viewportHeight);
        if (!projected.visible) {
            return {};
        }
        projectedPoints.push_back(projected.screenPosition);
    }
    return projectedPoints;
}

void appendBoundingBox(QSGNode *root,
                       const ThreeDViewerCamera &camera,
                       const ThreeDViewerBounds &bounds,
                       int viewportWidth,
                       int viewportHeight,
                       const QColor &color)
{
    if (!bounds.valid) {
        return;
    }

    const ThreeDViewerVec3 min = bounds.minimum;
    const ThreeDViewerVec3 max = bounds.maximum;
    const std::array<std::pair<ThreeDViewerVec3, ThreeDViewerVec3>, 12> edges = {
        std::make_pair(ThreeDViewerVec3{min.x, min.y, min.z}, ThreeDViewerVec3{max.x, min.y, min.z}),
        std::make_pair(ThreeDViewerVec3{max.x, min.y, min.z}, ThreeDViewerVec3{max.x, max.y, min.z}),
        std::make_pair(ThreeDViewerVec3{max.x, max.y, min.z}, ThreeDViewerVec3{min.x, max.y, min.z}),
        std::make_pair(ThreeDViewerVec3{min.x, max.y, min.z}, ThreeDViewerVec3{min.x, min.y, min.z}),

        std::make_pair(ThreeDViewerVec3{min.x, min.y, max.z}, ThreeDViewerVec3{max.x, min.y, max.z}),
        std::make_pair(ThreeDViewerVec3{max.x, min.y, max.z}, ThreeDViewerVec3{max.x, max.y, max.z}),
        std::make_pair(ThreeDViewerVec3{max.x, max.y, max.z}, ThreeDViewerVec3{min.x, max.y, max.z}),
        std::make_pair(ThreeDViewerVec3{min.x, max.y, max.z}, ThreeDViewerVec3{min.x, min.y, max.z}),

        std::make_pair(ThreeDViewerVec3{min.x, min.y, min.z}, ThreeDViewerVec3{min.x, min.y, max.z}),
        std::make_pair(ThreeDViewerVec3{max.x, min.y, min.z}, ThreeDViewerVec3{max.x, min.y, max.z}),
        std::make_pair(ThreeDViewerVec3{max.x, max.y, min.z}, ThreeDViewerVec3{max.x, max.y, max.z}),
        std::make_pair(ThreeDViewerVec3{min.x, max.y, min.z}, ThreeDViewerVec3{min.x, max.y, max.z}),
    };

    for (const auto &edge : edges) {
        QPointF fromScreen;
        QPointF toScreen;
        if (!ThreeDViewerProjection::projectLine(camera, edge.first, edge.second, viewportWidth, viewportHeight, &fromScreen, &toScreen)) {
            continue;
        }

        QVector<QPointF> linePoints;
        linePoints << fromScreen << toScreen;
        appendGeometryNode(root, linePoints, QSGGeometry::DrawLines, color, 1.0);
    }
}

double niceScaleLength(double targetLength)
{
    if (targetLength <= 0.0) {
        return 1.0;
    }

    const double exponent = std::floor(std::log10(targetLength));
    const double base = std::pow(10.0, exponent);
    const double normalized = targetLength / base;
    const double step = normalized < 1.5 ? 1.0 : normalized < 3.5 ? 2.0 : normalized < 7.5 ? 5.0 : 10.0;
    return step * base;
}

void appendScaleBar(QSGNode *root,
                    QQuickWindow *window,
                    const ThreeDViewerCamera &camera,
                    const ThreeDViewerBounds &bounds,
                    const QPointF &origin,
                    int viewportWidth,
                    int viewportHeight)
{
    if (window == nullptr || !bounds.valid) {
        return;
    }

    const ThreeDViewerVec3 sceneCenter = boundsCenter(bounds);
    const ThreeDViewerVec3 cameraPosition = camera.position();
    const ThreeDViewerVec3 forward = camera.forwardVector();
    const ThreeDViewerVec3 toCenter{
        sceneCenter.x - cameraPosition.x,
        sceneCenter.y - cameraPosition.y,
        sceneCenter.z - cameraPosition.z,
    };
    const double sceneDepth = std::max(1.0, toCenter.x * forward.x + toCenter.y * forward.y + toCenter.z * forward.z);
    const double worldPerPixel = 2.0 * sceneDepth * std::tan(ThreeDViewerCamera::defaultFieldOfViewRadians() * 0.5) / std::max(1, viewportHeight);
    const double targetPixels = std::clamp(double(viewportWidth) * 0.18, 80.0, 180.0);
    const double scaleLength = niceScaleLength(worldPerPixel * targetPixels);
    const double barPixels = scaleLength / std::max(0.000001, worldPerPixel);
    const QPointF barLeft(origin.x(), origin.y() + 20.0);
    const QPointF barRight(origin.x() + barPixels, origin.y() + 20.0);
    const QPointF barTickTopLeft(origin.x(), origin.y() + 14.0);
    const QPointF barTickBottomLeft(origin.x(), origin.y() + 26.0);
    const QPointF barTickTopRight(origin.x() + barPixels, origin.y() + 14.0);
    const QPointF barTickBottomRight(origin.x() + barPixels, origin.y() + 26.0);

    QVector<QPointF> barLine;
    barLine << barLeft << barRight;
    appendGeometryNode(root, barLine, QSGGeometry::DrawLines, QColor(QStringLiteral("#1d4ed8")), 2.0);

    QVector<QPointF> leftTick;
    leftTick << barTickTopLeft << barTickBottomLeft;
    appendGeometryNode(root, leftTick, QSGGeometry::DrawLines, QColor(QStringLiteral("#60a5fa")), 1.0);

    QVector<QPointF> rightTick;
    rightTick << barTickTopRight << barTickBottomRight;
    appendGeometryNode(root, rightTick, QSGGeometry::DrawLines, QColor(QStringLiteral("#60a5fa")), 1.0);

    const QString label = QLocale::system().toString(scaleLength, 'f', scaleLength < 10.0 ? 1 : 0) + QStringLiteral(" m");
    appendTextNode(root,
                   window,
                   label,
                   centeredTextOrigin(QFont(QStringLiteral("Menlo"), 10), label, QPointF(barLeft.x() + barPixels * 0.5, barLeft.y() - 10.0)),
                   QColor(QStringLiteral("#38bdf8")),
                   QFont(QStringLiteral("Menlo"), 10));
}

double normalizeDegrees(double value)
{
    value = std::fmod(value, 360.0);
    if (value < 0.0) {
        value += 360.0;
    }
    return value;
}

double cameraHeadingDegrees(const ThreeDViewerCamera &camera)
{
    const ThreeDViewerVec3 forward = camera.forwardVector();
    return normalizeDegrees(std::atan2(forward.x, forward.y) * 180.0 / kPi);
}

double cameraInclinationDegrees(const ThreeDViewerCamera &camera)
{
    const ThreeDViewerVec3 forward = camera.forwardVector();
    return std::atan2(-forward.z, std::hypot(forward.x, forward.y)) * 180.0 / kPi;
}

QVector<QPointF> projectedArc(const QPointF &center, double radius, double startDegrees, double endDegrees, int segments = 20)
{
    QVector<QPointF> points;
    points.reserve(segments + 1);
    for (int index = 0; index <= segments; ++index) {
        const double t = double(index) / double(segments);
        const double angle = (startDegrees + (endDegrees - startDegrees) * t) * kPi / 180.0;
        points.push_back(QPointF(center.x() + std::cos(angle) * radius, center.y() + std::sin(angle) * radius));
    }
    return points;
}

QVector<QPointF> projectedCircle(const QPointF &center, double radius, int segments);

void appendCompassIndicator(QSGNode *root,
                            QQuickWindow *window,
                            const ThreeDViewerBounds &bounds,
                            const ThreeDViewerCamera &camera,
                            int viewportWidth,
                            int viewportHeight,
                            const QPointF &center)
{
    if (window == nullptr || !bounds.valid) {
        return;
    }

    Q_UNUSED(viewportHeight);

    const ThreeDViewerVec3 worldNorth{0.0, 1.0, 0.0};
    const ThreeDViewerVec3 sceneCenter = boundsCenter(bounds);
    const ThreeDViewerVec3 northPoint{sceneCenter.x + worldNorth.x * 100.0, sceneCenter.y + worldNorth.y * 100.0, sceneCenter.z + worldNorth.z * 100.0};
    const ThreeDViewerProjectedPoint projectedCenter = ThreeDViewerProjection::projectPoint(camera, sceneCenter, viewportWidth, viewportHeight);
    const ThreeDViewerProjectedPoint projectedNorth = ThreeDViewerProjection::projectPoint(camera, northPoint, viewportWidth, viewportHeight);

    QPointF arrow(0.0, -26.0);
    if (projectedCenter.visible && projectedNorth.visible) {
        arrow = projectedNorth.screenPosition - projectedCenter.screenPosition;
        const double length = std::hypot(arrow.x(), arrow.y());
        if (length > 0.001) {
            arrow = QPointF(arrow.x() / length * 26.0, arrow.y() / length * 26.0);
        } else {
            arrow = QPointF(0.0, -26.0);
        }
    }

    const QVector<QPointF> disk = projectedCircle(center, 18.0, 24);
    appendClosedPolyline(root, disk, QColor(QStringLiteral("#2563eb")), 1.0);

    QVector<QPointF> crossHorizontal;
    crossHorizontal << QPointF(center.x() - 18.0, center.y())
                    << QPointF(center.x() + 18.0, center.y());
    appendGeometryNode(root, crossHorizontal, QSGGeometry::DrawLines, QColor(QStringLiteral("#2563eb")), 1.0);

    QVector<QPointF> crossVertical;
    crossVertical << QPointF(center.x(), center.y() - 18.0)
                  << QPointF(center.x(), center.y() + 18.0);
    appendGeometryNode(root, crossVertical, QSGGeometry::DrawLines, QColor(QStringLiteral("#2563eb")), 1.0);

    const QPointF tip = center + arrow;
    QVector<QPointF> shaft;
    shaft << center << tip;
    appendGeometryNode(root, shaft, QSGGeometry::DrawLines, QColor(QStringLiteral("#f59e0b")), 2.4);

    appendTextNode(root,
                   window,
                   QStringLiteral("N"),
                   centeredTextOrigin(QFont(QStringLiteral("Menlo"), 12, QFont::Bold), QStringLiteral("N"), QPointF(center.x(), center.y() - 28.0)),
                   QColor(QStringLiteral("#f8fafc")),
                   QFont(QStringLiteral("Menlo"), 12, QFont::Bold));
    const QString headingText = QLocale::system().toString(cameraHeadingDegrees(camera), 'f', 0) + QStringLiteral("°");
    appendTextNode(root,
                   window,
                   headingText,
                   centeredTextOrigin(QFont(QStringLiteral("Menlo"), 9), headingText, QPointF(center.x(), center.y() + 30.0)),
                   QColor(QStringLiteral("#67e8f9")),
                   QFont(QStringLiteral("Menlo"), 9));
}

void appendViewAngleIndicator(QSGNode *root,
                              QQuickWindow *window,
                              const ThreeDViewerCamera &camera,
                              const QPointF &center)
{
    if (window == nullptr) {
        return;
    }

    const double signedViewAngle = std::clamp(cameraInclinationDegrees(camera), -90.0, 90.0);
    const QVector<QPointF> arc = projectedArc(center, 18.0, -90.0, 90.0, 18);
    appendGeometryNode(root, arc, QSGGeometry::DrawLineStrip, QColor(QStringLiteral("#2563eb")), 1.8);

    QVector<QPointF> closure;
    closure << QPointF(center.x(), center.y() - 18.0)
            << QPointF(center.x(), center.y() + 18.0);
    appendGeometryNode(root, closure, QSGGeometry::DrawLines, QColor(QStringLiteral("#2563eb")), 1.0);

    QVector<QPointF> axis;
    axis << QPointF(center.x(), center.y())
         << QPointF(center.x() + 18.0, center.y());
    appendGeometryNode(root, axis, QSGGeometry::DrawLines, QColor(QStringLiteral("#2563eb")), 1.0);

    const double radians = -signedViewAngle * kPi / 180.0;
    const double needleLength = 26.0;
    const QPointF tip(center.x() + std::cos(radians) * needleLength,
                      center.y() + std::sin(radians) * needleLength);
    QVector<QPointF> indicator;
    indicator << center << tip;
    appendGeometryNode(root, indicator, QSGGeometry::DrawLines, QColor(QStringLiteral("#f59e0b")), 2.4);

    const QPointF headLeft(-std::sin(radians) * 0.14 * needleLength, std::cos(radians) * 0.14 * needleLength);
    const QPointF headRight(std::sin(radians) * 0.14 * needleLength, -std::cos(radians) * 0.14 * needleLength);
    QVector<QPointF> head;
    head << tip
         << tip + headLeft
         << tip + headRight;
    appendGeometryNode(root, head, QSGGeometry::DrawTriangles, QColor(QStringLiteral("#f97316")), 1.0);

    appendTextNode(root,
                   window,
                   QLocale::system().toString(signedViewAngle, 'f', 0) + QStringLiteral("°"),
                   centeredTextOrigin(QFont(QStringLiteral("Menlo"), 9), QLocale::system().toString(signedViewAngle, 'f', 0) + QStringLiteral("°"), QPointF(center.x(), center.y() + 30.0)),
                   QColor(QStringLiteral("#67e8f9")),
                   QFont(QStringLiteral("Menlo"), 9));
}

void appendAltitudeLegend(QSGNode *root,
                          QQuickWindow *window,
                          const ThreeDViewerBounds &bounds,
                          const QPointF &origin)
{
    if (window == nullptr || !bounds.valid) {
        return;
    }

    const QRectF legendRect(origin, QSizeF(16.0, 180.0));
    const int steps = 24;
    for (int index = 0; index < steps; ++index) {
        const double top = legendRect.top() + legendRect.height() * double(index) / double(steps);
        const double bottom = legendRect.top() + legendRect.height() * double(index + 1) / double(steps);
        const double normalized = 1.0 - double(index) / double(steps - 1);
        appendSolidRect(root, QRectF(legendRect.left(), top, legendRect.width(), bottom - top), altitudeColor(normalized));
    }
    QVector<QPointF> legendBorder;
    legendBorder << legendRect.topLeft() << legendRect.topRight() << legendRect.bottomRight() << legendRect.bottomLeft();
    appendClosedPolyline(root, legendBorder, QColor(QStringLiteral("#e2e8f0")), 1.0);

    const QFont titleFont(QStringLiteral("Menlo"), 9, QFont::Bold);
    appendTextNode(root,
                   window,
                   QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Altitude"),
                   QPointF(legendRect.left() - 2.0, legendRect.top() - 18.0),
                   QColor(QStringLiteral("#38bdf8")),
                   titleFont);

    const QFont labelFont(QStringLiteral("Menlo"), 8);
    const double minimum = bounds.minimum.z;
    const double maximum = bounds.maximum.z;
    const double range = maximum - minimum;
    const std::array<double, 5> values = {
        maximum,
        maximum - range * 0.25,
        maximum - range * 0.50,
        maximum - range * 0.75,
        minimum,
    };
    const std::array<QPointF, 5> positions = {
        QPointF(legendRect.right() + 8.0, legendRect.top() - 4.0),
        QPointF(legendRect.right() + 8.0, legendRect.top() + legendRect.height() * 0.25 - 4.0),
        QPointF(legendRect.right() + 8.0, legendRect.center().y() - 4.0),
        QPointF(legendRect.right() + 8.0, legendRect.top() + legendRect.height() * 0.75 - 4.0),
        QPointF(legendRect.right() + 8.0, legendRect.bottom() - 8.0),
    };
    for (size_t index = 0; index < values.size(); ++index) {
        const double value = values[index];
        const QPointF position = positions[index];
        const QString label = QLocale::system().toString(value, 'f', std::abs(value) < 10.0 ? 1 : 0) + QStringLiteral(" m");
        appendTextNode(root,
                       window,
                       label,
                       position,
                       QColor(QStringLiteral("#cbd5e1")),
                       labelFont);
    }
}

qreal appendTextCard(QSGNode *root,
                     QQuickWindow *window,
                     const QString &title,
                     const QString &body,
                     const QPointF &position,
                     qreal width);

QString formatMeters(double value)
{
    return QLocale::system().toString(value, 'f', std::abs(value) < 10.0 ? 1 : 0) + QStringLiteral(" m");
}

qreal measureTextCardHeight(const QString &title, const QString &body)
{
    const QStringList lines = body.split(QLatin1Char('\n'));
    const QFont titleFont(QStringLiteral("Menlo"), 11, QFont::Bold);
    const QFont bodyFont(QStringLiteral("Menlo"), 10);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics bodyMetrics(bodyFont);
    const qreal paddingTop = 12.0;
    const qreal paddingBottom = 12.0;
    const qreal titleGap = 9.0;
    const qreal bodyLineHeight = bodyMetrics.height();
    qreal bodyHeight = 0.0;
    for (const QString &line : lines) {
        bodyHeight += line.isEmpty() ? bodyLineHeight * 0.7 : bodyLineHeight;
    }
    return paddingTop + titleMetrics.height() + titleGap + bodyHeight + paddingBottom;
}

void appendSceneStatisticsOverlay(QSGNode *root,
                                  QQuickWindow *window,
                                  const ThreeDViewerSceneModel &sceneModel,
                                  int viewportWidth,
                                  int viewportHeight)
{
    Q_UNUSED(viewportWidth);
    Q_UNUSED(viewportHeight);

    const ThreeDViewerSceneStatistics statistics = computeThreeDViewerSceneStatistics(sceneModel);
    if (sceneModel.isEmpty() || (statistics.caveLengthMeters <= 0.0 && statistics.caveDepthMeters <= 0.0)) {
        return;
    }

    const QString title = statistics.sceneTitle.isEmpty()
        ? QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Scene")
        : statistics.sceneTitle;
    const QFont titleFont(QStringLiteral("Menlo"), 11, QFont::Bold);
    const QFont bodyFont(QStringLiteral("Menlo"), 10);
    const QColor accentColor(QStringLiteral("#38bdf8"));
    const QColor bodyColor(QStringLiteral("#cbd5e1"));
    const qreal left = 20.0;
    const qreal top = 20.0;
    qreal currentY = top;

    appendTextNode(root,
                   window,
                   title,
                   QPointF(left, currentY),
                   accentColor,
                   titleFont);
    currentY += 22.0;

    appendTextNode(root,
                   window,
                   QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Cave length")
                       + QStringLiteral(": ")
                       + formatMeters(statistics.caveLengthMeters),
                   QPointF(left, currentY),
                   bodyColor,
                   bodyFont);
    currentY += 18.0;

    appendTextNode(root,
                   window,
                   QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Cave depth")
                       + QStringLiteral(": ")
                       + formatMeters(statistics.caveDepthMeters),
                   QPointF(left, currentY),
                   bodyColor,
                   bodyFont);
}

qreal appendTextCard(QSGNode *root,
                     QQuickWindow *window,
                     const QString &title,
                     const QString &body,
                     const QPointF &position,
                     qreal width)
{
    if (window == nullptr || title.isEmpty() || body.isEmpty()) {
        return 0.0;
    }

    const QStringList lines = body.split(QLatin1Char('\n'));
    const QFont titleFont(QStringLiteral("Menlo"), 11, QFont::Bold);
    const QFont bodyFont(QStringLiteral("Menlo"), 10);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics bodyMetrics(bodyFont);
    const qreal paddingLeft = 12.0;
    const qreal paddingRight = 12.0;
    const qreal paddingTop = 12.0;
    const qreal paddingBottom = 12.0;
    const qreal titleGap = 9.0;
    const qreal bodyLineHeight = bodyMetrics.height();
    qreal bodyHeight = 0.0;
    for (const QString &line : lines) {
        bodyHeight += line.isEmpty() ? bodyLineHeight * 0.7 : bodyLineHeight;
    }
    const qreal cardHeight = paddingTop + titleMetrics.height() + titleGap + bodyHeight + paddingBottom;
    const QRectF cardRect(position, QSizeF(width, cardHeight));

    appendSolidRect(root, cardRect, QColor(0, 0, 0, 190));
    QVector<QPointF> border;
    border << cardRect.topLeft() << cardRect.topRight() << cardRect.bottomRight() << cardRect.bottomLeft();
    appendClosedPolyline(root, border, QColor(255, 255, 255, 50), 1.0);

    appendTextNode(root,
                   window,
                   title,
                   cardRect.topLeft() + QPointF(paddingLeft, paddingTop),
                   QColor(QStringLiteral("#f8fafc")),
                   titleFont);

    qreal bodyY = paddingTop + titleMetrics.height() + titleGap;
    for (const QString &line : lines) {
        if (line.isEmpty()) {
            bodyY += bodyLineHeight * 0.7;
            continue;
        }

        appendTextNode(root,
                       window,
                       line,
                       cardRect.topLeft() + QPointF(paddingLeft, bodyY),
                       QColor(QStringLiteral("#e2e8f0")),
                       bodyFont);
        bodyY += bodyLineHeight;
    }

    return cardRect.height();
}

const ThreeDViewerStation *pickStationAtPosition(const ThreeDViewerSceneModel &sceneModel,
                                                 const ThreeDViewerCamera &camera,
                                                 const QPointF &screenPosition,
                                                 int viewportWidth,
                                                 int viewportHeight,
                                                 double maxDistancePixels = 14.0)
{
    const double maxDistanceSquared = maxDistancePixels * maxDistancePixels;
    const ThreeDViewerStation *bestStation = nullptr;
    double bestDistanceSquared = maxDistanceSquared;

    for (const ThreeDViewerStation &station : sceneModel.stations) {
        const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera, station.position, viewportWidth, viewportHeight);
        if (!projected.visible) {
            continue;
        }

        const double dx = projected.screenPosition.x() - screenPosition.x();
        const double dy = projected.screenPosition.y() - screenPosition.y();
        const double distanceSquared = dx * dx + dy * dy;
        if (distanceSquared <= bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestStation = &station;
        }
    }

    return bestStation;
}

const ThreeDViewerStation *stationById(const ThreeDViewerSceneModel &sceneModel, quint32 stationId)
{
    if (stationId == 0) {
        return nullptr;
    }

    for (const ThreeDViewerStation &station : sceneModel.stations) {
        if (station.id == stationId) {
            return &station;
        }
    }
    return nullptr;
}

QString stationDetailsText(const ThreeDViewerSceneModel &sceneModel, const ThreeDViewerStation &station)
{
    const QString qualifiedName = sceneModel.stationQualifiedName(station);
    const QString comment = station.comment.trimmed();
    QStringList lines;
    if (!qualifiedName.isEmpty()) {
        lines << qualifiedName;
    }
    lines << QString();
    lines << QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Depth")
          + QStringLiteral(": ")
          + QLocale::system().toString(station.position.z, 'f', std::abs(station.position.z) < 10.0 ? 1 : 0)
          + QStringLiteral(" m");
    if (!comment.isEmpty()) {
        lines << comment;
    }
    return lines.join(QLatin1Char('\n'));
}

QString measurementText(const ThreeDViewerSceneModel &sceneModel,
                        const ThreeDViewerStation &startStation,
                        const ThreeDViewerStation &endStation)
{
    const QString startName = sceneModel.stationQualifiedName(startStation);
    const QString endName = sceneModel.stationQualifiedName(endStation);
    const double dx = endStation.position.x - startStation.position.x;
    const double dy = endStation.position.y - startStation.position.y;
    const double dz = endStation.position.z - startStation.position.z;
    const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    QStringList lines;
    lines << QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Start")
          + QStringLiteral(": ")
          + (startName.isEmpty() ? QStringLiteral("?") : startName);
    lines << QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "End")
          + QStringLiteral(": ")
          + (endName.isEmpty() ? QStringLiteral("?") : endName);
    lines << QString();
    lines << QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Distance")
          + QStringLiteral(": ")
          + QLocale::system().toString(distance, 'f', distance < 10.0 ? 2 : 1)
          + QStringLiteral(" m");
    const double azimuth = [] (double value) {
        value = std::fmod(value, 360.0);
        if (value < 0.0) {
            value += 360.0;
        }
        return value;
    }(std::atan2(dx, -dy) * 180.0 / kPi);
    lines << QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Azimuth")
          + QStringLiteral(": ")
          + QLocale::system().toString(azimuth, 'f', 1)
          + QStringLiteral("°");
    lines << QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Vertical difference")
          + QStringLiteral(": ")
          + QLocale::system().toString(dz, 'f', std::abs(dz) < 10.0 ? 2 : 1)
          + QStringLiteral(" m");
    return lines.join(QLatin1Char('\n'));
}

QVector<QPointF> projectedCircle(const QPointF &center, double radius, int segments = 20)
{
    QVector<QPointF> points;
    points.reserve(segments + 2);
    points.push_back(center);
    for (int index = 0; index <= segments; ++index) {
        const double angle = double(index) * 2.0 * kPi / double(segments);
        points.push_back(QPointF(center.x() + std::cos(angle) * radius, center.y() + std::sin(angle) * radius));
    }
    return points;
}

ThreeDViewerVec3 subtract(const ThreeDViewerVec3 &left, const ThreeDViewerVec3 &right)
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

double dot(const ThreeDViewerVec3 &left, const ThreeDViewerVec3 &right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

ThreeDViewerVec3 cross(const ThreeDViewerVec3 &left, const ThreeDViewerVec3 &right)
{
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

double lengthSquared(const ThreeDViewerVec3 &value)
{
    return dot(value, value);
}

ThreeDViewerVec3 normalizedOrFallback(const ThreeDViewerVec3 &value, const ThreeDViewerVec3 &fallback)
{
    const double squaredLength = lengthSquared(value);
    if (squaredLength <= 0.00000001) {
        return fallback;
    }

    const double length = std::sqrt(squaredLength);
    return {value.x / length, value.y / length, value.z / length};
}

QColor scaledColor(const QColor &baseColor, double factor, int alpha)
{
    const double clampedFactor = std::clamp(factor, 0.0, 1.0);
    QColor color;
    color.setRgbF(std::clamp(baseColor.redF() * clampedFactor, 0.0, 1.0),
                  std::clamp(baseColor.greenF() * clampedFactor, 0.0, 1.0),
                  std::clamp(baseColor.blueF() * clampedFactor, 0.0, 1.0),
                  std::clamp(alpha / 255.0, 0.0, 1.0));
    return color;
}

struct MeshTriangleRender
{
    std::array<QPointF, 3> screenPoints;
    QColor color;
    double depth = 0.0;
};

QColor surveyColor(quint32 surveyId)
{
    const double hue = std::fmod(double(surveyId) * 0.6180339887498949, 1.0);
    return QColor::fromHsvF(hue, 0.60, 0.95, 0.75);
}

QColor depthColor(double depth, const ThreeDViewerBounds &bounds)
{
    if (!bounds.valid || bounds.maximum.z <= bounds.minimum.z + 0.000001) {
        return QColor::fromHsvF(0.66, 0.85, 0.95, 0.75);
    }

    const double normalized = std::clamp((depth - bounds.minimum.z) / (bounds.maximum.z - bounds.minimum.z), 0.0, 1.0);
    const double hue = 0.66 * (1.0 - normalized);
    const double saturation = 0.88;
    const double value = 0.92 - normalized * 0.18;
    return QColor::fromHsvF(hue, saturation, value, 0.75);
}

QColor sceneColorForMode(ThreeDViewerMeshColorMode mode,
                         quint32 surveyId,
                         double depth,
                         const ThreeDViewerBounds &bounds)
{
    return mode == ThreeDViewerMeshColorMode::Depth ? depthColor(depth, bounds) : surveyColor(surveyId);
}

QSGGeometryNode *createMeshNode(const QVector<MeshTriangleRender> &triangles)
{
    if (triangles.isEmpty()) {
        return nullptr;
    }

    auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), triangles.size() * 3);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto *vertexData = geometry->vertexDataAsColoredPoint2D();

    int vertexIndex = 0;
    for (const MeshTriangleRender &triangle : triangles) {
        const uchar red = static_cast<uchar>(triangle.color.red());
        const uchar green = static_cast<uchar>(triangle.color.green());
        const uchar blue = static_cast<uchar>(triangle.color.blue());
        const uchar alpha = static_cast<uchar>(triangle.color.alpha());
        for (const QPointF &point : triangle.screenPoints) {
            vertexData[vertexIndex++].set(float(point.x()), float(point.y()), red, green, blue, alpha);
        }
    }

    auto *material = new QSGVertexColorMaterial();
    auto *node = new QSGGeometryNode();
    node->setGeometry(geometry);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

} // namespace

ThreeDViewerViewportItem::ThreeDViewerViewportItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);

    connect(&controller_, &ThreeDViewerViewportController::cameraChanged, this, [this]() {
        {
            QMutexLocker locker(&mutex_);
            cameraSnapshot_ = controller_.camera();
        }
        scheduleUpdate();
    });
}

void ThreeDViewerViewportItem::setSceneModel(const ThreeDViewerSceneModel &sceneModel)
{
    {
        QMutexLocker locker(&mutex_);
        sceneModel_ = sceneModel;
        hoveredStationId_ = 0;
        hasHoveredStation_ = false;
        hoveredStationScreenPosition_ = QPointF();
        measurementStartStationId_ = 0;
        hasMeasurementStartStation_ = false;
        measurementEndStationId_ = 0;
        hasMeasurementEndStation_ = false;
    }
    controller_.fitToScene(sceneModel);
}

void ThreeDViewerViewportItem::setLayerVisibility(const std::array<bool, 5> &layerVisibility)
{
    {
        QMutexLocker locker(&mutex_);
        layerVisibility_ = layerVisibility;
    }
    scheduleUpdate();
}

void ThreeDViewerViewportItem::setMeshColorMode(ThreeDViewerMeshColorMode meshColorMode)
{
    {
        QMutexLocker locker(&mutex_);
        meshColorMode_ = meshColorMode;
    }
    scheduleUpdate();
}

void ThreeDViewerViewportItem::setMeasurementMode(bool measurementMode)
{
    bool changed = false;
    {
        QMutexLocker locker(&mutex_);
        if (measurementMode_ != measurementMode) {
            measurementMode_ = measurementMode;
            changed = true;
        }
    }
    if (!measurementMode) {
        bool selectionChanged = false;
        {
            QMutexLocker locker(&mutex_);
            if (hasMeasurementStartStation_ || measurementStartStationId_ != 0 || hasMeasurementEndStation_ || measurementEndStationId_ != 0) {
                measurementStartStationId_ = 0;
                hasMeasurementStartStation_ = false;
                measurementEndStationId_ = 0;
                hasMeasurementEndStation_ = false;
                selectionChanged = true;
            }
            if (hasHoveredStation_ || hoveredStationId_ != 0) {
                hoveredStationId_ = 0;
                hasHoveredStation_ = false;
                selectionChanged = true;
            }
        }
        changed = changed || selectionChanged;
    }
    if (changed) {
        scheduleUpdate();
    }
}

void ThreeDViewerViewportItem::hoverMoveEvent(QHoverEvent *event)
{
    if (event == nullptr) {
        return;
    }

    const Snapshot current = snapshot();
    const ThreeDViewerStation *station = pickStationAtPosition(current.sceneModel,
                                                               current.camera,
                                                               event->position(),
                                                               std::max(1, int(width())),
                                                               std::max(1, int(height())));
    const quint32 hoveredStationId = station != nullptr ? station->id : 0;
    const bool hasHoveredStation = station != nullptr;
    const QPointF hoveredStationScreenPosition = event->position();

    bool changed = false;
    {
        QMutexLocker locker(&mutex_);
        if (hasHoveredStation_ != hasHoveredStation || hoveredStationId_ != hoveredStationId
            || hoveredStationScreenPosition_ != hoveredStationScreenPosition) {
            hoveredStationId_ = hoveredStationId;
            hasHoveredStation_ = hasHoveredStation;
            hoveredStationScreenPosition_ = hoveredStationScreenPosition;
            changed = true;
        }
    }
    if (changed) {
        scheduleUpdate();
    }
    event->accept();
}

void ThreeDViewerViewportItem::hoverLeaveEvent(QHoverEvent *event)
{
    Q_UNUSED(event);
    bool changed = false;
    {
        QMutexLocker locker(&mutex_);
        if (hasHoveredStation_ || hoveredStationId_ != 0) {
            hoveredStationId_ = 0;
            hasHoveredStation_ = false;
            hoveredStationScreenPosition_ = QPointF();
            changed = true;
        }
    }
    if (changed) {
        scheduleUpdate();
    }
}

void ThreeDViewerViewportItem::fitToScene()
{
    controller_.fitToScene(snapshot().sceneModel);
}

void ThreeDViewerViewportItem::resetView()
{
    controller_.resetView(snapshot().sceneModel);
}

void ThreeDViewerViewportItem::setViewPreset(ThreeDViewerViewPreset preset)
{
    controller_.setViewPreset(preset, snapshot().sceneModel);
}

void ThreeDViewerViewportItem::rollLeft()
{
    controller_.rotateLeft();
}

void ThreeDViewerViewportItem::rollRight()
{
    controller_.rotateRight();
}

QSGNode *ThreeDViewerViewportItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    delete oldNode;

    const Snapshot current = snapshot();
    auto *root = new QSGNode();

    if (current.sceneModel.isEmpty()) {
        appendTextNode(root,
                       window(),
                       QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "No 3D data loaded."),
                       QPointF(24.0, 24.0),
                       QColor(QStringLiteral("#4b5563")),
                       QFont(QStringLiteral("Menlo"), 12));
        return root;
    }

    const int viewportWidth = std::max(1, int(width()));
    const int viewportHeight = std::max(1, int(height()));
    const ThreeDViewerCamera &camera = current.camera;
    const ThreeDViewerBounds bounds = current.sceneModel.bounds();
    QSGNode *meshNode = nullptr;

    const ThreeDViewerProjectedPoint origin = ThreeDViewerProjection::projectPoint(camera, {0.0, 0.0, 0.0}, viewportWidth, viewportHeight);
    const ThreeDViewerProjectedPoint xAxis = ThreeDViewerProjection::projectPoint(camera, {40.0, 0.0, 0.0}, viewportWidth, viewportHeight);
    const ThreeDViewerProjectedPoint yAxis = ThreeDViewerProjection::projectPoint(camera, {0.0, 40.0, 0.0}, viewportWidth, viewportHeight);
    const ThreeDViewerProjectedPoint zAxis = ThreeDViewerProjection::projectPoint(camera, {0.0, 0.0, 40.0}, viewportWidth, viewportHeight);
    if (origin.visible) {
        if (xAxis.visible) {
            QVector<QPointF> points;
            points << origin.screenPosition << xAxis.screenPosition;
            appendGeometryNode(root, points, QSGGeometry::DrawLines, QColor(QStringLiteral("#dc2626")), 1.3);
        }
        if (yAxis.visible) {
            QVector<QPointF> points;
            points << origin.screenPosition << yAxis.screenPosition;
            appendGeometryNode(root, points, QSGGeometry::DrawLines, QColor(QStringLiteral("#16a34a")), 1.3);
        }
        if (zAxis.visible) {
            QVector<QPointF> points;
            points << origin.screenPosition << zAxis.screenPosition;
            appendGeometryNode(root, points, QSGGeometry::DrawLines, QColor(QStringLiteral("#2563eb")), 1.3);
        }
    }

    if (current.layerVisibility.at(kSurfacesLayer)) {
        const QColor surfaceColor = QColor(QStringLiteral("#7c3aed"));
        for (const ThreeDViewerSurface &surface : current.sceneModel.surfaces) {
            if (surface.width < 2 || surface.height < 2) {
                continue;
            }

            for (quint32 row = 0; row < surface.height; ++row) {
                QVector<QPointF> rowPoints;
                rowPoints.reserve(int(surface.width));
                for (quint32 column = 0; column < surface.width; ++column) {
                    const qsizetype index = qsizetype(row) * qsizetype(surface.width) + qsizetype(column);
                    if (index < 0 || index >= surface.elevations.size()) {
                        continue;
                    }
                    const ThreeDViewerVec3 point{
                        surface.calibration[0] + surface.calibration[2] * double(column) + surface.calibration[4] * double(row),
                        surface.calibration[1] + surface.calibration[3] * double(column) + surface.calibration[5] * double(row),
                        surface.elevations.at(index),
                    };
                    const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera, point, viewportWidth, viewportHeight);
                    if (!projected.visible) {
                        rowPoints.clear();
                        break;
                    }
                    rowPoints.push_back(projected.screenPosition);
                }
                appendGeometryNode(root, rowPoints, QSGGeometry::DrawLineStrip, translucentColor(surfaceColor, 90), 1.0);
            }

            for (quint32 column = 0; column < surface.width; ++column) {
                QVector<QPointF> columnPoints;
                columnPoints.reserve(int(surface.height));
                for (quint32 row = 0; row < surface.height; ++row) {
                    const qsizetype index = qsizetype(row) * qsizetype(surface.width) + qsizetype(column);
                    if (index < 0 || index >= surface.elevations.size()) {
                        continue;
                    }
                    const ThreeDViewerVec3 point{
                        surface.calibration[0] + surface.calibration[2] * double(column) + surface.calibration[4] * double(row),
                        surface.calibration[1] + surface.calibration[3] * double(column) + surface.calibration[5] * double(row),
                        surface.elevations.at(index),
                    };
                    const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera, point, viewportWidth, viewportHeight);
                    if (!projected.visible) {
                        columnPoints.clear();
                        break;
                    }
                    columnPoints.push_back(projected.screenPosition);
                }
                appendGeometryNode(root, columnPoints, QSGGeometry::DrawLineStrip, translucentColor(surfaceColor, 90), 1.0);
            }
        }
    }

    if (current.layerVisibility.at(kMeshesLayer)) {
        QVector<MeshTriangleRender> triangles;
        triangles.reserve(256);
        const ThreeDViewerVec3 cameraPosition = camera.position();
        const ThreeDViewerVec3 lightDirection = normalizedOrFallback(camera.forwardVector(), {0.0, 0.0, -1.0});

        for (const ThreeDViewerMeshGroup &mesh : current.sceneModel.meshGroups) {
            for (const std::array<quint32, 3> &triangle : mesh.triangles) {
                if (triangle[0] >= quint32(mesh.vertices.size())
                    || triangle[1] >= quint32(mesh.vertices.size())
                    || triangle[2] >= quint32(mesh.vertices.size())) {
                    continue;
                }

                const ThreeDViewerVec3 &v0 = mesh.vertices.at(triangle[0]);
                const ThreeDViewerVec3 &v1 = mesh.vertices.at(triangle[1]);
                const ThreeDViewerVec3 &v2 = mesh.vertices.at(triangle[2]);
                const ThreeDViewerProjectedPoint p0 = ThreeDViewerProjection::projectPoint(camera, v0, viewportWidth, viewportHeight);
                const ThreeDViewerProjectedPoint p1 = ThreeDViewerProjection::projectPoint(camera, v1, viewportWidth, viewportHeight);
                const ThreeDViewerProjectedPoint p2 = ThreeDViewerProjection::projectPoint(camera, v2, viewportWidth, viewportHeight);
                if (!p0.visible || !p1.visible || !p2.visible) {
                    continue;
                }

                const ThreeDViewerVec3 normal = normalizedOrFallback(cross(subtract(v1, v0), subtract(v2, v0)), {0.0, 0.0, 1.0});
                const double diffuse = std::clamp(std::abs(dot(normal, lightDirection)), 0.0, 1.0);
                const double averageDepth = (v0.z + v1.z + v2.z) / 3.0;
                const QColor baseColor = sceneColorForMode(current.meshColorMode, mesh.surveyId, averageDepth, bounds);
                const QColor color = scaledColor(baseColor, 0.45 + diffuse * 0.55, 190);
                triangles.push_back(MeshTriangleRender{
                    {p0.screenPosition, p1.screenPosition, p2.screenPosition},
                    color,
                    (dot(subtract(v0, cameraPosition), camera.forwardVector())
                     + dot(subtract(v1, cameraPosition), camera.forwardVector())
                     + dot(subtract(v2, cameraPosition), camera.forwardVector())) / 3.0,
                });
            }
        }

        std::sort(triangles.begin(), triangles.end(), [](const MeshTriangleRender &left, const MeshTriangleRender &right) {
            return left.depth > right.depth;
        });

        meshNode = createMeshNode(triangles);
    }

    const ThreeDViewerStation *hoveredStation = current.hasHoveredStation
        ? stationById(current.sceneModel, current.hoveredStationId)
        : nullptr;
    const ThreeDViewerStation *measurementStartStation = current.hasMeasurementStartStation
        ? stationById(current.sceneModel, current.measurementStartStationId)
        : nullptr;
    const ThreeDViewerStation *measurementEndStation = current.hasMeasurementEndStation
        ? stationById(current.sceneModel, current.measurementEndStationId)
        : nullptr;

    if (current.layerVisibility.at(kCenterlineLayer)) {
        for (const ThreeDViewerShot &shot : current.sceneModel.shots) {
            const ThreeDViewerStation *fromStation = nullptr;
            const ThreeDViewerStation *toStation = nullptr;
            for (const ThreeDViewerStation &station : current.sceneModel.stations) {
                if (station.id == shot.fromStationId) {
                    fromStation = &station;
                } else if (station.id == shot.toStationId) {
                    toStation = &station;
                }
                if (fromStation != nullptr && toStation != nullptr) {
                    break;
                }
            }

            if (fromStation == nullptr || toStation == nullptr) {
                continue;
            }

            QPointF fromScreen;
            QPointF toScreen;
            if (!ThreeDViewerProjection::projectLine(camera, fromStation->position, toStation->position, viewportWidth, viewportHeight, &fromScreen, &toScreen)) {
                continue;
            }

            const double averageDepth = (fromStation->position.z + toStation->position.z) * 0.5;
            const QColor baseColor = sceneColorForMode(current.meshColorMode, shot.surveyId, averageDepth, bounds);
            const QColor lineColor = shot.hidden ? scaledColor(baseColor, 0.45, 170)
                                                 : shot.surface ? scaledColor(baseColor, 0.85, 240)
                                                                : scaledColor(baseColor, 0.78, 235);
            QVector<QPointF> linePoints;
            linePoints << fromScreen << toScreen;
            appendGeometryNode(root,
                               linePoints,
                               QSGGeometry::DrawLines,
                               QColor(0, 0, 0, shot.hidden ? 170 : 210),
                               shot.hidden ? 4.0 : 6.0);
            appendGeometryNode(root,
                               linePoints,
                               QSGGeometry::DrawLines,
                               lineColor,
                               shot.hidden ? 2.0 : 3.5);
        }
    }

    if (current.layerVisibility.at(kStationsLayer)) {
        auto stationAccentColor = [&](const ThreeDViewerStation &station) {
            if (measurementStartStation != nullptr && station.id == measurementStartStation->id) {
                return QColor(QStringLiteral("#22c55e"));
            }
            if (measurementEndStation != nullptr && station.id == measurementEndStation->id) {
                return QColor(QStringLiteral("#fb7185"));
            }
            if (hoveredStation != nullptr && station.id == hoveredStation->id) {
                return QColor(QStringLiteral("#facc15"));
            }
            return QColor();
        };

        QVector<QRectF> occupiedStationBounds;
        occupiedStationBounds.reserve(std::min(current.sceneModel.stations.size(), qsizetype(512)));
        for (const ThreeDViewerStation &station : current.sceneModel.stations) {
            const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera, station.position, viewportWidth, viewportHeight);
            if (!projected.visible) {
                continue;
            }

            const QColor accentColor = stationAccentColor(station);
            const bool priorityStation = accentColor.isValid();
            const QRectF stationBounds = markerBoundsAt(projected.screenPosition);
            if (!priorityStation && intersectsAny(stationBounds, occupiedStationBounds)) {
                continue;
            }
            occupiedStationBounds.append(stationBounds);

            if (accentColor.isValid()) {
                const QVector<QPointF> outerCircle = projectedCircle(projected.screenPosition, 5.2);
                appendGeometryNode(root,
                                   outerCircle,
                                   QSGGeometry::DrawTriangleFan,
                                   translucentColor(accentColor, 90),
                                   1.0);
                appendClosedPolyline(root, outerCircle, accentColor, 1.4);
            }

            const QVector<QPointF> circlePoints = projectedCircle(projected.screenPosition, accentColor.isValid() ? 3.2 : 2.6);
            appendGeometryNode(root,
                               circlePoints,
                               QSGGeometry::DrawTriangleFan,
                               station.fixed ? QColor(QStringLiteral("#fcd34d"))
                                             : station.entrance ? QColor(QStringLiteral("#86efac"))
                                                                : station.surface ? QColor(QStringLiteral("#93c5fd"))
                                                                                  : QColor(QStringLiteral("#f8fafc")),
                               1.0);
            appendClosedPolyline(root, circlePoints, accentColor.isValid() ? accentColor : QColor(QStringLiteral("#e5e7eb")), accentColor.isValid() ? 1.5 : 0.8);
        }
    }

    auto appendStationHoverOverlay = [&](const ThreeDViewerStation *station, const QColor &accentColor) {
        if (station == nullptr || !accentColor.isValid()) {
            return;
        }

        const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera, station->position, viewportWidth, viewportHeight);
        if (!projected.visible) {
            return;
        }

        const QVector<QPointF> outerCircle = projectedCircle(projected.screenPosition, 6.0);
        appendGeometryNode(root,
                           outerCircle,
                           QSGGeometry::DrawTriangleFan,
                           translucentColor(accentColor, 100),
                           1.0);
        appendClosedPolyline(root, outerCircle, accentColor, 1.6);
    };

    appendStationHoverOverlay(hoveredStation, QColor(QStringLiteral("#facc15")));
    appendStationHoverOverlay(measurementStartStation, QColor(QStringLiteral("#22c55e")));
    appendStationHoverOverlay(measurementEndStation, QColor(QStringLiteral("#fb7185")));

    if (current.layerVisibility.at(kLabelsLayer)) {
        const QFont labelFont = [] {
            QFont font = QGuiApplication::font();
            font.setPointSizeF(std::max(8.0, font.pointSizeF() > 0.0 ? font.pointSizeF() - 1.0 : 10.0));
            return font;
        }();

        QVector<QRectF> occupiedLabelBounds;
        occupiedLabelBounds.reserve(std::min(current.sceneModel.stations.size(), qsizetype(512)));
        for (const ThreeDViewerStation &station : current.sceneModel.stations) {
            const QString label = current.sceneModel.stationQualifiedName(station);
            if (label.isEmpty()) {
                continue;
            }

            const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera, station.position, viewportWidth, viewportHeight);
            if (!projected.visible) {
                continue;
            }

            const QPointF labelPosition = projected.screenPosition + QPointF(kStationLabelOffsetX, kStationLabelOffsetY);
            const QRectF labelBounds = textBoundsAt(label, labelFont, labelPosition);
            if (intersectsAny(labelBounds, occupiedLabelBounds)) {
                continue;
            }
            occupiedLabelBounds.append(labelBounds);

            appendTextNodeWithShadow(root,
                                     window(),
                                     label,
                                     labelPosition,
                                     QColor(QStringLiteral("#f8fafc")),
                                     labelFont);
        }
    }

    if (meshNode != nullptr) {
        root->appendChildNode(meshNode);
    }

    if (measurementStartStation != nullptr && measurementEndStation != nullptr) {
        QPointF fromScreen;
        QPointF toScreen;
        if (ThreeDViewerProjection::projectLine(camera,
                                                measurementStartStation->position,
                                                measurementEndStation->position,
                                                viewportWidth,
                                                viewportHeight,
                                                &fromScreen,
                                                &toScreen)) {
            QVector<QPointF> linePoints;
            linePoints << fromScreen << toScreen;
            appendGeometryNode(root,
                               linePoints,
                               QSGGeometry::DrawLines,
                               QColor(0, 0, 0, 220),
                               6.0);
            appendGeometryNode(root,
                               linePoints,
                               QSGGeometry::DrawLines,
                               QColor(QStringLiteral("#facc15")),
                               3.0);
        }
    }

    appendBoundingBox(root, camera, bounds, viewportWidth, viewportHeight, QColor(QStringLiteral("#ff0000")));
    const QPointF altitudeOrigin(20.0, double(viewportHeight) - 300.0);
    const bool showAltitudeLegend = current.meshColorMode == ThreeDViewerMeshColorMode::Depth;
    if (showAltitudeLegend) {
        appendAltitudeLegend(root, window(), bounds, altitudeOrigin);
    }
    const double hudRowY = altitudeOrigin.y() + 240.0;
    appendCompassIndicator(root, window(), bounds, camera, viewportWidth, viewportHeight, QPointF(36.0, hudRowY));
    appendViewAngleIndicator(root, window(), camera, QPointF(98.0, hudRowY));
    appendScaleBar(root,
                   window(),
                   camera,
                   bounds,
                   QPointF(160.0, hudRowY - 20.0),
                   viewportWidth,
                   viewportHeight);
    appendSceneStatisticsOverlay(root, window(), current.sceneModel, viewportWidth, viewportHeight);

    const QFont cardTitleFont(QStringLiteral("Menlo"), 11, QFont::Bold);
    const QFont cardBodyFont(QStringLiteral("Menlo"), 10);
    const QFontMetrics cardTitleMetrics(cardTitleFont);
    const QFontMetrics cardBodyMetrics(cardBodyFont);
    const auto cardWidthForText = [&](const QString &title, const QString &body) {
        const QStringList lines = body.split(QLatin1Char('\n'));
        qreal widest = qreal(cardTitleMetrics.horizontalAdvance(title));
        for (const QString &line : lines) {
            if (line.isEmpty()) {
                continue;
            }
            widest = std::max(widest, qreal(cardBodyMetrics.horizontalAdvance(line)));
        }
        const qreal desiredWidth = widest + 24.0;
        const qreal maxWidth = std::max(280.0, double(viewportWidth) - 40.0);
        return std::clamp(desiredWidth, 280.0, maxWidth);
    };

    const QString stationTitle = QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Station");
    const QString measurementTitle = QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Measurement");
    const QString stationBody = hoveredStation != nullptr ? stationDetailsText(current.sceneModel, *hoveredStation) : QString();
    const QString measurementBody = current.measurementMode
        ? (measurementStartStation != nullptr && measurementEndStation != nullptr
               ? measurementText(current.sceneModel, *measurementStartStation, *measurementEndStation)
               : measurementStartStation != nullptr
                   ? QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Start")
                         + QStringLiteral(": ")
                         + (current.sceneModel.stationQualifiedName(*measurementStartStation).isEmpty()
                                ? QStringLiteral("?")
                                : current.sceneModel.stationQualifiedName(*measurementStartStation))
                         + QStringLiteral("\n")
                         + QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Click another station to finish.")
                   : QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer", "Click a station to start measuring."))
        : QString();
    const qreal stationCardWidth = stationBody.isEmpty() ? 280.0 : cardWidthForText(stationTitle, stationBody);
    const qreal measurementCardWidth = measurementBody.isEmpty() ? 280.0 : cardWidthForText(measurementTitle, measurementBody);
    const qreal cardWidth = std::max(stationCardWidth, measurementCardWidth);
    const qreal cardX = std::max(20.0, double(viewportWidth) - cardWidth - 20.0);

    qreal nextCardY = 20.0;
    if (!stationBody.isEmpty()) {
        const qreal stationCardHeight = measureTextCardHeight(stationTitle, stationBody);
        const qreal hoverX = std::clamp(current.hoveredStationScreenPosition.x() + 18.0,
                                        20.0,
                                        double(viewportWidth) - stationCardWidth - 20.0);
        const qreal hoverY = std::clamp(current.hoveredStationScreenPosition.y() + 18.0,
                                        20.0,
                                        double(viewportHeight) - stationCardHeight - 20.0);
        appendTextCard(root,
                       window(),
                       stationTitle,
                       stationBody,
                       QPointF(hoverX, hoverY),
                       stationCardWidth);
    }

    if (current.measurementMode) {
        if (measurementStartStation != nullptr && measurementEndStation != nullptr) {
            nextCardY += appendTextCard(root,
                                        window(),
                                        measurementTitle,
                                        measurementBody,
                                        QPointF(cardX, nextCardY),
                                        measurementCardWidth) + 12.0;
        } else if (measurementStartStation != nullptr) {
            nextCardY += appendTextCard(root,
                                        window(),
                                        measurementTitle,
                                        measurementBody,
                                        QPointF(cardX, nextCardY),
                                        measurementCardWidth) + 12.0;
        } else {
            nextCardY += appendTextCard(root,
                                        window(),
                                        measurementTitle,
                                        measurementBody,
                                        QPointF(cardX, nextCardY),
                                        measurementCardWidth) + 12.0;
        }
    }

    return root;
}

void ThreeDViewerViewportItem::mousePressEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const Snapshot current = snapshot();
        if (current.measurementMode) {
            const ThreeDViewerStation *station = pickStationAtPosition(current.sceneModel,
                                                                       current.camera,
                                                                       event->position(),
                                                                       std::max(1, int(width())),
                                                                       std::max(1, int(height())));
            if (station != nullptr) {
                {
                    QMutexLocker locker(&mutex_);
                    if (!hasMeasurementStartStation_ || hasMeasurementEndStation_) {
                        measurementStartStationId_ = station->id;
                        hasMeasurementStartStation_ = true;
                        measurementEndStationId_ = 0;
                        hasMeasurementEndStation_ = false;
                    } else {
                        measurementEndStationId_ = station->id;
                        hasMeasurementEndStation_ = true;
                    }
                }
                scheduleUpdate();
            }
            event->accept();
            return;
        }
    }

    if (controller_.mousePress(event->button(), event->position().toPoint())) {
        event->accept();
        return;
    }

    QQuickItem::mousePressEvent(event);
}

void ThreeDViewerViewportItem::mouseMoveEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        QQuickItem::mouseMoveEvent(event);
        return;
    }

    if (controller_.mouseMove(event->position().toPoint(), int(height()))) {
        event->accept();
        return;
    }

    QQuickItem::mouseMoveEvent(event);
}

void ThreeDViewerViewportItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }

    if (controller_.mouseRelease(event->button())) {
        event->accept();
        return;
    }

    QQuickItem::mouseReleaseEvent(event);
}

void ThreeDViewerViewportItem::wheelEvent(QWheelEvent *event)
{
    if (event == nullptr) {
        return;
    }

    if (!controller_.wheel(event->angleDelta())) {
        QQuickItem::wheelEvent(event);
        return;
    }
    event->accept();
}

ThreeDViewerViewportItem::Snapshot ThreeDViewerViewportItem::snapshot() const
{
    QMutexLocker locker(&mutex_);
    Snapshot current;
    current.sceneModel = sceneModel_;
    current.layerVisibility = layerVisibility_;
    current.meshColorMode = meshColorMode_;
    current.measurementMode = measurementMode_;
    current.hasHoveredStation = hasHoveredStation_;
    current.hoveredStationId = hoveredStationId_;
    current.hoveredStationScreenPosition = hoveredStationScreenPosition_;
    current.hasMeasurementStartStation = hasMeasurementStartStation_;
    current.measurementStartStationId = measurementStartStationId_;
    current.hasMeasurementEndStation = hasMeasurementEndStation_;
    current.measurementEndStationId = measurementEndStationId_;
    current.camera = cameraSnapshot_;
    return current;
}

void ThreeDViewerViewportItem::scheduleUpdate()
{
    update();
}

} // namespace TherionStudio
