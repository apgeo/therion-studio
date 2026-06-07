#include "../src/app/text_editor/TextEditorCommandMetadata.h"
#include "../src/app/text_editor/block_editor/BlockEditorDirectiveRules.h"
#include "../src/app/text_editor/block_editor/BlockEditorToolboxController.h"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>

#include <iostream>

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

QListWidgetItem *findToolboxCommandItem(QListWidget &toolboxList, const QString &commandToken)
{
    const QString normalizedToken = commandToken.trimmed().toLower();
    for (int row = 0; row < toolboxList.count(); ++row) {
        QListWidgetItem *item = toolboxList.item(row);
        if (item == nullptr) {
            continue;
        }
        if (item->data(Qt::UserRole).toString().trimmed().toLower() == normalizedToken) {
            return item;
        }
    }
    return nullptr;
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    TherionStudio::TextEditorCommandMetadata metadata;
    metadata.contextCommandTokens.insert(QStringLiteral("centerline"),
                                         {QStringLiteral("data"),
                                          QStringLiteral("extend"),
                                          QStringLiteral("team")});

    QLineEdit filterEdit;
    QListWidget toolboxList;
    QComboBox scopeCombo;
    scopeCombo.addItem(QStringLiteral("Inside Centerline"), QStringLiteral("centerline"));
    scopeCombo.setCurrentIndex(0);

    QLineEdit *filterEditPtr = &filterEdit;
    QListWidget *toolboxListPtr = &toolboxList;
    QComboBox *scopeComboPtr = &scopeCombo;
    bool tearingDown = false;

    TherionStudio::BlockEditorToolboxContext context;
    context.tearingDown = &tearingDown;
    context.filterEdit = &filterEditPtr;
    context.toolboxList = &toolboxListPtr;
    context.scopeCombo = &scopeComboPtr;
    context.commandMetadata = &metadata;
    context.normalizeCompletionContext = [](const QString &contextToken) {
        return TherionStudio::BlockEditorDirectiveRules::normalizedCompletionContextToken(contextToken);
    };
    context.isCompatibleChildKindForBlocks = [](const QString &, const QString &) {
        return true;
    };

    TherionStudio::BlockEditorToolboxController controller(context);
    controller.populateBlockToolbox();

    bool ok = true;
    ok &= expect(findToolboxCommandItem(toolboxList, QStringLiteral("data")) != nullptr,
                 "centerline toolbox should still show data.");
    ok &= expect(findToolboxCommandItem(toolboxList, QStringLiteral("team")) != nullptr,
                 "centerline toolbox should still show regular centerline commands.");
    ok &= expect(findToolboxCommandItem(toolboxList, QStringLiteral("extend")) == nullptr,
                 "centerline toolbox should hide data-body-only extend.");

    return ok ? 0 : 1;
}
