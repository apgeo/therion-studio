#pragma once

#include "TherionSourceDocument.h"

#include <QString>
#include <QVector>

namespace TherionStudio
{
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

    [[nodiscard]] bool shouldValidateCommandCatalog() const;
    [[nodiscard]] bool hasUnmatchedClose() const;
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
