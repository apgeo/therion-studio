#include "TextEditorTab.h"
#include "block_editor/BlockEditorApplyExecutor.h"
#include "block_editor/BlockEditorApplyStateController.h"
#include "block_editor/BlockEditorCanvasSelectionController.h"
#include "block_editor/BlockEditorCanvasBoundaryController.h"
#include "block_editor/BlockEditorCommandOptionParser.h"
#include "block_editor/BlockEditorDetailsHelpController.h"
#include "block_editor/BlockEditorDetailsPaneController.h"
#include "block_editor/BlockEditorDocumentOutlineBuilder.h"
#include "block_editor/BlockEditorDirectiveRules.h"
#include "block_editor/BlockEditorDropTargetResolver.h"
#include "block_editor/BlockEditorInsertionPlanner.h"
#include "block_editor/BlockEditorInsertionTemplateBuilder.h"
#include "block_editor/BlockEditorLineBuildService.h"
#include "block_editor/BlockEditorMovePlanner.h"
#include "block_editor/BlockEditorMovePreviewController.h"
#include "block_editor/BlockEditorMoveSourceRewriter.h"
#include "block_editor/BlockEditorOptionArgsController.h"
#include "block_editor/BlockEditorSelectionDetailsController.h"
#include "block_editor/BlockEditorToolboxDetailsController.h"
#include "ContextHelpController.h"
#include "raw_editor/RawEditorCompletionController.h"
#include "raw_editor/RawEditorFindController.h"
#include "TextEditorOptionValidation.h"
#include "TextEditorContextHelpController.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDrag>
#include <QComboBox>
#include <QFrame>
#include <QCompleter>
#include <QFileInfo>
#include <QDir>
#include <QFont>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QGraphicsObject>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QStackedWidget>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolTip>
#include <QToolButton>
#include <QHeaderView>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QScopeGuard>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleHints>
#include <QStringListModel>
#include <QTimer>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QResizeEvent>

#include <QJsonArray>
#include <QJsonObject>
#include <QTextBlock>
#include <QPen>

#include <algorithm>
#include <functional>
#include <limits>

#include "../../core/CommandCatalogService.h"
#include "../../core/DocumentFile.h"
#include "../../core/TherionCommandSyntax.h"
#include "../../core/TherionDocumentEditor.h"
#include "../../core/TherionDocumentParser.h"
#include "../../editor/TherionSyntaxHighlighter.h"

namespace
{
using namespace TherionStudio::BlockEditorDirectiveRules;

constexpr int kPanelPadding = 12;
constexpr int kPanelSpacing = 8;
constexpr int kBlocksSidePaneMinWidth = 320;
constexpr int kBlocksSidePaneMaxWidth = 460;
constexpr int kBlockEndHintContainerLineDataRole = 0x42554e44; // "BUND"

void applyThinSplitterStyle(QSplitter *splitter, const QString &objectName)
{
    if (splitter == nullptr) {
        return;
    }

    splitter->setObjectName(objectName);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QString());
}

QColor blockBaseColorForDirective(const QString &directive);
void syncPanelSurfaceToBaseTone(QWidget *panelWidget);
void syncPanelSurfaceToPalette(QWidget *panelWidget, const QPalette &sourcePalette);
void syncTextBrowserSurfaceToParent(QWidget *browserWidget);
void syncTextBrowserSurfaceToPalette(QWidget *browserWidget, const QPalette &sourcePalette);

class HighlightPlainTextEdit;

class LineNumberAreaWidget final : public QWidget
{
public:
    explicit LineNumberAreaWidget(HighlightPlainTextEdit *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    HighlightPlainTextEdit *editor_ = nullptr;
};

class HighlightPlainTextEdit final : public QPlainTextEdit
{
public:
    explicit HighlightPlainTextEdit(QWidget *parent = nullptr)
        : QPlainTextEdit(parent)
        , lineNumberArea_(new LineNumberAreaWidget(this))
    {
        connect(this,
                &QPlainTextEdit::blockCountChanged,
                this,
                [this](int) {
                    updateLineNumberAreaWidth();
                });
        connect(this, &QPlainTextEdit::updateRequest, this, &HighlightPlainTextEdit::handleLineNumberAreaUpdate);
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

    void setHighlightedLineNumber(int lineNumber)
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

    int lineNumberAreaWidth() const
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

    void paintLineNumberArea(QPaintEvent *event)
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

protected:
    void resizeEvent(QResizeEvent *event) override
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

    void paintEvent(QPaintEvent *event) override
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

private:
    void updateLineNumberAreaWidth()
    {
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    }

    void handleLineNumberAreaUpdate(const QRect &rect, int dy)
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

    int highlightedLineNumber_ = 0;
    QWidget *lineNumberArea_ = nullptr;
};

LineNumberAreaWidget::LineNumberAreaWidget(HighlightPlainTextEdit *editor)
    : QWidget(editor)
    , editor_(editor)
{
}

QSize LineNumberAreaWidget::sizeHint() const
{
    if (editor_ == nullptr) {
        return QWidget::sizeHint();
    }
    return QSize(editor_->lineNumberAreaWidth(), 0);
}

void LineNumberAreaWidget::paintEvent(QPaintEvent *event)
{
    if (editor_ != nullptr) {
        editor_->paintLineNumberArea(event);
    }
}

void appendUnique(QStringList &target, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (target.contains(trimmed, Qt::CaseInsensitive)) {
        return;
    }
    target.append(trimmed);
}

void appendUniqueList(QStringList &target, const QStringList &values)
{
    for (const QString &value : values) {
        appendUnique(target, value);
    }
}

bool isRequiredArgumentSignature(const QString &signature)
{
    const QString trimmed = signature.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    if (trimmed.startsWith(QLatin1Char('['))) {
        return false;
    }
    return trimmed.contains(QLatin1Char('<')) && trimmed.contains(QLatin1Char('>'));
}

QString argumentSignatureFromHelpLine(const QString &argumentLine)
{
    const int equalsIndex = argumentLine.indexOf(QLatin1Char('='));
    if (equalsIndex >= 0) {
        return argumentLine.left(equalsIndex).trimmed();
    }
    return argumentLine.trimmed();
}

QString argumentLabelFromSignature(const QString &signature)
{
    QString label = signature.trimmed();
    label.remove(QLatin1Char('|'));
    label.remove(QLatin1Char('['));
    label.remove(QLatin1Char(']'));
    label.remove(QLatin1Char('<'));
    label.remove(QLatin1Char('>'));
    label.replace(QLatin1Char('_'), QLatin1Char(' '));
    label.replace(QLatin1Char('-'), QLatin1Char(' '));
    label = label.simplified();
    label = label.trimmed();
    if (label.isEmpty()) {
        return QObject::tr("Value");
    }
    label = label.toLower();
    label[0] = label.at(0).toUpper();
    return label;
}

QString titleCaseWords(QString label)
{
    label = label.trimmed();
    if (label.isEmpty()) {
        return label;
    }

    label = label.toLower();
    QStringList words = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString &word : words) {
        if (word.isEmpty()) {
            continue;
        }
        word[0] = word.at(0).toUpper();
    }
    return words.join(QLatin1Char(' '));
}

QString dataFieldDisplayLabel(const QString &fieldToken)
{
    QString label = fieldToken.trimmed();
    if (label.isEmpty()) {
        return QObject::tr("Value");
    }

    label.replace(QLatin1Char('_'), QLatin1Char(' '));
    label.replace(QLatin1Char('-'), QLatin1Char(' '));
    label = label.simplified();
    if (label.isEmpty()) {
        return QObject::tr("Value");
    }
    return titleCaseWords(label);
}

void installLineEditCompleter(QLineEdit *lineEdit, const QStringList &values)
{
    if (lineEdit == nullptr) {
        return;
    }

    QStringList suggestions;
    for (const QString &value : values) {
        const QString normalized = value.trimmed();
        if (!normalized.isEmpty()) {
            suggestions.append(normalized);
        }
    }
    suggestions.removeDuplicates();
    std::sort(suggestions.begin(), suggestions.end(), [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });

    if (suggestions.isEmpty()) {
        lineEdit->setCompleter(nullptr);
        return;
    }

    auto *completer = new QCompleter(suggestions, lineEdit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    lineEdit->setCompleter(completer);
}

class TokenTagEditorWidget final : public QWidget
{
public:
    explicit TokenTagEditorWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        input_ = new QLineEdit(this);
        input_->setObjectName(QStringLiteral("tokenTagEditorInput"));
        input_->installEventFilter(this);
        layout->addWidget(input_, 1);

        connect(input_, &QLineEdit::returnPressed, this, [this]() {
            commitInputTokens();
        });
        connect(input_, &QLineEdit::selectionChanged, this, [this]() {
            if (onFocusContextChanged) {
                onFocusContextChanged();
            }
        });
        connect(input_, &QLineEdit::cursorPositionChanged, this, [this](int, int) {
            if (onFocusContextChanged) {
                onFocusContextChanged();
            }
        });
    }

    std::function<void()> onTokensChanged;
    std::function<void()> onFocusContextChanged;

    void setPlaceholderText(const QString &text)
    {
        placeholderText_ = text;
        refreshInputPlaceholder();
    }

    void setSuggestions(const QStringList &values)
    {
        suggestions_.clear();
        for (const QString &value : values) {
            const QString normalized = value.trimmed();
            if (!normalized.isEmpty()) {
                suggestions_.append(normalized);
            }
        }
        suggestions_.removeDuplicates();
        std::sort(suggestions_.begin(), suggestions_.end(), [](const QString &left, const QString &right) {
            return QString::compare(left, right, Qt::CaseInsensitive) < 0;
        });
        rebuildCompleter();
    }

    void setTokens(const QStringList &tokens)
    {
        QStringList normalized;
        for (const QString &token : tokens) {
            const QString trimmed = token.trimmed();
            if (!trimmed.isEmpty() && !containsTokenCaseInsensitive(normalized, trimmed)) {
                normalized.append(trimmed);
            }
        }
        tokens_ = normalized;
        rebuildChips();
        refreshInputPlaceholder();
    }

    QStringList tokens() const
    {
        return tokens_;
    }

    bool hasEditorFocus() const
    {
        if (input_ != nullptr && input_->hasFocus()) {
            return true;
        }
        for (QToolButton *chip : chipButtons_) {
            if (chip != nullptr && chip->hasFocus()) {
                return true;
            }
        }
        return false;
    }

private:
    static bool containsTokenCaseInsensitive(const QStringList &tokens, const QString &token)
    {
        for (const QString &existing : tokens) {
            if (QString::compare(existing.trimmed(), token.trimmed(), Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    }

    void rebuildCompleter()
    {
        if (input_ == nullptr) {
            return;
        }
        if (suggestions_.isEmpty()) {
            input_->setCompleter(nullptr);
            return;
        }
        auto *completer = new QCompleter(suggestions_, input_);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        input_->setCompleter(completer);
        connect(completer,
                QOverload<const QString &>::of(&QCompleter::activated),
                input_,
                [this](const QString &choice) {
                    const QString normalized = choice.trimmed();
                    if (normalized.isEmpty()) {
                        return;
                    }
                    appendToken(normalized);
                    // Clear after Qt finishes completer insertion to avoid stale token text.
                    QTimer::singleShot(0, this, [this]() {
                        if (input_ != nullptr) {
                            input_->clear();
                        }
                    });
                });
    }

    void refreshInputPlaceholder()
    {
        if (input_ == nullptr) {
            return;
        }
        input_->setPlaceholderText(tokens_.isEmpty() ? placeholderText_ : QString());
    }

    bool appendToken(const QString &token)
    {
        const QString normalized = token.trimmed();
        if (normalized.isEmpty()) {
            return false;
        }
        if (containsTokenCaseInsensitive(tokens_, normalized)) {
            return false;
        }
        tokens_.append(normalized);
        rebuildChips();
        refreshInputPlaceholder();
        if (onTokensChanged) {
            onTokensChanged();
        }
        return true;
    }

    bool commitInputTokens()
    {
        if (input_ == nullptr) {
            return false;
        }
        const QString rawInput = input_->text().trimmed();
        if (rawInput.isEmpty()) {
            return false;
        }
        QStringList parsedTokens = TherionStudio::TherionDocumentParser::tokenizeLine(rawInput);
        if (parsedTokens.isEmpty()) {
            parsedTokens = rawInput.split(QRegularExpression(QStringLiteral(R"(\s+)")), Qt::SkipEmptyParts);
        }
        if (parsedTokens.isEmpty()) {
            return false;
        }
        for (const QString &token : parsedTokens) {
            const QString normalized = token.trimmed();
            if (!normalized.isEmpty()) {
                appendToken(normalized);
            }
        }
        input_->clear();
        return true;
    }

    void rebuildChips()
    {
        auto *layout = qobject_cast<QHBoxLayout *>(this->layout());
        if (layout == nullptr || input_ == nullptr) {
            return;
        }
        for (QToolButton *chip : chipButtons_) {
            if (chip == nullptr) {
                continue;
            }
            layout->removeWidget(chip);
            chip->deleteLater();
        }
        chipButtons_.clear();

        for (int index = 0; index < tokens_.size(); ++index) {
            const QString token = tokens_.at(index);
            auto *chip = new QToolButton(this);
            chip->setText(QStringLiteral("%1  ✕").arg(token));
            chip->setProperty("token_index", index);
            chip->setAutoRaise(true);
            chip->setFocusPolicy(Qt::StrongFocus);
            connect(chip, &QToolButton::clicked, this, [this, chip]() {
                const int tokenIndex = chip->property("token_index").toInt();
                if (tokenIndex < 0 || tokenIndex >= tokens_.size()) {
                    return;
                }
                tokens_.removeAt(tokenIndex);
                rebuildChips();
                refreshInputPlaceholder();
                if (onTokensChanged) {
                    onTokensChanged();
                }
            });
            connect(chip, &QToolButton::pressed, this, [this]() {
                if (onFocusContextChanged) {
                    onFocusContextChanged();
                }
            });
            layout->insertWidget(layout->count() - 1, chip);
            chipButtons_.append(chip);
        }
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched != input_ || event == nullptr) {
            return QWidget::eventFilter(watched, event);
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent == nullptr) {
                return QWidget::eventFilter(watched, event);
            }
            if (keyEvent->key() == Qt::Key_Space
                || keyEvent->key() == Qt::Key_Comma
                || keyEvent->key() == Qt::Key_Return
                || keyEvent->key() == Qt::Key_Enter
                || keyEvent->key() == Qt::Key_Tab) {
                const bool isCompletionAcceptKey = keyEvent->key() == Qt::Key_Return
                    || keyEvent->key() == Qt::Key_Enter
                    || keyEvent->key() == Qt::Key_Tab;
                if (isCompletionAcceptKey) {
                    if (QCompleter *completer = input_->completer();
                        completer != nullptr && completer->popup() != nullptr && completer->popup()->isVisible()) {
                        return QWidget::eventFilter(watched, event);
                    }
                }
                if (commitInputTokens()) {
                    return true;
                }
            } else if (keyEvent->key() == Qt::Key_Backspace && input_->text().isEmpty() && !tokens_.isEmpty()) {
                tokens_.removeLast();
                rebuildChips();
                refreshInputPlaceholder();
                if (onTokensChanged) {
                    onTokensChanged();
                }
                return true;
            }
        } else if (event->type() == QEvent::FocusIn) {
            if (onFocusContextChanged) {
                onFocusContextChanged();
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    QLineEdit *input_ = nullptr;
    QString placeholderText_;
    QStringList suggestions_;
    QStringList tokens_;
    QList<QToolButton *> chipButtons_;
};

TokenTagEditorWidget *asTokenTagEditor(QWidget *widget)
{
    return dynamic_cast<TokenTagEditorWidget *>(widget);
}

void installTokenCompleter(QLineEdit *lineEdit, const QStringList &values)
{
    if (lineEdit == nullptr) {
        return;
    }

    QStringList suggestions;
    for (const QString &value : values) {
        const QString normalized = value.trimmed();
        if (!normalized.isEmpty()) {
            suggestions.append(normalized);
        }
    }
    suggestions.removeDuplicates();
    std::sort(suggestions.begin(), suggestions.end(), [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });

    if (suggestions.isEmpty()) {
        lineEdit->setCompleter(nullptr);
        return;
    }

    auto *completer = new QCompleter(suggestions, lineEdit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    lineEdit->setCompleter(completer);
    const char *kTokenCompleterTextSnapshotProperty = "_tokenCompleterTextSnapshot";
    const char *kTokenCompleterCursorSnapshotProperty = "_tokenCompleterCursorSnapshot";

    auto snapshotEditorState = [lineEdit,
                                kTokenCompleterTextSnapshotProperty,
                                kTokenCompleterCursorSnapshotProperty]() {
        lineEdit->setProperty(kTokenCompleterTextSnapshotProperty, lineEdit->text());
        lineEdit->setProperty(kTokenCompleterCursorSnapshotProperty, lineEdit->cursorPosition());
    };
    snapshotEditorState();

    auto currentTokenAtCursor = [lineEdit]() -> QString {
        const QString text = lineEdit->text();
        int cursor = qBound(0, lineEdit->cursorPosition(), text.size());
        int tokenStart = cursor;
        while (tokenStart > 0 && !text.at(tokenStart - 1).isSpace()) {
            --tokenStart;
        }
        int tokenEnd = cursor;
        while (tokenEnd < text.size() && !text.at(tokenEnd).isSpace()) {
            ++tokenEnd;
        }
        return text.mid(tokenStart, tokenEnd - tokenStart).trimmed();
    };

    auto refreshCompletionPopup = [lineEdit, completer, currentTokenAtCursor]() {
        const QString token = currentTokenAtCursor();
        completer->setCompletionPrefix(token);
        if (lineEdit->hasFocus()) {
            completer->complete();
        }
    };

    auto scheduleRefresh = [lineEdit,
                            completer,
                            refreshCompletionPopup,
                            kTokenCompleterTextSnapshotProperty,
                            kTokenCompleterCursorSnapshotProperty]() {
        QTimer::singleShot(0, lineEdit, [lineEdit,
                                         completer,
                                         refreshCompletionPopup,
                                         kTokenCompleterTextSnapshotProperty,
                                         kTokenCompleterCursorSnapshotProperty]() {
            if (lineEdit == nullptr || completer == nullptr) {
                return;
            }
            if (lineEdit->completer() != completer) {
                return;
            }
            lineEdit->setProperty(kTokenCompleterTextSnapshotProperty, lineEdit->text());
            lineEdit->setProperty(kTokenCompleterCursorSnapshotProperty, lineEdit->cursorPosition());
            refreshCompletionPopup();
        });
    };

    QObject::connect(lineEdit, &QLineEdit::textChanged, completer, [scheduleRefresh](const QString &) {
        scheduleRefresh();
    });
    QObject::connect(lineEdit, &QLineEdit::cursorPositionChanged, completer, [scheduleRefresh](int, int) {
        scheduleRefresh();
    });
    QObject::connect(completer,
                     QOverload<const QString &>::of(&QCompleter::activated),
                     lineEdit,
                     [lineEdit,
                      snapshotEditorState,
                      kTokenCompleterTextSnapshotProperty,
                      kTokenCompleterCursorSnapshotProperty](const QString &choice) {
                         QString text = lineEdit->property(kTokenCompleterTextSnapshotProperty).toString();
                         if (text.isNull()) {
                             text = lineEdit->text();
                         }
                         int cursor = lineEdit->property(kTokenCompleterCursorSnapshotProperty).toInt();
                         cursor = qBound(0, cursor, text.size());
                         int tokenStart = cursor;
                         while (tokenStart > 0 && !text.at(tokenStart - 1).isSpace()) {
                             --tokenStart;
                         }
                         int tokenEnd = cursor;
                         while (tokenEnd < text.size() && !text.at(tokenEnd).isSpace()) {
                             ++tokenEnd;
                         }
                         text.replace(tokenStart, tokenEnd - tokenStart, choice);
                         lineEdit->setText(text);
                         lineEdit->setCursorPosition(tokenStart + choice.size());
                         snapshotEditorState();
                     });
}

struct DataHeaderComponents
{
    QString style;
    QString readingsOrder;
};

DataHeaderComponents parseDataHeaderComponents(const QStringList &tokens)
{
    DataHeaderComponents components;
    if (tokens.size() <= 1) {
        return components;
    }

    components.style = tokens.at(1).trimmed();
    if (tokens.size() > 2) {
        components.readingsOrder = tokens.mid(2).join(QLatin1Char(' ')).trimmed();
    }
    return components;
}

QStringList optionArgumentLabelsFromSignature(const QString &signature)
{
    QStringList labels;
    static const QRegularExpression placeholderPattern(QStringLiteral(R"(<([^>]+)>)"));
    QRegularExpressionMatchIterator iterator = placeholderPattern.globalMatch(signature);
    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        const QString placeholder = match.captured(1).trimmed();
        if (placeholder.isEmpty()) {
            continue;
        }
        QString label = placeholder.toLower();
        QStringList words = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (QString &word : words) {
            if (!word.isEmpty()) {
                word[0] = word.at(0).toUpper();
            }
        }
        label = words.join(QLatin1Char(' '));
        if (!label.isEmpty()) {
            labels.append(label);
        }
    }
    return labels;
}

constexpr const char *kBlockMimeType = "application/x-therion-block-kind";

class BlockToolboxList final : public QListWidget
{
public:
    explicit BlockToolboxList(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        setDragEnabled(true);
        setSelectionMode(QAbstractItemView::SingleSelection);
    }

protected:
    void startDrag(Qt::DropActions supportedActions) override
    {
        Q_UNUSED(supportedActions);
        QListWidgetItem *item = currentItem();
        if (item == nullptr) {
            return;
        }

        const QString kind = item->data(Qt::UserRole).toString().trimmed();
        if (kind.isEmpty()) {
            return;
        }

        auto *mimeData = new QMimeData;
        mimeData->setData(QString::fromUtf8(kBlockMimeType), kind.toUtf8());
        mimeData->setText(item->text());

        auto *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec(Qt::CopyAction);
    }
};

class BlockDetailsOptionsTableDelegate final : public QStyledItemDelegate
{
public:
    using SuggestionsProvider = std::function<QStringList(const QModelIndex &)>;

    explicit BlockDetailsOptionsTableDelegate(SuggestionsProvider provider, QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , provider_(std::move(provider))
    {
    }

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
        auto *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit == nullptr || !provider_) {
            return editor;
        }

        QStringList suggestions = provider_(index);
        for (QString &entry : suggestions) {
            entry = entry.trimmed();
        }
        suggestions.removeAll(QString());
        suggestions.removeDuplicates();
        std::sort(suggestions.begin(), suggestions.end(), [](const QString &left, const QString &right) {
            return QString::compare(left, right, Qt::CaseInsensitive) < 0;
        });
        if (suggestions.isEmpty()) {
            return editor;
        }

        auto *completer = new QCompleter(suggestions, lineEdit);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        lineEdit->setCompleter(completer);
        return editor;
    }

private:
    SuggestionsProvider provider_;
};

class BlockCanvasView final : public QGraphicsView
{
public:
    explicit BlockCanvasView(QWidget *parent = nullptr)
        : QGraphicsView(parent)
    {
        setAcceptDrops(true);
        setDragMode(QGraphicsView::RubberBandDrag);
        setRenderHint(QPainter::Antialiasing, true);
    }

    std::function<void(const QString &, const QPointF &)> onDropBlock;
    std::function<void(const QString &, const QPointF &, bool)> onDragPreview;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event == nullptr || event->mimeData() == nullptr) {
            return;
        }
        if (event->mimeData()->hasFormat(QString::fromUtf8(kBlockMimeType))) {
            event->acceptProposedAction();
            return;
        }
        QGraphicsView::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event == nullptr || event->mimeData() == nullptr) {
            return;
        }
        if (event->mimeData()->hasFormat(QString::fromUtf8(kBlockMimeType))) {
            if (onDragPreview) {
                const QString kind = QString::fromUtf8(event->mimeData()->data(QString::fromUtf8(kBlockMimeType))).trimmed();
                onDragPreview(kind, mapToScene(event->position().toPoint()), true);
            }
            event->acceptProposedAction();
            return;
        }
        QGraphicsView::dragMoveEvent(event);
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        if (onDragPreview) {
            onDragPreview(QString(), QPointF(), false);
        }
        QGraphicsView::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        if (event == nullptr || event->mimeData() == nullptr) {
            return;
        }
        if (!event->mimeData()->hasFormat(QString::fromUtf8(kBlockMimeType))) {
            QGraphicsView::dropEvent(event);
            return;
        }

        const QString kind = QString::fromUtf8(event->mimeData()->data(QString::fromUtf8(kBlockMimeType))).trimmed();
        if (kind.isEmpty()) {
            return;
        }

        if (onDropBlock) {
            const QPointF scenePosition = mapToScene(event->position().toPoint());
            onDropBlock(kind, scenePosition);
        }
        if (onDragPreview) {
            onDragPreview(QString(), QPointF(), false);
        }
        event->acceptProposedAction();
    }
};

class BlockCanvasItem final : public QGraphicsObject
{
public:
    BlockCanvasItem(const QString &kind,
                    const QString &name,
                    const QString &inlineComment,
                    int lineNumber,
                    bool deletable = true,
                    bool movable = true,
                    bool isContainerOpen = false,
                    QGraphicsItem *parent = nullptr)
        : QGraphicsObject(parent)
        , kind_(kind)
        , name_(name)
        , inlineComment_(inlineComment)
        , lineNumber_(lineNumber)
        , deletable_(deletable)
        , movable_(movable)
        , isContainerOpen_(isContainerOpen)
    {
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemIsMovable, movable_);
        setCursor(movable_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        setToolTip(inlineComment_.trimmed().isEmpty() ? QString() : inlineComment_.trimmed());
    }

