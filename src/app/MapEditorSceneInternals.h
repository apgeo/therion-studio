#pragma once

#include <QColor>
#include <QCursor>
#include <QGraphicsEllipseItem>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QObject>
#include <QStringList>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QTransform>
#include <QVariant>
#include <QVector>
#include <functional>

namespace TherionStudio {

inline bool mapSourcePointsDiffer(const QPointF &a, const QPointF &b)
{
    return (a - b).manhattanLength() > 0.01;
}

struct CoordinateTransform
{
    bool valid = false;
    QTransform sourceToMap;
    QTransform mapToSource;
};

QRectF sceneCoordsPreviewBounds(const QRectF &sourceBounds, const QRectF &targetBounds);
QPointF sceneCoordsSourceToPreview(const QPointF &source, const QRectF &sourceBounds, const QRectF &previewBounds);
QPointF sceneCoordsPreviewToSource(const QPointF &preview, const QRectF &sourceBounds, const QRectF &previewBounds);
qreal sceneCoordsScaleFactor(const QRectF &sourceBounds, const QRectF &previewBounds);
CoordinateTransform coordinateTransformFromScrapScale(const QStringList &tokens);
QVector<QPointF> pointsFromTokens(const QStringList &tokens);

class MapEditablePointItem final : public QGraphicsEllipseItem
{
public:
    MapEditablePointItem(int lineNumber,
                         const QPointF &sourcePoint,
                         const QRectF &sourceBounds,
                         const QRectF &previewBounds)
        : QGraphicsEllipseItem(QRectF(-5.0, -5.0, 10.0, 10.0))
        , lineNumber_(lineNumber)
        , sourceBounds_(sourceBounds)
        , previewBounds_(previewBounds)
        , fittedBounds_(previewBounds)
    {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setAcceptHoverEvents(true);
        setCursor(Qt::OpenHandCursor);
        setToolTip(QObject::tr("Point handle (line %1)").arg(lineNumber_));
        setPos(sceneCoordsSourceToPreview(sourcePoint, sourceBounds, previewBounds));
    }

    void setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback)
    {
        moveCommittedCallback_ = std::move(callback);
    }

    void setDisplayToSourceMapper(std::function<QPointF(const QPointF &)> mapper)
    {
        displayToSourceMapper_ = std::move(mapper);
    }

    QPointF sourcePoint() const
    {
        return mapDisplayToSource(previewToSource(pos()));
    }

protected:
    QRectF boundingRect() const override
    {
        // Paint includes scaled marker plus halo; keep update region larger to avoid drag trails.
        return QGraphicsEllipseItem::boundingRect().adjusted(-6.0, -6.0, 6.0, 6.0);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        Q_UNUSED(widget);

        const bool selected = option != nullptr && (option->state & QStyle::State_Selected);
        const bool emphasize = selected || hoverActive_ || dragActive_;
        const qreal scale = dragActive_ ? 1.5 : (selected ? 1.4 : (hoverActive_ ? 1.25 : 1.0));
        const QRectF baseRect = rect();
        const QPointF center = baseRect.center();
        QRectF drawRect = baseRect;
        drawRect.setWidth(baseRect.width() * scale);
        drawRect.setHeight(baseRect.height() * scale);
        drawRect.moveCenter(center);

        const QColor baseFill = brush().color().isValid() ? brush().color() : QColor(24, 24, 24, 170);
        QColor fill = baseFill;
        fill.setAlpha(emphasize ? 235 : 180);
        QColor outline = selected ? QColor(QStringLiteral("#3ba4ff")) : QColor(22, 22, 22, 210);

        painter->setRenderHint(QPainter::Antialiasing, true);
        if (emphasize) {
            QColor halo = selected
                ? QColor(72, 166, 255, 110)
                : (fill.lightnessF() > 0.6 ? QColor(24, 38, 56, 70) : QColor(255, 255, 255, 70));
            painter->setPen(Qt::NoPen);
            painter->setBrush(halo);
            painter->drawEllipse(drawRect.adjusted(-2.2, -2.2, 2.2, 2.2));
        }

        painter->setPen(QPen(outline, selected ? 1.8 : 1.1));
        painter->setBrush(fill);
        painter->drawEllipse(drawRect);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        pressSourcePoint_ = mapDisplayToSource(previewToSource(pos()));
        dragActive_ = true;
        setCursor(Qt::ClosedHandCursor);
        QGraphicsEllipseItem::mousePressEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        dragActive_ = false;
        setCursor(Qt::OpenHandCursor);
        QGraphicsEllipseItem::mouseReleaseEvent(event);

        const QPointF releasedSourcePoint = mapDisplayToSource(previewToSource(pos()));
        if (moveCommittedCallback_ != nullptr && mapSourcePointsDiffer(pressSourcePoint_, releasedSourcePoint)) {
            moveCommittedCallback_(lineNumber_, pressSourcePoint_, releasedSourcePoint);
            // The callback may trigger a scene rebuild that deletes this item.
            return;
        }
        update();
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = true;
        QGraphicsEllipseItem::hoverEnterEvent(event);
        update();
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = false;
        QGraphicsEllipseItem::hoverLeaveEvent(event);
        update();
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == QGraphicsItem::ItemPositionChange) {
            QPointF candidate = value.toPointF();
            candidate.setX(qBound(fittedBounds_.left(), candidate.x(), fittedBounds_.right()));
            candidate.setY(qBound(fittedBounds_.top(), candidate.y(), fittedBounds_.bottom()));
            return candidate;
        }

