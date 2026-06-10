#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QSet>
#include <QHash>
#include <QJsonObject>

#include "../core/CommandCatalogStore.h"
#include "../core/TherionSourceValidator.h"

class QTextDocument;

namespace TherionStudio
{
class TherionSyntaxHighlighter final : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit TherionSyntaxHighlighter(CommandCatalogStore catalogStore, QTextDocument *parent = nullptr);
    void reloadPaletteForApplicationAppearance();

protected:
    void highlightBlock(const QString &text) override;

private:
    void loadPalette();
    void applyStyle(const QString &styleName, const QJsonObject &styleObject);
    void loadCommandCatalogKeywords();
    void applyValidatorInvalidTokenFormats(const QString &text);
    const TherionSourceValidationResult &cachedValidationResult();

    QTextCharFormat baseTextFormat_;
    QTextCharFormat keywordFormat_;
    QTextCharFormat optionFormat_;
    QTextCharFormat identifierFormat_;
    QTextCharFormat invalidTokenFormat_;
    QTextCharFormat stringFormat_;
    QTextCharFormat numberFormat_;
    QTextCharFormat commentFormat_;
    QSet<QString> keywordTokens_;
    QSet<QString> commandTokens_;
    QHash<QString, QSet<QString>> commandOptionTokens_;
    QHash<QString, QSet<QString>> commandAllowedValues_;
    QHash<QString, QHash<QString, QString>> commandOptionValueArity_;
    QHash<QString, QSet<int>> commandPositionalIdTokenIndexes_;
    QHash<QString, QSet<QString>> commandOptionIdValueTokens_;
    QSet<QString> closingDirectiveIdTokens_;
    TherionSourceValidationCatalog validationCatalog_;
    TherionSourceValidationResult cachedValidationResult_;
    int cachedValidationRevision_ = -1;
    CommandCatalogStore catalogStore_;
};
}