    std::function<void(int)> onDelete;
    std::function<void(int, const QPointF &)> onMoveRequest;
    std::function<void(int, const QPointF &, bool)> onMovePreview;

    QRectF boundingRect() const override
    {
        return QRectF(0.0, 0.0, 260.0, 42.0);
    }

    void setName(const QString &name)
    {
        name_ = name;
        update();
    }

    void setInlineComment(const QString &inlineComment)
    {
        inlineComment_ = inlineComment;
        setToolTip(inlineComment_.trimmed().isEmpty() ? QString() : inlineComment_.trimmed());
        update();
    }

    QString kind() const
    {
        return kind_;
    }

    int lineNumber() const
    {
        return lineNumber_;
    }

    void setLineNumber(int lineNumber)
    {
        lineNumber_ = lineNumber;
    }

    bool isContainerOpen() const
    {
        return isContainerOpen_;
    }

protected:
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget) override
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);
        if (painter == nullptr) {
            return;
        }

        const QColor baseColor = blockBaseColorForDirective(kind_);

        painter->setPen(QPen(isSelected() ? QColor(QStringLiteral("#2f6fed")) : QColor(QStringLiteral("#8c8c8c")), isSelected() ? 2.0 : 1.0));
        painter->setBrush(baseColor);
        painter->drawRoundedRect(boundingRect(), 6.0, 6.0);

        QRectF deleteButtonRect;
        if (deletable_) {
            deleteButtonRect = deleteIconRect();
            painter->setPen(QPen(QColor(QStringLiteral("#6a6a6a")), 1.0));
            painter->setBrush(QColor(QStringLiteral("#f2f2f2")));
            painter->drawRoundedRect(deleteButtonRect, 4.0, 4.0);

            const QRect deleteIconBounds = deleteButtonRect.adjusted(4.0, 4.0, -4.0, -4.0).toRect();
            if (QStyle *style = QApplication::style(); style != nullptr) {
                style->standardIcon(QStyle::SP_TrashIcon).paint(painter, deleteIconBounds);
            }
        }

        QRectF commentBadgeRect;
        if (!inlineComment_.trimmed().isEmpty()) {
            commentBadgeRect = inlineCommentBadgeRect();
            painter->setPen(QPen(QColor(QStringLiteral("#7c6a42")), 1.0));
            painter->setBrush(QColor(QStringLiteral("#fff1c9")));
            painter->drawRoundedRect(commentBadgeRect, 4.0, 4.0);
            painter->setPen(QColor(QStringLiteral("#5f4a1e")));
            painter->drawText(commentBadgeRect, Qt::AlignCenter, QStringLiteral("#"));
        }

        const QString title = name_.isEmpty() ? kind_ : QStringLiteral("%1: %2").arg(kind_, name_);
        painter->setPen(QColor(QStringLiteral("#1f1f1f")));
        qreal textRight = deletable_ ? deleteButtonRect.left() - 16.0 : boundingRect().right() - 10.0;
        if (!commentBadgeRect.isNull()) {
            textRight = qMin(textRight, commentBadgeRect.left() - 10.0);
        }
        painter->drawText(QRectF(10.0, 0.0, textRight, boundingRect().height()),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          title);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (deletable_ && event != nullptr && deleteIconRect().contains(event->pos())) {
            if (onDelete) {
                onDelete(lineNumber_);
            }
            event->accept();
            return;
        }

        dragStartScenePos_ = event != nullptr ? event->scenePos() : QPointF();
        dragStartItemPos_ = pos();
        dragging_ = false;
        if (onMovePreview) {
            onMovePreview(lineNumber_, QPointF(), false);
        }
        if (!movable_) {
            QGraphicsObject::mousePressEvent(event);
            return;
        }
        QGraphicsObject::mousePressEvent(event);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (!movable_) {
            QGraphicsObject::mouseMoveEvent(event);
            return;
        }
        if (event != nullptr && (event->buttons() & Qt::LeftButton)) {
            if (!dragging_ && (event->scenePos() - dragStartScenePos_).manhattanLength() >= 4.0) {
                dragging_ = true;
                setCursor(Qt::ClosedHandCursor);
            }
        }
        QGraphicsObject::mouseMoveEvent(event);
        if (dragging_ && onMovePreview) {
            const QPointF previewScenePos = event != nullptr
                ? event->scenePos()
                : mapToScene(boundingRect().center());
            onMovePreview(lineNumber_, previewScenePos, true);
        }
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        QGraphicsObject::mouseReleaseEvent(event);
        setCursor(movable_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        if (!movable_) {
            if (onMovePreview) {
                onMovePreview(lineNumber_, QPointF(), false);
            }
            dragging_ = false;
            return;
        }
        const bool moved = (pos() - dragStartItemPos_).manhattanLength() >= 2.0;
        if (!moved) {
            if (onMovePreview) {
                onMovePreview(lineNumber_, QPointF(), false);
            }
            dragging_ = false;
            return;
        }

        const QPointF dropScenePos = event != nullptr
            ? event->scenePos()
            : mapToScene(boundingRect().center());
        setPos(dragStartItemPos_);
        dragging_ = false;
        if (onMoveRequest) {
            onMoveRequest(lineNumber_, dropScenePos);
        }
    }

private:
    QRectF deleteIconRect() const
    {
        const qreal iconSize = 24.0;
        const qreal margin = 8.0;
        const qreal x = boundingRect().right() - margin - iconSize;
        return QRectF(x, 9.0, iconSize, iconSize);
    }

    QRectF inlineCommentBadgeRect() const
    {
        const qreal badgeWidth = 22.0;
        const qreal badgeHeight = 20.0;
        const qreal margin = 8.0;
        const qreal rightAnchor = deletable_ ? deleteIconRect().left() - 8.0 : boundingRect().right() - margin;
        return QRectF(rightAnchor - badgeWidth, 11.0, badgeWidth, badgeHeight);
    }

    QString kind_;
    QString name_;
    QString inlineComment_;
    int lineNumber_ = 0;
    bool deletable_ = true;
    bool movable_ = true;
    bool isContainerOpen_ = false;
    QPointF dragStartScenePos_;
    QPointF dragStartItemPos_;
    bool dragging_ = false;
};

class DataRowsTableWidget final : public QTableWidget
{
public:
    explicit DataRowsTableWidget(QWidget *parent = nullptr)
        : QTableWidget(parent)
    {
    }

    std::function<void()> onViewportResized;

    void setAdvanceColumns(const QSet<int> &columns)
    {
        advanceColumns_ = columns;
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QTableWidget::resizeEvent(event);
        if (onViewportResized) {
            onViewportResized();
        }
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event != nullptr
            && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            && (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) == 0
            && columnCount() > 0
            && (advanceColumns_.isEmpty() ? (currentColumn() == columnCount() - 1)
                                          : advanceColumns_.contains(currentColumn()))) {
            const int currentRowIndex = qMax(0, currentRow());
            if (currentRowIndex >= rowCount() - 1) {
                insertRow(rowCount());
            }

            const int nextRow = currentRowIndex + 1;
            const int targetColumn = qBound(0, 1, qMax(0, columnCount() - 1));
            if (item(nextRow, targetColumn) == nullptr) {
                setItem(nextRow, targetColumn, new QTableWidgetItem);
            }
            setCurrentCell(nextRow, targetColumn);
            editItem(item(nextRow, targetColumn));
            event->accept();
            return;
        }

        QTableWidget::keyPressEvent(event);
    }

private:
    QSet<int> advanceColumns_;
};

class DataRowsTableDelegate final : public QStyledItemDelegate
{
public:
    using SuggestionsProvider = std::function<QStringList(const QModelIndex &)>;
    using EditableProvider = std::function<bool(const QModelIndex &)>;

    explicit DataRowsTableDelegate(SuggestionsProvider suggestionsProvider,
                                   EditableProvider editableProvider,
                                   QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , suggestionsProvider_(std::move(suggestionsProvider))
        , editableProvider_(std::move(editableProvider))
    {
    }

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        if (index.isValid() && index.column() == 0) {
            auto *combo = new QComboBox(parent);
            combo->addItem(QStringLiteral("Data"));
            combo->addItem(QStringLiteral("Directive"));
            combo->addItem(QStringLiteral("Comment"));
            return combo;
        }

        if (editableProvider_ && !editableProvider_(index)) {
            return nullptr;
        }

        QWidget *editor = QStyledItemDelegate::createEditor(parent, option, index);
        auto *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit == nullptr || !suggestionsProvider_) {
            return editor;
        }

        const QStringList suggestions = suggestionsProvider_(index);
        if (!suggestions.isEmpty()) {
            installLineEditCompleter(lineEdit, suggestions);
        }
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor); combo != nullptr) {
            const QString value = index.data(Qt::EditRole).toString();
            const int found = combo->findText(value, Qt::MatchFixedString | Qt::MatchCaseSensitive);
            combo->setCurrentIndex(found >= 0 ? found : 0);
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        if (auto *combo = qobject_cast<QComboBox *>(editor); combo != nullptr) {
            model->setData(index, combo->currentText(), Qt::EditRole);
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }

private:
    SuggestionsProvider suggestionsProvider_;
    EditableProvider editableProvider_;
};

QColor blockBaseColorForDirective(const QString &directive)
{
    const QString normalizedKind = normalizeDirective(directive);
    if (normalizedKind == QStringLiteral("survey")) {
        return QColor(QStringLiteral("#d7f7d7"));
    }
    if (normalizedKind == QStringLiteral("centerline")) {
        return QColor(QStringLiteral("#ffe9cc"));
    }
    if (normalizedKind == QStringLiteral("data")) {
        return QColor(QStringLiteral("#e8dbff"));
    }
    if (normalizedKind == QStringLiteral("map")) {
        return QColor(QStringLiteral("#d5f3f0"));
    }
    return QColor(QStringLiteral("#d8e9ff"));
}

void syncTextBrowserSurfaceToParent(QWidget *browserWidget)
{
    QWidget *parent = browserWidget != nullptr ? browserWidget->parentWidget() : nullptr;
    syncTextBrowserSurfaceToPalette(browserWidget,
                                    parent != nullptr ? parent->palette() : QApplication::palette(browserWidget));
}

void syncTextBrowserSurfaceToPalette(QWidget *browserWidget, const QPalette &sourcePalette)
{
    auto *browser = qobject_cast<QTextBrowser *>(browserWidget);
    if (browser == nullptr) {
        return;
    }

    QColor surfaceColor = sourcePalette.color(QPalette::Base);
    if (!surfaceColor.isValid()) {
        surfaceColor = sourcePalette.color(QPalette::Window);
    }
    QColor textColor = sourcePalette.color(QPalette::Text);
    if (!textColor.isValid()) {
        textColor = sourcePalette.color(QPalette::WindowText);
    }
    QColor linkColor = sourcePalette.color(QPalette::Link);
    if (!linkColor.isValid()) {
        linkColor = textColor;
    }
    QPalette browserPalette = sourcePalette;
    browserPalette.setColor(QPalette::Base, surfaceColor);
    browserPalette.setColor(QPalette::Window, surfaceColor);
    browserPalette.setColor(QPalette::AlternateBase, surfaceColor);
    browserPalette.setColor(QPalette::Text, textColor);
    browserPalette.setColor(QPalette::WindowText, textColor);
    browserPalette.setColor(QPalette::ButtonText, textColor);
    browserPalette.setColor(QPalette::Link, linkColor);
    browser->setPalette(browserPalette);
    browser->setAutoFillBackground(true);
    browser->setStyleSheet(QStringLiteral(
                                "QTextBrowser {"
                                " background-color: %1;"
                                " color: %2;"
                                " border: none;"
                                "}"
                                "QTextBrowser:viewport {"
                                " background-color: %1;"
                                "}").arg(surfaceColor.name(QColor::HexRgb),
                                          textColor.name(QColor::HexRgb)));

    if (QWidget *viewport = browser->viewport(); viewport != nullptr) {
        QPalette viewportPalette = sourcePalette;
        viewportPalette.setColor(QPalette::Base, surfaceColor);
        viewportPalette.setColor(QPalette::Window, surfaceColor);
        viewportPalette.setColor(QPalette::AlternateBase, surfaceColor);
        viewportPalette.setColor(QPalette::Text, textColor);
        viewportPalette.setColor(QPalette::WindowText, textColor);
        viewportPalette.setColor(QPalette::ButtonText, textColor);
        viewportPalette.setColor(QPalette::Link, linkColor);
        viewport->setPalette(viewportPalette);
        viewport->setAutoFillBackground(true);
        viewport->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                     .arg(surfaceColor.name(QColor::HexRgb),
                                          textColor.name(QColor::HexRgb)));
    }

    if (QTextDocument *document = browser->document(); document != nullptr) {
        document->setDefaultStyleSheet(
            QStringLiteral(
                "body {"
                " color: %1;"
                " background-color: %2;"
                " margin: 0;"
                " line-height: 1.35;"
                "}"
                "h1, h2, h3, h4 { margin: 0 0 8px 0; }"
                "p { margin: 0 0 10px 0; }"
                "ul, ol { margin: 0 0 10px 20px; }"
                "li { margin: 0 0 4px 0; }"
                "a { color: %3; }"
                "code { color: %1; }")
                .arg(textColor.name(QColor::HexRgb),
                     surfaceColor.name(QColor::HexRgb),
                     linkColor.name(QColor::HexRgb)));
    }
}

void syncPanelSurfaceToBaseTone(QWidget *panelWidget)
{
    syncPanelSurfaceToPalette(panelWidget, QApplication::palette(panelWidget));
}

void syncPanelSurfaceToPalette(QWidget *panelWidget, const QPalette &sourcePalette)
{
    if (panelWidget == nullptr) {
        return;
    }

    QPalette panelPalette = sourcePalette;
    QColor surfaceColor = sourcePalette.color(QPalette::Base);
    if (!surfaceColor.isValid()) {
        surfaceColor = sourcePalette.color(QPalette::Window);
    }

    panelPalette.setColor(QPalette::Window, surfaceColor);
    panelPalette.setColor(QPalette::Base, surfaceColor);
    panelPalette.setColor(QPalette::AlternateBase, surfaceColor);
    panelWidget->setPalette(panelPalette);
    panelWidget->setAutoFillBackground(true);
    if (panelWidget->property("leftBorderOnly").toBool()) {
        if (panelWidget->objectName().isEmpty()) {
            panelWidget->setObjectName(QStringLiteral("leftBorderPanel"));
        }
        panelWidget->setStyleSheet(QStringLiteral(
                                       "QWidget#%1 {"
                                       " background-color: %2;"
                                       " color: %3;"
                                       " border-left: 1px solid palette(mid);"
                                       " border-right: none;"
                                       " border-top: none;"
                                       " border-bottom: none;"
                                       "}")
                                       .arg(panelWidget->objectName(),
                                            surfaceColor.name(QColor::HexRgb),
                                            panelPalette.color(QPalette::Text).name(QColor::HexRgb)));
        return;
    }

    panelWidget->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                   .arg(surfaceColor.name(QColor::HexRgb),
                                        panelPalette.color(QPalette::Text).name(QColor::HexRgb)));
}

}

namespace TherionStudio
{
TextEditorTab::TextEditorTab(QWidget *parent)
    : QWidget(parent)
{
    contextHelpController_ = std::make_unique<TextEditorContextHelpController>(this);
    rawEditorFindController_ = std::make_unique<RawEditorFindController>(this);
    rawEditorCompletionController_ = std::make_unique<RawEditorCompletionController>(this);
    blockDetailsOptionArgsController_ = std::make_unique<BlockEditorOptionArgsController>(this);
    blockDetailsHelpController_ = std::make_unique<BlockEditorDetailsHelpController>(this);
    blockDetailsLineBuildService_ = std::make_unique<BlockEditorLineBuildService>(this);
    blockDetailsApplyStateController_ = std::make_unique<BlockEditorApplyStateController>(this);
    blockDetailsApplyExecutor_ = std::make_unique<BlockEditorApplyExecutor>(this);
    blockDetailsSelectionDetailsController_ = std::make_unique<BlockEditorSelectionDetailsController>(this);
    blockDetailsPaneController_ = std::make_unique<BlockEditorDetailsPaneController>(this);
    blockDetailsToolboxDetailsController_ = std::make_unique<BlockEditorToolboxDetailsController>(this);
    blockDetailsCanvasSelectionController_ = std::make_unique<BlockEditorCanvasSelectionController>(this);
    blockDetailsMovePreviewController_ = std::make_unique<BlockEditorMovePreviewController>(this);
    blockDetailsCanvasBoundaryController_ = std::make_unique<BlockEditorCanvasBoundaryController>(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    searchBar_ = new QFrame(this);
    searchBar_->setFrameShape(QFrame::StyledPanel);
    searchBar_->setVisible(false);

    auto *searchLayout = new QVBoxLayout(searchBar_);
    searchLayout->setContentsMargins(kPanelPadding, kPanelPadding, kPanelPadding, kPanelPadding);
    searchLayout->setSpacing(kPanelSpacing);

    auto *findRow = new QHBoxLayout;
    findRow->setSpacing(kPanelSpacing);

    findEdit_ = new QLineEdit(searchBar_);
    findEdit_->setPlaceholderText(tr("Find"));

    findNextButton_ = new QPushButton(tr("Next"), searchBar_);
    findPreviousButton_ = new QPushButton(tr("Previous"), searchBar_);

    closeSearchButton_ = new QPushButton(tr("Close"), searchBar_);
    closeSearchButton_->setAutoDefault(false);

    searchStatusLabel_ = new QLabel(searchBar_);
    searchStatusLabel_->setMinimumWidth(140);

    findRow->addWidget(findEdit_, 1);
    findRow->addWidget(findPreviousButton_);
    findRow->addWidget(findNextButton_);
    findRow->addWidget(closeSearchButton_);
    findRow->addWidget(searchStatusLabel_);

    auto *optionRow = new QHBoxLayout;
    optionRow->setSpacing(kPanelSpacing);

    wholeWordCheck_ = new QCheckBox(tr("Whole word"), searchBar_);
    caseSensitiveCheck_ = new QCheckBox(tr("Case sensitive"), searchBar_);

    optionRow->addWidget(wholeWordCheck_);
    optionRow->addWidget(caseSensitiveCheck_);
    optionRow->addStretch(1);

    replaceRow_ = new QWidget(searchBar_);
    auto *replaceLayout = new QHBoxLayout(replaceRow_);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(kPanelSpacing);

    replaceEdit_ = new QLineEdit(replaceRow_);
    replaceEdit_->setPlaceholderText(tr("Replace with"));
    replaceButton_ = new QPushButton(tr("Replace"), replaceRow_);
    replaceAllButton_ = new QPushButton(tr("Replace All"), replaceRow_);

    replaceLayout->addWidget(replaceEdit_, 1);
    replaceLayout->addWidget(replaceButton_);
    replaceLayout->addWidget(replaceAllButton_);

    searchLayout->addLayout(findRow);
    searchLayout->addLayout(optionRow);
    searchLayout->addWidget(replaceRow_);

    connect(findEdit_, &QLineEdit::textEdited, this, &TextEditorTab::handleFindTextEdited);
    connect(replaceEdit_, &QLineEdit::textEdited, this, &TextEditorTab::handleReplaceTextEdited);
    connect(wholeWordCheck_, &QCheckBox::toggled, this, &TextEditorTab::handleSearchOptionsChanged);
    connect(caseSensitiveCheck_, &QCheckBox::toggled, this, &TextEditorTab::handleSearchOptionsChanged);
    connect(findNextButton_, &QPushButton::clicked, this, &TextEditorTab::handleFindNextTriggered);
    connect(findPreviousButton_, &QPushButton::clicked, this, &TextEditorTab::handleFindPreviousTriggered);
    connect(replaceButton_, &QPushButton::clicked, this, &TextEditorTab::handleReplaceTriggered);
    connect(replaceAllButton_, &QPushButton::clicked, this, &TextEditorTab::handleReplaceAllTriggered);
    connect(closeSearchButton_, &QPushButton::clicked, this, &TextEditorTab::handleCloseSearchTriggered);

    editor_ = new HighlightPlainTextEdit(this);
    editor_->setFrameShape(QFrame::NoFrame);
    editor_->setObjectName(QStringLiteral("rawTextEditorCanvas"));
    editor_->setStyleSheet(QStringLiteral(
        "QPlainTextEdit#rawTextEditorCanvas {"
        " border-left: none;"
        " border-right: 1px solid palette(mid);"
        " border-top: none;"
        " border-bottom: none;"
        "}"));
    editor_->setTabChangesFocus(false);
    editor_->setTabStopDistance(QFontMetricsF(editor_->font()).horizontalAdvance(QLatin1Char(' ')) * 4.0);
    editor_->setPlaceholderText(tr("Open a Therion text file to begin editing."));
    highlighter_ = new TherionSyntaxHighlighter(editor_->document());

    completionModel_ = new QStringListModel(this);
    completionCompleter_ = new QCompleter(completionModel_, this);
    completionCompleter_->setWidget(editor_);
    completionCompleter_->setCaseSensitivity(Qt::CaseInsensitive);
    completionCompleter_->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    completionCompleter_->setCompletionMode(QCompleter::PopupCompletion);
    connect(completionCompleter_,
            QOverload<const QString &>::of(&QCompleter::activated),
            this,
            &TextEditorTab::insertCompletionToken);
    editor_->installEventFilter(this);
    editor_->viewport()->installEventFilter(this);
    if (completionCompleter_->popup() != nullptr) {
        completionCompleter_->popup()->setFocusPolicy(Qt::NoFocus);
        completionCompleter_->popup()->installEventFilter(this);
    }

    loadHelpMetadata();

    statusRow_ = new QWidget(this);
    auto *statusLayout = new QHBoxLayout(statusRow_);
    statusLayout->setContentsMargins(kPanelPadding, 0, kPanelPadding, kPanelPadding);
    statusLayout->setSpacing(kPanelSpacing);

    encodingNoteLabel_ = new QLabel(statusRow_);
    encodingNoteLabel_->setWordWrap(true);
    encodingNoteLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    encodingNoteLabel_->setVisible(false);
    convertEncodingButton_ = new QPushButton(tr("Convert to UTF-8"), statusRow_);
    convertEncodingButton_->setVisible(false);
    convertEncodingButton_->setEnabled(false);
    convertEncodingButton_->setAutoDefault(false);
    connect(convertEncodingButton_, &QPushButton::clicked, this, &TextEditorTab::handleConvertToUtf8Triggered);

    statusLayout->addWidget(encodingNoteLabel_, 1);
    statusLayout->addWidget(convertEncodingButton_);

    editorHelpSplitter_ = new QSplitter(Qt::Horizontal, this);
    editorHelpSplitter_->setChildrenCollapsible(false);
    applyThinSplitterStyle(editorHelpSplitter_, QStringLiteral("textEditorHelpSplitter"));
    editorHelpSplitter_->addWidget(editor_);

    buildHelpPanel();
    helpPanel_->setMinimumWidth(kBlocksSidePaneMinWidth);
    helpPanel_->setMaximumWidth(kBlocksSidePaneMaxWidth);
    editorHelpSplitter_->addWidget(helpPanel_);
    editorHelpSplitter_->setStretchFactor(0, 1);
    editorHelpSplitter_->setStretchFactor(1, 0);
    editorHelpSplitter_->setCollapsible(1, true);
    editorHelpSplitter_->setSizes({980, 380});

    rawEditorPanel_ = new QWidget(this);
    auto *rawEditorLayout = new QHBoxLayout(rawEditorPanel_);
    rawEditorLayout->setContentsMargins(0, 0, 0, 0);
    rawEditorLayout->setSpacing(kPanelSpacing);
    rawEditorLayout->addWidget(editorHelpSplitter_, 1);

    modeRow_ = new QWidget(this);
    auto *modeLayout = new QHBoxLayout(modeRow_);
    modeLayout->setContentsMargins(kPanelPadding, kPanelSpacing, kPanelPadding, kPanelSpacing);
    modeLayout->setSpacing(kPanelSpacing);
    modeLayout->addWidget(new QLabel(tr("Mode:"), modeRow_));
    rawModeButton_ = new QPushButton(tr("Raw"), modeRow_);
    rawModeButton_->setCheckable(true);
    blocksModeButton_ = new QPushButton(tr("Blocks"), modeRow_);
    blocksModeButton_->setCheckable(true);
    blocksModeButton_->setToolTip(tr("Structured block canvas for .th and .thconfig files."));
    modeLayout->addWidget(rawModeButton_);
    modeLayout->addWidget(blocksModeButton_);
    modeLayout->addStretch(1);
    modeRow_->setMaximumHeight(modeSelectorRequestedVisible_ ? QWIDGETSIZE_MAX : 0);

    blocksPanel_ = new QWidget(this);
    auto *blocksLayout = new QHBoxLayout(blocksPanel_);
    blocksLayout->setContentsMargins(0, 0, 0, 0);
    blocksLayout->setSpacing(kPanelSpacing);

    auto *blocksSplitter = new QSplitter(Qt::Horizontal, blocksPanel_);
    blocksSplitter->setChildrenCollapsible(false);
    applyThinSplitterStyle(blocksSplitter, QStringLiteral("textBlocksSplitter"));

    auto *toolboxColumn = new QWidget(blocksSplitter);
    toolboxColumn->setObjectName(QStringLiteral("blocksToolboxPane"));
    toolboxColumn->setAttribute(Qt::WA_StyledBackground, true);
    toolboxColumn->setStyleSheet(QStringLiteral(
        "QWidget#blocksToolboxPane {"
        " border-left: none;"
        " border-right: none;"
        " border-top: none;"
        " border-bottom: none;"
        "}"));
    auto *toolboxColumnLayout = new QVBoxLayout(toolboxColumn);
    toolboxColumnLayout->setContentsMargins(kPanelPadding, kPanelPadding, kPanelPadding, kPanelPadding);
    toolboxColumnLayout->setSpacing(kPanelSpacing);
    blockToolboxScopeCombo_ = new QComboBox(toolboxColumn);
    populateBlockToolboxScopeCombo();
    blockToolboxFilterEdit_ = new QLineEdit(toolboxColumn);
    blockToolboxFilterEdit_->setPlaceholderText(tr("Filter commands..."));

    blockToolboxList_ = new BlockToolboxList(toolboxColumn);
    blockToolboxList_->setMinimumWidth(180);
    toolboxColumnLayout->addWidget(blockToolboxScopeCombo_);
    toolboxColumnLayout->addWidget(blockToolboxFilterEdit_);
    toolboxColumnLayout->addWidget(blockToolboxList_, 1);
    populateBlockToolbox();
    updateBlockToolboxScopeLabel();
    connect(blockToolboxScopeCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        populateBlockToolbox();
    });
    connect(blockToolboxFilterEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        populateBlockToolbox();
    });

