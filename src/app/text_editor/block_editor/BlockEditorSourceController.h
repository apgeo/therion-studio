#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorSourceController final
{
public:
    explicit BlockEditorSourceController(TextEditorTab *owner);
    explicit BlockEditorSourceController(const TextEditorTab *owner);

    bool hasEditor() const;
    QString text() const;
    QStringList normalizedLines() const;
    bool insertLinesBefore(int lineNumber, const QStringList &newLines, QString *errorMessage = nullptr) const;
    bool removeLineRange(int startLine, int endLine) const;
    bool replaceLine(int lineNumber, const QString &line) const;
    void replaceWithLines(const QString &originalContents, const QStringList &lines) const;
    void replaceText(const QString &contents) const;

private:
    const TextEditorTab *owner_ = nullptr;
    TextEditorTab *mutableOwner_ = nullptr;
};
}
