#pragma once

#include <QPointF>

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorMovePreviewController final
{
public:
    explicit BlockEditorMovePreviewController(TextEditorTab *owner);

    void updateMovePreview(int sourceLineNumber, const QPointF &scenePos);
    void clearMovePreview();

private:
    TextEditorTab *owner_ = nullptr;
};
}
