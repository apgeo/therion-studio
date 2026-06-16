#include "ThreeDViewerViewportWidget.h"

#include "../../core/ThreeDViewerCamera.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPolygonF>
#include <QResizeEvent>
#include <QWheelEvent>

namespace
{
constexpr int kCenterlineLayer = 0;
constexpr int kStationsLayer = 1;
constexpr int kLabelsLayer = 2;
constexpr int kMeshesLayer = 3;
constexpr int kSurfacesLayer = 4;

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
    camera_.fitToBounds(sceneModel_.bounds());
    update();
}

void ThreeDViewerViewportWidget::setLayerVisibility(const std::array<bool, 5> &layerVisibility)
{
    layerVisibility_ = layerVisibility;
    update();
}

void ThreeDViewerViewportWidget::fitToScene()
{
    camera_.fitToBounds(sceneModel_.bounds());
    update();
}

void ThreeDViewerViewportWidget::resetView()
{
    camera_.resetToBounds(sceneModel_.bounds());
    update();
}

void ThreeDViewerViewportWidget::setViewPreset(ThreeDViewerViewPreset preset)
{
    camera_.setViewPreset(preset);
    camera_.fitToBounds(sceneModel_.bounds());
    update();
}

void ThreeDViewerViewportWidget::rollLeft()
{
    camera_.yawByRadians(3.14159265358979323846 / 12.0);
    update();
}

void ThreeDViewerViewportWidget::rollRight()
{
    camera_.yawByRadians(-3.14159265358979323846 / 12.0);
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
    cameraAtPress_ = camera_.state();

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
        camera_.setState(cameraAtPress_);
        camera_.orbitByPixels(delta.x(), delta.y());
        update();
    } else if (interactionMode_ == InteractionMode::Pan) {
        camera_.setState(cameraAtPress_);
        camera_.panByPixels(delta.x(), delta.y(), height());
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
    camera_.zoomByFactor(factor);
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
    const ThreeDViewerProjectedPoint origin = ThreeDViewerProjection::projectPoint(camera_, {0.0, 0.0, 0.0}, width(), height());
    const ThreeDViewerProjectedPoint xAxis = ThreeDViewerProjection::projectPoint(camera_, {40.0, 0.0, 0.0}, width(), height());
    const ThreeDViewerProjectedPoint yAxis = ThreeDViewerProjection::projectPoint(camera_, {0.0, 40.0, 0.0}, width(), height());
    const ThreeDViewerProjectedPoint zAxis = ThreeDViewerProjection::projectPoint(camera_, {0.0, 0.0, 40.0}, width(), height());

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
                    if (ThreeDViewerProjection::projectLine(camera_, currentPoint, rightPoint, width(), height(), &fromScreen, &toScreen)) {
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
                        if (ThreeDViewerProjection::projectLine(camera_, currentPoint, downPoint, width(), height(), &fromScreen, &toScreen)) {
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

            const ThreeDViewerProjectedPoint p0 = ThreeDViewerProjection::projectPoint(camera_, mesh.vertices.at(triangle[0]), width(), height());
            const ThreeDViewerProjectedPoint p1 = ThreeDViewerProjection::projectPoint(camera_, mesh.vertices.at(triangle[1]), width(), height());
            const ThreeDViewerProjectedPoint p2 = ThreeDViewerProjection::projectPoint(camera_, mesh.vertices.at(triangle[2]), width(), height());
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
        if (!ThreeDViewerProjection::projectLine(camera_, fromStation->position, toStation->position, width(), height(), &fromScreen, &toScreen)) {
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
        const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera_, station.position, width(), height());
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

        const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera_, station.position, width(), height());
        if (!projected.visible) {
            continue;
        }

        painter.drawText(projected.screenPosition + QPointF(8.0, -8.0), station.name);
    }
    painter.restore();
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
