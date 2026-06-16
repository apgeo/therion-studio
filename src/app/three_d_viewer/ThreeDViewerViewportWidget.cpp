#include "ThreeDViewerViewportWidget.h"

#include "ThreeDViewerViewportRenderer.h"

#include "../../core/ThreeDViewerCamera.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>

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
        ThreeDViewerViewportRenderer::paintEmptyState(painter, rect(), palette());
        return;
    }

    ThreeDViewerViewportRenderer::paintScene(painter, sceneModel_, camera_, layerVisibility_, palette(), width(), height());
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

} // namespace TherionStudio
