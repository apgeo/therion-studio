#pragma once

#include <functional>

#include <QString>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QWidget;

namespace TherionStudio
{
struct TextEditorStatusContext
{
    QPlainTextEdit *editor = nullptr;
    QLabel *encodingNoteLabel = nullptr;
    QPushButton *convertEncodingButton = nullptr;
    QWidget *statusRow = nullptr;
    QString *filePath = nullptr;
    QString *untitledDisplayName = nullptr;
    QString *projectRootPath = nullptr;
    QString *fileEncodingName = nullptr;
    QString *fileEncodingLabel = nullptr;
    QString *cleanTextSnapshot = nullptr;
    QString *cleanEncodingNameSnapshot = nullptr;
    QString *encodingStatusNote = nullptr;
    bool *dirty = nullptr;
    bool *inlineStatusRequestedVisible = nullptr;
    std::function<void()> titleChanged;
    std::function<void(bool)> dirtyStateChanged;
};

class TextEditorStatusController final
{
public:
    explicit TextEditorStatusController(TextEditorStatusContext context);

    QString displayName() const;
    QString statusPathText() const;
    QString statusEncodingText() const;
    void setInlineStatusVisible(bool visible);
    void refreshTitle();
    void refreshStatus();
    bool isCurrentStateDirty() const;
    void applyDirtyStateFromCurrentState();
    QString displayPath() const;

private:
    static QString trText(const char *sourceText);

    TextEditorStatusContext context_;
};
}
