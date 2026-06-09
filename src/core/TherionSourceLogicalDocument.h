#pragma once

#include "TherionSourceDocument.h"

#include <QString>
#include <QVector>

namespace TherionStudio
{
struct TherionSourceLogicalTextPart
{
    int logicalStart = 0;
    int logicalLength = 0;
    int lineNumber = 0;
    int columnNumber = 1;
    int startOffset = 0;
    QString lineText;
};

struct TherionSourcePhysicalRange
{
    int lineNumber = 0;
    int columnNumber = 0;
    int columnLength = 0;
    int startOffset = 0;
    int length = 0;
    QString lineText;
};

struct TherionSourceLogicalCommand
{
    int startLineNumber = 0;
    int endLineNumber = 0;
    int startOffset = 0;
    int endOffset = 0;
    QString text;
    TherionParsedLine parsed;
    QString normalizedDirective;
    QString currentBlockDirective;
    TherionSourceLineRole role = TherionSourceLineRole::Empty;
    bool opensBlock = false;
    bool closesBlock = false;
    QString closeMatchesOpenDirective;
    QVector<TherionSourceBlockFrame> blockStackBefore;
    QVector<int> physicalLineNumbers;
    QVector<TherionSourceLogicalTextPart> textParts;

    [[nodiscard]] bool shouldValidateCommandCatalog() const;
    [[nodiscard]] bool hasUnmatchedClose() const;
    [[nodiscard]] bool physicalRangeForLogicalRange(int logicalStart,
                                                    int logicalLength,
                                                    TherionSourcePhysicalRange *range) const;
};

class TherionSourceLogicalDocument final
{
public:
    [[nodiscard]] static TherionSourceLogicalDocument fromText(const QString &contents);
    [[nodiscard]] static TherionSourceLogicalDocument fromSourceDocument(const TherionSourceDocument &sourceDocument);

    [[nodiscard]] const QVector<TherionSourceLogicalCommand> &commands() const;

private:
    QVector<TherionSourceLogicalCommand> commands_;
};
}
