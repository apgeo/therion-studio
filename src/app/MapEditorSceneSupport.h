#pragma once

#include <QColor>
#include <QHash>
#include <QGraphicsRectItem>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTransform>
#include <QVariant>
#include <QVector>

#include <functional>
#include <optional>

class QGraphicsScene;
class QGraphicsSceneMouseEvent;
class QPainter;
class QStyleOptionGraphicsItem;
class QUndoCommand;
class QWidget;

namespace TherionStudio
{
struct TherionParsedLine;

enum class DraftGeometryKind
{
    Point,
    Line,
    Area
};

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
    struct LineSegment
    {
        enum class Type
        {
            Linear,
            Cubic
        };

        Type type = Type::Linear;
        int endVertexIndex = -1;
        int control1VertexIndex = -1;
        int control2VertexIndex = -1;
    };
    struct TH2LineVertex
    {
        QPointF anchor;
        std::optional<QPointF> incomingControl;
        std::optional<QPointF> outgoingControl;
        bool isSmooth = true;
        int anchorSourceVertexIndex = -1;
        int incomingSourceVertexIndex = -1;
        int outgoingSourceVertexIndex = -1;
    };

    Kind kind = Kind::Point;
    int lineNumber = 0;
    QString category;
    QString label;
    QString subtitle;
    QColor accent;
    QVector<QPointF> vertices;
    QPointF anchor;
    QPointF sourceAnchor;
    bool hasAnchor = false;
    bool hasSourceAnchor = false;
    bool closed = false;
    bool reversed = false;
    bool stationPoint = false;
    bool hasCoordinateTransform = false;
    QTransform sourceToMapTransform;
    QTransform mapToSourceTransform;
    QVector<LineSegment> lineSegments;
    QVector<TH2LineVertex> lineVertices;
};

class MapCardItem final : public QGraphicsRectItem
{
public:
    MapCardItem(const MapSceneEntry &entry, const QColor &accent);

    void setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback);
    void setVisibilityCommittedCallback(std::function<void(int, bool, bool)> callback);

    int lineNumber() const;
    QString category() const;
    bool isObjectVisible() const;
    void setObjectVisible(bool visible);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QRectF visibilityHotspot() const;

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

class MapDraftGeometryItem final : public QGraphicsRectItem
{
public:
    MapDraftGeometryItem(int draftId, DraftGeometryKind kind);

    int draftId() const;
    DraftGeometryKind kind() const;

    bool isDraftComplete() const;
    void setDraftComplete(bool complete);

    bool isObjectVisible() const;
    void setObjectVisible(bool visible);

    void setMoveCommittedCallback(std::function<void(int, const QPointF &, const QPointF &)> callback);
    void setVisibilityCommittedCallback(std::function<void(int, bool, bool)> callback);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QRectF visibilityHotspot() const;
    QString geometryDescription() const;

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

QString draftGeometryLabel(DraftGeometryKind kind);
QString mapWorkspaceHelpHtml();

QString mapEntryCategoryForLine(const TherionParsedLine &parsedLine);
QString mapEntryTitleForLine(const TherionParsedLine &parsedLine);
QString mapEntrySubtitleForLine(const TherionParsedLine &parsedLine);
QColor mapEntryAccentForCategory(const QString &category);
QVector<MapSceneEntry> collectMapSceneEntries(const QVector<TherionParsedLine> &parsedLines);

QRectF geometryBoundsForFeatures(const QVector<MapGeometryFeature> &features);
QPointF mapGeometryPointToPreview(const QPointF &point, const QRectF &sourceBounds, const QRectF &targetBounds);
QVector<MapGeometryFeature> collectGeometryFeatures(const QVector<TherionParsedLine> &parsedLines);
void renderMapWorkspaceScene(QGraphicsScene *scene,
                             const QString &documentPath,
                             const QVector<MapSceneEntry> &entries,
                             const QVector<MapGeometryFeature> &geometryFeatures,
                             QHash<int, QGraphicsRectItem *> *mapItemsByLine,
                             const std::function<void(int, const QPointF &, const QPointF &)> &recordCardMove,
                             const std::function<void(int, bool, bool)> &recordCardVisibility,
                             const std::function<void(int, const QPointF &, const QPointF &)> &recordPointGeometryMove,
                             const std::function<void(int, const QString &, int, const QPointF &, const QPointF &)> &recordLineAreaVertexMove);

QUndoCommand *createMapCardMoveCommand(MapCardItem *item, const QPointF &oldPosition, const QPointF &newPosition);
QUndoCommand *createMapCardVisibilityCommand(MapCardItem *item, bool oldVisible, bool newVisible);
QUndoCommand *createMapDraftMoveCommand(MapDraftGeometryItem *item, const QPointF &oldPosition, const QPointF &newPosition);
QUndoCommand *createMapDraftVisibilityCommand(MapDraftGeometryItem *item, bool oldVisible, bool newVisible);
QUndoCommand *createMapDraftCompleteCommand(MapDraftGeometryItem *item, bool oldComplete, bool newComplete);
QUndoCommand *createMapDraftAddCommand(QGraphicsScene *scene,
                                       QVector<QGraphicsRectItem *> *items,
                                       MapDraftGeometryItem *item,
                                       const QPointF &position);

} // namespace TherionStudio
