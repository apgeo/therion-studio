#pragma once

#include "BlockEditorDocumentOutlineBuilder.h"
#include "BlockEditorDropTargetResolver.h"

#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>

namespace TherionStudio
{
class TextEditorTab;

struct BlockEditorMovePlan
{
    int destinationParentLine = 0;
    int insertBeforeLineOriginal = 1;
    QString destinationParentKind;
    bool resolved = false;
};

class BlockEditorMovePlanner final
{
public:
    explicit BlockEditorMovePlanner(const TextEditorTab *owner);

    BlockEditorMovePlan planMove(const BlockEditorDocumentEntry &sourceEntry,
                                 const QVector<BlockEditorDocumentEntry> &entries,
                                 const QHash<int, int> &entryIndexByStartLine,
                                 const BlockEditorDropTargetResolver::SceneItemsByLine &sceneItemsByLine,
                                 const BlockEditorSceneVerticalPlacement &verticalPlacement,
                                 int explicitEndHintContainerLine,
                                 const QPointF &scenePos,
                                 int appendLineNumber) const;

private:
    const TextEditorTab *owner_ = nullptr;
};
}
