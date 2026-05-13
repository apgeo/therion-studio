#include "MapEditorTab.h"

#include <QFileInfo>
#include <QFrame>
#include <QComboBox>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsSceneEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsRectItem>
#include <QGraphicsPathItem>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QFont>
#include <QBrush>
#include <QColor>
#include <QPen>
#include <QRectF>
#include <QFontMetrics>
#include <QLineF>
#include <QTextBrowser>
#include <QToolButton>
#include <functional>

#include "TextEditorTab.h"
#include "../core/SessionStore.h"
#include "../core/TherionDocumentParser.h"
#include "../editor/TherionSyntaxHighlighter.h"

namespace TherionStudio
{
enum class DraftGeometryKind
{
    Point,
    Line,
    Area
};

namespace
{
struct MapSceneEntry
{
    int lineNumber = 0;
    QString category;
    QString title;
    QString subtitle;
};

struct MapGeometryFeature
{
    enum class Kind
    {
        Point,
        Line,
        Area
    };

    Kind kind = Kind::Point;
    int lineNumber = 0;
    QString category;
    QString label;
    QString subtitle;
    QColor accent;
    QVector<QPointF> vertices;
    QPointF anchor;
    bool hasAnchor = false;
    bool closed = false;
    bool stationPoint = false;
};

class MapCardItem final : public QGraphicsRectItem
{
public:
    MapCardItem(const MapSceneEntry &entry, const QColor &accent)
        : lineNumber_(entry.lineNumber)
        , category_(entry.category)
        , title_(entry.title)
        , subtitle_(entry.subtitle)
        , accent_(accent)
    {
        setRect(QRectF(0, 0, 360, 96));
        setPen(Qt::NoPen);
        setBrush(Qt::NoBrush);
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
        setCursor(Qt::PointingHandCursor);
    }

    void setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback)
    {
        moveCommittedCallback_ = std::move(callback);
    }

    void setVisibilityCommittedCallback(std::function<void(int, bool, bool)> callback)
    {
        visibilityCommittedCallback_ = std::move(callback);
    }

    int lineNumber() const
    {
        return lineNumber_;
    }

    QString category() const
    {
        return category_;
    }

    bool isObjectVisible() const
    {
        return objectVisible_;
    }

    void setObjectVisible(bool visible)
    {
        if (objectVisible_ == visible) {
            return;
        }

        objectVisible_ = visible;
        setOpacity(objectVisible_ ? 1.0 : 0.45);
        update();
    }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
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

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
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

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        QGraphicsRectItem::mouseReleaseEvent(event);

        if (moveCommittedCallback_ && pressPosition_ != pos()) {
            moveCommittedCallback_(lineNumber_, pressPosition_, pos());
        }
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == QGraphicsItem::ItemPositionChange && scene() != nullptr) {
            QPointF point = value.toPointF();
            point.setX(qBound(56.0, point.x(), scene()->sceneRect().right() - rect().width() - 56.0));
            point.setY(qMax(point.y(), 132.0));
            return point;
        }

        return QGraphicsRectItem::itemChange(change, value);
    }

private:
    QRectF visibilityHotspot() const
    {
        return QRectF(rect().right() - 74, rect().top() + 8, 56, 24);
    }

    int lineNumber_ = 0;
    QString category_;
    QString title_;
    QString subtitle_;
    QColor accent_;
    bool objectVisible_ = true;
    QPointF pressPosition_;
    std::function<void(int, const QPointF &, const QPointF &)> moveCommittedCallback_;
    std::function<void(int, bool, bool)> visibilityCommittedCallback_;
};

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

class MapDraftGeometryItem final : public QGraphicsRectItem
{
public:
    MapDraftGeometryItem(int draftId, DraftGeometryKind kind)
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

    int draftId() const
    {
        return draftId_;
    }

    DraftGeometryKind kind() const
    {
        return kind_;
    }

    bool isDraftComplete() const
    {
        return draftComplete_;
    }

    void setDraftComplete(bool complete)
    {
        if (draftComplete_ == complete) {
            return;
        }

        draftComplete_ = complete;
        update();
    }

    bool isObjectVisible() const
    {
        return objectVisible_;
    }

    void setObjectVisible(bool visible)
    {
        if (objectVisible_ == visible) {
            return;
        }

        objectVisible_ = visible;
        setOpacity(objectVisible_ ? 1.0 : 0.45);
        update();
    }

    void setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback)
    {
        moveCommittedCallback_ = std::move(callback);
    }

    void setVisibilityCommittedCallback(std::function<void(int, bool, bool)> callback)
    {
        visibilityCommittedCallback_ = std::move(callback);
    }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
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

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
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

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        QGraphicsRectItem::mouseReleaseEvent(event);

        if (moveCommittedCallback_ && pressPosition_ != pos()) {
            moveCommittedCallback_(draftId_, pressPosition_, pos());
        }
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == QGraphicsItem::ItemPositionChange && scene() != nullptr) {
            QPointF point = value.toPointF();
            point.setX(qBound(56.0, point.x(), scene()->sceneRect().right() - rect().width() - 56.0));
            point.setY(qMax(point.y(), 132.0));
            return point;
        }

        return QGraphicsRectItem::itemChange(change, value);
    }

private:
    QRectF visibilityHotspot() const
    {
        return QRectF(rect().right() - 74, rect().top() + 8, 56, 24);
    }

    QString geometryDescription() const
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

    int draftId_ = 0;
    DraftGeometryKind kind_ = DraftGeometryKind::Point;
    QColor accent_;
    QString label_;
    bool draftComplete_ = false;
    bool objectVisible_ = true;
    QPointF pressPosition_;
    std::function<void(int, const QPointF &, const QPointF &)> moveCommittedCallback_;
    std::function<void(int, bool, bool)> visibilityCommittedCallback_;
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

QString mapWorkspaceHelpHtml()
{
    return QStringLiteral(
        "<h3>Map Workspace</h3>"
        "<p>The map canvas renders parsed TH2 geometry and keeps the object cards available for source-level navigation.</p>"
        "<h4>Toolbar</h4>"
        "<ul>"
        "<li><strong>Point</strong>, <strong>Line</strong>, and <strong>Area</strong> create draft geometry in the scene.</li>"
        "<li><strong>Complete Draft</strong> marks the selected draft geometry as finished.</li>"
        "<li><strong>Undo</strong> and <strong>Redo</strong> follow the same command stack as source edits.</li>"
        "<li><strong>Fit</strong> recenters the geometry preview.</li>"
        "</ul>"
        "<p>Select a map card to jump to its source line. Select a draft item to move or toggle it.</p>");
}

