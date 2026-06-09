#pragma once

#include "TherionDocumentParser.h"

#include <QHash>
#include <QString>
#include <QVector>

namespace TherionStudio
{
enum class TherionSourceLineRole
{
    Empty,
    Comment,
    Command,
    BlockContent,
};

struct TherionSourceBlockFrame
{
    QString directive;
    int lineNumber = 0;
    QString lineText;
};

struct TherionSourceDocumentLine
{
    TherionParsedSourceLine sourceLine;
    QString normalizedDirective;
    QString currentBlockDirective;
    TherionSourceLineRole role = TherionSourceLineRole::Empty;
    bool opensBlock = false;
    bool closesBlock = false;
    QString closeMatchesOpenDirective;
    QVector<TherionSourceBlockFrame> blockStackBefore;

    [[nodiscard]] bool shouldValidateCommandCatalog() const;
    [[nodiscard]] bool hasUnmatchedClose() const;
};

class TherionSourceDocument final
{
public:
    [[nodiscard]] static TherionSourceDocument fromText(const QString &contents);

    [[nodiscard]] const TherionParsedSourceDocument &parsedDocument() const;
    [[nodiscard]] const QVector<TherionSourceDocumentLine> &lines() const;
    [[nodiscard]] const QVector<TherionSourceBlockFrame> &openBlocksAtEnd() const;
    [[nodiscard]] QString toText() const;
    [[nodiscard]] QVector<TherionParsedLine> tokenLines() const;

private:
    TherionParsedSourceDocument parsedDocument_;
    QVector<TherionSourceDocumentLine> lines_;
    QVector<TherionSourceBlockFrame> openBlocksAtEnd_;
};

[[nodiscard]] QString normalizedTherionDirectiveToken(const QString &token);
[[nodiscard]] QHash<QString, QString> therionOpenToCloseDirectiveMap();
[[nodiscard]] QHash<QString, QString> therionCloseToOpenDirectiveMap();
[[nodiscard]] bool therionDirectiveIsKnownBlockDirective(const QString &directive);
[[nodiscard]] bool therionBlockTreatsChildrenAsContent(const QString &directive);
}
