#include "TextEditorAppearanceController.h"

#include "TextEditorSurfaceStyler.h"
#include "TextEditorTab.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPalette>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QWidget>

namespace TherionStudio
{
TextEditorAppearanceController::TextEditorAppearanceController(TextEditorTab *owner)
    : owner_(owner)
{
}

QColor TextEditorAppearanceController::sourceSurfaceColor() const
{
    if (owner_ == nullptr) {
        return QColor();
    }

    if (owner_->blocksModeActive_ && owner_->blockCanvasView_ != nullptr) {
        const QColor blocksSurface = owner_->blockCanvasView_->backgroundBrush().color();
        if (blocksSurface.isValid()) {
            return blocksSurface;
        }
        const QColor blocksBase = owner_->blockCanvasView_->palette().color(QPalette::Base);
        if (blocksBase.isValid()) {
            return blocksBase;
        }
    }

    if (owner_->editor_ != nullptr) {
        const QColor editorBase = owner_->editor_->palette().color(QPalette::Base);
        if (editorBase.isValid()) {
            return editorBase;
        }
        const QColor editorWindow = owner_->editor_->palette().color(QPalette::Window);
        if (editorWindow.isValid()) {
            return editorWindow;
        }
    }

    const QColor widgetBase = owner_->palette().color(QPalette::Base);
    if (widgetBase.isValid()) {
        return widgetBase;
    }
    return owner_->palette().color(QPalette::Window);
}

void TextEditorAppearanceController::handleApplicationAppearanceChanged()
{
    if (owner_ == nullptr) {
        return;
    }

    if (owner_->blockCanvasView_ != nullptr) {
        owner_->blockCanvasView_->setBackgroundBrush(owner_->blockCanvasView_->palette().color(QPalette::Base));
        if (QWidget *viewport = owner_->blockCanvasView_->viewport(); viewport != nullptr) {
            viewport->update();
        }
    }

    QPalette textSurfacePalette = owner_->editor_ != nullptr ? owner_->editor_->palette() : owner_->palette();
    const QColor sourceSurface = sourceSurfaceColor();
    textSurfacePalette.setColor(QPalette::Window, sourceSurface);
    textSurfacePalette.setColor(QPalette::Base, sourceSurface);
    textSurfacePalette.setColor(QPalette::AlternateBase, sourceSurface);
    syncPanelSurfaceToPalette(owner_->helpPanel_, textSurfacePalette);
    syncPanelSurfaceToPalette(owner_->blockDetailsPanel_, textSurfacePalette);
    syncPanelSurfaceToPalette(owner_->blockDetailsEditPanel_, textSurfacePalette);
    syncPanelSurfaceToPalette(owner_->blockDetailsHelpPanel_, textSurfacePalette);
    syncTextBrowserSurfaceToPalette(owner_->helpBrowser_, textSurfacePalette);
    syncTextBrowserSurfaceToPalette(owner_->blockDetailsHelpBrowser_, textSurfacePalette);
    owner_->updateContextHelp();
    owner_->updateBlockDetailsHelpForCurrentFocus();

    if (owner_->blocksModeActive_ && owner_->blockCanvasScene_ != nullptr) {
        owner_->rebuildBlocksCanvasFromText();
    }
}
}
