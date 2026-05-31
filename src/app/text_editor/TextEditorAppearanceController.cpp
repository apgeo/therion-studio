#include "TextEditorAppearanceController.h"

#include "block_editor/BlockEditorCanvasItem.h"
#include "TextEditorSurfaceStyler.h"

#include <QApplication>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QWidget>

#include <QHash>
#include <utility>

namespace TherionStudio
{
namespace
{
void refreshBlockCanvasSceneAppearance(QGraphicsScene *scene)
{
    if (scene == nullptr) {
        return;
    }

    QHash<int, QString> blockKindByLine;
    for (QGraphicsItem *item : scene->items()) {
        if (auto *blockItem = dynamic_cast<BlockCanvasItem *>(item)) {
            blockKindByLine.insert(blockItem->lineNumber(), blockItem->kind());
        }
    }

    const QPalette palette = QApplication::palette();
    QPen connectorPen(palette.color(QPalette::Mid));
    connectorPen.setWidthF(1.2);
    connectorPen.setStyle(Qt::SolidLine);

    for (QGraphicsItem *item : scene->items()) {
        auto *lineItem = qgraphicsitem_cast<QGraphicsLineItem *>(item);
        if (lineItem == nullptr) {
            continue;
        }

        if (qFuzzyCompare(lineItem->zValue(), -100.0)) {
            lineItem->setPen(connectorPen);
            continue;
        }

        if (!qFuzzyCompare(lineItem->zValue(), -99.0)) {
            continue;
        }

        bool ok = false;
        const int sourceLine = lineItem->data(kBlockEditorCanvasEndHintContainerLineDataRole).toInt(&ok);
        if (!ok) {
            continue;
        }

        const QColor baseColor = blockEditorCanvasBaseColorForDirective(blockKindByLine.value(sourceLine));
        QColor closeColor = baseColor.lightnessF() < 0.5
            ? baseColor.lighter(155)
            : baseColor.darker(130);
        closeColor.setAlpha(245);
        QPen closePen(closeColor);
        closePen.setWidthF(3.4);
        closePen.setStyle(Qt::SolidLine);
        lineItem->setPen(closePen);
    }

    scene->update();
}
}

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
        && context_.blockCanvasScene != nullptr) {
        refreshBlockCanvasSceneAppearance(context_.blockCanvasScene);
    }
}
}