    blockCanvasScene_ = new QGraphicsScene(blocksSplitter);
    auto *typedCanvasView = new BlockCanvasView(blocksSplitter);
    typedCanvasView->setFrameShape(QFrame::NoFrame);
    typedCanvasView->setObjectName(QStringLiteral("blocksCanvasView"));
    typedCanvasView->setStyleSheet(QStringLiteral(
        "QGraphicsView#blocksCanvasView {"
        " border-left: none;"
        " border-right: 1px solid palette(mid);"
        " border-top: none;"
        " border-bottom: none;"
        "}"));
    typedCanvasView->setScene(blockCanvasScene_);
    typedCanvasView->setSceneRect(QRectF(0.0, 0.0, 1400.0, 2000.0));
    typedCanvasView->setBackgroundBrush(palette().color(QPalette::Base));
    typedCanvasView->onDropBlock = [this](const QString &kind, const QPointF &scenePos) {
        handleCanvasDrop(kind, scenePos);
    };
    typedCanvasView->onDragPreview = [this](const QString &, const QPointF &scenePos, bool active) {
        if (active) {
            updateBlockMovePreview(-1, scenePos);
        } else {
            clearBlockMovePreview();
        }
    };
    blockCanvasView_ = typedCanvasView;

    blockDetailsPanel_ = new QFrame(blocksSplitter);
    blockDetailsPanel_->setFrameShape(QFrame::NoFrame);
    blockDetailsPanel_->setMinimumWidth(kBlocksSidePaneMinWidth);
    blockDetailsPanel_->setMaximumWidth(kBlocksSidePaneMaxWidth);
    syncPanelSurfaceToBaseTone(blockDetailsPanel_);
    auto *blockDetailsLayout = new QVBoxLayout(blockDetailsPanel_);
    blockDetailsLayout->setContentsMargins(kPanelPadding, kPanelPadding, kPanelPadding, kPanelPadding);
    blockDetailsLayout->setSpacing(kPanelSpacing);

    blockDetailsEditPanel_ = new QWidget(blockDetailsPanel_);
    syncPanelSurfaceToBaseTone(blockDetailsEditPanel_);
    auto *blockDetailsEditLayout = new QVBoxLayout(blockDetailsEditPanel_);
    blockDetailsEditLayout->setContentsMargins(0, 0, 0, 0);
    blockDetailsEditLayout->setSpacing(kPanelSpacing);

    auto *blockDetailsHeader = new QLabel(tr("Block Details"), blockDetailsEditPanel_);
    QFont blockDetailsHeaderFont = blockDetailsHeader->font();
    blockDetailsHeaderFont.setBold(true);
    blockDetailsHeader->setFont(blockDetailsHeaderFont);
    blockDetailsEditLayout->addWidget(blockDetailsHeader);

    blockDetailsStatusLabel_ = new QLabel(blockDetailsEditPanel_);
    blockDetailsStatusLabel_->setObjectName(QStringLiteral("blockDetailsStatusLabel"));
    blockDetailsStatusLabel_->setWordWrap(true);
    blockDetailsEditLayout->addWidget(blockDetailsStatusLabel_);

    auto *blockDetailsFormLayout = new QFormLayout;
    blockDetailsFormLayout->setContentsMargins(0, 0, 0, 0);
    blockDetailsFormLayout->setSpacing(kPanelSpacing);
    blockDetailsPrimaryFieldLabel_ = new QLabel(tr("ID"), blockDetailsEditPanel_);
    blockDetailsPrimaryFieldLabel_->setObjectName(QStringLiteral("blockDetailsPrimaryLabel"));
    blockDetailsIdEdit_ = new QLineEdit(blockDetailsEditPanel_);
    blockDetailsIdEdit_->setObjectName(QStringLiteral("blockDetailsPrimaryEdit"));
    blockDetailsFormLayout->addRow(blockDetailsPrimaryFieldLabel_, blockDetailsIdEdit_);
    blockDetailsSecondaryFieldLabel_ = new QLabel(tr("Extra Arguments (Advanced)"), blockDetailsEditPanel_);
    blockDetailsSecondaryFieldLabel_->setObjectName(QStringLiteral("blockDetailsSecondaryLabel"));
    blockDetailsSecondaryFieldStack_ = new QStackedWidget(blockDetailsEditPanel_);
    blockDetailsSecondaryFieldStack_->setObjectName(QStringLiteral("blockDetailsSecondaryFieldStack"));
    blockDetailsAdditionalPositionalEdit_ = new QLineEdit(blockDetailsSecondaryFieldStack_);
    blockDetailsAdditionalPositionalEdit_->setObjectName(QStringLiteral("blockDetailsSecondaryEdit"));
    blockDetailsAdditionalPositionalEdit_->setToolTip(
        tr("Rare fallback for unsupported positional tokens. Prefer explicit options where available."));
    blockDetailsAdditionalPositionalEdit_->setPlaceholderText(tr("shown only when present in source"));
    blockDetailsSecondaryFieldStack_->addWidget(blockDetailsAdditionalPositionalEdit_);
    blockDetailsReadingsTagEditor_ = new TokenTagEditorWidget(blockDetailsSecondaryFieldStack_);
    blockDetailsReadingsTagEditor_->setObjectName(QStringLiteral("blockDetailsReadingsTagEditor"));
    blockDetailsSecondaryFieldStack_->addWidget(blockDetailsReadingsTagEditor_);
    blockDetailsSecondaryFieldStack_->setCurrentWidget(blockDetailsAdditionalPositionalEdit_);
    blockDetailsFormLayout->addRow(blockDetailsSecondaryFieldLabel_, blockDetailsSecondaryFieldStack_);
    blockDetailsCommentFieldLabel_ = new QLabel(tr("Comment"), blockDetailsEditPanel_);
    blockDetailsCommentFieldLabel_->setObjectName(QStringLiteral("blockDetailsCommentLabel"));
    blockDetailsCommentEdit_ = new QLineEdit(blockDetailsEditPanel_);
    blockDetailsCommentEdit_->setObjectName(QStringLiteral("blockDetailsCommentEdit"));
    blockDetailsCommentEdit_->setPlaceholderText(tr("optional"));
    blockDetailsFormLayout->addRow(blockDetailsCommentFieldLabel_, blockDetailsCommentEdit_);
    blockDetailsEditLayout->addLayout(blockDetailsFormLayout);

    blockDetailsAddOptionButton_ = new QPushButton(QStringLiteral("+"), blockDetailsEditPanel_);
    blockDetailsRemoveOptionButton_ = new QPushButton(QStringLiteral("-"), blockDetailsEditPanel_);
    blockDetailsAddOptionButton_->setObjectName(QStringLiteral("blockDetailsAddOptionButton"));
    blockDetailsRemoveOptionButton_->setObjectName(QStringLiteral("blockDetailsRemoveOptionButton"));
    blockDetailsAddOptionButton_->setAutoDefault(false);
    blockDetailsRemoveOptionButton_->setAutoDefault(false);
    blockDetailsAddOptionButton_->setToolTip(tr("Add option"));
    blockDetailsRemoveOptionButton_->setToolTip(tr("Remove selected option"));
    blockDetailsAddOptionButton_->setFixedWidth(28);
    blockDetailsRemoveOptionButton_->setFixedWidth(28);

    auto *blockDetailsOptionsHeaderRow = new QHBoxLayout;
    blockDetailsOptionsHeaderRow->setContentsMargins(0, 0, 0, 0);
    blockDetailsOptionsHeaderRow->setSpacing(kPanelSpacing);
    blockDetailsOptionsLabel_ = new QLabel(tr("Options"), blockDetailsEditPanel_);
    blockDetailsOptionsLabel_->setObjectName(QStringLiteral("blockDetailsOptionsLabel"));
    blockDetailsOptionsHeaderRow->addWidget(blockDetailsOptionsLabel_);
    blockDetailsOptionsHeaderRow->addStretch(1);
    blockDetailsOptionsHeaderRow->addWidget(blockDetailsAddOptionButton_);
    blockDetailsOptionsHeaderRow->addWidget(blockDetailsRemoveOptionButton_);
    blockDetailsEditLayout->addLayout(blockDetailsOptionsHeaderRow);

    blockDetailsOptionsTable_ = new QTableWidget(blockDetailsEditPanel_);
    blockDetailsOptionsTable_->setObjectName(QStringLiteral("blockDetailsOptionsTable"));
    blockDetailsOptionsTable_->setColumnCount(2);
    blockDetailsOptionsTable_->setHorizontalHeaderLabels({tr("Option"), tr("Value")});
    blockDetailsOptionsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    blockDetailsOptionsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    blockDetailsOptionsTable_->verticalHeader()->setVisible(false);
    blockDetailsOptionsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    blockDetailsOptionsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    blockDetailsOptionsTable_->setAlternatingRowColors(true);
    blockDetailsOptionsTable_->setMinimumHeight(140);
    blockDetailsOptionsTable_->setItemDelegate(
        new BlockDetailsOptionsTableDelegate(
            [this](const QModelIndex &index) {
                if (index.column() == 0) {
                    return commandOptionTokens_.value(blockDetailsSelectedKind_);
                }
                if (index.column() == 1) {
                    if (blockDetailsOptionsTable_ == nullptr) {
                        return QStringList{};
                    }
                    const int row = index.row();
                    const QTableWidgetItem *optionItem = blockDetailsOptionsTable_->item(row, 0);
                    const QString optionToken = optionItem != nullptr
                        ? optionItem->text().trimmed().toLower()
                        : QString();
                    if (optionToken.isEmpty()) {
                        return QStringList{};
                    }
                    return commandOptionValueTokens_.value(
                        commandOptionValueKey(blockDetailsSelectedKind_, optionToken));
                }
                return QStringList{};
            },
            blockDetailsOptionsTable_));
    blockDetailsEditLayout->addWidget(blockDetailsOptionsTable_, 1);

    blockDetailsOptionArgsLabel_ = new QLabel(tr("Selected Option Parameters"), blockDetailsEditPanel_);
    blockDetailsOptionArgsLabel_->setObjectName(QStringLiteral("blockDetailsOptionArgsLabel"));
    blockDetailsEditLayout->addWidget(blockDetailsOptionArgsLabel_);

    blockDetailsOptionArgsPanel_ = new QWidget(blockDetailsEditPanel_);
    blockDetailsOptionArgsPanel_->setObjectName(QStringLiteral("blockDetailsOptionArgsPanel"));
    blockDetailsOptionArgsFormLayout_ = new QFormLayout(blockDetailsOptionArgsPanel_);
    blockDetailsOptionArgsFormLayout_->setContentsMargins(0, 0, 0, 0);
    blockDetailsOptionArgsFormLayout_->setSpacing(kPanelSpacing);
    blockDetailsOptionArgsLabel_->setVisible(false);
    blockDetailsOptionArgsPanel_->setVisible(false);
    blockDetailsEditLayout->addWidget(blockDetailsOptionArgsPanel_);

    auto *blockDetailsButtonsRow = new QHBoxLayout;
    blockDetailsButtonsRow->setContentsMargins(0, 0, 0, 0);
    blockDetailsButtonsRow->setSpacing(kPanelSpacing);
    blockDetailsLegacyConfigureButton_ = new QPushButton(tr("Legacy Configure..."), blockDetailsEditPanel_);
    blockDetailsLegacyConfigureButton_->setObjectName(QStringLiteral("blockDetailsLegacyButton"));
    blockDetailsLegacyConfigureButton_->setAutoDefault(false);
    blockDetailsApplyButton_ = new QPushButton(tr("Apply"), blockDetailsEditPanel_);
    blockDetailsApplyButton_->setObjectName(QStringLiteral("blockDetailsApplyButton"));
    blockDetailsApplyButton_->setAutoDefault(false);
    blockDetailsButtonsRow->addStretch(1);
    blockDetailsButtonsRow->addWidget(blockDetailsLegacyConfigureButton_);
    blockDetailsButtonsRow->addWidget(blockDetailsApplyButton_);
    blockDetailsEditLayout->addLayout(blockDetailsButtonsRow);
    blockDetailsEditLayout->addSpacing(10);

    blockDetailsLayout->addWidget(blockDetailsEditPanel_);

    blockDetailsHelpPanel_ = new QFrame(blockDetailsPanel_);
    auto *blockDetailsHelpFrame = qobject_cast<QFrame *>(blockDetailsHelpPanel_);
    if (blockDetailsHelpFrame != nullptr) {
        blockDetailsHelpFrame->setFrameShape(QFrame::NoFrame);
    }
    blockDetailsHelpPanel_->setObjectName(QStringLiteral("blocksContextHelpPanel"));
    syncPanelSurfaceToBaseTone(blockDetailsHelpPanel_);
    auto *blockDetailsHelpPanelLayout = new QVBoxLayout(blockDetailsHelpPanel_);
    blockDetailsHelpPanelLayout->setContentsMargins(0, 0, 0, 0);
    blockDetailsHelpPanelLayout->setSpacing(kPanelSpacing);

    auto *blockDetailsHelpHeaderRow = new QHBoxLayout;
    blockDetailsHelpHeaderRow->setContentsMargins(0, 0, 0, 0);
    auto *blockDetailsHelpLabel = new QLabel(tr("Contextual Help"), blockDetailsHelpPanel_);
    QFont blockDetailsHelpLabelFont = blockDetailsHelpLabel->font();
    blockDetailsHelpLabelFont.setBold(true);
    blockDetailsHelpLabel->setFont(blockDetailsHelpLabelFont);
    blockDetailsHelpHeaderRow->addWidget(blockDetailsHelpLabel);
    blockDetailsHelpHeaderRow->addStretch(1);

    blockDetailsHelpBrowser_ = new QTextBrowser(blockDetailsHelpPanel_);
    blockDetailsHelpBrowser_->setObjectName(QStringLiteral("blockDetailsHelpBrowser"));
    blockDetailsHelpBrowser_->setFrameShape(QFrame::NoFrame);
    blockDetailsHelpBrowser_->setOpenLinks(false);
    blockDetailsHelpBrowser_->setOpenExternalLinks(false);
    blockDetailsHelpBrowser_->setMinimumHeight(140);
    syncTextBrowserSurfaceToParent(blockDetailsHelpBrowser_);

    blockDetailsHelpPanelLayout->addLayout(blockDetailsHelpHeaderRow);
    blockDetailsHelpPanelLayout->addWidget(blockDetailsHelpBrowser_, 1);
    blockDetailsLayout->addWidget(blockDetailsHelpPanel_, 1);

    blocksSplitter->addWidget(toolboxColumn);
    blocksSplitter->addWidget(blockCanvasView_);
    blocksSplitter->addWidget(blockDetailsPanel_);
    blocksSplitter->setStretchFactor(0, 0);
    blocksSplitter->setStretchFactor(1, 1);
    blocksSplitter->setStretchFactor(2, 0);
    blocksSplitter->setSizes({220, 980, 380});
    blocksLayout->addWidget(blocksSplitter, 1);

    editorModeStack_ = new QStackedWidget(this);
    editorModeStack_->addWidget(rawEditorPanel_);
    editorModeStack_->addWidget(blocksPanel_);

    auto *toolbarSeparator = new QFrame(this);
    toolbarSeparator->setFrameShape(QFrame::NoFrame);
    toolbarSeparator->setFixedHeight(1);
    toolbarSeparator->setStyleSheet(QStringLiteral("background-color: palette(mid);"));

    layout->addWidget(searchBar_);
    layout->addWidget(modeRow_);
    layout->addWidget(toolbarSeparator);
    layout->addWidget(editorModeStack_, 1);
    layout->addWidget(statusRow_);

    connect(editor_, &QPlainTextEdit::textChanged, this, &TextEditorTab::handleTextChanged);
    connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, &TextEditorTab::handleCursorPositionChanged);
    connect(rawModeButton_, &QPushButton::clicked, this, &TextEditorTab::handleRawModeRequested);
    connect(blocksModeButton_, &QPushButton::clicked, this, &TextEditorTab::handleBlocksModeRequested);
    connect(blockCanvasScene_, &QGraphicsScene::selectionChanged, this, [this]() {
        if (tearingDown_) {
            return;
        }
        refreshBlockDetailsSelectionFromScene();
        if (blockToolboxScopeCombo_ != nullptr
            && blockToolboxScopeCombo_->currentData().toString() == QStringLiteral("__auto__")) {
            populateBlockToolbox();
        }
    });
    connect(blockToolboxList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current, QListWidgetItem *) {
        if (tearingDown_) {
            return;
        }
        if (current == nullptr) {
            return;
        }
        const QString commandToken = normalizeDirective(current->data(Qt::UserRole).toString());
        if (commandToken.isEmpty()) {
            return;
        }
        showBlockDetailsForToolboxCommand(commandToken);
    });
    connect(blockDetailsOptionsTable_, &QTableWidget::currentCellChanged, this, [this](int, int, int, int) {
        if (blockDetailsRemoveOptionButton_ != nullptr && blockDetailsOptionsTable_ != nullptr) {
            const bool canRemove = blockDetailsOptionsTable_->isVisible()
                && blockDetailsOptionsTable_->isEnabled()
                && blockDetailsOptionsTable_->currentRow() >= 0
                && blockDetailsOptionsTable_->rowCount() > 0;
            blockDetailsRemoveOptionButton_->setEnabled(canRemove);
        }
        refreshBlockDetailsOptionArgumentEditors();
        updateBlockDetailsHelpForCurrentFocus();
        refreshBlockDetailsApplyState();
    });
    connect(blockDetailsOptionsTable_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *) {
        if (!blockDetailsPopulating_ && !blockDetailsOptionArgsSyncing_) {
            refreshBlockDetailsOptionArgumentEditors();
            updateBlockDetailsHelpForCurrentFocus();
            refreshBlockDetailsApplyState();
        }
    });
    connect(blockDetailsIdEdit_, &QLineEdit::selectionChanged, this, [this]() {
        updateBlockDetailsHelpForCurrentFocus();
    });
    connect(blockDetailsIdEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        updateBlockDetailsHelpForCurrentFocus();
        refreshBlockDetailsApplyState();
    });
    connect(blockDetailsAdditionalPositionalEdit_, &QLineEdit::selectionChanged, this, [this]() {
        updateBlockDetailsHelpForCurrentFocus();
    });
    connect(blockDetailsAdditionalPositionalEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        updateBlockDetailsHelpForCurrentFocus();
        refreshBlockDetailsApplyState();
    });
    if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
        readingsTagEditor->onTokensChanged = [this, readingsTagEditor]() {
            if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
                blockDetailsAdditionalPositionalEdit_->setText(readingsTagEditor->tokens().join(QLatin1Char(' ')));
            }
            updateBlockDetailsHelpForCurrentFocus();
            refreshBlockDetailsApplyState();
        };
        readingsTagEditor->onFocusContextChanged = [this]() {
            updateBlockDetailsHelpForCurrentFocus();
        };
    }
    connect(blockDetailsCommentEdit_, &QLineEdit::selectionChanged, this, [this]() {
        updateBlockDetailsHelpForCurrentFocus();
    });
    connect(blockDetailsCommentEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        updateBlockDetailsHelpForCurrentFocus();
        refreshBlockDetailsApplyState();
    });
    connect(blockDetailsAddOptionButton_, &QPushButton::clicked, this, [this]() {
        if (blockDetailsOptionsTable_ == nullptr
            || blockDetailsPopulating_
            || blockDetailsMode_ != BlockDetailsMode::StructuredOptions) {
            return;
        }
        const int row = blockDetailsOptionsTable_->rowCount();
        blockDetailsOptionsTable_->insertRow(row);
        blockDetailsOptionsTable_->setItem(row, 0, new QTableWidgetItem(QString()));
        blockDetailsOptionsTable_->setItem(row, 1, new QTableWidgetItem(QString()));
        blockDetailsOptionsTable_->setCurrentCell(row, 0);
        blockDetailsOptionsTable_->editItem(blockDetailsOptionsTable_->item(row, 0));
        if (blockDetailsRemoveOptionButton_ != nullptr) {
            blockDetailsRemoveOptionButton_->setEnabled(true);
        }
        refreshBlockDetailsOptionArgumentEditors();
        updateBlockDetailsHelpForCurrentFocus();
        refreshBlockDetailsApplyState();
    });
    connect(blockDetailsRemoveOptionButton_, &QPushButton::clicked, this, [this]() {
        if (blockDetailsOptionsTable_ == nullptr
            || blockDetailsPopulating_
            || blockDetailsMode_ != BlockDetailsMode::StructuredOptions
            || blockDetailsOptionsTable_->rowCount() == 0) {
            return;
        }
        const int row = blockDetailsOptionsTable_->currentRow() >= 0
            ? blockDetailsOptionsTable_->currentRow()
            : blockDetailsOptionsTable_->rowCount() - 1;
        blockDetailsOptionsTable_->removeRow(row);
        if (blockDetailsRemoveOptionButton_ != nullptr) {
            blockDetailsRemoveOptionButton_->setEnabled(blockDetailsOptionsTable_->rowCount() > 0);
        }
        refreshBlockDetailsOptionArgumentEditors();
        updateBlockDetailsHelpForCurrentFocus();
        refreshBlockDetailsApplyState();
    });
    connect(blockDetailsApplyButton_, &QPushButton::clicked, this, &TextEditorTab::applyBlockDetailsChanges);
    connect(blockDetailsLegacyConfigureButton_, &QPushButton::clicked, this, [this]() {
        if (blockDetailsSelectedLineNumber_ <= 0 || blockDetailsSelectedKind_.isEmpty()) {
            return;
        }
        handleBlockConfigureRequest(blockDetailsSelectedKind_, blockDetailsSelectedLineNumber_);
    });

    refreshBlocksModeAvailability();
    refreshEditorModeUi();
    rebuildBlocksCanvasFromText();
    clearBlockDetailsPane();
    refreshStatus();
    refreshCurrentLineHighlight();
    updateContextHelp();

    if (QStyleHints *styleHints = QGuiApplication::styleHints()) {
        connect(styleHints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            handleApplicationAppearanceChanged();
        });
    }
    if (qApp != nullptr) {
        qApp->installEventFilter(this);
    }
    handleApplicationAppearanceChanged();
}

TextEditorTab::~TextEditorTab()
{
    tearingDown_ = true;
    if (blockCanvasScene_ != nullptr) {
        disconnect(blockCanvasScene_, nullptr, this, nullptr);
    }
}

