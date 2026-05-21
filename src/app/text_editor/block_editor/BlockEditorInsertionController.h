#pragma once

#include <QPointF>
#include <QString>

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorInsertionController final
{
public:
    explicit BlockEditorInsertionController(TextEditorTab *owner);

    void insertFromDrop(const QString &kind, const QPointF &scenePos);

private:
    TextEditorTab *owner_ = nullptr;
};
}
