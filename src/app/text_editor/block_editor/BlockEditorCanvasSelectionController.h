#pragma once

#include <QString>

class QGraphicsItem;
class QGraphicsScene;
class QGraphicsView;

#include <functional>

namespace TherionStudio
{
struct BlockEditorCanvasSelectionContext
{
    bool *tearingDown = nullptr;
    QGraphicsScene *scene = nullptr;
    QGraphicsView *view = nullptr;
    std::function<void()> clearDetailsPane;
    std::function<bool(const QString &, int)> loadDetailsForSelection;
    std::function<void(int, const QString &)> setDetailsSelectionFallback;
};

class BlockEditorCanvasSelectionController final
{
public:
    explicit BlockEditorCanvasSelectionController(BlockEditorCanvasSelectionContext context);

    void selectBlockInCanvasAndDetails(int lineNumber);
    void refreshDetailsSelectionFromScene();
    QGraphicsItem *resolveBlockCanvasItem(QGraphicsItem *item) const;
    int blockCanvasItemLineNumber(const QGraphicsItem *item) const;
    QString blockCanvasItemKind(const QGraphicsItem *item) const;
    void selectBlockCanvasItem(QGraphicsItem *item, bool centerView);

private:
    BlockEditorCanvasSelectionContext context_;
};
}