void TextEditorTab::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        handleApplicationAppearanceChanged();
        break;
    default:
        break;
    }
}

void TextEditorTab::handleApplicationAppearanceChanged()
{
    if (blockCanvasView_ != nullptr) {
        blockCanvasView_->setBackgroundBrush(blockCanvasView_->palette().color(QPalette::Base));
        if (QWidget *viewport = blockCanvasView_->viewport(); viewport != nullptr) {
            viewport->update();
        }
    }

    QPalette textSurfacePalette = editor_ != nullptr ? editor_->palette() : palette();
    const QColor sourceSurface = sourceSurfaceColor();
    textSurfacePalette.setColor(QPalette::Window, sourceSurface);
    textSurfacePalette.setColor(QPalette::Base, sourceSurface);
    textSurfacePalette.setColor(QPalette::AlternateBase, sourceSurface);
    syncPanelSurfaceToPalette(helpPanel_, textSurfacePalette);
    syncPanelSurfaceToPalette(blockDetailsPanel_, textSurfacePalette);
    syncPanelSurfaceToPalette(blockDetailsEditPanel_, textSurfacePalette);
    syncPanelSurfaceToPalette(blockDetailsHelpPanel_, textSurfacePalette);
    syncTextBrowserSurfaceToPalette(helpBrowser_, textSurfacePalette);
    syncTextBrowserSurfaceToPalette(blockDetailsHelpBrowser_, textSurfacePalette);
    updateContextHelp();
    updateBlockDetailsHelpForCurrentFocus();

    if (blocksModeActive_ && blockCanvasScene_ != nullptr) {
        rebuildBlocksCanvasFromText();
    }
}

bool TextEditorTab::loadFile(const QString &filePath, QString *errorMessage)
{
    QString contents;
    QString loadedEncoding = QStringLiteral("UTF-8");
    QString loadedEncodingLabel;
    if (!DocumentFile::readTextFile(filePath, &contents, &loadedEncoding, &loadedEncodingLabel, errorMessage)) {
        return false;
    }

    filePath_ = filePath;
    fileEncodingName_ = loadedEncoding.trimmed().isEmpty() ? QStringLiteral("UTF-8") : loadedEncoding.trimmed();
    fileEncodingLabel_ = loadedEncodingLabel.isEmpty() ? QStringLiteral("UTF-8") : loadedEncodingLabel;
    if (fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        encodingStatusNote_.clear();
    } else {
        encodingStatusNote_ = tr("Opened as %1. Save keeps this encoding unless you convert to UTF-8.")
                                  .arg(fileEncodingLabel_);
    }
    loading_ = true;
    editor_->setPlainText(contents);
    loading_ = false;
    const QTextCursor cursor = editor_->textCursor();
    currentLineNumber_ = cursor.blockNumber() + 1;
    currentColumnNumber_ = cursor.positionInBlock() + 1;
    highlightedLineNumber_ = currentLineNumber_;
    cleanTextSnapshot_ = editor_->toPlainText();
    cleanEncodingNameSnapshot_ = fileEncodingName_;
    editor_->document()->setModified(false);
    dirty_ = false;
    refreshBlocksModeAvailability();
    if (blocksModeActive_ && !isBlocksModeSupportedForCurrentFile()) {
        setBlocksModeActive(false);
    }
    blockDetailsSelectedLineNumber_ = 0;
    blockDetailsSelectedKind_.clear();
    rebuildBlocksCanvasFromText();
    clearBlockDetailsPane();
    populateBlockToolbox();
    refreshEditorModeUi();
    refreshTitle();
    refreshCurrentLineHighlight();
    emit dirtyStateChanged(false);
    updateContextHelp();
    return true;
}

void TextEditorTab::setProjectRootPath(const QString &projectRootPath)
{
    projectRootPath_ = projectRootPath;
    refreshStatus();
}

bool TextEditorTab::save(QString *errorMessage)
{
    if (filePath_.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("This document does not have a file path yet.");
        }
        return false;
    }

    if (!DocumentFile::writeTextFile(filePath_, editor_->toPlainText(), fileEncodingName_, errorMessage)) {
        return false;
    }

    cleanTextSnapshot_ = editor_->toPlainText();
    cleanEncodingNameSnapshot_ = fileEncodingName_;
    editor_->document()->setModified(false);
    dirty_ = false;
    if (fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        encodingStatusNote_.clear();
    } else {
        encodingStatusNote_ = tr("Saved using %1 encoding.").arg(fileEncodingLabel_);
    }
    refreshTitle();
    emit dirtyStateChanged(false);
    return true;
}

void TextEditorTab::goToLine(int lineNumber)
{
    if (blocksModeActive_) {
        setBlocksModeActive(false);
    }

    if (lineNumber <= 0) {
        return;
    }

    const QTextBlock block = editor_->document()->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid()) {
        return;
    }

    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::StartOfBlock);
    editor_->setTextCursor(cursor);
    highlightedLineNumber_ = lineNumber;
    editor_->centerCursor();
    editor_->ensureCursorVisible();
    refreshCurrentLineHighlight();
    updateContextHelp();

    if (searchBar_->isVisible()) {
        findEdit_->clearFocus();
    }
}

void TextEditorTab::goToLineColumn(int lineNumber, int columnNumber)
{
    if (blocksModeActive_) {
        setBlocksModeActive(false);
    }

    if (lineNumber <= 0) {
        return;
    }

    const QTextBlock block = editor_->document()->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid()) {
        return;
    }

    const int clampedColumn = qMax(1, columnNumber);
    QTextCursor cursor(block);
    const int offsetInBlock = qMin(clampedColumn - 1, qMax(0, block.length() - 1));
    cursor.setPosition(block.position() + offsetInBlock);
    editor_->setTextCursor(cursor);
    highlightedLineNumber_ = lineNumber;
    editor_->centerCursor();
    editor_->ensureCursorVisible();
    refreshCurrentLineHighlight();
    updateContextHelp();

    if (searchBar_->isVisible()) {
        findEdit_->clearFocus();
    }
}

void TextEditorTab::showFindBar(bool replaceMode)
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->showFindBar(replaceMode);
    }
}

void TextEditorTab::hideFindBar()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->hideFindBar();
    }
}

bool TextEditorTab::findNext()
{
    if (rawEditorFindController_ == nullptr) {
        return false;
    }
    return rawEditorFindController_->findNext();
}

bool TextEditorTab::findPrevious()
{
    if (rawEditorFindController_ == nullptr) {
        return false;
    }
    return rawEditorFindController_->findPrevious();
}

bool TextEditorTab::replaceCurrent()
{
    if (rawEditorFindController_ == nullptr) {
        return false;
    }
    return rawEditorFindController_->replaceCurrent();
}

int TextEditorTab::replaceAll()
{
    if (rawEditorFindController_ == nullptr) {
        return 0;
    }
    return rawEditorFindController_->replaceAll();
}

bool TextEditorTab::rewriteStructureEntryName(int lineNumber, const QString &category, const QString &newName, QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteStructureEntryName(&contents, lineNumber, category, newName, errorMessage)) {
        return false;
    }

    const QTextCursor previousCursor = editor_->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    loading_ = true;
    editor_->setPlainText(contents);

    const int targetLine = qBound(0, previousLine, qMax(0, editor_->document()->blockCount() - 1));
    const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        editor_->setTextCursor(restoredCursor);
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    updateContextHelp();
    return true;
}

bool TextEditorTab::insertScrapBlock(const QString &preferredName,
                                     int *insertedLineNumber,
                                     QString *errorMessage,
                                     const QString &options)
{
    QString contents = editor_->toPlainText();
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendScrapBlock(&contents, preferredName, &resolvedLineNumber, errorMessage, options)) {
        return false;
    }

    loading_ = true;
    editor_->setPlainText(contents);

    if (resolvedLineNumber > 0) {
        const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(resolvedLineNumber - 1);
        if (targetBlock.isValid()) {
            QTextCursor cursor(targetBlock);
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            editor_->setTextCursor(cursor);
            editor_->centerCursor();
        }
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();

    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }

    return true;
}

bool TextEditorTab::insertDraftGeometry(const QString &kind,
                                        const QVector<QPointF> &vertices,
                                        int *insertedLineNumber,
                                        QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftGeometry(&contents, kind, vertices, &resolvedLineNumber, errorMessage)) {
        return false;
    }

    loading_ = true;
    editor_->setPlainText(contents);

    if (resolvedLineNumber > 0) {
        const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(resolvedLineNumber - 1);
        if (targetBlock.isValid()) {
            QTextCursor cursor(targetBlock);
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            editor_->setTextCursor(cursor);
            editor_->centerCursor();
        }
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();

    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }

    return true;
}

bool TextEditorTab::insertDraftLineGeometry(const QStringList &coordinateRows,
                                            int *insertedLineNumber,
                                            QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftLineGeometry(&contents, coordinateRows, &resolvedLineNumber, errorMessage)) {
        return false;
    }

    loading_ = true;
    editor_->setPlainText(contents);

    if (resolvedLineNumber > 0) {
        const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(resolvedLineNumber - 1);
        if (targetBlock.isValid()) {
            QTextCursor cursor(targetBlock);
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            editor_->setTextCursor(cursor);
            editor_->centerCursor();
        }
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();

    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }

    return true;
}

bool TextEditorTab::insertDraftAreaGeometry(const QStringList &coordinateRows,
                                            int *insertedLineNumber,
                                            QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftAreaGeometry(&contents, coordinateRows, &resolvedLineNumber, errorMessage)) {
        return false;
    }

    loading_ = true;
    editor_->setPlainText(contents);

    if (resolvedLineNumber > 0) {
        const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(resolvedLineNumber - 1);
        if (targetBlock.isValid()) {
            QTextCursor cursor(targetBlock);
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            editor_->setTextCursor(cursor);
            editor_->centerCursor();
        }
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();

    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }

    return true;
}

bool TextEditorTab::rewritePointCoordinates(int lineNumber,
                                            const QPointF &point,
                                            QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    if (!TherionDocumentEditor::rewritePointCoordinates(&contents, lineNumber, point, errorMessage)) {
        return false;
    }

    const QTextCursor previousCursor = editor_->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    loading_ = true;
    editor_->setPlainText(contents);

    const int targetLine = qBound(0, previousLine, qMax(0, editor_->document()->blockCount() - 1));
    const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        editor_->setTextCursor(restoredCursor);
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();
    return true;
}

bool TextEditorTab::rewriteLineAreaVertex(int lineNumber,
                                          const QString &kind,
                                          int vertexIndex,
                                          const QPointF &point,
                                          QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteLineAreaVertex(&contents, lineNumber, kind, vertexIndex, point, errorMessage)) {
        return false;
    }

    const QTextCursor previousCursor = editor_->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    loading_ = true;
    editor_->setPlainText(contents);

    const int targetLine = qBound(0, previousLine, qMax(0, editor_->document()->blockCount() - 1));
    const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        editor_->setTextCursor(restoredCursor);
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();
    return true;
}

bool TextEditorTab::rewriteLineOptionToggle(int lineNumber,
                                            const QString &optionName,
                                            bool enabled,
                                            QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteLineOptionToggle(&contents, lineNumber, optionName, enabled, errorMessage)) {
        return false;
    }

    const QTextCursor previousCursor = editor_->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    loading_ = true;
    editor_->setPlainText(contents);

    const int targetLine = qBound(0, previousLine, qMax(0, editor_->document()->blockCount() - 1));
    const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        editor_->setTextCursor(restoredCursor);
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();
    return true;
}

bool TextEditorTab::rewritePointOrientation(int lineNumber,
                                            bool enabled,
                                            qreal orientationDegrees,
                                            QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    if (!TherionDocumentEditor::rewritePointOrientation(&contents,
                                                        lineNumber,
                                                        enabled,
                                                        orientationDegrees,
                                                        errorMessage)) {
        return false;
    }

    const QTextCursor previousCursor = editor_->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    loading_ = true;
    editor_->setPlainText(contents);

    const int targetLine = qBound(0, previousLine, qMax(0, editor_->document()->blockCount() - 1));
    const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        editor_->setTextCursor(restoredCursor);
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();
    return true;
}

bool TextEditorTab::rewriteLinePointOrientation(int lineNumber,
                                                int sourceVertexIndex,
                                                bool enabled,
                                                qreal orientationDegrees,
                                                QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteLinePointOrientation(&contents,
                                                            lineNumber,
                                                            sourceVertexIndex,
                                                            enabled,
                                                            orientationDegrees,
                                                            errorMessage)) {
        return false;
    }

    const QTextCursor previousCursor = editor_->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    loading_ = true;
    editor_->setPlainText(contents);

    const int targetLine = qBound(0, previousLine, qMax(0, editor_->document()->blockCount() - 1));
    const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        editor_->setTextCursor(restoredCursor);
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();
    return true;
}

bool TextEditorTab::rewriteLinePointLeftSize(int lineNumber,
                                             int sourceVertexIndex,
                                             bool enabled,
                                             qreal sizeValue,
                                             QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteLinePointLeftSize(&contents,
                                                         lineNumber,
                                                         sourceVertexIndex,
                                                         enabled,
                                                         sizeValue,
                                                         errorMessage)) {
        return false;
    }

    const QTextCursor previousCursor = editor_->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    loading_ = true;
    editor_->setPlainText(contents);

    const int targetLine = qBound(0, previousLine, qMax(0, editor_->document()->blockCount() - 1));
    const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        editor_->setTextCursor(restoredCursor);
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();
    return true;
}

bool TextEditorTab::rewriteLineCoordinateRows(int lineNumber,
                                              const QStringList &coordinateRows,
                                              QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteLineCoordinateRows(&contents, lineNumber, coordinateRows, errorMessage)) {
        return false;
    }

    const QTextCursor previousCursor = editor_->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    loading_ = true;
    editor_->setPlainText(contents);

    const int targetLine = qBound(0, previousLine, qMax(0, editor_->document()->blockCount() - 1));
    const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        editor_->setTextCursor(restoredCursor);
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();
    return true;
}

bool TextEditorTab::configureCommandAtLine(const QString &kind, int lineNumber)
{
    if (lineNumber <= 0 || editor_ == nullptr) {
        return false;
    }

    handleBlockConfigureRequest(kind, lineNumber);
    return true;
}

bool TextEditorTab::deleteCommandAtLine(int lineNumber)
{
    return handleBlockDeleteRequest(lineNumber);
}

void TextEditorTab::replaceTextForCommand(const QString &contents)
{
    const QTextCursor previousCursor = editor_->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    loading_ = true;
    editor_->setPlainText(contents);

    const int targetLine = qBound(0, previousLine, qMax(0, editor_->document()->blockCount() - 1));
    const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        editor_->setTextCursor(restoredCursor);
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    rebuildBlocksCanvasFromText();
    emit documentTextChanged();
    updateContextHelp();
}

QString TextEditorTab::filePath() const
{
    return filePath_;
}

QString TextEditorTab::displayName() const
{
    if (filePath_.isEmpty()) {
        return tr("Untitled");
    }

    QString name = QFileInfo(filePath_).fileName();
    if (name.isEmpty()) {
        name = tr("Untitled");
    }

    if (dirty_) {
        name.prepend(QStringLiteral("*"));
    }

    return name;
}

bool TextEditorTab::isDirty() const
{
    return dirty_;
}

int TextEditorTab::currentLineNumber() const
{
    return editor_->textCursor().blockNumber() + 1;
}

int TextEditorTab::currentColumnNumber() const
{
    return editor_->textCursor().positionInBlock() + 1;
}

QString TextEditorTab::text() const
{
    return editor_->toPlainText();
}

QColor TextEditorTab::sourceSurfaceColor() const
{
    if (blocksModeActive_ && blockCanvasView_ != nullptr) {
        const QColor blocksSurface = blockCanvasView_->backgroundBrush().color();
        if (blocksSurface.isValid()) {
            return blocksSurface;
        }
        const QColor blocksBase = blockCanvasView_->palette().color(QPalette::Base);
        if (blocksBase.isValid()) {
            return blocksBase;
        }
    }

    if (editor_ != nullptr) {
        const QColor editorBase = editor_->palette().color(QPalette::Base);
        if (editorBase.isValid()) {
            return editorBase;
        }
        const QColor editorWindow = editor_->palette().color(QPalette::Window);
        if (editorWindow.isValid()) {
            return editorWindow;
        }
    }

    const QColor widgetBase = palette().color(QPalette::Base);
    if (widgetBase.isValid()) {
        return widgetBase;
    }
    return palette().color(QPalette::Window);
}

QString TextEditorTab::statusPathText() const
{
    return displayPath();
}

QString TextEditorTab::statusEncodingText() const
{
    return fileEncodingLabel_;
}

bool TextEditorTab::canUndo() const
{
    return editor_ != nullptr && editor_->document() != nullptr && editor_->document()->isUndoAvailable();
}

bool TextEditorTab::canRedo() const
{
    return editor_ != nullptr && editor_->document() != nullptr && editor_->document()->isRedoAvailable();
}

void TextEditorTab::triggerUndo()
{
    if (editor_ != nullptr) {
        editor_->undo();
    }
}

void TextEditorTab::triggerRedo()
{
    if (editor_ != nullptr) {
        editor_->redo();
    }
}

void TextEditorTab::setInlineStatusVisible(bool visible)
{
    inlineStatusRequestedVisible_ = visible;
    refreshStatus();
}

void TextEditorTab::setModeSelectorVisible(bool visible)
{
    modeSelectorRequestedVisible_ = visible;
    if (modeRow_ != nullptr) {
        modeRow_->setVisible(modeSelectorRequestedVisible_);
        modeRow_->setMaximumHeight(modeSelectorRequestedVisible_ ? QWIDGETSIZE_MAX : 0);
        if (!modeSelectorRequestedVisible_) {
            modeRow_->setMinimumHeight(0);
        } else {
            modeRow_->setMinimumHeight(modeRow_->sizeHint().height());
        }
    }
    if (QLayout *rootLayout = layout(); rootLayout != nullptr) {
        rootLayout->invalidate();
        rootLayout->activate();
    }
}

TextEditorTab::EditorMode TextEditorTab::editorMode() const
{
    return blocksModeActive_ ? EditorMode::Blocks : EditorMode::Raw;
}

bool TextEditorTab::isBlocksModeAvailable() const
{
    return isBlocksModeSupportedForCurrentFile();
}

void TextEditorTab::setEditorMode(EditorMode mode)
{
    setBlocksModeActive(mode == EditorMode::Blocks);
}

bool TextEditorTab::isBlocksModeSupportedForCurrentFile() const
{
    if (filePath_.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(filePath_);
    const QString suffix = fileInfo.suffix().trimmed().toLower();
    const QString fileName = fileInfo.fileName().trimmed().toLower();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("thconfig")
        || fileName == QStringLiteral("thconfig");
}

void TextEditorTab::refreshBlocksModeAvailability()
{
    const bool supported = isBlocksModeSupportedForCurrentFile();
    if (blocksModeButton_ != nullptr) {
        blocksModeButton_->setEnabled(supported);
    }
    if (!supported) {
        blocksModeActive_ = false;
    }
}

void TextEditorTab::setBlocksModeActive(bool active)
{
    const bool targetActive = active && isBlocksModeSupportedForCurrentFile();
    if (blocksModeActive_ == targetActive) {
        refreshEditorModeUi();
        return;
    }

    blocksModeActive_ = targetActive;
    if (blocksModeActive_) {
        hideFindBar();
        if (!ensureEncodingRootDirectiveForBlocks()) {
            rebuildBlocksCanvasFromText();
        }
        populateBlockToolbox();
    } else if (editor_ != nullptr) {
        editor_->setFocus();
    }
    refreshEditorModeUi();
    emit editorModeChanged(editorMode());
}

bool TextEditorTab::ensureEncodingRootDirectiveForBlocks()
{
    if (editor_ == nullptr || enforcingEncodingRootDirective_) {
        return false;
    }

    QString contents = editor_->toPlainText();
    QString lineEnding = QStringLiteral("\n");
    if (contents.contains(QStringLiteral("\r\n"))) {
        lineEnding = QStringLiteral("\r\n");
    }

    QStringList lines = contents.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    if (lines.size() == 1 && lines.first().isEmpty()) {
        lines.clear();
    }

    QString encodingName = fileEncodingName_.trimmed();
    if (encodingName.isEmpty()) {
        encodingName = QStringLiteral("utf-8");
    }
    const QString desiredEncodingLine =
        QStringLiteral("encoding %1").arg(encodingName.toLower());

    QStringList normalizedLines;
    normalizedLines.reserve(lines.size() + 1);
    normalizedLines.append(desiredEncodingLine);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        if (isEncodingDirective(parsedLine.directive)) {
            continue;
        }
        normalizedLines.append(lines.at(lineIndex));
    }

    const QString normalizedContents = normalizedLines.join(lineEnding);
    if (normalizedContents == lines.join(lineEnding)) {
        return false;
    }

    enforcingEncodingRootDirective_ = true;
    replaceTextForCommand(normalizedContents);
    enforcingEncodingRootDirective_ = false;
    return true;
}

void TextEditorTab::refreshEditorModeUi()
{
    if (rawModeButton_ == nullptr || blocksModeButton_ == nullptr || editorModeStack_ == nullptr) {
        return;
    }

    rawModeButton_->setChecked(!blocksModeActive_);
    blocksModeButton_->setChecked(blocksModeActive_);
    if (blocksModeActive_) {
        editorModeStack_->setCurrentWidget(blocksPanel_);
    } else {
        if (rawEditorPanel_ != nullptr) {
            editorModeStack_->setCurrentWidget(rawEditorPanel_);
        }
    }
}

void TextEditorTab::populateBlockToolbox()
{
    if (tearingDown_ || blockToolboxList_ == nullptr) {
        return;
    }

    blockToolboxList_->clear();
    const QString filterText = blockToolboxFilterEdit_ != nullptr
        ? blockToolboxFilterEdit_->text().trimmed().toLower()
        : QString();

    auto labelForCommand = [](const QString &commandToken) {
        QStringList parts = commandToken.split(QLatin1Char('-'), Qt::SkipEmptyParts);
        for (QString &part : parts) {
            if (!part.isEmpty()) {
                part[0] = part.at(0).toUpper();
            }
        }
        return parts.join(QLatin1Char(' '));
    };

    QString selectedScope = QStringLiteral("all");
    if (blockToolboxScopeCombo_ != nullptr) {
        selectedScope = blockToolboxScopeCombo_->currentData().toString().trimmed().toLower();
    }

    QString effectiveScope = selectedScope;
    if (effectiveScope == QStringLiteral("__auto__")) {
        effectiveScope = selectedBlockInsertionContextToken();
    }
    if (effectiveScope != QStringLiteral("all")) {
        effectiveScope = normalizeCompletionContext(effectiveScope);
        if (effectiveScope.isEmpty()) {
            effectiveScope = QStringLiteral("none");
        }
    }

    QStringList contextsToShow;
    if (effectiveScope == QStringLiteral("all")) {
        appendUnique(contextsToShow, QStringLiteral("none"));
        QStringList remainingContexts;
        for (auto it = contextCommandTokens_.cbegin(); it != contextCommandTokens_.cend(); ++it) {
            const QString normalizedContext = normalizeCompletionContext(it.key());
            if (normalizedContext.isEmpty()
                || normalizedContext == QStringLiteral("all")
                || normalizedContext == QStringLiteral("none")) {
                continue;
            }
            appendUnique(remainingContexts, normalizedContext);
        }
        std::sort(remainingContexts.begin(), remainingContexts.end(), [](const QString &left, const QString &right) {
            return QString::compare(contextDisplayLabel(left), contextDisplayLabel(right), Qt::CaseInsensitive) < 0;
        });
        appendUniqueList(contextsToShow, remainingContexts);
    } else {
        appendUnique(contextsToShow, effectiveScope);
    }

    contextsToShow.erase(std::remove_if(contextsToShow.begin(),
                                        contextsToShow.end(),
                                        [this](const QString &contextToken) {
                                            QStringList candidates = contextCommandTokens_.value(contextToken);
                                            if (contextToken == QStringLiteral("none")) {
                                                appendUniqueList(candidates, contextCommandTokens_.value(QStringLiteral("none")));
                                            }
                                            appendUniqueList(candidates, contextCommandTokens_.value(QStringLiteral("all")));
                                            return candidates.isEmpty();
                                        }),
                         contextsToShow.end());

    int insertedRows = 0;
    for (const QString &contextToken : contextsToShow) {
        QStringList sectionCommands = contextCommandTokens_.value(contextToken);
        if (contextToken == QStringLiteral("none")) {
            appendUniqueList(sectionCommands, contextCommandTokens_.value(QStringLiteral("none")));
        }
        appendUniqueList(sectionCommands, contextCommandTokens_.value(QStringLiteral("all")));
        appendUnique(sectionCommands, QStringLiteral("comment"));

        QStringList visibleCommands;
        for (const QString &command : sectionCommands) {
            const QString normalizedCommand = normalizeDirective(command);
            if (normalizedCommand.isEmpty()) {
                continue;
            }
            if (normalizedCommand == QStringLiteral("all")
                || normalizedCommand == QStringLiteral("none")
                || normalizedCommand == QStringLiteral("encoding")
                || normalizedCommand.startsWith(QStringLiteral("end"))) {
                continue;
            }
            const QString parentKind = contextToken == QStringLiteral("none") ? QString() : contextToken;
            if (!isCompatibleChildKindForBlocks(parentKind, normalizedCommand)) {
                continue;
            }
            const QString closingDirective = completionClosingDirectiveForOpening(normalizedCommand);
            const bool unsupportedContainer = !closingDirective.isEmpty()
                && !isContainerBlockDirective(normalizedCommand);
            if (unsupportedContainer) {
                continue;
            }

            const QString label = labelForCommand(normalizedCommand);
            const QString searchable = normalizedCommand + QStringLiteral(" ") + label.toLower();
            if (!filterText.isEmpty() && !searchable.contains(filterText)) {
                continue;
            }
            appendUnique(visibleCommands, normalizedCommand);
        }

        std::sort(visibleCommands.begin(), visibleCommands.end(), [](const QString &a, const QString &b) {
            return QString::compare(a, b, Qt::CaseInsensitive) < 0;
        });

        if (visibleCommands.isEmpty()) {
            continue;
        }

        auto *categoryItem =
            new QListWidgetItem(QStringLiteral("[%1]").arg(contextDisplayLabel(contextToken)), blockToolboxList_);
        categoryItem->setFlags(Qt::ItemIsEnabled);
        ++insertedRows;

        for (const QString &commandToken : visibleCommands) {
            const QString label = labelForCommand(commandToken);
            auto *item = new QListWidgetItem(QStringLiteral("  %1").arg(label), blockToolboxList_);
            item->setData(Qt::UserRole, commandToken);
            item->setToolTip(tr("Drag to canvas."));
            ++insertedRows;
        }
    }

    if (insertedRows == 0) {
        auto *emptyItem = new QListWidgetItem(tr("[No commands match filter]"), blockToolboxList_);
        emptyItem->setFlags(Qt::ItemIsEnabled);
    }

    updateBlockToolboxScopeLabel();
}

