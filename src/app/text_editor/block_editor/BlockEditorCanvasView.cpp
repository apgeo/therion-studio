#include "BlockEditorCanvasView.h"

#include "BlockEditorDragMime.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <QtGlobal>

namespace TherionStudio
{
BlockEditorCanvasView::BlockEditorCanvasView(QWidget *parent)
    : QGraphicsView(parent)
{
    setAcceptDrops(true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setRenderHint(QPainter::Antialiasing, true);
}

void BlockEditorCanvasView::mousePressEvent(QMouseEvent *event)
{
    QGraphicsView::mousePressEvent(event);
    movingBlockLineNumber_ = 0;
    blockMoveActive_ = false;
    if (event == nullptr || event->button() != Qt::LeftButton || !blockLineAtScenePosition) {
        return;
    }

    const QPointF scenePos = mapToScene(event->position().toPoint());
    const int lineNumber = blockLineAtScenePosition(scenePos);
    if (lineNumber <= 0) {
        return;
    }

    movingBlockLineNumber_ = lineNumber;
    movePressViewportPos_ = event->position().toPoint();
    movePressScenePos_ = scenePos;
}

void BlockEditorCanvasView::mouseMoveEvent(QMouseEvent *event)
{
    if (event != nullptr
        && movingBlockLineNumber_ > 0
        && (event->buttons() & Qt::LeftButton)) {
        const int dragDistance = (event->position().toPoint() - movePressViewportPos_).manhattanLength();
        if (!blockMoveActive_ && dragDistance >= QApplication::startDragDistance()) {
            blockMoveActive_ = true;
            setCursor(Qt::ClosedHandCursor);
        }
        if (blockMoveActive_) {
            if (onBlockMovePreview) {
                onBlockMovePreview(movingBlockLineNumber_, mapToScene(event->position().toPoint()), true);
            }
            event->accept();
            return;
        }
    }

    QGraphicsView::mouseMoveEvent(event);
}

void BlockEditorCanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event != nullptr && movingBlockLineNumber_ > 0 && event->button() == Qt::LeftButton) {
        const int lineNumber = movingBlockLineNumber_;
        const bool moved = blockMoveActive_
            || (event->position().toPoint() - movePressViewportPos_).manhattanLength() >= QApplication::startDragDistance();
        const QPointF releaseScenePos = mapToScene(event->position().toPoint());
        movingBlockLineNumber_ = 0;
        blockMoveActive_ = false;
        unsetCursor();
        if (onBlockMovePreview) {
            onBlockMovePreview(lineNumber, QPointF(), false);
        }
        if (moved && onBlockMoveRequest) {
            onBlockMoveRequest(lineNumber, releaseScenePos);
            event->accept();
            return;
        }
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void BlockEditorCanvasView::wheelEvent(QWheelEvent *event)
{
    if (event == nullptr) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    QPoint panDelta = event->pixelDelta();
    if (panDelta.isNull()) {
        const QPoint angleDelta = event->angleDelta();
        panDelta = QPoint(qRound(static_cast<qreal>(angleDelta.x()) / 4.0),
                          qRound(static_cast<qreal>(angleDelta.y()) / 4.0));
    }

    if (panDelta.isNull()) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    if (horizontalScrollBar() != nullptr) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - panDelta.x());
    }
    if (verticalScrollBar() != nullptr) {
        verticalScrollBar()->setValue(verticalScrollBar()->value() - panDelta.y());
    }

    event->accept();
}

void BlockEditorCanvasView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event == nullptr || event->mimeData() == nullptr) {
        return;
    }
    if (event->mimeData()->hasFormat(QString::fromUtf8(kBlockEditorDragMimeType))) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragEnterEvent(event);
}

void BlockEditorCanvasView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event == nullptr || event->mimeData() == nullptr) {
        return;
    }
    if (event->mimeData()->hasFormat(QString::fromUtf8(kBlockEditorDragMimeType))) {
        if (onDragPreview) {
            const QString kind = QString::fromUtf8(event->mimeData()->data(QString::fromUtf8(kBlockEditorDragMimeType))).trimmed();
            onDragPreview(kind, mapToScene(event->position().toPoint()), true);
        }
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dragMoveEvent(event);
}

void BlockEditorCanvasView::dragLeaveEvent(QDragLeaveEvent *event)
{
    if (onDragPreview) {
        onDragPreview(QString(), QPointF(), false);
    }
    QGraphicsView::dragLeaveEvent(event);
}

void BlockEditorCanvasView::dropEvent(QDropEvent *event)
{
    if (event == nullptr || event->mimeData() == nullptr) {
        return;
    }
    if (!event->mimeData()->hasFormat(QString::fromUtf8(kBlockEditorDragMimeType))) {
        QGraphicsView::dropEvent(event);
        return;
    }

    const QString kind = QString::fromUtf8(event->mimeData()->data(QString::fromUtf8(kBlockEditorDragMimeType))).trimmed();
    if (kind.isEmpty()) {
        return;
    }

    if (onDropBlock) {
        const QPointF scenePosition = mapToScene(event->position().toPoint());
        onDropBlock(kind, scenePosition);
    }
    if (onDragPreview) {
        onDragPreview(QString(), QPointF(), false);
    }
    event->acceptProposedAction();
}
}
