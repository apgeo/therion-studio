#pragma once

#include "BlockEditorSourceController.h"

#include <functional>

#include <QString>

class QLabel;

namespace TherionStudio
{
struct BlockEditorApplyExecutorContext
{
    bool *tearingDown = nullptr;
    bool *detailsPopulating = nullptr;
    int *selectedLineNumber = nullptr;
    QString *baseStatusText = nullptr;
    QLabel *statusLabel = nullptr;
    std::function<BlockEditorSourceContext()> sourceContext;
    std::function<bool(QString *, QString *)> buildUpdatedLine;
    std::function<void(int)> selectBlockInCanvasAndDetails;
    std::function<void()> refreshApplyState;
};

class BlockEditorApplyExecutor final
{
public:
    explicit BlockEditorApplyExecutor(BlockEditorApplyExecutorContext context);

    void applyChanges();

private:
    BlockEditorApplyExecutorContext context_;
};
}