void TextEditorTab::populateBlockToolboxScopeCombo()
{
    if (blockToolboxScopeCombo_ == nullptr) {
        return;
    }

    const QString previousScope = blockToolboxScopeCombo_->currentData().toString().trimmed().toLower();
    const QSignalBlocker scopeSignalBlocker(blockToolboxScopeCombo_);
    blockToolboxScopeCombo_->clear();
    blockToolboxScopeCombo_->addItem(tr("Auto (selected block)"), QStringLiteral("__auto__"));
    blockToolboxScopeCombo_->addItem(tr("All"), QStringLiteral("all"));
    blockToolboxScopeCombo_->addItem(tr("Top-level"), QStringLiteral("none"));

    QStringList dynamicContexts;
    for (auto contextIterator = contextCommandTokens_.cbegin(); contextIterator != contextCommandTokens_.cend(); ++contextIterator) {
        const QString normalizedContext = normalizeCompletionContext(contextIterator.key());
        if (normalizedContext.isEmpty()
            || normalizedContext == QStringLiteral("all")
            || normalizedContext == QStringLiteral("none")) {
            continue;
        }
        appendUnique(dynamicContexts, normalizedContext);
    }
    std::sort(dynamicContexts.begin(), dynamicContexts.end(), [this](const QString &left, const QString &right) {
        return QString::compare(contextDisplayLabel(left), contextDisplayLabel(right), Qt::CaseInsensitive) < 0;
    });

    for (const QString &contextToken : dynamicContexts) {
        blockToolboxScopeCombo_->addItem(contextDisplayLabel(contextToken), contextToken);
    }

    int restoreIndex = 0;
    if (!previousScope.isEmpty()) {
        restoreIndex = blockToolboxScopeCombo_->findData(previousScope);
    }
    if (restoreIndex < 0) {
        restoreIndex = 0;
    }
    blockToolboxScopeCombo_->setCurrentIndex(restoreIndex);
}

QString TextEditorTab::selectedBlockInsertionContextToken() const
{
    if (blockCanvasScene_ == nullptr) {
        return QStringLiteral("none");
    }

    auto resolveBlockFromItem = [](QGraphicsItem *item) -> BlockCanvasItem * {
        while (item != nullptr) {
            if (auto *blockItem = dynamic_cast<BlockCanvasItem *>(item)) {
                return blockItem;
            }
            item = item->parentItem();
        }
        return nullptr;
    };
    BlockCanvasItem *selectedBlock = nullptr;
    if (!blockCanvasScene_->selectedItems().isEmpty()) {
        selectedBlock = resolveBlockFromItem(blockCanvasScene_->selectedItems().first());
    }
    if (selectedBlock == nullptr) {
        selectedBlock = resolveBlockFromItem(blockCanvasScene_->focusItem());
    }
    if (selectedBlock == nullptr) {
        return QStringLiteral("none");
    }

    const QString selectedKind = normalizeDirective(selectedBlock->kind());
    const QString selectedContext = normalizeCompletionContext(selectedKind);
    if (selectedBlock->isContainerOpen() && !selectedContext.isEmpty()) {
        return selectedContext;
    }

    if (editor_ == nullptr || selectedBlock->lineNumber() <= 0) {
        return QStringLiteral("none");
    }

    QStringList stack;
    const QStringList lines = editor_->toPlainText().split(QLatin1Char('\n'));
    const int lastLine = qMin(selectedBlock->lineNumber() - 1, lines.size());
    for (int lineIndex = 0; lineIndex < lastLine; ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        const QString directive = normalizeDirective(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }

        if (isBlockClosingDirective(directive)) {
            const QString openingDirective = completionOpeningDirectiveForClosing(directive);
            for (int stackIndex = stack.size() - 1; stackIndex >= 0; --stackIndex) {
                if (stack.at(stackIndex) == openingDirective) {
                    stack.removeAt(stackIndex);
                    break;
                }
            }
            continue;
        }

        if (isContainerDirectiveInstance(directive, parsedLine)) {
            stack.append(directive);
        }
    }

    if (stack.isEmpty()) {
        return QStringLiteral("none");
    }

    const QString parentContext = normalizeCompletionContext(stack.constLast());
    return parentContext.isEmpty() ? QStringLiteral("none") : parentContext;
}

void TextEditorTab::updateBlockToolboxScopeLabel()
{
    if (blockToolboxScopeCombo_ == nullptr) {
        return;
    }

    QString selectedScope = QStringLiteral("all");
    selectedScope = blockToolboxScopeCombo_->currentData().toString().trimmed().toLower();

    if (selectedScope == QStringLiteral("__auto__")) {
        const QString contextToken = selectedBlockInsertionContextToken();
        blockToolboxScopeCombo_->setToolTip(tr("Auto scope currently resolves to: %1.")
                                                .arg(contextDisplayLabel(contextToken)));
        return;
    }

    if (selectedScope == QStringLiteral("all")) {
        blockToolboxScopeCombo_->setToolTip(tr("Shows commands from all supported contexts."));
        return;
    }

    const QString normalizedScope = normalizeCompletionContext(selectedScope);
    const QString labelText = contextDisplayLabel(normalizedScope.isEmpty() ? QStringLiteral("none") : normalizedScope);
    blockToolboxScopeCombo_->setToolTip(tr("Shows commands for: %1.").arg(labelText));
}

void TextEditorTab::rebuildBlocksCanvasFromText()
{
    if (blockCanvasScene_ == nullptr || tearingDown_) {
        return;
    }

    const int preferredSelectedLine = blockDetailsSelectedLineNumber_;
    blockMovePreviewLine_ = nullptr;
    blockContainerBoundaryGuideItems_.clear();
    blockContainerBoundaryEndYByLine_.clear();
    {
        const QSignalBlocker sceneSignalBlocker(blockCanvasScene_);
        blockCanvasScene_->clear();
    }

    if (!isBlocksModeSupportedForCurrentFile()) {
        auto *note = blockCanvasScene_->addText(
            tr("Blocks mode is currently available only for .th and .thconfig files."));
        note->setPos(16.0, 16.0);
        return;
    }

    if (blocksModeActive_ && ensureEncodingRootDirectiveForBlocks()) {
        // Normalization replaced the text and triggered a fresh rebuild.
        return;
    }

    QStringList lines = editor_->toPlainText().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }

    struct StackEntry
    {
        QString directive;
        BlockCanvasItem *item = nullptr;
    };

    QVector<StackEntry> stack;
    QVector<BlockCanvasItem *> roots;
    QVector<BlockCanvasItem *> allItems;

    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        if (isFullLineComment(parsedLine)) {
            BlockCanvasItem *parentItem = nullptr;
            if (!stack.isEmpty()) {
                parentItem = stack.last().item;
            }

            auto *item = new BlockCanvasItem(QStringLiteral("comment"),
                                             parsedLine.commentText.trimmed(),
                                             QString(),
                                             parsedLine.lineNumber,
                                             true,
                                             true,
                                             false,
                                             parentItem);
            item->onDelete = [this](int lineNumber) {
                handleBlockDeleteRequest(lineNumber);
            };
            item->onMoveRequest = [this](int lineNumber, const QPointF &scenePos) {
                handleBlockMoveRequest(lineNumber, scenePos);
            };
            item->onMovePreview = [this](int sourceLineNumber, const QPointF &scenePos, bool active) {
                if (active) {
                    updateBlockMovePreview(sourceLineNumber, scenePos);
                } else {
                    clearBlockMovePreview();
                }
            };
            if (parentItem == nullptr) {
                roots.append(item);
                blockCanvasScene_->addItem(item);
            }
            allItems.append(item);
            continue;
        }

        QString directive = normalizeDirective(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }

        if (isBlockClosingDirective(directive)) {
            while (!stack.isEmpty()) {
                const QString openingDirective = stack.last().directive;
                const QString expectedClosing = closingDirectiveFor(openingDirective);
                stack.removeLast();
                if (expectedClosing == directive) {
                    break;
                }
            }
            continue;
        }

        QString activeScope = QStringLiteral("none");
        if (!stack.isEmpty()) {
            activeScope = normalizeDirective(stack.last().directive);
        }

        auto isCommandDirectiveInScope = [this](const QString &commandToken, const QString &scope) {
            QStringList candidates = contextCommandTokens_.value(scope);
            appendUniqueList(candidates, contextCommandTokens_.value(QStringLiteral("all")));
            if (scope == QStringLiteral("none")) {
                appendUniqueList(candidates, contextCommandTokens_.value(QStringLiteral("none")));
            }
            return candidates.contains(commandToken, Qt::CaseInsensitive);
        };

        const bool commandDirective = !isBlockOpeningDirective(directive)
            && isCommandDirectiveInScope(directive, activeScope);
        if (!isBlockOpeningDirective(directive) && !commandDirective) {
            continue;
        }

        BlockCanvasItem *parentItem = nullptr;
        if (!stack.isEmpty()) {
            parentItem = stack.last().item;
        }

        const QString name = blockDisplayName(parsedLine);
        const QString inlineComment = parsedLine.commentStart >= 0 ? parsedLine.commentText.trimmed() : QString();
        const bool encodingDirective = isEncodingDirective(directive);
        const bool isContainerInstance = isContainerDirectiveInstance(directive, parsedLine);
        auto *item = new BlockCanvasItem(directive,
                                         name,
                                         inlineComment,
                                         parsedLine.lineNumber,
                                         !encodingDirective,
                                         !encodingDirective,
                                         isContainerInstance,
                                         parentItem);
        item->onDelete = [this](int lineNumber) {
            handleBlockDeleteRequest(lineNumber);
        };
        item->onMoveRequest = [this](int lineNumber, const QPointF &scenePos) {
            handleBlockMoveRequest(lineNumber, scenePos);
        };
        item->onMovePreview = [this](int sourceLineNumber, const QPointF &scenePos, bool active) {
            if (active) {
                updateBlockMovePreview(sourceLineNumber, scenePos);
            } else {
                clearBlockMovePreview();
            }
        };
        if (parentItem == nullptr) {
            roots.append(item);
            blockCanvasScene_->addItem(item);
        }
        allItems.append(item);

        if (!commandDirective && isContainerInstance) {
            stack.append(StackEntry{directive, item});
        }
    }

    if (allItems.isEmpty()) {
        auto *note = blockCanvasScene_->addText(tr("No Therion directives found."));
        note->setPos(16.0, 16.0);
        return;
    }

    qreal y = 16.0;
    std::function<void(BlockCanvasItem *, int)> layoutTree = [&](BlockCanvasItem *item, int depth) {
        if (item == nullptr) {
            return;
        }
        int visualDepth = depth;
        if (!isEncodingDirective(item->kind())) {
            // Keep `encoding` as visual document root; indent all other content one level below it.
            ++visualDepth;
        }
        const QPointF scenePosition(24.0 + (visualDepth * 28.0), y);
        if (QGraphicsItem *parent = item->parentItem(); parent != nullptr) {
            item->setPos(parent->mapFromScene(scenePosition));
        } else {
            item->setPos(scenePosition);
        }
        y += 52.0;

        QList<BlockCanvasItem *> childBlocks;
        const QList<QGraphicsItem *> children = item->childItems();
        for (QGraphicsItem *child : children) {
            auto *childBlock = dynamic_cast<BlockCanvasItem *>(child);
            if (childBlock != nullptr) {
                childBlocks.append(childBlock);
            }
        }
        std::sort(childBlocks.begin(), childBlocks.end(), [](BlockCanvasItem *left, BlockCanvasItem *right) {
            return left->lineNumber() < right->lineNumber();
        });
        for (BlockCanvasItem *childBlock : childBlocks) {
            layoutTree(childBlock, depth + 1);
        }
    };

    for (BlockCanvasItem *root : roots) {
        layoutTree(root, 0);
    }

    QStringList sourceLines = editor_->toPlainText().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : sourceLines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }

    auto visualSubtreeBottomForSpan = [&allItems](int startLine, int endLine, qreal fallbackBottom) {
        qreal bottom = fallbackBottom;
        for (BlockCanvasItem *blockItem : allItems) {
            if (blockItem == nullptr) {
                continue;
            }
            const int blockLine = blockItem->lineNumber();
            if (blockLine < startLine || blockLine > endLine) {
                continue;
            }
            bottom = qMax(bottom, blockItem->sceneBoundingRect().bottom());
        }
        return bottom;
    };

    struct ContainerBoundary
    {
        BlockCanvasItem *item = nullptr;
        int closingLine = 0;
        qreal minEndY = 0.0;
        qreal endY = 0.0;
        qreal maxEndY = std::numeric_limits<qreal>::max();
        int depth = 0;
    };

    QVector<ContainerBoundary> boundaries;
    boundaries.reserve(allItems.size());

    // Keep closure lines on the same visible rhythm as block-to-block spacing.
    // Because closure lines are thick, include half stroke width in spacing math.
    constexpr qreal kClosureMarkerStrokeWidth = 3.4;
    constexpr qreal kCardToCardGap = 10.0;
    constexpr qreal kClosurePadAboveSubtree = kCardToCardGap + (kClosureMarkerStrokeWidth * 0.5);
    constexpr qreal kClosurePadBeforeNextBlock = kCardToCardGap + (kClosureMarkerStrokeWidth * 0.5);
    constexpr qreal kClosurePreferredOffset = kClosurePadAboveSubtree;

    auto subtreeBottomForItem = [](BlockCanvasItem *root) {
        if (root == nullptr) {
            return 0.0;
        }
        qreal bottom = root->sceneBoundingRect().bottom();
        QList<QGraphicsItem *> stack;
        stack.append(root);
        while (!stack.isEmpty()) {
            QGraphicsItem *current = stack.takeLast();
            if (current == nullptr) {
                continue;
            }
            bottom = qMax(bottom, current->sceneBoundingRect().bottom());
            for (QGraphicsItem *child : current->childItems()) {
                stack.append(child);
            }
        }
        return bottom;
    };

    auto compactImmediateChildGaps = [&allItems]() {
        constexpr qreal kGapEpsilon = 0.5;
        for (BlockCanvasItem *parent : allItems) {
            if (parent == nullptr) {
                continue;
            }

            QList<BlockCanvasItem *> childBlocks;
            const QList<QGraphicsItem *> children = parent->childItems();
            for (QGraphicsItem *child : children) {
                auto *childBlock = dynamic_cast<BlockCanvasItem *>(child);
                if (childBlock != nullptr) {
                    childBlocks.append(childBlock);
                }
            }
            std::sort(childBlocks.begin(), childBlocks.end(), [](BlockCanvasItem *left, BlockCanvasItem *right) {
                return left->lineNumber() < right->lineNumber();
            });
            if (childBlocks.isEmpty()) {
                continue;
            }

            BlockCanvasItem *firstChild = childBlocks.first();
            if (firstChild == nullptr) {
                continue;
            }
            const qreal expectedTop = parent->sceneBoundingRect().bottom() + kCardToCardGap;
            const qreal actualTop = firstChild->sceneBoundingRect().top();
            if (actualTop > expectedTop + kGapEpsilon) {
                const qreal deltaY = actualTop - expectedTop;
                // Move the whole first-child subtree by moving its root item.
                firstChild->moveBy(0.0, -deltaY);
            }
        }
    };

    auto nextBlockTopAfterLine = [&allItems](int lineNumber) {
        qreal nextTop = std::numeric_limits<qreal>::max();
        for (BlockCanvasItem *candidate : allItems) {
            if (candidate == nullptr || candidate->lineNumber() <= lineNumber) {
                continue;
            }
            nextTop = qMin(nextTop, candidate->sceneBoundingRect().top());
        }
        return nextTop;
    };
    auto shiftBlocksAfterLine = [&allItems, &compactImmediateChildGaps](int lineNumber, qreal deltaY) {
        if (deltaY <= 0.0) {
            return;
        }
        for (BlockCanvasItem *candidate : allItems) {
            if (candidate == nullptr || candidate->lineNumber() <= lineNumber) {
                continue;
            }
            candidate->moveBy(0.0, deltaY);
        }
        compactImmediateChildGaps();
    };
    auto maxDescendantLineNumber = [](BlockCanvasItem *root) {
        if (root == nullptr) {
            return 0;
        }
        int maxLine = root->lineNumber();
        QList<QGraphicsItem *> stack;
        stack.append(root);
        while (!stack.isEmpty()) {
            QGraphicsItem *current = stack.takeLast();
            if (current == nullptr) {
                continue;
            }
            if (auto *blockItem = dynamic_cast<BlockCanvasItem *>(current)) {
                maxLine = qMax(maxLine, blockItem->lineNumber());
            }
            for (QGraphicsItem *child : current->childItems()) {
                stack.append(child);
            }
        }
        return maxLine;
    };

    struct ContainerLayoutTarget
    {
        BlockCanvasItem *item = nullptr;
        int openingLine = 0;
        int closingLine = 0;
        int depth = 0;
    };

    QVector<ContainerLayoutTarget> targets;
    targets.reserve(allItems.size());

    for (BlockCanvasItem *item : allItems) {
        if (item == nullptr || !item->isContainerOpen()) {
            continue;
        }

        int closureLine = item->lineNumber();
        const QString openingDirective = normalizeDirective(item->kind());
        const QString closingDirective = closingDirectiveFor(openingDirective);
        if (!openingDirective.isEmpty() && !closingDirective.isEmpty()) {
            const int openingLine = item->lineNumber();
            const int closingLine = findMatchingBlockEndLine(sourceLines,
                                                             openingLine,
                                                             openingDirective,
                                                             closingDirective);
            if (closingLine > openingLine) {
                closureLine = closingLine;
            }
        }
        closureLine = qMax(closureLine, maxDescendantLineNumber(item));

        int depth = 0;
        for (QGraphicsItem *parent = item->parentItem(); parent != nullptr; parent = parent->parentItem()) {
            if (dynamic_cast<BlockCanvasItem *>(parent) != nullptr) {
                ++depth;
            }
        }

        targets.append(ContainerLayoutTarget{item, item->lineNumber(), closureLine, depth});
    }

    std::sort(targets.begin(), targets.end(), [](const ContainerLayoutTarget &left, const ContainerLayoutTarget &right) {
        if (left.closingLine != right.closingLine) {
            return left.closingLine < right.closingLine;
        }
        return left.depth > right.depth;
    });

    for (const ContainerLayoutTarget &target : std::as_const(targets)) {
        BlockCanvasItem *item = target.item;
        if (item == nullptr) {
            continue;
        }

        qreal subtreeBottom = visualSubtreeBottomForSpan(target.openingLine,
                                                         target.closingLine,
                                                         item->sceneBoundingRect().bottom());
        const qreal minEndY = subtreeBottom + kClosurePadAboveSubtree;
        qreal preferredEndY = qMax(subtreeBottom + kClosurePreferredOffset, minEndY);

        qreal maxEndY = std::numeric_limits<qreal>::max();
        const qreal nextTop = nextBlockTopAfterLine(target.closingLine);
        if (nextTop != std::numeric_limits<qreal>::max()) {
            maxEndY = nextTop - kClosurePadBeforeNextBlock;
        }

        if (maxEndY < minEndY) {
            const qreal deltaY = minEndY - maxEndY;
            shiftBlocksAfterLine(target.closingLine, deltaY);
            maxEndY += deltaY;
        }

        boundaries.append(ContainerBoundary{item,
                                            target.closingLine,
                                            minEndY,
                                            qMin(preferredEndY, maxEndY),
                                            maxEndY,
                                            target.depth});
    }

    std::sort(boundaries.begin(), boundaries.end(), [](const ContainerBoundary &left, const ContainerBoundary &right) {
        if (!qFuzzyCompare(left.endY + 1.0, right.endY + 1.0)) {
            return left.endY < right.endY;
        }
        return left.depth > right.depth;
    });

    // Keep nested closure markers visually distinct even when container spans end
    // at the same rendered subtree bottom (for example `endcenterline` + `endsurvey`).
    // Respect each boundary's [minEndY, maxEndY] corridor and back-propagate when
    // a lower marker is clamped by available space.
    constexpr qreal kMinClosureMarkerGap = 10.0;
    for (int i = 0; i < boundaries.size(); ++i) {
        ContainerBoundary &boundary = boundaries[i];
        if (i > 0) {
            const qreal minAllowedY = boundaries.at(i - 1).endY + kMinClosureMarkerGap;
            if (boundary.endY < minAllowedY) {
                boundary.endY = minAllowedY;
            }
        }
        boundary.endY = qMax(boundary.endY, boundary.minEndY);
        if (boundary.endY > boundary.maxEndY) {
            boundary.endY = boundary.maxEndY;
            for (int j = i - 1; j >= 0; --j) {
                ContainerBoundary &previous = boundaries[j];
                const qreal maxAllowedY = boundaries.at(j + 1).endY - kMinClosureMarkerGap;
                if (previous.endY <= maxAllowedY) {
                    break;
                }
                previous.endY = qMax(previous.minEndY, maxAllowedY);
            }
        }
    }

    // If constraints are still too tight for exact nested spacing, expand the
    // layout by pushing subsequent blocks down so closure lines never collapse.
    for (int i = 1; i < boundaries.size(); ++i) {
        const qreal requiredY = boundaries.at(i - 1).endY + kMinClosureMarkerGap;
        if (boundaries.at(i).endY >= requiredY) {
            continue;
        }

        const qreal deltaY = requiredY - boundaries.at(i).endY;
        shiftBlocksAfterLine(boundaries.at(i).closingLine, deltaY);

        for (int j = i; j < boundaries.size(); ++j) {
            ContainerBoundary &shifted = boundaries[j];
            shifted.minEndY += deltaY;
            shifted.endY += deltaY;
            if (shifted.maxEndY < std::numeric_limits<qreal>::max()) {
                shifted.maxEndY += deltaY;
            }
        }
    }

    // Compaction/shift steps above can move cards after a boundary was initially
    // solved. Re-clamp each closure marker against current subtree/next-card
    // geometry so end-caps never ride into the following card.
    const qreal kInfinity = std::numeric_limits<qreal>::max();
    for (ContainerBoundary &boundary : boundaries) {
        BlockCanvasItem *item = boundary.item;
        if (item == nullptr) {
            continue;
        }

        const qreal subtreeBottom = visualSubtreeBottomForSpan(item->lineNumber(),
                                                               boundary.closingLine,
                                                               item->sceneBoundingRect().bottom());
        boundary.minEndY = subtreeBottom + kClosurePadAboveSubtree;

        const qreal nextTop = nextBlockTopAfterLine(boundary.closingLine);
        boundary.maxEndY = nextTop != kInfinity
            ? (nextTop - kClosurePadBeforeNextBlock)
            : kInfinity;

        // Prefer keeping distance from the next card over preserving a stale
        // lower endY, even when constraints are tight.
        if (boundary.maxEndY != kInfinity && boundary.maxEndY < boundary.minEndY) {
            boundary.endY = boundary.maxEndY;
            continue;
        }

        boundary.endY = qMax(boundary.endY, boundary.minEndY);
        if (boundary.maxEndY != kInfinity) {
            boundary.endY = qMin(boundary.endY, boundary.maxEndY);
        }
    }

    QPen connectorPen(QColor(QStringLiteral("#4f6b86")));
    connectorPen.setWidthF(1.2);
    connectorPen.setStyle(Qt::SolidLine);

    qreal minGuideX = std::numeric_limits<qreal>::max();
    qreal maxGuideBottom = 0.0;
    constexpr qreal kConnectorInsetX = 2.0;

    for (const ContainerBoundary &boundary : std::as_const(boundaries)) {
        BlockCanvasItem *item = boundary.item;
        if (item == nullptr) {
            continue;
        }

        const QRectF itemRect = item->sceneBoundingRect();
        const qreal guideX = itemRect.left() + kConnectorInsetX;
        const qreal startY = itemRect.bottom();
        const qreal endY = boundary.endY;

        auto *vertical = blockCanvasScene_->addLine(QLineF(guideX, startY, guideX, endY), connectorPen);
        vertical->setOpacity(0.38);
        vertical->setZValue(-100.0);
        blockContainerBoundaryGuideItems_.append(vertical);

        // Keep the closure marker aligned with the opening card width.
        const qreal endCapStartX = itemRect.left();
        const qreal endCapEndX = itemRect.right();

        QColor closeColor = blockBaseColorForDirective(item->kind()).darker(130);
        closeColor.setAlpha(245);
        QPen closePen(closeColor);
        closePen.setWidthF(kClosureMarkerStrokeWidth);
        closePen.setStyle(Qt::SolidLine);
        auto *endCap = blockCanvasScene_->addLine(QLineF(endCapStartX, endY, endCapEndX, endY), closePen);
        endCap->setOpacity(0.95);
        endCap->setZValue(-99.0);
        endCap->setData(kBlockEndHintContainerLineDataRole, item->lineNumber());
        blockContainerBoundaryGuideItems_.append(endCap);
        blockContainerBoundaryEndYByLine_.insert(item->lineNumber(), endY);

        minGuideX = qMin(minGuideX, guideX);
        maxGuideBottom = qMax(maxGuideBottom, endY);
    }

    blockCanvasScene_->setSceneRect(0.0, 0.0, 1400.0, qMax<qreal>(y + 40.0, 600.0));
    if (minGuideX != std::numeric_limits<qreal>::max() || maxGuideBottom > 0.0) {
        QRectF sceneRect = blockCanvasScene_->sceneRect();
        if (minGuideX != std::numeric_limits<qreal>::max() && minGuideX - 12.0 < sceneRect.left()) {
            sceneRect.setLeft(minGuideX - 12.0);
        }
        if (maxGuideBottom + 16.0 > sceneRect.bottom()) {
            sceneRect.setBottom(maxGuideBottom + 16.0);
        }
        blockCanvasScene_->setSceneRect(sceneRect);
    }

    if (preferredSelectedLine > 0) {
        for (BlockCanvasItem *item : allItems) {
            if (item == nullptr || item->lineNumber() != preferredSelectedLine) {
                continue;
            }
            blockCanvasScene_->clearSelection();
            item->setSelected(true);
            blockCanvasScene_->setFocusItem(item);
            break;
        }
    }

    refreshBlockDetailsSelectionFromScene();
}

