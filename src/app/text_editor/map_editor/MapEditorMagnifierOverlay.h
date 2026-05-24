#pragma once

#include <QPoint>
#include <QPointF>
#include <QString>
#include <QWidget>

class QGraphicsView;

namespace TherionStudio
{
class MapEditorMagnifierOverlay final : public QWidget
{
public:
    explicit MapEditorMagnifierOverlay(QGraphicsView *view, QWidget *parent = nullptr);

    void setView(QGraphicsView *view);
    void setCursorPosition(const QPoint &viewportPosition,
                           const QPointF &scenePosition,
                           const QString &coordinateText);
    void setOverlayActive(bool active);
    void updatePlacement();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRect contentRect() const;
    QRectF sceneSourceRect() const;

    QGraphicsView *view_ = nullptr;
    QPoint viewportPosition_;
    QPointF scenePosition_;
    QString coordinateText_;
    bool hasCursorPosition_ = false;
    bool active_ = false;
};
}
