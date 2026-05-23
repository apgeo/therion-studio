#pragma once

#include <functional>

#include <QString>
#include <QStringList>

class QPlainTextEdit;

namespace TherionStudio
{
struct BlockEditorSourceContext
{
    QPlainTextEdit *editor = nullptr;
    bool editable = false;
    std::function<void(const QString &)> replaceText;
    std::function<QString(const char *)> translate;
};

class BlockEditorSourceController final
{
public:
    explicit BlockEditorSourceController(BlockEditorSourceContext context);

    bool hasEditor() const;
    QString text() const;
    QStringList normalizedLines() const;
    bool insertLinesBefore(int lineNumber, const QStringList &newLines, QString *errorMessage = nullptr) const;
    bool removeLineRange(int startLine, int endLine) const;
    bool replaceLine(int lineNumber, const QString &line) const;
    void replaceWithLines(const QString &originalContents, const QStringList &lines) const;
    void replaceText(const QString &contents) const;

private:
    QString tr(const char *text) const;
    bool canEdit() const;

    BlockEditorSourceContext context_;
};
}
