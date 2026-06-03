#include "MapEditorCanvasEditCommandFactory.h"

#include "../TextEditorTab.h"

#include <QCoreApplication>
#include <QGraphicsScene>
#include <QPointer>
#include <QUndoCommand>

#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
QString pointGeometryRevertedMessage(int lineNumber)
{
    return QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                       "Reverted point geometry at source line %1.")
        .arg(lineNumber);
}

QString pointGeometryUpdatedMessage(int lineNumber)
{
    return QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                       "Updated point geometry at source line %1.")
        .arg(lineNumber);
}

QString pointMoveFailedMessage(const QString &errorMessage)
{
    return errorMessage.isEmpty()
        ? QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory", "Point move failed.")
        : QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory", "Point move failed: %1")
              .arg(errorMessage);
}

QString vertexRevertedMessage(const QString &kind, int vertexIndex, int lineNumber)
{
    return QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                       "Reverted %1 vertex %2 at source line %3.")
        .arg(kind)
        .arg(vertexIndex + 1)
        .arg(lineNumber);
}

QString vertexUpdatedMessage(const QString &kind, int vertexIndex, int lineNumber)
{
    return QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                       "Updated %1 vertex %2 at source line %3.")
        .arg(kind)
        .arg(vertexIndex + 1)
        .arg(lineNumber);
}

QString vertexMoveFailedMessage(const QString &kind, const QString &errorMessage)
{
    return errorMessage.isEmpty()
        ? QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory", "%1 vertex move failed.")
              .arg(kind)
        : QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory", "%1 vertex move failed: %2")
              .arg(kind, errorMessage);
}

QString coupledVertexMoveFailedMessage(const QString &kind, const QString &errorMessage)
{
    return errorMessage.isEmpty()
        ? QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "%1 coupled vertex move failed.")
              .arg(kind)
        : QCoreApplication::translate("TherionStudio::MapEditorCanvasEditCommandFactory",
                                      "%1 coupled vertex move failed: %2")
              .arg(kind, errorMessage);
}

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

class MapPointGeometryMoveCommand final : public QUndoCommand
{
public:
    static constexpr int kCommandId = 0x4d50474d; // MPGM

    MapPointGeometryMoveCommand(TextEditorTab *textEditor,
                                int lineNumber,
                                const QPointF &oldPoint,
                                const QPointF &newPoint,
                                std::function<void(const QString &)> statusCallback)
        : textEditor_(textEditor)
        , lineNumber_(lineNumber)
        , oldPoint_(oldPoint)
        , newPoint_(newPoint)
        , statusCallback_(std::move(statusCallback))
    {
        if (textEditor_ != nullptr) {
            beforeTextSnapshot_ = textEditor_->text();
        }
    }

    int id() const override
    {
        return kCommandId;
    }

    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *otherCommand = dynamic_cast<const MapPointGeometryMoveCommand *>(other);
        if (otherCommand == nullptr
            || textEditor_ != otherCommand->textEditor_
            || lineNumber_ != otherCommand->lineNumber_) {
            return false;
        }

        newPoint_ = otherCommand->newPoint_;
        return true;
    }

    void undo() override
    {
        if (textEditor_ == nullptr || beforeTextSnapshot_.isNull()) {
            setObsolete(true);
            return;
        }

        textEditor_->replaceTextForCommand(beforeTextSnapshot_);
        if (statusCallback_ != nullptr) {
            statusCallback_(pointGeometryRevertedMessage(lineNumber_));
        }
    }

    void redo() override
    {
        if (textEditor_ == nullptr) {
            setObsolete(true);
            return;
        }

        if (afterTextSnapshot_.has_value()) {
            textEditor_->replaceTextForCommand(afterTextSnapshot_.value());
            if (statusCallback_ != nullptr) {
                statusCallback_(pointGeometryUpdatedMessage(lineNumber_));
            }
            return;
        }

        if (!applyPoint(newPoint_, pointGeometryUpdatedMessage(lineNumber_))) {
            setObsolete(true);
            return;
        }

        afterTextSnapshot_ = textEditor_->text();
    }

private:
    bool applyPoint(const QPointF &point, const QString &successMessage)
    {
        if (textEditor_ == nullptr) {
            return false;
        }

        QString errorMessage;
        if (!textEditor_->rewritePointCoordinates(lineNumber_, point, &errorMessage)) {
            if (statusCallback_ != nullptr) {
                statusCallback_(pointMoveFailedMessage(errorMessage));
            }
            return false;
        }

        if (statusCallback_ != nullptr) {
            statusCallback_(successMessage);
        }
        return true;
    }

    QPointer<TextEditorTab> textEditor_;
    int lineNumber_ = 0;
    QPointF oldPoint_;
    QPointF newPoint_;
    std::function<void(const QString &)> statusCallback_;
    QString beforeTextSnapshot_;
    std::optional<QString> afterTextSnapshot_;
};

class MapLineAreaVertexMoveCommand final : public QUndoCommand
{
public:
    static constexpr int kCommandId = 0x4d4c564d; // MLVM

