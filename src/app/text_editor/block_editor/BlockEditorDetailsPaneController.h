#pragma once

#include <QString>

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
struct BlockEditorDetailsPaneContext
{
    bool *tearingDown = nullptr;
    QWidget *editPanel = nullptr;
    QLabel *statusLabel = nullptr;
    QLineEdit *idEdit = nullptr;
    QLineEdit *additionalPositionalEdit = nullptr;
    QStackedWidget *secondaryFieldStack = nullptr;
    QLineEdit *commentEdit = nullptr;
    QLabel *primaryFieldLabel = nullptr;
    QLabel *secondaryFieldLabel = nullptr;
    QLabel *commentFieldLabel = nullptr;
    QTableWidget *optionsTable = nullptr;
    QLabel *optionArgsLabel = nullptr;
    QWidget *optionArgsPanel = nullptr;
    QPushButton *addOptionButton = nullptr;
    QPushButton *removeOptionButton = nullptr;
    QPushButton *applyButton = nullptr;
    QPushButton *legacyConfigureButton = nullptr;
    QTextBrowser *helpBrowser = nullptr;
    std::function<void(bool)> resetDetailsState;
    std::function<void(bool)> setDetailsPopulating;
    std::function<void()> clearReadingsTagEditor;
    std::function<void()> refreshOptionArgumentEditors;
    std::function<QString(const char *)> translate;
};

class BlockEditorDetailsPaneController final
{
public:
    explicit BlockEditorDetailsPaneController(BlockEditorDetailsPaneContext context);

    void clearDetailsPane();

private:
    BlockEditorDetailsPaneContext context_;
};
}
