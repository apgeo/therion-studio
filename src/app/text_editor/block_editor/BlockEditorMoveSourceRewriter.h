#pragma once

#include "BlockEditorDocumentOutlineBuilder.h"

#include <QStringList>

namespace TherionStudio
{
struct BlockEditorMoveRewriteResult
{
    QStringList lines;
    bool applied = false;
};

class BlockEditorMoveSourceRewriter final
{
public:
    BlockEditorMoveRewriteResult rewriteMovedBlock(QStringList lines,
                                                   const BlockEditorDocumentEntry &sourceEntry,
                                                   int insertBeforeLineOriginal) const;
};
}