void TextEditorTab::updateBlockMovePreview(int sourceLineNumber, const QPointF &scenePos)
{
    if (blockDetailsMovePreviewController_ != nullptr) {
        blockDetailsMovePreviewController_->updateMovePreview(sourceLineNumber, scenePos);
    }
}

void TextEditorTab::clearBlockMovePreview()
{
    if (blockDetailsMovePreviewController_ != nullptr) {
        blockDetailsMovePreviewController_->clearMovePreview();
    }
}

int TextEditorTab::resolveEndHintContainerStartLineAtScenePos(const QPointF &scenePos) const
{
    if (blockDetailsCanvasBoundaryController_ == nullptr) {
        return 0;
    }
    return blockDetailsCanvasBoundaryController_->resolveEndHintContainerStartLineAtScenePos(scenePos);
}

bool TextEditorTab::supportsDetailsPaneForKind(const QString &kind) const
{
    const QString normalizedKind = normalizeDirective(kind);
    if (normalizedKind.isEmpty() || isBlockClosingDirective(normalizedKind)) {
        return false;
    }
    if (normalizedKind == QStringLiteral("encoding")) {
        return false;
    }
    if (isContainerBlockDirective(normalizedKind) || normalizedKind == QStringLiteral("data")) {
        return true;
    }
    if (!commandOptionTokens_.value(normalizedKind).isEmpty()) {
        return true;
    }
    return true;
}

void TextEditorTab::clearBlockDetailsPane()
{
    if (blockDetailsPaneController_ != nullptr) {
        blockDetailsPaneController_->clearDetailsPane();
    }
}

void TextEditorTab::showBlockDetailsForToolboxCommand(const QString &commandToken)
{
    if (blockDetailsToolboxDetailsController_ != nullptr) {
        blockDetailsToolboxDetailsController_->showToolboxCommandDetails(commandToken);
    }
}

void TextEditorTab::selectBlockInCanvasAndDetails(int lineNumber)
{
    if (blockDetailsCanvasSelectionController_ != nullptr) {
        blockDetailsCanvasSelectionController_->selectBlockInCanvasAndDetails(lineNumber);
    }
}

void TextEditorTab::refreshBlockDetailsSelectionFromScene()
{
    if (blockDetailsCanvasSelectionController_ != nullptr) {
        blockDetailsCanvasSelectionController_->refreshDetailsSelectionFromScene();
    }
}

bool TextEditorTab::loadBlockDetailsForSelection(const QString &kind, int lineNumber)
{
    if (blockDetailsSelectionDetailsController_ == nullptr) {
        return false;
    }
    return blockDetailsSelectionDetailsController_->loadSelectionDetails(kind, lineNumber);
}

void TextEditorTab::refreshBlockDetailsOptionArgumentEditors()
{
    if (blockDetailsOptionArgsController_ != nullptr) {
        blockDetailsOptionArgsController_->refreshOptionArgumentEditors();
    }
}

QGraphicsItem *TextEditorTab::resolveBlockCanvasItem(QGraphicsItem *item) const
{
    while (item != nullptr) {
        if (dynamic_cast<BlockCanvasItem *>(item) != nullptr) {
            return item;
        }
        item = item->parentItem();
    }
    return nullptr;
}

int TextEditorTab::blockCanvasItemLineNumber(const QGraphicsItem *item) const
{
    if (const auto *blockItem = dynamic_cast<const BlockCanvasItem *>(item); blockItem != nullptr) {
        return blockItem->lineNumber();
    }
    return 0;
}

QString TextEditorTab::blockCanvasItemKind(const QGraphicsItem *item) const
{
    if (const auto *blockItem = dynamic_cast<const BlockCanvasItem *>(item); blockItem != nullptr) {
        return blockItem->kind();
    }
    return QString();
}

void TextEditorTab::selectBlockCanvasItem(QGraphicsItem *item, bool centerView)
{
    if (blockCanvasScene_ == nullptr || item == nullptr) {
        return;
    }
    blockCanvasScene_->clearSelection();
    item->setSelected(true);
    blockCanvasScene_->setFocusItem(item);
    if (centerView && blockCanvasView_ != nullptr) {
        blockCanvasView_->centerOn(item);
    }
}

void TextEditorTab::updateBlockDetailsHelpForCurrentFocus()
{
    if (blockDetailsHelpController_ != nullptr) {
        blockDetailsHelpController_->updateHelpForCurrentFocus();
    }
}

bool TextEditorTab::blockDetailsReadingsTagEditorHasEditorFocus() const
{
    if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
        return readingsTagEditor->hasEditorFocus();
    }
    return false;
}

QString TextEditorTab::blockDetailsReadingsOrderTextForBuild() const
{
    if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
        return readingsTagEditor->tokens().join(QLatin1Char(' ')).trimmed();
    }
    if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
        return blockDetailsAdditionalPositionalEdit_->text().trimmed();
    }
    return QString();
}

void TextEditorTab::setBlockDetailsReadingsTagEditor(const QString &placeholderText,
                                                     const QStringList &suggestions,
                                                     const QStringList &tokens)
{
    if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
        readingsTagEditor->setPlaceholderText(placeholderText);
        readingsTagEditor->setSuggestions(suggestions);
        readingsTagEditor->setTokens(tokens);
    }
}

void TextEditorTab::installBlockDetailsLineEditCompleter(QLineEdit *lineEdit,
                                                          const QStringList &values) const
{
    installLineEditCompleter(lineEdit, values);
}

bool TextEditorTab::isContainerBlockDirectiveForBlocks(const QString &directive) const
{
    return isContainerBlockDirective(directive);
}

bool TextEditorTab::isRequiredArgumentSignatureForBlocks(const QString &signature) const
{
    return isRequiredArgumentSignature(signature);
}

bool TextEditorTab::buildUpdatedLineFromBlockDetails(QString *updatedLine, QString *validationError) const
{
    if (blockDetailsLineBuildService_ == nullptr) {
        return false;
    }
    return blockDetailsLineBuildService_->buildUpdatedLine(updatedLine, validationError);
}

void TextEditorTab::refreshBlockDetailsApplyState()
{
    if (blockDetailsApplyStateController_ != nullptr) {
        blockDetailsApplyStateController_->refreshApplyState();
    }
}

void TextEditorTab::applyBlockDetailsChanges()
{
    if (blockDetailsApplyExecutor_ != nullptr) {
        blockDetailsApplyExecutor_->applyChanges();
    }
}

bool TextEditorTab::insertLinesBefore(int lineNumber,
                                      const QStringList &newLines,
                                      QString *errorMessage)
{
    if (lineNumber <= 0 || newLines.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Invalid insertion request.");
        }
        return false;
    }

    QStringList trimmedLines;
    trimmedLines.reserve(newLines.size());
    for (const QString &line : newLines) {
        const QString candidate = line.trimmed();
        if (candidate.isEmpty()) {
            continue;
        }
        trimmedLines.append(line);
    }
    if (trimmedLines.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No lines to insert.");
        }
        return false;
    }

    QString contents = editor_->toPlainText();
    const QString lineEnding = contents.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    QStringList lines = contents.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }

    const int insertIndex = qBound(0, lineNumber - 1, lines.size());
    for (int offset = 0; offset < trimmedLines.size(); ++offset) {
        lines.insert(insertIndex + offset, trimmedLines.at(offset));
    }

    const QTextCursor previousCursor = editor_->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    loading_ = true;
    editor_->setPlainText(lines.join(lineEnding));

    const int targetLine = qBound(0, previousLine, qMax(0, editor_->document()->blockCount() - 1));
    const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        editor_->setTextCursor(restoredCursor);
    }

    loading_ = false;
    currentLineNumber_ = editor_->textCursor().blockNumber() + 1;
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
    updateContextHelp();
    rebuildBlocksCanvasFromText();
    return true;
}

void TextEditorTab::handleCanvasDrop(const QString &kind, const QPointF &scenePos)
{
    if (!isBlocksModeSupportedForCurrentFile()) {
        QMessageBox::information(this,
                                 tr("Blocks Mode"),
                                 tr("Blocks mode is available only for .th and .thconfig files."));
        return;
    }

    const QString normalizedKind = kind.trimmed().toLower();
    if (normalizedKind.isEmpty()) {
        return;
    }

    const QString contents = editor_->toPlainText();
    const BlockEditorDocumentOutlineBuilder outlineBuilder(this);
    const BlockEditorDocumentOutline outline = outlineBuilder.buildFromContents(contents);
    const QStringList &existingLines = outline.lines;
    const QVector<BlockEditorDocumentEntry> &entries = outline.entries;
    const QHash<int, int> &entryIndexByStartLine = outline.entryIndexByStartLine;

    const BlockEditorDropTargetResolver targetResolver(this);
    const auto sceneItemsByLine = targetResolver.collectSceneItemsByLine();
    const BlockEditorSceneVerticalPlacement verticalPlacement =
        targetResolver.resolveVerticalPlacement(entries, sceneItemsByLine, scenePos);

    int explicitEndHintContainerLine = 0;
    if (blockCanvasScene_ != nullptr) {
        explicitEndHintContainerLine = resolveEndHintContainerStartLineAtScenePos(scenePos);
    }

    QGraphicsItem *targetBlockItem = nullptr;
    if (!verticalPlacement.beforeAllBlocks
        && !verticalPlacement.afterAllBlocks
        && explicitEndHintContainerLine <= 0) {
        targetBlockItem = targetResolver.resolveTargetItem(entries, sceneItemsByLine, scenePos);
    }

    const int appendLineNumber = qMax(1, editor_->document()->blockCount() + 1);
    const BlockEditorInsertionPlanner insertionPlanner(this);
    const BlockEditorInsertionPlan insertionPlan = insertionPlanner.planInsertion(normalizedKind,
                                                                                  entries,
                                                                                  entryIndexByStartLine,
                                                                                  verticalPlacement,
                                                                                  explicitEndHintContainerLine,
                                                                                  targetBlockItem,
                                                                                  scenePos,
                                                                                  appendLineNumber);
    if (!insertionPlan.compatible) {
        QMessageBox::warning(this,
                             tr("Incompatible Drop"),
                             tr("`%1` cannot be inserted inside `%2`.")
                                 .arg(normalizedKind,
                                      insertionPlan.parentKind.isEmpty() ? tr("root") : insertionPlan.parentKind));
        return;
    }

    const BlockEditorInsertionTemplateBuilder templateBuilder;
    const QStringList linesToInsert = templateBuilder.buildLines(normalizedKind, existingLines, insertionPlan);

    QString errorMessage;
    if (!insertLinesBefore(insertionPlan.insertBeforeLine, linesToInsert, &errorMessage)) {
        QMessageBox::warning(this, tr("Block Insert Failed"), errorMessage);
    }
}

void TextEditorTab::handleBlockMoveRequest(int lineNumber, const QPointF &scenePos)
{
    const auto clearPreviewOnExit = qScopeGuard([this]() {
        clearBlockMovePreview();
    });

    if (!isBlocksModeSupportedForCurrentFile()) {
        return;
    }
    if (lineNumber <= 0 || editor_ == nullptr || blockCanvasScene_ == nullptr) {
        return;
    }

    const QString contents = editor_->toPlainText();
    const BlockEditorDocumentOutlineBuilder outlineBuilder(this);
    const BlockEditorDocumentOutline outline = outlineBuilder.buildFromContents(contents);
    QStringList lines = outline.lines;
    const QVector<BlockEditorDocumentEntry> &entries = outline.entries;
    const QHash<int, int> &entryIndexByStartLine = outline.entryIndexByStartLine;

    const auto sourceIndexIt = entryIndexByStartLine.constFind(lineNumber);
    if (sourceIndexIt == entryIndexByStartLine.cend()) {
        return;
    }
    const BlockEditorDocumentEntry sourceEntry = entries.at(*sourceIndexIt);
    if (sourceEntry.kind == QStringLiteral("encoding")) {
        return;
    }

    const BlockEditorDropTargetResolver targetResolver(this);
    const auto sceneItemsByLine = targetResolver.collectSceneItemsByLine();
    const BlockEditorSceneVerticalPlacement verticalPlacement =
        targetResolver.resolveVerticalPlacement(entries, sceneItemsByLine, scenePos);
    const int explicitEndHintContainerLine = resolveEndHintContainerStartLineAtScenePos(scenePos);
    const BlockEditorMovePlanner movePlanner(this);
    const BlockEditorMovePlan movePlan = movePlanner.planMove(sourceEntry,
                                                              entries,
                                                              entryIndexByStartLine,
                                                              sceneItemsByLine,
                                                              verticalPlacement,
                                                              explicitEndHintContainerLine,
                                                              scenePos,
                                                              lines.size() + 1);
    if (!movePlan.resolved) {
        return;
    }

    if (movePlan.destinationParentLine >= sourceEntry.startLine && movePlan.destinationParentLine <= sourceEntry.endLine) {
        QMessageBox::warning(this, tr("Reorder Block"), tr("Cannot move a block inside itself."));
        return;
    }
    if (movePlan.insertBeforeLineOriginal > sourceEntry.startLine
        && movePlan.insertBeforeLineOriginal <= sourceEntry.endLine + 1) {
        return;
    }

    if (!isCompatibleChildKindForBlocks(movePlan.destinationParentKind, sourceEntry.kind)) {
        QMessageBox::warning(this,
                             tr("Reorder Block"),
                             tr("`%1` cannot be moved under `%2`.")
                                 .arg(sourceEntry.kind,
                                      movePlan.destinationParentKind.isEmpty() ? tr("root") : movePlan.destinationParentKind));
        return;
    }

    const BlockEditorMoveSourceRewriter sourceRewriter;
    const BlockEditorMoveRewriteResult rewriteResult =
        sourceRewriter.rewriteMovedBlock(lines, sourceEntry, movePlan.insertBeforeLineOriginal);
    if (!rewriteResult.applied) {
        return;
    }

    const QString lineEnding = contents.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    replaceTextForCommand(rewriteResult.lines.join(lineEnding));
}

