#include "MapEditorSceneSupport.h"
#include "MapEditorSceneInternals.h"

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QUndoCommand>

#include <cmath>

#include "../core/TherionDocumentParser.h"

namespace TherionStudio {

namespace {

bool parseScaleNumber(const QString &token, qreal *value)
{
    if (value == nullptr) {
        return false;
    }

    QString normalized = token.trimmed();
    while (!normalized.isEmpty()) {
        const QChar character = normalized.front();
        if (character.isDigit() || character == QLatin1Char('+') || character == QLatin1Char('-') || character == QLatin1Char('.')) {
            break;
        }
        normalized.remove(0, 1);
    }

    while (!normalized.isEmpty()) {
        const QChar character = normalized.back();
        if (character.isDigit() || character == QLatin1Char('.')) {
            break;
        }
        normalized.chop(1);
    }

    if (normalized.isEmpty()) {
        return false;
    }

    bool ok = false;
    const qreal parsedValue = normalized.toDouble(&ok);
    if (!ok) {
        return false;
    }

    *value = parsedValue;
    return true;
}

bool tokenLooksNumeric(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    int index = 0;
    if (token.at(index) == QLatin1Char('+') || token.at(index) == QLatin1Char('-')) {
        ++index;
    }

    bool sawDigit = false;
    bool sawDecimalPoint = false;
    for (; index < token.size(); ++index) {
        const QChar character = token.at(index);
        if (character.isDigit()) {
            sawDigit = true;
            continue;
        }

        if (character == QLatin1Char('.') && !sawDecimalPoint) {
            sawDecimalPoint = true;
            continue;
        }

        return false;
    }

    return sawDigit;
}

class MapCardMoveCommand final : public QUndoCommand
{
public:
    MapCardMoveCommand(MapCardItem *item, const QPointF &oldPosition, const QPointF &newPosition)
        : item_(item)
        , oldPosition_(oldPosition)
        , newPosition_(newPosition)
    {
    }

    void undo() override
    {
        if (item_ != nullptr) {
            item_->setPos(oldPosition_);
        }
    }

    void redo() override
    {
        if (item_ != nullptr) {
            item_->setPos(newPosition_);
        }
    }

private:
    MapCardItem *item_ = nullptr;
    QPointF oldPosition_;
    QPointF newPosition_;
};

class MapCardVisibilityCommand final : public QUndoCommand
{
public:
    MapCardVisibilityCommand(MapCardItem *item, bool oldVisible, bool newVisible)
        : item_(item)
        , oldVisible_(oldVisible)
        , newVisible_(newVisible)
    {
    }

    void undo() override
    {
        if (item_ != nullptr) {
            item_->setObjectVisible(oldVisible_);
        }
    }

    void redo() override
    {
        if (item_ != nullptr) {
            item_->setObjectVisible(newVisible_);
        }
    }

private:
    MapCardItem *item_ = nullptr;
    bool oldVisible_ = true;
    bool newVisible_ = true;
};

class MapDraftMoveCommand final : public QUndoCommand
{
public:
    MapDraftMoveCommand(MapDraftGeometryItem *item, const QPointF &oldPosition, const QPointF &newPosition)
        : item_(item)
        , oldPosition_(oldPosition)
        , newPosition_(newPosition)
    {
    }

    void undo() override
    {
        if (item_ != nullptr) {
            item_->setPos(oldPosition_);
        }
    }

    void redo() override
    {
        if (item_ != nullptr) {
            item_->setPos(newPosition_);
        }
    }

private:
    MapDraftGeometryItem *item_ = nullptr;
    QPointF oldPosition_;
    QPointF newPosition_;
};

class MapDraftVisibilityCommand final : public QUndoCommand
{
public:
    MapDraftVisibilityCommand(MapDraftGeometryItem *item, bool oldVisible, bool newVisible)
        : item_(item)
        , oldVisible_(oldVisible)
        , newVisible_(newVisible)
    {
    }

    void undo() override
    {
        if (item_ != nullptr) {
            item_->setObjectVisible(oldVisible_);
        }
    }

    void redo() override
    {
        if (item_ != nullptr) {
            item_->setObjectVisible(newVisible_);
        }
    }

private:
    MapDraftGeometryItem *item_ = nullptr;
    bool oldVisible_ = true;
    bool newVisible_ = true;
};

class MapDraftCompleteCommand final : public QUndoCommand
{
public:
    MapDraftCompleteCommand(MapDraftGeometryItem *item, bool oldComplete, bool newComplete)
        : item_(item)
        , oldComplete_(oldComplete)
        , newComplete_(newComplete)
    {
    }

