#pragma once

#include <functional>

class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QString;
class QWidget;

namespace TherionStudio
{
struct TextEditorModeContext
{
    const QString *filePath = nullptr;
    const QString *untitledDisplayName = nullptr;
    const QString *fileEncodingName = nullptr;
    bool *blocksModeActive = nullptr;
    bool *enforcingEncodingRootDirective = nullptr;
    QPlainTextEdit *editor = nullptr;
    QPushButton *rawModeButton = nullptr;
    QPushButton *blocksModeButton = nullptr;
    QStackedWidget *editorModeStack = nullptr;
    QWidget *rawEditorPanel = nullptr;
    QWidget *blocksPanel = nullptr;
    std::function<void()> hideFindBar;
    std::function<bool(const QString &)> replaceTextForSystemNormalization;
    std::function<void()> rebuildBlocksCanvasFromText;
    std::function<void()> populateBlockToolbox;
    std::function<void()> editorModeChanged;
};

class TextEditorModeController final
{
public:
    explicit TextEditorModeController(TextEditorModeContext context);

    bool isBlocksModeSupportedForCurrentFile() const;
    void refreshBlocksModeAvailability();
    void setBlocksModeActive(bool active);
    bool ensureEncodingRootDirectiveForBlocks();
    void refreshEditorModeUi();

private:
    TextEditorModeContext context_;
};
}
