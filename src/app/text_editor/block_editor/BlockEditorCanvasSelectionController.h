#pragma once

#include <QString>

class QGraphicsItem;

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorCanvasSelectionController final
{
public:
    explicit BlockEditorCanvasSelectionController(TextEditorTab *owner);

    void selectBlockInCanvasAndDetails(int lineNumber);
    void refreshDetailsSelectionFromScene();
    QGraphicsItem *resolveBlockCanvasItem(QGraphicsItem *item) const;
    int blockCanvasItemLineNumber(const QGraphicsItem *item) const;
    QString blockCanvasItemKind(const QGraphicsItem *item) const;
    void selectBlockCanvasItem(QGraphicsItem *item, bool centerView);

private:
    TextEditorTab *owner_ = nullptr;
};
}
