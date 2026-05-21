#pragma once

#include <QPlainTextEdit>

class QWidget;

namespace TherionStudio
{
class RawEditorLineNumberArea;

class RawEditorTextEdit final : public QPlainTextEdit
{
public:
    explicit RawEditorTextEdit(QWidget *parent = nullptr);

    void setHighlightedLineNumber(int lineNumber);
    int lineNumberAreaWidth() const;
    void paintLineNumberArea(QPaintEvent *event);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void updateLineNumberAreaWidth();
    void handleLineNumberAreaUpdate(const QRect &rect, int dy);

    int highlightedLineNumber_ = 0;
    RawEditorLineNumberArea *lineNumberArea_ = nullptr;
};
}
