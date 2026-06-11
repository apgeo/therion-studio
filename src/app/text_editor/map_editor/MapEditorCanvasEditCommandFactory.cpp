#include "MapEditorCanvasEditCommandFactory.h"

#include "../TextEditorSourceTransactionController.h"
#include "../TextEditorTab.h"
#include <QCoreApplication>
#include <QGraphicsScene>
#include <QPointer>
#include <QUndoCommand>

#include <utility>

namespace TherionStudio
{
namespace
{
QString completedDraftRevertedMessage(int lineNumber)
{
    return lineNumber > 0
        ? QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Reverted completed draft at source line %1.")
              .arg(lineNumber)
        : QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Reverted completed draft.");
}

QString completedDraftRestoredMessage(int lineNumber)
{
    return lineNumber > 0
        ? QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Restored completed draft at source line %1.")
              .arg(lineNumber)
        : QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "Restored completed draft.");
}

class MapDraftCompletionCommand final : public QUndoCommand
{
public:
    MapDraftCompletionCommand(TextEditorTab *textEditor,
                              QGraphicsScene *scene,
                              QVector<QGraphicsRectItem *> *draftItems,
                              MapDraftGeometryItem *draftItem,
                              QString label,
                              QString beforeText,
                              QString afterText,
                              int insertedLineNumber,
                              std::function<void(const QString &)> statusCallback)
        : textEditor_(textEditor)
        , scene_(scene)
        , draftItems_(draftItems)
        , draftItem_(draftItem)
        , beforeText_(std::move(beforeText))
        , afterText_(std::move(afterText))
        , insertedLineNumber_(insertedLineNumber)
        , statusCallback_(std::move(statusCallback))
    {
        setText(std::move(label));
    }

    ~MapDraftCompletionCommand() override
    {
        if (draftItem_ != nullptr && !isDraftAttached()) {
            delete draftItem_;
            draftItem_ = nullptr;
        }
    }

    void undo() override
    {
        if (textEditor_ == nullptr) {
            setObsolete(true);
            return;
        }

        applyTextEditorSourceSnapshot(textEditor_, beforeText_);
        restoreDraftItem();
        if (statusCallback_ != nullptr) {
            statusCallback_(completedDraftRevertedMessage(insertedLineNumber_));
        }
    }

    void redo() override
    {
        if (firstRedo_) {
            firstRedo_ = false;
            detachDraftItem();
            return;
        }
        if (textEditor_ == nullptr) {
            setObsolete(true);
            return;
        }

        applyTextEditorSourceSnapshot(textEditor_, afterText_);
        detachDraftItem();
        if (statusCallback_ != nullptr) {
            statusCallback_(completedDraftRestoredMessage(insertedLineNumber_));
        }
    }

private:
    bool isDraftAttached() const
    {
        const bool inScene = scene_ != nullptr && draftItem_ != nullptr && scene_->items().contains(draftItem_);
        const bool inList = draftItems_ != nullptr && draftItem_ != nullptr && draftItems_->contains(draftItem_);
        return inScene || inList;
    }

    void detachDraftItem()
    {
        if (draftItem_ == nullptr) {
            return;
        }
        if (scene_ != nullptr) {
            scene_->removeItem(draftItem_);
        }
        if (draftItems_ != nullptr) {
            draftItems_->removeAll(draftItem_);
        }
    }

    void restoreDraftItem()
    {
        if (draftItem_ == nullptr) {
            return;
        }
        if (scene_ != nullptr && !scene_->items().contains(draftItem_)) {
            scene_->addItem(draftItem_);
            draftItem_->setZValue(20.0);
            draftItem_->setSelected(true);
        }
        if (draftItems_ != nullptr && !draftItems_->contains(draftItem_)) {
            draftItems_->append(draftItem_);
        }
    }

    QPointer<TextEditorTab> textEditor_;
    QPointer<QGraphicsScene> scene_;
    QVector<QGraphicsRectItem *> *draftItems_ = nullptr;
    MapDraftGeometryItem *draftItem_ = nullptr;
    QString beforeText_;
    QString afterText_;
    int insertedLineNumber_ = 0;
    std::function<void(const QString &)> statusCallback_;
    bool firstRedo_ = true;
};

}

QUndoCommand *createMapDraftCompletionCommand(TextEditorTab *textEditor,
                                              QGraphicsScene *scene,
                                              QVector<QGraphicsRectItem *> *draftItems,
                                              MapDraftGeometryItem *draftItem,
                                              const QString &label,
                                              const QString &beforeText,
                                              const QString &afterText,
                                              int insertedLineNumber,
                                              MapCanvasEditStatusCallback statusCallback)
{
    return new MapDraftCompletionCommand(textEditor,
                                         scene,
                                         draftItems,
                                         draftItem,
                                         label,
                                         beforeText,
                                         afterText,
                                         insertedLineNumber,
                                         std::move(statusCallback));
}

MapLineAreaVertexMoveSet coupledLineVertexMoveSet(const MapGeometryFeature &lineFeature,
                                                  int sourceVertexIndex,
                                                  const QPointF &oldPoint,
                                                  const QPointF &newPoint)
{
    MapLineAreaVertexMoveSet result;
    const QVector<MapLineSecondaryMove> moves = collectLineSecondaryMovesForVertexDrag(lineFeature,
                                                                                       sourceVertexIndex,
                                                                                       oldPoint,
                                                                                       newPoint);
    result.secondaryMoves.reserve(moves.size());
    for (const MapLineSecondaryMove &sourceMove : moves) {
        MapLineAreaVertexSecondaryMove move;
        move.vertexIndex = sourceMove.sourceVertexIndex;
        move.oldPoint = sourceMove.oldPoint;
        move.newPoint = sourceMove.newPoint;
        result.secondaryMoves.append(move);
    }
    return result;
}
}
