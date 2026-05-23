#include "BlockEditorCanvasItem.h"

#include "BlockEditorDirectiveRules.h"

#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QStyle>

namespace TherionStudio
{
QColor blockEditorCanvasBaseColorForDirective(const QString &directive)
{
    const QString normalizedKind = BlockEditorDirectiveRules::normalizeDirective(directive);
    if (normalizedKind == QStringLiteral("survey")) {
        return QColor(QStringLiteral("#d7f7d7"));
    }
    if (normalizedKind == QStringLiteral("centerline")) {
        return QColor(QStringLiteral("#ffe9cc"));
    }
    if (normalizedKind == QStringLiteral("data")) {
        return QColor(QStringLiteral("#e8dbff"));
    }
    if (normalizedKind == QStringLiteral("map")) {
        return QColor(QStringLiteral("#d5f3f0"));
    }
    if (BlockEditorDirectiveRules::isMapObjectReferenceKind(normalizedKind)) {
        return QColor(QStringLiteral("#fff0c2"));
    }
    return QColor(QStringLiteral("#d8e9ff"));
}

BlockCanvasItem::BlockCanvasItem(const QString &kind,
                                 const QString &name,
                                 const QString &inlineComment,
                                 int lineNumber,
                                 bool deletable,
                                 bool movable,
                                 bool isContainerOpen,
                                 QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , kind_(kind)
    , name_(name)
    , inlineComment_(inlineComment)
    , lineNumber_(lineNumber)
    , deletable_(deletable)
    , movable_(movable)
    , isContainerOpen_(isContainerOpen)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, movable_);
    setCursor(movable_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
    setToolTip(inlineComment_.trimmed().isEmpty() ? QString() : inlineComment_.trimmed());
}

QRectF BlockCanvasItem::boundingRect() const
{
    return QRectF(0.0, 0.0, 260.0, 42.0);
}

void BlockCanvasItem::setName(const QString &name)
{
    name_ = name;
    update();
}

void BlockCanvasItem::setInlineComment(const QString &inlineComment)
{
    inlineComment_ = inlineComment;
    setToolTip(inlineComment_.trimmed().isEmpty() ? QString() : inlineComment_.trimmed());
    update();
}

QString BlockCanvasItem::kind() const
{
    return kind_;
}

int BlockCanvasItem::lineNumber() const
{
    return lineNumber_;
}

void BlockCanvasItem::setLineNumber(int lineNumber)
{
    lineNumber_ = lineNumber;
}

bool BlockCanvasItem::isContainerOpen() const
{
    return isContainerOpen_;
}

void BlockCanvasItem::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    if (painter == nullptr) {
        return;
    }

    const QColor baseColor = blockEditorCanvasBaseColorForDirective(kind_);

    painter->setPen(QPen(isSelected() ? QColor(QStringLiteral("#2f6fed")) : QColor(QStringLiteral("#8c8c8c")), isSelected() ? 2.0 : 1.0));
    painter->setBrush(baseColor);
    painter->drawRoundedRect(boundingRect(), 6.0, 6.0);

    QRectF deleteButtonRect;
    if (deletable_) {
        deleteButtonRect = deleteIconRect();
        painter->setPen(QPen(QColor(QStringLiteral("#6a6a6a")), 1.0));
        painter->setBrush(QColor(QStringLiteral("#f2f2f2")));
        painter->drawRoundedRect(deleteButtonRect, 4.0, 4.0);

        const QRect deleteIconBounds = deleteButtonRect.adjusted(4.0, 4.0, -4.0, -4.0).toRect();
        if (QStyle *style = QApplication::style(); style != nullptr) {
            style->standardIcon(QStyle::SP_TrashIcon).paint(painter, deleteIconBounds);
        }
    }

