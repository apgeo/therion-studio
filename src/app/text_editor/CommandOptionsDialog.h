#pragma once

#include "TextEditorCommandMetadata.h"

#include <QString>

#include <optional>

class QWidget;

namespace TherionStudio
{
struct CommandOptionsDialogContext
{
    QWidget *dialogParent = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
};

enum class CommandOptionsIdFieldMode
{
    None,
    Optional,
    Required,
};

enum class CommandOptionsHelpMode
{
    SelectedRow,
    CommandOnly,
};

class CommandOptionsDialog final
{
public:
    explicit CommandOptionsDialog(CommandOptionsDialogContext context);

    std::optional<QString> configureLine(const QString &commandName,
                                         const QString &sourceLine,
                                         int lineNumber,
                                         CommandOptionsIdFieldMode idFieldMode,
                                         CommandOptionsHelpMode helpMode = CommandOptionsHelpMode::SelectedRow);

private:
    QString tr(const char *text) const;

    CommandOptionsDialogContext context_;
};
}
