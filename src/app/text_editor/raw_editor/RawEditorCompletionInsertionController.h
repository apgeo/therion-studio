#pragma once

#include <functional>

#include <QString>

class QPlainTextEdit;

namespace TherionStudio
{
struct RawEditorCompletionInsertionContext
{
    QPlainTextEdit *editor = nullptr;
    std::function<QString(const QString &)> normalizedDirectiveToken;
    std::function<QString(const QString &)> closingDirectiveForOpeningToken;
};

class RawEditorCompletionInsertionController final
{
public:
    explicit RawEditorCompletionInsertionController(RawEditorCompletionInsertionContext context);

    void insertCompletionToken(const QString &completion);

private:
    RawEditorCompletionInsertionContext context_;
};
}
