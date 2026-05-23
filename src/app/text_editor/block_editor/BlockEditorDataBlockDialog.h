#pragma once

#include "../TextEditorCommandMetadata.h"

#include <functional>
#include <QString>
#include <QStringList>

#include <optional>

class QWidget;

namespace TherionStudio
{
struct BlockEditorDataBlockDialogContext
{
    QWidget *dialogParent = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
    std::function<QString(const QString &, const QStringList &, int)> resolveScopeForCommandAtLine;
    std::function<bool(const QString &, const QString &)> isCommandDirectiveInScope;
    std::function<QString(const char *)> translate;
};

struct BlockEditorDataBlockDialogResult
{
    QStringList rowLines;
    int dataBodyLastLine = 0;
    bool schemaMismatchDetected = false;
};

class BlockEditorDataBlockDialog final
{
public:
    explicit BlockEditorDataBlockDialog(BlockEditorDataBlockDialogContext context);

    std::optional<BlockEditorDataBlockDialogResult> configureRows(const QStringList &lines,
                                                                  int lineNumber);

private:
    QString tr(const char *text) const;

    BlockEditorDataBlockDialogContext context_;
};
}