QString workspaceModeLabel(MapEditorTab::WorkspaceMode mode)
{
    switch (mode) {
    case MapEditorTab::WorkspaceMode::TextOnly:
        return QObject::tr("Text Only");
    case MapEditorTab::WorkspaceMode::MapOnly:
        return QObject::tr("Map Only");
    case MapEditorTab::WorkspaceMode::Split:
        return QObject::tr("Split");
    }

    return QObject::tr("Split");
}

QString mapEntryCategoryForLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString directive = parsedLine.tokens.first().toLower();
    const QString secondToken = parsedLine.tokens.value(1).toLower();

    if (directive == QStringLiteral("survey")) {
        return QObject::tr("Survey");
    }
    if (directive == QStringLiteral("map")) {
        return QObject::tr("Map");
    }
    if (directive == QStringLiteral("scrap")) {
        return QObject::tr("Scrap");
    }
    if (directive == QStringLiteral("line")) {
        return QObject::tr("Line");
    }
    if (directive == QStringLiteral("area")) {
        return QObject::tr("Area");
    }
    if (directive == QStringLiteral("station")) {
        return QObject::tr("Station");
    }
    if (directive == QStringLiteral("point") && secondToken == QStringLiteral("station")) {
        return QObject::tr("Station");
    }
    if (directive == QStringLiteral("point")) {
        return QObject::tr("Point");
    }

    return QString();
}

QString mapEntryTitleForLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString directive = parsedLine.tokens.first().toLower();
    if (directive == QStringLiteral("point") && parsedLine.tokens.size() >= 3 && parsedLine.tokens.value(1).toLower() == QStringLiteral("station")) {
        return parsedLine.tokens.value(2);
    }

    if (parsedLine.tokens.size() > 1) {
        return parsedLine.tokens.value(1);
    }

    return parsedLine.tokens.first();
}

QString mapEntrySubtitleForLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.size() <= 1) {
        return parsedLine.rawText.trimmed();
    }

    QStringList remainder = parsedLine.tokens.mid(1);
    if (parsedLine.tokens.first().toLower() == QStringLiteral("point") && parsedLine.tokens.value(1).toLower() == QStringLiteral("station") && parsedLine.tokens.size() > 2) {
        remainder = parsedLine.tokens.mid(2);
    }

    return remainder.join(QStringLiteral(" "));
}

QColor mapEntryAccentForCategory(const QString &category);

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
    const qreal sourceWidth = qMax(1.0, sourceBounds.width());
    const qreal sourceHeight = qMax(1.0, sourceBounds.height());
    const qreal normalizedX = (point.x() - sourceBounds.left()) / sourceWidth;
    const qreal normalizedY = (point.y() - sourceBounds.top()) / sourceHeight;

    return QPointF(targetBounds.left() + (normalizedX * targetBounds.width()),
                   targetBounds.top() + (normalizedY * targetBounds.height()));
}

QVector<MapGeometryFeature> collectGeometryFeatures(const QVector<TherionParsedLine> &parsedLines)
{
    QVector<MapGeometryFeature> features;
    MapGeometryFeature currentFeature;
    bool inLineBlock = false;
    bool inAreaBlock = false;

    auto flushCurrentFeature = [&]() {
        if (currentFeature.kind == MapGeometryFeature::Kind::Point) {
            if (currentFeature.hasAnchor) {
                features.append(currentFeature);
            }
        } else if (currentFeature.kind == MapGeometryFeature::Kind::Line) {
            if (currentFeature.vertices.size() >= 2) {
                features.append(currentFeature);
            }
        } else if (currentFeature.kind == MapGeometryFeature::Kind::Area) {
            if (currentFeature.vertices.size() >= 3) {
                features.append(currentFeature);
            }
        }

        currentFeature = MapGeometryFeature{};
        inLineBlock = false;
        inAreaBlock = false;
    };

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive;

        if (directive == QStringLiteral("endline")) {
            if (inLineBlock) {
                flushCurrentFeature();
            }
            continue;
        }

        if (directive == QStringLiteral("endarea")) {
            if (inAreaBlock) {
                flushCurrentFeature();
            }
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("point")) {
            const QVector<QPointF> pointTokens = pointsFromTokens(parsedLine.tokens.mid(1));
            if (pointTokens.isEmpty()) {
                continue;
            }

            MapGeometryFeature feature;
            feature.kind = MapGeometryFeature::Kind::Point;
            feature.lineNumber = parsedLine.lineNumber;
            feature.category = mapEntryCategoryForLine(parsedLine);
            feature.label = mapEntryTitleForLine(parsedLine);
            feature.subtitle = mapEntrySubtitleForLine(parsedLine);
            feature.accent = mapEntryAccentForCategory(feature.category);
            feature.anchor = pointTokens.first();
            feature.hasAnchor = true;
            feature.stationPoint = false;
            features.append(feature);
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("station")) {
            const QVector<QPointF> pointTokens = pointsFromTokens(parsedLine.tokens.mid(1));
            if (pointTokens.isEmpty()) {
                continue;
            }

            MapGeometryFeature feature;
            feature.kind = MapGeometryFeature::Kind::Point;
            feature.lineNumber = parsedLine.lineNumber;
            feature.category = mapEntryCategoryForLine(parsedLine);
            feature.label = mapEntryTitleForLine(parsedLine);
            feature.subtitle = mapEntrySubtitleForLine(parsedLine);
            feature.accent = mapEntryAccentForCategory(feature.category);
            feature.anchor = pointTokens.first();
            feature.hasAnchor = true;
            feature.stationPoint = true;
            features.append(feature);
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("line")) {
            flushCurrentFeature();
            currentFeature.kind = MapGeometryFeature::Kind::Line;
            currentFeature.lineNumber = parsedLine.lineNumber;
            currentFeature.category = mapEntryCategoryForLine(parsedLine);
            currentFeature.label = mapEntryTitleForLine(parsedLine);
            currentFeature.subtitle = mapEntrySubtitleForLine(parsedLine);
            currentFeature.accent = mapEntryAccentForCategory(currentFeature.category);
            currentFeature.vertices.append(pointsFromTokens(parsedLine.tokens.mid(1)));
            inLineBlock = true;
            continue;
        }

        if (!inLineBlock && !inAreaBlock && directive == QStringLiteral("area")) {
            flushCurrentFeature();
            currentFeature.kind = MapGeometryFeature::Kind::Area;
            currentFeature.lineNumber = parsedLine.lineNumber;
            currentFeature.category = mapEntryCategoryForLine(parsedLine);
            currentFeature.label = mapEntryTitleForLine(parsedLine);
            currentFeature.subtitle = mapEntrySubtitleForLine(parsedLine);
            currentFeature.accent = mapEntryAccentForCategory(currentFeature.category);
            currentFeature.vertices.append(pointsFromTokens(parsedLine.tokens.mid(1)));
            inAreaBlock = true;
            continue;
        }

        if (inLineBlock) {
            currentFeature.vertices.append(pointsFromTokens(parsedLine.tokens));
            continue;
        }

        if (inAreaBlock) {
            currentFeature.vertices.append(pointsFromTokens(parsedLine.tokens));
            continue;
        }
    }

    if (inLineBlock || inAreaBlock) {
        flushCurrentFeature();
    }

    return features;
}

