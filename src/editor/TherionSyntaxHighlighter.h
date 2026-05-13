#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QSet>
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

    QTextCharFormat baseTextFormat_;
    QTextCharFormat keywordFormat_;
    QTextCharFormat stringFormat_;
    QTextCharFormat numberFormat_;
    QTextCharFormat commentFormat_;
    QSet<QString> keywordTokens_;
};
}
