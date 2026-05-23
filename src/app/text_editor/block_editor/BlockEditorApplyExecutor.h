#pragma once

#include "BlockEditorSourceController.h"

#include <functional>

#include <QString>

class QWidget;

namespace TherionStudio
{
struct BlockEditorApplyExecutorContext
{
    QWidget *dialogParent = nullptr;
    bool *tearingDown = nullptr;
    int *selectedLineNumber = nullptr;
    std::function<BlockEditorSourceContext()> sourceContext;
    std::function<bool(QString *, QString *)> buildUpdatedLine;
    std::function<void(int)> selectBlockInCanvasAndDetails;
    std::function<void()> refreshApplyState;
    std::function<QString(const char *)> translate;
};

class BlockEditorApplyExecutor final
{
public:
    explicit BlockEditorApplyExecutor(BlockEditorApplyExecutorContext context);

    void applyChanges();

private:
    QString tr(const char *text) const;

    BlockEditorApplyExecutorContext context_;
};
}