QColor mapEntryAccentForCategory(const QString &category)
{
    if (category == QObject::tr("Survey")) {
        return QColor(QStringLiteral("#5aa9ff"));
    }
    if (category == QObject::tr("Map")) {
        return QColor(QStringLiteral("#8f8bff"));
    }
    if (category == QObject::tr("Scrap")) {
        return QColor(QStringLiteral("#4dd6a8"));
    }
    if (category == QObject::tr("Line")) {
        return QColor(QStringLiteral("#ffb15a"));
    }
    if (category == QObject::tr("Area")) {
        return QColor(QStringLiteral("#ff7f8f"));
    }
    if (category == QObject::tr("Station")) {
        return QColor(QStringLiteral("#ffd86b"));
    }
    if (category == QObject::tr("Point")) {
        return QColor(QStringLiteral("#7ed0ff"));
    }

    return QColor(QStringLiteral("#7f8ca3"));
}
}

MapEditorTab::MapEditorTab(QWidget *parent)
    : QWidget(parent)
{
    undoStack_ = new QUndoStack(this);
    workspaceMode_ = workspaceModeFromSetting(SessionStore::therionMapWorkspaceMode());
    buildUi();
    setWorkspaceMode(workspaceMode_);
}

void MapEditorTab::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(12, 12, 12, 8);
    toolbarLayout->setSpacing(8);

    auto *modeLabel = new QLabel(tr("Workspace"), toolbar);

    pointButton_ = new QPushButton(tr("Point"), toolbar);
    lineButton_ = new QPushButton(tr("Line"), toolbar);
    areaButton_ = new QPushButton(tr("Area"), toolbar);
    completeDraftButton_ = new QPushButton(tr("Complete Draft"), toolbar);

    connect(pointButton_, &QPushButton::clicked, this, &MapEditorTab::handleAddPointTriggered);
    connect(lineButton_, &QPushButton::clicked, this, &MapEditorTab::handleAddLineTriggered);
    connect(areaButton_, &QPushButton::clicked, this, &MapEditorTab::handleAddAreaTriggered);
    connect(completeDraftButton_, &QPushButton::clicked, this, &MapEditorTab::handleCompleteDraftTriggered);

    undoButton_ = new QPushButton(tr("Undo"), toolbar);
    redoButton_ = new QPushButton(tr("Redo"), toolbar);
    zoomOutButton_ = new QPushButton(tr("Zoom -"), toolbar);
    zoomInButton_ = new QPushButton(tr("Zoom +"), toolbar);
    fitButton_ = new QPushButton(tr("Fit"), toolbar);
    zoomLabel_ = new QLabel(tr("100%"), toolbar);
    zoomLabel_->setMinimumWidth(52);
    zoomLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    connect(undoButton_, &QPushButton::clicked, this, &MapEditorTab::handleUndoTriggered);
    connect(redoButton_, &QPushButton::clicked, this, &MapEditorTab::handleRedoTriggered);
    connect(zoomOutButton_, &QPushButton::clicked, this, &MapEditorTab::handleZoomOutTriggered);
    connect(zoomInButton_, &QPushButton::clicked, this, &MapEditorTab::handleZoomInTriggered);
    connect(fitButton_, &QPushButton::clicked, this, &MapEditorTab::handleFitTriggered);

    workspaceModeCombo_ = new QComboBox(toolbar);
    workspaceModeCombo_->addItem(tr("Text Only"));
    workspaceModeCombo_->addItem(tr("Map Only"));
    workspaceModeCombo_->addItem(tr("Split"));
    workspaceModeCombo_->setCurrentIndex(workspaceModeToIndex(workspaceMode_));
    connect(workspaceModeCombo_, &QComboBox::currentIndexChanged, this, &MapEditorTab::handleWorkspaceModeChanged);

    detachButton_ = new QPushButton(tr("Open Dedicated Window"), toolbar);
    detachButton_->setEnabled(false);

    summaryLabel_ = new QLabel(tr("TH2 map workspace scaffolding"), toolbar);
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    toolbarLayout->addWidget(modeLabel);
    toolbarLayout->addWidget(workspaceModeCombo_);
    toolbarLayout->addWidget(pointButton_);
    toolbarLayout->addWidget(lineButton_);
    toolbarLayout->addWidget(areaButton_);
    toolbarLayout->addWidget(completeDraftButton_);
    toolbarLayout->addWidget(undoButton_);
    toolbarLayout->addWidget(redoButton_);
    toolbarLayout->addWidget(zoomOutButton_);
    toolbarLayout->addWidget(zoomInButton_);
    toolbarLayout->addWidget(fitButton_);
    toolbarLayout->addWidget(zoomLabel_);
    toolbarLayout->addWidget(detachButton_);
    toolbarLayout->addWidget(summaryLabel_, 1);

    layout->addWidget(toolbar);

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(6);

    textEditor_ = new TextEditorTab(splitter_);
    connect(textEditor_, &TextEditorTab::titleChanged, this, &MapEditorTab::titleChanged);
    connect(textEditor_, &TextEditorTab::dirtyStateChanged, this, &MapEditorTab::dirtyStateChanged);
    connect(textEditor_, &TextEditorTab::currentLineChanged, this, &MapEditorTab::handleTextEditorCurrentLineChanged);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::refreshMapScene);
    connect(textEditor_, &TextEditorTab::documentTextChanged, this, &MapEditorTab::documentTextChanged);

    connect(undoStack_, &QUndoStack::canUndoChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(undoStack_, &QUndoStack::canRedoChanged, this, &MapEditorTab::updateCommandSurfaceState);
    connect(undoStack_, &QUndoStack::indexChanged, this, &MapEditorTab::updateCommandSurfaceState);

    mapView_ = new QGraphicsView(splitter_);
    mapView_->setFrameShape(QFrame::NoFrame);
    mapView_->setDragMode(QGraphicsView::NoDrag);
    mapView_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    mapView_->setRenderHint(QPainter::Antialiasing, true);
    mapView_->setBackgroundBrush(QColor(QStringLiteral("#1e1f24")));

    buildMapScene();

    splitter_->addWidget(textEditor_);
    splitter_->addWidget(mapView_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 1);

    layout->addWidget(splitter_, 1);

    buildHelpPanel();
    layout->addWidget(helpPanel_);

    statusLabel_ = new QLabel(tr("Open a TH2 file to begin."), this);
    statusLabel_->setMargin(8);
    statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(statusLabel_);

    refreshMapScene();
    updateCommandSurfaceState();
}

