#pragma once

#include <QVector>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
struct TherionSourceLine
{
    QString text;
    QString lineEnding;
};

class TherionSourceText
{
public:
    static TherionSourceText fromText(const QString &contents)
    {
        TherionSourceText sourceText;
        QString currentLine;

        for (int index = 0; index < contents.size(); ++index) {
            const QChar current = contents.at(index);
            if (current == QLatin1Char('\r')) {
                if (index + 1 < contents.size() && contents.at(index + 1) == QLatin1Char('\n')) {
                    sourceText.lines_.append(TherionSourceLine{currentLine, QStringLiteral("\r\n")});
                    currentLine.clear();
                    ++index;
                } else {
                    sourceText.lines_.append(TherionSourceLine{currentLine, QStringLiteral("\r")});
                    currentLine.clear();
                }
                continue;
            }
            if (current == QLatin1Char('\n')) {
                sourceText.lines_.append(TherionSourceLine{currentLine, QStringLiteral("\n")});
                currentLine.clear();
                continue;
            }

            currentLine.append(current);
        }

        sourceText.lines_.append(TherionSourceLine{currentLine, QString()});
        return sourceText;
    }

    static QString detectedLineEnding(const QString &contents)
    {
        return contents.contains(QStringLiteral("\r\n"))
            ? QStringLiteral("\r\n")
            : QStringLiteral("\n");
    }

    static QStringList splitTextLines(const QString &contents)
    {
        return fromText(contents).textLines();
    }

    static QString joinTextLines(const QString &originalContents, const QStringList &lines)
    {
        return lines.join(detectedLineEnding(originalContents));
    }

    const QVector<TherionSourceLine> &physicalLines() const
    {
        return lines_;
    }

    QStringList textLines() const
    {
        QStringList lines;
        lines.reserve(lines_.size());
        for (const TherionSourceLine &line : lines_) {
            lines.append(line.text);
        }
        return lines;
    }

    QString toText() const
    {
        QString contents;
        for (const TherionSourceLine &line : lines_) {
            contents += line.text;
            contents += line.lineEnding;
        }
        return contents;
    }

    QString preferredLineEnding() const
    {
        for (const TherionSourceLine &line : lines_) {
            if (!line.lineEnding.isEmpty()) {
                return line.lineEnding;
            }
        }
        return QStringLiteral("\n");
    }

    bool replaceLineRange(int firstLineIndex, int removeCount, const QStringList &replacementLines)
    {
        if (firstLineIndex < 0 || firstLineIndex > lines_.size() || removeCount < 0) {
            return false;
        }
        if (firstLineIndex + removeCount > lines_.size()) {
            return false;
        }

        const QString replacementLineEnding = preferredLineEnding();
        const bool followedByExistingLine = firstLineIndex + removeCount < lines_.size();
        QVector<TherionSourceLine> replacement;
        replacement.reserve(replacementLines.size());
        for (int index = 0; index < replacementLines.size(); ++index) {
            const bool last = index == replacementLines.size() - 1;
            replacement.append(TherionSourceLine{
                replacementLines.at(index),
                last && !followedByExistingLine ? QString() : replacementLineEnding});
        }

        lines_.remove(firstLineIndex, removeCount);
        for (int index = replacement.size() - 1; index >= 0; --index) {
            lines_.insert(firstLineIndex, replacement.at(index));
        }
        return true;
    }

private:
    QVector<TherionSourceLine> lines_;
};
}
