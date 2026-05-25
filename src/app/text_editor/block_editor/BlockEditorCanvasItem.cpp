#include "BlockEditorCanvasItem.h"

#include "BlockEditorDirectiveRules.h"

#include <QApplication>
#include <QFile>
#include <QGraphicsSceneMouseEvent>
#include <QHash>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QStyle>
#include <QSvgRenderer>

namespace TherionStudio
{
namespace
{
bool blockEditorUsesDarkPalette()
{
    const QColor base = QApplication::palette().color(QPalette::Base);
    return base.isValid() && base.lightnessF() < 0.5;
}

QColor readableTextColorForFill(const QColor &fill)
{
    return fill.lightnessF() < 0.5
        ? QColor(QStringLiteral("#f2f5f9"))
        : QColor(QStringLiteral("#1f1f1f"));
}

QPixmap renderLucidePixmap(const QString &iconName,
                           const QColor &color,
                           int extent,
                           qreal devicePixelRatio)
{
    const QString cacheKey = QStringLiteral("%1|%2|%3|%4")
                                 .arg(iconName,
                                      color.name(QColor::HexArgb))
                                 .arg(extent)
                                 .arg(devicePixelRatio, 0, 'f', 2);
    static QHash<QString, QPixmap> sPixmapCache;
    if (sPixmapCache.contains(cacheKey)) {
        return sPixmapCache.value(cacheKey);
    }

    QFile file(QStringLiteral(":/resources/icons/lucide/%1.svg").arg(iconName));
    if (!file.open(QIODevice::ReadOnly)) {
        return QPixmap();
    }

    QString svg = QString::fromUtf8(file.readAll());
    svg.replace(QStringLiteral("currentColor"), color.name(QColor::HexRgb));
    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid()) {
        return QPixmap();
    }

    QPixmap pixmap(QSize(extent, extent) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, extent, extent));
    sPixmapCache.insert(cacheKey, pixmap);
    return pixmap;
}
}

