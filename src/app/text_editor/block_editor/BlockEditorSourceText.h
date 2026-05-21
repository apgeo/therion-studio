#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
QString blockEditorSourceLineEnding(const QString &contents);
QStringList blockEditorNormalizedSourceLines(const QString &contents);
QString blockEditorJoinSourceLines(const QString &originalContents, const QStringList &lines);
bool blockEditorReplaceSourceLineRange(QStringList *lines,
                                       int startLine,
                                       int endLine,
                                       const QStringList &replacementLines);
}
