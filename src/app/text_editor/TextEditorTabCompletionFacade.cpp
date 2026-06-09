#include "TextEditorTab.h"

#include <QApplication>
#include <QEvent>
#include <QHelpEvent>
#include <QPlainTextEdit>
#include <QWidget>

#include "TextEditorContextHelpController.h"
#include "TextEditorStatusController.h"
#include "block_editor/BlockEditorDirectiveRules.h"
#include "raw_editor/RawEditorCompletionController.h"

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

bool TextEditorTab::eventFilter(QObject *watched, QEvent *event)
{
    if (event != nullptr && watched == qApp) {
        switch (event->type()) {
        case QEvent::ApplicationPaletteChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            handleApplicationAppearanceChanged();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(watched, event);
    }

    if (rawEditorCompletionController_ == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (event != nullptr
        && event->type() == QEvent::ToolTip
        && editor_ != nullptr
        && watched == editor_->viewport()
        && contextHelpController_ != nullptr) {
        auto *helpEvent = static_cast<QHelpEvent *>(event);
        if (contextHelpController_->showValidationTooltipForPosition(helpEvent->pos(), helpEvent->globalPos())) {
            return true;
        }
    }

    switch (rawEditorCompletionController_->handleEventFilter(watched, event)) {
    case RawEditorCompletionController::EventHandling::Consumed:
        return true;
    case RawEditorCompletionController::EventHandling::PassToBase:
        return QWidget::eventFilter(watched, event);
    case RawEditorCompletionController::EventHandling::NotHandled:
    default:
        return QWidget::eventFilter(watched, event);
    }
}

QString TextEditorTab::normalizeCompletionContext(const QString &contextToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return normalizedCompletionContextToken(contextToken);
    }
    return rawEditorCompletionController_->normalizeCompletionContext(contextToken);
}

bool TextEditorTab::isCompatibleChildKindForBlocks(const QString &parentKind, const QString &childKind) const
{
    if (BlockEditorDirectiveRules::isMapObjectReferenceKind(childKind)) {
        return normalizeDirective(parentKind) == QStringLiteral("map");
    }
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->isCompatibleChildKindForBlocks(parentKind, childKind);
}

bool TextEditorTab::isCommandDirectiveInScope(const QString &directive, const QString &scopeToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->isCommandDirectiveInScope(directive, scopeToken);
}

QStringList TextEditorTab::commandArgumentSignaturesFor(const QString &commandToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return {};
    }
    return rawEditorCompletionController_->commandArgumentSignaturesFor(commandToken);
}

bool TextEditorTab::commandHasRequiredIdArgument(const QString &commandToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->commandHasRequiredIdArgument(commandToken);
}

bool TextEditorTab::commandSupportsInlineIdField(const QString &commandToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->commandSupportsInlineIdField(commandToken);
}

QString TextEditorTab::resolveScopeForCommandAtLine(const QString &commandToken,
                                                    const QStringList &lines,
                                                    int lineNumber) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QString();
    }
    return rawEditorCompletionController_->resolveScopeForCommandAtLine(commandToken, lines, lineNumber);
}

QString TextEditorTab::currentCompletionCommand() const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QString();
    }
    return rawEditorCompletionController_->currentCompletionCommand();
}

void TextEditorTab::insertCompletionToken(const QString &completion)
{
    if (rawEditorCompletionController_ == nullptr) {
        return;
    }
    rawEditorCompletionController_->insertCompletionToken(completion);
}

QString TextEditorTab::normalizedDirectiveToken(const QString &directive) const
{
    return normalizeDirective(directive);
}

QString TextEditorTab::openingDirectiveForClosingToken(const QString &directive) const
{
    return completionOpeningDirectiveForClosing(directive);
}

QString TextEditorTab::closingDirectiveForOpeningToken(const QString &directive) const
{
    return completionClosingDirectiveForOpening(directive);
}

bool TextEditorTab::isContainerDirectiveInstanceForParsedLine(const QString &directive,
                                                              const TherionParsedLine &parsedLine) const
{
    return isContainerDirectiveInstance(directive, parsedLine);
}

void TextEditorTab::updateValidationTooltipForCursor()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->updateValidationTooltipForCursor();
    }
}

void TextEditorTab::updateContextHelp()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->updateContextHelp();
    }
}

void TextEditorTab::setHelpCollapsed(bool collapsed)
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->setHelpCollapsed(collapsed);
    }
}

QString TextEditorTab::displayPath() const
{
    return statusController_ != nullptr ? statusController_->displayPath() : tr("No file open");
}

}