void TextEditorTab::handleBlockConfigureRequest(const QString &kind, int lineNumber)
{
    if (lineNumber <= 0) {
        return;
    }

    QString contents = editor_->toPlainText();
    QStringList lines = contents.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    if (lineNumber > lines.size()) {
        return;
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineNumber - 1), lineNumber);
    if (parsedLine.tokens.isEmpty()) {
        return;
    }

    const QString normalizedKind = kind.toLower();

    enum class IdFieldMode
    {
        None,
        Optional,
        Required,
    };

    auto configureCommandOptionsDialog = [this, &contents, &lines](const QString &commandName,
                                                                    int commandLineNumber,
                                                                    IdFieldMode idFieldMode) {
        if (commandLineNumber <= 0 || commandLineNumber > lines.size()) {
            return;
        }

        const TherionParsedLine commandParsedLine = TherionDocumentParser::parseLine(lines.at(commandLineNumber - 1), commandLineNumber);
        if (commandParsedLine.tokens.isEmpty()) {
            return;
        }

        const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
        const auto match = indentPattern.match(lines.at(commandLineNumber - 1));
        const QString indent = match.hasMatch() ? match.captured(0) : QString();
        const bool hasIdField = idFieldMode != IdFieldMode::None;
        const bool requiresId = idFieldMode == IdFieldMode::Required;

        const BlockEditorParsedCommandOptions parsedOptions =
            parseBlockEditorCommandOptions(commandName,
                                           commandParsedLine.tokens,
                                           commandOptionFixedArityByKey_,
                                           hasIdField);
        const QString currentAdditionalPositionalTokens = parsedOptions.extraPositionalTokens.join(QLatin1Char(' '));

        QDialog dialog(this);
        dialog.setWindowTitle(tr("Configure %1").arg(commandName));
        dialog.setModal(true);
        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);

        auto *formLayout = new QFormLayout;
        QLineEdit *idEdit = nullptr;
        if (hasIdField) {
            idEdit = new QLineEdit(parsedOptions.leadingValue, &dialog);
            idEdit->setPlaceholderText(requiresId ? tr("required") : tr("optional"));
            formLayout->addRow(tr("ID"), idEdit);
        }
        QLineEdit *additionalPositionalTokensEdit = nullptr;
        if (!currentAdditionalPositionalTokens.isEmpty()) {
            additionalPositionalTokensEdit = new QLineEdit(currentAdditionalPositionalTokens, &dialog);
            additionalPositionalTokensEdit->setToolTip(
                tr("Preserved positional tokens that are not parsed as options. "
                   "Prefer using explicit options whenever possible."));
            formLayout->addRow(tr("Extra Arguments (Advanced)"), additionalPositionalTokensEdit);
        }
        layout->addLayout(formLayout);

        auto *optionsLabel = new QLabel(tr("Options"), &dialog);
        layout->addWidget(optionsLabel);

        auto *optionsActionsLayout = new QHBoxLayout;
        optionsActionsLayout->setContentsMargins(0, 0, 0, 0);
        optionsActionsLayout->setSpacing(6);
        auto *addOptionButton = new QPushButton(tr("Add New Option"), &dialog);
        auto *removeOptionButton = new QPushButton(tr("Remove Option"), &dialog);
        addOptionButton->setAutoDefault(false);
        removeOptionButton->setAutoDefault(false);
        optionsActionsLayout->addWidget(addOptionButton);
        optionsActionsLayout->addWidget(removeOptionButton);
        optionsActionsLayout->addStretch(1);
        layout->addLayout(optionsActionsLayout);

        auto *optionsTable = new QTableWidget(&dialog);
        optionsTable->setColumnCount(2);
        optionsTable->setHorizontalHeaderLabels({tr("Option"), tr("Value")});
        optionsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        optionsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        optionsTable->verticalHeader()->setVisible(false);
        optionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        optionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
        optionsTable->setAlternatingRowColors(true);
        optionsTable->setMinimumHeight(160);
        optionsTable->setRowCount(parsedOptions.optionEntries.size());
        for (int row = 0; row < parsedOptions.optionEntries.size(); ++row) {
            const BlockEditorParsedOptionEntry &entry = parsedOptions.optionEntries.at(row);
            optionsTable->setItem(row, 0, new QTableWidgetItem(entry.key));
            optionsTable->setItem(row, 1, new QTableWidgetItem(entry.value));
        }
        layout->addWidget(optionsTable, 1);

        auto *helpLabel = new QLabel(tr("Contextual Help"), &dialog);
        QFont helpLabelFont = helpLabel->font();
        helpLabelFont.setBold(true);
        helpLabel->setFont(helpLabelFont);
        layout->addWidget(helpLabel);

        auto *helpBrowser = new QTextBrowser(&dialog);
        helpBrowser->setOpenLinks(false);
        helpBrowser->setOpenExternalLinks(false);
        helpBrowser->setMinimumHeight(160);
        layout->addWidget(helpBrowser, 1);

        const TherionHelpEntry commandHelpEntry = helpEntries_.value(commandName);
        const QString commandHelpHtml = ContextHelpController::renderHelpHtml(commandName,
                                                                               commandHelpEntry.summary,
                                                                               commandHelpEntry.syntax,
                                                                               commandHelpEntry.arguments,
                                                                               commandHelpEntry.acceptedValues,
                                                                               commandHelpEntry.options,
                                                                               true);

        QString idHelpHtml;
        if (hasIdField) {
            QString idArgumentLine;
            for (const QString &argumentLine : commandHelpEntry.arguments) {
                if (argumentLine.contains(QStringLiteral("<id>"), Qt::CaseInsensitive)) {
                    idArgumentLine = argumentLine.trimmed();
                    break;
                }
            }
            if (idArgumentLine.isEmpty()) {
                for (const QString &argumentLine : commandHelpEntry.arguments) {
                    if (isRequiredArgumentSignature(argumentLine.section(QLatin1Char('='), 0, 0).trimmed())) {
                        idArgumentLine = argumentLine.trimmed();
                        break;
                    }
                }
            }
            if (idArgumentLine.isEmpty() && !commandHelpEntry.arguments.isEmpty()) {
                idArgumentLine = commandHelpEntry.arguments.first().trimmed();
            }

            QString signature = idArgumentLine;
            QString description;
            const int equalsIndex = idArgumentLine.indexOf(QLatin1Char('='));
            if (equalsIndex >= 0) {
                signature = idArgumentLine.left(equalsIndex).trimmed();
                description = idArgumentLine.mid(equalsIndex + 1).trimmed();
            }

            QStringList html;
            html << QStringLiteral("<p><b>Parameter:</b> %1</p>").arg(signature.isEmpty()
                                                                           ? QStringLiteral("&lt;id&gt;")
                                                                           : signature.toHtmlEscaped());
            if (!description.isEmpty()) {
                html << QStringLiteral("<p><b>Description:</b> %1</p>").arg(description.toHtmlEscaped());
            }
            idHelpHtml = html.join(QString());
        }

        const auto updateHelpForCurrentOption = [this, helpBrowser, optionsTable, commandName, commandHelpHtml]() {
            if (helpBrowser == nullptr || optionsTable == nullptr) {
                return;
            }

            const int row = optionsTable->currentRow();
            const QString optionToken = row >= 0 && optionsTable->item(row, 0) != nullptr
                ? optionsTable->item(row, 0)->text().trimmed().toLower()
                : QString();
            const QString optionHelp = commandOptionHelpHtmlByKey_.value(commandOptionHelpKey(commandName, optionToken));
            if (!optionHelp.trimmed().isEmpty()) {
                helpBrowser->setHtml(optionHelp);
                return;
            }

            helpBrowser->setHtml(commandHelpHtml);
        };
        const auto updateHelpForId = [helpBrowser, idHelpHtml, commandHelpHtml]() {
            if (helpBrowser == nullptr) {
                return;
            }
            helpBrowser->setHtml(!idHelpHtml.isEmpty() ? idHelpHtml : commandHelpHtml);
        };
        const auto updateHelpForAdditionalPositionalTokens = [helpBrowser, commandHelpHtml]() {
            if (helpBrowser == nullptr) {
                return;
            }
            const QString html = QStringLiteral("<p><b>Additional positional tokens</b> keep unsupported tokens intact.</p>"
                                                "<p>Prefer explicit key/value options where available.</p>%1")
                                     .arg(commandHelpHtml);
            helpBrowser->setHtml(html);
        };
        if (hasIdField) {
            updateHelpForId();
        } else {
            updateHelpForCurrentOption();
        }

        connect(optionsTable, &QTableWidget::currentCellChanged, &dialog, [updateHelpForCurrentOption](int, int, int, int) {
            updateHelpForCurrentOption();
        });
        connect(optionsTable, &QTableWidget::itemChanged, &dialog, [updateHelpForCurrentOption](QTableWidgetItem *) {
            updateHelpForCurrentOption();
        });
        if (idEdit != nullptr) {
            connect(idEdit, &QLineEdit::selectionChanged, &dialog, [updateHelpForId]() {
                updateHelpForId();
            });
            connect(idEdit, &QLineEdit::textEdited, &dialog, [updateHelpForId](const QString &) {
                updateHelpForId();
            });
        }
        if (additionalPositionalTokensEdit != nullptr) {
            connect(additionalPositionalTokensEdit, &QLineEdit::selectionChanged, &dialog, [updateHelpForAdditionalPositionalTokens]() {
                updateHelpForAdditionalPositionalTokens();
            });
            connect(additionalPositionalTokensEdit, &QLineEdit::textEdited, &dialog, [updateHelpForAdditionalPositionalTokens](const QString &) {
                updateHelpForAdditionalPositionalTokens();
            });
        }

        connect(addOptionButton, &QPushButton::clicked, &dialog, [this, optionsTable, commandName, updateHelpForCurrentOption]() {
            if (optionsTable == nullptr) {
                return;
            }
            const int row = optionsTable->rowCount();
            optionsTable->insertRow(row);
            const QString defaultOption = !commandOptionTokens_.value(commandName).isEmpty()
                ? commandOptionTokens_.value(commandName).first()
                : QStringLiteral("-option");
            optionsTable->setItem(row, 0, new QTableWidgetItem(defaultOption));
            optionsTable->setItem(row, 1, new QTableWidgetItem(QString()));
            optionsTable->setCurrentCell(row, 0);
            optionsTable->editItem(optionsTable->item(row, 0));
            updateHelpForCurrentOption();
        });

        connect(removeOptionButton, &QPushButton::clicked, &dialog, [optionsTable, updateHelpForCurrentOption]() {
            if (optionsTable == nullptr || optionsTable->rowCount() == 0) {
                return;
            }
            const int row = optionsTable->currentRow() >= 0 ? optionsTable->currentRow() : optionsTable->rowCount() - 1;
            optionsTable->removeRow(row);
            updateHelpForCurrentOption();
        });
        if (optionsTable->rowCount() > 0) {
            optionsTable->setCurrentCell(0, 0);
        }

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttonBox);
        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        const QString updatedId = hasIdField && idEdit != nullptr ? idEdit->text().trimmed() : QString();
        if (requiresId && updatedId.isEmpty()) {
            QMessageBox::warning(this, tr("Configure Block"), tr("ID cannot be empty."));
            return;
        }

        QVector<TextEditorOptionRow> optionRows;
        optionRows.reserve(optionsTable->rowCount());
        for (int row = 0; row < optionsTable->rowCount(); ++row) {
            const QString key = (optionsTable->item(row, 0) != nullptr
                                     ? optionsTable->item(row, 0)->text()
                                     : QString())
                                    .trimmed();
            const QString value = (optionsTable->item(row, 1) != nullptr
                                       ? optionsTable->item(row, 1)->text()
                                       : QString())
                                      .trimmed();
            optionRows.append(TextEditorOptionRow{key, value, row + 1});
        }
        const TextEditorOptionValidationResult validation = validateAndSerializeCommandOptions(
            commandName,
            optionRows,
            commandOptionValueArityTokens_,
            commandOptionFixedArityByKey_,
            commandOptionArgumentLabelsByKey_,
            commandOptionValueTokens_,
            false);
        if (!validation.ok) {
            if (validation.failingRow >= 0 && validation.failingRow < optionsTable->rowCount()) {
                optionsTable->setCurrentCell(validation.failingRow, 0);
            }
            QMessageBox::warning(this, tr("Configure Block"), validation.errorMessage);
            return;
        }
        const QStringList serializedOptions = validation.serializedOptions;

        QString updatedLine = QStringLiteral("%1%2").arg(indent, commandName);
        if (hasIdField && !updatedId.isEmpty()) {
            updatedLine += QStringLiteral(" ") + updatedId;
        }
        const QString updatedAdditionalPositionalTokens = additionalPositionalTokensEdit != nullptr
            ? additionalPositionalTokensEdit->text().trimmed()
            : QString();
        if (!updatedAdditionalPositionalTokens.isEmpty()) {
            updatedLine += QStringLiteral(" ") + updatedAdditionalPositionalTokens;
        }
        if (!serializedOptions.isEmpty()) {
            updatedLine += QStringLiteral(" ") + serializedOptions.join(QLatin1Char(' '));
        }

        lines[commandLineNumber - 1] = updatedLine;
        const QString lineEnding = contents.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
        replaceTextForCommand(lines.join(lineEnding));
    };

    if (normalizedKind != QStringLiteral("data")) {
        if (!commandOptionTokens_.value(normalizedKind).isEmpty() || commandSupportsInlineIdField(normalizedKind)) {
            const IdFieldMode idFieldMode = commandHasRequiredIdArgument(normalizedKind)
                ? IdFieldMode::Required
                : (commandSupportsInlineIdField(normalizedKind) ? IdFieldMode::Optional : IdFieldMode::None);
            configureCommandOptionsDialog(normalizedKind, lineNumber, idFieldMode);
            return;
        }
    }

    if (normalizedKind == QStringLiteral("data")) {
        const QString dataScope = resolveScopeForCommandAtLine(QStringLiteral("data"), lines, lineNumber);
        const QString dataScopeClosing = completionClosingDirectiveForOpening(dataScope);
        if (dataScopeClosing.isEmpty()) {
            QMessageBox::warning(this, tr("Configure Data"), tr("Unable to resolve parent data scope."));
            return;
        }

        int dataScopeStartLine = -1;
        int dataScopeDepth = 0;
        for (int currentLine = lineNumber; currentLine >= 1; --currentLine) {
            const TherionParsedLine currentParsedLine = TherionDocumentParser::parseLine(lines.at(currentLine - 1), currentLine);
            const QString directive = normalizeDirective(currentParsedLine.directive);
            if (directive == dataScopeClosing) {
                ++dataScopeDepth;
                continue;
            }
            if (directive != dataScope) {
                continue;
            }
            if (dataScopeDepth == 0) {
                dataScopeStartLine = currentLine;
                break;
            }
            --dataScopeDepth;
        }

        if (dataScopeStartLine <= 0) {
            QMessageBox::warning(this, tr("Configure Data"), tr("Unable to resolve parent data scope block."));
            return;
        }

        const int dataScopeEndLine = findMatchingBlockEndLine(lines,
                                                              dataScopeStartLine,
                                                              dataScope,
                                                              dataScopeClosing);
        if (dataScopeEndLine <= lineNumber) {
            QMessageBox::warning(this, tr("Configure Data"), tr("Unable to resolve end of parent data scope block."));
            return;
        }

        int dataBodyLastLine = dataScopeEndLine - 1;
        for (int currentLine = lineNumber + 1; currentLine <= dataScopeEndLine - 1; ++currentLine) {
            const TherionParsedLine currentParsedLine = TherionDocumentParser::parseLine(lines.at(currentLine - 1), currentLine);
            const QString directive = normalizeDirective(currentParsedLine.directive);
            if (directive.isEmpty() || directive == QStringLiteral("extend")) {
                continue;
            }
            if (directive == QStringLiteral("data")
                || directive == dataScopeClosing
                || (!dataScope.isEmpty() && isCommandDirectiveInScope(directive, dataScope))) {
                dataBodyLastLine = currentLine - 1;
                break;
            }
        }
        if (dataBodyLastLine < lineNumber) {
            dataBodyLastLine = lineNumber;
        }

        const QString currentColumns = parsedLine.tokens.size() > 1
            ? parsedLine.tokens.mid(1).join(QLatin1Char(' '))
            : QString();
        const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
        const auto dataIndentMatch = indentPattern.match(lines.at(lineNumber - 1));
        const QString dataIndent = dataIndentMatch.hasMatch() ? dataIndentMatch.captured(0) : QString();
        const QString rowIndent = dataIndent + QStringLiteral("  ");

        const QStringList dataStyleValues = commandArgumentValueTokens_.value(commandArgumentValueKey(QStringLiteral("data"), 0));
        auto parseDataFields = [dataStyleValues](const QString &columnsText) {
            QStringList tokens = TherionDocumentParser::tokenizeLine(columnsText.trimmed());
            if (tokens.isEmpty()) {
                return QStringList{};
            }
            if (tokens.size() > 1 && dataStyleValues.contains(tokens.first(), Qt::CaseInsensitive)) {
                tokens.removeFirst();
            }
            return tokens;
        };

        const QStringList currentFieldNames = parseDataFields(currentColumns);
        const int currentFieldCount = currentFieldNames.size();
        QStringList directiveSuggestions = contextCommandTokens_.value(dataScope);
        directiveSuggestions.removeAll(QStringLiteral("data"));
        if (!dataScopeClosing.isEmpty()) {
            directiveSuggestions.removeAll(dataScopeClosing);
        }
        const QStringList extendValues = commandArgumentValueTokens_.value(commandArgumentValueKey(QStringLiteral("extend"), 0));
        if (extendValues.isEmpty()) {
            appendUnique(directiveSuggestions, QStringLiteral("extend right"));
            appendUnique(directiveSuggestions, QStringLiteral("extend left"));
            appendUnique(directiveSuggestions, QStringLiteral("extend vertical"));
        } else {
            for (const QString &extendValue : extendValues) {
                appendUnique(directiveSuggestions, QStringLiteral("extend %1").arg(extendValue.trimmed()));
            }
        }
        std::sort(directiveSuggestions.begin(), directiveSuggestions.end(), [](const QString &left, const QString &right) {
            return QString::compare(left, right, Qt::CaseInsensitive) < 0;
        });

        struct MixedRowEntry
        {
            bool directive = false;
            bool commentOnly = false;
            QString directiveText;
            QStringList dataValues;
            QString commentText;
            QChar commentMarker = QLatin1Char('#');
        };
        QVector<MixedRowEntry> initialRows;
        bool schemaMismatchDetected = false;
        for (int currentLine = lineNumber + 1; currentLine <= dataBodyLastLine; ++currentLine) {
            const QString rowLine = lines.at(currentLine - 1);
            const TherionParsedLine parsedRow = TherionDocumentParser::parseLine(rowLine, currentLine);
            QString rowText = parsedRow.commentStart >= 0 ? rowLine.left(parsedRow.commentStart) : rowLine;
            if (rowText.startsWith(rowIndent)) {
                rowText = rowText.mid(rowIndent.size());
            } else {
                rowText = rowText.trimmed();
            }
            rowText = rowText.trimmed();

            const QStringList rowTokens = parsedRow.tokens;
            const QString commentText = parsedRow.commentStart >= 0 ? parsedRow.commentText.trimmed() : QString();
            const QChar commentMarker = (parsedRow.commentStart >= 0 && parsedRow.commentStart < rowLine.size())
                ? rowLine.at(parsedRow.commentStart)
                : QLatin1Char('#');
            const bool looksLikeMeasurementRow = currentFieldCount > 0
                && rowTokens.size() == currentFieldCount
                && (rowTokens.isEmpty() || rowTokens.first().toLower() != QStringLiteral("extend"));
            if (looksLikeMeasurementRow) {
                initialRows.append(MixedRowEntry{false, false, QString(), rowTokens, commentText, commentMarker});
                continue;
            }

            if (rowText.trimmed().isEmpty() && commentText.isEmpty()) {
                initialRows.append(MixedRowEntry{false, false, QString(), {}, QString(), commentMarker});
                continue;
            }

            if (rowText.trimmed().isEmpty() && !commentText.isEmpty()) {
                initialRows.append(MixedRowEntry{false, true, QString(), {}, commentText, commentMarker});
                continue;
            }

            const QString firstToken = rowTokens.isEmpty() ? QString() : rowTokens.first().trimmed().toLower();
            const bool recognizedDirective = firstToken == QStringLiteral("extend")
                || isCommandDirectiveInScope(firstToken, dataScope);
            if (recognizedDirective) {
                initialRows.append(MixedRowEntry{true, false, rowText, {}, commentText, commentMarker});
                continue;
            }

            if (!rowTokens.isEmpty() && currentFieldCount > 0) {
                schemaMismatchDetected = true;
                const QStringList clipped = rowTokens.mid(0, qMin(currentFieldCount, rowTokens.size()));
                initialRows.append(MixedRowEntry{false, false, QString(), clipped, commentText, commentMarker});
                continue;
            }

            initialRows.append(MixedRowEntry{true, false, rowText, {}, commentText, commentMarker});
        }

        if (initialRows.isEmpty()) {
            initialRows.append(MixedRowEntry{false, false, QString(), {}, QString(), QLatin1Char('#')});
        }

        auto *dialog = new QDialog(this);
        dialog->setWindowTitle(tr("Configure Data Block"));
        dialog->setModal(true);
        dialog->resize(900, 520);

        auto *layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(10);

        auto *rowsLabel = new QLabel(tr("Rows (mix measurement rows, directives, and comments)"), dialog);
        layout->addWidget(rowsLabel);

        auto *buttonRow = new QHBoxLayout;
        auto *addDataRowButton = new QPushButton(tr("Add Data Row"), dialog);
        auto *addDirectiveRowButton = new QPushButton(tr("Add Directive Row"), dialog);
        auto *addCommentRowButton = new QPushButton(tr("Add Comment Row"), dialog);
        auto *removeRowButton = new QPushButton(tr("Remove Row"), dialog);
        buttonRow->addWidget(addDataRowButton);
        buttonRow->addWidget(addDirectiveRowButton);
        buttonRow->addWidget(addCommentRowButton);
        buttonRow->addWidget(removeRowButton);
        buttonRow->addStretch(1);
        layout->addLayout(buttonRow);

        auto *rowsTable = new DataRowsTableWidget(dialog);
        rowsTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                   | QAbstractItemView::SelectedClicked
                                   | QAbstractItemView::EditKeyPressed);
        rowsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
        rowsTable->setSelectionMode(QAbstractItemView::SingleSelection);
        rowsTable->horizontalHeader()->setStretchLastSection(true);
        rowsTable->verticalHeader()->setVisible(false);
        rowsTable->setAlternatingRowColors(false);
        rowsTable->setRowCount(initialRows.size());

        QStringList headerLabels;
        headerLabels << tr("Type");
        for (const QString &fieldName : currentFieldNames) {
            headerLabels << dataFieldDisplayLabel(fieldName);
        }
        headerLabels << tr("Directive") << tr("Comment");
        rowsTable->setColumnCount(headerLabels.size());
        rowsTable->setHorizontalHeaderLabels(headerLabels);

        const int directiveColumn = headerLabels.size() - 2;
        const int commentColumn = headerLabels.size() - 1;

        auto applyRowType = [rowsTable, directiveColumn, commentColumn](int row, const QString &typeText) {
            if (row < 0 || row >= rowsTable->rowCount()) {
                return;
            }

            const QString normalizedType = typeText.trimmed().toLower();
            const bool directiveRow = normalizedType == QStringLiteral("directive");
            const bool commentRow = normalizedType == QStringLiteral("comment");
            const bool dataRow = !directiveRow && !commentRow;
            const int dataStartColumn = 1;

            if (auto *typeItem = rowsTable->item(row, 0); typeItem != nullptr) {
                typeItem->setText(directiveRow ? QStringLiteral("Directive")
                                               : (commentRow ? QStringLiteral("Comment")
                                                             : QStringLiteral("Data")));
            }

            for (int column = dataStartColumn; column < directiveColumn; ++column) {
                QTableWidgetItem *item = rowsTable->item(row, column);
                if (item == nullptr) {
                    item = new QTableWidgetItem;
                    rowsTable->setItem(row, column, item);
                }
                Qt::ItemFlags flags = item->flags();
                if (dataRow) {
                    flags |= Qt::ItemIsEditable;
                    flags |= Qt::ItemIsEnabled;
                } else {
                    flags &= ~Qt::ItemIsEditable;
                    flags &= ~Qt::ItemIsEnabled;
                    item->setText(QString());
                }
                item->setFlags(flags);
            }

            {
                QTableWidgetItem *item = rowsTable->item(row, directiveColumn);
                if (item == nullptr) {
                    item = new QTableWidgetItem;
                    rowsTable->setItem(row, directiveColumn, item);
                }
                Qt::ItemFlags flags = item->flags();
                if (directiveRow) {
                    flags |= Qt::ItemIsEditable;
                    flags |= Qt::ItemIsEnabled;
                } else {
                    flags &= ~Qt::ItemIsEditable;
                    flags &= ~Qt::ItemIsEnabled;
                    item->setText(QString());
                }
                item->setFlags(flags);
            }

            {
                QTableWidgetItem *item = rowsTable->item(row, commentColumn);
                if (item == nullptr) {
                    item = new QTableWidgetItem;
                    rowsTable->setItem(row, commentColumn, item);
                }
                Qt::ItemFlags flags = item->flags();
                if (directiveRow || commentRow || dataRow) {
                    flags |= Qt::ItemIsEditable;
                    flags |= Qt::ItemIsEnabled;
                }
                item->setFlags(flags);
            }
        };

        for (int row = 0; row < initialRows.size(); ++row) {
            const MixedRowEntry &entry = initialRows.at(row);
            const QString typeText = entry.commentOnly ? QStringLiteral("Comment")
                : (entry.directive ? QStringLiteral("Directive") : QStringLiteral("Data"));
            rowsTable->setItem(row, 0, new QTableWidgetItem(typeText));

            for (int column = 1; column < directiveColumn; ++column) {
                const int valueIndex = column - 1;
                const QString value = valueIndex < entry.dataValues.size()
                    ? entry.dataValues.at(valueIndex)
                    : QString();
                rowsTable->setItem(row, column, new QTableWidgetItem(value));
            }

            rowsTable->setItem(row, directiveColumn, new QTableWidgetItem(entry.directiveText));
            rowsTable->setItem(row, commentColumn, new QTableWidgetItem(entry.commentText));
            applyRowType(row, typeText);
        }

        if (rowsTable->rowCount() > 0) {
            rowsTable->setCurrentCell(0, 0);
        }

        rowsTable->setAdvanceColumns({directiveColumn, commentColumn});
        rowsTable->onViewportResized = [rowsTable]() {
            if (rowsTable == nullptr || rowsTable->columnCount() <= 0) {
                return;
            }
            const int fixedTypeColumnWidth = 110;
            const int fixedDirectiveColumnWidth = 130;
            const int fixedCommentColumnWidth = 130;
            rowsTable->setColumnWidth(0, fixedTypeColumnWidth);
            rowsTable->setColumnWidth(rowsTable->columnCount() - 2, fixedDirectiveColumnWidth);
            rowsTable->setColumnWidth(rowsTable->columnCount() - 1, fixedCommentColumnWidth);
            const int viewportWidth = rowsTable->viewport()->width();
            const int dataColumns = qMax(0, rowsTable->columnCount() - 3);
            if (dataColumns <= 0) {
                return;
            }
            const int remainingWidth =
                qMax(0, viewportWidth - fixedTypeColumnWidth - fixedDirectiveColumnWidth - fixedCommentColumnWidth);
            const int perColumnWidth = qMax(90, remainingWidth / dataColumns);
            for (int column = 1; column <= dataColumns; ++column) {
                rowsTable->setColumnWidth(column, perColumnWidth);
            }
        };

        auto suggestionsProvider = [directiveColumn, commentColumn, currentFieldNames, directiveSuggestions](const QModelIndex &index) {
            if (!index.isValid()) {
                return QStringList{};
            }
            if (index.column() == directiveColumn) {
                return directiveSuggestions;
            }
            if (index.column() == commentColumn) {
                return QStringList{};
            }
            const int fieldIndex = index.column() - 1;
            if (fieldIndex >= 0 && fieldIndex < currentFieldNames.size()) {
                return QStringList{};
            }
            return QStringList{};
        };
        auto editableProvider = [rowsTable, directiveColumn](const QModelIndex &index) {
            if (!index.isValid()) {
                return false;
            }
            if (index.column() == 0) {
                return true;
            }
            QTableWidgetItem *typeItem = rowsTable->item(index.row(), 0);
            const QString rowType = typeItem != nullptr ? typeItem->text().trimmed().toLower() : QStringLiteral("data");
            if (rowType == QStringLiteral("directive")) {
                return index.column() == directiveColumn || index.column() == rowsTable->columnCount() - 1;
            }
            if (rowType == QStringLiteral("comment")) {
                return index.column() == rowsTable->columnCount() - 1;
            }
            return index.column() >= 1 && index.column() < rowsTable->columnCount();
        };

        rowsTable->setItemDelegate(new DataRowsTableDelegate(suggestionsProvider, editableProvider, rowsTable));
        rowsTable->onViewportResized();
        layout->addWidget(rowsTable, 1);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
        layout->addWidget(buttons);

        QObject::connect(addDataRowButton, &QPushButton::clicked, dialog, [rowsTable]() {
            const int row = rowsTable->rowCount();
            rowsTable->insertRow(row);
            rowsTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("Data")));
            rowsTable->setCurrentCell(row, 0);
        });

        QObject::connect(addDirectiveRowButton, &QPushButton::clicked, dialog, [rowsTable]() {
            const int row = rowsTable->rowCount();
            rowsTable->insertRow(row);
            rowsTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("Directive")));
            rowsTable->setCurrentCell(row, 0);
        });

        QObject::connect(addCommentRowButton, &QPushButton::clicked, dialog, [rowsTable]() {
            const int row = rowsTable->rowCount();
            rowsTable->insertRow(row);
            rowsTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("Comment")));
            rowsTable->setCurrentCell(row, rowsTable->columnCount() - 1);
        });

        QObject::connect(removeRowButton, &QPushButton::clicked, dialog, [rowsTable]() {
            const int currentRow = rowsTable->currentRow();
            if (currentRow >= 0 && currentRow < rowsTable->rowCount()) {
                rowsTable->removeRow(currentRow);
            }
            if (rowsTable->rowCount() == 0) {
                rowsTable->insertRow(0);
                rowsTable->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Data")));
                rowsTable->setCurrentCell(0, 0);
            }
        });

        QObject::connect(rowsTable,
                         &QTableWidget::itemChanged,
                         dialog,
                         [rowsTable, applyRowType](QTableWidgetItem *item) {
                             if (item == nullptr || item->column() != 0) {
                                 return;
                             }
                             applyRowType(item->row(), item->text());
                         });

        QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, [this,
                                                                        dialog,
                                                                        rowsTable,
                                                                        lineNumber,
                                                                        dataBodyLastLine,
                                                                        currentFieldCount,
                                                                        currentFieldNames,
                                                                        dataStyleValues,
                                                                        dataIndent,
                                                                        rowIndent,
                                                                        schemaMismatchDetected]() {
            QStringList nextLines = editor_->toPlainText().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
            for (QString &line : nextLines) {
                if (line.endsWith(QLatin1Char('\r'))) {
                    line.chop(1);
                }
            }
            if (lineNumber <= 0 || lineNumber > nextLines.size()) {
                dialog->reject();
                return;
            }

            QStringList columnsTokens = TherionDocumentParser::tokenizeLine(nextLines.at(lineNumber - 1).trimmed());
            if (!columnsTokens.isEmpty() && columnsTokens.first().compare(QStringLiteral("data"), Qt::CaseInsensitive) == 0) {
                columnsTokens.removeFirst();
            }
            if (!columnsTokens.isEmpty() && dataStyleValues.contains(columnsTokens.first(), Qt::CaseInsensitive)) {
                // keep style token as is
            }
            const QStringList configuredFields = currentFieldNames;

            QStringList rowLines;
            bool mismatchDetected = schemaMismatchDetected;

            for (int row = 0; row < rowsTable->rowCount(); ++row) {
                const QString rowType = (rowsTable->item(row, 0) != nullptr
                                             ? rowsTable->item(row, 0)->text().trimmed().toLower()
                                             : QStringLiteral("data"));
                const QString commentText = (rowsTable->item(row, rowsTable->columnCount() - 1) != nullptr
                                                 ? rowsTable->item(row, rowsTable->columnCount() - 1)->text().trimmed()
                                                 : QString());

                if (rowType == QStringLiteral("directive")) {
                    const QString directiveText = (rowsTable->item(row, rowsTable->columnCount() - 2) != nullptr
                                                      ? rowsTable->item(row, rowsTable->columnCount() - 2)->text().trimmed()
                                                      : QString());
                    if (directiveText.isEmpty() && commentText.isEmpty()) {
                        continue;
                    }
                    QString line = rowIndent + directiveText;
                    if (!commentText.isEmpty()) {
                        if (!directiveText.isEmpty()) {
                            line += QStringLiteral(" ");
                        }
                        line += QStringLiteral("# ") + commentText;
                    }
                    rowLines.append(line.trimmed().isEmpty() ? QString() : line);
                    continue;
                }

                if (rowType == QStringLiteral("comment")) {
                    if (commentText.isEmpty()) {
                        continue;
                    }
                    rowLines.append(rowIndent + QStringLiteral("# ") + commentText);
                    continue;
                }

                QStringList values;
                for (int column = 1; column < rowsTable->columnCount() - 2; ++column) {
                    values.append(rowsTable->item(row, column) != nullptr
                                      ? rowsTable->item(row, column)->text().trimmed()
                                      : QString());
                }

                const bool rowCompletelyEmpty = std::all_of(values.begin(), values.end(), [](const QString &value) {
                    return value.trimmed().isEmpty();
                });
                if (rowCompletelyEmpty && commentText.isEmpty()) {
                    continue;
                }

                const int nonEmptyCount = std::count_if(values.begin(), values.end(), [](const QString &value) {
                    return !value.trimmed().isEmpty();
                });
                if (nonEmptyCount > 0 && nonEmptyCount < currentFieldCount) {
                    mismatchDetected = true;
                }

                QStringList serializedValues;
                for (const QString &value : values) {
                    serializedValues.append(value.trimmed());
                }

                QString line = rowIndent + serializedValues.join(QLatin1Char(' ')).trimmed();
                if (!commentText.isEmpty()) {
                    if (!line.trimmed().isEmpty()) {
                        line += QStringLiteral(" ");
                    }
                    line += QStringLiteral("# ") + commentText;
                }
                rowLines.append(line.trimmed().isEmpty() ? QString() : line);
            }

            const int removeCount = qMax(0, dataBodyLastLine - lineNumber);
            for (int index = 0; index < removeCount && lineNumber < nextLines.size(); ++index) {
                nextLines.removeAt(lineNumber);
            }
            for (int row = rowLines.size() - 1; row >= 0; --row) {
                nextLines.insert(lineNumber, rowLines.at(row));
            }

            const QString lineEnding = editor_->toPlainText().contains(QStringLiteral("\r\n"))
                ? QStringLiteral("\r\n")
                : QStringLiteral("\n");
            replaceTextForCommand(nextLines.join(lineEnding));
            dialog->accept();

            if (mismatchDetected) {
                QMessageBox::information(this,
                                         tr("Schema no longer matches"),
                                         tr("Some existing data rows did not match the current columns schema and were preserved as best effort. Please review rows in Raw mode if needed."));
            }
        });

        dialog->exec();
        return;
    }

    const bool hasCatalogOptions = !commandOptionTokens_.value(normalizedKind).isEmpty();
    if (!isContainerBlockDirective(normalizedKind)
        && normalizedKind != QStringLiteral("data")
        && !hasCatalogOptions) {
        const QString currentValue = parsedLine.tokens.size() > 1
            ? parsedLine.tokens.mid(1).join(QLatin1Char(' '))
            : QString();

        QDialog dialog(this);
        dialog.setWindowTitle(tr("Configure Block"));
        dialog.setModal(true);

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);

        auto *formLayout = new QFormLayout;
        auto *valueEdit = new QLineEdit(currentValue, &dialog);
        formLayout->addRow(tr("Value"), valueEdit);
        layout->addLayout(formLayout);

        TherionHelpEntry helpEntry = helpEntries_.value(normalizedKind);
        if (helpEntry.summary.trimmed().isEmpty()
            && helpEntry.syntax.trimmed().isEmpty()
            && helpEntry.arguments.isEmpty()
            && helpEntry.options.isEmpty()
            && helpEntry.acceptedValues.isEmpty()) {
            const QString openingDirective = completionOpeningDirectiveForClosing(normalizedKind);
            if (!openingDirective.isEmpty()) {
                helpEntry = helpEntries_.value(openingDirective);
            }
        }

        auto *helpLabel = new QLabel(tr("Contextual Help"), &dialog);
        QFont helpLabelFont = helpLabel->font();
        helpLabelFont.setBold(true);
        helpLabel->setFont(helpLabelFont);
        layout->addWidget(helpLabel);

        auto *helpBrowser = new QTextBrowser(&dialog);
        helpBrowser->setOpenLinks(false);
        helpBrowser->setOpenExternalLinks(false);
        helpBrowser->setMinimumHeight(140);
        helpBrowser->setHtml(ContextHelpController::renderHelpHtml(normalizedKind,
                                                                   helpEntry.summary,
                                                                   helpEntry.syntax,
                                                                   helpEntry.arguments,
                                                                   helpEntry.acceptedValues,
                                                                   helpEntry.options,
                                                                   true));
        layout->addWidget(helpBrowser, 1);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttonBox);
        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        const QString updatedValue = valueEdit->text();
        if (updatedValue.trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Configure Block"), tr("Value cannot be empty."));
            return;
        }
        QString updatedLine = QStringLiteral("%1 %2").arg(normalizedKind, updatedValue.trimmed());
        const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
        const auto match = indentPattern.match(lines.at(lineNumber - 1));
        if (match.hasMatch()) {
            updatedLine.prepend(match.captured(0));
        }
        lines[lineNumber - 1] = updatedLine;

        const QString lineEnding = contents.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
        replaceTextForCommand(lines.join(lineEnding));
        return;
    }

    QMessageBox::information(this,
                             tr("Configure Block"),
                             tr("Configuration for `%1` is not implemented yet.")
                                 .arg(kind));
}

