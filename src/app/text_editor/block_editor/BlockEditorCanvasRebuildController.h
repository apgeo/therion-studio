#pragma once

#include "BlockEditorSourceController.h"
#include "../TextEditorCommandMetadata.h"

#include <functional>

#include <QHash>
#include <QList>
#include <QPointF>

class QGraphicsLineItem;
class QGraphicsScene;

namespace TherionStudio
{
struct BlockEditorCanvasRebuildContext
{
    QGraphicsScene *scene = nullptr;
    QGraphicsLineItem **movePreviewLine = nullptr;
    QList<QGraphicsLineItem *> *containerBoundaryGuideItems = nullptr;
    QHash<int, qreal> *containerBoundaryEndYByLine = nullptr;
    const int *selectedLineNumber = nullptr;
    const bool *tearingDown = nullptr;
    const bool *blocksModeActive = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
    std::function<BlockEditorSourceContext()> sourceContext;
    std::function<bool()> isBlocksModeSupportedForCurrentFile;
    std::function<bool()> ensureEncodingRootDirectiveForBlocks;
    std::function<void(int)> handleBlockDeleteRequest;
    std::function<void(int, const QPointF &)> handleBlockMoveRequest;
    std::function<void(int, const QPointF &)> updateBlockMovePreview;
    std::function<void()> clearBlockMovePreview;
    std::function<void()> refreshBlockDetailsSelectionFromScene;
    std::function<QString(const char *)> translate;
};

class BlockEditorCanvasRebuildController final
{
public:
    explicit BlockEditorCanvasRebuildController(BlockEditorCanvasRebuildContext context);

    void rebuildBlocksCanvasFromText();

private:
    QString tr(const char *text) const;

    BlockEditorCanvasRebuildContext context_;
};
}
