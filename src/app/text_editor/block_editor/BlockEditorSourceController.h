#pragma once

#include <functional>

#include <QString>
#include <QStringList>

class QPlainTextEdit;

namespace TherionStudio
{
struct TherionSourceTextEdit;

struct BlockEditorSourceContext
{
    QPlainTextEdit *editor = nullptr;
    bool editable = false;
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
    bool applyTextEdit(const TherionSourceTextEdit &edit) const;

private:
    QString tr(const char *text) const;
    bool canEdit() const;

    BlockEditorSourceContext context_;
};
}