    MapLineAreaVertexMoveCommand(TextEditorTab *textEditor,
                                 int lineNumber,
                                 QString kind,
                                 int vertexIndex,
                                 const QPointF &oldPoint,
                                 const QPointF &newPoint,
                                 QVector<MapLineAreaVertexSecondaryMove> secondaryMoves,
                                 std::function<void(const QString &)> statusCallback)
        : textEditor_(textEditor)
        , lineNumber_(lineNumber)
        , kind_(std::move(kind))
        , vertexIndex_(vertexIndex)
        , oldPoint_(oldPoint)
        , newPoint_(newPoint)
        , secondaryMoves_(std::move(secondaryMoves))
        , statusCallback_(std::move(statusCallback))
    {
        if (textEditor_ != nullptr) {
            beforeTextSnapshot_ = textEditor_->text();
        }
    }

    int id() const override
    {
        return kCommandId;
    }

    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *otherCommand = dynamic_cast<const MapLineAreaVertexMoveCommand *>(other);
        if (otherCommand == nullptr
            || textEditor_ != otherCommand->textEditor_
            || lineNumber_ != otherCommand->lineNumber_
            || kind_ != otherCommand->kind_
            || vertexIndex_ != otherCommand->vertexIndex_
            || !secondaryMoves_.isEmpty()
            || !otherCommand->secondaryMoves_.isEmpty()) {
            return false;
        }

        newPoint_ = otherCommand->newPoint_;
        return true;
    }

    void undo() override
    {
        if (textEditor_ == nullptr || beforeTextSnapshot_.isNull()) {
            setObsolete(true);
            return;
        }

        textEditor_->replaceTextForCommand(beforeTextSnapshot_);
        if (statusCallback_ != nullptr) {
            statusCallback_(vertexRevertedMessage(kind_, vertexIndex_, lineNumber_));
        }
    }

    void redo() override
    {
        if (textEditor_ == nullptr) {
            setObsolete(true);
            return;
        }

        if (afterTextSnapshot_.has_value()) {
            textEditor_->replaceTextForCommand(afterTextSnapshot_.value());
            if (statusCallback_ != nullptr) {
                statusCallback_(vertexUpdatedMessage(kind_, vertexIndex_, lineNumber_));
            }
            return;
        }

        if (!applyVertexWithSecondaries(newPoint_,
                                        secondaryMoves_,
                                        vertexUpdatedMessage(kind_, vertexIndex_, lineNumber_))) {
            setObsolete(true);
            return;
        }

        afterTextSnapshot_ = textEditor_->text();
    }

private:
    bool applyVertexWithSecondaries(const QPointF &point,
                                    const QVector<MapLineAreaVertexSecondaryMove> &secondaryMoves,
                                    const QString &successMessage)
    {
        if (textEditor_ == nullptr) {
            return false;
        }

        QString errorMessage;
        if (!textEditor_->rewriteLineAreaVertex(lineNumber_, kind_, vertexIndex_, point, &errorMessage)) {
            if (statusCallback_ != nullptr) {
                statusCallback_(vertexMoveFailedMessage(kind_, errorMessage));
            }
            return false;
        }

        for (const MapLineAreaVertexSecondaryMove &move : secondaryMoves) {
            if (move.vertexIndex < 0) {
                continue;
            }

            if (!textEditor_->rewriteLineAreaVertex(lineNumber_, kind_, move.vertexIndex, move.newPoint, &errorMessage)) {
                if (statusCallback_ != nullptr) {
                    statusCallback_(coupledVertexMoveFailedMessage(kind_, errorMessage));
                }
                return false;
            }
        }

        if (statusCallback_ != nullptr) {
            statusCallback_(successMessage);
        }
        return true;
    }

    QPointer<TextEditorTab> textEditor_;
    int lineNumber_ = 0;
    QString kind_;
    int vertexIndex_ = -1;
    QPointF oldPoint_;
    QPointF newPoint_;
    QVector<MapLineAreaVertexSecondaryMove> secondaryMoves_;
    std::function<void(const QString &)> statusCallback_;
    QString beforeTextSnapshot_;
    std::optional<QString> afterTextSnapshot_;
};

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

        textEditor_->replaceTextForCommand(beforeText_);
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

        textEditor_->replaceTextForCommand(afterText_);
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

QUndoCommand *createMapPointGeometryMoveCommand(TextEditorTab *textEditor,
                                                int lineNumber,
                                                const QPointF &oldPoint,
                                                const QPointF &newPoint,
                                                MapCanvasEditStatusCallback statusCallback)
{
    return new MapPointGeometryMoveCommand(textEditor, lineNumber, oldPoint, newPoint, std::move(statusCallback));
}

QUndoCommand *createMapLineAreaVertexMoveCommand(TextEditorTab *textEditor,
                                                 int lineNumber,
                                                 const QString &kind,
                                                 int vertexIndex,
                                                 const QPointF &oldPoint,
                                                 const QPointF &newPoint,
                                                 const QVector<MapLineAreaVertexSecondaryMove> &secondaryMoves,
                                                 MapCanvasEditStatusCallback statusCallback)
{
    return new MapLineAreaVertexMoveCommand(textEditor,
                                            lineNumber,
                                            kind,
                                            vertexIndex,
                                            oldPoint,
                                            newPoint,
                                            secondaryMoves,
                                            std::move(statusCallback));
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