void MapEditorTab::buildMapScene()
{
    mapScene_ = new QGraphicsScene(this);
    mapView_->setScene(mapScene_);
    connect(mapScene_, &QGraphicsScene::selectionChanged, this, &MapEditorTab::handleMapSceneSelectionChanged);
}

void MapEditorTab::buildHelpPanel()
{
    helpPanel_ = new QFrame(this);
    helpPanel_->setObjectName(QStringLiteral("MapHelpPanel"));
    helpPanel_->setFrameShape(QFrame::StyledPanel);

    auto *panelLayout = new QVBoxLayout(helpPanel_);
    panelLayout->setContentsMargins(10, 8, 10, 10);
    panelLayout->setSpacing(6);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);

    auto *headerLabel = new QLabel(tr("Map Help"), helpPanel_);
    QFont headerFont = headerLabel->font();
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);

    helpToggleButton_ = new QToolButton(helpPanel_);
    helpToggleButton_->setCheckable(true);
    helpToggleButton_->setChecked(true);
    helpToggleButton_->setText(tr("Hide"));
    helpToggleButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    helpToggleButton_->setAutoRaise(true);
    connect(helpToggleButton_, &QToolButton::toggled, this, [this](bool checked) {
        setHelpCollapsed(!checked);
    });

    headerRow->addWidget(headerLabel);
    headerRow->addStretch(1);
    headerRow->addWidget(helpToggleButton_);

    auto *subtitleLabel = new QLabel(tr("Context-sensitive guidance for the selected map object or draft geometry."), helpPanel_);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setObjectName(QStringLiteral("MapHelpSubtitle"));

    auto *divider = new QFrame(helpPanel_);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);

    helpBrowser_ = new QTextBrowser(helpPanel_);
    helpBrowser_->setObjectName(QStringLiteral("MapHelpBrowser"));
    helpBrowser_->setFrameShape(QFrame::NoFrame);
    helpBrowser_->setOpenLinks(false);
    helpBrowser_->setOpenExternalLinks(false);
    helpBrowser_->document()->setDocumentMargin(0);
    helpBrowser_->setMinimumHeight(112);
    
#if defined(Q_OS_MACOS)
    helpBrowser_->setFrameShape(QFrame::StyledPanel);
#else
    helpBrowser_->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
#endif
    helpBrowser_->setHtml(mapWorkspaceHelpHtml());

#if !defined(Q_OS_MACOS)
    helpPanel_->setStyleSheet(QStringLiteral(
        "QFrame#MapHelpPanel {"
        " background: #f5f7fa;"
        " border-top: 1px solid #d5dbe4;"
        " }"
        "QFrame#MapHelpPanel QLabel#MapHelpSubtitle {"
        " color: #667080;"
        " }"));
#endif

    panelLayout->addLayout(headerRow);
    panelLayout->addWidget(subtitleLabel);
    panelLayout->addWidget(divider);
    panelLayout->addWidget(helpBrowser_, 1);
}

