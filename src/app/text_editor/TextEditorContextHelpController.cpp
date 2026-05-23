#include "TextEditorContextHelpController.h"

#include "ContextHelpController.h"
#include "TextEditorCommandMetadata.h"
#include "TextEditorSurfaceStyler.h"
#include "block_editor/BlockEditorDirectiveRules.h"

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

TextEditorContextHelpController::TextEditorContextHelpController(TextEditorContextHelpContext context,
                                                                 CommandCatalogStore catalogStore)
    : context_(std::move(context))
    , catalogStore_(std::move(catalogStore))
{
}

QString TextEditorContextHelpController::tr(const char *text) const
{
    if (context_.translate) {
        return context_.translate(text);
    }
    return QString::fromUtf8(text);
}

QPlainTextEdit *TextEditorContextHelpController::editor() const
{
    return context_.editor != nullptr ? *context_.editor : nullptr;
}

QSplitter *TextEditorContextHelpController::editorHelpSplitter() const
{
    return context_.editorHelpSplitter != nullptr ? *context_.editorHelpSplitter : nullptr;
}

QWidget *TextEditorContextHelpController::helpPanel() const
{
    return context_.helpPanel != nullptr ? *context_.helpPanel : nullptr;
}

QTextBrowser *TextEditorContextHelpController::helpBrowser() const
{
    return context_.helpBrowser != nullptr ? *context_.helpBrowser : nullptr;
}

void TextEditorContextHelpController::setHelpPanel(QWidget *helpPanel)
{
    if (context_.helpPanel != nullptr) {
        *context_.helpPanel = helpPanel;
    }
}

void TextEditorContextHelpController::setHelpBrowser(QTextBrowser *helpBrowser)
{
    if (context_.helpBrowser != nullptr) {
        *context_.helpBrowser = helpBrowser;
    }
}

void TextEditorContextHelpController::buildHelpPanel()
{
    if (context_.rootWidget == nullptr) {
        return;
    }

    auto *framedHelpPanel = new QFrame(context_.rootWidget);
    framedHelpPanel->setFrameShape(QFrame::NoFrame);
    setHelpPanel(framedHelpPanel);
    helpPanel()->setObjectName(QStringLiteral("textContextHelpPanel"));
    syncPanelSurfaceToBaseTone(helpPanel());
    auto *panelLayout = new QVBoxLayout(helpPanel());
    panelLayout->setContentsMargins(kHelpPanelPadding, kHelpPanelPadding, kHelpPanelPadding, kHelpPanelPadding);
    panelLayout->setSpacing(kHelpPanelSpacing);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);

    auto *headerLabel = new QLabel(tr("Contextual Help"), helpPanel());
    QFont headerFont = headerLabel->font();
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);

    headerRow->addWidget(headerLabel);
    headerRow->addStretch(1);

    setHelpBrowser(new QTextBrowser(helpPanel()));
    helpBrowser()->setFrameShape(QFrame::NoFrame);
    helpBrowser()->setOpenLinks(false);
    helpBrowser()->setOpenExternalLinks(false);
    helpBrowser()->setMinimumHeight(120);
    syncTextBrowserSurfaceToParent(helpBrowser());
    helpBrowser()->setHtml(
        tr("<p>Select a Therion command or item to see contextual help.</p>"));

    panelLayout->addLayout(headerRow);
    panelLayout->addWidget(helpBrowser(), 1);
}

void TextEditorContextHelpController::loadHelpMetadata()
{
    if (context_.metadata == nullptr) {
        return;
    }

    (*context_.metadata).clear();
    loadHelpMetadataFromCommandCatalog();
    if (context_.rebuildCompletionModel) {
        context_.rebuildCompletionModel();
    }
    if (context_.populateBlockToolboxScopeCombo) {
        context_.populateBlockToolboxScopeCombo();
    }
}

