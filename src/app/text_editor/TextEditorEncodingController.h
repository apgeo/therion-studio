#pragma once

#include <functional>

class QString;
class QWidget;

namespace TherionStudio
{
struct TextEditorEncodingContext
{
    QWidget *dialogParent = nullptr;
    QString *fileEncodingName = nullptr;
    QString *fileEncodingLabel = nullptr;
    QString *encodingStatusNote = nullptr;
    std::function<bool()> isBlocksModeSupportedForCurrentFile;
    std::function<bool()> ensureEncodingRootDirectiveForBlocks;
    std::function<void()> applyDirtyStateFromCurrentState;
    std::function<void()> refreshStatus;
};

class TextEditorEncodingController final
{
public:
    explicit TextEditorEncodingController(TextEditorEncodingContext context);

    void handleConvertToUtf8Triggered();

private:
    static QString trText(const char *sourceText);

    TextEditorEncodingContext context_;
};
}
