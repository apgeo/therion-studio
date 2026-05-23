#pragma once

#include <QString>

class QGraphicsScene;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTextBrowser;
class QWidget;

#include <functional>

namespace TherionStudio
{
struct TextEditorCommandMetadata;

struct BlockEditorToolboxDetailsContext
{
    bool *tearingDown = nullptr;
    QGraphicsScene *scene = nullptr;
    QWidget *editPanel = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *primaryFieldLabel = nullptr;
    QLabel *secondaryFieldLabel = nullptr;
    QLabel *commentFieldLabel = nullptr;
    QLineEdit *idEdit = nullptr;
    QLineEdit *additionalPositionalEdit = nullptr;
    QStackedWidget *secondaryFieldStack = nullptr;
    QLineEdit *commentEdit = nullptr;
    QTableWidget *optionsTable = nullptr;
    QPushButton *addOptionButton = nullptr;
    QPushButton *removeOptionButton = nullptr;
    QLabel *optionArgsLabel = nullptr;
    QWidget *optionArgsPanel = nullptr;
    QPushButton *legacyConfigureButton = nullptr;
    QPushButton *applyButton = nullptr;
    QTextBrowser *helpBrowser = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
    std::function<QString(const QString &)> normalizeDirectiveToken;
    std::function<void(const QString &, const QString &)> beginToolboxCommandDetails;
    std::function<void(bool)> setDetailsPopulating;
    std::function<void()> refreshOptionArgumentEditors;
    std::function<QString(const char *)> translate;
};

class BlockEditorToolboxDetailsController final
{
public:
    explicit BlockEditorToolboxDetailsController(BlockEditorToolboxDetailsContext context);

    void showToolboxCommandDetails(const QString &commandToken);

private:
    BlockEditorToolboxDetailsContext context_;
};
}
