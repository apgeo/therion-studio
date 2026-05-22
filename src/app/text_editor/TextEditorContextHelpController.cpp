#include "TextEditorContextHelpController.h"

#include "TextEditorTab.h"
#include "ContextHelpController.h"
#include "TextEditorSurfaceStyler.h"
#include "block_editor/BlockEditorDirectiveRules.h"
#include "raw_editor/RawEditorCompletionController.h"

#include "../../core/TherionCommandSyntax.h"
#include "../../core/TherionDocumentParser.h"

#include <algorithm>
#include <utility>
#include <QFrame>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QRect>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextBlock>
#include <QToolTip>
#include <QVBoxLayout>

namespace
{
constexpr int kHelpPanelPadding = 12;
constexpr int kHelpPanelSpacing = 8;

void appendUniqueCaseInsensitive(QStringList &target, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (target.contains(trimmed, Qt::CaseInsensitive)) {
        return;
    }
    target.append(trimmed);
}

void appendUniqueListCaseInsensitive(QStringList &target, const QStringList &values)
{
    for (const QString &value : values) {
        appendUniqueCaseInsensitive(target, value);
    }
}
}

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

TextEditorContextHelpController::TextEditorContextHelpController(TextEditorTab *owner,
                                                                 CommandCatalogStore catalogStore)
    : owner_(owner)
    , catalogStore_(std::move(catalogStore))
{
}

void TextEditorContextHelpController::buildHelpPanel()
{
    if (owner_ == nullptr) {
        return;
    }

    auto *framedHelpPanel = new QFrame(owner_);
    framedHelpPanel->setFrameShape(QFrame::NoFrame);
    owner_->helpPanel_ = framedHelpPanel;
    owner_->helpPanel_->setObjectName(QStringLiteral("textContextHelpPanel"));
    syncPanelSurfaceToBaseTone(owner_->helpPanel_);
    auto *panelLayout = new QVBoxLayout(owner_->helpPanel_);
    panelLayout->setContentsMargins(kHelpPanelPadding, kHelpPanelPadding, kHelpPanelPadding, kHelpPanelPadding);
    panelLayout->setSpacing(kHelpPanelSpacing);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);

    auto *headerLabel = new QLabel(TextEditorTab::tr("Contextual Help"), owner_->helpPanel_);
    QFont headerFont = headerLabel->font();
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);

    headerRow->addWidget(headerLabel);
    headerRow->addStretch(1);

    owner_->helpBrowser_ = new QTextBrowser(owner_->helpPanel_);
    owner_->helpBrowser_->setFrameShape(QFrame::NoFrame);
    owner_->helpBrowser_->setOpenLinks(false);
    owner_->helpBrowser_->setOpenExternalLinks(false);
    owner_->helpBrowser_->setMinimumHeight(120);
    syncTextBrowserSurfaceToParent(owner_->helpBrowser_);
    owner_->helpBrowser_->setHtml(
        TextEditorTab::tr("<p>Select a Therion command or item to see contextual help.</p>"));

    panelLayout->addLayout(headerRow);
    panelLayout->addWidget(owner_->helpBrowser_, 1);
}

void TextEditorContextHelpController::loadHelpMetadata()
{
    if (owner_ == nullptr) {
        return;
    }

    owner_->mutableCommandMetadata().clear();
    loadHelpMetadataFromCommandCatalog();
    if (owner_->rawEditorCompletionController_ != nullptr) {
        owner_->rawEditorCompletionController_->rebuildCompletionModel();
    }
    owner_->populateBlockToolboxScopeCombo();
}

void TextEditorContextHelpController::loadHelpMetadataFromCommandCatalog()
{
    if (owner_ == nullptr) {
        return;
    }

    resetCatalogBlockDirectiveMetadataToDefaults();

    const QJsonObject catalogObject = catalogStore_.catalogObject();
    if (catalogObject.isEmpty()) {
        return;
    }
    applyCatalogBlockDirectiveMetadata(catalogObject);
    if (owner_->rawEditorCompletionController_ != nullptr) {
        owner_->rawEditorCompletionController_->applyCatalogCommandsMetadata(catalogObject);
    }
}

void TextEditorContextHelpController::setHelpCollapsed(bool collapsed)
{
    if (owner_ == nullptr) {
        return;
    }

    owner_->helpCollapsed_ = collapsed;
    if (owner_->helpBrowser_ != nullptr) {
        owner_->helpBrowser_->setVisible(!collapsed);
    }
    if (owner_->editorHelpSplitter_ != nullptr) {
        if (!collapsed && owner_->helpPanelExtent_ > 0) {
            const QList<int> sizes = owner_->editorHelpSplitter_->sizes();
            owner_->editorHelpSplitter_->setSizes(QList<int>{sizes.value(0, 1), owner_->helpPanelExtent_});
        } else if (collapsed) {
            const QList<int> sizes = owner_->editorHelpSplitter_->sizes();
            if (sizes.size() >= 2) {
                const int minimumExtent = owner_->helpPanel_ != nullptr
                    ? qMax(owner_->helpPanel_->minimumSizeHint().width(), owner_->helpPanel_->minimumWidth())
                    : 1;
                owner_->helpPanelExtent_ = qMax(sizes.at(1), minimumExtent);
            }
            owner_->editorHelpSplitter_->setSizes(QList<int>{1, 0});
        }
    }
}