        return QGraphicsEllipseItem::itemChange(change, value);
    }

private:
    QPointF mapDisplayToSource(const QPointF &displayPoint) const
    {
        if (displayToSourceMapper_ != nullptr) {
            return displayToSourceMapper_(displayPoint);
        }

        return displayPoint;
    }

    QPointF previewToSource(const QPointF &previewPoint) const
    {
        return sceneCoordsPreviewToSource(previewPoint, sourceBounds_, previewBounds_);
    }

    int lineNumber_ = 0;
    QRectF sourceBounds_;
    QRectF previewBounds_;
    QRectF fittedBounds_;
    QPointF pressSourcePoint_;
    bool hoverActive_ = false;
    bool dragActive_ = false;
    std::function<QPointF(const QPointF &)> displayToSourceMapper_;
    std::function<void(int, const QPointF &, const QPointF &)> moveCommittedCallback_;
};

class MapEditableGeometryVertexItem final : public QGraphicsEllipseItem
{
public:
    MapEditableGeometryVertexItem(int lineNumber,
                                  const QString &geometryKind,
                                  int vertexIndex,
                                  const QPointF &sourcePoint,
                                  const QRectF &sourceBounds,
                                  const QRectF &previewBounds)
        : QGraphicsEllipseItem(QRectF(-4.0, -4.0, 8.0, 8.0))
        , lineNumber_(lineNumber)
        , geometryKind_(geometryKind)
        , vertexIndex_(vertexIndex)
        , sourceBounds_(sourceBounds)
        , previewBounds_(previewBounds)
        , fittedBounds_(previewBounds)
    {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setAcceptHoverEvents(true);
        setCursor(Qt::OpenHandCursor);
        setToolTip(QObject::tr("%1 vertex %2 (line %3)")
                       .arg(geometryKind_.isEmpty() ? QObject::tr("Geometry") : geometryKind_)
                       .arg(vertexIndex_ + 1)
                       .arg(lineNumber_));
        setPos(sceneCoordsSourceToPreview(sourcePoint, sourceBounds, previewBounds));
    }

    int lineNumber() const
    {
        return lineNumber_;
    }

    QString geometryKind() const
    {
        return geometryKind_;
    }

    int vertexIndex() const
    {
        return vertexIndex_;
    }

    void setMoveCommittedCallback(std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> callback)
    {
        moveCommittedCallback_ = std::move(callback);
    }

    void setDisplayToSourceMapper(std::function<QPointF(const QPointF &)> mapper)
    {
        displayToSourceMapper_ = std::move(mapper);
    }

