#include "ThreeDViewerViewportWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPolygonF>
#include <QResizeEvent>
#include <QWheelEvent>

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kCenterlineLayer = 0;
constexpr int kStationsLayer = 1;
constexpr int kLabelsLayer = 2;
constexpr int kMeshesLayer = 3;
constexpr int kSurfacesLayer = 4;

QVector3D toVector3D(const TherionStudio::ThreeDViewerVec3 &value)
{
    return {float(value.x), float(value.y), float(value.z)};
}

TherionStudio::ThreeDViewerVec3 toSceneVec3(const QVector3D &value)
{
    return {double(value.x()), double(value.y()), double(value.z())};
}

double vectorLength(const QVector3D &value)
{
    return std::sqrt(double(QVector3D::dotProduct(value, value)));
}

QColor translucentColor(const QColor &color, int alpha)
{
    QColor copy = color;
    copy.setAlpha(alpha);
    return copy;
}

QPen cosmeticPen(const QColor &color, qreal width, Qt::PenStyle style = Qt::SolidLine)
{
    QPen pen(color, width, style);
    pen.setCosmetic(true);
    return pen;
}
} // namespace

namespace TherionStudio
{

ThreeDViewerViewportWidget::ThreeDViewerViewportWidget(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void ThreeDViewerViewportWidget::setSceneModel(const ThreeDViewerSceneModel &sceneModel)
{
    sceneModel_ = sceneModel;
    recenterCameraOnScene();
    update();
}

void ThreeDViewerViewportWidget::setLayerVisibility(const std::array<bool, 5> &layerVisibility)
{
    layerVisibility_ = layerVisibility;
    update();
}

void ThreeDViewerViewportWidget::fitToScene()
{
    recenterCameraOnScene();
    update();
}

void ThreeDViewerViewportWidget::resetView()
{
    camera_.yaw = -0.85;
    camera_.pitch = 0.45;
    recenterCameraOnScene();
    update();
}

void ThreeDViewerViewportWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(rect(), palette().base());

    if (sceneModel_.isEmpty()) {
        paintEmptyState(painter);
        return;
    }

    paintScene(painter);
}

void ThreeDViewerViewportWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void ThreeDViewerViewportWidget::mousePressEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }

    lastMousePosition_ = event->pos();
    cameraAtPress_ = camera_;

    if (event->button() == Qt::LeftButton) {
        interactionMode_ = InteractionMode::Orbit;
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        interactionMode_ = InteractionMode::Pan;
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void ThreeDViewerViewportWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event == nullptr || interactionMode_ == InteractionMode::None) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint delta = event->pos() - lastMousePosition_;
    if (interactionMode_ == InteractionMode::Orbit) {
        camera_.yaw = cameraAtPress_.yaw + double(delta.x()) * 0.008;
        camera_.pitch = clampPitch(cameraAtPress_.pitch + double(delta.y()) * 0.008);
        update();
    } else if (interactionMode_ == InteractionMode::Pan) {
        const QVector3D right = cameraRightVector();
        const QVector3D up = cameraUpVector();
        const double scale = screenPanScale();
        const QVector3D target = toVector3D(cameraAtPress_.target)
            - right * float(double(delta.x()) * scale)
            + up * float(double(delta.y()) * scale);
        camera_.target = toSceneVec3(target);
        update();
    }

    event->accept();
}

void ThreeDViewerViewportWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }

    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        interactionMode_ = InteractionMode::None;
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void ThreeDViewerViewportWidget::wheelEvent(QWheelEvent *event)
{
    if (event == nullptr) {
        return;
    }

    const QPoint angleDelta = event->angleDelta();
    if (angleDelta.y() == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    const double factor = angleDelta.y() > 0 ? 0.88 : 1.0 / 0.88;
    camera_.distance = std::clamp(camera_.distance * factor, 4.0, 100000.0);
    update();
    event->accept();
}

void ThreeDViewerViewportWidget::paintScene(QPainter &painter)
{
    paintAxes(painter);
    if (layerVisibility_[kSurfacesLayer]) {
        paintSurfaces(painter);
    }
    if (layerVisibility_[kMeshesLayer]) {
        paintMeshes(painter);
    }
    if (layerVisibility_[kCenterlineLayer]) {
        paintShots(painter);
    }
    if (layerVisibility_[kStationsLayer]) {
        paintStations(painter);
    }
    if (layerVisibility_[kLabelsLayer]) {
        paintLabels(painter);
    }
}

void ThreeDViewerViewportWidget::paintEmptyState(QPainter &painter)
{
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(rect().adjusted(24, 24, -24, -24), Qt::AlignCenter, tr("No 3D data loaded."));
}

void ThreeDViewerViewportWidget::paintAxes(QPainter &painter)
{
    const ProjectedPoint origin = projectPoint({0.0, 0.0, 0.0});
    const ProjectedPoint xAxis = projectPoint({40.0, 0.0, 0.0});
    const ProjectedPoint yAxis = projectPoint({0.0, 40.0, 0.0});
    const ProjectedPoint zAxis = projectPoint({0.0, 0.0, 40.0});

    if (!origin.visible) {
        return;
    }

    painter.save();
    painter.setPen(cosmeticPen(QColor(QStringLiteral("#dc2626")), 1.3));
    if (xAxis.visible) {
        painter.drawLine(origin.screenPosition, xAxis.screenPosition);
    }
    painter.setPen(cosmeticPen(QColor(QStringLiteral("#16a34a")), 1.3));
    if (yAxis.visible) {
        painter.drawLine(origin.screenPosition, yAxis.screenPosition);
    }
    painter.setPen(cosmeticPen(QColor(QStringLiteral("#2563eb")), 1.3));
    if (zAxis.visible) {
        painter.drawLine(origin.screenPosition, zAxis.screenPosition);
    }
    painter.restore();
}

void ThreeDViewerViewportWidget::paintSurfaces(QPainter &painter)
{
    const QPen outlinePen = cosmeticPen(QColor(QStringLiteral("#7c3aed")), 1.0);
    const QColor fillColor = translucentColor(QColor(QStringLiteral("#c084fc")), 20);

    painter.save();
    painter.setPen(outlinePen);
    painter.setBrush(fillColor);
    for (const ThreeDViewerSurface &surface : sceneModel_.surfaces) {
        if (surface.width < 2 || surface.height < 2) {
            continue;
        }

        for (quint32 row = 0; row < surface.height; ++row) {
            for (quint32 column = 0; column < surface.width; ++column) {
                const qsizetype index = qsizetype(row) * qsizetype(surface.width) + qsizetype(column);
                if (index < 0 || index >= surface.elevations.size()) {
                    continue;
                }

                const ThreeDViewerVec3 currentPoint{
                    surface.calibration[0] + surface.calibration[2] * double(column) + surface.calibration[4] * double(row),
                    surface.calibration[1] + surface.calibration[3] * double(column) + surface.calibration[5] * double(row),
                    surface.elevations.at(index),
                };

                if (column + 1 < surface.width) {
                    const ThreeDViewerVec3 rightPoint{
                        surface.calibration[0] + surface.calibration[2] * double(column + 1) + surface.calibration[4] * double(row),
                        surface.calibration[1] + surface.calibration[3] * double(column + 1) + surface.calibration[5] * double(row),
                        surface.elevations.at(index + 1),
                    };

                    QPointF fromScreen;
                    QPointF toScreen;
                    if (projectLine(currentPoint, rightPoint, &fromScreen, &toScreen)) {
                        painter.drawLine(fromScreen, toScreen);
                    }
                }

                if (row + 1 < surface.height) {
                    const qsizetype downIndex = index + qsizetype(surface.width);
                    if (downIndex >= 0 && downIndex < surface.elevations.size()) {
                        const ThreeDViewerVec3 downPoint{
                            surface.calibration[0] + surface.calibration[2] * double(column) + surface.calibration[4] * double(row + 1),
                            surface.calibration[1] + surface.calibration[3] * double(column) + surface.calibration[5] * double(row + 1),
                            surface.elevations.at(downIndex),
                        };

                        QPointF fromScreen;
                        QPointF toScreen;
                        if (projectLine(currentPoint, downPoint, &fromScreen, &toScreen)) {
                            painter.drawLine(fromScreen, toScreen);
                        }
                    }
                }
            }
        }
    }
    painter.restore();
}

void ThreeDViewerViewportWidget::paintMeshes(QPainter &painter)
{
    const QPen meshPen = cosmeticPen(QColor(QStringLiteral("#16a34a")), 1.0, Qt::DashLine);

    painter.save();
    painter.setPen(meshPen);
    painter.setBrush(Qt::NoBrush);
    for (const ThreeDViewerMeshGroup &mesh : sceneModel_.meshGroups) {
        for (const std::array<quint32, 3> &triangle : mesh.triangles) {
            if (triangle[0] >= quint32(mesh.vertices.size())
                || triangle[1] >= quint32(mesh.vertices.size())
                || triangle[2] >= quint32(mesh.vertices.size())) {
                continue;
            }

            const ProjectedPoint p0 = projectPoint(mesh.vertices.at(triangle[0]));
            const ProjectedPoint p1 = projectPoint(mesh.vertices.at(triangle[1]));
            const ProjectedPoint p2 = projectPoint(mesh.vertices.at(triangle[2]));
            if (!p0.visible || !p1.visible || !p2.visible) {
                continue;
            }

            QPolygonF polygon;
            polygon << p0.screenPosition << p1.screenPosition << p2.screenPosition;
            painter.drawPolygon(polygon);
        }
    }
    painter.restore();
}

void ThreeDViewerViewportWidget::paintShots(QPainter &painter)
{
    painter.save();
    for (const ThreeDViewerShot &shot : sceneModel_.shots) {
        const ThreeDViewerStation *fromStation = nullptr;
        const ThreeDViewerStation *toStation = nullptr;
        for (const ThreeDViewerStation &station : sceneModel_.stations) {
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
        if (!projectLine(fromStation->position, toStation->position, &fromScreen, &toScreen)) {
            continue;
        }

        painter.setPen(cosmeticPen(shotColorForFlags(shot),
                                   shot.hidden ? 1.0 : 2.0,
                                   shot.hidden ? Qt::DashLine : Qt::SolidLine));
        painter.drawLine(fromScreen, toScreen);
    }
    painter.restore();
}

void ThreeDViewerViewportWidget::paintStations(QPainter &painter)
{
    const QPen pen = cosmeticPen(QColor(QStringLiteral("#ef4444")), 1.2);

    painter.save();
    for (const ThreeDViewerStation &station : sceneModel_.stations) {
        const ProjectedPoint projected = projectPoint(station.position);
        if (!projected.visible) {
            continue;
        }

        painter.setPen(pen);
        painter.setBrush(stationColorForFlags(station));
        painter.drawEllipse(projected.screenPosition, 4.5, 4.5);
    }
    painter.restore();
}

void ThreeDViewerViewportWidget::paintLabels(QPainter &painter)
{
    painter.save();
    painter.setPen(palette().color(QPalette::Text));
    for (const ThreeDViewerStation &station : sceneModel_.stations) {
        if (station.name.isEmpty()) {
            continue;
        }

        const ProjectedPoint projected = projectPoint(station.position);
        if (!projected.visible) {
            continue;
        }

        painter.drawText(projected.screenPosition + QPointF(8.0, -8.0), station.name);
    }
    painter.restore();
}

ThreeDViewerViewportWidget::ProjectedPoint ThreeDViewerViewportWidget::projectPoint(const ThreeDViewerVec3 &point) const
{
    ProjectedPoint projected;
    if (width() <= 0 || height() <= 0) {
        return projected;
    }

    const QVector3D forward = cameraForwardVector();
    const QVector3D right = cameraRightVector();
    const QVector3D up = cameraUpVector();
    const QVector3D delta = toVector3D(point) - cameraPosition();

    const double depth = double(QVector3D::dotProduct(delta, forward));
    if (depth <= 0.1) {
        return projected;
    }

    const double x = double(QVector3D::dotProduct(delta, right));
    const double y = double(QVector3D::dotProduct(delta, up));
    const double halfHeight = qMax(1.0, double(height()) * 0.5);
    const double halfWidth = qMax(1.0, double(width()) * 0.5);
    constexpr double fovRadians = 35.0 * M_PI / 180.0;
    const double scale = halfHeight / (depth * std::tan(fovRadians * 0.5));

    projected.screenPosition = QPointF(halfWidth + x * scale, halfHeight - y * scale);
    projected.depth = depth;
    projected.visible = true;
    return projected;
}

bool ThreeDViewerViewportWidget::projectLine(const ThreeDViewerVec3 &from,
                                             const ThreeDViewerVec3 &to,
                                             QPointF *fromScreen,
                                             QPointF *toScreen) const
{
    const ProjectedPoint projectedFrom = projectPoint(from);
    const ProjectedPoint projectedTo = projectPoint(to);
    if (!projectedFrom.visible || !projectedTo.visible) {
        return false;
    }

    if (fromScreen != nullptr) {
        *fromScreen = projectedFrom.screenPosition;
    }
    if (toScreen != nullptr) {
        *toScreen = projectedTo.screenPosition;
    }
    return true;
}

QVector3D ThreeDViewerViewportWidget::cameraPosition() const
{
    const double cosPitch = std::cos(camera_.pitch);
    return toVector3D(camera_.target) + QVector3D(float(camera_.distance * cosPitch * std::cos(camera_.yaw)),
                                                 float(camera_.distance * cosPitch * std::sin(camera_.yaw)),
                                                 float(camera_.distance * std::sin(camera_.pitch)));
}

QVector3D ThreeDViewerViewportWidget::cameraForwardVector() const
{
    const QVector3D forward = toVector3D(camera_.target) - cameraPosition();
    if (vectorLength(forward) <= 0.0001) {
        return {0.0f, 0.0f, -1.0f};
    }
    return forward.normalized();
}

QVector3D ThreeDViewerViewportWidget::worldUpVector() const
{
    return {0.0f, 0.0f, 1.0f};
}

QVector3D ThreeDViewerViewportWidget::cameraRightVector() const
{
    const QVector3D forward = cameraForwardVector();
    QVector3D right = QVector3D::crossProduct(forward, worldUpVector());
    if (vectorLength(right) <= 0.0001) {
        right = QVector3D::crossProduct(forward, {0.0f, 1.0f, 0.0f});
    }
    if (vectorLength(right) <= 0.0001) {
        return {1.0f, 0.0f, 0.0f};
    }
    return right.normalized();
}

QVector3D ThreeDViewerViewportWidget::cameraUpVector() const
{
    const QVector3D right = cameraRightVector();
    const QVector3D forward = cameraForwardVector();
    QVector3D up = QVector3D::crossProduct(right, forward);
    if (vectorLength(up) <= 0.0001) {
        up = worldUpVector();
    }
    return up.normalized();
}

void ThreeDViewerViewportWidget::recenterCameraOnScene()
{
    const ThreeDViewerBounds bounds = sceneModel_.bounds();
    if (!bounds.valid) {
        camera_.target = {0.0, 0.0, 0.0};
        camera_.distance = 120.0;
        return;
    }

    camera_.target = {
        (bounds.minimum.x + bounds.maximum.x) * 0.5,
        (bounds.minimum.y + bounds.maximum.y) * 0.5,
        (bounds.minimum.z + bounds.maximum.z) * 0.5,
    };

    const QVector3D diagonal = toVector3D(bounds.maximum) - toVector3D(bounds.minimum);
    const double radius = qMax(1.0, vectorLength(diagonal) * 0.5);
    constexpr double fovRadians = 35.0 * M_PI / 180.0;
    camera_.distance = qMax(12.0, radius / std::tan(fovRadians * 0.5) * 1.35);
}

double ThreeDViewerViewportWidget::screenPanScale() const
{
    const double viewHeight = qMax(1, height());
    constexpr double fovRadians = 35.0 * M_PI / 180.0;
    return 2.0 * camera_.distance * std::tan(fovRadians * 0.5) / viewHeight;
}

double ThreeDViewerViewportWidget::clampPitch(double pitch)
{
    return std::clamp(pitch, -1.45, 1.45);
}

QColor ThreeDViewerViewportWidget::shotColorForFlags(const ThreeDViewerShot &shot)
{
    if (shot.hidden) {
        return QColor(QStringLiteral("#6b7280"));
    }
    if (shot.splay) {
        return QColor(QStringLiteral("#0ea5e9"));
    }
    if (shot.surface) {
        return QColor(QStringLiteral("#ef4444"));
    }
    return QColor(QStringLiteral("#111827"));
}

QColor ThreeDViewerViewportWidget::stationColorForFlags(const ThreeDViewerStation &station)
{
    if (station.fixed) {
        return QColor(QStringLiteral("#f59e0b"));
    }
    if (station.entrance) {
        return QColor(QStringLiteral("#22c55e"));
    }
    if (station.surface) {
        return QColor(QStringLiteral("#60a5fa"));
    }
    return QColor(QStringLiteral("#93c5fd"));
}

} // namespace TherionStudio
