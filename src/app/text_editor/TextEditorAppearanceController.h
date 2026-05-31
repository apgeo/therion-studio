#pragma once

#include <functional>

#include <QColor>

class QGraphicsScene;
class QGraphicsView;
class QPlainTextEdit;
class QTextBrowser;
class QWidget;

namespace TherionStudio
{
struct TextEditorAppearanceContext
{
    QWidget *rootWidget = nullptr;
    QPlainTextEdit *editor = nullptr;
    QGraphicsView *blockCanvasView = nullptr;
    QGraphicsScene *blockCanvasScene = nullptr;
    QWidget *helpPanel = nullptr;
    QWidget *blockDetailsPanel = nullptr;
    QWidget *blockDetailsEditPanel = nullptr;
    QWidget *blockDetailsHelpPanel = nullptr;
    QTextBrowser *helpBrowser = nullptr;
    QTextBrowser *blockDetailsHelpBrowser = nullptr;
    bool *blocksModeActive = nullptr;
    std::function<void()> updateContextHelp;
    std::function<void()> updateBlockDetailsHelpForCurrentFocus;
};

class TextEditorAppearanceController final
{
public:
    explicit TextEditorAppearanceController(TextEditorAppearanceContext context);

    QColor sourceSurfaceColor() const;
    void handleApplicationAppearanceChanged();

private:
    TextEditorAppearanceContext context_;
};
}
