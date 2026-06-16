#include "ThreeDViewerViewportWidget.h"

#include "ThreeDViewerViewportRenderer.h"

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
    connect(&controller_, &ThreeDViewerViewportController::cameraChanged, this, QOverload<>::of(&QWidget::update));
}

void ThreeDViewerViewportWidget::setSceneModel(const ThreeDViewerSceneModel &sceneModel)
{
    sceneModel_ = sceneModel;
    controller_.fitToScene(sceneModel_);
}

void ThreeDViewerViewportWidget::setLayerVisibility(const std::array<bool, 5> &layerVisibility)
{
    layerVisibility_ = layerVisibility;
    update();
}

void ThreeDViewerViewportWidget::fitToScene()
{
    controller_.fitToScene(sceneModel_);
}

void ThreeDViewerViewportWidget::resetView()
{
    controller_.resetView(sceneModel_);
}

void ThreeDViewerViewportWidget::setViewPreset(ThreeDViewerViewPreset preset)
{
    controller_.setViewPreset(preset, sceneModel_);
}

void ThreeDViewerViewportWidget::rollLeft()
{
    controller_.rotateLeft();
}

void ThreeDViewerViewportWidget::rollRight()
{
    controller_.rotateRight();
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

    ThreeDViewerViewportRenderer::paintScene(painter, sceneModel_, controller_.camera(), layerVisibility_, palette(), width(), height());
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

    if (controller_.mousePress(event->button(), event->pos())) {
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void ThreeDViewerViewportWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (controller_.mouseMove(event->pos(), height())) {
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void ThreeDViewerViewportWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event == nullptr) {
        return;
    }

    if (controller_.mouseRelease(event->button())) {
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

    if (!controller_.wheel(event->angleDelta())) {
        QWidget::wheelEvent(event);
        return;
    }
    event->accept();
}

} // namespace TherionStudio