void MapEditorTab::refreshMapScene()
{
    if (mapScene_ == nullptr) {
        return;
    }

    if (undoStack_ != nullptr) {
        undoStack_->clear();
    }

    clearMapScene();

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(textEditor_->text());
    QVector<MapSceneEntry> entries;
    entries.reserve(parsedLines.size());

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString category = mapEntryCategoryForLine(parsedLine);
        if (category.isEmpty()) {
            continue;
        }

        MapSceneEntry entry;
        entry.lineNumber = parsedLine.lineNumber;
        entry.category = category;
        entry.title = mapEntryTitleForLine(parsedLine);
        entry.subtitle = mapEntrySubtitleForLine(parsedLine);
        entries.append(entry);
    }

    const QVector<MapGeometryFeature> geometryFeatures = collectGeometryFeatures(parsedLines);
    const QRectF sceneFrame(0, 0, 980, qMax(760, 424 + ((entries.size() + 1) / 2) * 124));
    mapScene_->setSceneRect(sceneFrame);
    mapScene_->addRect(QRectF(32, 32, sceneFrame.width() - 64, sceneFrame.height() - 64), QPen(QColor(QStringLiteral("#596477")), 1.5), QBrush(QColor(QStringLiteral("#232833"))));

    QFont headerFont(QStringLiteral("Georgia"), 18, QFont::Bold);
    auto *titleItem = mapScene_->addText(tr("TH2 Map Workspace"), headerFont);
    titleItem->setDefaultTextColor(QColor(QStringLiteral("#e2e9f3")));
    titleItem->setPos(56, 48);

    auto *summaryItem = mapScene_->addText(tr("%1 object card(s) plus %2 geometry feature(s) rendered from the current TH2 source.").arg(entries.size()).arg(geometryFeatures.size()), QFont(QStringLiteral("Menlo"), 11));
    summaryItem->setDefaultTextColor(QColor(QStringLiteral("#b4bfd0")));
    summaryItem->setPos(56, 84);

    const QRectF geometryCanvas(56.0, 132.0, sceneFrame.width() - 112.0, 260.0);
    mapScene_->addRect(geometryCanvas, QPen(QColor(QStringLiteral("#d8dde5")), 1.2), QBrush(QColor(QStringLiteral("#ffffff"))));

    auto *geometryTitle = mapScene_->addText(tr("Rendered TH2 geometry preview"), QFont(QStringLiteral("Menlo"), 12, QFont::Bold));
    geometryTitle->setDefaultTextColor(QColor(QStringLiteral("#111418")));
    geometryTitle->setPos(geometryCanvas.left() + 16.0, geometryCanvas.top() + 12.0);

    auto *geometrySubtitle = mapScene_->addText(tr("Points, lines, and areas are drawn from parsed source geometry when available."), QFont(QStringLiteral("Menlo"), 10));
    geometrySubtitle->setDefaultTextColor(QColor(QStringLiteral("#7b8392")));
    geometrySubtitle->setPos(geometryCanvas.left() + 16.0, geometryCanvas.top() + 36.0);

    const QRectF previewBounds = geometryCanvas.adjusted(24.0, 64.0, -24.0, -24.0);
    const QRectF sourceBounds = geometryBoundsForFeatures(geometryFeatures);

    for (int index = 0; index < 6; ++index) {
        const qreal y = previewBounds.top() + (index * previewBounds.height() / 5.0);
        mapScene_->addLine(previewBounds.left(), y, previewBounds.right(), y, QPen(QColor(QStringLiteral("#edf0f4")), 1.0, Qt::SolidLine));
    }
    for (int index = 0; index < 8; ++index) {
        const qreal x = previewBounds.left() + (index * previewBounds.width() / 7.0);
        mapScene_->addLine(x, previewBounds.top(), x, previewBounds.bottom(), QPen(QColor(QStringLiteral("#edf0f4")), 1.0, Qt::SolidLine));
    }

    if (geometryFeatures.isEmpty()) {
        auto *emptyGeometryItem = mapScene_->addText(tr("No parseable point, line, or area geometry was found in this document yet."), QFont(QStringLiteral("Menlo"), 11));
        emptyGeometryItem->setDefaultTextColor(QColor(QStringLiteral("#92a1b4")));
        emptyGeometryItem->setPos(previewBounds.left() + 16.0, previewBounds.top() + 16.0);
    } else {
        for (const MapGeometryFeature &feature : geometryFeatures) {
            switch (feature.kind) {
            case MapGeometryFeature::Kind::Point: {
                if (!feature.hasAnchor) {
                    break;
                }

                const QPointF previewPoint = mapGeometryPointToPreview(feature.anchor, sourceBounds, previewBounds);
                auto *pointItem = mapScene_->addEllipse(QRectF(previewPoint.x() - 5.0, previewPoint.y() - 5.0, 10.0, 10.0), QPen(QColor(QStringLiteral("#101010")), 1.0), QBrush(QColor(QStringLiteral("#101010"))));
                pointItem->setZValue(3.0);

                if (feature.stationPoint) {
                    QPainterPath markerPath;
                    markerPath.moveTo(previewPoint + QPointF(0.0, -12.0));
                    markerPath.lineTo(previewPoint + QPointF(10.0, 6.0));
                    markerPath.lineTo(previewPoint + QPointF(-10.0, 6.0));
                    markerPath.closeSubpath();

                    auto *triangle = mapScene_->addPath(markerPath, QPen(QColor(QStringLiteral("#ff4f3d")), 1.2), QBrush(QColor(QStringLiteral("#ff4f3d"))));
                    triangle->setZValue(3.5);
                }

                auto *label = mapScene_->addText(feature.label.isEmpty() ? feature.category : feature.label, QFont(QStringLiteral("Menlo"), 10, QFont::Bold));
                label->setDefaultTextColor(QColor(QStringLiteral("#9d9d9d")));
                label->setPos(previewPoint + QPointF(10.0, -18.0));
                label->setZValue(4.0);
                break;
            }
            case MapGeometryFeature::Kind::Line: {
                if (feature.vertices.size() < 2) {
                    break;
                }

                QPainterPath path;
                const QPointF firstPoint = mapGeometryPointToPreview(feature.vertices.first(), sourceBounds, previewBounds);
                path.moveTo(firstPoint);
                for (int vertexIndex = 1; vertexIndex < feature.vertices.size(); ++vertexIndex) {
                    path.lineTo(mapGeometryPointToPreview(feature.vertices.at(vertexIndex), sourceBounds, previewBounds));
                }

                auto *lineItem = mapScene_->addPath(path, QPen(QColor(QStringLiteral("#222222")), 4.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                lineItem->setZValue(2.5);
                auto *detailItem = mapScene_->addPath(path, QPen(QColor(QStringLiteral("#2b2b2b")), 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                detailItem->setZValue(3.0);

                for (const QPointF &vertex : feature.vertices) {
                    const QPointF previewPoint = mapGeometryPointToPreview(vertex, sourceBounds, previewBounds);
                    auto *vertexItem = mapScene_->addEllipse(QRectF(previewPoint.x() - 3.0, previewPoint.y() - 3.0, 6.0, 6.0), QPen(QColor(QStringLiteral("#101010")), 0.8), QBrush(QColor(QStringLiteral("#101010"))));
                    vertexItem->setZValue(4.0);
                }

                if (feature.stationPoint) {
                    const QPointF headPoint = mapGeometryPointToPreview(feature.vertices.first(), sourceBounds, previewBounds);
                    QPainterPath markerPath;
                    markerPath.moveTo(headPoint + QPointF(0.0, -12.0));
                    markerPath.lineTo(headPoint + QPointF(10.0, 6.0));
                    markerPath.lineTo(headPoint + QPointF(-10.0, 6.0));
                    markerPath.closeSubpath();

                    auto *triangle = mapScene_->addPath(markerPath, QPen(QColor(QStringLiteral("#ff4f3d")), 1.2), QBrush(QColor(QStringLiteral("#ff4f3d"))));
                    triangle->setZValue(3.5);
                }

                auto *label = mapScene_->addText(feature.label.isEmpty() ? feature.category : feature.label, QFont(QStringLiteral("Menlo"), 10, QFont::Bold));
                label->setDefaultTextColor(QColor(QStringLiteral("#9d9d9d")));
                label->setPos(mapGeometryPointToPreview(feature.vertices.first(), sourceBounds, previewBounds) + QPointF(10.0, -18.0));
                label->setZValue(4.0);
                break;
            }
            case MapGeometryFeature::Kind::Area: {
                if (feature.vertices.size() < 3) {
                    break;
                }

                QPainterPath path;
                const QPointF firstPoint = mapGeometryPointToPreview(feature.vertices.first(), sourceBounds, previewBounds);
                path.moveTo(firstPoint);
                for (int vertexIndex = 1; vertexIndex < feature.vertices.size(); ++vertexIndex) {
                    path.lineTo(mapGeometryPointToPreview(feature.vertices.at(vertexIndex), sourceBounds, previewBounds));
                }
                path.closeSubpath();

                auto *fillItem = mapScene_->addPath(path, QPen(QColor(QStringLiteral("#222222")), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin), QBrush(QColor(0, 0, 0, 18)));
                fillItem->setZValue(2.0);

                auto *label = mapScene_->addText(feature.label.isEmpty() ? feature.category : feature.label, QFont(QStringLiteral("Menlo"), 10, QFont::Bold));
                label->setDefaultTextColor(QColor(QStringLiteral("#9d9d9d")));
                label->setPos(mapGeometryPointToPreview(feature.vertices.first(), sourceBounds, previewBounds) + QPointF(10.0, -18.0));
                label->setZValue(4.0);
                break;
            }
            }
        }
    }

    const qreal cardsStartY = geometryCanvas.bottom() + 36.0;

    if (entries.isEmpty()) {
        auto *emptyItem = mapScene_->addText(tr("No Therion map objects were detected in this document."), QFont(QStringLiteral("Menlo"), 12));
        emptyItem->setDefaultTextColor(QColor(QStringLiteral("#9ca7b6")));
        emptyItem->setPos(56, cardsStartY);
    } else {
        const int columns = 2;
        const qreal cardWidth = 360.0;
        const qreal cardHeight = 96.0;
        const qreal gapX = 22.0;
        const qreal gapY = 18.0;
        const qreal startX = 56.0;
        const qreal startY = cardsStartY;

        for (int index = 0; index < entries.size(); ++index) {
            const MapSceneEntry &entry = entries.at(index);
            auto *card = new MapCardItem(entry, mapEntryAccentForCategory(entry.category));
            card->setMoveCommittedCallback([this](int lineNumber, const QPointF &oldPosition, const QPointF &newPosition) {
                recordCardMove(lineNumber, oldPosition, newPosition);
            });
            card->setVisibilityCommittedCallback([this](int lineNumber, bool oldVisible, bool newVisible) {
                recordCardVisibility(lineNumber, oldVisible, newVisible);
            });
            card->setPos(startX + (index % columns) * (cardWidth + gapX), startY + (index / columns) * (cardHeight + gapY));
            card->setToolTip(tr("%1:%2\n%3 %4").arg(filePath(), QString::number(entry.lineNumber), entry.category, entry.title));
            mapScene_->addItem(card);
            mapItemsByLine_.insert(entry.lineNumber, card);
        }
    }

    restoreDraftGeometryItems();
    selectMapLine(textEditor_->currentLineNumber());
    refreshStatus();
    updateCommandSurfaceState();
    updateHelpPanel();
}

bool MapEditorTab::loadFile(const QString &filePath, QString *errorMessage)
{
    const QString currentFilePath = this->filePath();
    if (!currentFilePath.isEmpty() && QFileInfo(currentFilePath).canonicalFilePath() != QFileInfo(filePath).canonicalFilePath()) {
        clearDraftGeometryItems();
    }

    const bool loaded = textEditor_->loadFile(filePath, errorMessage);
    if (!loaded) {
        return false;
    }

    refreshMapScene();
    fitMapToView();
    refreshTitle();
    refreshStatus();
    return true;
}

bool MapEditorTab::save(QString *errorMessage)
{
    return textEditor_->save(errorMessage);
}

bool MapEditorTab::rewriteStructureEntryName(int lineNumber, const QString &category, const QString &newName, QString *errorMessage)
{
    const bool rewritten = textEditor_->rewriteStructureEntryName(lineNumber, category, newName, errorMessage);
    if (rewritten) {
        refreshMapScene();
        refreshTitle();
        refreshStatus();
    }

    return rewritten;
}

void MapEditorTab::setProjectRootPath(const QString &projectRootPath)
{
    projectRootPath_ = projectRootPath;
    textEditor_->setProjectRootPath(projectRootPath);
    refreshStatus();
}

void MapEditorTab::showFindBar(bool replaceMode)
{
    if (workspaceMode_ == WorkspaceMode::MapOnly) {
        setWorkspaceMode(WorkspaceMode::Split);
    }

    textEditor_->showFindBar(replaceMode);
}

void MapEditorTab::hideFindBar()
{
    textEditor_->hideFindBar();
}

void MapEditorTab::goToLine(int lineNumber)
{
    if (workspaceMode_ == WorkspaceMode::MapOnly) {
        setWorkspaceMode(WorkspaceMode::Split);
    }

    textEditor_->goToLine(lineNumber);
}

QString MapEditorTab::filePath() const
{
    return textEditor_->filePath();
}

QString MapEditorTab::displayName() const
{
    return textEditor_->displayName();
}

bool MapEditorTab::isDirty() const
{
    return textEditor_->isDirty();
}

int MapEditorTab::currentLineNumber() const
{
    return textEditor_->currentLineNumber();
}

QString MapEditorTab::text() const
{
    return textEditor_ != nullptr ? textEditor_->text() : QString();
}

MapEditorTab::WorkspaceMode MapEditorTab::workspaceMode() const
{
    return workspaceMode_;
}

void MapEditorTab::setWorkspaceMode(WorkspaceMode mode)
{
    workspaceMode_ = mode;
    if (workspaceModeCombo_ != nullptr) {
        workspaceModeCombo_->blockSignals(true);
        workspaceModeCombo_->setCurrentIndex(workspaceModeToIndex(mode));
        workspaceModeCombo_->blockSignals(false);
    }

    SessionStore::setTherionMapWorkspaceMode(workspaceModeToSetting(mode));
    updateWorkspaceVisibility();
    refreshMapScene();
}

void MapEditorTab::handleWorkspaceModeChanged(int index)
{
    setWorkspaceMode(workspaceModeFromIndex(index));
}

void MapEditorTab::handleTextEditorCurrentLineChanged(int lineNumber)
{
    selectMapLine(lineNumber);
    emit currentLineChanged(lineNumber);
}

void MapEditorTab::handleMapSceneSelectionChanged()
{
    if (updatingSelection_ || mapScene_ == nullptr) {
        updateHelpPanel();
        return;
    }

    const QList<QGraphicsItem *> selectedItems = mapScene_->selectedItems();
    if (selectedItems.isEmpty()) {
        updateHelpPanel();
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>(selectedItems.first());
    if (card != nullptr) {
        if (card->lineNumber() > 0 && card->lineNumber() != currentLineNumber()) {
            textEditor_->goToLine(card->lineNumber());
        }

        updateCommandSurfaceState();
        updateHelpPanel();
        return;
    }

    updateCommandSurfaceState();
    updateHelpPanel();
}

void MapEditorTab::handleAddPointTriggered()
{
    addDraftGeometryItem(createDraftGeometryItem(DraftGeometryKind::Point), mapView_ != nullptr ? mapView_->mapToScene(mapView_->viewport()->rect().center()) : QPointF(160.0, 160.0));
}

void MapEditorTab::handleAddLineTriggered()
{
    addDraftGeometryItem(createDraftGeometryItem(DraftGeometryKind::Line), mapView_ != nullptr ? mapView_->mapToScene(mapView_->viewport()->rect().center()) : QPointF(180.0, 180.0));
}

void MapEditorTab::handleAddAreaTriggered()
{
    addDraftGeometryItem(createDraftGeometryItem(DraftGeometryKind::Area), mapView_ != nullptr ? mapView_->mapToScene(mapView_->viewport()->rect().center()) : QPointF(200.0, 200.0));
}

void MapEditorTab::handleCompleteDraftTriggered()
{
    auto *draftRectItem = selectedDraftGeometryItem();
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(draftRectItem);
    if (draftItem == nullptr || draftItem->isDraftComplete()) {
        return;
    }

    if (undoStack_ != nullptr) {
        undoStack_->push(new MapDraftCompleteCommand(draftItem, false, true));
    } else {
        draftItem->setDraftComplete(true);
    }

    updateHelpPanel();
}

void MapEditorTab::handleUndoTriggered()
{
    if (undoStack_ != nullptr) {
        undoStack_->undo();
    }
}

void MapEditorTab::handleRedoTriggered()
{
    if (undoStack_ != nullptr) {
        undoStack_->redo();
    }
}

void MapEditorTab::handleZoomInTriggered()
{
    adjustMapZoom(1.2);
}

void MapEditorTab::handleZoomOutTriggered()
{
    adjustMapZoom(1.0 / 1.2);
}

void MapEditorTab::handleFitTriggered()
{
    fitMapToView();
}

void MapEditorTab::updateCommandSurfaceState()
{
    if (undoButton_ != nullptr) {
        undoButton_->setEnabled(undoStack_ != nullptr && undoStack_->canUndo());
    }
    if (redoButton_ != nullptr) {
        redoButton_->setEnabled(undoStack_ != nullptr && undoStack_->canRedo());
    }
    if (zoomLabel_ != nullptr) {
        zoomLabel_->setText(tr("%1%").arg(qRound(zoomFactor_ * 100.0)));
    }
    if (pointButton_ != nullptr) {
        const bool mapReady = mapScene_ != nullptr;
        pointButton_->setEnabled(mapReady);
        lineButton_->setEnabled(mapReady);
        areaButton_->setEnabled(mapReady);
        completeDraftButton_->setEnabled(mapReady && selectedDraftGeometryItem() != nullptr);
    }
}

void MapEditorTab::updateHelpPanel()
{
    if (helpBrowser_ == nullptr) {
        return;
    }

    if (helpPanel_ != nullptr) {
        helpPanel_->setToolTip(helpCollapsed_ ? tr("Expand map help") : tr("Map help and selection guidance"));
    }

    if (mapScene_ == nullptr) {
        helpBrowser_->setHtml(mapWorkspaceHelpHtml());
        return;
    }

    const QList<QGraphicsItem *> selectedItems = mapScene_->selectedItems();
    if (selectedItems.isEmpty()) {
        helpBrowser_->setHtml(mapWorkspaceHelpHtml());
        return;
    }

    if (auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(selectedItems.first())) {
        QString html;
        html += QStringLiteral("<h3 style='margin:0 0 6px 0;'>Draft Geometry</h3>");
        html += QStringLiteral("<p style='margin:0 0 4px 0;'><strong>Kind:</strong> %1</p>").arg(draftGeometryLabel(draftItem->kind()).toHtmlEscaped());
        html += QStringLiteral("<p style='margin:0 0 4px 0;'><strong>Status:</strong> %1</p>").arg(draftItem->isDraftComplete() ? tr("Complete") : tr("Draft"));
        html += QStringLiteral("<p style='margin:8px 0 0 0;'>Drag the item to reposition it, or use Complete Draft to finish it.</p>");
        helpBrowser_->setHtml(html);
        return;
    }

    if (auto *card = dynamic_cast<MapCardItem *>(selectedItems.first())) {
        QString html;
        html += QStringLiteral("<h3 style='margin:0 0 6px 0;'>%1</h3>").arg(card->category().toHtmlEscaped());
        html += QStringLiteral("<p style='margin:0 0 4px 0;'><strong>Source line:</strong> %1</p>").arg(card->lineNumber());
        html += QStringLiteral("<p style='margin:8px 0 0 0;'>Select a card to jump to the source line and keep the text and map in sync.</p>");
        helpBrowser_->setHtml(html);
        return;
    }

    helpBrowser_->setHtml(mapWorkspaceHelpHtml());
}

void MapEditorTab::setHelpCollapsed(bool collapsed)
{
    helpCollapsed_ = collapsed;
    if (helpBrowser_ != nullptr) {
        helpBrowser_->setVisible(!collapsed);
    }
    if (helpToggleButton_ != nullptr) {
        helpToggleButton_->setText(collapsed ? tr("Show") : tr("Hide"));
    }
    if (helpPanel_ != nullptr) {
        helpPanel_->setMinimumHeight(collapsed ? helpPanel_->sizeHint().height() : helpPanelHeight_);
    }
}

void MapEditorTab::updateWorkspaceVisibility()
{
    if (workspaceMode_ == WorkspaceMode::TextOnly) {
        textEditor_->show();
        mapView_->hide();
    } else if (workspaceMode_ == WorkspaceMode::MapOnly) {
        textEditor_->hide();
        mapView_->show();
    } else {
        textEditor_->show();
        mapView_->show();
        splitter_->setSizes(QList<int>{1, 1});
    }

    refreshStatus();
}

void MapEditorTab::clearMapScene()
{
    mapItemsByLine_.clear();
    if (mapScene_ == nullptr) {
        return;
    }

    QVector<QGraphicsRectItem *> preservedDrafts;
    const QList<QGraphicsItem *> items = mapScene_->items();
    for (QGraphicsItem *item : items) {
        if (auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item)) {
            mapScene_->removeItem(draftItem);
            preservedDrafts.append(draftItem);
            continue;
        }

        mapScene_->removeItem(item);
        delete item;
    }

    draftGeometryItems_ = preservedDrafts;
}

void MapEditorTab::clearDraftGeometryItems()
{
    if (mapScene_ != nullptr) {
        for (QGraphicsRectItem *item : std::as_const(draftGeometryItems_)) {
            if (item != nullptr) {
                mapScene_->removeItem(item);
                delete item;
            }
        }
    }

    draftGeometryItems_.clear();
    nextDraftGeometryId_ = 1;
}

void MapEditorTab::restoreDraftGeometryItems()
{
    if (mapScene_ == nullptr) {
        return;
    }

    for (QGraphicsRectItem *item : std::as_const(draftGeometryItems_)) {
        if (item == nullptr || mapScene_->items().contains(item)) {
            continue;
        }

        mapScene_->addItem(item);
        item->setZValue(20.0);
    }
}

void MapEditorTab::fitMapToView()
{
    if (mapScene_ == nullptr || mapView_ == nullptr || mapScene_->items().isEmpty()) {
        return;
    }

    zoomFactor_ = 1.0;
    mapView_->resetTransform();
    mapView_->fitInView(mapScene_->itemsBoundingRect().adjusted(-24, -24, 24, 24), Qt::KeepAspectRatio);
    updateCommandSurfaceState();
}

void MapEditorTab::adjustMapZoom(qreal factor)
{
    if (mapView_ == nullptr) {
        return;
    }

    const qreal nextZoomFactor = qBound(0.25, zoomFactor_ * factor, 4.0);
    const qreal appliedFactor = nextZoomFactor / zoomFactor_;
    zoomFactor_ = nextZoomFactor;
    mapView_->scale(appliedFactor, appliedFactor);
    updateCommandSurfaceState();
}

void MapEditorTab::recordCardMove(int lineNumber, const QPointF &oldPosition, const QPointF &newPosition)
{
    if (undoStack_ == nullptr || oldPosition == newPosition) {
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>(mapItemsByLine_.value(lineNumber, nullptr));
    if (card == nullptr) {
        return;
    }

    undoStack_->push(new MapCardMoveCommand(card, oldPosition, newPosition));
}

void MapEditorTab::recordCardVisibility(int lineNumber, bool oldVisible, bool newVisible)
{
    if (undoStack_ == nullptr || oldVisible == newVisible) {
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>(mapItemsByLine_.value(lineNumber, nullptr));
    if (card == nullptr) {
        return;
    }

    undoStack_->push(new MapCardVisibilityCommand(card, oldVisible, newVisible));
}

QGraphicsRectItem *MapEditorTab::selectedDraftGeometryItem() const
{
    if (mapScene_ == nullptr) {
        return nullptr;
    }

    const QList<QGraphicsItem *> selectedItems = mapScene_->selectedItems();
    for (QGraphicsItem *item : selectedItems) {
        if (auto *draftItem = dynamic_cast<QGraphicsRectItem *>(item)) {
            if (dynamic_cast<MapDraftGeometryItem *>(draftItem) != nullptr) {
                return draftItem;
            }
        }
    }

    return nullptr;
}

QGraphicsRectItem *MapEditorTab::createDraftGeometryItem(DraftGeometryKind kind)
{
    return new MapDraftGeometryItem(nextDraftGeometryId_++, kind);
}

void MapEditorTab::addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return;
    }

    draftItem->setPos(position);
    draftItem->setMoveCommittedCallback([this, draftItem](int, const QPointF &oldPosition, const QPointF &newPosition) {
        recordDraftMove(draftItem, oldPosition, newPosition);
    });
    draftItem->setVisibilityCommittedCallback([this, draftItem](int, bool oldVisible, bool newVisible) {
        recordDraftVisibility(draftItem, oldVisible, newVisible);
    });
    if (undoStack_ != nullptr) {
        undoStack_->push(new MapDraftAddCommand(mapScene_, &draftGeometryItems_, draftItem, position));
    } else {
        if (mapScene_ != nullptr) {
            mapScene_->addItem(draftItem);
        }
        draftGeometryItems_.append(draftItem);
    }

    updateCommandSurfaceState();
}

void MapEditorTab::removeDraftGeometryItem(QGraphicsRectItem *item)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return;
    }

    draftGeometryItems_.removeAll(draftItem);
    if (mapScene_ != nullptr) {
        mapScene_->removeItem(draftItem);
    }
    delete draftItem;
}

void MapEditorTab::recordDraftMove(QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr || undoStack_ == nullptr || oldPosition == newPosition) {
        return;
    }

    undoStack_->push(new MapDraftMoveCommand(draftItem, oldPosition, newPosition));
}