QColor blockEditorCanvasBaseColorForDirective(const QString &directive)
{
    const QString normalizedKind = BlockEditorDirectiveRules::normalizeDirective(directive);
    const bool darkPalette = blockEditorUsesDarkPalette();
    if (normalizedKind == QStringLiteral("survey")) {
        return darkPalette ? QColor(QStringLiteral("#243f2b")) : QColor(QStringLiteral("#d7f7d7"));
    }
    if (normalizedKind == QStringLiteral("centerline")) {
        return darkPalette ? QColor(QStringLiteral("#493520")) : QColor(QStringLiteral("#ffe9cc"));
    }
    if (normalizedKind == QStringLiteral("data")) {
        return darkPalette ? QColor(QStringLiteral("#372b4d")) : QColor(QStringLiteral("#e8dbff"));
    }
    if (normalizedKind == QStringLiteral("map")) {
        return darkPalette ? QColor(QStringLiteral("#1f4543")) : QColor(QStringLiteral("#d5f3f0"));
    }
    if (BlockEditorDirectiveRules::isUnrecognizedKind(normalizedKind)) {
        return darkPalette ? QColor(QStringLiteral("#4a2d2d")) : QColor(QStringLiteral("#ffd9d2"));
    }
    if (BlockEditorDirectiveRules::isMapObjectReferenceKind(normalizedKind)) {
        return darkPalette ? QColor(QStringLiteral("#493d1c")) : QColor(QStringLiteral("#fff0c2"));
    }
    return darkPalette ? QColor(QStringLiteral("#25384f")) : QColor(QStringLiteral("#d8e9ff"));
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
    return QRectF(0.0, 0.0, 290.0, 42.0);
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
    const QPalette palette = QApplication::palette();
    const QColor textColor = readableTextColorForFill(baseColor);
    const QColor selectedBorder = palette.color(QPalette::Highlight);
    const QColor normalBorder = palette.color(QPalette::Mid);
    const QColor iconColor = palette.color(QPalette::ButtonText);
    const bool darkPalette = blockEditorUsesDarkPalette();

    painter->setPen(QPen(isSelected() ? selectedBorder : normalBorder, isSelected() ? 2.0 : 1.0));
    painter->setBrush(baseColor);
    painter->drawRoundedRect(boundingRect(), 6.0, 6.0);

    QRectF gripButtonRect;
    if (movable_) {
        gripButtonRect = moveGripIconRect();
        const QRect gripIconBounds = gripButtonRect.adjusted(2.0, 2.0, -2.0, -2.0).toRect();
        const QPixmap gripPixmap = renderLucidePixmap(QStringLiteral("grip-vertical"),
                                                      iconColor,
                                                      qMax(8, gripIconBounds.width()),
                                                      painter->device()->devicePixelRatioF());
        if (!gripPixmap.isNull()) {
            painter->drawPixmap(gripIconBounds.topLeft(), gripPixmap);
        }
    }

    QRectF deleteButtonRect;
    if (deletable_) {
        deleteButtonRect = deleteIconRect();
        const QRect deleteIconBounds = deleteButtonRect.adjusted(4.0, 4.0, -4.0, -4.0).toRect();
        const QPixmap deletePixmap = renderLucidePixmap(QStringLiteral("trash-2"),
                                                        iconColor,
                                                        qMax(8, deleteIconBounds.width()),
                                                        painter->device()->devicePixelRatioF());
        if (!deletePixmap.isNull()) {
            painter->drawPixmap(deleteIconBounds.topLeft(), deletePixmap);
        }
    }

    QRectF commentBadgeRect;
    if (!inlineComment_.trimmed().isEmpty()) {
        commentBadgeRect = inlineCommentBadgeRect();
        painter->setPen(QPen(darkPalette ? QColor(QStringLiteral("#d7b95f")) : QColor(QStringLiteral("#7c6a42")), 1.0));
        painter->setBrush(darkPalette ? QColor(QStringLiteral("#3a3018")) : QColor(QStringLiteral("#fff1c9")));
        painter->drawRoundedRect(commentBadgeRect, 4.0, 4.0);
        painter->setPen(darkPalette ? QColor(QStringLiteral("#f4dc8b")) : QColor(QStringLiteral("#5f4a1e")));
        painter->drawText(commentBadgeRect, Qt::AlignCenter, QStringLiteral("#"));
    }

    const QString kindLabel = BlockEditorDirectiveRules::blockDisplayKindLabel(kind_);
    const QString title = name_.isEmpty() ? kindLabel : QStringLiteral("%1: %2").arg(kindLabel, name_);
    painter->setPen(textColor);
    qreal textRight = deletable_ ? deleteButtonRect.left() - 16.0 : boundingRect().right() - 10.0;
    if (!gripButtonRect.isNull()) {
        textRight = qMin(textRight, gripButtonRect.left() - 10.0);
    }
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

QRectF BlockCanvasItem::moveGripIconRect() const
{
    if (!movable_) {
        return QRectF();
    }
    const qreal iconSize = 18.0;
    const qreal margin = 8.0;
    qreal rightAnchor = boundingRect().right() - margin;
    if (deletable_) {
        rightAnchor = deleteIconRect().left() - 6.0;
    }
    return QRectF(rightAnchor - iconSize, 12.0, iconSize, iconSize);
}

QRectF BlockCanvasItem::inlineCommentBadgeRect() const
{
    const qreal badgeWidth = 22.0;
    const qreal badgeHeight = 20.0;
    const qreal margin = 8.0;
    qreal rightAnchor = boundingRect().right() - margin;
    if (movable_) {
        rightAnchor = moveGripIconRect().left() - 8.0;
    } else if (deletable_) {
        rightAnchor = deleteIconRect().left() - 8.0;
    }
    return QRectF(rightAnchor - badgeWidth, 11.0, badgeWidth, badgeHeight);
}
}