void TextEditorContextHelpController::updateContextHelp()
{
    if (owner_ == nullptr || owner_->helpBrowser_ == nullptr) {
        return;
    }

    const QString validationHtml = validationHelpHtmlForCursor();
    if (!validationHtml.isEmpty()) {
        owner_->helpBrowser_->setHtml(validationHtml);
        return;
    }

    const QStringList candidates = helpCandidateTokens();
    for (const QString &candidate : candidates) {
        const auto entryIt = owner_->commandMetadata().helpEntries.constFind(candidate.toLower());
        if (entryIt == owner_->commandMetadata().helpEntries.constEnd()) {
            continue;
        }

        const TherionHelpEntry &entry = entryIt.value();
        owner_->helpBrowser_->setHtml(ContextHelpController::renderHelpHtml(candidate,
                                                                             entry.summary,
                                                                             entry.syntax,
                                                                             entry.arguments,
                                                                             entry.acceptedValues,
                                                                             entry.options,
                                                                             true));
        return;
    }

    owner_->helpBrowser_->setHtml(TextEditorTab::tr("<p>No contextual help is available for the current token.</p>"));
}

void TextEditorContextHelpController::updateValidationTooltipForCursor()
{
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return;
    }

    QString tooltipText;
    QString tooltipKey;
    const QString validationHtml = validationHelpHtmlForCursor(&tooltipText, &tooltipKey);
    if (validationHtml.isEmpty() || tooltipText.trimmed().isEmpty()) {
        owner_->editor_->setToolTip(QString());
        owner_->lastValidationTooltipKey_.clear();
        return;
    }

    owner_->editor_->setToolTip(tooltipText);
    if (tooltipKey == owner_->lastValidationTooltipKey_) {
        return;
    }

    owner_->lastValidationTooltipKey_ = tooltipKey;
    QToolTip::showText(owner_->editor_->mapToGlobal(owner_->editor_->cursorRect().bottomLeft()),
                       tooltipText,
                       owner_->editor_,
                       QRect(),
                       2200);
}

QStringList TextEditorContextHelpController::helpCandidateTokens() const
{
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return {};
    }

    QStringList candidates;
    const QString directToken = currentHelpTokenForCursor();
    if (!directToken.isEmpty()) {
        appendUniqueCaseInsensitive(candidates, directToken);
    }

    const QTextCursor cursor = owner_->editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return candidates;
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
    if (!parsedLine.directive.isEmpty() && parsedLine.directive != directToken.toLower()) {
        appendUniqueCaseInsensitive(candidates, parsedLine.directive);
        const QString normalizedDirective = owner_->normalizedDirectiveToken(parsedLine.directive);
        const QString openingDirective = owner_->openingDirectiveForClosingToken(normalizedDirective);
        if (!openingDirective.isEmpty()) {
            appendUniqueCaseInsensitive(candidates, openingDirective);
        }
    }

    const QString resolvedCommand = owner_->currentCompletionCommand();
    if (!resolvedCommand.isEmpty()) {
        appendUniqueCaseInsensitive(candidates, resolvedCommand);
    }

    return candidates;
}

QString TextEditorContextHelpController::currentHelpTokenForCursor() const
{
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return QString();
    }

    const QTextCursor cursor = owner_->editor_->textCursor();
    if (!cursor.block().isValid()) {
        return QString();
    }

    const QTextBlock block = cursor.block();
    const int column = cursor.position() - block.position();
    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);

    for (const TherionParsedToken &tokenSpan : parsedLine.tokenSpans) {
        if (tokenSpan.type == TherionTokenType::Comment) {
            continue;
        }

        const int tokenEnd = tokenSpan.start + tokenSpan.length;
        if (column >= tokenSpan.start && column <= tokenEnd) {
            return tokenSpan.text.toLower();
        }
    }

    return QString();
}

