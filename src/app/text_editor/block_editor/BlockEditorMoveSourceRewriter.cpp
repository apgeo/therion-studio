#include "BlockEditorMoveSourceRewriter.h"

#include <QtGlobal>

namespace TherionStudio
{
BlockEditorMoveRewriteResult BlockEditorMoveSourceRewriter::rewriteMovedBlock(
    QStringList lines,
    const BlockEditorDocumentEntry &sourceEntry,
    int insertBeforeLineOriginal) const
{
    const int sourceStartIndex = sourceEntry.startLine - 1;
    const int sourceEndIndex = sourceEntry.endLine - 1;
    if (sourceStartIndex < 0 || sourceEndIndex >= lines.size() || sourceStartIndex > sourceEndIndex) {
        return BlockEditorMoveRewriteResult{lines, false};
    }

    QStringList movedChunk;
    for (int index = sourceStartIndex; index <= sourceEndIndex; ++index) {
        movedChunk.append(lines.at(index));
    }
    for (int index = sourceEndIndex; index >= sourceStartIndex; --index) {
        lines.removeAt(index);
    }

    int insertIndex = insertBeforeLineOriginal - 1;
    if (sourceEntry.startLine < insertBeforeLineOriginal) {
        insertIndex -= movedChunk.size();
    }
    insertIndex = qBound(0, insertIndex, lines.size());

    for (int offset = 0; offset < movedChunk.size(); ++offset) {
        lines.insert(insertIndex + offset, movedChunk.at(offset));
    }

    return BlockEditorMoveRewriteResult{lines, true};
}
}