void TextEditorContextHelpController::loadHelpMetadataFromCommandCatalog()
{
    resetCatalogBlockDirectiveMetadataToDefaults();

    const QJsonObject catalogObject = catalogStore_.catalogObject();
    if (catalogObject.isEmpty()) {
        return;
    }
    applyCatalogBlockDirectiveMetadata(catalogObject);
    if (context_.applyCatalogCommandsMetadata) {
        context_.applyCatalogCommandsMetadata(catalogObject);
    }
}

void TextEditorContextHelpController::setHelpCollapsed(bool collapsed)
{
    if (context_.helpCollapsed == nullptr || context_.helpPanelExtent == nullptr) {
        return;
    }

    (*context_.helpCollapsed) = collapsed;
    if (helpBrowser() != nullptr) {
        helpBrowser()->setVisible(!collapsed);
    }
    if (editorHelpSplitter() != nullptr) {
        if (!collapsed && (*context_.helpPanelExtent) > 0) {
            const QList<int> sizes = editorHelpSplitter()->sizes();
            editorHelpSplitter()->setSizes(QList<int>{sizes.value(0, 1), (*context_.helpPanelExtent)});
        } else if (collapsed) {
            const QList<int> sizes = editorHelpSplitter()->sizes();
            if (sizes.size() >= 2) {
                const int minimumExtent = helpPanel() != nullptr
                    ? qMax(helpPanel()->minimumSizeHint().width(), helpPanel()->minimumWidth())
                    : 1;
                (*context_.helpPanelExtent) = qMax(sizes.at(1), minimumExtent);
            }
            editorHelpSplitter()->setSizes(QList<int>{1, 0});
        }
    }
}

void TextEditorContextHelpController::updateContextHelp()
{
    if (helpBrowser() == nullptr || context_.metadata == nullptr) {
        return;
    }

    const QString validationHtml = validationHelpHtmlForCursor();
    if (!validationHtml.isEmpty()) {
        helpBrowser()->setHtml(validationHtml);
        return;
    }

    const QStringList candidates = helpCandidateTokens();
    for (const QString &candidate : candidates) {
        const auto entryIt = (*context_.metadata).helpEntries.constFind(candidate.toLower());
        if (entryIt == (*context_.metadata).helpEntries.constEnd()) {
            continue;
        }

        const TherionHelpEntry &entry = entryIt.value();
        helpBrowser()->setHtml(ContextHelpController::renderHelpHtml(candidate,
                                                                             entry.summary,
                                                                             entry.syntax,
                                                                             entry.arguments,
                                                                             entry.acceptedValues,
                                                                             entry.options,
                                                                             true));
        return;
    }

    helpBrowser()->setHtml(tr("<p>No contextual help is available for the current token.</p>"));
}

void TextEditorContextHelpController::updateValidationTooltipForCursor()
{
    if (editor() == nullptr || context_.lastValidationTooltipKey == nullptr) {
        return;
    }

    QString tooltipText;
    QString tooltipKey;
    const QString validationHtml = validationHelpHtmlForCursor(&tooltipText, &tooltipKey);
    if (validationHtml.isEmpty() || tooltipText.trimmed().isEmpty()) {
        editor()->setToolTip(QString());
        (*context_.lastValidationTooltipKey).clear();
        return;
    }

    editor()->setToolTip(tooltipText);
    if (tooltipKey == (*context_.lastValidationTooltipKey)) {
        return;
    }

    (*context_.lastValidationTooltipKey) = tooltipKey;
    QToolTip::showText(editor()->mapToGlobal(editor()->cursorRect().bottomLeft()),
                       tooltipText,
                       editor(),
                       QRect(),
                       2200);
}