    void setMovePreviewCallback(std::function<void(MapEditableGeometryVertexItem *,
                                                   const QPointF &,
                                                   const QPointF &,
                                                   bool)> callback)
    {
        movePreviewCallback_ = std::move(callback);
    }

protected:
    QRectF boundingRect() const override
    {
        // Paint includes scaled marker plus halo; keep update region larger to avoid drag trails.
        return QGraphicsEllipseItem::boundingRect().adjusted(-6.0, -6.0, 6.0, 6.0);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        Q_UNUSED(widget);

        const bool selected = option != nullptr && (option->state & QStyle::State_Selected);
        const bool emphasize = selected || hoverActive_ || dragActive_;
        const qreal scale = dragActive_ ? 1.45 : (selected ? 1.35 : (hoverActive_ ? 1.2 : 1.0));
        const QRectF baseRect = rect();
        const QPointF center = baseRect.center();
        QRectF drawRect = baseRect;
        drawRect.setWidth(baseRect.width() * scale);
        drawRect.setHeight(baseRect.height() * scale);
        drawRect.moveCenter(center);

        QColor fill = brush().color().isValid() ? brush().color() : QColor(40, 40, 40, 160);
        fill.setAlpha(emphasize ? 240 : 175);
        QColor outline = selected ? QColor(QStringLiteral("#3ba4ff")) : pen().color();
        if (!outline.isValid()) {
            outline = QColor(24, 24, 24, 210);
        }

        painter->setRenderHint(QPainter::Antialiasing, true);
        if (emphasize) {
            QColor halo = selected
                ? QColor(72, 166, 255, 110)
                : (fill.lightnessF() > 0.6 ? QColor(24, 38, 56, 70) : QColor(255, 255, 255, 70));
            painter->setPen(Qt::NoPen);
            painter->setBrush(halo);
            painter->drawEllipse(drawRect.adjusted(-2.0, -2.0, 2.0, 2.0));
        }

        painter->setPen(QPen(outline, selected ? 1.7 : 1.0));
        painter->setBrush(fill);
        painter->drawEllipse(drawRect);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        pressSourcePoint_ = sourcePointForPreviewPos(pos());
        lastPreviewSourcePoint_ = pressSourcePoint_;
        dragActive_ = true;
        setCursor(Qt::ClosedHandCursor);
        QGraphicsEllipseItem::mousePressEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        dragActive_ = false;
        setCursor(Qt::OpenHandCursor);
        QGraphicsEllipseItem::mouseReleaseEvent(event);

        const QPointF releasedSourcePoint = sourcePointForPreviewPos(pos());
        if (moveCommittedCallback_ != nullptr && mapSourcePointsDiffer(pressSourcePoint_, releasedSourcePoint)) {
            moveCommittedCallback_(lineNumber_, geometryKind_, vertexIndex_, pressSourcePoint_, releasedSourcePoint);
            // The callback may trigger a scene rebuild that deletes this item.
            return;
        }
        update();
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = true;
        QGraphicsEllipseItem::hoverEnterEvent(event);
        update();
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
    {
        hoverActive_ = false;
        QGraphicsEllipseItem::hoverLeaveEvent(event);
        update();
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == QGraphicsItem::ItemPositionChange) {
            QPointF candidate = value.toPointF();
            candidate.setX(qBound(fittedBounds_.left(), candidate.x(), fittedBounds_.right()));
            candidate.setY(qBound(fittedBounds_.top(), candidate.y(), fittedBounds_.bottom()));
            return candidate;
        }
        if (change == QGraphicsItem::ItemPositionHasChanged && movePreviewCallback_ != nullptr) {
            const QPointF newSourcePoint = sourcePointForPreviewPos(pos());
            movePreviewCallback_(this, lastPreviewSourcePoint_, newSourcePoint, dragActive_);
            lastPreviewSourcePoint_ = newSourcePoint;
        }
        return QGraphicsEllipseItem::itemChange(change, value);
    }

private:
    QPointF sourcePointForPreviewPos(const QPointF &previewPoint) const
    {
        return mapDisplayToSource(previewToSource(previewPoint));
    }

    QPointF mapDisplayToSource(const QPointF &displayPoint) const
    {
        if (displayToSourceMapper_ != nullptr) {
            return displayToSourceMapper_(displayPoint);
        }

        return displayPoint;
    }

    QPointF previewToSource(const QPointF &previewPoint) const
    {
        return sceneCoordsPreviewToSource(previewPoint, sourceBounds_, previewBounds_);
    }

    int lineNumber_ = 0;
    QString geometryKind_;
    int vertexIndex_ = -1;
    QRectF sourceBounds_;
    QRectF previewBounds_;
    QRectF fittedBounds_;
    QPointF pressSourcePoint_;
    QPointF lastPreviewSourcePoint_;
    bool hoverActive_ = false;
    bool dragActive_ = false;
    std::function<QPointF(const QPointF &)> displayToSourceMapper_;
    std::function<void(MapEditableGeometryVertexItem *, const QPointF &, const QPointF &, bool)> movePreviewCallback_;
    std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> moveCommittedCallback_;
};

} // namespace TherionStudio
