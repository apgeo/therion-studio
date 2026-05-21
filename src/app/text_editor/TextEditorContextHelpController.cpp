#include "TextEditorContextHelpController.h"

#include "TextEditorTab.h"
#include "ContextHelpController.h"

#include "../../core/TherionCommandSyntax.h"
#include "../../core/TherionDocumentParser.h"

#include <algorithm>
#include <QRect>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QTextBlock>
#include <QToolTip>

namespace
{
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
TextEditorContextHelpController::TextEditorContextHelpController(TextEditorTab *owner)
    : owner_(owner)
{
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
        const auto entryIt = owner_->helpEntries_.constFind(candidate.toLower());
        if (entryIt == owner_->helpEntries_.constEnd()) {
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
    if (command.isEmpty() || !owner_->commandOptionTokens_.contains(command)) {
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
        const QStringList knownOptions = owner_->commandOptionTokens_.value(command);
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
        const QString arity = owner_->commandOptionValueArityTokens_
            .value(commandOptionValueKey(command, optionToken))
            .trimmed()
            .toUpper();
        const int valueOrdinal = tokenIndexAtCursor - optionIndex;
        if (canonicalOptionArityToken(arity) == QStringLiteral("EXACTLY_ONE") && valueOrdinal != 1) {
            return QString();
        }

        if (optionToken == QStringLiteral("-subtype")) {
            const QString symbolTypeToken = symbolTypeForSubtypeLookup(command, parsedLine);
            const QHash<QString, QStringList> subtypeByType = owner_->commandSubtypeByTypeTokens_.value(command);
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
            allowedValues = owner_->commandOptionValueTokens_.value(commandOptionValueKey(command, optionToken));
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
