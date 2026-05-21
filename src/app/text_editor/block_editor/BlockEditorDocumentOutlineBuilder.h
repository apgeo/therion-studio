#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
class TextEditorTab;

struct BlockEditorDocumentEntry
{
    QString kind;
    int startLine = 0;
    int endLine = 0;
    int parentLine = 0;
};

struct BlockEditorDocumentOutline
{
    QStringList lines;
    QVector<BlockEditorDocumentEntry> entries;
    QHash<int, int> entryIndexByStartLine;
};

class BlockEditorDocumentOutlineBuilder final
{
public:
    explicit BlockEditorDocumentOutlineBuilder(const TextEditorTab *owner);

    BlockEditorDocumentOutline buildFromContents(const QString &contents) const;

private:
    const TextEditorTab *owner_ = nullptr;
};
}
