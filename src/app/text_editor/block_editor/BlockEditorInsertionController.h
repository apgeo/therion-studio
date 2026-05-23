#pragma once

#include "BlockEditorDocumentOutlineBuilder.h"
#include "BlockEditorDropTargetResolver.h"
#include "BlockEditorInsertionPlanner.h"
#include "BlockEditorSourceController.h"

#include <functional>
#include <QPointF>
#include <QString>

class QWidget;

namespace TherionStudio
{
struct BlockEditorInsertionContext
{
    QWidget *dialogParent = nullptr;
    std::function<BlockEditorSourceContext()> sourceContext;
    BlockEditorDocumentOutlineContext documentOutlineContext;
    BlockEditorDropTargetContext dropTargetContext;
    BlockEditorInsertionPlannerContext insertionPlannerContext;
    std::function<bool()> isBlocksModeSupportedForCurrentFile;
    std::function<int(const QPointF &)> resolveEndHintContainerStartLineAtScenePos;
    std::function<int()> appendLineNumber;
    std::function<QString(const char *)> translate;
};

class BlockEditorInsertionController final
{
public:
    explicit BlockEditorInsertionController(BlockEditorInsertionContext context);

    void insertFromDrop(const QString &kind, const QPointF &scenePos);

private:
    QString tr(const char *text) const;

    BlockEditorInsertionContext context_;
};
}
