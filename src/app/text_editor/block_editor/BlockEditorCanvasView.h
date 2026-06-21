#pragma once

#include <QGraphicsView>
#include <QPointF>
#include <QString>

#include <functional>

namespace TherionStudio
{
class BlockEditorCanvasView final : public QGraphicsView
{
public:
    explicit BlockEditorCanvasView(QWidget *parent = nullptr);

    std::function<void(const QString &, const QPointF &)> onDropBlock;
    std::function<void(const QString &, const QPointF &, bool)> onDragPreview;
    std::function<int(const QPointF &)> blockLineAtScenePosition;
    std::function<void(int, const QPointF &, bool)> onBlockMovePreview;
    std::function<void(int, const QPointF &)> onBlockMoveRequest;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    int movingBlockLineNumber_ = 0;
    QPoint movePressViewportPos_;
    QPointF movePressScenePos_;
    bool blockMoveActive_ = false;
};
}
