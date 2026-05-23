#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace TherionStudio
{
struct TherionParsedLine;

struct BlockEditorDocumentOutlineContext
{
    std::function<QString(const QString &, const QStringList &, int)> resolveScopeForCommandAtLine;
    std::function<bool(const QString &, const TherionParsedLine &)> isContainerDirectiveInstanceForParsedLine;
    std::function<bool(const QString &, const QString &)> isCommandDirectiveInScope;
};

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
    explicit BlockEditorDocumentOutlineBuilder(BlockEditorDocumentOutlineContext context);

    BlockEditorDocumentOutline buildFromContents(const QString &contents) const;

private:
    BlockEditorDocumentOutlineContext context_;
};
}
