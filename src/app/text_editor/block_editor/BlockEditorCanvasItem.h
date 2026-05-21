#pragma once

#include <QColor>
#include <QGraphicsObject>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <functional>

namespace TherionStudio
{
constexpr int kBlockEditorCanvasEndHintContainerLineDataRole = 0x42554e44; // "BUND"

QColor blockEditorCanvasBaseColorForDirective(const QString &directive);

class BlockCanvasItem final : public QGraphicsObject
{
public:
    BlockCanvasItem(const QString &kind,
                    const QString &name,
                    const QString &inlineComment,
                    int lineNumber,
                    bool deletable = true,
                    bool movable = true,
                    bool isContainerOpen = false,
                    QGraphicsItem *parent = nullptr);

    std::function<void(int)> onDelete;
    std::function<void(int, const QPointF &)> onMoveRequest;
    std::function<void(int, const QPointF &, bool)> onMovePreview;

    QRectF boundingRect() const override;
    void setName(const QString &name);
    void setInlineComment(const QString &inlineComment);
    QString kind() const;
    int lineNumber() const;
    void setLineNumber(int lineNumber);
    bool isContainerOpen() const;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QRectF deleteIconRect() const;
    QRectF inlineCommentBadgeRect() const;

    QString kind_;
    QString name_;
    QString inlineComment_;
    int lineNumber_ = 0;
    bool deletable_ = true;
    bool movable_ = true;
    bool isContainerOpen_ = false;
    QPointF dragStartScenePos_;
    QPointF dragStartItemPos_;
    bool dragging_ = false;
};
}