bool TextEditorTab::removeSourceLineRange(int startLine, int endLine)
{
    if (editor_ == nullptr || editor_->document() == nullptr || startLine <= 0 || endLine < startLine) {
        return false;
    }

    QTextDocument *document = editor_->document();
    const QTextBlock startBlock = document->findBlockByLineNumber(startLine - 1);
    const QTextBlock endBlock = document->findBlockByLineNumber(endLine - 1);
    if (!startBlock.isValid() || !endBlock.isValid()) {
        return false;
    }

    const QTextBlock afterEndBlock = endBlock.next();
    const int selectionStart = startBlock.position();
    const int selectionEnd = afterEndBlock.isValid()
        ? afterEndBlock.position()
        : endBlock.position() + qMax(0, endBlock.length() - 1);
    if (selectionEnd < selectionStart) {
        return false;
    }

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    cursor.setPosition(selectionStart);
    cursor.setPosition(selectionEnd, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.endEditBlock();
    editor_->setTextCursor(cursor);
    return true;
}

bool TextEditorTab::handleBlockDeleteRequest(int lineNumber)
{
    if (lineNumber <= 0) {
        return false;
    }

    QString contents = editor_->toPlainText();
    QStringList lines = contents.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    if (lineNumber > lines.size()) {
        return false;
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineNumber - 1), lineNumber);
    const QString directive = normalizeDirective(parsedLine.directive);
    if (directive.isEmpty() && isFullLineComment(parsedLine)) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            tr("Delete Block"),
            tr("Delete comment line?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return false;
        }
        return removeSourceLineRange(lineNumber, lineNumber);
    }
    if (directive.isEmpty()) {
        return false;
    }
    if (directive == QStringLiteral("encoding")) {
        QMessageBox::information(this,
                                 tr("Delete Block"),
                                 tr("`encoding` is fixed as the document root in Blocks mode and cannot be deleted."));
        return false;
    }

    int removeStartLine = lineNumber;
    int removeEndLine = lineNumber;

    if (isContainerDirectiveInstance(directive, parsedLine)) {
        const QString closingDirective = closingDirectiveFor(directive);
        const int endLine = findMatchingBlockEndLine(lines, lineNumber, directive, closingDirective);
        if (endLine <= lineNumber) {
            QMessageBox::warning(this,
                                 tr("Delete Block"),
                                 tr("Unable to resolve closing directive for `%1`.").arg(directive));
            return false;
        }
        removeEndLine = endLine;
    } else if (directive == QStringLiteral("data")) {
        const QString dataScope = resolveScopeForCommandAtLine(QStringLiteral("data"), lines, lineNumber);
        const QString dataScopeClosing = completionClosingDirectiveForOpening(dataScope);
        if (dataScopeClosing.isEmpty()) {
            QMessageBox::warning(this, tr("Delete Block"), tr("Unable to resolve parent data scope."));
            return false;
        }

        int dataScopeStartLine = -1;
        int dataScopeDepth = 0;
        for (int currentLine = lineNumber; currentLine >= 1; --currentLine) {
            const TherionParsedLine currentParsedLine = TherionDocumentParser::parseLine(lines.at(currentLine - 1), currentLine);
            const QString currentDirective = normalizeDirective(currentParsedLine.directive);
            if (currentDirective == dataScopeClosing) {
                ++dataScopeDepth;
                continue;
            }
            if (currentDirective != dataScope) {
                continue;
            }
            if (dataScopeDepth == 0) {
                dataScopeStartLine = currentLine;
                break;
            }
            --dataScopeDepth;
        }

        if (dataScopeStartLine <= 0) {
            QMessageBox::warning(this, tr("Delete Block"), tr("Unable to resolve parent data scope block."));
            return false;
        }

        const int dataScopeEndLine = findMatchingBlockEndLine(lines,
                                                              dataScopeStartLine,
                                                              dataScope,
                                                              dataScopeClosing);
        if (dataScopeEndLine <= lineNumber) {
            QMessageBox::warning(this, tr("Delete Block"), tr("Unable to resolve end of parent data scope block."));
            return false;
        }

        int dataBodyLastLine = dataScopeEndLine - 1;
        for (int currentLine = lineNumber + 1; currentLine <= dataScopeEndLine - 1; ++currentLine) {
            const TherionParsedLine currentParsedLine = TherionDocumentParser::parseLine(lines.at(currentLine - 1), currentLine);
            const QString currentDirective = normalizeDirective(currentParsedLine.directive);
            if (currentDirective.isEmpty() || currentDirective == QStringLiteral("extend")) {
                continue;
            }
            if (currentDirective == QStringLiteral("data")
                || currentDirective == dataScopeClosing
                || (!dataScope.isEmpty() && isCommandDirectiveInScope(currentDirective, dataScope))) {
                dataBodyLastLine = currentLine - 1;
                break;
            }
        }
        removeEndLine = qMax(lineNumber, dataBodyLastLine);
    }

    const QString label = directive == QStringLiteral("data")
        ? tr("data block")
        : tr("`%1` block").arg(directive);
    const int lineCount = qMax(1, removeEndLine - removeStartLine + 1);
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        tr("Delete Block"),
        tr("Delete %1 (%2 line(s))?").arg(label).arg(lineCount),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return false;
    }

    return removeSourceLineRange(removeStartLine, removeEndLine);
}

bool TextEditorTab::isCurrentStateDirty() const
{
    const bool textDirty = editor_ != nullptr && editor_->toPlainText() != cleanTextSnapshot_;
    const bool encodingDirty = fileEncodingName_.compare(cleanEncodingNameSnapshot_, Qt::CaseInsensitive) != 0;
    return textDirty || encodingDirty;
}

void TextEditorTab::applyDirtyStateFromCurrentState()
{
    const bool dirtyNow = isCurrentStateDirty();
    if (editor_ != nullptr && editor_->document() != nullptr) {
        editor_->document()->setModified(dirtyNow);
    }
    if (dirty_ == dirtyNow) {
        return;
    }

    dirty_ = dirtyNow;
    refreshTitle();
    emit dirtyStateChanged(dirty_);
}

void TextEditorTab::handleTextChanged()
{
    if (loading_) {
        return;
    }

    rebuildBlocksCanvasFromText();
    applyDirtyStateFromCurrentState();
    emit documentTextChanged();
}

void TextEditorTab::handleFindTextEdited(const QString &)
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleFindTextEdited();
    }
}

void TextEditorTab::handleReplaceTextEdited(const QString &)
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleReplaceTextEdited();
    }
}

void TextEditorTab::handleSearchOptionsChanged(bool)
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleSearchOptionsChanged();
    }
}

void TextEditorTab::handleFindNextTriggered()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleFindNextTriggered();
    }
}

void TextEditorTab::handleFindPreviousTriggered()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleFindPreviousTriggered();
    }
}

void TextEditorTab::handleReplaceTriggered()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleReplaceTriggered();
    }
}

void TextEditorTab::handleReplaceAllTriggered()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleReplaceAllTriggered();
    }
}

void TextEditorTab::handleCloseSearchTriggered()
{
    if (rawEditorFindController_ != nullptr) {
        rawEditorFindController_->handleCloseSearchTriggered();
    }
}

void TextEditorTab::handleRawModeRequested()
{
    setBlocksModeActive(false);
}

void TextEditorTab::handleBlocksModeRequested()
{
    setBlocksModeActive(true);
}

void TextEditorTab::handleConvertToUtf8Triggered()
{
    if (fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        return;
    }

    const auto answer = QMessageBox::question(this,
                                              tr("Convert to UTF-8"),
                                              tr("Convert this document from %1 to UTF-8?\n\n"
                                                 "The file on disk is not changed until you save.")
                                                  .arg(fileEncodingLabel_),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    fileEncodingName_ = QStringLiteral("UTF-8");
    fileEncodingLabel_ = QStringLiteral("UTF-8");
    encodingStatusNote_ = tr("Converted to UTF-8 in memory. Save to write UTF-8 to disk.");
    if (isBlocksModeSupportedForCurrentFile()) {
        ensureEncodingRootDirectiveForBlocks();
    }
    applyDirtyStateFromCurrentState();
    refreshStatus();
}

void TextEditorTab::handleCursorPositionChanged()
{
    if (loading_) {
        return;
    }

    const QTextCursor cursor = editor_->textCursor();
    const int currentLineNumber = cursor.blockNumber() + 1;
    const int currentColumnNumber = cursor.positionInBlock() + 1;
    const bool lineChanged = currentLineNumber != currentLineNumber_;
    const bool columnChanged = currentColumnNumber != currentColumnNumber_;
    if (currentLineNumber != currentLineNumber_) {
        currentLineNumber_ = currentLineNumber;
        emit currentLineChanged(currentLineNumber_);
    }
    if (columnChanged) {
        currentColumnNumber_ = currentColumnNumber;
    }
    if (lineChanged || columnChanged) {
        emit cursorPositionChanged(currentLineNumber_, currentColumnNumber_);
    }

    highlightedLineNumber_ = currentLineNumber_;
    refreshCurrentLineHighlight();
    updateContextHelp();
    updateValidationTooltipForCursor();
}

void TextEditorTab::refreshCurrentLineHighlight()
{
    if (editor_ == nullptr || editor_->isReadOnly()) {
        return;
    }

    if (auto *highlightEditor = dynamic_cast<HighlightPlainTextEdit *>(editor_)) {
        highlightEditor->setHighlightedLineNumber(highlightedLineNumber_);
    }
}

void TextEditorTab::refreshTitle()
{
    refreshStatus();
    emit titleChanged();
}

void TextEditorTab::refreshStatus()
{
    if (encodingNoteLabel_ != nullptr) {
        encodingNoteLabel_->setText(encodingStatusNote_);
        encodingNoteLabel_->setVisible(!encodingStatusNote_.isEmpty());
    }
    if (convertEncodingButton_ != nullptr) {
        const bool showConversion = fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) != 0;
        convertEncodingButton_->setVisible(showConversion);
        convertEncodingButton_->setEnabled(showConversion);
    }

    const bool showInlineRow = inlineStatusRequestedVisible_ && (!encodingStatusNote_.isEmpty()
                                                                 || (convertEncodingButton_ != nullptr
                                                                     && convertEncodingButton_->isVisible()));
    if (statusRow_ != nullptr) {
        statusRow_->setVisible(showInlineRow);
    }
}

void TextEditorTab::buildHelpPanel()
{
    auto *framedHelpPanel = new QFrame(this);
    framedHelpPanel->setFrameShape(QFrame::NoFrame);
    helpPanel_ = framedHelpPanel;
    helpPanel_->setObjectName(QStringLiteral("textContextHelpPanel"));
    syncPanelSurfaceToBaseTone(helpPanel_);
    auto *panelLayout = new QVBoxLayout(helpPanel_);
    panelLayout->setContentsMargins(kPanelPadding, kPanelPadding, kPanelPadding, kPanelPadding);
    panelLayout->setSpacing(kPanelSpacing);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);

    auto *headerLabel = new QLabel(tr("Contextual Help"), helpPanel_);
    QFont headerFont = headerLabel->font();
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);

    headerRow->addWidget(headerLabel);
    headerRow->addStretch(1);

    helpBrowser_ = new QTextBrowser(helpPanel_);
    helpBrowser_->setFrameShape(QFrame::NoFrame);
    helpBrowser_->setOpenLinks(false);
    helpBrowser_->setOpenExternalLinks(false);
    helpBrowser_->setMinimumHeight(120);
    syncTextBrowserSurfaceToParent(helpBrowser_);
    helpBrowser_->setHtml(tr("<p>Select a Therion command or item to see contextual help.</p>"));

    panelLayout->addLayout(headerRow);
    panelLayout->addWidget(helpBrowser_, 1);
}

void TextEditorTab::loadHelpMetadata()
{
    helpEntries_.clear();
    completionTokens_.clear();
    commandCompletionTokens_.clear();
    commandOptionTokens_.clear();
    commandValueTokens_.clear();
    commandArgumentValueTokens_.clear();
    commandOptionValueTokens_.clear();
    commandOptionValueArityTokens_.clear();
    commandOptionArgumentLabelsByKey_.clear();
    commandOptionFixedArityByKey_.clear();
    commandOptionHelpHtmlByKey_.clear();
    commandTypeValueTokens_.clear();
    commandSubtypeByTypeTokens_.clear();
    commandRequiredPositionalCount_.clear();
    commandArgumentSignaturesByToken_.clear();
    commandPrimaryValueIsPerson_.clear();
    contextCommandTokens_.clear();
    blockCommandContextsByKind_.clear();
    loadHelpMetadataFromCommandCatalog();
    if (rawEditorCompletionController_ != nullptr) {
        rawEditorCompletionController_->rebuildCompletionModel();
    }
    populateBlockToolboxScopeCombo();
}

void TextEditorTab::loadHelpMetadataFromCommandCatalog()
{
    resetCatalogBlockDirectiveMetadataToDefaults();

    const QJsonObject catalogObject = CommandCatalogService::catalogObject();
    if (catalogObject.isEmpty()) {
        return;
    }
    applyCatalogBlockDirectiveMetadata(catalogObject);
    if (rawEditorCompletionController_ != nullptr) {
        rawEditorCompletionController_->applyCatalogCommandsMetadata(catalogObject);
    }
}

bool TextEditorTab::eventFilter(QObject *watched, QEvent *event)
{
    if (event != nullptr && watched == qApp) {
        switch (event->type()) {
        case QEvent::ApplicationPaletteChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            handleApplicationAppearanceChanged();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(watched, event);
    }

    if (rawEditorCompletionController_ == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    switch (rawEditorCompletionController_->handleEventFilter(watched, event)) {
    case RawEditorCompletionController::EventHandling::Consumed:
        return true;
    case RawEditorCompletionController::EventHandling::PassToBase:
        return QWidget::eventFilter(watched, event);
    case RawEditorCompletionController::EventHandling::NotHandled:
    default:
        return QWidget::eventFilter(watched, event);
    }
}

QString TextEditorTab::currentCompletionPrefix() const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QString();
    }
    return rawEditorCompletionController_->currentCompletionPrefix();
}

QString TextEditorTab::normalizeCompletionContext(const QString &contextToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return normalizedCompletionContextToken(contextToken);
    }
    return rawEditorCompletionController_->normalizeCompletionContext(contextToken);
}

bool TextEditorTab::isCompatibleChildKindForBlocks(const QString &parentKind, const QString &childKind) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->isCompatibleChildKindForBlocks(parentKind, childKind);
}

bool TextEditorTab::isCommandDirectiveInScope(const QString &directive, const QString &scopeToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->isCommandDirectiveInScope(directive, scopeToken);
}

QStringList TextEditorTab::commandArgumentSignaturesFor(const QString &commandToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return {};
    }
    return rawEditorCompletionController_->commandArgumentSignaturesFor(commandToken);
}

bool TextEditorTab::commandHasRequiredIdArgument(const QString &commandToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->commandHasRequiredIdArgument(commandToken);
}

bool TextEditorTab::commandSupportsInlineIdField(const QString &commandToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return false;
    }
    return rawEditorCompletionController_->commandSupportsInlineIdField(commandToken);
}

QString TextEditorTab::primaryInsertionScopeForCommand(const QString &commandToken) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QString();
    }
    return rawEditorCompletionController_->primaryInsertionScopeForCommand(commandToken);
}

QString TextEditorTab::resolveScopeForCommandAtLine(const QString &commandToken,
                                                    const QStringList &lines,
                                                    int lineNumber) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QString();
    }
    return rawEditorCompletionController_->resolveScopeForCommandAtLine(commandToken, lines, lineNumber);
}

QStringList TextEditorTab::activeCompletionScopeStack() const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QStringList();
    }
    return rawEditorCompletionController_->activeCompletionScopeStack();
}

QString TextEditorTab::currentCompletionCommand() const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QString();
    }
    return rawEditorCompletionController_->currentCompletionCommand();
}

QString TextEditorTab::currentCompletionScopeLabel() const
{
    if (rawEditorCompletionController_ == nullptr) {
        return tr("top-level");
    }
    return rawEditorCompletionController_->currentCompletionScopeLabel();
}

QStringList TextEditorTab::projectInputFileCompletionCandidates() const
{
    if (rawEditorCompletionController_ == nullptr) {
        return {};
    }
    return rawEditorCompletionController_->projectInputFileCompletionCandidates();
}

QStringList TextEditorTab::buildCompletionSuggestionsForCursor(const QString &prefix) const
{
    if (rawEditorCompletionController_ == nullptr) {
        return QStringList();
    }
    return rawEditorCompletionController_->buildCompletionSuggestionsForCursor(prefix);
}

void TextEditorTab::triggerCompletionPopup()
{
    if (rawEditorCompletionController_ == nullptr) {
        return;
    }
    rawEditorCompletionController_->triggerCompletionPopup();
}

void TextEditorTab::insertCompletionToken(const QString &completion)
{
    if (rawEditorCompletionController_ == nullptr) {
        return;
    }
    rawEditorCompletionController_->insertCompletionToken(completion);
}

QString TextEditorTab::normalizedDirectiveToken(const QString &directive) const
{
    return normalizeDirective(directive);
}

QString TextEditorTab::openingDirectiveForClosingToken(const QString &directive) const
{
    return completionOpeningDirectiveForClosing(directive);
}

QString TextEditorTab::closingDirectiveForOpeningToken(const QString &directive) const
{
    return completionClosingDirectiveForOpening(directive);
}

bool TextEditorTab::isContainerDirectiveInstanceForParsedLine(const QString &directive,
                                                              const TherionParsedLine &parsedLine) const
{
    return isContainerDirectiveInstance(directive, parsedLine);
}

void TextEditorTab::updateValidationTooltipForCursor()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->updateValidationTooltipForCursor();
    }
}

void TextEditorTab::updateContextHelp()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->updateContextHelp();
    }
}

void TextEditorTab::setHelpCollapsed(bool collapsed)
{
    helpCollapsed_ = collapsed;
    if (helpBrowser_ != nullptr) {
        helpBrowser_->setVisible(!collapsed);
    }
    if (editorHelpSplitter_ != nullptr) {
        if (!collapsed && helpPanelExtent_ > 0) {
            const QList<int> sizes = editorHelpSplitter_->sizes();
            editorHelpSplitter_->setSizes(QList<int>{sizes.value(0, 1), helpPanelExtent_});
        } else if (collapsed) {
            const QList<int> sizes = editorHelpSplitter_->sizes();
            if (sizes.size() >= 2) {
                const int minimumExtent = helpPanel_ != nullptr
                    ? qMax(helpPanel_->minimumSizeHint().width(), helpPanel_->minimumWidth())
                    : 1;
                helpPanelExtent_ = qMax(sizes.at(1), minimumExtent);
            }
            editorHelpSplitter_->setSizes(QList<int>{1, 0});
        }
    }
}

QString TextEditorTab::displayPath() const
{
    if (filePath_.isEmpty()) {
        return tr("No file open");
    }

    if (projectRootPath_.isEmpty()) {
        return filePath_;
    }

    const QString relativePath = QDir(projectRootPath_).relativeFilePath(filePath_);
    if (relativePath.isEmpty() || relativePath.startsWith(QStringLiteral(".."))) {
        return filePath_;
    }

    return relativePath;
}

}