    void undo() override
    {
        if (item_ != nullptr) {
            item_->setDraftComplete(oldComplete_);
        }
    }

    void redo() override
    {
        if (item_ != nullptr) {
            item_->setDraftComplete(newComplete_);
        }
    }

private:
    MapDraftGeometryItem *item_ = nullptr;
    bool oldComplete_ = false;
    bool newComplete_ = false;
};

class MapDraftAddCommand final : public QUndoCommand
{
public:
    MapDraftAddCommand(QGraphicsScene *scene, QVector<QGraphicsRectItem *> *items, MapDraftGeometryItem *item, const QPointF &position)
        : scene_(scene)
        , items_(items)
        , item_(item)
        , position_(position)
    {
    }

    void undo() override
    {
        if (scene_ != nullptr && item_ != nullptr) {
            scene_->removeItem(item_);
        }
        if (items_ != nullptr && item_ != nullptr) {
            items_->removeAll(item_);
        }
    }

    void redo() override
    {
        if (scene_ != nullptr && item_ != nullptr) {
            if (!scene_->items().contains(item_)) {
                scene_->addItem(item_);
            }
            item_->setPos(position_);
            item_->setZValue(20.0);
            item_->setSelected(true);
        }
        if (items_ != nullptr && item_ != nullptr && !items_->contains(item_)) {
            items_->append(item_);
        }
    }

private:
    QGraphicsScene *scene_ = nullptr;
    QVector<QGraphicsRectItem *> *items_ = nullptr;
    MapDraftGeometryItem *item_ = nullptr;
    QPointF position_;
};

QColor draftGeometryAccent(DraftGeometryKind kind)
{
    switch (kind) {
    case DraftGeometryKind::Point:
        return QColor(QStringLiteral("#72d7ff"));
    case DraftGeometryKind::Line:
        return QColor(QStringLiteral("#ffb15a"));
    case DraftGeometryKind::Area:
        return QColor(QStringLiteral("#ff7f8f"));
    }

    return QColor(QStringLiteral("#72d7ff"));
}

QRectF draftGeometryBounds(DraftGeometryKind kind)
{
    switch (kind) {
    case DraftGeometryKind::Point:
        return QRectF(0, 0, 220, 120);
    case DraftGeometryKind::Line:
        return QRectF(0, 0, 260, 120);
    case DraftGeometryKind::Area:
        return QRectF(0, 0, 280, 140);
    }

    return QRectF(0, 0, 220, 120);
}

} // namespace

QRectF sceneCoordsPreviewBounds(const QRectF &sourceBounds, const QRectF &targetBounds)
{
    if (!sourceBounds.isValid() || !targetBounds.isValid()) {
        return targetBounds;
    }

    const qreal sourceWidth = qMax(1.0, sourceBounds.width());
    const qreal sourceHeight = qMax(1.0, sourceBounds.height());
    const qreal targetWidth = qMax(1.0, targetBounds.width());
    const qreal targetHeight = qMax(1.0, targetBounds.height());
    const qreal scale = qMin(targetWidth / sourceWidth, targetHeight / sourceHeight);
    const qreal fittedWidth = sourceWidth * scale;
    const qreal fittedHeight = sourceHeight * scale;
    const qreal left = targetBounds.left() + ((targetWidth - fittedWidth) / 2.0);
    const qreal top = targetBounds.top() + ((targetHeight - fittedHeight) / 2.0);
    return QRectF(left, top, fittedWidth, fittedHeight);
}

QPointF sceneCoordsSourceToPreview(const QPointF &source, const QRectF &sourceBounds, const QRectF &previewBounds)
{
    const QRectF fitted = sceneCoordsPreviewBounds(sourceBounds, previewBounds);
    const qreal zoom = qMin(fitted.width() / qMax(1.0, sourceBounds.width()),
                            fitted.height() / qMax(1.0, sourceBounds.height()));
    const qreal panX = fitted.left() - (sourceBounds.left() * zoom);
    const qreal panY = fitted.top() + (sourceBounds.bottom() * zoom);
    return QPointF((source.x() * zoom) + panX,
                   panY - (source.y() * zoom));
}

QPointF sceneCoordsPreviewToSource(const QPointF &preview, const QRectF &sourceBounds, const QRectF &previewBounds)
{
    const QRectF fitted = sceneCoordsPreviewBounds(sourceBounds, previewBounds);
    const qreal zoom = qMin(fitted.width() / qMax(1.0, sourceBounds.width()),
                            fitted.height() / qMax(1.0, sourceBounds.height()));
    const qreal panX = fitted.left() - (sourceBounds.left() * zoom);
    const qreal panY = fitted.top() + (sourceBounds.bottom() * zoom);
    const qreal clampedX = qBound(fitted.left(), preview.x(), fitted.right());
    const qreal clampedY = qBound(fitted.top(), preview.y(), fitted.bottom());
    return QPointF((clampedX - panX) / qMax(1e-6, zoom),
                   (panY - clampedY) / qMax(1e-6, zoom));
}

qreal sceneCoordsScaleFactor(const QRectF &sourceBounds, const QRectF &previewBounds)
{
    if (!sourceBounds.isValid() || !previewBounds.isValid()) {
        return 1.0;
    }

    const qreal sourceWidth = qMax(1.0, sourceBounds.width());
    const qreal sourceHeight = qMax(1.0, sourceBounds.height());
    const qreal targetWidth = qMax(1.0, previewBounds.width());
    const qreal targetHeight = qMax(1.0, previewBounds.height());
    return qMin(targetWidth / sourceWidth, targetHeight / sourceHeight);
}

CoordinateTransform coordinateTransformFromScrapScale(const QStringList &tokens)
{
    CoordinateTransform transform;

    const int scaleOptionIndex = tokens.indexOf(QStringLiteral("-scale"));
    if (scaleOptionIndex < 0) {
        return transform;
    }

    QVector<qreal> numbers;
    numbers.reserve(8);
    for (int index = scaleOptionIndex + 1; index < tokens.size(); ++index) {
        qreal parsedValue = 0.0;
        if (!parseScaleNumber(tokens.at(index), &parsedValue)) {
            continue;
        }
        numbers.append(parsedValue);
        if (numbers.size() >= 8) {
            break;
        }
    }

    if (numbers.size() < 8) {
        return transform;
    }

    const QPointF sourcePoint1(numbers.at(0), numbers.at(1));
    const QPointF sourcePoint2(numbers.at(2), numbers.at(3));
    const QPointF mapPoint1(numbers.at(4), numbers.at(5));
    const QPointF mapPoint2(numbers.at(6), numbers.at(7));

    const QPointF sourceVector = sourcePoint2 - sourcePoint1;
    const QPointF mapVector = mapPoint2 - mapPoint1;
    const qreal sourceLength = std::hypot(sourceVector.x(), sourceVector.y());
    const qreal mapLength = std::hypot(mapVector.x(), mapVector.y());
    if (sourceLength < 1e-6 || mapLength < 1e-6) {
        return transform;
    }

    const qreal scale = mapLength / sourceLength;
    const qreal cosTheta = ((sourceVector.x() * mapVector.x()) + (sourceVector.y() * mapVector.y())) / (sourceLength * mapLength);
    const qreal sinTheta = ((sourceVector.x() * mapVector.y()) - (sourceVector.y() * mapVector.x())) / (sourceLength * mapLength);
    const qreal a = scale * cosTheta;
    const qreal b = scale * sinTheta;
    const qreal c = -scale * sinTheta;
    const qreal d = scale * cosTheta;
    const qreal tx = mapPoint1.x() - (a * sourcePoint1.x()) - (c * sourcePoint1.y());
    const qreal ty = mapPoint1.y() - (b * sourcePoint1.x()) - (d * sourcePoint1.y());

    const QTransform sourceToMap(a, b, c, d, tx, ty);
    bool invertible = false;
    const QTransform mapToSource = sourceToMap.inverted(&invertible);
    if (!invertible) {
        return transform;
    }

    transform.valid = true;
    transform.sourceToMap = sourceToMap;
    transform.mapToSource = mapToSource;
    return transform;
}

QVector<QPointF> pointsFromTokens(const QStringList &tokens)
{
    QVector<QPointF> points;
    QVector<qreal> numericValues;
    numericValues.reserve(tokens.size());

    for (const QString &token : tokens) {
        if (!tokenLooksNumeric(token)) {
            continue;
        }

        bool ok = false;
        const qreal numericValue = token.toDouble(&ok);
        if (ok) {
            numericValues.append(numericValue);
        }
    }

    for (int index = 0; index + 1 < numericValues.size(); index += 2) {
        points.append(QPointF(numericValues.at(index), numericValues.at(index + 1)));
    }

    return points;
}

MapCardItem::MapCardItem(const MapSceneEntry &entry, const QColor &accent)
    : lineNumber_(entry.lineNumber)
    , category_(entry.category)
    , title_(entry.title)
    , subtitle_(entry.subtitle)
    , accent_(accent)
{
    setRect(QRectF(0, 0, 260, 100));
    setPen(Qt::NoPen);
    setBrush(Qt::NoBrush);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setCursor(Qt::PointingHandCursor);
}

int MapCardItem::lineNumber() const
{
    return lineNumber_;
}

QString MapCardItem::category() const
{
    return category_;
}

bool MapCardItem::isObjectVisible() const
{
    return objectVisible_;
}

void MapCardItem::setObjectVisible(bool visible)
{
    if (objectVisible_ == visible) {
        return;
    }

    objectVisible_ = visible;
    setOpacity(objectVisible_ ? 1.0 : 0.45);
    update();
}

void MapCardItem::setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback)
{
    moveCommittedCallback_ = std::move(callback);
}

