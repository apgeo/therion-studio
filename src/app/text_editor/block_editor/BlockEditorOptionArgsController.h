#pragma once

#include <functional>

#include <QList>
#include <QString>

class QFormLayout;
class QLabel;
class QLineEdit;
class QObject;
class QTableWidget;
class QWidget;

namespace TherionStudio
{
struct TextEditorCommandMetadata;

struct BlockEditorOptionArgsContext
{
    QObject *signalContext = nullptr;
    QLabel *optionArgsLabel = nullptr;
    QWidget *optionArgsPanel = nullptr;
    QFormLayout *optionArgsFormLayout = nullptr;
    QTableWidget *optionsTable = nullptr;
    QList<QLineEdit *> *optionArgEditors = nullptr;
    bool *optionArgsSyncing = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
    std::function<bool()> isStructuredOptionsMode;
    std::function<QString()> selectedKind;
    std::function<QString(const QString &)> normalizeDirectiveToken;
    std::function<void()> refreshApplyState;
    std::function<void()> updateHelpForCurrentFocus;
    std::function<QString(const char *)> translate;
};

class BlockEditorOptionArgsController final
{
public:
    explicit BlockEditorOptionArgsController(BlockEditorOptionArgsContext context);

    void refreshOptionArgumentEditors();

private:
    QString tr(const char *text) const;
    bool hasRequiredContext() const;
    void clearEditors();
    void setArgumentEditorsVisible(bool visible);

    BlockEditorOptionArgsContext context_;
};
}
