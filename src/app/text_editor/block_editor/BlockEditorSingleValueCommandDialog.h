#pragma once

#include "../TextEditorCommandMetadata.h"

#include <functional>
#include <QString>

#include <optional>

class QWidget;

namespace TherionStudio
{
struct BlockEditorSingleValueCommandDialogContext
{
    QWidget *dialogParent = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
    std::function<QString(const char *)> translate;
};

class BlockEditorSingleValueCommandDialog final
{
public:
    explicit BlockEditorSingleValueCommandDialog(BlockEditorSingleValueCommandDialogContext context);

    std::optional<QString> configureLine(const QString &commandName,
                                         const QString &sourceLine,
                                         int lineNumber);

private:
    QString tr(const char *text) const;

    BlockEditorSingleValueCommandDialogContext context_;
};
}
