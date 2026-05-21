#include "RawEditorTextEdit.h"

#include <QColor>
#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTextBlock>
#include <QWidget>

namespace TherionStudio
{
class RawEditorLineNumberArea final : public QWidget
{
public:
    explicit RawEditorLineNumberArea(RawEditorTextEdit *editor)
        : QWidget(editor)
        , editor_(editor)
    {
    }

    QSize sizeHint() const override
    {
        if (editor_ == nullptr) {
            return QWidget::sizeHint();
        }
        return QSize(editor_->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        if (editor_ != nullptr) {
            editor_->paintLineNumberArea(event);
        }
    }

private:
    RawEditorTextEdit *editor_ = nullptr;
};

RawEditorTextEdit::RawEditorTextEdit(QWidget *parent)
    : QPlainTextEdit(parent)
    , lineNumberArea_(new RawEditorLineNumberArea(this))
{
    connect(this,
            &QPlainTextEdit::blockCountChanged,
            this,
            [this](int) {
                updateLineNumberAreaWidth();
            });
    connect(this, &QPlainTextEdit::updateRequest, this, &RawEditorTextEdit::handleLineNumberAreaUpdate);
    connect(this,
            &QPlainTextEdit::cursorPositionChanged,
            this,
            [this] {
                if (lineNumberArea_ != nullptr) {
                    lineNumberArea_->update();
                }
            });
    updateLineNumberAreaWidth();
}

void RawEditorTextEdit::setHighlightedLineNumber(int lineNumber)
{
    if (highlightedLineNumber_ == lineNumber) {
        return;
    }

    highlightedLineNumber_ = lineNumber;
    viewport()->update();
    if (lineNumberArea_ != nullptr) {
        lineNumberArea_->update();
    }
}

int RawEditorTextEdit::lineNumberAreaWidth() const
{
    int digits = 1;
    int maxLineNumber = qMax(1, blockCount());
    while (maxLineNumber >= 10) {
        maxLineNumber /= 10;
        ++digits;
    }

    const int horizontalPadding = 8;
    return (horizontalPadding * 2) + (fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits);
}

void RawEditorTextEdit::paintLineNumberArea(QPaintEvent *event)
{
    if (lineNumberArea_ == nullptr || event == nullptr) {
        return;
    }

    QPainter painter(lineNumberArea_);

    QColor gutterBackground = palette().color(QPalette::Window);
    if (!gutterBackground.isValid()) {
        gutterBackground = palette().color(QPalette::Base);
    }
    painter.fillRect(event->rect(), gutterBackground);

    QColor dividerColor = palette().color(QPalette::Mid);
    if (!dividerColor.isValid()) {
        dividerColor = QColor(QStringLiteral("#bcbcbc"));
    }
    painter.setPen(dividerColor);
    painter.drawLine(lineNumberArea_->width() - 1,
                     event->rect().top(),
                     lineNumberArea_->width() - 1,
                     event->rect().bottom());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const int visibleLineNumber = blockNumber + 1;
            const bool isHighlightedLine = highlightedLineNumber_ == visibleLineNumber;

            QFont numberFont = painter.font();
            numberFont.setBold(isHighlightedLine);
            painter.setFont(numberFont);

            QColor activeNumberColor = palette().color(QPalette::Text);
            if (!activeNumberColor.isValid()) {
                activeNumberColor = QColor(QStringLiteral("#202020"));
            }

            QColor numberColor;
            if (isHighlightedLine) {
                numberColor = activeNumberColor;
            } else {
                const QColor baseColor = palette().color(QPalette::Base);
                const bool darkPalette = baseColor.isValid() && baseColor.lightnessF() < 0.5;
                if (darkPalette) {
                    numberColor = activeNumberColor;
                    numberColor.setAlpha(190);
                } else {
                    numberColor = palette().color(QPalette::Mid);
                    if (!numberColor.isValid()) {
                        numberColor = QColor(QStringLiteral("#808080"));
                    }
                }
            }

            painter.setPen(numberColor);

            painter.drawText(0,
                             top,
                             lineNumberArea_->width() - 6,
                             qRound(blockBoundingRect(block).height()),
                             Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(visibleLineNumber));
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void RawEditorTextEdit::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    if (lineNumberArea_ == nullptr) {
        return;
    }

    const QRect contentRect = contentsRect();
    lineNumberArea_->setGeometry(QRect(contentRect.left(),
                                       contentRect.top(),
                                       lineNumberAreaWidth(),
                                       contentRect.height()));
}

void RawEditorTextEdit::paintEvent(QPaintEvent *event)
{
    QPlainTextEdit::paintEvent(event);

    if (highlightedLineNumber_ <= 0 || document() == nullptr) {
        return;
    }

    const int targetBlockNumber = highlightedLineNumber_ - 1;
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int blockTop = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int blockBottom = blockTop + qRound(blockBoundingRect(block).height());
    bool foundVisibleBlock = false;
    while (block.isValid() && blockNumber <= targetBlockNumber) {
        if (blockNumber == targetBlockNumber) {
            foundVisibleBlock = true;
            break;
        }
        block = block.next();
        blockTop = blockBottom;
        blockBottom = blockTop + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
    if (!foundVisibleBlock || !block.isVisible()) {
        return;
    }
    const QRect blockBounds(0, blockTop, viewport()->width(), qMax(1, blockBottom - blockTop));
    if (!blockBounds.intersects(viewport()->rect())) {
        return;
    }

    QColor fill = palette().color(QPalette::Highlight);
    if (!fill.isValid()) {
        fill = QColor(QStringLiteral("#4f8ad9"));
    }

    QPainter painter(viewport());
    painter.setPen(Qt::NoPen);
    fill.setAlpha(52);
    painter.setBrush(fill);
    painter.drawRect(QRectF(blockBounds));
}

void RawEditorTextEdit::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void RawEditorTextEdit::handleLineNumberAreaUpdate(const QRect &rect, int dy)
{
    if (lineNumberArea_ == nullptr) {
        return;
    }

    if (dy != 0) {
        lineNumberArea_->scroll(0, dy);
    } else {
        lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth();
    }
}
}