QStringList TextEditorContextHelpController::helpCandidateTokens() const
{
    if (editor() == nullptr) {
        return {};
    }

    QStringList candidates;
    const QString directToken = currentHelpTokenForCursor();
    if (!directToken.isEmpty()) {
        appendUniqueCaseInsensitive(candidates, directToken);
    }

    const QTextCursor cursor = editor()->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return candidates;
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
    if (!parsedLine.directive.isEmpty() && parsedLine.directive != directToken.toLower()) {
        appendUniqueCaseInsensitive(candidates, parsedLine.directive);
        if (context_.normalizedDirectiveToken && context_.openingDirectiveForClosingToken) {
            const QString normalizedDirective = context_.normalizedDirectiveToken(parsedLine.directive);
            const QString openingDirective = context_.openingDirectiveForClosingToken(normalizedDirective);
            if (!openingDirective.isEmpty()) {
                appendUniqueCaseInsensitive(candidates, openingDirective);
            }
        }
    }

    if (context_.currentCompletionCommand) {
        const QString resolvedCommand = context_.currentCompletionCommand();
        if (!resolvedCommand.isEmpty()) {
            appendUniqueCaseInsensitive(candidates, resolvedCommand);
        }
    }

    return candidates;
}

QString TextEditorContextHelpController::currentHelpTokenForCursor() const
{
    if (editor() == nullptr) {
        return QString();
    }

    const QTextCursor cursor = editor()->textCursor();
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
    if (editor() == nullptr || context_.metadata == nullptr || !context_.normalizedDirectiveToken) {
        return QString();
    }

    const QTextCursor cursor = editor()->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QString();
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString command = context_.normalizedDirectiveToken(parsedLine.directive.toLower());
    if (command.isEmpty() || !(*context_.metadata).commandOptionTokens.contains(command)) {
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
        const QStringList knownOptions = (*context_.metadata).commandOptionTokens.value(command);
        if (knownOptions.contains(normalizedCursorToken, Qt::CaseInsensitive)) {
            return QString();
        }

        allowedValues = knownOptions;
        std::sort(allowedValues.begin(),
                  allowedValues.end(),
                  [](const QString &a, const QString &b) {
                      return QString::compare(a, b, Qt::CaseInsensitive) < 0;
                  });
        detailMessage = tr("Unknown option for command '%1'.").arg(command.toHtmlEscaped());
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
        const QString arity = (*context_.metadata).commandOptionValueArityTokens
            .value(commandOptionValueKey(command, optionToken))
            .trimmed()
            .toUpper();
        const int valueOrdinal = tokenIndexAtCursor - optionIndex;
        if (canonicalOptionArityToken(arity) == QStringLiteral("EXACTLY_ONE") && valueOrdinal != 1) {
            return QString();
        }

        if (optionToken == QStringLiteral("-subtype")) {
            const QString symbolTypeToken = symbolTypeForSubtypeLookup(command, parsedLine);
            const QHash<QString, QStringList> subtypeByType = (*context_.metadata).commandSubtypeByTypeTokens.value(command);
            allowedValues = subtypeByType.value(symbolTypeToken);
            if (allowedValues.contains(QStringLiteral("*"), Qt::CaseInsensitive)) {
                return QString();
            }
            if (allowedValues.contains(normalizedCursorToken, Qt::CaseInsensitive)) {
                return QString();
            }

            if (!symbolTypeToken.isEmpty() && !allowedValues.isEmpty()) {
                detailMessage = tr("Subtype '%1' is not allowed for type '%2'.")
                    .arg(cursorToken.toHtmlEscaped(), symbolTypeToken.toHtmlEscaped());
                issueKey = QStringLiteral("subtype|%1|%2|%3")
                    .arg(command, symbolTypeToken, normalizedCursorToken);
            }
        } else {
            allowedValues = (*context_.metadata).commandOptionValueTokens.value(commandOptionValueKey(command, optionToken));
            if (allowedValues.isEmpty()
                || allowedValues.contains(normalizedCursorToken, Qt::CaseInsensitive)) {
                return QString();
            }
            detailMessage = tr("Value '%1' is not allowed for option '%2'.")
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
        detailMessage = tr("Token '%1' is not allowed in current context.")
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
        *tooltipText = tr("%1 Allowed: %2")
            .arg(plainDetail, normalizedAllowed.join(QStringLiteral(", ")));
    }
    if (tooltipKey != nullptr) {
        *tooltipKey = issueKey;
    }

    return ContextHelpController::renderValidationHtml(cursorToken, detailMessage, normalizedAllowed);
}
}
