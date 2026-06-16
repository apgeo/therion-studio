#include "ThreeDViewerViewportRenderer.h"

#include "ThreeDViewerProjection.h"

#include <QColor>
#include <QCoreApplication>
#include <QPainter>
#include <QPalette>
#include <QPolygonF>

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
namespace
{
QColor shotColorForFlags(const ThreeDViewerShot &shot)
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

QColor stationColorForFlags(const ThreeDViewerStation &station)
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
} // namespace

void ThreeDViewerViewportRenderer::paintEmptyState(QPainter &painter, const QRect &bounds, const QPalette &palette)
{
    painter.setPen(palette.color(QPalette::Text));
    painter.drawText(bounds.adjusted(24, 24, -24, -24),
                     Qt::AlignCenter,
                     QCoreApplication::translate("TherionStudio::ThreeDViewerViewportRenderer",
                                                 "No 3D data loaded."));
}

void ThreeDViewerViewportRenderer::paintScene(QPainter &painter,
                                              const ThreeDViewerSceneModel &sceneModel,
                                              const ThreeDViewerCamera &camera,
                                              const std::array<bool, 5> &layerVisibility,
                                              const QPalette &palette,
                                              int viewportWidth,
                                              int viewportHeight)
{
    const auto paintAxes = [&](QPainter &targetPainter) {
        const ThreeDViewerProjectedPoint origin = ThreeDViewerProjection::projectPoint(camera, {0.0, 0.0, 0.0}, viewportWidth, viewportHeight);
        const ThreeDViewerProjectedPoint xAxis = ThreeDViewerProjection::projectPoint(camera, {40.0, 0.0, 0.0}, viewportWidth, viewportHeight);
        const ThreeDViewerProjectedPoint yAxis = ThreeDViewerProjection::projectPoint(camera, {0.0, 40.0, 0.0}, viewportWidth, viewportHeight);
        const ThreeDViewerProjectedPoint zAxis = ThreeDViewerProjection::projectPoint(camera, {0.0, 0.0, 40.0}, viewportWidth, viewportHeight);

        if (!origin.visible) {
            return;
        }

        targetPainter.save();
        targetPainter.setPen(cosmeticPen(QColor(QStringLiteral("#dc2626")), 1.3));
        if (xAxis.visible) {
            targetPainter.drawLine(origin.screenPosition, xAxis.screenPosition);
        }
        targetPainter.setPen(cosmeticPen(QColor(QStringLiteral("#16a34a")), 1.3));
        if (yAxis.visible) {
            targetPainter.drawLine(origin.screenPosition, yAxis.screenPosition);
        }
        targetPainter.setPen(cosmeticPen(QColor(QStringLiteral("#2563eb")), 1.3));
        if (zAxis.visible) {
            targetPainter.drawLine(origin.screenPosition, zAxis.screenPosition);
        }
        targetPainter.restore();
    };

    const auto paintSurfaces = [&](QPainter &targetPainter) {
        const QPen outlinePen = cosmeticPen(QColor(QStringLiteral("#7c3aed")), 1.0);
        const QColor fillColor = translucentColor(QColor(QStringLiteral("#c084fc")), 20);

        targetPainter.save();
        targetPainter.setPen(outlinePen);
        targetPainter.setBrush(fillColor);
        for (const ThreeDViewerSurface &surface : sceneModel.surfaces) {
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
                        if (ThreeDViewerProjection::projectLine(camera, currentPoint, rightPoint, viewportWidth, viewportHeight, &fromScreen, &toScreen)) {
                            targetPainter.drawLine(fromScreen, toScreen);
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
                            if (ThreeDViewerProjection::projectLine(camera, currentPoint, downPoint, viewportWidth, viewportHeight, &fromScreen, &toScreen)) {
                                targetPainter.drawLine(fromScreen, toScreen);
                            }
                        }
                    }
                }
            }
        }
        targetPainter.restore();
    };

    const auto paintMeshes = [&](QPainter &targetPainter) {
        const QPen meshPen = cosmeticPen(QColor(QStringLiteral("#16a34a")), 1.0, Qt::DashLine);

        targetPainter.save();
        targetPainter.setPen(meshPen);
        targetPainter.setBrush(Qt::NoBrush);
        for (const ThreeDViewerMeshGroup &mesh : sceneModel.meshGroups) {
            for (const std::array<quint32, 3> &triangle : mesh.triangles) {
                if (triangle[0] >= quint32(mesh.vertices.size())
                    || triangle[1] >= quint32(mesh.vertices.size())
                    || triangle[2] >= quint32(mesh.vertices.size())) {
                    continue;
                }

                const ThreeDViewerProjectedPoint p0 = ThreeDViewerProjection::projectPoint(camera, mesh.vertices.at(triangle[0]), viewportWidth, viewportHeight);
                const ThreeDViewerProjectedPoint p1 = ThreeDViewerProjection::projectPoint(camera, mesh.vertices.at(triangle[1]), viewportWidth, viewportHeight);
                const ThreeDViewerProjectedPoint p2 = ThreeDViewerProjection::projectPoint(camera, mesh.vertices.at(triangle[2]), viewportWidth, viewportHeight);
                if (!p0.visible || !p1.visible || !p2.visible) {
                    continue;
                }

                QPolygonF polygon;
                polygon << p0.screenPosition << p1.screenPosition << p2.screenPosition;
                targetPainter.drawPolygon(polygon);
            }
        }
        targetPainter.restore();
    };

    const auto paintShots = [&](QPainter &targetPainter) {
        targetPainter.save();
        for (const ThreeDViewerShot &shot : sceneModel.shots) {
            const ThreeDViewerStation *fromStation = nullptr;
            const ThreeDViewerStation *toStation = nullptr;
            for (const ThreeDViewerStation &station : sceneModel.stations) {
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

            targetPainter.setPen(cosmeticPen(shotColorForFlags(shot),
                                             shot.hidden ? 1.0 : 2.0,
                                             shot.hidden ? Qt::DashLine : Qt::SolidLine));
            targetPainter.drawLine(fromScreen, toScreen);
        }
        targetPainter.restore();
    };

    const auto paintStations = [&](QPainter &targetPainter) {
        const QPen pen = cosmeticPen(QColor(QStringLiteral("#ef4444")), 1.2);

        targetPainter.save();
        for (const ThreeDViewerStation &station : sceneModel.stations) {
            const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera, station.position, viewportWidth, viewportHeight);
            if (!projected.visible) {
                continue;
            }

            targetPainter.setPen(pen);
            targetPainter.setBrush(stationColorForFlags(station));
            targetPainter.drawEllipse(projected.screenPosition, 4.5, 4.5);
        }
        targetPainter.restore();
    };

    const auto paintLabels = [&](QPainter &targetPainter) {
        targetPainter.save();
        targetPainter.setPen(palette.color(QPalette::Text));
        for (const ThreeDViewerStation &station : sceneModel.stations) {
            if (station.name.isEmpty()) {
                continue;
            }

            const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera, station.position, viewportWidth, viewportHeight);
            if (!projected.visible) {
                continue;
            }

            targetPainter.drawText(projected.screenPosition + QPointF(8.0, -8.0), station.name);
        }
        targetPainter.restore();
    };

    paintAxes(painter);
    if (layerVisibility[kSurfacesLayer]) {
        paintSurfaces(painter);
    }
    if (layerVisibility[kMeshesLayer]) {
        paintMeshes(painter);
    }
    if (layerVisibility[kCenterlineLayer]) {
        paintShots(painter);
    }
    if (layerVisibility[kStationsLayer]) {
        paintStations(painter);
    }
    if (layerVisibility[kLabelsLayer]) {
        paintLabels(painter);
    }
}

} // namespace TherionStudio
