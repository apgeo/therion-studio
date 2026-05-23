#pragma once

#include "BlockEditorSourceController.h"

#include <functional>

#include <QString>

class QLabel;
class QPushButton;

namespace TherionStudio
{
struct BlockEditorApplyStateContext
{
    bool *tearingDown = nullptr;
    bool *detailsPopulating = nullptr;
    QPushButton **applyButton = nullptr;
    QLabel **statusLabel = nullptr;
    int *selectedLineNumber = nullptr;
    QString *baseStatusText = nullptr;
    std::function<BlockEditorSourceContext()> sourceContext;
    std::function<bool(QString *, QString *)> buildUpdatedLine;
};

class BlockEditorApplyStateController final
{
public:
    explicit BlockEditorApplyStateController(BlockEditorApplyStateContext context);

    void refreshApplyState();

private:
    QPushButton *applyButton() const;
    QLabel *statusLabel() const;

    BlockEditorApplyStateContext context_;
};
}