void MapCardItem::setVisibilityCommittedCallback(std::function<void(int, bool, bool)> callback)
{
    visibilityCommittedCallback_ = std::move(callback);
}

void MapCardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    const bool selected = option->state & QStyle::State_Selected;
    const QColor backgroundColor = selected ? accent_.lighter(170) : QColor(QStringLiteral("#2a2f3a"));
    const QColor borderColor = selected ? accent_.lighter(120) : accent_.darker(160);
    const QColor titleColor = selected ? QColor(QStringLiteral("#f8fbff")) : QColor(QStringLiteral("#e8edf4"));
    const QColor bodyColor = selected ? QColor(QStringLiteral("#f0f4fb")) : QColor(QStringLiteral("#bac5d3"));
    const QColor badgeColor = selected ? accent_.lighter(150) : accent_;
    const QColor visibilityColor = objectVisible_ ? QColor(QStringLiteral("#6ed8a8")) : QColor(QStringLiteral("#f08a7f"));

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(borderColor, selected ? 2.5 : 1.4));
    painter->setBrush(backgroundColor);
    painter->drawRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);

    painter->setPen(Qt::NoPen);
    painter->setBrush(badgeColor);
    painter->drawRoundedRect(QRectF(rect().left() + 12, rect().top() + 12, 72, 22), 8, 8);

    QFont badgeFont = painter->font();
    badgeFont.setPointSize(badgeFont.pointSize() - 1);
    badgeFont.setBold(true);
    painter->setFont(badgeFont);
    painter->setPen(QColor(QStringLiteral("#111318")));
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 12, 72, 22), Qt::AlignCenter, category_);

    painter->setBrush(visibilityColor);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QRectF(rect().right() - 26, rect().top() + 16, 12, 12));
    QFont visibilityFont = painter->font();
    visibilityFont.setPointSize(visibilityFont.pointSize() - 4);
    visibilityFont.setBold(true);
    painter->setFont(visibilityFont);
    painter->setPen(QColor(QStringLiteral("#dfe6ef")));
    painter->drawText(QRectF(rect().right() - 68, rect().top() + 8, 34, 20), Qt::AlignRight | Qt::AlignVCenter, objectVisible_ ? QObject::tr("On") : QObject::tr("Off"));

    QFont titleFont = painter->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->setPen(titleColor);
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 42, rect().width() - 72, 28), Qt::AlignLeft | Qt::AlignVCenter, title_);

    QFont subtitleFont = painter->font();
    subtitleFont.setPointSize(subtitleFont.pointSize() - 1);
    subtitleFont.setBold(false);
    painter->setFont(subtitleFont);
    painter->setPen(bodyColor);
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 68, rect().width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, subtitle_);

    QFont lineFont = painter->font();
    lineFont.setPointSize(lineFont.pointSize() - 2);
    painter->setFont(lineFont);
    painter->setPen(QColor(QStringLiteral("#9ca7b6")));
    painter->drawText(QRectF(rect().right() - 84, rect().top() + 12, 72, 22), Qt::AlignRight | Qt::AlignVCenter, QObject::tr("L%1").arg(lineNumber_));
}

void MapCardItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && visibilityHotspot().contains(event->pos())) {
        const bool previousVisible = objectVisible_;
        setObjectVisible(!objectVisible_);
        if (visibilityCommittedCallback_) {
            visibilityCommittedCallback_(lineNumber_, previousVisible, objectVisible_);
        }
        event->accept();
        return;
    }

    pressPosition_ = pos();
    QGraphicsRectItem::mousePressEvent(event);
}

void MapCardItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsRectItem::mouseReleaseEvent(event);

    if (moveCommittedCallback_ && pressPosition_ != pos()) {
        moveCommittedCallback_(lineNumber_, pressPosition_, pos());
    }
}

QVariant MapCardItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemPositionChange && scene() != nullptr) {
        QPointF point = value.toPointF();
        point.setX(qBound(56.0, point.x(), scene()->sceneRect().right() - rect().width() - 56.0));
        point.setY(qMax(point.y(), 132.0));
        return point;
    }

    return QGraphicsRectItem::itemChange(change, value);
}

QRectF MapCardItem::visibilityHotspot() const
{
    return QRectF(rect().right() - 74, rect().top() + 8, 56, 24);
}

QString draftGeometryLabel(DraftGeometryKind kind)
{
    switch (kind) {
    case DraftGeometryKind::Point:
        return QObject::tr("Point");
    case DraftGeometryKind::Line:
        return QObject::tr("Line");
    case DraftGeometryKind::Area:
        return QObject::tr("Area");
    }

    return QObject::tr("Point");
}

