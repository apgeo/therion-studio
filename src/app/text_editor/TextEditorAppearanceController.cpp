#include "TextEditorAppearanceController.h"

#include "TextEditorSurfaceStyler.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPalette>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QWidget>

#include <utility>

namespace TherionStudio
{
TextEditorAppearanceController::TextEditorAppearanceController(TextEditorAppearanceContext context)
    : context_(std::move(context))
{
}

QColor TextEditorAppearanceController::sourceSurfaceColor() const
{
    if (context_.rootWidget == nullptr) {
        return QColor();
    }

    if (context_.blocksModeActive != nullptr
        && *context_.blocksModeActive
        && context_.blockCanvasView != nullptr) {
        const QColor blocksSurface = context_.blockCanvasView->backgroundBrush().color();
        if (blocksSurface.isValid()) {
            return blocksSurface;
        }
        const QColor blocksBase = context_.blockCanvasView->palette().color(QPalette::Base);
        if (blocksBase.isValid()) {
            return blocksBase;
        }
    }

    if (context_.editor != nullptr) {
        const QColor editorBase = context_.editor->palette().color(QPalette::Base);
        if (editorBase.isValid()) {
            return editorBase;
        }
        const QColor editorWindow = context_.editor->palette().color(QPalette::Window);
        if (editorWindow.isValid()) {
            return editorWindow;
        }
    }

    const QColor widgetBase = context_.rootWidget->palette().color(QPalette::Base);
    if (widgetBase.isValid()) {
        return widgetBase;
    }
    return context_.rootWidget->palette().color(QPalette::Window);
}

void TextEditorAppearanceController::handleApplicationAppearanceChanged()
{
    if (context_.rootWidget == nullptr) {
        return;
    }

    if (context_.blockCanvasView != nullptr) {
        context_.blockCanvasView->setBackgroundBrush(context_.blockCanvasView->palette().color(QPalette::Base));
        if (QWidget *viewport = context_.blockCanvasView->viewport(); viewport != nullptr) {
            viewport->update();
        }
    }

    QPalette textSurfacePalette = context_.editor != nullptr ? context_.editor->palette() : context_.rootWidget->palette();
    const QColor sourceSurface = sourceSurfaceColor();
    textSurfacePalette.setColor(QPalette::Window, sourceSurface);
    textSurfacePalette.setColor(QPalette::Base, sourceSurface);
    textSurfacePalette.setColor(QPalette::AlternateBase, sourceSurface);
    syncPanelSurfaceToPalette(context_.helpPanel, textSurfacePalette);
    syncPanelSurfaceToPalette(context_.blockDetailsPanel, textSurfacePalette);
    syncPanelSurfaceToPalette(context_.blockDetailsEditPanel, textSurfacePalette);
    syncPanelSurfaceToPalette(context_.blockDetailsHelpPanel, textSurfacePalette);
    syncTextBrowserSurfaceToPalette(context_.helpBrowser, textSurfacePalette);
    syncTextBrowserSurfaceToPalette(context_.blockDetailsHelpBrowser, textSurfacePalette);
    if (context_.updateContextHelp) {
        context_.updateContextHelp();
    }
    if (context_.updateBlockDetailsHelpForCurrentFocus) {
        context_.updateBlockDetailsHelpForCurrentFocus();
    }

    if (context_.blocksModeActive != nullptr
        && *context_.blocksModeActive
        && context_.blockCanvasScene != nullptr
        && context_.rebuildBlocksCanvasFromText) {
        context_.rebuildBlocksCanvasFromText();
    }
}
}
