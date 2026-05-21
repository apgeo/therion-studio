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

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};
}
