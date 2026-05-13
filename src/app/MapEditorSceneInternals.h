#pragma once

#include <QCursor>
#include <QGraphicsEllipseItem>
#include <QGraphicsSceneMouseEvent>
#include <QPointF>
#include <QRectF>
#include <QStringList>
#include <QTransform>
#include <QVariant>
#include <QVector>
#include <functional>

namespace TherionStudio {

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
        , fittedBounds_(sceneCoordsPreviewBounds(sourceBounds, previewBounds))
    {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setCursor(Qt::OpenHandCursor);
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

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        pressSourcePoint_ = mapDisplayToSource(previewToSource(pos()));
        setCursor(Qt::ClosedHandCursor);
        QGraphicsEllipseItem::mousePressEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        setCursor(Qt::OpenHandCursor);
        QGraphicsEllipseItem::mouseReleaseEvent(event);

        const QPointF releasedSourcePoint = mapDisplayToSource(previewToSource(pos()));
        if (moveCommittedCallback_ != nullptr && pressSourcePoint_ != releasedSourcePoint) {
            moveCommittedCallback_(lineNumber_, pressSourcePoint_, releasedSourcePoint);
        }
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
        , fittedBounds_(sceneCoordsPreviewBounds(sourceBounds, previewBounds))
    {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setCursor(Qt::OpenHandCursor);
        setPos(sceneCoordsSourceToPreview(sourcePoint, sourceBounds, previewBounds));
    }

    void setMoveCommittedCallback(std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> callback)
    {
        moveCommittedCallback_ = std::move(callback);
    }

    void setDisplayToSourceMapper(std::function<QPointF(const QPointF &)> mapper)
    {
        displayToSourceMapper_ = std::move(mapper);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        pressSourcePoint_ = mapDisplayToSource(previewToSource(pos()));
        setCursor(Qt::ClosedHandCursor);
        QGraphicsEllipseItem::mousePressEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        setCursor(Qt::OpenHandCursor);
        QGraphicsEllipseItem::mouseReleaseEvent(event);

        const QPointF releasedSourcePoint = mapDisplayToSource(previewToSource(pos()));
        if (moveCommittedCallback_ != nullptr && pressSourcePoint_ != releasedSourcePoint) {
            moveCommittedCallback_(lineNumber_, geometryKind_, vertexIndex_, pressSourcePoint_, releasedSourcePoint);
        }
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
    QString geometryKind_;
    int vertexIndex_ = -1;
    QRectF sourceBounds_;
    QRectF previewBounds_;
    QRectF fittedBounds_;
    QPointF pressSourcePoint_;
    std::function<QPointF(const QPointF &)> displayToSourceMapper_;
    std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> moveCommittedCallback_;
};

} // namespace TherionStudio
