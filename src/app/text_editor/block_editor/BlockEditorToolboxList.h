#pragma once

#include <QListWidget>

namespace TherionStudio
{
class BlockEditorToolboxList final : public QListWidget
{
public:
    explicit BlockEditorToolboxList(QWidget *parent = nullptr);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
};
}
