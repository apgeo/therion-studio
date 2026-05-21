#include "BlockEditorToolboxController.h"

#include "BlockEditorCanvasItem.h"
#include "BlockEditorDirectiveRules.h"
#include "BlockEditorSourceText.h"
#include "../TextEditorTab.h"

#include "../../../core/TherionDocumentParser.h"

#include <QComboBox>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QStringList>

#include <algorithm>

namespace
{
using namespace TherionStudio::BlockEditorDirectiveRules;

void appendUnique(QStringList &target, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty() || target.contains(trimmed, Qt::CaseInsensitive)) {
        return;
    }
    target.append(trimmed);
}

void appendUniqueList(QStringList &target, const QStringList &values)
{
    for (const QString &value : values) {
        appendUnique(target, value);
    }
}
}

namespace TherionStudio
{
BlockEditorToolboxController::BlockEditorToolboxController(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorToolboxController::populateBlockToolbox()
{
    if (owner_ == nullptr || owner_->tearingDown_ || owner_->blockToolboxList_ == nullptr) {
        return;
    }

    owner_->blockToolboxList_->clear();
    const QString filterText = owner_->blockToolboxFilterEdit_ != nullptr
        ? owner_->blockToolboxFilterEdit_->text().trimmed().toLower()
        : QString();

    auto labelForCommand = [](const QString &commandToken) {
        QStringList parts = commandToken.split(QLatin1Char('-'), Qt::SkipEmptyParts);
        for (QString &part : parts) {
            if (!part.isEmpty()) {
                part[0] = part.at(0).toUpper();
            }
        }
        return parts.join(QLatin1Char(' '));
    };

    QString selectedScope = QStringLiteral("all");
    if (owner_->blockToolboxScopeCombo_ != nullptr) {
        selectedScope = owner_->blockToolboxScopeCombo_->currentData().toString().trimmed().toLower();
    }

    QString effectiveScope = selectedScope;
    if (effectiveScope == QStringLiteral("__auto__")) {
        effectiveScope = selectedBlockInsertionContextToken();
    }
    if (effectiveScope != QStringLiteral("all")) {
        effectiveScope = owner_->normalizeCompletionContext(effectiveScope);
        if (effectiveScope.isEmpty()) {
            effectiveScope = QStringLiteral("none");
        }
    }

    QStringList contextsToShow;
    if (effectiveScope == QStringLiteral("all")) {
        appendUnique(contextsToShow, QStringLiteral("none"));
        QStringList remainingContexts;
        for (auto it = owner_->commandMetadata().contextCommandTokens.cbegin(); it != owner_->commandMetadata().contextCommandTokens.cend(); ++it) {
            const QString normalizedContext = owner_->normalizeCompletionContext(it.key());
            if (normalizedContext.isEmpty()
                || normalizedContext == QStringLiteral("all")
                || normalizedContext == QStringLiteral("none")) {
                continue;
            }
            appendUnique(remainingContexts, normalizedContext);
        }
        std::sort(remainingContexts.begin(), remainingContexts.end(), [](const QString &left, const QString &right) {
            return QString::compare(contextDisplayLabel(left), contextDisplayLabel(right), Qt::CaseInsensitive) < 0;
        });
        appendUniqueList(contextsToShow, remainingContexts);
    } else {
        appendUnique(contextsToShow, effectiveScope);
    }

    contextsToShow.erase(std::remove_if(contextsToShow.begin(),
                                        contextsToShow.end(),
                                        [this](const QString &contextToken) {
                                            QStringList candidates = owner_->commandMetadata().contextCommandTokens.value(contextToken);
                                            if (contextToken == QStringLiteral("none")) {
                                                appendUniqueList(candidates, owner_->commandMetadata().contextCommandTokens.value(QStringLiteral("none")));
                                            }
                                            appendUniqueList(candidates, owner_->commandMetadata().contextCommandTokens.value(QStringLiteral("all")));
                                            return candidates.isEmpty();
                                        }),
                         contextsToShow.end());

    int insertedRows = 0;
    for (const QString &contextToken : contextsToShow) {
        QStringList sectionCommands = owner_->commandMetadata().contextCommandTokens.value(contextToken);
        if (contextToken == QStringLiteral("none")) {
            appendUniqueList(sectionCommands, owner_->commandMetadata().contextCommandTokens.value(QStringLiteral("none")));
        }
        appendUniqueList(sectionCommands, owner_->commandMetadata().contextCommandTokens.value(QStringLiteral("all")));
        appendUnique(sectionCommands, QStringLiteral("comment"));

        QStringList visibleCommands;
        for (const QString &command : sectionCommands) {
            const QString normalizedCommand = normalizeDirective(command);
            if (normalizedCommand.isEmpty()) {
                continue;
            }
            if (normalizedCommand == QStringLiteral("all")
                || normalizedCommand == QStringLiteral("none")
                || normalizedCommand == QStringLiteral("encoding")
                || normalizedCommand.startsWith(QStringLiteral("end"))) {
                continue;
            }
            const QString parentKind = contextToken == QStringLiteral("none") ? QString() : contextToken;
            if (!owner_->isCompatibleChildKindForBlocks(parentKind, normalizedCommand)) {
                continue;
            }
            const QString closingDirective = completionClosingDirectiveForOpening(normalizedCommand);
            const bool unsupportedContainer = !closingDirective.isEmpty()
                && !isContainerBlockDirective(normalizedCommand);
            if (unsupportedContainer) {
                continue;
            }

            const QString label = labelForCommand(normalizedCommand);
            const QString searchable = normalizedCommand + QStringLiteral(" ") + label.toLower();
            if (!filterText.isEmpty() && !searchable.contains(filterText)) {
                continue;
            }
            appendUnique(visibleCommands, normalizedCommand);
        }

        std::sort(visibleCommands.begin(), visibleCommands.end(), [](const QString &a, const QString &b) {
            return QString::compare(a, b, Qt::CaseInsensitive) < 0;
        });

        if (visibleCommands.isEmpty()) {
            continue;
        }

        auto *categoryItem =
            new QListWidgetItem(QStringLiteral("[%1]").arg(contextDisplayLabel(contextToken)), owner_->blockToolboxList_);
        categoryItem->setFlags(Qt::ItemIsEnabled);
        ++insertedRows;

        for (const QString &commandToken : visibleCommands) {
            const QString label = labelForCommand(commandToken);
            auto *item = new QListWidgetItem(QStringLiteral("  %1").arg(label), owner_->blockToolboxList_);
            item->setData(Qt::UserRole, commandToken);
            item->setToolTip(TextEditorTab::tr("Drag to canvas."));
            ++insertedRows;
        }
    }

    if (insertedRows == 0) {
        auto *emptyItem = new QListWidgetItem(TextEditorTab::tr("[No commands match filter]"), owner_->blockToolboxList_);
        emptyItem->setFlags(Qt::ItemIsEnabled);
    }

    updateBlockToolboxScopeLabel();
}

void BlockEditorToolboxController::populateBlockToolboxScopeCombo()
{
    if (owner_ == nullptr || owner_->blockToolboxScopeCombo_ == nullptr) {
        return;
    }

    const QString previousScope = owner_->blockToolboxScopeCombo_->currentData().toString().trimmed().toLower();
    const QSignalBlocker scopeSignalBlocker(owner_->blockToolboxScopeCombo_);
    owner_->blockToolboxScopeCombo_->clear();
    owner_->blockToolboxScopeCombo_->addItem(TextEditorTab::tr("Auto (selected block)"), QStringLiteral("__auto__"));
    owner_->blockToolboxScopeCombo_->addItem(TextEditorTab::tr("All"), QStringLiteral("all"));
    owner_->blockToolboxScopeCombo_->addItem(TextEditorTab::tr("Top-level"), QStringLiteral("none"));

    QStringList dynamicContexts;
    for (auto contextIterator = owner_->commandMetadata().contextCommandTokens.cbegin(); contextIterator != owner_->commandMetadata().contextCommandTokens.cend(); ++contextIterator) {
        const QString normalizedContext = owner_->normalizeCompletionContext(contextIterator.key());
        if (normalizedContext.isEmpty()
            || normalizedContext == QStringLiteral("all")
            || normalizedContext == QStringLiteral("none")) {
            continue;
        }
        appendUnique(dynamicContexts, normalizedContext);
    }
    std::sort(dynamicContexts.begin(), dynamicContexts.end(), [this](const QString &left, const QString &right) {
        return QString::compare(contextDisplayLabel(left), contextDisplayLabel(right), Qt::CaseInsensitive) < 0;
    });

    for (const QString &contextToken : dynamicContexts) {
        owner_->blockToolboxScopeCombo_->addItem(contextDisplayLabel(contextToken), contextToken);
    }

    int restoreIndex = 0;
    if (!previousScope.isEmpty()) {
        restoreIndex = owner_->blockToolboxScopeCombo_->findData(previousScope);
    }
    if (restoreIndex < 0) {
        restoreIndex = 0;
    }
    owner_->blockToolboxScopeCombo_->setCurrentIndex(restoreIndex);
}

QString BlockEditorToolboxController::selectedBlockInsertionContextToken() const
{
    if (owner_ == nullptr || owner_->blockCanvasScene_ == nullptr) {
        return QStringLiteral("none");
    }

    auto resolveBlockFromItem = [](QGraphicsItem *item) -> BlockCanvasItem * {
        while (item != nullptr) {
            if (auto *blockItem = dynamic_cast<BlockCanvasItem *>(item)) {
                return blockItem;
            }
            item = item->parentItem();
        }
        return nullptr;
    };
    BlockCanvasItem *selectedBlock = nullptr;
    if (!owner_->blockCanvasScene_->selectedItems().isEmpty()) {
        selectedBlock = resolveBlockFromItem(owner_->blockCanvasScene_->selectedItems().first());
    }
    if (selectedBlock == nullptr) {
        selectedBlock = resolveBlockFromItem(owner_->blockCanvasScene_->focusItem());
    }
    if (selectedBlock == nullptr) {
        return QStringLiteral("none");
    }

    const QString selectedKind = normalizeDirective(selectedBlock->kind());
    const QString selectedContext = owner_->normalizeCompletionContext(selectedKind);
    if (selectedBlock->isContainerOpen() && !selectedContext.isEmpty()) {
        return selectedContext;
    }

    if (owner_->editor_ == nullptr || selectedBlock->lineNumber() <= 0) {
        return QStringLiteral("none");
    }

    QStringList stack;
    const QStringList lines = blockEditorNormalizedSourceLines(owner_->editor_->toPlainText());
    const int lastLine = qMin(selectedBlock->lineNumber() - 1, lines.size());
    for (int lineIndex = 0; lineIndex < lastLine; ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        const QString directive = normalizeDirective(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }

        if (isBlockClosingDirective(directive)) {
            const QString openingDirective = completionOpeningDirectiveForClosing(directive);
            for (int stackIndex = stack.size() - 1; stackIndex >= 0; --stackIndex) {
                if (stack.at(stackIndex) == openingDirective) {
                    stack.removeAt(stackIndex);
                    break;
                }
            }
            continue;
        }

        if (isContainerDirectiveInstance(directive, parsedLine)) {
            stack.append(directive);
        }
    }

    if (stack.isEmpty()) {
        return QStringLiteral("none");
    }

    const QString parentContext = owner_->normalizeCompletionContext(stack.constLast());
    return parentContext.isEmpty() ? QStringLiteral("none") : parentContext;
}

void BlockEditorToolboxController::updateBlockToolboxScopeLabel()
{
    if (owner_ == nullptr || owner_->blockToolboxScopeCombo_ == nullptr) {
        return;
    }

    QString selectedScope = QStringLiteral("all");
    selectedScope = owner_->blockToolboxScopeCombo_->currentData().toString().trimmed().toLower();

    if (selectedScope == QStringLiteral("__auto__")) {
        const QString contextToken = selectedBlockInsertionContextToken();
        owner_->blockToolboxScopeCombo_->setToolTip(TextEditorTab::tr("Auto scope currently resolves to: %1.")
                                                .arg(contextDisplayLabel(contextToken)));
        return;
    }

    if (selectedScope == QStringLiteral("all")) {
        owner_->blockToolboxScopeCombo_->setToolTip(TextEditorTab::tr("Shows commands from all supported contexts."));
        return;
    }

    const QString normalizedScope = owner_->normalizeCompletionContext(selectedScope);
    const QString labelText = contextDisplayLabel(normalizedScope.isEmpty() ? QStringLiteral("none") : normalizedScope);
    owner_->blockToolboxScopeCombo_->setToolTip(TextEditorTab::tr("Shows commands for: %1.").arg(labelText));
}

}
