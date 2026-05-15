#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QSet>
#include <QHash>
#include <QJsonObject>

class QTextDocument;

namespace TherionStudio
{
class TherionSyntaxHighlighter final : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit TherionSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    void loadPalette();
    void applyStyle(const QString &styleName, const QJsonObject &styleObject);
    void loadCommandCatalogKeywords();

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
    QHash<QString, QHash<QString, QSet<QString>>> commandOptionEnumValues_;
    QHash<QString, QHash<QString, QSet<QString>>> commandSubtypeByTypeTokens_;
    QHash<QString, QSet<int>> commandPositionalIdTokenIndexes_;
    QHash<QString, QSet<QString>> commandOptionIdValueTokens_;
    QSet<QString> closingDirectiveIdTokens_;
};
}
