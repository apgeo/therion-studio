#pragma once

#include "BlockEditorSourceController.h"

#include <functional>

#include <QString>

class QLabel;

namespace TherionStudio
{
struct BlockEditorApplyStateContext
{
    bool *tearingDown = nullptr;
    bool *detailsPopulating = nullptr;
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
    QLabel *statusLabel() const;

    BlockEditorApplyStateContext context_;
};
}