QString TextEditorContextHelpController::validationHelpHtmlForCursor(QString *tooltipText, QString *tooltipKey) const
{
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return QString();
    }

    const QTextCursor cursor = owner_->editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QString();
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString command = owner_->normalizedDirectiveToken(parsedLine.directive.toLower());
    if (command.isEmpty() || !owner_->commandMetadata().commandOptionTokens.contains(command)) {
        return QString();
    }

    const int column = cursor.position() - block.position();
    int tokenIndexAtCursor = -1;
    int tokenCounter = 0;
    for (const TherionParsedToken &tokenSpan : parsedLine.tokenSpans) {
        if (tokenSpan.type == TherionTokenType::Comment) {
            continue;
        }

        const int tokenEnd = tokenSpan.start + tokenSpan.length;
        if (column >= tokenSpan.start && column <= tokenEnd) {
            tokenIndexAtCursor = tokenCounter;
            break;
        }
        ++tokenCounter;
    }

    if (tokenIndexAtCursor <= 0 || tokenIndexAtCursor >= parsedLine.tokens.size()) {
        return QString();
    }

    const QString cursorToken = parsedLine.tokens.at(tokenIndexAtCursor).trimmed();
    const QString normalizedCursorToken = cursorToken.toLower();
    QStringList allowedValues;
    QString detailMessage;
    QString issueKey;

    if (looksLikeOptionToken(cursorToken)) {
        const QStringList knownOptions = owner_->commandMetadata().commandOptionTokens.value(command);
        if (knownOptions.contains(normalizedCursorToken, Qt::CaseInsensitive)) {
            return QString();
        }

        allowedValues = knownOptions;
        std::sort(allowedValues.begin(),
                  allowedValues.end(),
                  [](const QString &a, const QString &b) {
                      return QString::compare(a, b, Qt::CaseInsensitive) < 0;
                  });
        detailMessage = TextEditorTab::tr("Unknown option for command '%1'.").arg(command.toHtmlEscaped());
        issueKey = QStringLiteral("option|%1|%2").arg(command, normalizedCursorToken);
    } else {
        int optionIndex = -1;
        for (int index = tokenIndexAtCursor - 1; index >= 1; --index) {
            const QString token = parsedLine.tokens.at(index).trimmed();
            if (token.startsWith(QLatin1Char('-'))) {
                optionIndex = index;
                break;
            }
        }
        if (optionIndex < 1) {
            return QString();
        }

        const QString optionToken = parsedLine.tokens.at(optionIndex).trimmed().toLower();
        const QString arity = owner_->commandMetadata().commandOptionValueArityTokens
            .value(commandOptionValueKey(command, optionToken))
            .trimmed()
            .toUpper();
        const int valueOrdinal = tokenIndexAtCursor - optionIndex;
        if (canonicalOptionArityToken(arity) == QStringLiteral("EXACTLY_ONE") && valueOrdinal != 1) {
            return QString();
        }

        if (optionToken == QStringLiteral("-subtype")) {
            const QString symbolTypeToken = symbolTypeForSubtypeLookup(command, parsedLine);
            const QHash<QString, QStringList> subtypeByType = owner_->commandMetadata().commandSubtypeByTypeTokens.value(command);
            allowedValues = subtypeByType.value(symbolTypeToken);
            if (allowedValues.contains(QStringLiteral("*"), Qt::CaseInsensitive)) {
                return QString();
            }
            if (allowedValues.contains(normalizedCursorToken, Qt::CaseInsensitive)) {
                return QString();
            }

            if (!symbolTypeToken.isEmpty() && !allowedValues.isEmpty()) {
                detailMessage = TextEditorTab::tr("Subtype '%1' is not allowed for type '%2'.")
                    .arg(cursorToken.toHtmlEscaped(), symbolTypeToken.toHtmlEscaped());
                issueKey = QStringLiteral("subtype|%1|%2|%3")
                    .arg(command, symbolTypeToken, normalizedCursorToken);
            }
        } else {
            allowedValues = owner_->commandMetadata().commandOptionValueTokens.value(commandOptionValueKey(command, optionToken));
            if (allowedValues.isEmpty()
                || allowedValues.contains(normalizedCursorToken, Qt::CaseInsensitive)) {
                return QString();
            }
            detailMessage = TextEditorTab::tr("Value '%1' is not allowed for option '%2'.")
                .arg(cursorToken.toHtmlEscaped(), optionToken.toHtmlEscaped());
            issueKey = QStringLiteral("value|%1|%2|%3")
                .arg(command, optionToken, normalizedCursorToken);
        }
    }

    if (allowedValues.isEmpty()) {
        return QString();
    }

    QStringList normalizedAllowed;
    appendUniqueListCaseInsensitive(normalizedAllowed, allowedValues);
    std::sort(normalizedAllowed.begin(),
              normalizedAllowed.end(),
              [](const QString &a, const QString &b) {
                  return QString::compare(a, b, Qt::CaseInsensitive) < 0;
              });

    if (detailMessage.isEmpty()) {
        detailMessage = TextEditorTab::tr("Token '%1' is not allowed in current context.")
            .arg(cursorToken.toHtmlEscaped());
    }
    if (issueKey.isEmpty()) {
        issueKey = QStringLiteral("generic|%1|%2").arg(command, normalizedCursorToken);
    }

    if (tooltipText != nullptr) {
        QString plainDetail = detailMessage;
        plainDetail.remove(QStringLiteral("<strong>"));
        plainDetail.remove(QStringLiteral("</strong>"));
        plainDetail.remove(QStringLiteral("<code>"));
        plainDetail.remove(QStringLiteral("</code>"));
        plainDetail.remove(QStringLiteral("&apos;"));
        plainDetail.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
        plainDetail.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
        plainDetail.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
        plainDetail.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        *tooltipText = TextEditorTab::tr("%1 Allowed: %2")
            .arg(plainDetail, normalizedAllowed.join(QStringLiteral(", ")));
    }
    if (tooltipKey != nullptr) {
        *tooltipKey = issueKey;
    }

    return ContextHelpController::renderValidationHtml(cursorToken, detailMessage, normalizedAllowed);
}
}
