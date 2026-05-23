#pragma once

#include "BlockEditorSourceController.h"

#include <functional>

#include <QChar>
#include <QString>

class QLineEdit;
class QTableWidget;

namespace TherionStudio
{
struct TextEditorCommandMetadata;

struct BlockEditorLineBuildContext
{
    int *selectedLineNumber = nullptr;
    QString *selectedKind = nullptr;
    QLineEdit *idEdit = nullptr;
    QLineEdit *additionalPositionalEdit = nullptr;
    QLineEdit *commentEdit = nullptr;
    QTableWidget *optionsTable = nullptr;
    QChar *commentMarker = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
    std::function<BlockEditorSourceContext()> sourceContext;
    std::function<QString()> readingsOrderTextForBuild;
    std::function<QString(const QString &)> normalizeDirectiveToken;
    std::function<bool(const QString &)> commandHasRequiredIdArgument;
    std::function<bool()> isSimpleValueMode;
    std::function<bool()> isDataHeaderMode;
    std::function<bool()> isStructuredOptionsMode;
    std::function<QString(const char *)> translate;
};

class BlockEditorLineBuildService final
{
public:
    explicit BlockEditorLineBuildService(BlockEditorLineBuildContext context);

    bool buildUpdatedLine(QString *updatedLine, QString *validationError) const;

private:
    QString tr(const char *text) const;
    bool hasRequiredContext() const;

    BlockEditorLineBuildContext context_;
};
}