    QRectF commentBadgeRect;
    if (!inlineComment_.trimmed().isEmpty()) {
        commentBadgeRect = inlineCommentBadgeRect();
        painter->setPen(QPen(QColor(QStringLiteral("#7c6a42")), 1.0));
        painter->setBrush(QColor(QStringLiteral("#fff1c9")));
        painter->drawRoundedRect(commentBadgeRect, 4.0, 4.0);
        painter->setPen(QColor(QStringLiteral("#5f4a1e")));
        painter->drawText(commentBadgeRect, Qt::AlignCenter, QStringLiteral("#"));
    }

    const QString kindLabel = BlockEditorDirectiveRules::blockDisplayKindLabel(kind_);
    const QString title = name_.isEmpty() ? kindLabel : QStringLiteral("%1: %2").arg(kindLabel, name_);
    painter->setPen(QColor(QStringLiteral("#1f1f1f")));
    qreal textRight = deletable_ ? deleteButtonRect.left() - 16.0 : boundingRect().right() - 10.0;
    if (!commentBadgeRect.isNull()) {
        textRight = qMin(textRight, commentBadgeRect.left() - 10.0);
    }
    painter->drawText(QRectF(10.0, 0.0, textRight, boundingRect().height()),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      title);
}

void BlockCanvasItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (deletable_ && event != nullptr && deleteIconRect().contains(event->pos())) {
        if (onDelete) {
            onDelete(lineNumber_);
        }
        event->accept();
        return;
    }

    dragStartScenePos_ = event != nullptr ? event->scenePos() : QPointF();
    dragStartItemPos_ = pos();
    dragging_ = false;
    if (onMovePreview) {
        onMovePreview(lineNumber_, QPointF(), false);
    }
    if (!movable_) {
        QGraphicsObject::mousePressEvent(event);
        return;
    }
    QGraphicsObject::mousePressEvent(event);
}

void BlockCanvasItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!movable_) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }
    if (event != nullptr && (event->buttons() & Qt::LeftButton)) {
        if (!dragging_ && (event->scenePos() - dragStartScenePos_).manhattanLength() >= 4.0) {
            dragging_ = true;
            setCursor(Qt::ClosedHandCursor);
        }
    }
    QGraphicsObject::mouseMoveEvent(event);
    if (dragging_ && onMovePreview) {
        const QPointF previewScenePos = event != nullptr
            ? event->scenePos()
            : mapToScene(boundingRect().center());
        onMovePreview(lineNumber_, previewScenePos, true);
    }
}

void BlockCanvasItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mouseReleaseEvent(event);
    setCursor(movable_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
    if (!movable_) {
        if (onMovePreview) {
            onMovePreview(lineNumber_, QPointF(), false);
        }
        dragging_ = false;
        return;
    }
    const bool moved = (pos() - dragStartItemPos_).manhattanLength() >= 2.0;
    if (!moved) {
        if (onMovePreview) {
            onMovePreview(lineNumber_, QPointF(), false);
        }
        dragging_ = false;
        return;
    }

    const QPointF dropScenePos = event != nullptr
        ? event->scenePos()
        : mapToScene(boundingRect().center());
    setPos(dragStartItemPos_);
    dragging_ = false;
    if (onMoveRequest) {
        onMoveRequest(lineNumber_, dropScenePos);
    }
}

QRectF BlockCanvasItem::deleteIconRect() const
{
    const qreal iconSize = 24.0;
    const qreal margin = 8.0;
    const qreal x = boundingRect().right() - margin - iconSize;
    return QRectF(x, 9.0, iconSize, iconSize);
}

QRectF BlockCanvasItem::inlineCommentBadgeRect() const
{
    const qreal badgeWidth = 22.0;
    const qreal badgeHeight = 20.0;
    const qreal margin = 8.0;
    const qreal rightAnchor = deletable_ ? deleteIconRect().left() - 8.0 : boundingRect().right() - margin;
    return QRectF(rightAnchor - badgeWidth, 11.0, badgeWidth, badgeHeight);
}
}
