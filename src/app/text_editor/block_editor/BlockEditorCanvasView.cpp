#include "BlockEditorCanvasView.h"

#include "BlockEditorDragMime.h"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>

namespace TherionStudio
{
BlockEditorCanvasView::BlockEditorCanvasView(QWidget *parent)
    : QGraphicsView(parent)
{
    setAcceptDrops(true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setRenderHint(QPainter::Antialiasing, true);
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