void MapEditorTab::recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr || undoStack_ == nullptr || oldVisible == newVisible) {
        return;
    }

    undoStack_->push(new MapDraftVisibilityCommand(draftItem, oldVisible, newVisible));
}

void MapEditorTab::refreshTitle()
{
    emit titleChanged();
}

void MapEditorTab::selectMapLine(int lineNumber)
{
    if (mapScene_ == nullptr) {
        return;
    }

    updatingSelection_ = true;
    mapScene_->clearSelection();

    auto selectedItemIt = mapItemsByLine_.find(lineNumber);
    if (selectedItemIt != mapItemsByLine_.end() && selectedItemIt.value() != nullptr) {
        selectedItemIt.value()->setSelected(true);
        mapView_->centerOn(selectedItemIt.value());
    }

    updatingSelection_ = false;
}

void MapEditorTab::refreshStatus()
{
    if (statusLabel_ == nullptr) {
        return;
    }

    const QString documentLabel = displayPath().isEmpty() ? tr("No file open") : displayPath();
    statusLabel_->setText(tr("%1 | %2").arg(documentLabel, workspaceModeLabel(workspaceMode_)));
}

QString MapEditorTab::displayPath() const
{
    return textEditor_->filePath();
}

MapEditorTab::WorkspaceMode MapEditorTab::workspaceModeFromIndex(int index)
{
    switch (index) {
    case 0:
        return WorkspaceMode::TextOnly;
    case 1:
        return WorkspaceMode::MapOnly;
    default:
        return WorkspaceMode::Split;
    }
}

int MapEditorTab::workspaceModeToIndex(WorkspaceMode mode)
{
    switch (mode) {
    case WorkspaceMode::TextOnly:
        return 0;
    case WorkspaceMode::MapOnly:
        return 1;
    case WorkspaceMode::Split:
        return 2;
    }

    return 2;
}

MapEditorTab::WorkspaceMode MapEditorTab::workspaceModeFromSetting(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("text-only")) {
        return WorkspaceMode::TextOnly;
    }
    if (normalized == QStringLiteral("map-only")) {
        return WorkspaceMode::MapOnly;
    }

    return WorkspaceMode::Split;
}

QString MapEditorTab::workspaceModeToSetting(WorkspaceMode mode)
{
    switch (mode) {
    case WorkspaceMode::TextOnly:
        return QStringLiteral("text-only");
    case WorkspaceMode::MapOnly:
        return QStringLiteral("map-only");
    case WorkspaceMode::Split:
        return QStringLiteral("split");
    }

    return QStringLiteral("split");
}
}