MapDraftGeometryItem::MapDraftGeometryItem(int draftId, DraftGeometryKind kind)
    : draftId_(draftId)
    , kind_(kind)
    , accent_(draftGeometryAccent(kind))
    , label_(draftGeometryLabel(kind))
{
    setRect(draftGeometryBounds(kind));
    setPen(Qt::NoPen);
    setBrush(Qt::NoBrush);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setCursor(Qt::PointingHandCursor);
}

int MapDraftGeometryItem::draftId() const
{
    return draftId_;
}

DraftGeometryKind MapDraftGeometryItem::kind() const
{
    return kind_;
}

bool MapDraftGeometryItem::isDraftComplete() const
{
    return draftComplete_;
}

void MapDraftGeometryItem::setDraftComplete(bool complete)
{
    if (draftComplete_ == complete) {
        return;
    }

    draftComplete_ = complete;
    update();
}

bool MapDraftGeometryItem::isObjectVisible() const
{
    return objectVisible_;
}

void MapDraftGeometryItem::setObjectVisible(bool visible)
{
    if (objectVisible_ == visible) {
        return;
    }

    objectVisible_ = visible;
    setOpacity(objectVisible_ ? 1.0 : 0.45);
    update();
}

void MapDraftGeometryItem::setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback)
{
    moveCommittedCallback_ = std::move(callback);
}

void MapDraftGeometryItem::setVisibilityCommittedCallback(std::function<void(int, bool, bool)> callback)
{
    visibilityCommittedCallback_ = std::move(callback);
}

void MapDraftGeometryItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    const bool selected = option->state & QStyle::State_Selected;
    const QColor borderColor = selected ? accent_.lighter(120) : accent_.darker(150);
    const QColor fillColor = draftComplete_ ? accent_.lighter(150) : QColor(QStringLiteral("#2b2f38"));
    const QColor titleColor = selected ? QColor(QStringLiteral("#ffffff")) : QColor(QStringLiteral("#edf3fb"));
    const QColor bodyColor = selected ? QColor(QStringLiteral("#eef5ff")) : QColor(QStringLiteral("#b9c4d3"));
    const QColor completionColor = draftComplete_ ? QColor(QStringLiteral("#6ed8a8")) : QColor(QStringLiteral("#f2c65f"));

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(borderColor, selected ? 2.2 : 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(fillColor);
    painter->drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);

    painter->setPen(Qt::NoPen);
    painter->setBrush(completionColor);
    painter->drawRoundedRect(QRectF(rect().left() + 12, rect().top() + 12, 84, 22), 8, 8);

    QFont badgeFont = painter->font();
    badgeFont.setPointSize(badgeFont.pointSize() - 1);
    badgeFont.setBold(true);
    painter->setFont(badgeFont);
    painter->setPen(QColor(QStringLiteral("#13161b")));
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 12, 84, 22), Qt::AlignCenter, draftComplete_ ? QObject::tr("Complete") : QObject::tr("Draft"));

    painter->setPen(titleColor);
    QFont titleFont = painter->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 42, rect().width() - 24, 24), Qt::AlignLeft | Qt::AlignVCenter, label_);

    painter->setPen(bodyColor);
    QFont bodyFont = painter->font();
    bodyFont.setPointSize(bodyFont.pointSize() - 1);
    bodyFont.setBold(false);
    painter->setFont(bodyFont);
    painter->drawText(QRectF(rect().left() + 12, rect().top() + 66, rect().width() - 24, 20), Qt::AlignLeft | Qt::AlignVCenter, geometryDescription());

    painter->setBrush(objectVisible_ ? QColor(QStringLiteral("#6ed8a8")) : QColor(QStringLiteral("#f08a7f")));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QRectF(rect().right() - 26, rect().top() + 16, 12, 12));
}

void MapDraftGeometryItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && visibilityHotspot().contains(event->pos())) {
        const bool previousVisible = objectVisible_;
        setObjectVisible(!objectVisible_);
        if (visibilityCommittedCallback_) {
            visibilityCommittedCallback_(draftId_, previousVisible, objectVisible_);
        }
        event->accept();
        return;
    }

    pressPosition_ = pos();
    QGraphicsRectItem::mousePressEvent(event);
}

void MapDraftGeometryItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsRectItem::mouseReleaseEvent(event);

    if (moveCommittedCallback_ && pressPosition_ != pos()) {
        moveCommittedCallback_(draftId_, pressPosition_, pos());
    }
}

QVariant MapDraftGeometryItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemPositionChange && scene() != nullptr) {
        QPointF point = value.toPointF();
        point.setX(qBound(56.0, point.x(), scene()->sceneRect().right() - rect().width() - 56.0));
        point.setY(qMax(point.y(), 132.0));
        return point;
    }

    return QGraphicsRectItem::itemChange(change, value);
}

QRectF MapDraftGeometryItem::visibilityHotspot() const
{
    return QRectF(rect().right() - 74, rect().top() + 8, 56, 24);
}

QString MapDraftGeometryItem::geometryDescription() const
{
    switch (kind_) {
    case DraftGeometryKind::Point:
        return QObject::tr("Draft point anchor");
    case DraftGeometryKind::Line:
        return QObject::tr("Draft line segment");
    case DraftGeometryKind::Area:
        return QObject::tr("Draft area outline");
    }

    return QString();
}

QRectF geometryBoundsForFeatures(const QVector<MapGeometryFeature> &features)
{
    QRectF bounds;
    bool hasBounds = false;

    auto includePoint = [&bounds, &hasBounds](const QPointF &point) {
        if (!hasBounds) {
            bounds = QRectF(point, QSizeF(1.0, 1.0));
            hasBounds = true;
            return;
        }

        bounds = bounds.united(QRectF(point, QSizeF(1.0, 1.0)));
    };

    for (const MapGeometryFeature &feature : features) {
        if (feature.hasAnchor) {
            includePoint(feature.anchor);
        }

        for (const QPointF &vertex : feature.vertices) {
            includePoint(vertex);
        }
    }

    if (!hasBounds) {
        return QRectF(0.0, 0.0, 1.0, 1.0);
    }

    return bounds;
}

QPointF mapGeometryPointToPreview(const QPointF &point, const QRectF &sourceBounds, const QRectF &targetBounds)
{
    return sceneCoordsSourceToPreview(point, sourceBounds, targetBounds);
}

QUndoCommand *createMapCardMoveCommand(MapCardItem *item, const QPointF &oldPosition, const QPointF &newPosition)
{
    return new MapCardMoveCommand(item, oldPosition, newPosition);
}

QUndoCommand *createMapCardVisibilityCommand(MapCardItem *item, bool oldVisible, bool newVisible)
{
    return new MapCardVisibilityCommand(item, oldVisible, newVisible);
}

QUndoCommand *createMapDraftMoveCommand(MapDraftGeometryItem *item, const QPointF &oldPosition, const QPointF &newPosition)
{
    return new MapDraftMoveCommand(item, oldPosition, newPosition);
}

QUndoCommand *createMapDraftVisibilityCommand(MapDraftGeometryItem *item, bool oldVisible, bool newVisible)
{
    return new MapDraftVisibilityCommand(item, oldVisible, newVisible);
}

QUndoCommand *createMapDraftCompleteCommand(MapDraftGeometryItem *item, bool oldComplete, bool newComplete)
{
    return new MapDraftCompleteCommand(item, oldComplete, newComplete);
}

QUndoCommand *createMapDraftAddCommand(QGraphicsScene *scene,
                                       QVector<QGraphicsRectItem *> *items,
                                       MapDraftGeometryItem *item,
                                       const QPointF &position)
{
    return new MapDraftAddCommand(scene, items, item, position);
}

} // namespace TherionStudio
