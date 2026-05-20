#include "TextEditorTab.h"

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
#include <QDirIterator>
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

#include "../core/CommandCatalogService.h"
#include "../core/DocumentFile.h"
#include "../core/TherionDocumentEditor.h"
#include "../core/TherionDocumentParser.h"
#include "../editor/TherionSyntaxHighlighter.h"

namespace
{
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

QString renderList(const QStringList &items)
{
    if (items.isEmpty()) {
        return QString();
    }

    QString html = QStringLiteral("<ul>");
    for (const QString &item : items) {
        html += QStringLiteral("<li>%1</li>").arg(item.toHtmlEscaped());
    }
    html += QStringLiteral("</ul>");
    return html;
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

QString normalizeInputSuggestionPath(QString path)
{
    path = QDir::fromNativeSeparators(path).trimmed();
    if (path.isEmpty() || path == QStringLiteral(".")) {
        return QString();
    }
    while (path.startsWith(QStringLiteral("./"))) {
        path.remove(0, 2);
        path = path.trimmed();
    }
    return path;
}

QString normalizeInputCompletionPrefix(QString prefix)
{
    prefix = QDir::fromNativeSeparators(prefix).trimmed();
    if (prefix.isEmpty()) {
        return prefix;
    }

    while (prefix.startsWith(QStringLiteral("./"))) {
        prefix.remove(0, 2);
    }
    return prefix;
}

bool isActivatedInputPathPrefix(const QString &rawPrefix)
{
    const QString normalized = QDir::fromNativeSeparators(rawPrefix).trimmed();
    return normalized.startsWith(QStringLiteral("./")) || normalized.startsWith(QStringLiteral("../"));
}

QString commandOptionValueKey(const QString &commandName, const QString &optionToken)
{
    return commandName.trimmed().toLower() + QStringLiteral("\x1f")
        + optionToken.trimmed().toLower();
}

QString commandOptionHelpKey(const QString &commandName, const QString &optionToken)
{
    return commandName.trimmed().toLower() + QStringLiteral("\x1fhelp\x1f")
        + optionToken.trimmed().toLower();
}

QString commandArgumentValueKey(const QString &commandName, int argumentIndex)
{
    return commandName.trimmed().toLower() + QStringLiteral("\x1farg\x1f")
        + QString::number(qMax(argumentIndex, 0));
}

QStringList extractOptionKeys(const QString &optionKeyField)
{
    QStringList keys;
    const QStringList segments = optionKeyField.split(QRegularExpression(QStringLiteral("[,/]")),
                                                      Qt::SkipEmptyParts);
    for (QString segment : segments) {
        segment = segment.trimmed();
        if (!segment.startsWith(QLatin1Char('-'))) {
            continue;
        }
        if (segment.contains(QLatin1Char('<')) || segment.contains(QLatin1Char('>'))) {
            continue;
        }

        static const QRegularExpression keyPattern(QStringLiteral("^-[A-Za-z][A-Za-z0-9-]*$"));
        if (!keyPattern.match(segment).hasMatch()) {
            continue;
        }
        appendUnique(keys, segment);
    }
    return keys;
}

bool looksLikeOptionToken(const QString &token)
{
    static const QRegularExpression keyPattern(QStringLiteral("^-[A-Za-z][A-Za-z0-9-]*$"));
    return keyPattern.match(token.trimmed()).hasMatch();
}

QString normalizeSymbolTypeToken(const QString &token)
{
    QString normalized = token.trimmed().toLower();
    if (normalized.isEmpty()) {
        return QString();
    }
    if (normalized.startsWith(QLatin1Char('-'))) {
        return QString();
    }

    const int separatorIndex = normalized.indexOf(QLatin1Char(':'));
    if (separatorIndex > 0) {
        normalized = normalized.left(separatorIndex).trimmed();
    }
    return normalized;
}

QString symbolTypeForSubtypeLookup(const QString &commandName, const TherionStudio::TherionParsedLine &parsedLine)
{
    const QString normalizedCommand = commandName.trimmed().toLower();

    if (normalizedCommand == QStringLiteral("line")
        || normalizedCommand == QStringLiteral("area")) {
        for (int tokenIndex = 1; tokenIndex < parsedLine.tokens.size(); ++tokenIndex) {
            const QString token = parsedLine.tokens.at(tokenIndex).trimmed();
            if (token.startsWith(QLatin1Char('-'))) {
                break;
            }

            const QString normalizedType = normalizeSymbolTypeToken(token);
            if (!normalizedType.isEmpty()) {
                return normalizedType;
            }
        }
        return QString();
    }

    if (normalizedCommand == QStringLiteral("point")) {
        int positionalToken = 0;
        for (int tokenIndex = 1; tokenIndex < parsedLine.tokens.size(); ++tokenIndex) {
            const QString token = parsedLine.tokens.at(tokenIndex).trimmed();
            if (token.isEmpty()) {
                continue;
            }
            if (token.startsWith(QLatin1Char('-'))) {
                break;
            }

            ++positionalToken;
            if (positionalToken == 3) {
                return normalizeSymbolTypeToken(token);
            }
        }
    }

    return QString();
}

void appendConcreteSubtypeValues(QStringList &target, const QStringList &source)
{
    for (const QString &value : source) {
        const QString normalized = value.trimmed();
        if (normalized.isEmpty()) {
            continue;
        }
        if (normalized == QStringLiteral("*")) {
            continue;
        }
        if (normalized.startsWith(QLatin1Char('<')) && normalized.endsWith(QLatin1Char('>'))) {
            continue;
        }

        appendUnique(target, normalized);
    }
}

int positionalTokenCountBeforeCursor(const TherionStudio::TherionParsedLine &parsedLine, int tokenIndexExclusive)
{
    int positionalCount = 0;
    for (int index = 1; index < tokenIndexExclusive && index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (token.startsWith(QLatin1Char('-'))) {
            break;
        }
        ++positionalCount;
    }
    return positionalCount;
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

QString quoteTherionArgument(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

bool shouldQuoteTherionArgument(const QString &value)
{
    if (value.isEmpty()) {
        return true;
    }

    static const QRegularExpression requiresQuotePattern(
        QStringLiteral(R"([\s#%"\[\]\\])"));
    return requiresQuotePattern.match(value).hasMatch();
}

bool isBracketedTherionValueGroup(const QString &value)
{
    const QString trimmed = value.trimmed();
    return trimmed.size() >= 2
        && trimmed.startsWith(QLatin1Char('['))
        && trimmed.endsWith(QLatin1Char(']'));
}

QString serializeTherionArgumentToken(const QString &value)
{
    if (isBracketedTherionValueGroup(value)) {
        return value.trimmed();
    }
    if (shouldQuoteTherionArgument(value)) {
        return quoteTherionArgument(value);
    }
    return value;
}

QString canonicalOptionArityToken(const QString &rawArityToken)
{
    const QString normalized = rawArityToken.trimmed().toUpper();
    if (normalized == QStringLiteral("0") || normalized == QStringLiteral("NONE")) {
        return QStringLiteral("NONE");
    }
    if (normalized == QStringLiteral("1") || normalized == QStringLiteral("EXACTLY_ONE")) {
        return QStringLiteral("EXACTLY_ONE");
    }
    if (normalized == QStringLiteral("N") || normalized == QStringLiteral("ONE_OR_MORE")) {
        return QStringLiteral("ONE_OR_MORE");
    }
    if (normalized == QStringLiteral("*") || normalized == QStringLiteral("ZERO_OR_MORE")) {
        return QStringLiteral("ZERO_OR_MORE");
    }
    return normalized;
}

bool optionArityRequiresValue(const QString &rawArityToken)
{
    const QString arity = canonicalOptionArityToken(rawArityToken);
    return arity == QStringLiteral("EXACTLY_ONE") || arity == QStringLiteral("ONE_OR_MORE");
}

bool optionArityForbidsValue(const QString &rawArityToken)
{
    return canonicalOptionArityToken(rawArityToken) == QStringLiteral("NONE");
}

bool configureOptionTokenLooksNumeric(QString token)
{
    token = token.trimmed();
    while (token.startsWith(QLatin1Char('['))) {
        token.remove(0, 1);
    }
    while (token.endsWith(QLatin1Char(']'))) {
        token.chop(1);
    }
    if (token.isEmpty()) {
        return false;
    }

    bool ok = false;
    token.toDouble(&ok);
    return ok;
}

bool configureTokenStartsNewOption(const QString &token)
{
    const QString trimmed = token.trimmed();
    return trimmed.startsWith(QLatin1Char('-'))
        && !configureOptionTokenLooksNumeric(trimmed);
}

int nextConfigureOptionIndex(const QStringList &tokens, int optionIndex)
{
    bool inBracketedValue = false;
    for (int scan = optionIndex + 1; scan < tokens.size(); ++scan) {
        const QString token = tokens.at(scan).trimmed();
        if (inBracketedValue) {
            if (token.contains(QLatin1Char(']'))) {
                inBracketedValue = false;
            }
            continue;
        }

        if (scan == optionIndex + 1 && token.contains(QLatin1Char('[')) && !token.contains(QLatin1Char(']'))) {
            inBracketedValue = true;
            continue;
        }

        if (configureTokenStartsNewOption(token)) {
            return scan;
        }
    }

    return tokens.size();
}

QStringList parseOptionValuesFromEditor(const QString &rawValue, const QString &rawArityToken, int fixedArity)
{
    const QString value = rawValue.trimmed();
    if (value.isEmpty()) {
        return {};
    }

    const QString arity = canonicalOptionArityToken(rawArityToken);
    const bool singleValueExpected = fixedArity == 1
        || (fixedArity < 0 && arity == QStringLiteral("EXACTLY_ONE"));

    if (singleValueExpected) {
        // In UI, a single-value option cell represents one logical value even when it contains spaces.
        // If user entered an explicitly quoted token, prefer parser-unquoted token.
        const QStringList tokenized = TherionStudio::TherionDocumentParser::tokenizeLine(value);
        if (tokenized.size() == 1 && tokenized.first() != value) {
            return tokenized;
        }
        return {value};
    }

    QStringList values = TherionStudio::TherionDocumentParser::tokenizeLine(value);
    if (values.isEmpty()) {
        values.append(value);
    }
    return values;
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

QString blockDisplayName(const TherionStudio::TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.size() >= 2) {
        return parsedLine.tokens.at(1);
    }
    return QString();
}

QString normalizeDirectiveToken(const QString &directive)
{
    const QString normalized = directive.trimmed().toLower();
    if (normalized == QStringLiteral("centreline")) {
        return QStringLiteral("centerline");
    }
    if (normalized == QStringLiteral("endcentreline")) {
        return QStringLiteral("endcenterline");
    }
    return normalized;
}

QHash<QString, QString> defaultBlockOpenToCloseMap()
{
    return {
        {QStringLiteral("survey"), QStringLiteral("endsurvey")},
        {QStringLiteral("centerline"), QStringLiteral("endcenterline")},
        {QStringLiteral("map"), QStringLiteral("endmap")},
        {QStringLiteral("scrap"), QStringLiteral("endscrap")},
        {QStringLiteral("surface"), QStringLiteral("endsurface")},
        {QStringLiteral("layout"), QStringLiteral("endlayout")},
        {QStringLiteral("lookup"), QStringLiteral("endlookup")},
        {QStringLiteral("line"), QStringLiteral("endline")},
        {QStringLiteral("area"), QStringLiteral("endarea")},
    };
}

QHash<QString, QString> invertBlockOpenCloseMap(const QHash<QString, QString> &openToCloseMap)
{
    QHash<QString, QString> closeToOpenMap;
    for (auto iterator = openToCloseMap.constBegin(); iterator != openToCloseMap.constEnd(); ++iterator) {
        const QString openDirective = normalizeDirectiveToken(iterator.key());
        const QString closeDirective = normalizeDirectiveToken(iterator.value());
        if (openDirective.isEmpty() || closeDirective.isEmpty()) {
            continue;
        }
        closeToOpenMap.insert(closeDirective, openDirective);
    }
    return closeToOpenMap;
}

QHash<QString, QString> gBlockOpenToCloseMap = defaultBlockOpenToCloseMap();
QHash<QString, QString> gBlockCloseToOpenMap = invertBlockOpenCloseMap(gBlockOpenToCloseMap);

QColor blockBaseColorForDirective(const QString &directive)
{
    const QString normalizedKind = normalizeDirectiveToken(directive);
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

void resetCatalogBlockDirectiveMetadataToDefaults()
{
    gBlockOpenToCloseMap = defaultBlockOpenToCloseMap();
    gBlockCloseToOpenMap = invertBlockOpenCloseMap(gBlockOpenToCloseMap);
}

void applyCatalogBlockDirectiveMetadata(const QJsonObject &catalogObject)
{
    const QJsonArray blockPairs = catalogObject.value(QStringLiteral("block_pairs")).toArray();
    if (blockPairs.isEmpty()) {
        return;
    }

    QHash<QString, QString> openToCloseMap;
    for (const QJsonValue &pairValue : blockPairs) {
        const QJsonObject pairObject = pairValue.toObject();
        const QString openDirective = normalizeDirectiveToken(pairObject.value(QStringLiteral("open_directive")).toString());
        const QString closeDirective = normalizeDirectiveToken(pairObject.value(QStringLiteral("close_directive")).toString());
        if (openDirective.isEmpty() || closeDirective.isEmpty() || openDirective.startsWith(QStringLiteral("end"))) {
            continue;
        }
        openToCloseMap.insert(openDirective, closeDirective);
    }

    if (openToCloseMap.isEmpty()) {
        return;
    }

    gBlockOpenToCloseMap = openToCloseMap;
    gBlockCloseToOpenMap = invertBlockOpenCloseMap(gBlockOpenToCloseMap);
}

bool isBlockOpeningDirective(const QString &directive)
{
    const QString normalizedDirective = normalizeDirectiveToken(directive);
    return normalizedDirective == QStringLiteral("data")
        || gBlockOpenToCloseMap.contains(normalizedDirective);
}

bool isContainerBlockDirective(const QString &directive)
{
    const QString normalizedDirective = normalizeDirectiveToken(directive);
    return gBlockOpenToCloseMap.contains(normalizedDirective);
}

bool isContainerDirectiveInstance(const QString &directive, const TherionStudio::TherionParsedLine &parsedLine)
{
    const QString normalizedDirective = normalizeDirectiveToken(directive);
    if (!gBlockOpenToCloseMap.contains(normalizedDirective)) {
        return false;
    }

    // `source` supports both inline form (`source file.th`) and block form
    // (`source` ... `endsource`). Only the block form opens a nested scope.
    if (normalizedDirective == QStringLiteral("source") && parsedLine.tokens.size() > 1) {
        return false;
    }

    return true;
}

bool isBlockClosingDirective(const QString &directive)
{
    const QString normalizedDirective = normalizeDirectiveToken(directive);
    return gBlockCloseToOpenMap.contains(normalizedDirective);
}

QString closingDirectiveFor(const QString &openingDirective)
{
    return gBlockOpenToCloseMap.value(normalizeDirectiveToken(openingDirective));
}

QString normalizeDirective(const QString &directive)
{
    return normalizeDirectiveToken(directive);
}

bool isEncodingDirective(const QString &directive)
{
    return normalizeDirective(directive) == QStringLiteral("encoding");
}

bool isFullLineComment(const TherionStudio::TherionParsedLine &parsedLine)
{
    if (parsedLine.commentStart < 0) {
        return false;
    }
    const QString beforeComment = parsedLine.rawText.left(parsedLine.commentStart).trimmed();
    return beforeComment.isEmpty() && !parsedLine.commentText.trimmed().isEmpty();
}

QString normalizedCompletionContextToken(const QString &token)
{
    const QString normalized = normalizeDirective(token.trimmed());
    if (normalized == QStringLiteral("all")) {
        return QStringLiteral("all");
    }
    if (normalized == QStringLiteral("none")) {
        return QStringLiteral("none");
    }
    static const QRegularExpression contextTokenPattern(QStringLiteral("^[a-z][a-z0-9-]*$"));
    if (contextTokenPattern.match(normalized).hasMatch()) {
        return normalized;
    }
    return QString();
}

QString contextDisplayLabel(const QString &contextToken)
{
    const QString normalized = normalizedCompletionContextToken(contextToken);
    if (normalized == QStringLiteral("none")) {
        return QObject::tr("Top-level");
    }
    if (normalized == QStringLiteral("all")) {
        return QObject::tr("All");
    }

    QStringList parts = normalized.split(QLatin1Char('-'), Qt::SkipEmptyParts);
    for (QString &part : parts) {
        if (!part.isEmpty()) {
            part[0] = part.at(0).toUpper();
        }
    }
    return QObject::tr("Inside %1").arg(parts.join(QLatin1Char(' ')));
}

QString completionOpeningDirectiveForClosing(const QString &directive)
{
    return gBlockCloseToOpenMap.value(normalizeDirectiveToken(directive));
}

QString completionClosingDirectiveForOpening(const QString &directive)
{
    return gBlockOpenToCloseMap.value(normalizeDirectiveToken(directive));
}

int findMatchingBlockEndLine(const QStringList &lines,
                             int openingLineNumber,
                             const QString &openingDirective,
                             const QString &closingDirective)
{
    if (openingLineNumber <= 0 || openingLineNumber > lines.size()) {
        return -1;
    }

    int depth = 0;
    for (int lineNumber = openingLineNumber; lineNumber <= lines.size(); ++lineNumber) {
        const TherionStudio::TherionParsedLine parsedLine = TherionStudio::TherionDocumentParser::parseLine(lines.at(lineNumber - 1), lineNumber);
        const QString directive = normalizeDirective(parsedLine.directive);
        if (directive == openingDirective) {
            ++depth;
            continue;
        }
        if (directive != closingDirective) {
            continue;
        }

        --depth;
        if (depth == 0) {
            return lineNumber;
        }
    }

    return -1;
}

}

namespace TherionStudio
{
TextEditorTab::TextEditorTab(QWidget *parent)
    : QWidget(parent)
{
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
    if (blocksModeActive_) {
        setBlocksModeActive(false);
    }

    replaceMode_ = replaceMode;
    updateSearchVisibility(replaceMode);
    searchBar_->setVisible(true);

    if (findEdit_->text().isEmpty() && !editor_->textCursor().selectedText().isEmpty()) {
        findEdit_->setText(editor_->textCursor().selectedText());
    }

    findEdit_->setFocus();
    findEdit_->selectAll();
    updateSearchResults(tr("Ready"));
}

void TextEditorTab::hideFindBar()
{
    searchBar_->setVisible(false);
    editor_->setFocus();
}

bool TextEditorTab::findNext()
{
    return performFind(true, true);
}

bool TextEditorTab::findPrevious()
{
    return performFind(false, true);
}

bool TextEditorTab::replaceCurrent()
{
    const QString findText = currentFindText();
    if (findText.isEmpty()) {
        updateSearchResults(tr("Enter text to find."), true);
        return false;
    }

    QTextCursor cursor = editor_->textCursor();
    if (!cursor.hasSelection() || cursor.selectedText() != findText) {
        if (!performFind(true, true)) {
            return false;
        }

        cursor = editor_->textCursor();
    }

    cursor.insertText(currentReplaceText());
    editor_->setTextCursor(cursor);
    updateSearchResults(tr("Replaced current match."));
    return performFind(true, true);
}

int TextEditorTab::replaceAll()
{
    const QString findText = currentFindText();
    if (findText.isEmpty()) {
        updateSearchResults(tr("Enter text to find."), true);
        return 0;
    }

    const QString replaceText = currentReplaceText();
    QTextCursor cursor(editor_->document());
    cursor.beginEditBlock();

    int replacements = 0;
    cursor = editor_->document()->find(findText, cursor, findFlags());
    while (!cursor.isNull()) {
        cursor.insertText(replaceText);
        ++replacements;
        cursor = editor_->document()->find(findText, cursor, findFlags());
    }

    cursor.endEditBlock();
    editor_->moveCursor(QTextCursor::Start);
    updateSearchResults(tr("Replaced %1 occurrence(s).").arg(replacements));
    return replacements;
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
    if (blockCanvasScene_ == nullptr) {
        return;
    }

    const QList<QGraphicsItem *> sceneItems = blockCanvasScene_->items();

    auto resolveBlockFromItem = [](QGraphicsItem *item) -> BlockCanvasItem * {
        while (item != nullptr) {
            if (auto *blockItem = dynamic_cast<BlockCanvasItem *>(item)) {
                return blockItem;
            }
            item = item->parentItem();
        }
        return nullptr;
    };
    auto subtreeBottomForItem = [](BlockCanvasItem *rootItem) -> qreal {
        if (rootItem == nullptr) {
            return 0.0;
        }
        qreal subtreeBottom = rootItem->sceneBoundingRect().bottom();
        QList<QGraphicsItem *> stack;
        stack.append(rootItem);
        while (!stack.isEmpty()) {
            QGraphicsItem *current = stack.takeLast();
            if (current == nullptr) {
                continue;
            }
            subtreeBottom = qMax(subtreeBottom, current->sceneBoundingRect().bottom());
            for (QGraphicsItem *child : current->childItems()) {
                stack.append(child);
            }
        }
        return subtreeBottom;
    };

    qreal sceneTop = 0.0;
    qreal sceneBottom = 0.0;
    bool hasSceneVerticalBounds = false;
    for (QGraphicsItem *item : sceneItems) {
        auto *blockItem = dynamic_cast<BlockCanvasItem *>(item);
        if (blockItem == nullptr) {
            continue;
        }
        const QRectF blockRect = blockItem->sceneBoundingRect();
        if (!hasSceneVerticalBounds) {
            sceneTop = blockRect.top();
            sceneBottom = blockRect.bottom();
            hasSceneVerticalBounds = true;
        } else {
            sceneTop = qMin(sceneTop, blockRect.top());
            sceneBottom = qMax(sceneBottom, blockRect.bottom());
        }
    }

    const int hintedContainerLine = resolveEndHintContainerStartLineAtScenePos(scenePos);
    if (hintedContainerLine > 0) {
        BlockCanvasItem *hintedContainerItem = nullptr;
        for (QGraphicsItem *item : sceneItems) {
            auto *blockItem = dynamic_cast<BlockCanvasItem *>(item);
            if (blockItem == nullptr || blockItem->lineNumber() != hintedContainerLine) {
                continue;
            }
            hintedContainerItem = blockItem;
            break;
        }
        if (hintedContainerItem != nullptr && hintedContainerItem->lineNumber() != sourceLineNumber) {
            qreal y = blockContainerBoundaryEndYByLine_.value(hintedContainerLine,
                                                              subtreeBottomForItem(hintedContainerItem) + 10.0) + 4.0;
            const qreal left = 10.0;
            const qreal right = qMax<qreal>(left + 120.0, blockCanvasScene_->sceneRect().right() - 10.0);
            if (blockMovePreviewLine_ == nullptr) {
                QPen guidePen(QColor(QStringLiteral("#2f6fed")));
                guidePen.setWidthF(2.0);
                guidePen.setStyle(Qt::DashLine);
                blockMovePreviewLine_ = blockCanvasScene_->addLine(QLineF(left, y, right, y), guidePen);
                blockMovePreviewLine_->setZValue(10000.0);
            } else {
                blockMovePreviewLine_->setLine(QLineF(left, y, right, y));
                blockMovePreviewLine_->show();
            }
            return;
        }
    }

    const qreal outerDropThreshold = 16.0;
    const bool dropBeforeAllBlocks = hasSceneVerticalBounds && scenePos.y() < (sceneTop - outerDropThreshold);
    const bool dropAfterAllBlocks = hasSceneVerticalBounds && scenePos.y() > (sceneBottom + outerDropThreshold);
    if (dropBeforeAllBlocks || dropAfterAllBlocks) {
        const qreal y = dropBeforeAllBlocks ? (sceneTop - 4.0) : (sceneBottom + 4.0);
        const qreal left = 10.0;
        const qreal right = qMax<qreal>(left + 120.0, blockCanvasScene_->sceneRect().right() - 10.0);
        if (blockMovePreviewLine_ == nullptr) {
            QPen guidePen(QColor(QStringLiteral("#2f6fed")));
            guidePen.setWidthF(2.0);
            guidePen.setStyle(Qt::DashLine);
            blockMovePreviewLine_ = blockCanvasScene_->addLine(QLineF(left, y, right, y), guidePen);
            blockMovePreviewLine_->setZValue(10000.0);
        } else {
            blockMovePreviewLine_->setLine(QLineF(left, y, right, y));
            blockMovePreviewLine_->show();
        }
        return;
    }

    BlockCanvasItem *targetBlockItem = resolveBlockFromItem(blockCanvasScene_->itemAt(scenePos, QTransform()));
    if (targetBlockItem != nullptr && targetBlockItem->lineNumber() == sourceLineNumber) {
        targetBlockItem = nullptr;
    }

    if (targetBlockItem == nullptr) {
        qreal bestDistance = std::numeric_limits<qreal>::max();
        for (QGraphicsItem *item : sceneItems) {
            auto *blockItem = dynamic_cast<BlockCanvasItem *>(item);
            if (blockItem == nullptr || blockItem->lineNumber() == sourceLineNumber) {
                continue;
            }
            const QRectF blockRect = blockItem->sceneBoundingRect();
            const qreal topDistance = qAbs(blockRect.top() - scenePos.y());
            const qreal bottomDistance = qAbs(blockRect.bottom() - scenePos.y());
            const qreal distance = qMin(topDistance, bottomDistance);
            if (distance < bestDistance) {
                bestDistance = distance;
                targetBlockItem = blockItem;
            }
        }
    }

    if (targetBlockItem == nullptr) {
        clearBlockMovePreview();
        return;
    }

    const QRectF targetRect = targetBlockItem->sceneBoundingRect();
    const bool insertAfterTarget = scenePos.y() > targetRect.center().y();
    const qreal y = insertAfterTarget ? (targetRect.bottom() + 4.0) : (targetRect.top() - 4.0);
    const qreal left = 10.0;
    const qreal right = qMax<qreal>(left + 120.0, blockCanvasScene_->sceneRect().right() - 10.0);

    if (blockMovePreviewLine_ == nullptr) {
        QPen guidePen(QColor(QStringLiteral("#2f6fed")));
        guidePen.setWidthF(2.0);
        guidePen.setStyle(Qt::DashLine);
        blockMovePreviewLine_ = blockCanvasScene_->addLine(QLineF(left, y, right, y), guidePen);
        blockMovePreviewLine_->setZValue(10000.0);
    } else {
        blockMovePreviewLine_->setLine(QLineF(left, y, right, y));
        blockMovePreviewLine_->show();
    }
}

void TextEditorTab::clearBlockMovePreview()
{
    if (blockMovePreviewLine_ != nullptr) {
        blockMovePreviewLine_->hide();
    }
}

int TextEditorTab::resolveEndHintContainerStartLineAtScenePos(const QPointF &scenePos) const
{
    if (blockCanvasScene_ == nullptr) {
        return 0;
    }

    auto resolveFromItem = [](QGraphicsItem *item) -> int {
        while (item != nullptr) {
            const QVariant hintValue = item->data(kBlockEndHintContainerLineDataRole);
            bool ok = false;
            const int lineNumber = hintValue.toInt(&ok);
            if (ok && lineNumber > 0) {
                return lineNumber;
            }
            item = item->parentItem();
        }
        return 0;
    };

    const int directLine = resolveFromItem(blockCanvasScene_->itemAt(scenePos, QTransform()));
    if (directLine > 0) {
        return directLine;
    }

    auto pointToSegmentDistance = [](const QPointF &point, const QLineF &segment) {
        const QPointF p1 = segment.p1();
        const QPointF p2 = segment.p2();
        const qreal dx = p2.x() - p1.x();
        const qreal dy = p2.y() - p1.y();
        const qreal lengthSquared = (dx * dx) + (dy * dy);
        if (lengthSquared <= std::numeric_limits<qreal>::epsilon()) {
            return QLineF(point, p1).length();
        }

        const qreal t = qBound<qreal>(0.0,
                                      ((point.x() - p1.x()) * dx + (point.y() - p1.y()) * dy) / lengthSquared,
                                      1.0);
        const QPointF projection(p1.x() + (t * dx), p1.y() + (t * dy));
        return QLineF(point, projection).length();
    };

    constexpr qreal kLineHitThreshold = 12.0;
    int bestLineNumber = 0;
    qreal bestDistance = std::numeric_limits<qreal>::max();

    for (QGraphicsLineItem *guideItem : blockContainerBoundaryGuideItems_) {
        if (guideItem == nullptr || !guideItem->isVisible()) {
            continue;
        }
        bool ok = false;
        const int lineNumber = guideItem->data(kBlockEndHintContainerLineDataRole).toInt(&ok);
        if (!ok || lineNumber <= 0) {
            continue;
        }

        const QLineF sceneLine = QLineF(guideItem->mapToScene(guideItem->line().p1()),
                                        guideItem->mapToScene(guideItem->line().p2()));
        const qreal distance = pointToSegmentDistance(scenePos, sceneLine);
        if (distance <= kLineHitThreshold && distance < bestDistance) {
            bestDistance = distance;
            bestLineNumber = lineNumber;
        }
    }

    return bestLineNumber;
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
    if (tearingDown_) {
        blockDetailsMode_ = BlockDetailsMode::None;
        blockDetailsSelectedLineNumber_ = 0;
        blockDetailsSelectedKind_.clear();
        return;
    }
    blockDetailsMode_ = BlockDetailsMode::None;
    blockDetailsSelectedLineNumber_ = 0;
    blockDetailsSelectedKind_.clear();
    blockDetailsBaseStatusText_.clear();
    if (blockDetailsEditPanel_ != nullptr) {
        blockDetailsEditPanel_->setVisible(false);
    }
    blockDetailsPopulating_ = true;
    if (blockDetailsStatusLabel_ != nullptr) {
        blockDetailsStatusLabel_->setStyleSheet(QString());
        blockDetailsStatusLabel_->setText(tr("Select a block in the canvas to edit its parameters."));
    }
    if (blockDetailsIdEdit_ != nullptr) {
        blockDetailsIdEdit_->clear();
        blockDetailsIdEdit_->setEnabled(false);
        blockDetailsIdEdit_->setVisible(true);
    }
    if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
        blockDetailsAdditionalPositionalEdit_->clear();
        blockDetailsAdditionalPositionalEdit_->setEnabled(false);
        blockDetailsAdditionalPositionalEdit_->setVisible(true);
    }
    if (blockDetailsSecondaryFieldStack_ != nullptr) {
        blockDetailsSecondaryFieldStack_->setVisible(true);
        if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
            blockDetailsSecondaryFieldStack_->setCurrentWidget(blockDetailsAdditionalPositionalEdit_);
        }
    }
    if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
        readingsTagEditor->setTokens({});
    }
    if (blockDetailsCommentEdit_ != nullptr) {
        blockDetailsCommentEdit_->clear();
        blockDetailsCommentEdit_->setEnabled(false);
        blockDetailsCommentEdit_->setVisible(true);
    }
    if (blockDetailsPrimaryFieldLabel_ != nullptr) {
        blockDetailsPrimaryFieldLabel_->setText(tr("ID"));
        blockDetailsPrimaryFieldLabel_->setVisible(true);
    }
    if (blockDetailsSecondaryFieldLabel_ != nullptr) {
        blockDetailsSecondaryFieldLabel_->setText(tr("Extra Arguments (Advanced)"));
        blockDetailsSecondaryFieldLabel_->setVisible(true);
    }
    if (blockDetailsCommentFieldLabel_ != nullptr) {
        blockDetailsCommentFieldLabel_->setText(tr("Comment"));
        blockDetailsCommentFieldLabel_->setVisible(true);
    }
    if (blockDetailsOptionsTable_ != nullptr) {
        blockDetailsOptionsTable_->setRowCount(0);
        blockDetailsOptionsTable_->setEnabled(false);
        blockDetailsOptionsTable_->setVisible(true);
    }
    if (blockDetailsOptionArgsLabel_ != nullptr) {
        blockDetailsOptionArgsLabel_->setVisible(false);
    }
    if (blockDetailsOptionArgsPanel_ != nullptr) {
        blockDetailsOptionArgsPanel_->setVisible(false);
    }
    if (blockDetailsAddOptionButton_ != nullptr) {
        blockDetailsAddOptionButton_->setEnabled(false);
        blockDetailsAddOptionButton_->setVisible(true);
    }
    if (blockDetailsRemoveOptionButton_ != nullptr) {
        blockDetailsRemoveOptionButton_->setEnabled(false);
        blockDetailsRemoveOptionButton_->setVisible(true);
    }
    if (blockDetailsApplyButton_ != nullptr) {
        blockDetailsApplyButton_->setEnabled(false);
    }
    if (blockDetailsLegacyConfigureButton_ != nullptr) {
        blockDetailsLegacyConfigureButton_->setEnabled(false);
        blockDetailsLegacyConfigureButton_->setVisible(false);
    }
    if (blockDetailsHelpBrowser_ != nullptr) {
        blockDetailsHelpBrowser_->setHtml(tr("<p>Select a block parameter to see contextual help.</p>"));
    }
    refreshBlockDetailsOptionArgumentEditors();
    blockDetailsPopulating_ = false;
}

void TextEditorTab::showBlockDetailsForToolboxCommand(const QString &commandToken)
{
    if (tearingDown_) {
        return;
    }
    const QString normalizedCommand = normalizeDirective(commandToken);
    if (normalizedCommand.isEmpty()) {
        return;
    }

    if (blockCanvasScene_ != nullptr) {
        const QSignalBlocker signalBlocker(blockCanvasScene_);
        blockCanvasScene_->clearSelection();
    }

    blockDetailsPopulating_ = true;
    blockDetailsMode_ = BlockDetailsMode::Unsupported;
    blockDetailsSelectedLineNumber_ = 0;
    blockDetailsSelectedKind_ = normalizedCommand;
    blockDetailsBaseStatusText_ = tr("Command: %1").arg(normalizedCommand);
    if (blockDetailsEditPanel_ != nullptr) {
        blockDetailsEditPanel_->setVisible(false);
    }

    if (blockDetailsStatusLabel_ != nullptr) {
        blockDetailsStatusLabel_->setStyleSheet(QString());
        blockDetailsStatusLabel_->setText(blockDetailsBaseStatusText_);
    }
    if (blockDetailsPrimaryFieldLabel_ != nullptr) {
        blockDetailsPrimaryFieldLabel_->setVisible(false);
    }
    if (blockDetailsSecondaryFieldLabel_ != nullptr) {
        blockDetailsSecondaryFieldLabel_->setVisible(false);
    }
    if (blockDetailsCommentFieldLabel_ != nullptr) {
        blockDetailsCommentFieldLabel_->setVisible(false);
    }
    if (blockDetailsCommentFieldLabel_ != nullptr) {
        blockDetailsCommentFieldLabel_->setVisible(false);
    }
    if (blockDetailsIdEdit_ != nullptr) {
        blockDetailsIdEdit_->clear();
        blockDetailsIdEdit_->setEnabled(false);
        blockDetailsIdEdit_->setVisible(false);
    }
    if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
        blockDetailsAdditionalPositionalEdit_->clear();
        blockDetailsAdditionalPositionalEdit_->setEnabled(false);
        blockDetailsAdditionalPositionalEdit_->setVisible(false);
    }
    if (blockDetailsSecondaryFieldStack_ != nullptr) {
        blockDetailsSecondaryFieldStack_->setVisible(false);
        if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
            blockDetailsSecondaryFieldStack_->setCurrentWidget(blockDetailsAdditionalPositionalEdit_);
        }
    }
    if (blockDetailsCommentEdit_ != nullptr) {
        blockDetailsCommentEdit_->clear();
        blockDetailsCommentEdit_->setEnabled(false);
        blockDetailsCommentEdit_->setVisible(false);
    }
    if (blockDetailsCommentEdit_ != nullptr) {
        blockDetailsCommentEdit_->clear();
        blockDetailsCommentEdit_->setEnabled(false);
        blockDetailsCommentEdit_->setVisible(false);
    }
    if (blockDetailsOptionsTable_ != nullptr) {
        blockDetailsOptionsTable_->clearSelection();
        blockDetailsOptionsTable_->setRowCount(0);
        blockDetailsOptionsTable_->setEnabled(false);
        blockDetailsOptionsTable_->setVisible(false);
    }
    if (blockDetailsAddOptionButton_ != nullptr) {
        blockDetailsAddOptionButton_->setEnabled(false);
        blockDetailsAddOptionButton_->setVisible(false);
    }
    if (blockDetailsRemoveOptionButton_ != nullptr) {
        blockDetailsRemoveOptionButton_->setEnabled(false);
        blockDetailsRemoveOptionButton_->setVisible(false);
    }
    if (blockDetailsOptionArgsLabel_ != nullptr) {
        blockDetailsOptionArgsLabel_->setVisible(false);
    }
    if (blockDetailsOptionArgsPanel_ != nullptr) {
        blockDetailsOptionArgsPanel_->setVisible(false);
    }
    if (blockDetailsLegacyConfigureButton_ != nullptr) {
        blockDetailsLegacyConfigureButton_->setVisible(false);
        blockDetailsLegacyConfigureButton_->setEnabled(false);
    }
    if (blockDetailsApplyButton_ != nullptr) {
        blockDetailsApplyButton_->setEnabled(false);
    }
    if (blockDetailsHelpBrowser_ != nullptr) {
        const TherionHelpEntry entry = helpEntries_.value(normalizedCommand);
        blockDetailsHelpBrowser_->setHtml(renderHelpSummaryHtml(normalizedCommand, entry));
    }
    refreshBlockDetailsOptionArgumentEditors();
    blockDetailsPopulating_ = false;
}

void TextEditorTab::selectBlockInCanvasAndDetails(int lineNumber)
{
    if (tearingDown_) {
        return;
    }
    if (lineNumber <= 0 || blockCanvasScene_ == nullptr) {
        clearBlockDetailsPane();
        return;
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

    const QList<QGraphicsItem *> sceneItems = blockCanvasScene_->items();
    for (QGraphicsItem *item : sceneItems) {
        auto *blockItem = resolveBlockFromItem(item);
        if (blockItem == nullptr) {
            continue;
        }
        if (blockItem->lineNumber() == lineNumber) {
            blockCanvasScene_->clearSelection();
            blockItem->setSelected(true);
            blockCanvasScene_->setFocusItem(blockItem);
            if (blockCanvasView_ != nullptr) {
                blockCanvasView_->centerOn(blockItem);
            }
            refreshBlockDetailsSelectionFromScene();
            return;
        }
    }
    clearBlockDetailsPane();
}

void TextEditorTab::refreshBlockDetailsSelectionFromScene()
{
    if (tearingDown_) {
        return;
    }
    if (blockCanvasScene_ == nullptr) {
        clearBlockDetailsPane();
        return;
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
        clearBlockDetailsPane();
        return;
    }

    if (!loadBlockDetailsForSelection(selectedBlock->kind(), selectedBlock->lineNumber())) {
        blockDetailsSelectedLineNumber_ = selectedBlock->lineNumber();
        blockDetailsSelectedKind_ = normalizeDirective(selectedBlock->kind());
    }
}

bool TextEditorTab::loadBlockDetailsForSelection(const QString &kind, int lineNumber)
{
    if (tearingDown_) {
        return false;
    }
    if (lineNumber <= 0 || editor_ == nullptr) {
        clearBlockDetailsPane();
        return false;
    }

    QStringList lines = editor_->toPlainText().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    if (lineNumber > lines.size()) {
        clearBlockDetailsPane();
        return false;
    }

    const QString normalizedKind = normalizeDirective(kind);
    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineNumber - 1), lineNumber);
    if (parsedLine.tokens.isEmpty()) {
        clearBlockDetailsPane();
        return false;
    }
    const QString inlineComment = parsedLine.commentStart >= 0 ? parsedLine.commentText.trimmed() : QString();
    if (parsedLine.commentStart >= 0 && parsedLine.commentStart < lines.at(lineNumber - 1).size()) {
        blockDetailsCommentMarker_ = lines.at(lineNumber - 1).at(parsedLine.commentStart);
    } else {
        blockDetailsCommentMarker_ = QLatin1Char('#');
    }

    blockDetailsSelectedLineNumber_ = lineNumber;
    blockDetailsSelectedKind_ = normalizedKind;
    if (blockDetailsEditPanel_ != nullptr) {
        blockDetailsEditPanel_->setVisible(true);
    }

    blockDetailsPopulating_ = true;

    const bool supported = supportsDetailsPaneForKind(normalizedKind);
    const bool hasCatalogOptions = !commandOptionTokens_.value(normalizedKind).isEmpty();
    const bool structuredOptionsMode = isContainerBlockDirective(normalizedKind) || hasCatalogOptions;
    const bool simpleValueMode = !structuredOptionsMode && normalizedKind != QStringLiteral("data");
    const bool dataHeaderMode = normalizedKind == QStringLiteral("data");
    if (!supported) {
        blockDetailsMode_ = BlockDetailsMode::Unsupported;
    } else if (structuredOptionsMode) {
        blockDetailsMode_ = BlockDetailsMode::StructuredOptions;
    } else if (simpleValueMode) {
        blockDetailsMode_ = BlockDetailsMode::SimpleValue;
    } else if (dataHeaderMode) {
        blockDetailsMode_ = BlockDetailsMode::DataHeader;
    } else {
        blockDetailsMode_ = BlockDetailsMode::Unsupported;
    }

    blockDetailsBaseStatusText_ = tr("Command: %1").arg(normalizedKind);
    if (blockDetailsStatusLabel_ != nullptr) {
        blockDetailsStatusLabel_->setStyleSheet(QString());
        blockDetailsStatusLabel_->setText(blockDetailsBaseStatusText_);
    }

    if (!supported) {
        if (blockDetailsEditPanel_ != nullptr) {
            blockDetailsEditPanel_->setVisible(false);
        }
        if (blockDetailsPrimaryFieldLabel_ != nullptr) {
            blockDetailsPrimaryFieldLabel_->setVisible(false);
        }
        if (blockDetailsSecondaryFieldLabel_ != nullptr) {
            blockDetailsSecondaryFieldLabel_->setVisible(false);
        }
        if (blockDetailsCommentFieldLabel_ != nullptr) {
            blockDetailsCommentFieldLabel_->setVisible(false);
        }
        if (blockDetailsIdEdit_ != nullptr) {
            blockDetailsIdEdit_->clear();
            blockDetailsIdEdit_->setEnabled(false);
            blockDetailsIdEdit_->setVisible(false);
        }
        if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
            blockDetailsAdditionalPositionalEdit_->clear();
            blockDetailsAdditionalPositionalEdit_->setEnabled(false);
            blockDetailsAdditionalPositionalEdit_->setVisible(false);
        }
        if (blockDetailsSecondaryFieldStack_ != nullptr) {
            blockDetailsSecondaryFieldStack_->setVisible(false);
            if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
                blockDetailsSecondaryFieldStack_->setCurrentWidget(blockDetailsAdditionalPositionalEdit_);
            }
        }
        if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
            readingsTagEditor->setTokens({});
        }
        if (blockDetailsCommentEdit_ != nullptr) {
            blockDetailsCommentEdit_->clear();
            blockDetailsCommentEdit_->setEnabled(false);
            blockDetailsCommentEdit_->setVisible(false);
        }
        if (blockDetailsOptionsLabel_ != nullptr) {
            blockDetailsOptionsLabel_->setVisible(false);
        }
        if (blockDetailsOptionsTable_ != nullptr) {
            blockDetailsOptionsTable_->clearSelection();
            blockDetailsOptionsTable_->setEnabled(false);
            blockDetailsOptionsTable_->setRowCount(0);
            blockDetailsOptionsTable_->setVisible(false);
        }
        if (blockDetailsAddOptionButton_ != nullptr) {
            blockDetailsAddOptionButton_->setEnabled(false);
            blockDetailsAddOptionButton_->setVisible(false);
        }
        if (blockDetailsRemoveOptionButton_ != nullptr) {
            blockDetailsRemoveOptionButton_->setEnabled(false);
            blockDetailsRemoveOptionButton_->setVisible(false);
        }
        if (blockDetailsLegacyConfigureButton_ != nullptr) {
            blockDetailsLegacyConfigureButton_->setEnabled(false);
            blockDetailsLegacyConfigureButton_->setVisible(false);
        }
        if (blockDetailsOptionArgsLabel_ != nullptr) {
            blockDetailsOptionArgsLabel_->setVisible(false);
        }
        if (blockDetailsOptionArgsPanel_ != nullptr) {
            blockDetailsOptionArgsPanel_->setVisible(false);
        }
        if (blockDetailsApplyButton_ != nullptr) {
            blockDetailsApplyButton_->setEnabled(false);
        }
        blockDetailsPopulating_ = false;
        refreshBlockDetailsOptionArgumentEditors();
        updateBlockDetailsHelpForCurrentFocus();
        refreshBlockDetailsApplyState();
        return true;
    }

    if (blockDetailsPrimaryFieldLabel_ != nullptr) {
        blockDetailsPrimaryFieldLabel_->setVisible(true);
    }
    if (blockDetailsSecondaryFieldLabel_ != nullptr) {
        blockDetailsSecondaryFieldLabel_->setVisible(true);
    }
    if (blockDetailsCommentFieldLabel_ != nullptr) {
        blockDetailsCommentFieldLabel_->setText(tr("Comment"));
        blockDetailsCommentFieldLabel_->setVisible(true);
    }
    if (blockDetailsIdEdit_ != nullptr) {
        blockDetailsIdEdit_->setVisible(true);
        installLineEditCompleter(blockDetailsIdEdit_, {});
    }
    if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
        blockDetailsAdditionalPositionalEdit_->setVisible(true);
        installLineEditCompleter(blockDetailsAdditionalPositionalEdit_, {});
    }
    if (blockDetailsSecondaryFieldStack_ != nullptr) {
        blockDetailsSecondaryFieldStack_->setVisible(true);
        if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
            blockDetailsSecondaryFieldStack_->setCurrentWidget(blockDetailsAdditionalPositionalEdit_);
        }
    }
    if (blockDetailsCommentEdit_ != nullptr) {
        blockDetailsCommentEdit_->setVisible(true);
        blockDetailsCommentEdit_->setEnabled(true);
        blockDetailsCommentEdit_->setPlaceholderText(tr("optional"));
        blockDetailsCommentEdit_->setText(inlineComment);
    }
    if (blockDetailsOptionsLabel_ != nullptr) {
        blockDetailsOptionsLabel_->setVisible(false);
    }
    if (blockDetailsOptionsTable_ != nullptr) {
        blockDetailsOptionsTable_->setEnabled(false);
        blockDetailsOptionsTable_->setRowCount(0);
        blockDetailsOptionsTable_->setVisible(false);
    }
    if (blockDetailsOptionArgsLabel_ != nullptr) {
        blockDetailsOptionArgsLabel_->setVisible(false);
    }
    if (blockDetailsOptionArgsPanel_ != nullptr) {
        blockDetailsOptionArgsPanel_->setVisible(false);
    }
    if (blockDetailsAddOptionButton_ != nullptr) {
        blockDetailsAddOptionButton_->setEnabled(false);
        blockDetailsAddOptionButton_->setVisible(false);
    }
    if (blockDetailsRemoveOptionButton_ != nullptr) {
        blockDetailsRemoveOptionButton_->setEnabled(false);
        blockDetailsRemoveOptionButton_->setVisible(false);
    }
    if (blockDetailsApplyButton_ != nullptr) {
        blockDetailsApplyButton_->setEnabled(false);
    }

    if (blockDetailsLegacyConfigureButton_ != nullptr) {
        if (!supported) {
            blockDetailsLegacyConfigureButton_->setText(tr("Legacy Configure..."));
            blockDetailsLegacyConfigureButton_->setVisible(true);
            blockDetailsLegacyConfigureButton_->setEnabled(true);
        } else if (dataHeaderMode) {
            blockDetailsLegacyConfigureButton_->setText(tr("Edit Data Rows..."));
            blockDetailsLegacyConfigureButton_->setVisible(true);
            blockDetailsLegacyConfigureButton_->setEnabled(true);
        } else {
            blockDetailsLegacyConfigureButton_->setVisible(false);
            blockDetailsLegacyConfigureButton_->setEnabled(false);
        }
    }

    if (!supported) {
        if (blockDetailsIdEdit_ != nullptr) {
            blockDetailsIdEdit_->clear();
            blockDetailsIdEdit_->setEnabled(false);
            blockDetailsIdEdit_->setPlaceholderText(QString());
        }
        if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
            blockDetailsAdditionalPositionalEdit_->clear();
            blockDetailsAdditionalPositionalEdit_->setEnabled(false);
        }
        if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
            readingsTagEditor->setTokens({});
        }
        if (blockDetailsHelpBrowser_ != nullptr) {
            const TherionHelpEntry entry = helpEntries_.value(normalizedKind);
            QString html = tr("<p>This block currently uses legacy dialog-based configuration.</p>");
            html += renderHelpHtml(normalizedKind, entry, false);
            blockDetailsHelpBrowser_->setHtml(html);
        }
        blockDetailsPopulating_ = false;
        return false;
    }

    if (structuredOptionsMode) {
        const bool explicitIdMode = commandSupportsInlineIdField(normalizedKind);
        const bool requiresId = commandHasRequiredIdArgument(normalizedKind);

        QString currentId;
        int optionsStartIndex = 1;
        if (parsedLine.tokens.size() > 1
            && !parsedLine.tokens.at(1).trimmed().startsWith(QLatin1Char('-'))) {
            currentId = parsedLine.tokens.at(1).trimmed();
            optionsStartIndex = 2;
        }

        QStringList extraPositionalTokens;
        struct OptionEntry
        {
            QString key;
            QString value;
        };
        QVector<OptionEntry> optionEntries;
        for (int index = optionsStartIndex; index < parsedLine.tokens.size();) {
            const QString token = parsedLine.tokens.at(index).trimmed();
            if (configureTokenStartsNewOption(token)) {
                const int nextOptionIndex = nextConfigureOptionIndex(parsedLine.tokens, index);
                const QStringList rawOptionValues =
                    parsedLine.tokens.mid(index + 1, nextOptionIndex - index - 1);
                QString optionDisplayValue = rawOptionValues.join(QLatin1Char(' '));
                const int fixedArity = commandOptionFixedArityByKey_.value(
                    commandOptionValueKey(normalizedKind, token.toLower().trimmed()), -1);
                if (fixedArity > 1 && !rawOptionValues.isEmpty()) {
                    QStringList serializedValues;
                    serializedValues.reserve(rawOptionValues.size());
                    for (const QString &rawOptionValue : rawOptionValues) {
                        serializedValues.append(serializeTherionArgumentToken(rawOptionValue));
                    }
                    optionDisplayValue = serializedValues.join(QLatin1Char(' '));
                }
                optionEntries.append(OptionEntry{
                    token,
                    optionDisplayValue,
                });
                index = nextOptionIndex;
                continue;
            }
            extraPositionalTokens.append(token);
            ++index;
        }

        if (blockDetailsPrimaryFieldLabel_ != nullptr) {
            if (explicitIdMode) {
                blockDetailsPrimaryFieldLabel_->setText(tr("ID"));
            } else {
                const QStringList argumentSignatures = commandArgumentSignaturesFor(normalizedKind);
                if (!argumentSignatures.isEmpty()) {
                    blockDetailsPrimaryFieldLabel_->setText(argumentLabelFromSignature(argumentSignatures.first()));
                } else {
                    blockDetailsPrimaryFieldLabel_->setText(tr("Value"));
                }
            }
        }
        if (blockDetailsIdEdit_ != nullptr) {
            blockDetailsIdEdit_->setEnabled(true);
            if (explicitIdMode) {
                blockDetailsIdEdit_->setPlaceholderText(requiresId ? tr("required") : tr("optional"));
            } else {
                blockDetailsIdEdit_->setPlaceholderText(tr("optional"));
            }
            blockDetailsIdEdit_->setText(currentId);
        }
        const bool showExtraArguments = !extraPositionalTokens.isEmpty();
        if (blockDetailsSecondaryFieldLabel_ != nullptr) {
            blockDetailsSecondaryFieldLabel_->setText(tr("Extra Arguments (Advanced)"));
            blockDetailsSecondaryFieldLabel_->setVisible(showExtraArguments);
        }
        if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
            blockDetailsAdditionalPositionalEdit_->setEnabled(showExtraArguments);
            blockDetailsAdditionalPositionalEdit_->setVisible(showExtraArguments);
            blockDetailsAdditionalPositionalEdit_->setText(showExtraArguments
                                                               ? extraPositionalTokens.join(QLatin1Char(' '))
                                                               : QString());
        }
        if (blockDetailsSecondaryFieldStack_ != nullptr) {
            blockDetailsSecondaryFieldStack_->setVisible(showExtraArguments);
            if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
                blockDetailsSecondaryFieldStack_->setCurrentWidget(blockDetailsAdditionalPositionalEdit_);
            }
        }
        if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
            readingsTagEditor->setTokens({});
        }
        const bool showOptionsSection = hasCatalogOptions || !optionEntries.isEmpty();
        if (blockDetailsOptionsLabel_ != nullptr) {
            blockDetailsOptionsLabel_->setVisible(showOptionsSection);
        }
        if (blockDetailsOptionsTable_ != nullptr) {
            blockDetailsOptionsTable_->setVisible(showOptionsSection);
            blockDetailsOptionsTable_->setEnabled(showOptionsSection);
            blockDetailsOptionsTable_->setRowCount(showOptionsSection ? optionEntries.size() : 0);
            if (showOptionsSection) {
                for (int row = 0; row < optionEntries.size(); ++row) {
                    blockDetailsOptionsTable_->setItem(row, 0, new QTableWidgetItem(optionEntries.at(row).key));
                    blockDetailsOptionsTable_->setItem(row, 1, new QTableWidgetItem(optionEntries.at(row).value));
                }
            }
            if (showOptionsSection && optionEntries.size() > 0) {
                blockDetailsOptionsTable_->setCurrentCell(0, 0);
            }
        }
        if (blockDetailsAddOptionButton_ != nullptr) {
            blockDetailsAddOptionButton_->setVisible(showOptionsSection);
            blockDetailsAddOptionButton_->setEnabled(showOptionsSection);
        }
        if (blockDetailsRemoveOptionButton_ != nullptr) {
            blockDetailsRemoveOptionButton_->setVisible(showOptionsSection);
            blockDetailsRemoveOptionButton_->setEnabled(showOptionsSection && !optionEntries.isEmpty());
        }
    } else if (simpleValueMode) {
        const TherionHelpEntry helpEntry = helpEntries_.value(normalizedKind);
        QStringList argumentSignatures;
        for (const QString &argumentLine : helpEntry.arguments) {
            const QString signature = argumentSignatureFromHelpLine(argumentLine);
            if (!signature.isEmpty()) {
                argumentSignatures.append(signature);
            }
        }
        const bool hasSecondaryArgument = argumentSignatures.size() > 1;

        QString currentValue = parsedLine.tokens.size() > 1 ? parsedLine.tokens.at(1) : QString();
        QString secondaryValue;
        if (hasSecondaryArgument && parsedLine.tokens.size() > 2) {
            secondaryValue = parsedLine.tokens.mid(2).join(QLatin1Char(' '));
        } else if (!hasSecondaryArgument) {
            currentValue = parsedLine.tokens.size() > 1
                ? parsedLine.tokens.mid(1).join(QLatin1Char(' '))
                : QString();
        }

        if (blockDetailsPrimaryFieldLabel_ != nullptr) {
            if (commandPrimaryValueIsPerson_.value(normalizedKind, false)) {
                blockDetailsPrimaryFieldLabel_->setText(tr("Person"));
            } else if (!argumentSignatures.isEmpty()) {
                blockDetailsPrimaryFieldLabel_->setText(argumentLabelFromSignature(argumentSignatures.first()));
            } else {
                blockDetailsPrimaryFieldLabel_->setText(tr("Value"));
            }
        }
        if (blockDetailsSecondaryFieldLabel_ != nullptr) {
            if (hasSecondaryArgument) {
                if (argumentSignatures.size() > 1) {
                    blockDetailsSecondaryFieldLabel_->setText(argumentLabelFromSignature(argumentSignatures.at(1)));
                } else {
                    blockDetailsSecondaryFieldLabel_->setText(tr("Value 2"));
                }
                blockDetailsSecondaryFieldLabel_->setVisible(true);
            } else {
                blockDetailsSecondaryFieldLabel_->setVisible(false);
            }
        }
        if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
            if (hasSecondaryArgument) {
                blockDetailsAdditionalPositionalEdit_->setEnabled(true);
                blockDetailsAdditionalPositionalEdit_->setVisible(true);
                blockDetailsAdditionalPositionalEdit_->setPlaceholderText(tr("optional"));
                blockDetailsAdditionalPositionalEdit_->setText(secondaryValue);
            } else {
                blockDetailsAdditionalPositionalEdit_->clear();
                blockDetailsAdditionalPositionalEdit_->setEnabled(false);
                blockDetailsAdditionalPositionalEdit_->setVisible(false);
            }
        }
        if (blockDetailsSecondaryFieldStack_ != nullptr) {
            blockDetailsSecondaryFieldStack_->setVisible(hasSecondaryArgument);
            if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
                blockDetailsSecondaryFieldStack_->setCurrentWidget(blockDetailsAdditionalPositionalEdit_);
            }
        }
        if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
            readingsTagEditor->setTokens({});
        }
        if (blockDetailsIdEdit_ != nullptr) {
            blockDetailsIdEdit_->setEnabled(true);
            blockDetailsIdEdit_->setPlaceholderText(tr("required"));
            blockDetailsIdEdit_->setText(currentValue);
        }
        if (blockDetailsOptionsTable_ != nullptr) {
            blockDetailsOptionsTable_->setVisible(false);
        }
        if (blockDetailsOptionsLabel_ != nullptr) {
            blockDetailsOptionsLabel_->setVisible(false);
        }
        if (blockDetailsAddOptionButton_ != nullptr) {
            blockDetailsAddOptionButton_->setVisible(false);
        }
        if (blockDetailsRemoveOptionButton_ != nullptr) {
            blockDetailsRemoveOptionButton_->setVisible(false);
        }
    } else if (dataHeaderMode) {
        const DataHeaderComponents components = parseDataHeaderComponents(parsedLine.tokens);
        const QStringList styleSuggestions = commandArgumentValueTokens_.value(commandArgumentValueKey(QStringLiteral("data"), 0));
        const QStringList readingSuggestions = commandArgumentValueTokens_.value(commandArgumentValueKey(QStringLiteral("data"), 1));
        if (blockDetailsPrimaryFieldLabel_ != nullptr) {
            blockDetailsPrimaryFieldLabel_->setText(tr("Style"));
        }
        if (blockDetailsSecondaryFieldLabel_ != nullptr) {
            blockDetailsSecondaryFieldLabel_->setText(tr("Readings Order"));
            blockDetailsSecondaryFieldLabel_->setVisible(true);
        }
        if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
            blockDetailsAdditionalPositionalEdit_->setEnabled(true);
            blockDetailsAdditionalPositionalEdit_->setVisible(false);
            blockDetailsAdditionalPositionalEdit_->setText(components.readingsOrder);
        }
        if (blockDetailsSecondaryFieldStack_ != nullptr) {
            blockDetailsSecondaryFieldStack_->setVisible(true);
            if (blockDetailsReadingsTagEditor_ != nullptr) {
                blockDetailsSecondaryFieldStack_->setCurrentWidget(blockDetailsReadingsTagEditor_);
            }
        }
        if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
            readingsTagEditor->setPlaceholderText(tr("Type token and press Enter/Space"));
            readingsTagEditor->setSuggestions(readingSuggestions);
            readingsTagEditor->setTokens(TherionDocumentParser::tokenizeLine(components.readingsOrder));
        }
        if (blockDetailsIdEdit_ != nullptr) {
            blockDetailsIdEdit_->setEnabled(true);
            blockDetailsIdEdit_->setPlaceholderText(tr("required"));
            blockDetailsIdEdit_->setText(components.style);
            installLineEditCompleter(blockDetailsIdEdit_, styleSuggestions);
        }
        if (blockDetailsOptionsTable_ != nullptr) {
            blockDetailsOptionsTable_->setVisible(false);
        }
        if (blockDetailsOptionsLabel_ != nullptr) {
            blockDetailsOptionsLabel_->setVisible(false);
        }
        if (blockDetailsAddOptionButton_ != nullptr) {
            blockDetailsAddOptionButton_->setVisible(false);
        }
        if (blockDetailsRemoveOptionButton_ != nullptr) {
            blockDetailsRemoveOptionButton_->setVisible(false);
        }
    }

    blockDetailsPopulating_ = false;
    refreshBlockDetailsOptionArgumentEditors();
    updateBlockDetailsHelpForCurrentFocus();
    refreshBlockDetailsApplyState();
    return true;
}

void TextEditorTab::refreshBlockDetailsOptionArgumentEditors()
{
    if (blockDetailsOptionArgsLabel_ == nullptr
        || blockDetailsOptionArgsPanel_ == nullptr
        || blockDetailsOptionArgsFormLayout_ == nullptr
        || blockDetailsOptionsTable_ == nullptr) {
        return;
    }

    auto clearEditors = [this]() {
        blockDetailsOptionArgsSyncing_ = true;
        blockDetailsOptionArgEditors_.clear();
        while (blockDetailsOptionArgsFormLayout_->rowCount() > 0) {
            QFormLayout::TakeRowResult row = blockDetailsOptionArgsFormLayout_->takeRow(0);
            if (row.labelItem != nullptr) {
                if (QWidget *widget = row.labelItem->widget(); widget != nullptr) {
                    widget->deleteLater();
                }
                delete row.labelItem;
            }
            if (row.fieldItem != nullptr) {
                if (QWidget *widget = row.fieldItem->widget(); widget != nullptr) {
                    widget->deleteLater();
                }
                delete row.fieldItem;
            }
        }
        blockDetailsOptionArgsSyncing_ = false;
    };

    const bool supportedMode = blockDetailsMode_ == BlockDetailsMode::StructuredOptions
        && blockDetailsOptionsTable_->isVisible()
        && blockDetailsOptionsTable_->isEnabled();
    if (!supportedMode) {
        clearEditors();
        blockDetailsOptionArgsLabel_->setVisible(false);
        blockDetailsOptionArgsPanel_->setVisible(false);
        return;
    }

    auto updateOptionValueCellEditability = [this]() {
        if (blockDetailsOptionsTable_ == nullptr) {
            return;
        }
        QSignalBlocker tableSignalBlocker(blockDetailsOptionsTable_);
        const QString commandToken = normalizeDirective(blockDetailsSelectedKind_);
        const QString multiValueToolTip
            = tr("Use Selected Option Parameters below to edit this multi-value option.");
        for (int optionRow = 0; optionRow < blockDetailsOptionsTable_->rowCount(); ++optionRow) {
            QTableWidgetItem *valueItem = blockDetailsOptionsTable_->item(optionRow, 1);
            if (valueItem == nullptr) {
                valueItem = new QTableWidgetItem(QString());
                blockDetailsOptionsTable_->setItem(optionRow, 1, valueItem);
            }
            const QString optionToken = (blockDetailsOptionsTable_->item(optionRow, 0) != nullptr
                                             ? blockDetailsOptionsTable_->item(optionRow, 0)->text()
                                             : QString())
                                            .trimmed()
                                            .toLower();
            const int fixedArity = commandOptionFixedArityByKey_.value(
                commandOptionValueKey(commandToken, optionToken), -1);
            Qt::ItemFlags flags = valueItem->flags();
            if (fixedArity > 1) {
                flags &= ~Qt::ItemIsEditable;
                if (valueItem->toolTip() != multiValueToolTip) {
                    valueItem->setToolTip(multiValueToolTip);
                }
            } else {
                flags |= Qt::ItemIsEditable;
                if (!valueItem->toolTip().isEmpty()) {
                    valueItem->setToolTip(QString());
                }
            }
            if (valueItem->flags() != flags) {
                valueItem->setFlags(flags);
            }
        }
    };

    updateOptionValueCellEditability();

    const int row = blockDetailsOptionsTable_->currentRow();
    if (row < 0 || row >= blockDetailsOptionsTable_->rowCount()) {
        clearEditors();
        blockDetailsOptionArgsLabel_->setVisible(false);
        blockDetailsOptionArgsPanel_->setVisible(false);
        return;
    }

    const QString commandToken = normalizeDirective(blockDetailsSelectedKind_);
    const QString optionToken = (blockDetailsOptionsTable_->item(row, 0) != nullptr
                                     ? blockDetailsOptionsTable_->item(row, 0)->text()
                                     : QString())
                                    .trimmed()
                                    .toLower();
    if (optionToken.isEmpty()) {
        clearEditors();
        blockDetailsOptionArgsLabel_->setVisible(false);
        blockDetailsOptionArgsPanel_->setVisible(false);
        return;
    }

    const QString optionKey = commandOptionValueKey(commandToken, optionToken);
    const int fixedArity = commandOptionFixedArityByKey_.value(optionKey, -1);
    if (fixedArity <= 1) {
        clearEditors();
        blockDetailsOptionArgsLabel_->setVisible(false);
        blockDetailsOptionArgsPanel_->setVisible(false);
        return;
    }

    QStringList argumentLabels = commandOptionArgumentLabelsByKey_.value(optionKey);
    while (argumentLabels.size() < fixedArity) {
        argumentLabels.append(tr("Value %1").arg(argumentLabels.size() + 1));
    }
    if (argumentLabels.size() > fixedArity) {
        argumentLabels = argumentLabels.mid(0, fixedArity);
    }

    QStringList valueTokens;
    const QString valueText = (blockDetailsOptionsTable_->item(row, 1) != nullptr
                                   ? blockDetailsOptionsTable_->item(row, 1)->text()
                                   : QString())
                                  .trimmed();
    if (!valueText.isEmpty()) {
        valueTokens = TherionDocumentParser::tokenizeLine(valueText);
        if (valueTokens.isEmpty()) {
            valueTokens.append(valueText);
        }
    }

    clearEditors();
    blockDetailsOptionArgsSyncing_ = true;
    for (int index = 0; index < fixedArity; ++index) {
        auto *edit = new QLineEdit(blockDetailsOptionArgsPanel_);
        edit->setPlaceholderText(tr("required"));
        edit->setText(index < valueTokens.size() ? valueTokens.at(index) : QString());
        blockDetailsOptionArgsFormLayout_->addRow(argumentLabels.at(index), edit);
        blockDetailsOptionArgEditors_.append(edit);

        connect(edit, &QLineEdit::textChanged, this, [this, optionKey, row](const QString &) {
            if (blockDetailsOptionArgsSyncing_
                || blockDetailsOptionsTable_ == nullptr
                || row < 0
                || row >= blockDetailsOptionsTable_->rowCount()) {
                return;
            }

            const QString selectedOptionToken = (blockDetailsOptionsTable_->item(row, 0) != nullptr
                                                     ? blockDetailsOptionsTable_->item(row, 0)->text()
                                                     : QString())
                                                    .trimmed()
                                                    .toLower();
            if (commandOptionValueKey(normalizeDirective(blockDetailsSelectedKind_), selectedOptionToken) != optionKey) {
                return;
            }

            QStringList serializedValues;
            serializedValues.reserve(blockDetailsOptionArgEditors_.size());
            for (QLineEdit *valueEdit : blockDetailsOptionArgEditors_) {
                const QString rawValue = valueEdit != nullptr ? valueEdit->text().trimmed() : QString();
                serializedValues.append(serializeTherionArgumentToken(rawValue));
            }

            QSignalBlocker blocker(blockDetailsOptionsTable_);
            blockDetailsOptionArgsSyncing_ = true;
            QTableWidgetItem *valueItem = blockDetailsOptionsTable_->item(row, 1);
            if (valueItem == nullptr) {
                valueItem = new QTableWidgetItem;
                blockDetailsOptionsTable_->setItem(row, 1, valueItem);
            }
            valueItem->setText(serializedValues.join(QLatin1Char(' ')));
            blockDetailsOptionArgsSyncing_ = false;

            refreshBlockDetailsApplyState();
            updateBlockDetailsHelpForCurrentFocus();
        });
        connect(edit, &QLineEdit::selectionChanged, this, [this]() {
            updateBlockDetailsHelpForCurrentFocus();
        });
    }
    blockDetailsOptionArgsSyncing_ = false;

    blockDetailsOptionArgsLabel_->setVisible(true);
    blockDetailsOptionArgsPanel_->setVisible(true);
}

void TextEditorTab::updateBlockDetailsHelpForCurrentFocus()
{
    if (tearingDown_) {
        return;
    }
    if (blockDetailsHelpBrowser_ == nullptr) {
        return;
    }
    if (blockDetailsSelectedKind_.isEmpty()) {
        blockDetailsHelpBrowser_->setHtml(tr("<p>Select a block parameter to see contextual help.</p>"));
        return;
    }

    const QString normalizedKind = normalizeDirective(blockDetailsSelectedKind_);
    const TherionHelpEntry commandHelpEntry = helpEntries_.value(normalizedKind);
    const QString commandHelpHtml = renderHelpHtml(normalizedKind, commandHelpEntry, false);

    auto buildIdHelpHtml = [commandHelpEntry]() {
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
        return html.join(QString());
    };

    if ((blockDetailsMode_ == BlockDetailsMode::StructuredOptions
         || blockDetailsMode_ == BlockDetailsMode::SimpleValue
         || blockDetailsMode_ == BlockDetailsMode::DataHeader)
        && blockDetailsIdEdit_ != nullptr
        && blockDetailsIdEdit_->hasFocus()) {
        const bool idSemantics = blockDetailsPrimaryFieldLabel_ != nullptr
            && blockDetailsPrimaryFieldLabel_->text().trimmed().compare(tr("ID"), Qt::CaseInsensitive) == 0;
        if (blockDetailsMode_ == BlockDetailsMode::SimpleValue) {
            blockDetailsHelpBrowser_->setHtml(commandHelpHtml);
            return;
        }
        if (blockDetailsMode_ == BlockDetailsMode::DataHeader) {
            const QString html = QStringLiteral("<p><b>Style:</b> first token after <code>data</code> (for example <code>normal</code>).</p>"
                                                "<p><b>Readings order:</b> remaining tokens that define row column order (for example <code>from to length compass clino</code>).</p>"
                                                "<p>Use `Edit Data Rows...` to modify measurement rows and directives.</p>%1")
                                     .arg(commandHelpHtml);
            blockDetailsHelpBrowser_->setHtml(html);
            return;
        }
        if (!idSemantics) {
            blockDetailsHelpBrowser_->setHtml(commandHelpHtml);
            return;
        }
        const QString idHelpHtml = buildIdHelpHtml();
        blockDetailsHelpBrowser_->setHtml(!idHelpHtml.isEmpty() ? idHelpHtml : commandHelpHtml);
        return;
    }

    if (blockDetailsMode_ == BlockDetailsMode::StructuredOptions
        && blockDetailsAdditionalPositionalEdit_ != nullptr
        && blockDetailsAdditionalPositionalEdit_->hasFocus()) {
        const QString html = QStringLiteral("<p><b>Extra arguments</b> keep unsupported positional tokens intact.</p>"
                                            "<p>Prefer explicit key/value options where available.</p>%1")
                                 .arg(commandHelpHtml);
        blockDetailsHelpBrowser_->setHtml(html);
        return;
    }
    if (blockDetailsMode_ == BlockDetailsMode::DataHeader) {
        if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_);
            readingsTagEditor != nullptr && readingsTagEditor->hasEditorFocus()) {
            const QString html = QStringLiteral("<p><b>Style:</b> first token after <code>data</code> (for example <code>normal</code>).</p>"
                                                "<p><b>Readings order:</b> tokenized column order. Add a token and confirm with Enter/Space; remove via chip <code>✕</code>.</p>"
                                                "<p>Use `Edit Data Rows...` to modify measurement rows and directives.</p>%1")
                                     .arg(commandHelpHtml);
            blockDetailsHelpBrowser_->setHtml(html);
            return;
        }
    }
    if ((blockDetailsMode_ == BlockDetailsMode::StructuredOptions
         || blockDetailsMode_ == BlockDetailsMode::SimpleValue
         || blockDetailsMode_ == BlockDetailsMode::DataHeader)
        && blockDetailsCommentEdit_ != nullptr
        && blockDetailsCommentEdit_->hasFocus()) {
        const QString markerText = QString(blockDetailsCommentMarker_);
        const QString html = QStringLiteral("<p><b>Inline comment:</b> optional end-of-line note appended as <code>%1 ...</code>.</p>%2")
                                 .arg(markerText.toHtmlEscaped(), commandHelpHtml);
        blockDetailsHelpBrowser_->setHtml(html);
        return;
    }

    blockDetailsHelpBrowser_->setHtml(commandHelpHtml);
}

bool TextEditorTab::buildUpdatedLineFromBlockDetails(QString *updatedLine, QString *validationError) const
{
    if (updatedLine == nullptr) {
        return false;
    }
    updatedLine->clear();
    if (validationError != nullptr) {
        validationError->clear();
    }

    if (blockDetailsSelectedLineNumber_ <= 0 || blockDetailsSelectedKind_.isEmpty() || editor_ == nullptr) {
        if (validationError != nullptr) {
            *validationError = tr("No block is selected.");
        }
        return false;
    }

    QStringList lines = editor_->toPlainText().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    if (blockDetailsSelectedLineNumber_ > lines.size()) {
        if (validationError != nullptr) {
            *validationError = tr("Selected line is out of range.");
        }
        return false;
    }

    const QString normalizedKind = normalizeDirective(blockDetailsSelectedKind_);
    const TherionParsedLine parsedLine =
        TherionDocumentParser::parseLine(lines.at(blockDetailsSelectedLineNumber_ - 1), blockDetailsSelectedLineNumber_);
    if (parsedLine.tokens.isEmpty()) {
        if (validationError != nullptr) {
            *validationError = tr("Selected line is empty.");
        }
        return false;
    }

    const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
    const auto match = indentPattern.match(lines.at(blockDetailsSelectedLineNumber_ - 1));
    const QString indent = match.hasMatch() ? match.captured(0) : QString();
    const QString commandToken = parsedLine.tokens.value(0).trimmed().isEmpty()
        ? normalizedKind
        : parsedLine.tokens.value(0).trimmed();

    QString result = QStringLiteral("%1%2").arg(indent, commandToken);

    if (blockDetailsMode_ == BlockDetailsMode::SimpleValue) {
        const QString updatedValue = blockDetailsIdEdit_ != nullptr ? blockDetailsIdEdit_->text().trimmed() : QString();
        if (updatedValue.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = tr("Value cannot be empty.");
            }
            return false;
        }
        if (commandPrimaryValueIsPerson_.value(normalizedKind, false)) {
            result += QStringLiteral(" ") + serializeTherionArgumentToken(updatedValue);
        } else {
            result += QStringLiteral(" ") + updatedValue;
        }
        if (blockDetailsAdditionalPositionalEdit_ != nullptr && blockDetailsAdditionalPositionalEdit_->isVisible()) {
            const QString secondaryValue = blockDetailsAdditionalPositionalEdit_->text().trimmed();
            if (!secondaryValue.isEmpty()) {
                result += QStringLiteral(" ") + secondaryValue;
            }
        }
    } else if (blockDetailsMode_ == BlockDetailsMode::DataHeader) {
        const QString updatedStyle = blockDetailsIdEdit_ != nullptr ? blockDetailsIdEdit_->text().trimmed() : QString();
        if (updatedStyle.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = tr("Style cannot be empty.");
            }
            return false;
        }
        QString updatedReadingsOrder;
        if (auto *readingsTagEditor = asTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
            updatedReadingsOrder = readingsTagEditor->tokens().join(QLatin1Char(' ')).trimmed();
        } else if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
            updatedReadingsOrder = blockDetailsAdditionalPositionalEdit_->text().trimmed();
        }
        if (updatedReadingsOrder.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = tr("Readings order cannot be empty.");
            }
            return false;
        }
        result += QStringLiteral(" ") + updatedStyle;
        result += QStringLiteral(" ") + updatedReadingsOrder;
    } else if (blockDetailsMode_ == BlockDetailsMode::StructuredOptions) {
        const bool requiresId = commandHasRequiredIdArgument(normalizedKind);
        const QString updatedId = blockDetailsIdEdit_ != nullptr ? blockDetailsIdEdit_->text().trimmed() : QString();
        if (requiresId && updatedId.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = tr("ID cannot be empty.");
            }
            return false;
        }

        QStringList serializedOptions;
        if (blockDetailsOptionsTable_ != nullptr) {
            for (int row = 0; row < blockDetailsOptionsTable_->rowCount(); ++row) {
                const QString key = (blockDetailsOptionsTable_->item(row, 0) != nullptr
                                         ? blockDetailsOptionsTable_->item(row, 0)->text()
                                         : QString())
                                        .trimmed();
                const QString value = (blockDetailsOptionsTable_->item(row, 1) != nullptr
                                           ? blockDetailsOptionsTable_->item(row, 1)->text()
                                           : QString())
                                          .trimmed();
                if (key.isEmpty()) {
                    continue;
                }
                if (!key.startsWith(QLatin1Char('-'))) {
                    if (validationError != nullptr) {
                        *validationError = tr("Option key `%1` must start with '-'.").arg(key);
                    }
                    return false;
                }

                const QString normalizedKey = key.toLower();
                const QString arity = commandOptionValueArityTokens_.value(commandOptionValueKey(commandToken, normalizedKey));
                const bool arityRequiresValue = optionArityRequiresValue(arity);
                const bool arityForbidsValue = optionArityForbidsValue(arity);
                if (arityRequiresValue && value.isEmpty()) {
                    if (validationError != nullptr) {
                        *validationError = tr("Option `%1` requires a value.").arg(key);
                    }
                    return false;
                }
                if (arityForbidsValue && !value.isEmpty()) {
                    if (validationError != nullptr) {
                        *validationError = tr("Option `%1` does not accept a value.").arg(key);
                    }
                    return false;
                }

                const QString optionKey = commandOptionValueKey(commandToken, normalizedKey);
                const int fixedArity = commandOptionFixedArityByKey_.value(optionKey, -1);
                const QStringList providedValues = parseOptionValuesFromEditor(value, arity, fixedArity);
                if (fixedArity >= 0) {
                    if (providedValues.size() != fixedArity) {
                        if (blockDetailsOptionsTable_ != nullptr
                            && row >= 0
                            && row < blockDetailsOptionsTable_->rowCount()) {
                            blockDetailsOptionsTable_->setCurrentCell(row, 0);
                        }
                        if (validationError != nullptr) {
                            const QStringList argumentLabels = commandOptionArgumentLabelsByKey_.value(optionKey);
                            if (!argumentLabels.isEmpty()) {
                                *validationError = tr("Option `%1` requires exactly %2 value(s): %3.")
                                                       .arg(key)
                                                       .arg(fixedArity)
                                                       .arg(argumentLabels.join(QStringLiteral(", ")));
                            } else {
                                *validationError = tr("Option `%1` requires exactly %2 value(s).")
                                                       .arg(key)
                                                       .arg(fixedArity);
                            }
                        }
                        return false;
                    }
                } else if (canonicalOptionArityToken(arity) == QStringLiteral("EXACTLY_ONE")
                           && !providedValues.isEmpty()
                           && providedValues.size() != 1) {
                    if (blockDetailsOptionsTable_ != nullptr
                        && row >= 0
                        && row < blockDetailsOptionsTable_->rowCount()) {
                        blockDetailsOptionsTable_->setCurrentCell(row, 0);
                    }
                    if (validationError != nullptr) {
                        *validationError = tr("Option `%1` requires exactly one value.").arg(key);
                    }
                    return false;
                } else if (canonicalOptionArityToken(arity) == QStringLiteral("ONE_OR_MORE")
                           && providedValues.isEmpty()) {
                    if (blockDetailsOptionsTable_ != nullptr
                        && row >= 0
                        && row < blockDetailsOptionsTable_->rowCount()) {
                        blockDetailsOptionsTable_->setCurrentCell(row, 0);
                    }
                    if (validationError != nullptr) {
                        *validationError = tr("Option `%1` requires at least one value.").arg(key);
                    }
                    return false;
                }

                const QStringList allowedValues =
                    commandOptionValueTokens_.value(commandOptionValueKey(commandToken, normalizedKey));
                if (!allowedValues.isEmpty() && !value.isEmpty()) {
                    for (const QString &providedValue : providedValues) {
                        if (allowedValues.contains(providedValue, Qt::CaseInsensitive)) {
                            continue;
                        }
                        if (validationError != nullptr) {
                            const QString allowedValuesText = allowedValues.join(QStringLiteral(", "));
                            *validationError = tr("Option `%1` value `%2` is not allowed. Allowed values: %3.")
                                                   .arg(key, providedValue, allowedValuesText);
                        }
                        return false;
                    }
                }

                serializedOptions.append(key);
                if (!providedValues.isEmpty()) {
                    QStringList serializedValues;
                    serializedValues.reserve(providedValues.size());
                    for (const QString &providedValue : providedValues) {
                        serializedValues.append(serializeTherionArgumentToken(providedValue.trimmed()));
                    }
                    serializedOptions.append(serializedValues.join(QLatin1Char(' ')));
                }
            }
        }

        if (!updatedId.isEmpty()) {
            result += QStringLiteral(" ") + updatedId;
        }
        const QString updatedAdditionalPositionalTokens = blockDetailsAdditionalPositionalEdit_ != nullptr
            ? blockDetailsAdditionalPositionalEdit_->text().trimmed()
            : QString();
        if (!updatedAdditionalPositionalTokens.isEmpty()) {
            result += QStringLiteral(" ") + updatedAdditionalPositionalTokens;
        }
        if (!serializedOptions.isEmpty()) {
            result += QStringLiteral(" ") + serializedOptions.join(QLatin1Char(' '));
        }
    } else {
        if (validationError != nullptr) {
            *validationError = tr("This block cannot be edited in details pane.");
        }
        return false;
    }

    const QString updatedComment = blockDetailsCommentEdit_ != nullptr
        ? blockDetailsCommentEdit_->text().trimmed()
        : QString();
    if (!updatedComment.isEmpty()) {
        QChar marker = blockDetailsCommentMarker_;
        if (marker != QLatin1Char('#') && marker != QLatin1Char('%')) {
            marker = QLatin1Char('#');
        }
        result += QStringLiteral(" %1 %2").arg(QString(marker), updatedComment);
    }

    *updatedLine = result;
    return true;
}

void TextEditorTab::refreshBlockDetailsApplyState()
{
    if (tearingDown_ || blockDetailsPopulating_ || blockDetailsApplyButton_ == nullptr || editor_ == nullptr) {
        return;
    }

    QString validationError;
    QString candidateLine;
    const bool buildOk = buildUpdatedLineFromBlockDetails(&candidateLine, &validationError);

    QString currentLine;
    bool hasCurrentLine = false;
    if (blockDetailsSelectedLineNumber_ > 0) {
        QStringList lines = editor_->toPlainText().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        for (QString &line : lines) {
            if (line.endsWith(QLatin1Char('\r'))) {
                line.chop(1);
            }
        }
        if (blockDetailsSelectedLineNumber_ <= lines.size()) {
            currentLine = lines.at(blockDetailsSelectedLineNumber_ - 1);
            hasCurrentLine = true;
        }
    }

    const bool hasChanges = buildOk && hasCurrentLine && candidateLine != currentLine;
    blockDetailsApplyButton_->setEnabled(hasChanges);

    if (blockDetailsStatusLabel_ == nullptr) {
        return;
    }
    if (!validationError.isEmpty()) {
        blockDetailsStatusLabel_->setStyleSheet(QStringLiteral("color: #c0392b;"));
        blockDetailsStatusLabel_->setText(QStringLiteral("%1 — %2").arg(blockDetailsBaseStatusText_, validationError));
        return;
    }
    blockDetailsStatusLabel_->setStyleSheet(QString());
    blockDetailsStatusLabel_->setText(blockDetailsBaseStatusText_);
}

void TextEditorTab::applyBlockDetailsChanges()
{
    if (tearingDown_) {
        return;
    }
    QString updatedLine;
    QString validationError;
    if (!buildUpdatedLineFromBlockDetails(&updatedLine, &validationError)) {
        if (!validationError.isEmpty()) {
            QMessageBox::warning(this, tr("Configure Block"), validationError);
        }
        return;
    }

    const QTextBlock targetBlock = editor_->document()->findBlockByLineNumber(blockDetailsSelectedLineNumber_ - 1);
    if (!targetBlock.isValid()) {
        return;
    }

    QTextCursor editCursor(targetBlock);
    editCursor.beginEditBlock();
    editCursor.movePosition(QTextCursor::StartOfBlock);
    editCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    editCursor.insertText(updatedLine);
    editCursor.endEditBlock();
    editor_->setTextCursor(editCursor);

    selectBlockInCanvasAndDetails(blockDetailsSelectedLineNumber_);
    refreshBlockDetailsApplyState();
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

    auto resolveBlockFromItem = [](QGraphicsItem *item) -> BlockCanvasItem * {
        while (item != nullptr) {
            if (auto *blockItem = dynamic_cast<BlockCanvasItem *>(item)) {
                return blockItem;
            }
            item = item->parentItem();
        }
        return nullptr;
    };
    const QString normalizedKind = kind.trimmed().toLower();
    if (normalizedKind.isEmpty()) {
        return;
    }

    QString contents = editor_->toPlainText();
    QStringList existingLines = contents.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : existingLines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));

    auto lineIndent = [&existingLines, &indentPattern](int lineNumber) {
        if (lineNumber <= 0 || lineNumber > existingLines.size()) {
            return QString();
        }
        const auto match = indentPattern.match(existingLines.at(lineNumber - 1));
        return match.hasMatch() ? match.captured(0) : QString();
    };

    QHash<int, BlockCanvasItem *> sceneBlocksByLine;
    if (blockCanvasScene_ != nullptr) {
        const QList<QGraphicsItem *> sceneItems = blockCanvasScene_->items();
        for (QGraphicsItem *item : sceneItems) {
            auto *blockItem = dynamic_cast<BlockCanvasItem *>(item);
            if (blockItem == nullptr) {
                continue;
            }
            sceneBlocksByLine.insert(blockItem->lineNumber(), blockItem);
        }
    }

    struct BlockEntry
    {
        QString kind;
        int startLine = 0;
        int endLine = 0;
        int parentLine = 0;
    };

    QVector<TherionParsedLine> parsedLines;
    parsedLines.reserve(existingLines.size());
    for (int lineIndex = 0; lineIndex < existingLines.size(); ++lineIndex) {
        parsedLines.append(TherionDocumentParser::parseLine(existingLines.at(lineIndex), lineIndex + 1));
    }

    QVector<BlockEntry> entries;
    entries.reserve(parsedLines.size());
    QHash<int, int> entryIndexByStartLine;

    struct OpenContainer
    {
        QString directive;
        int lineNumber = 0;
    };
    QVector<OpenContainer> openStack;

    const QString dataScope = resolveScopeForCommandAtLine(QStringLiteral("data"),
                                                           existingLines,
                                                           existingLines.size() + 1);
    const QString dataScopeClosing = completionClosingDirectiveForOpening(dataScope);

    auto nearestOpenContainerLine = [&openStack](const QString &directive) {
        for (int index = openStack.size() - 1; index >= 0; --index) {
            if (openStack.at(index).directive == directive) {
                return openStack.at(index).lineNumber;
            }
        }
        return 0;
    };

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const int currentLineNumber = parsedLine.lineNumber;
        if (isFullLineComment(parsedLine)) {
            BlockEntry entry;
            entry.kind = QStringLiteral("comment");
            entry.startLine = currentLineNumber;
            entry.endLine = currentLineNumber;
            entry.parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            entryIndexByStartLine.insert(entry.startLine, entries.size());
            entries.append(entry);
            continue;
        }

        const QString directive = normalizeDirective(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }

        if (isBlockClosingDirective(directive)) {
            const QString openingDirective = completionOpeningDirectiveForClosing(directive);
            if (openingDirective.isEmpty()) {
                continue;
            }
            for (int index = openStack.size() - 1; index >= 0; --index) {
                if (openStack.at(index).directive == openingDirective) {
                    openStack.remove(index, openStack.size() - index);
                    break;
                }
            }
            continue;
        }

        if (isBlockOpeningDirective(directive)) {
            int parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            if (directive == QStringLiteral("data")) {
                const int dataScopeParentLine = nearestOpenContainerLine(dataScope);
                if (dataScopeParentLine > 0) {
                    parentLine = dataScopeParentLine;
                }
            }

            BlockEntry entry;
            entry.kind = directive;
            entry.startLine = currentLineNumber;
            entry.endLine = currentLineNumber;
            entry.parentLine = parentLine;

            if (isContainerDirectiveInstance(directive, parsedLine)) {
                const QString closingDirective = closingDirectiveFor(directive);
                const int endLine = findMatchingBlockEndLine(existingLines, currentLineNumber, directive, closingDirective);
                if (endLine > currentLineNumber) {
                    entry.endLine = endLine;
                }
                openStack.append(OpenContainer{directive, currentLineNumber});
            } else if (directive == QStringLiteral("data")) {
                const int dataScopeParentLine = nearestOpenContainerLine(dataScope);
                int dataScopeEndLine = existingLines.size() + 1;
                if (dataScopeParentLine > 0) {
                    const int resolvedEndLine = findMatchingBlockEndLine(existingLines,
                                                                         dataScopeParentLine,
                                                                         dataScope,
                                                                         dataScopeClosing);
                    if (resolvedEndLine > dataScopeParentLine) {
                        dataScopeEndLine = resolvedEndLine;
                    }
                }

                int dataBodyLastLine = dataScopeEndLine - 1;
                for (int scanLine = currentLineNumber + 1; scanLine <= dataScopeEndLine - 1; ++scanLine) {
                    const TherionParsedLine scanParsedLine = TherionDocumentParser::parseLine(existingLines.at(scanLine - 1), scanLine);
                    const QString scanDirective = normalizeDirective(scanParsedLine.directive);
                    if (scanDirective.isEmpty() || scanDirective == QStringLiteral("extend")) {
                        continue;
                    }
                    if (scanDirective == QStringLiteral("data")
                        || (!dataScopeClosing.isEmpty() && scanDirective == dataScopeClosing)
                        || (!dataScope.isEmpty() && isCommandDirectiveInScope(scanDirective, dataScope))) {
                        dataBodyLastLine = scanLine - 1;
                        break;
                    }
                }
                if (dataBodyLastLine < currentLineNumber) {
                    dataBodyLastLine = currentLineNumber;
                }
                entry.endLine = dataBodyLastLine;
            }

            entryIndexByStartLine.insert(entry.startLine, entries.size());
            entries.append(entry);
            continue;
        }

        QString activeScope = QStringLiteral("none");
        if (!openStack.isEmpty()) {
            activeScope = normalizeDirective(openStack.last().directive);
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
        if (commandDirective) {
            BlockEntry entry;
            entry.kind = directive;
            entry.startLine = currentLineNumber;
            entry.endLine = currentLineNumber;
            entry.parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            entryIndexByStartLine.insert(entry.startLine, entries.size());
            entries.append(entry);
        }
    }

    qreal sceneTop = 0.0;
    qreal sceneBottom = 0.0;
    bool hasSceneVerticalBounds = false;
    for (const BlockEntry &entry : entries) {
        const auto sceneItemIt = sceneBlocksByLine.constFind(entry.startLine);
        if (sceneItemIt == sceneBlocksByLine.cend() || sceneItemIt.value() == nullptr) {
            continue;
        }
        const QRectF blockRect = sceneItemIt.value()->sceneBoundingRect();
        if (!hasSceneVerticalBounds) {
            sceneTop = blockRect.top();
            sceneBottom = blockRect.bottom();
            hasSceneVerticalBounds = true;
        } else {
            sceneTop = qMin(sceneTop, blockRect.top());
            sceneBottom = qMax(sceneBottom, blockRect.bottom());
        }
    }

    const qreal outerDropThreshold = 16.0;
    const bool dropBeforeAllBlocks = hasSceneVerticalBounds && scenePos.y() < (sceneTop - outerDropThreshold);
    const bool dropAfterAllBlocks = hasSceneVerticalBounds && scenePos.y() > (sceneBottom + outerDropThreshold);

    int explicitEndHintContainerLine = 0;
    if (blockCanvasScene_ != nullptr) {
        explicitEndHintContainerLine = resolveEndHintContainerStartLineAtScenePos(scenePos);
    }

    BlockCanvasItem *targetBlockItem = nullptr;
    if (!dropBeforeAllBlocks && !dropAfterAllBlocks && explicitEndHintContainerLine <= 0) {
        if (blockCanvasScene_ != nullptr) {
            targetBlockItem = resolveBlockFromItem(blockCanvasScene_->itemAt(scenePos, QTransform()));
        }
        if (targetBlockItem == nullptr) {
            qreal bestDistance = std::numeric_limits<qreal>::max();
            for (const BlockEntry &entry : entries) {
                const auto sceneItemIt = sceneBlocksByLine.constFind(entry.startLine);
                if (sceneItemIt == sceneBlocksByLine.cend() || sceneItemIt.value() == nullptr) {
                    continue;
                }
                const QRectF blockRect = sceneItemIt.value()->sceneBoundingRect();
                const qreal topDistance = qAbs(blockRect.top() - scenePos.y());
                const qreal bottomDistance = qAbs(blockRect.bottom() - scenePos.y());
                const qreal distance = qMin(topDistance, bottomDistance);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    targetBlockItem = sceneItemIt.value();
                }
            }
        }
    }

    QStringList linesToInsert;
    int insertBeforeLine = qMax(1, editor_->document()->blockCount() + 1);
    int parentLine = 0;
    QString parentKind;

    if (dropBeforeAllBlocks) {
        insertBeforeLine = 1;
        parentLine = 0;
    } else if (explicitEndHintContainerLine > 0) {
        const auto hintedEntryIt = entryIndexByStartLine.constFind(explicitEndHintContainerLine);
        if (hintedEntryIt != entryIndexByStartLine.cend()) {
            const BlockEntry hintedEntry = entries.at(*hintedEntryIt);
            insertBeforeLine = hintedEntry.endLine + 1;
            parentLine = hintedEntry.parentLine;
        }
    } else if (targetBlockItem != nullptr) {
        const auto targetIndexIt = entryIndexByStartLine.constFind(targetBlockItem->lineNumber());
        if (targetIndexIt != entryIndexByStartLine.cend()) {
            const BlockEntry targetEntry = entries.at(*targetIndexIt);
            parentLine = targetEntry.parentLine;
            insertBeforeLine = targetEntry.startLine;

            const QRectF targetRect = targetBlockItem->sceneBoundingRect();
            const qreal edgeDropThreshold = qMin<qreal>(10.0, targetRect.height() * 0.25);
            const bool nearTopEdge = scenePos.y() <= (targetRect.top() + edgeDropThreshold);
            const bool nearBottomEdge = scenePos.y() >= (targetRect.bottom() - edgeDropThreshold);
            const bool preferBetweenInsertion = nearTopEdge || nearBottomEdge;

            const bool targetAcceptsSourceAsChild = isContainerBlockDirective(targetEntry.kind)
                && isCompatibleChildKindForBlocks(targetEntry.kind, normalizedKind);
            if (targetAcceptsSourceAsChild) {
                if (!preferBetweenInsertion) {
                    parentLine = targetEntry.startLine;
                    insertBeforeLine = targetEntry.endLine;
                } else if (nearBottomEdge) {
                    int firstChildStartLine = 0;
                    for (const BlockEntry &entry : entries) {
                        if (entry.parentLine != targetEntry.startLine) {
                            continue;
                        }
                        if (firstChildStartLine == 0 || entry.startLine < firstChildStartLine) {
                            firstChildStartLine = entry.startLine;
                        }
                    }
                    parentLine = targetEntry.startLine;
                    insertBeforeLine = firstChildStartLine > 0 ? firstChildStartLine : targetEntry.endLine;
                } else {
                    const qreal targetCenterY = targetRect.center().y();
                    const bool insertAfterTarget = scenePos.y() > targetCenterY;
                    insertBeforeLine = insertAfterTarget ? (targetEntry.endLine + 1) : targetEntry.startLine;
                    parentLine = targetEntry.parentLine;
                }
            } else {
                const qreal targetCenterY = targetRect.center().y();
                const bool insertAfterTarget = scenePos.y() > targetCenterY;
                insertBeforeLine = insertAfterTarget ? (targetEntry.endLine + 1) : targetEntry.startLine;
                parentLine = targetEntry.parentLine;
            }
        }
    }

    if (parentLine > 0) {
        const auto parentEntryIt = entryIndexByStartLine.constFind(parentLine);
        if (parentEntryIt != entryIndexByStartLine.cend()) {
            parentKind = entries.at(*parentEntryIt).kind;
        }
    }

    if (!isCompatibleChildKindForBlocks(parentKind, normalizedKind)) {
        bool promoted = false;
        int incompatibleParentLine = parentLine;
        while (incompatibleParentLine > 0) {
            const auto incompatibleParentIt = entryIndexByStartLine.constFind(incompatibleParentLine);
            if (incompatibleParentIt == entryIndexByStartLine.cend()) {
                break;
            }
            const BlockEntry incompatibleParentEntry = entries.at(*incompatibleParentIt);
            const int candidateParentLine = incompatibleParentEntry.parentLine;
            QString candidateParentKind;
            if (candidateParentLine > 0) {
                const auto candidateParentIt = entryIndexByStartLine.constFind(candidateParentLine);
                if (candidateParentIt != entryIndexByStartLine.cend()) {
                    candidateParentKind = entries.at(*candidateParentIt).kind;
                }
            }
            if (isCompatibleChildKindForBlocks(candidateParentKind, normalizedKind)) {
                insertBeforeLine = incompatibleParentEntry.endLine + 1;
                parentLine = candidateParentLine;
                parentKind = candidateParentKind;
                promoted = true;
                break;
            }
            incompatibleParentLine = candidateParentLine;
        }

        if (!promoted && isCompatibleChildKindForBlocks(QString(), normalizedKind)) {
            insertBeforeLine = qMax(1, editor_->document()->blockCount() + 1);
            parentLine = 0;
            parentKind.clear();
            promoted = true;
        }

        if (!promoted) {
            QMessageBox::warning(this,
                                 tr("Incompatible Drop"),
                                 tr("`%1` cannot be inserted inside `%2`.")
                                     .arg(normalizedKind, parentKind.isEmpty() ? tr("root") : parentKind));
            return;
        }
    }

    const QString indent = parentLine > 0
        ? lineIndent(parentLine) + (isContainerBlockDirective(parentKind) ? QStringLiteral("  ") : QString())
        : QString();

    if (normalizedKind == QStringLiteral("comment")) {
        linesToInsert << QStringLiteral("%1#").arg(indent);
    } else if (normalizedKind == QStringLiteral("data")) {
        // Insert bare `data` so users can define style/readings explicitly.
        linesToInsert << QStringLiteral("%1data").arg(indent);
    } else if (isContainerBlockDirective(normalizedKind)) {
        QString line = QStringLiteral("%1%2").arg(indent, normalizedKind);
        const QString closingDirective = completionClosingDirectiveForOpening(normalizedKind);
        linesToInsert << line;
        if (!closingDirective.isEmpty()) {
            linesToInsert << QStringLiteral("%1%2").arg(indent, closingDirective);
        }
    } else {
        linesToInsert << QStringLiteral("%1%2").arg(indent, normalizedKind);
    }

    if (linesToInsert.isEmpty()) {
        const QString indent = parentLine > 0
            ? lineIndent(parentLine) + (isContainerBlockDirective(parentKind) ? QStringLiteral("  ") : QString())
            : QString();
        linesToInsert << QStringLiteral("%1%2").arg(indent, normalizedKind);
    }

    QString errorMessage;
    if (!insertLinesBefore(insertBeforeLine, linesToInsert, &errorMessage)) {
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

    auto resolveBlockFromItem = [](QGraphicsItem *item) -> BlockCanvasItem * {
        while (item != nullptr) {
            if (auto *blockItem = dynamic_cast<BlockCanvasItem *>(item)) {
                return blockItem;
            }
            item = item->parentItem();
        }
        return nullptr;
    };

    QHash<int, BlockCanvasItem *> sceneBlocksByLine;
    const QList<QGraphicsItem *> sceneItems = blockCanvasScene_->items();
    for (QGraphicsItem *item : sceneItems) {
        auto *blockItem = dynamic_cast<BlockCanvasItem *>(item);
        if (blockItem == nullptr) {
            continue;
        }
        sceneBlocksByLine.insert(blockItem->lineNumber(), blockItem);
    }

    struct BlockEntry
    {
        QString kind;
        int startLine = 0;
        int endLine = 0;
        int parentLine = 0;
    };

    QString contents = editor_->toPlainText();
    QStringList lines = contents.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }

    QVector<TherionParsedLine> parsedLines;
    parsedLines.reserve(lines.size());
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        parsedLines.append(TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1));
    }
    QVector<BlockEntry> entries;
    entries.reserve(parsedLines.size());
    QHash<int, int> entryIndexByStartLine;

    struct OpenContainer
    {
        QString directive;
        int lineNumber = 0;
    };
    QVector<OpenContainer> openStack;

    const QString dataScope = resolveScopeForCommandAtLine(QStringLiteral("data"),
                                                           lines,
                                                           lines.size() + 1);
    const QString dataScopeClosing = completionClosingDirectiveForOpening(dataScope);

    auto nearestOpenContainerLine = [&openStack](const QString &directive) {
        for (int index = openStack.size() - 1; index >= 0; --index) {
            if (openStack.at(index).directive == directive) {
                return openStack.at(index).lineNumber;
            }
        }
        return 0;
    };

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const int currentLineNumber = parsedLine.lineNumber;
        if (isFullLineComment(parsedLine)) {
            BlockEntry entry;
            entry.kind = QStringLiteral("comment");
            entry.startLine = currentLineNumber;
            entry.endLine = currentLineNumber;
            entry.parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            entryIndexByStartLine.insert(entry.startLine, entries.size());
            entries.append(entry);
            continue;
        }

        const QString directive = normalizeDirective(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }

        if (isBlockClosingDirective(directive)) {
            const QString openingDirective = completionOpeningDirectiveForClosing(directive);
            if (openingDirective.isEmpty()) {
                continue;
            }
            for (int index = openStack.size() - 1; index >= 0; --index) {
                if (openStack.at(index).directive == openingDirective) {
                    openStack.remove(index, openStack.size() - index);
                    break;
                }
            }
            continue;
        }

        if (isBlockOpeningDirective(directive)) {
            int parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            if (directive == QStringLiteral("data")) {
                const int dataScopeParentLine = nearestOpenContainerLine(dataScope);
                if (dataScopeParentLine > 0) {
                    parentLine = dataScopeParentLine;
                }
            }

            BlockEntry entry;
            entry.kind = directive;
            entry.startLine = currentLineNumber;
            entry.endLine = currentLineNumber;
            entry.parentLine = parentLine;

            if (isContainerDirectiveInstance(directive, parsedLine)) {
                const QString closingDirective = closingDirectiveFor(directive);
                const int endLine = findMatchingBlockEndLine(lines, currentLineNumber, directive, closingDirective);
                if (endLine > currentLineNumber) {
                    entry.endLine = endLine;
                }
                openStack.append(OpenContainer{directive, currentLineNumber});
            } else if (directive == QStringLiteral("data")) {
                const int dataScopeParentLine = nearestOpenContainerLine(dataScope);
                int dataScopeEndLine = lines.size() + 1;
                if (dataScopeParentLine > 0) {
                    const int resolvedEndLine = findMatchingBlockEndLine(lines,
                                                                         dataScopeParentLine,
                                                                         dataScope,
                                                                         dataScopeClosing);
                    if (resolvedEndLine > dataScopeParentLine) {
                        dataScopeEndLine = resolvedEndLine;
                    }
                }

                int dataBodyLastLine = dataScopeEndLine - 1;
                for (int scanLine = currentLineNumber + 1; scanLine <= dataScopeEndLine - 1; ++scanLine) {
                    const TherionParsedLine scanParsedLine = TherionDocumentParser::parseLine(lines.at(scanLine - 1), scanLine);
                    const QString scanDirective = normalizeDirective(scanParsedLine.directive);
                    if (scanDirective.isEmpty() || scanDirective == QStringLiteral("extend")) {
                        continue;
                    }
                    if (scanDirective == QStringLiteral("data")
                        || (!dataScopeClosing.isEmpty() && scanDirective == dataScopeClosing)
                        || (!dataScope.isEmpty() && isCommandDirectiveInScope(scanDirective, dataScope))) {
                        dataBodyLastLine = scanLine - 1;
                        break;
                    }
                }
                if (dataBodyLastLine < currentLineNumber) {
                    dataBodyLastLine = currentLineNumber;
                }
                entry.endLine = dataBodyLastLine;
            }

            entryIndexByStartLine.insert(entry.startLine, entries.size());
            entries.append(entry);
            continue;
        }

        QString activeScope = QStringLiteral("none");
        if (!openStack.isEmpty()) {
            activeScope = normalizeDirective(openStack.last().directive);
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
        if (commandDirective) {
            BlockEntry entry;
            entry.kind = directive;
            entry.startLine = currentLineNumber;
            entry.endLine = currentLineNumber;
            entry.parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            entryIndexByStartLine.insert(entry.startLine, entries.size());
            entries.append(entry);
        }
    }

    const auto sourceIndexIt = entryIndexByStartLine.constFind(lineNumber);
    if (sourceIndexIt == entryIndexByStartLine.cend()) {
        return;
    }
    const BlockEntry sourceEntry = entries.at(*sourceIndexIt);
    if (sourceEntry.kind == QStringLiteral("encoding")) {
        return;
    }

    auto isEntryInsideSourceSubtree = [&sourceEntry](const BlockEntry &entry) {
        return entry.startLine >= sourceEntry.startLine && entry.startLine <= sourceEntry.endLine;
    };

    qreal sceneTop = 0.0;
    qreal sceneBottom = 0.0;
    bool hasSceneVerticalBounds = false;
    for (const BlockEntry &entry : entries) {
        const auto sceneItemIt = sceneBlocksByLine.constFind(entry.startLine);
        if (sceneItemIt == sceneBlocksByLine.cend() || sceneItemIt.value() == nullptr) {
            continue;
        }
        const QRectF blockRect = sceneItemIt.value()->sceneBoundingRect();
        if (!hasSceneVerticalBounds) {
            sceneTop = blockRect.top();
            sceneBottom = blockRect.bottom();
            hasSceneVerticalBounds = true;
        } else {
            sceneTop = qMin(sceneTop, blockRect.top());
            sceneBottom = qMax(sceneBottom, blockRect.bottom());
        }
    }

    const qreal outerDropThreshold = 16.0;
    const bool dropBeforeAllBlocks = hasSceneVerticalBounds && scenePos.y() < (sceneTop - outerDropThreshold);
    const bool dropAfterAllBlocks = hasSceneVerticalBounds && scenePos.y() > (sceneBottom + outerDropThreshold);
    const int explicitEndHintContainerLine = resolveEndHintContainerStartLineAtScenePos(scenePos);

    int destinationParentLine = 0;
    int insertBeforeLineOriginal = 1;
    bool destinationResolved = false;

    if (dropBeforeAllBlocks) {
        destinationParentLine = 0;
        insertBeforeLineOriginal = 1;
        destinationResolved = true;
    } else if (dropAfterAllBlocks) {
        destinationParentLine = 0;
        insertBeforeLineOriginal = lines.size() + 1;
        destinationResolved = true;
    } else if (explicitEndHintContainerLine > 0) {
        const auto hintedIndexIt = entryIndexByStartLine.constFind(explicitEndHintContainerLine);
        if (hintedIndexIt != entryIndexByStartLine.cend()) {
            const BlockEntry hintedEntry = entries.at(*hintedIndexIt);
            destinationParentLine = hintedEntry.parentLine;
            insertBeforeLineOriginal = hintedEntry.endLine + 1;
            destinationResolved = true;
        }
    }

    if (!destinationResolved) {
        BlockCanvasItem *targetBlockItem = resolveBlockFromItem(blockCanvasScene_->itemAt(scenePos, QTransform()));
        if (targetBlockItem != nullptr && targetBlockItem->lineNumber() == sourceEntry.startLine) {
            targetBlockItem = nullptr;
        }
        if (targetBlockItem == nullptr) {
            qreal bestDistance = std::numeric_limits<qreal>::max();
            for (const BlockEntry &entry : entries) {
                if (isEntryInsideSourceSubtree(entry)) {
                    continue;
                }
                const auto sceneItemIt = sceneBlocksByLine.constFind(entry.startLine);
                if (sceneItemIt == sceneBlocksByLine.cend() || sceneItemIt.value() == nullptr) {
                    continue;
                }
                const QRectF blockRect = sceneItemIt.value()->sceneBoundingRect();
                const qreal topDistance = qAbs(blockRect.top() - scenePos.y());
                const qreal bottomDistance = qAbs(blockRect.bottom() - scenePos.y());
                const qreal distance = qMin(topDistance, bottomDistance);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    targetBlockItem = sceneItemIt.value();
                }
            }
        }
        if (targetBlockItem == nullptr) {
            return;
        }

        const auto targetIndexIt = entryIndexByStartLine.constFind(targetBlockItem->lineNumber());
        if (targetIndexIt == entryIndexByStartLine.cend()) {
            return;
        }
        const BlockEntry targetEntry = entries.at(*targetIndexIt);

        destinationParentLine = targetEntry.parentLine;
        insertBeforeLineOriginal = targetEntry.startLine;

        const QRectF targetRect = targetBlockItem->sceneBoundingRect();
        const qreal edgeDropThreshold = qMin<qreal>(10.0, targetRect.height() * 0.25);
        const bool nearTopEdge = scenePos.y() <= (targetRect.top() + edgeDropThreshold);
        const bool nearBottomEdge = scenePos.y() >= (targetRect.bottom() - edgeDropThreshold);
        const bool preferBetweenInsertion = nearTopEdge || nearBottomEdge;

        const bool targetAcceptsSourceAsChild = isContainerBlockDirective(targetEntry.kind)
            && isCompatibleChildKindForBlocks(targetEntry.kind, sourceEntry.kind);
        if (targetAcceptsSourceAsChild && targetEntry.startLine != sourceEntry.startLine) {
            if (!preferBetweenInsertion) {
                destinationParentLine = targetEntry.startLine;
                insertBeforeLineOriginal = targetEntry.endLine;
            } else if (nearBottomEdge) {
                int firstChildStartLine = 0;
                for (const BlockEntry &entry : entries) {
                    if (entry.parentLine != targetEntry.startLine || isEntryInsideSourceSubtree(entry)) {
                        continue;
                    }
                    if (firstChildStartLine == 0 || entry.startLine < firstChildStartLine) {
                        firstChildStartLine = entry.startLine;
                    }
                }
                destinationParentLine = targetEntry.startLine;
                insertBeforeLineOriginal = firstChildStartLine > 0 ? firstChildStartLine : targetEntry.endLine;
            } else {
                const qreal targetCenterY = targetRect.center().y();
                const bool insertAfterTarget = scenePos.y() > targetCenterY;
                insertBeforeLineOriginal = insertAfterTarget ? (targetEntry.endLine + 1) : targetEntry.startLine;
                destinationParentLine = targetEntry.parentLine;
            }
        } else {
            const qreal targetCenterY = targetRect.center().y();
            const bool insertAfterTarget = scenePos.y() > targetCenterY;
            insertBeforeLineOriginal = insertAfterTarget ? (targetEntry.endLine + 1) : targetEntry.startLine;
            destinationParentLine = targetEntry.parentLine;
        }
    }

    if (destinationParentLine >= sourceEntry.startLine && destinationParentLine <= sourceEntry.endLine) {
        QMessageBox::warning(this, tr("Reorder Block"), tr("Cannot move a block inside itself."));
        return;
    }
    if (insertBeforeLineOriginal > sourceEntry.startLine && insertBeforeLineOriginal <= sourceEntry.endLine + 1) {
        return;
    }

    QString destinationParentKind;
    if (destinationParentLine > 0) {
        const auto parentEntryIt = entryIndexByStartLine.constFind(destinationParentLine);
        if (parentEntryIt != entryIndexByStartLine.cend()) {
            destinationParentKind = entries.at(*parentEntryIt).kind;
        }
    }

    if (!isCompatibleChildKindForBlocks(destinationParentKind, sourceEntry.kind)) {
        QMessageBox::warning(this,
                             tr("Reorder Block"),
                             tr("`%1` cannot be moved under `%2`.")
                                 .arg(sourceEntry.kind, destinationParentKind.isEmpty() ? tr("root") : destinationParentKind));
        return;
    }

    const int sourceStartIndex = sourceEntry.startLine - 1;
    const int sourceEndIndex = sourceEntry.endLine - 1;
    if (sourceStartIndex < 0 || sourceEndIndex >= lines.size() || sourceStartIndex > sourceEndIndex) {
        return;
    }

    QStringList movedChunk;
    for (int index = sourceStartIndex; index <= sourceEndIndex; ++index) {
        movedChunk.append(lines.at(index));
    }
    for (int index = sourceEndIndex; index >= sourceStartIndex; --index) {
        lines.removeAt(index);
    }

    int insertIndex = insertBeforeLineOriginal - 1;
    if (sourceEntry.startLine < insertBeforeLineOriginal) {
        insertIndex -= movedChunk.size();
    }
    insertIndex = qBound(0, insertIndex, lines.size());

    for (int offset = 0; offset < movedChunk.size(); ++offset) {
        lines.insert(insertIndex + offset, movedChunk.at(offset));
    }

    const QString lineEnding = contents.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    replaceTextForCommand(lines.join(lineEnding));
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

        QString currentId;
        int optionsStartIndex = 1;
        if (hasIdField
            && commandParsedLine.tokens.size() > 1
            && !commandParsedLine.tokens.at(1).trimmed().startsWith(QLatin1Char('-'))) {
            currentId = commandParsedLine.tokens.at(1).trimmed();
            optionsStartIndex = 2;
        }

        struct OptionEntry
        {
            QString key;
            QString value;
        };

        QStringList extraPositionalTokens;
        QVector<OptionEntry> optionEntries;
        for (int index = optionsStartIndex; index < commandParsedLine.tokens.size();) {
            const QString token = commandParsedLine.tokens.at(index).trimmed();
            if (configureTokenStartsNewOption(token)) {
                const int nextOptionIndex = nextConfigureOptionIndex(commandParsedLine.tokens, index);
                const QStringList rawOptionValues =
                    commandParsedLine.tokens.mid(index + 1, nextOptionIndex - index - 1);
                QString optionDisplayValue = rawOptionValues.join(QLatin1Char(' '));
                const int fixedArity = commandOptionFixedArityByKey_.value(
                    commandOptionValueKey(commandName, token.toLower().trimmed()), -1);
                if (fixedArity > 1 && !rawOptionValues.isEmpty()) {
                    QStringList serializedValues;
                    serializedValues.reserve(rawOptionValues.size());
                    for (const QString &rawOptionValue : rawOptionValues) {
                        serializedValues.append(serializeTherionArgumentToken(rawOptionValue));
                    }
                    optionDisplayValue = serializedValues.join(QLatin1Char(' '));
                }
                optionEntries.append(OptionEntry{
                    token,
                    optionDisplayValue,
                });
                index = nextOptionIndex;
                continue;
            }

            extraPositionalTokens.append(token);
            ++index;
        }

        const QString currentAdditionalPositionalTokens = extraPositionalTokens.join(QLatin1Char(' '));

        QDialog dialog(this);
        dialog.setWindowTitle(tr("Configure %1").arg(commandName));
        dialog.setModal(true);
        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);

        auto *formLayout = new QFormLayout;
        QLineEdit *idEdit = nullptr;
        if (hasIdField) {
            idEdit = new QLineEdit(currentId, &dialog);
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
        optionsTable->setRowCount(optionEntries.size());
        for (int row = 0; row < optionEntries.size(); ++row) {
            const OptionEntry &entry = optionEntries.at(row);
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
        const QString commandHelpHtml = renderHelpHtml(commandName, commandHelpEntry);

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

        QStringList serializedOptions;
        for (int row = 0; row < optionsTable->rowCount(); ++row) {
            const QString key = (optionsTable->item(row, 0) != nullptr
                                     ? optionsTable->item(row, 0)->text()
                                     : QString())
                                    .trimmed();
            const QString value = (optionsTable->item(row, 1) != nullptr
                                       ? optionsTable->item(row, 1)->text()
                                       : QString())
                                      .trimmed();
            if (key.isEmpty()) {
                continue;
            }
            if (!key.startsWith(QLatin1Char('-'))) {
                QMessageBox::warning(this,
                                     tr("Configure Block"),
                                     tr("Option key `%1` must start with '-' in row %2.").arg(key).arg(row + 1));
                return;
            }
            const QString normalizedKey = key.toLower();
            const QString arity = commandOptionValueArityTokens_.value(commandOptionValueKey(commandName, normalizedKey));
            const bool arityRequiresValue = optionArityRequiresValue(arity);
            const bool arityForbidsValue = optionArityForbidsValue(arity);
            if (arityRequiresValue && value.isEmpty()) {
                QMessageBox::warning(this,
                                     tr("Configure Block"),
                                     tr("Option `%1` in row %2 requires a value.")
                                         .arg(key)
                                         .arg(row + 1));
                return;
            }
            if (arityForbidsValue && !value.isEmpty()) {
                QMessageBox::warning(this,
                                     tr("Configure Block"),
                                     tr("Option `%1` in row %2 does not accept a value.")
                                         .arg(key)
                                         .arg(row + 1));
                return;
            }
            const QString optionKey = commandOptionValueKey(commandName, normalizedKey);
            const int fixedArity = commandOptionFixedArityByKey_.value(optionKey, -1);
            const QStringList providedValues = parseOptionValuesFromEditor(value, arity, fixedArity);
            if (fixedArity >= 0 && providedValues.size() != fixedArity) {
                QString detail = tr("Option `%1` in row %2 requires exactly %3 value(s).")
                                     .arg(key)
                                     .arg(row + 1)
                                     .arg(fixedArity);
                const QStringList labels = commandOptionArgumentLabelsByKey_.value(optionKey);
                if (!labels.isEmpty()) {
                    detail += QStringLiteral(" ") + tr("Parameters: %1.").arg(labels.join(QStringLiteral(", ")));
                }
                QMessageBox::warning(this, tr("Configure Block"), detail);
                return;
            }
            if (canonicalOptionArityToken(arity) == QStringLiteral("EXACTLY_ONE")
                && !providedValues.isEmpty()
                && providedValues.size() != 1) {
                QMessageBox::warning(this,
                                     tr("Configure Block"),
                                     tr("Option `%1` in row %2 requires exactly one value.")
                                         .arg(key)
                                         .arg(row + 1));
                return;
            }
            if (canonicalOptionArityToken(arity) == QStringLiteral("ONE_OR_MORE")
                && providedValues.isEmpty()) {
                QMessageBox::warning(this,
                                     tr("Configure Block"),
                                     tr("Option `%1` in row %2 requires at least one value.")
                                         .arg(key)
                                         .arg(row + 1));
                return;
            }
            serializedOptions.append(key);
            if (!providedValues.isEmpty()) {
                QStringList serializedValues;
                serializedValues.reserve(providedValues.size());
                for (const QString &providedValue : providedValues) {
                    serializedValues.append(serializeTherionArgumentToken(providedValue.trimmed()));
                }
                serializedOptions.append(serializedValues.join(QLatin1Char(' ')));
            }
        }

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
        helpBrowser->setHtml(renderHelpHtml(normalizedKind, helpEntry));
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
    if (!searchBar_->isVisible()) {
        return;
    }

    updateSearchResults(tr("Ready"));
}

void TextEditorTab::handleReplaceTextEdited(const QString &)
{
    if (!searchBar_->isVisible()) {
        return;
    }

    updateSearchResults(tr("Ready"));
}

void TextEditorTab::handleSearchOptionsChanged(bool)
{
    if (!searchBar_->isVisible()) {
        return;
    }

    updateSearchResults(tr("Ready"));
}

void TextEditorTab::handleFindNextTriggered()
{
    performFind(true, true);
}

void TextEditorTab::handleFindPreviousTriggered()
{
    performFind(false, true);
}

void TextEditorTab::handleReplaceTriggered()
{
    replaceCurrent();
}

void TextEditorTab::handleReplaceAllTriggered()
{
    replaceAll();
}

void TextEditorTab::handleCloseSearchTriggered()
{
    hideFindBar();
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
    rebuildCompletionModel();
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
    const QJsonArray commands = catalogObject.value(QStringLiteral("commands")).toArray();
    for (const QJsonValue &commandValue : commands) {
        const QJsonObject commandObject = commandValue.toObject();
        const QString commandName = commandObject.value(QStringLiteral("name")).toString().trimmed().toLower();
        if (commandName.isEmpty()) {
            continue;
        }

        TherionHelpEntry entry;
        entry.summary = commandObject.value(QStringLiteral("summary")).toString().trimmed();
        int requiredPositionalCount = 0;
        QStringList commandArgumentSignatures;
        bool primaryValueIsPerson = false;

        const QJsonArray syntaxArray = commandObject.value(QStringLiteral("syntax")).toArray();
        QStringList syntaxRows;
        for (const QJsonValue &value : syntaxArray) {
            appendUnique(syntaxRows, value.toString());
        }
        entry.syntax = syntaxRows.join(QStringLiteral("\n"));

        const QJsonArray argumentsArray = commandObject.value(QStringLiteral("arguments")).toArray();
        for (int argumentIndex = 0; argumentIndex < argumentsArray.size(); ++argumentIndex) {
            const QJsonValue value = argumentsArray.at(argumentIndex);
            const QJsonObject argumentObject = value.toObject();
            const QString signature = argumentObject.value(QStringLiteral("signature")).toString().trimmed();
            if (!signature.isEmpty()) {
                commandArgumentSignatures.append(signature);
            }
            const QString description = argumentObject.value(QStringLiteral("description")).toString().trimmed();
            const QString argumentLine = description.isEmpty() ? signature : QStringLiteral("%1 = %2").arg(signature, description);
            appendUnique(entry.arguments, argumentLine);
            if (isRequiredArgumentSignature(signature)) {
                ++requiredPositionalCount;
            }
            if (!primaryValueIsPerson) {
                const QString normalizedSignature = signature.trimmed().toLower();
                if (normalizedSignature.contains(QStringLiteral("<person>"))) {
                    primaryValueIsPerson = true;
                }
            }

            QStringList argumentAllowedValues;
            const QJsonArray argumentAllowedValuesArray = argumentObject.value(QStringLiteral("allowed_values")).toArray();
            for (const QJsonValue &argumentAllowedValue : argumentAllowedValuesArray) {
                appendUnique(argumentAllowedValues, argumentAllowedValue.toString().trimmed().toLower());
            }
            if (!argumentAllowedValues.isEmpty()) {
                appendUniqueList(commandArgumentValueTokens_[commandArgumentValueKey(commandName, argumentIndex)],
                                 argumentAllowedValues);
            }
        }

        const QJsonArray optionsArray = commandObject.value(QStringLiteral("options")).toArray();
        for (const QJsonValue &value : optionsArray) {
            const QJsonObject optionObject = value.toObject();
            const QString signature = optionObject.value(QStringLiteral("signature")).toString().trimmed();
            const QString description = optionObject.value(QStringLiteral("description")).toString().trimmed();
            const QString optionKey = optionObject.value(QStringLiteral("option_key")).toString().trimmed();
            const QString valueArity = canonicalOptionArityToken(
                optionObject.value(QStringLiteral("value_arity")).toString());
            const QString optionLine = description.isEmpty() ? signature : QStringLiteral("%1 = %2").arg(signature, description);
            appendUnique(entry.options, optionLine);
            const QStringList normalizedOptionKeys = extractOptionKeys(optionKey);
            const QStringList optionArgumentLabels = optionArgumentLabelsFromSignature(signature);
            const bool signatureHasEllipsis = signature.contains(QStringLiteral("..."));
            int fixedArity = -1;
            if (valueArity == QStringLiteral("NONE")) {
                fixedArity = 0;
            } else if (valueArity == QStringLiteral("EXACTLY_ONE")) {
                fixedArity = 1;
            } else if (valueArity == QStringLiteral("ONE_OR_MORE")
                       && !signatureHasEllipsis
                       && optionArgumentLabels.size() >= 2) {
                fixedArity = optionArgumentLabels.size();
            }
            QStringList normalizedOptionValues;
            for (const QString &normalizedOptionKey : normalizedOptionKeys) {
                appendUnique(entry.relatedKeywords, normalizedOptionKey);
                appendUnique(commandOptionTokens_[commandName], normalizedOptionKey);
                if (!valueArity.isEmpty()) {
                    commandOptionValueArityTokens_.insert(commandOptionValueKey(commandName, normalizedOptionKey), valueArity);
                }
                if (!optionArgumentLabels.isEmpty()) {
                    commandOptionArgumentLabelsByKey_.insert(commandOptionValueKey(commandName, normalizedOptionKey),
                                                             optionArgumentLabels);
                }
                if (fixedArity >= 0) {
                    commandOptionFixedArityByKey_.insert(commandOptionValueKey(commandName, normalizedOptionKey),
                                                         fixedArity);
                }
                registerCompletionToken(normalizedOptionKey);
            }
            const QJsonArray optionValues = optionObject.value(QStringLiteral("allowed_values")).toArray();
            for (const QJsonValue &optionValue : optionValues) {
                const QString normalizedValue = optionValue.toString().trimmed();
                appendUnique(normalizedOptionValues, normalizedValue);
            }
            QString optionHelpHtml;
            {
                QStringList html;
                html << QStringLiteral("<p><b>Option:</b> %1</p>").arg(signature.toHtmlEscaped());
                if (!description.isEmpty()) {
                    html << QStringLiteral("<p><b>Description:</b> %1</p>").arg(description.toHtmlEscaped());
                }
                if (!valueArity.isEmpty()) {
                    html << QStringLiteral("<p><b>Value Arity:</b> %1</p>").arg(valueArity.toHtmlEscaped());
                }
                if (!normalizedOptionValues.isEmpty()) {
                    html << QStringLiteral("<p><b>Accepted Values:</b> %1</p>")
                                .arg(normalizedOptionValues.join(QStringLiteral(", ")).toHtmlEscaped());
                }
                optionHelpHtml = html.join(QString());
            }
            if (!normalizedOptionValues.isEmpty()) {
                for (const QString &normalizedOptionKey : normalizedOptionKeys) {
                    appendUniqueList(commandOptionValueTokens_[commandOptionValueKey(commandName, normalizedOptionKey)],
                                     normalizedOptionValues);
                }
            }
            if (!optionHelpHtml.isEmpty()) {
                for (const QString &normalizedOptionKey : normalizedOptionKeys) {
                    commandOptionHelpHtmlByKey_.insert(commandOptionHelpKey(commandName, normalizedOptionKey), optionHelpHtml);
                }
            }
        }

        const QJsonArray allowedValuesArray = commandObject.value(QStringLiteral("allowed_values")).toArray();
        for (const QJsonValue &value : allowedValuesArray) {
            appendUnique(entry.acceptedValues, value.toString());
        }

        const QJsonArray typeValuesArray = commandObject.value(QStringLiteral("type_values")).toArray();
        for (const QJsonValue &value : typeValuesArray) {
            appendUnique(commandTypeValueTokens_[commandName], value.toString().trimmed().toLower());
        }

        const QJsonObject subtypeByTypeObject = commandObject.value(QStringLiteral("subtype_by_type")).toObject();
        for (auto subtypeIterator = subtypeByTypeObject.begin(); subtypeIterator != subtypeByTypeObject.end(); ++subtypeIterator) {
            const QString typeKey = subtypeIterator.key().trimmed().toLower();
            if (typeKey.isEmpty()) {
                continue;
            }

            const QJsonArray subtypeArray = subtypeIterator.value().toArray();
            for (const QJsonValue &subtypeValue : subtypeArray) {
                appendUnique(commandSubtypeByTypeTokens_[commandName][typeKey],
                             subtypeValue.toString().trimmed().toLower());
            }
        }

        appendUnique(entry.relatedKeywords, commandName);

        const QJsonArray contextsArray = commandObject.value(QStringLiteral("contexts")).toArray();
        QStringList normalizedCommandContexts;
        for (const QJsonValue &contextValue : contextsArray) {
            const QString contextToken = normalizeCompletionContext(contextValue.toString());
            if (contextToken.isEmpty()) {
                continue;
            }
            appendUnique(normalizedCommandContexts, contextToken);
            appendUnique(contextCommandTokens_[contextToken], commandName);
        }
        appendUniqueList(blockCommandContextsByKind_[normalizeDirective(commandName)], normalizedCommandContexts);

        QStringList inlineCommands;
        const QJsonArray inlineCommandArray = commandObject.value(QStringLiteral("inline_commands")).toArray();
        for (const QJsonValue &inlineCommandValue : inlineCommandArray) {
            appendUnique(inlineCommands, inlineCommandValue.toString());
        }
        if (!inlineCommands.isEmpty()) {
            QStringList targetContexts = normalizedCommandContexts;
            if (targetContexts.isEmpty()) {
                appendUnique(targetContexts, QStringLiteral("all"));
            }
            for (const QString &inlineCommand : inlineCommands) {
                const QString normalizedInlineCommand = normalizeDirective(inlineCommand);
                if (normalizedInlineCommand.isEmpty()) {
                    continue;
                }
                for (const QString &contextToken : targetContexts) {
                    appendUnique(contextCommandTokens_[contextToken], normalizedInlineCommand);
                }
                registerCompletionToken(inlineCommand);
            }
        }

        appendUnique(commandCompletionTokens_, commandName);
        commandRequiredPositionalCount_.insert(commandName, requiredPositionalCount);
        if (!commandArgumentSignatures.isEmpty()) {
            commandArgumentSignaturesByToken_.insert(commandName, commandArgumentSignatures);
        }
        commandPrimaryValueIsPerson_.insert(commandName, primaryValueIsPerson);
        registerCompletionToken(commandName);
        for (const QString &keyword : entry.relatedKeywords) {
            registerCompletionToken(keyword);
        }
        for (const QString &acceptedValue : entry.acceptedValues) {
            registerCompletionToken(acceptedValue);
            appendUnique(commandValueTokens_[commandName], acceptedValue);
        }

        mergeHelpEntry(commandName, entry);

        const QJsonArray aliasesArray = commandObject.value(QStringLiteral("aliases")).toArray();
        for (const QJsonValue &aliasValue : aliasesArray) {
            const QString alias = aliasValue.toString().trimmed().toLower();
            if (alias.isEmpty()) {
                continue;
            }
            appendUnique(commandCompletionTokens_, alias);
            commandRequiredPositionalCount_.insert(alias, requiredPositionalCount);
            appendUniqueList(commandArgumentSignaturesByToken_[alias], commandArgumentSignaturesByToken_.value(commandName));
            commandPrimaryValueIsPerson_.insert(alias, commandPrimaryValueIsPerson_.value(commandName));
            registerCompletionToken(alias);
            TherionHelpEntry aliasEntry = entry;
            appendUnique(aliasEntry.relatedKeywords, commandName);
            mergeHelpEntry(alias, aliasEntry);
            appendUniqueList(commandOptionTokens_[alias], commandOptionTokens_.value(commandName));
            appendUniqueList(commandValueTokens_[alias], commandValueTokens_.value(commandName));
            const QString argumentPrefix = commandName + QStringLiteral("\x1farg\x1f");
            QStringList commandArgumentKeys;
            for (auto argumentIterator = commandArgumentValueTokens_.cbegin();
                 argumentIterator != commandArgumentValueTokens_.cend();
                 ++argumentIterator) {
                if (argumentIterator.key().startsWith(argumentPrefix)) {
                    commandArgumentKeys.append(argumentIterator.key());
                }
            }
            for (const QString &key : commandArgumentKeys) {
                const QString suffix = key.mid(commandName.size());
                appendUniqueList(commandArgumentValueTokens_[alias + suffix], commandArgumentValueTokens_.value(key));
            }
            appendUniqueList(commandTypeValueTokens_[alias], commandTypeValueTokens_.value(commandName));
            for (const QString &optionKey : commandOptionTokens_.value(commandName)) {
                appendUniqueList(commandOptionValueTokens_[commandOptionValueKey(alias, optionKey)],
                                 commandOptionValueTokens_.value(commandOptionValueKey(commandName, optionKey)));
                const QString key = commandOptionValueKey(commandName, optionKey);
                const QString aliasKey = commandOptionValueKey(alias, optionKey);
                const QString valueArity = commandOptionValueArityTokens_.value(key);
                if (!valueArity.isEmpty()) {
                    commandOptionValueArityTokens_.insert(aliasKey, valueArity);
                }
                const QStringList optionArgumentLabels = commandOptionArgumentLabelsByKey_.value(key);
                if (!optionArgumentLabels.isEmpty()) {
                    commandOptionArgumentLabelsByKey_.insert(aliasKey, optionArgumentLabels);
                }
                const int fixedArity = commandOptionFixedArityByKey_.value(key, -1);
                if (fixedArity >= 0) {
                    commandOptionFixedArityByKey_.insert(aliasKey, fixedArity);
                }
                const QString optionHelpHtml = commandOptionHelpHtmlByKey_.value(commandOptionHelpKey(commandName, optionKey));
                if (!optionHelpHtml.isEmpty()) {
                    commandOptionHelpHtmlByKey_.insert(commandOptionHelpKey(alias, optionKey), optionHelpHtml);
                }
            }
            const QHash<QString, QStringList> subtypeByType = commandSubtypeByTypeTokens_.value(commandName);
            for (auto subtypeIterator = subtypeByType.begin(); subtypeIterator != subtypeByType.end(); ++subtypeIterator) {
                appendUniqueList(commandSubtypeByTypeTokens_[alias][subtypeIterator.key()], subtypeIterator.value());
            }
            for (auto contextIterator = contextCommandTokens_.begin(); contextIterator != contextCommandTokens_.end(); ++contextIterator) {
                if (contextIterator.value().contains(commandName, Qt::CaseInsensitive)) {
                    appendUnique(contextIterator.value(), alias);
                }
            }
            appendUniqueList(blockCommandContextsByKind_[normalizeDirective(alias)], normalizedCommandContexts);
        }
    }
}

void TextEditorTab::mergeHelpEntry(const QString &token, const TherionHelpEntry &entry)
{
    if (token.trimmed().isEmpty()) {
        return;
    }

    const QString normalizedToken = token.toLower();
    TherionHelpEntry merged = helpEntries_.value(normalizedToken);
    if (merged.summary.trimmed().isEmpty() && !entry.summary.trimmed().isEmpty()) {
        merged.summary = entry.summary.trimmed();
    }
    if (merged.syntax.trimmed().isEmpty() && !entry.syntax.trimmed().isEmpty()) {
        merged.syntax = entry.syntax.trimmed();
    }
    appendUniqueList(merged.arguments, entry.arguments);
    appendUniqueList(merged.acceptedValues, entry.acceptedValues);
    appendUniqueList(merged.options, entry.options);
    appendUniqueList(merged.relatedKeywords, entry.relatedKeywords);
    helpEntries_.insert(normalizedToken, merged);
}

void TextEditorTab::registerCompletionToken(const QString &token)
{
    const QString normalized = token.trimmed();
    if (normalized.isEmpty()) {
        return;
    }
    if (normalized.startsWith(QLatin1Char('<')) && normalized.endsWith(QLatin1Char('>'))) {
        return;
    }
    if (normalized.contains(QLatin1Char(' '))) {
        return;
    }
    if (completionTokens_.contains(normalized, Qt::CaseInsensitive)) {
        return;
    }
    completionTokens_.append(normalized);
}

void TextEditorTab::rebuildCompletionModel()
{
    if (completionModel_ == nullptr) {
        return;
    }

    QStringList sortedTokens = completionTokens_;
    std::sort(sortedTokens.begin(),
              sortedTokens.end(),
              [](const QString &a, const QString &b) {
                  return QString::compare(a, b, Qt::CaseInsensitive) < 0;
              });
    completionModel_->setStringList(sortedTokens);
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

    if (event == nullptr || completionCompleter_ == nullptr || editor_ == nullptr) {
        return QWidget::eventFilter(watched, event);
    }
    const QObject *popupObject = completionCompleter_->popup();
    const bool watchedEditor = watched == editor_ || watched == editor_->viewport();
    const bool watchedPopup = popupObject != nullptr && watched == popupObject;
    if (!watchedEditor && !watchedPopup) {
        return QWidget::eventFilter(watched, event);
    }

    if (watchedEditor
        && (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonDblClick)) {
        if (completionCompleter_->popup() != nullptr) {
            completionCompleter_->popup()->hide();
        }

        if ((event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
            && watched == editor_->viewport()) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                editor_->setFocus(Qt::MouseFocusReason);
                editor_->setTextCursor(editor_->cursorForPosition(mouseEvent->position().toPoint()));
            }
        }

        return QWidget::eventFilter(watched, event);
    }

    if (event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (completionCompleter_->popup() != nullptr && completionCompleter_->popup()->isVisible()) {
        auto *popup = completionCompleter_->popup();
        if (watchedPopup) {
            const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
            const bool noModifiers = modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier;
            const QString text = keyEvent->text();
            const bool typedText = !text.isEmpty() && noModifiers && text.at(0).isPrint();
            if (typedText) {
                editor_->setFocus();
                editor_->insertPlainText(text);
                QTimer::singleShot(0, this, [this]() {
                    triggerCompletionPopup();
                });
                return true;
            }
            if (keyEvent->key() == Qt::Key_Backspace) {
                editor_->setFocus();
                QTextCursor cursor = editor_->textCursor();
                cursor.deletePreviousChar();
                editor_->setTextCursor(cursor);
                QTimer::singleShot(0, this, [this]() {
                    triggerCompletionPopup();
                });
                return true;
            }
        }

        switch (keyEvent->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
        {
            QString completion;
            const auto currentIndex = popup->currentIndex();
            if (currentIndex.isValid()) {
                completion = currentIndex.data(Qt::DisplayRole).toString().trimmed();
            }
            if (completion.isEmpty()) {
                completion = completionCompleter_->currentCompletion().trimmed();
            }
            if (!completion.isEmpty()) {
                insertCompletionToken(completion);
            }
            popup->hide();
            return true;
        }
        case Qt::Key_Escape:
            popup->hide();
            return true;
        default:
            break;
        }
    }

    if (!watchedEditor) {
        return QWidget::eventFilter(watched, event);
    }

    const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
    if (keyEvent->key() == Qt::Key_Tab && modifiers == Qt::NoModifier) {
        QTextCursor cursor = editor_->textCursor();
        cursor.insertText(QStringLiteral("    "));
        editor_->setTextCursor(cursor);
        return true;
    }
    const bool controlSpace = keyEvent->key() == Qt::Key_Space
        && modifiers.testFlag(Qt::ControlModifier)
        && (modifiers & ~(Qt::ControlModifier)) == Qt::NoModifier;
    if (!controlSpace) {
        const bool noModifiers = modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier;
        if (!noModifiers) {
            return QWidget::eventFilter(watched, event);
        }

        const int key = keyEvent->key();
        const bool triggerKey = key == Qt::Key_Backspace
            || key == Qt::Key_Delete
            || key == Qt::Key_Minus
            || key == Qt::Key_Underscore
            || key == Qt::Key_Period
            || key == Qt::Key_Slash
            || key == Qt::Key_Apostrophe
            || key == Qt::Key_QuoteDbl
            || (key >= Qt::Key_A && key <= Qt::Key_Z)
            || (key >= Qt::Key_0 && key <= Qt::Key_9);

        const bool hideKey = key == Qt::Key_Space
            || key == Qt::Key_Tab
            || key == Qt::Key_Backtab
            || key == Qt::Key_Return
            || key == Qt::Key_Enter
            || key == Qt::Key_Escape;

        if (hideKey && completionCompleter_->popup() != nullptr) {
            completionCompleter_->popup()->hide();
            return QWidget::eventFilter(watched, event);
        }

        if (!triggerKey) {
            return QWidget::eventFilter(watched, event);
        }

        const bool deletionKey = key == Qt::Key_Backspace || key == Qt::Key_Delete;
        if (deletionKey) {
            const QTextCursor cursor = editor_->textCursor();
            const QTextBlock block = cursor.block();
            if (block.isValid()) {
                const QString blockText = block.text();
                const int cursorColumn = cursor.positionInBlock();
                auto isCompletionCharacter = [](const QChar ch) {
                    return ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_');
                };

                const bool leftTokenChar = cursorColumn > 0 && isCompletionCharacter(blockText.at(cursorColumn - 1));
                const bool rightTokenChar = cursorColumn < blockText.size() && isCompletionCharacter(blockText.at(cursorColumn));
                if (!leftTokenChar && !rightTokenChar) {
                    if (completionCompleter_->popup() != nullptr) {
                        completionCompleter_->popup()->hide();
                    }
                    return QWidget::eventFilter(watched, event);
                }
            }
        }

        QTimer::singleShot(0, this, [this]() {
            triggerCompletionPopup();
        });
        return QWidget::eventFilter(watched, event);
    }

    triggerCompletionPopup();
    return true;
}

QString TextEditorTab::currentCompletionPrefix() const
{
    if (editor_ == nullptr) {
        return QString();
    }

    const QTextCursor cursor = editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QString();
    }

    const QString blockText = block.text();
    int start = cursor.positionInBlock();
    int end = cursor.positionInBlock();

    auto isCompletionCharacter = [](QChar ch) {
        return ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_');
    };

    while (start > 0 && isCompletionCharacter(blockText.at(start - 1))) {
        --start;
    }
    while (end < blockText.length() && isCompletionCharacter(blockText.at(end))) {
        ++end;
    }

    return blockText.mid(start, end - start).trimmed();
}

QString TextEditorTab::normalizeCompletionContext(const QString &contextToken) const
{
    return normalizedCompletionContextToken(contextToken);
}

bool TextEditorTab::isCompatibleChildKindForBlocks(const QString &parentKind, const QString &childKind) const
{
    const QString normalizedParent = normalizeDirective(parentKind.trimmed().toLower());
    const QString normalizedChild = normalizeDirective(childKind.trimmed().toLower());
    if (normalizedChild.isEmpty()) {
        return false;
    }
    if (normalizedChild == QStringLiteral("encoding") || normalizedParent == QStringLiteral("encoding")) {
        return false;
    }
    if (normalizedChild == QStringLiteral("comment")) {
        return true;
    }

    const QString parentContext = normalizedParent.isEmpty()
        ? QStringLiteral("none")
        : normalizeCompletionContext(normalizedParent);
    const QStringList childContexts = blockCommandContextsByKind_.value(normalizedChild);
    if (!childContexts.isEmpty()) {
        if (childContexts.contains(QStringLiteral("all"), Qt::CaseInsensitive)) {
            return true;
        }
        if (!parentContext.isEmpty() && childContexts.contains(parentContext, Qt::CaseInsensitive)) {
            return true;
        }
        return false;
    }

    return normalizedParent.isEmpty();
}

bool TextEditorTab::isCommandDirectiveInScope(const QString &directive, const QString &scopeToken) const
{
    const QString normalizedDirective = normalizeDirective(directive.trimmed());
    if (normalizedDirective.isEmpty()) {
        return false;
    }

    QString normalizedScope = normalizeCompletionContext(scopeToken);
    if (normalizedScope.isEmpty()) {
        normalizedScope = QStringLiteral("none");
    }

    QStringList candidates = contextCommandTokens_.value(normalizedScope);
    appendUniqueList(candidates, contextCommandTokens_.value(QStringLiteral("all")));
    if (normalizedScope == QStringLiteral("none")) {
        appendUniqueList(candidates, contextCommandTokens_.value(QStringLiteral("none")));
    }
    return candidates.contains(normalizedDirective, Qt::CaseInsensitive);
}

QStringList TextEditorTab::commandArgumentSignaturesFor(const QString &commandToken) const
{
    return commandArgumentSignaturesByToken_.value(normalizeDirective(commandToken.trimmed()));
}

bool TextEditorTab::commandHasRequiredIdArgument(const QString &commandToken) const
{
    const QStringList signatures = commandArgumentSignaturesFor(commandToken);
    for (const QString &signature : signatures) {
        if (!isRequiredArgumentSignature(signature)) {
            continue;
        }
        const QString normalizedSignature = signature.trimmed().toLower();
        return normalizedSignature.contains(QStringLiteral("<id>"));
    }
    return false;
}

bool TextEditorTab::commandSupportsInlineIdField(const QString &commandToken) const
{
    const QString normalizedCommand = normalizeDirective(commandToken.trimmed());
    if (commandHasRequiredIdArgument(normalizedCommand)) {
        return true;
    }
    return commandOptionTokens_.value(normalizedCommand).contains(QStringLiteral("-id"), Qt::CaseInsensitive);
}

QString TextEditorTab::primaryInsertionScopeForCommand(const QString &commandToken) const
{
    const QString normalizedCommand = normalizeDirective(commandToken.trimmed());
    const QStringList contexts = blockCommandContextsByKind_.value(normalizedCommand);
    for (const QString &context : contexts) {
        const QString normalizedContext = normalizeCompletionContext(context);
        if (normalizedContext.isEmpty()
            || normalizedContext == QStringLiteral("all")
            || normalizedContext == QStringLiteral("none")) {
            continue;
        }
        return normalizedContext;
    }
    return QString();
}

QString TextEditorTab::resolveScopeForCommandAtLine(const QString &commandToken,
                                                    const QStringList &lines,
                                                    int lineNumber) const
{
    const QString normalizedCommand = normalizeDirective(commandToken.trimmed());
    if (normalizedCommand.isEmpty()) {
        return QString();
    }

    const QString preferredScope = primaryInsertionScopeForCommand(normalizedCommand);
    if (!preferredScope.isEmpty()) {
        return preferredScope;
    }

    const int lastLine = qBound(0, lineNumber - 1, lines.size());
    QStringList scopeStack;
    for (int lineIndex = 0; lineIndex < lastLine; ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        const QString directive = normalizeDirective(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }

        const QString openingFromClosing = completionOpeningDirectiveForClosing(directive);
        if (!openingFromClosing.isEmpty()) {
            for (int stackIndex = scopeStack.size() - 1; stackIndex >= 0; --stackIndex) {
                if (scopeStack.at(stackIndex) == openingFromClosing) {
                    scopeStack.removeAt(stackIndex);
                    break;
                }
            }
            continue;
        }

        if (isContainerDirectiveInstance(directive, parsedLine)) {
            scopeStack.append(directive);
        }
    }

    for (int stackIndex = scopeStack.size() - 1; stackIndex >= 0; --stackIndex) {
        const QString scopeDirective = scopeStack.at(stackIndex);
        if (isCommandDirectiveInScope(normalizedCommand, scopeDirective)) {
            return scopeDirective;
        }
    }

    if (isCommandDirectiveInScope(normalizedCommand, QStringLiteral("none"))) {
        return QStringLiteral("none");
    }

    return QString();
}

QStringList TextEditorTab::activeCompletionScopeStack() const
{
    QStringList scopeStack;
    if (editor_ == nullptr) {
        return scopeStack;
    }

    const QTextCursor cursor = editor_->textCursor();
    const int currentBlockNumber = cursor.block().blockNumber();
    if (currentBlockNumber <= 0) {
        return scopeStack;
    }

    const QStringList lines = editor_->toPlainText().split(QLatin1Char('\n'));
    const int lastLine = qMin(currentBlockNumber, lines.size());
    for (int lineIndex = 0; lineIndex < lastLine; ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        if (parsedLine.directive.isEmpty()) {
            continue;
        }

        const QString directive = normalizeDirective(parsedLine.directive);
        const QString openingFromClosing = completionOpeningDirectiveForClosing(directive);
        if (!openingFromClosing.isEmpty()) {
            for (int stackIndex = scopeStack.size() - 1; stackIndex >= 0; --stackIndex) {
                if (scopeStack.at(stackIndex) == openingFromClosing) {
                    scopeStack.removeAt(stackIndex);
                    break;
                }
            }
            continue;
        }

        if (isContainerDirectiveInstance(directive, parsedLine)) {
            scopeStack.append(directive);
        }
    }

    return scopeStack;
}

QString TextEditorTab::currentCompletionCommand() const
{
    if (editor_ == nullptr) {
        return QString();
    }

    const QTextCursor cursor = editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QString();
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString directive = normalizeDirective(parsedLine.directive.toLower());
    if (commandCompletionTokens_.contains(directive, Qt::CaseInsensitive)
        || commandOptionTokens_.contains(directive)
        || commandValueTokens_.contains(directive)) {
        return directive;
    }

    const QStringList scopeStack = activeCompletionScopeStack();
    for (int index = scopeStack.size() - 1; index >= 0; --index) {
        const QString scopeDirective = scopeStack.at(index);
        if (commandCompletionTokens_.contains(scopeDirective, Qt::CaseInsensitive)) {
            return scopeDirective;
        }
    }
    return QString();
}

QString TextEditorTab::currentCompletionScopeLabel() const
{
    const QStringList scopeStack = activeCompletionScopeStack();
    if (scopeStack.isEmpty()) {
        return tr("top-level");
    }

    return scopeStack.constLast();
}

QStringList TextEditorTab::projectInputFileCompletionCandidates() const
{
    QString rootPath = projectRootPath_.trimmed();
    if (rootPath.isEmpty()) {
        if (filePath_.isEmpty()) {
            return {};
        }
        rootPath = QFileInfo(filePath_).absolutePath();
    }

    QDir rootDir(rootPath);
    if (!rootDir.exists()) {
        return {};
    }

    QString rootBasePath = QFileInfo(rootDir.absolutePath()).canonicalFilePath();
    if (rootBasePath.isEmpty()) {
        rootBasePath = rootDir.absolutePath();
    }
    QDir rootBaseDir(rootBasePath);

    QString baseDirPath = rootBasePath;
    if (!filePath_.isEmpty()) {
        const QFileInfo currentFileInfo(filePath_);
        QString candidateBasePath = currentFileInfo.absolutePath();
        const QString canonicalCandidateBasePath = QFileInfo(candidateBasePath).canonicalFilePath();
        if (!canonicalCandidateBasePath.isEmpty()) {
            candidateBasePath = canonicalCandidateBasePath;
        }
        if (QDir(candidateBasePath).exists()) {
            baseDirPath = candidateBasePath;
        }
    }

    QDir baseDir(baseDirPath);

    QStringList candidates;
    QDirIterator iterator(rootBaseDir.absolutePath(),
                          QDir::Files | QDir::Readable | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString absolutePath = iterator.next();
        const QFileInfo fileInfo(absolutePath);
        const QString suffix = fileInfo.suffix().trimmed().toLower();
        if (suffix != QStringLiteral("th") && suffix != QStringLiteral("th2")) {
            continue;
        }

        QString candidatePath = fileInfo.absoluteFilePath();
        const QString canonicalCandidatePath = fileInfo.canonicalFilePath();
        if (!canonicalCandidatePath.isEmpty()) {
            candidatePath = canonicalCandidatePath;
        }

        const QString projectRelativePath = QDir::fromNativeSeparators(rootBaseDir.relativeFilePath(candidatePath)).trimmed();
        if (projectRelativePath.isEmpty() || projectRelativePath.startsWith(QStringLiteral("../"))) {
            continue;
        }

        const QString relativePath = normalizeInputSuggestionPath(baseDir.relativeFilePath(candidatePath));
        if (relativePath.isEmpty()) {
            continue;
        }
        appendUnique(candidates, relativePath);
    }

    return candidates;
}

QStringList TextEditorTab::buildCompletionSuggestionsForCursor(const QString &prefix) const
{
    if (editor_ == nullptr) {
        return QStringList();
    }

    const QTextCursor cursor = editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QStringList();
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
    const int column = cursor.positionInBlock();

    int tokenIndexAtCursor = parsedLine.tokens.size();
    bool cursorInsideToken = false;
    int tokenCounter = 0;
    for (const TherionParsedToken &tokenSpan : parsedLine.tokenSpans) {
        if (tokenSpan.type == TherionTokenType::Comment) {
            continue;
        }
        const int tokenEnd = tokenSpan.start + tokenSpan.length;
        if (column >= tokenSpan.start && column <= tokenEnd) {
            tokenIndexAtCursor = tokenCounter;
            cursorInsideToken = true;
            break;
        }
        if (column > tokenEnd) {
            tokenIndexAtCursor = tokenCounter + 1;
        }
        ++tokenCounter;
    }

    const QString currentToken = cursorInsideToken && tokenIndexAtCursor < parsedLine.tokens.size()
        ? parsedLine.tokens.at(tokenIndexAtCursor)
        : QString();
    const QString previousToken = tokenIndexAtCursor > 0 && tokenIndexAtCursor - 1 < parsedLine.tokens.size()
        ? parsedLine.tokens.at(tokenIndexAtCursor - 1)
        : QString();
    QString effectivePrefix = !prefix.isEmpty()
        ? prefix
        : (cursorInsideToken ? currentToken.trimmed() : QString());

    const QString command = currentCompletionCommand();
    int tokenStart = column;
    int tokenEnd = column;
    const QString blockText = block.text();
    while (tokenStart > 0 && !blockText.at(tokenStart - 1).isSpace()) {
        --tokenStart;
    }
    while (tokenEnd < blockText.length() && !blockText.at(tokenEnd).isSpace()) {
        ++tokenEnd;
    }
    const QString rawTokenAtCursor = blockText.mid(tokenStart, tokenEnd - tokenStart).trimmed();

    if (command == QStringLiteral("input") && cursorInsideToken) {
        const QString tokenPrefix = currentToken.trimmed();
        if (!tokenPrefix.isEmpty()) {
            effectivePrefix = tokenPrefix;
        }
    }
    QStringList candidates;
    bool allowGlobalFallback = true;
    bool inputFileContext = false;
    struct ActiveValueContext
    {
        bool active = false;
        QString optionToken;
        QString arity;
        int optionIndex = -1;
        int nextOptionIndex = -1;
    };
    ActiveValueContext activeValueContext;

    if (tokenIndexAtCursor <= 0 && !effectivePrefix.startsWith(QLatin1Char('-'))) {
        const QStringList scopeStack = activeCompletionScopeStack();
        const QString activeScope = scopeStack.isEmpty() ? QStringLiteral("none") : scopeStack.constLast();
        const bool strictScopedCommandContext = !scopeStack.isEmpty();

        if (scopeStack.isEmpty()) {
            appendUniqueList(candidates, contextCommandTokens_.value(QStringLiteral("none")));
            appendUniqueList(candidates, contextCommandTokens_.value(QStringLiteral("all")));
        } else {
            appendUniqueList(candidates, contextCommandTokens_.value(activeScope));
            appendUniqueList(candidates, contextCommandTokens_.value(QStringLiteral("all")));
        }

        if (candidates.isEmpty() && !strictScopedCommandContext) {
            candidates = commandCompletionTokens_;
        }
    } else if (!command.isEmpty()) {
        const bool optionContext = currentToken.startsWith(QLatin1Char('-'))
            || effectivePrefix.startsWith(QLatin1Char('-'))
            || (previousToken.startsWith(QLatin1Char('-')) && effectivePrefix.startsWith(QLatin1Char('-')));
        if (!optionContext) {
            const QString normalizedPreviousToken = previousToken.trimmed().toLower();
            if (normalizedPreviousToken.startsWith(QLatin1Char('-'))) {
                activeValueContext.active = true;
                activeValueContext.optionToken = normalizedPreviousToken;
                activeValueContext.arity = commandOptionValueArityTokens_
                    .value(commandOptionValueKey(command, activeValueContext.optionToken))
                    .trimmed()
                    .toUpper();
            } else {
                const int scanTokenIndex = cursorInsideToken ? tokenIndexAtCursor : tokenIndexAtCursor - 1;
                if (scanTokenIndex >= 1 && scanTokenIndex < parsedLine.tokens.size()) {
                    int optionIndex = -1;
                    for (int index = scanTokenIndex; index >= 1; --index) {
                        const QString token = parsedLine.tokens.at(index).trimmed().toLower();
                        if (token.startsWith(QLatin1Char('-'))) {
                            optionIndex = index;
                            break;
                        }
                    }

                    if (optionIndex >= 1) {
                        int nextOptionIndex = parsedLine.tokens.size();
                        for (int index = optionIndex + 1; index < parsedLine.tokens.size(); ++index) {
                            const QString token = parsedLine.tokens.at(index).trimmed();
                            if (token.startsWith(QLatin1Char('-'))) {
                                nextOptionIndex = index;
                                break;
                            }
                        }

                        const QString optionToken = parsedLine.tokens.at(optionIndex).trimmed().toLower();
                        const QString arity = commandOptionValueArityTokens_
                            .value(commandOptionValueKey(command, optionToken))
                            .trimmed()
                            .toUpper();

                        const bool cursorWithinValueRange = tokenIndexAtCursor > optionIndex
                            && tokenIndexAtCursor <= nextOptionIndex
                            && !(cursorInsideToken && currentToken.trimmed().startsWith(QLatin1Char('-')));
                        if (cursorWithinValueRange) {
                            const int valueOrdinal = tokenIndexAtCursor - optionIndex;
                            const QString normalizedArity = canonicalOptionArityToken(arity);
                            const bool multiValueOption = normalizedArity == QStringLiteral("ONE_OR_MORE")
                                || normalizedArity == QStringLiteral("ZERO_OR_MORE");
                            const bool singleValueOption = normalizedArity == QStringLiteral("EXACTLY_ONE");
                            if (multiValueOption || (singleValueOption && valueOrdinal == 1)) {
                                activeValueContext.active = true;
                                activeValueContext.optionToken = optionToken;
                                activeValueContext.arity = arity;
                                activeValueContext.optionIndex = optionIndex;
                                activeValueContext.nextOptionIndex = nextOptionIndex;
                            }
                        }
                    }
                }
            }
        }
        const bool valueContext = activeValueContext.active;
        inputFileContext = command == QStringLiteral("input") && !optionContext;

        if (inputFileContext) {
            allowGlobalFallback = false;
            if (isActivatedInputPathPrefix(rawTokenAtCursor)) {
                candidates = projectInputFileCompletionCandidates();
            }
        } else if (optionContext) {
            allowGlobalFallback = false;
            candidates = commandOptionTokens_.value(command);
            QSet<QString> usedOptionTokens;
            for (const QString &lineToken : parsedLine.tokens) {
                const QString normalizedLineToken = lineToken.trimmed().toLower();
                if (normalizedLineToken.startsWith(QLatin1Char('-'))) {
                    usedOptionTokens.insert(normalizedLineToken);
                }
            }

            const QString activeOptionToken = cursorInsideToken && currentToken.trimmed().startsWith(QLatin1Char('-'))
                ? currentToken.trimmed().toLower()
                : QString();
            QStringList optionCandidates;
            for (const QString &candidate : candidates) {
                if (!candidate.startsWith(QLatin1Char('-'))) {
                    continue;
                }

                const QString normalizedCandidate = candidate.trimmed().toLower();
                if (usedOptionTokens.contains(normalizedCandidate) && normalizedCandidate != activeOptionToken) {
                    continue;
                }

                if (candidate.startsWith(QLatin1Char('-'))) {
                    appendUnique(optionCandidates, candidate);
                }
            }
            candidates = optionCandidates;
        } else if (valueContext) {
            allowGlobalFallback = false;
            const QString valueOptionToken = activeValueContext.optionToken;
            if (valueOptionToken == QStringLiteral("-subtype")) {
                const QString symbolTypeToken = symbolTypeForSubtypeLookup(command, parsedLine);
                const QHash<QString, QStringList> subtypeByType = commandSubtypeByTypeTokens_.value(command);

                if (!symbolTypeToken.isEmpty()) {
                    appendConcreteSubtypeValues(candidates, subtypeByType.value(symbolTypeToken));
                }

                if (candidates.isEmpty()) {
                    for (auto subtypeIterator = subtypeByType.begin(); subtypeIterator != subtypeByType.end(); ++subtypeIterator) {
                        appendConcreteSubtypeValues(candidates, subtypeIterator.value());
                    }
                }
            }

            if (candidates.isEmpty()) {
                candidates = commandOptionValueTokens_.value(commandOptionValueKey(command, valueOptionToken));
            }

            if (canonicalOptionArityToken(activeValueContext.arity) == QStringLiteral("ONE_OR_MORE")
                && activeValueContext.optionIndex >= 0
                && !candidates.isEmpty()) {
                QSet<QString> usedValues;
                for (int index = activeValueContext.optionIndex + 1; index < activeValueContext.nextOptionIndex; ++index) {
                    if (cursorInsideToken && index == tokenIndexAtCursor) {
                        continue;
                    }
                    usedValues.insert(parsedLine.tokens.at(index).trimmed().toLower());
                }

                QStringList filteredCandidates;
                for (const QString &candidate : candidates) {
                    if (!usedValues.contains(candidate.trimmed().toLower())) {
                        appendUnique(filteredCandidates, candidate);
                    }
                }
                candidates = filteredCandidates;
            }
        } else {
            const int requiredPositionalCount = qMax(0, commandRequiredPositionalCount_.value(command));
            const int positionalCountBeforeCursor = positionalTokenCountBeforeCursor(parsedLine, tokenIndexAtCursor);
            if (requiredPositionalCount > 0 && positionalCountBeforeCursor < requiredPositionalCount) {
                allowGlobalFallback = false;
                candidates = commandValueTokens_.value(command);
            } else {
                candidates = commandValueTokens_.value(command);
                appendUniqueList(candidates, commandOptionTokens_.value(command));
            }
        }
    }

    if (candidates.isEmpty() && allowGlobalFallback) {
        candidates = completionTokens_;
    }

    QStringList filtered;
    QString inputPathPrefix;
    QString normalizedInputPathPrefix;
    if (inputFileContext) {
        const QString rawInputPrefix = rawTokenAtCursor;
        inputPathPrefix = QDir::fromNativeSeparators(rawInputPrefix);
        normalizedInputPathPrefix = normalizeInputCompletionPrefix(rawInputPrefix);
    }
    const bool sameDirectoryHint = inputFileContext && inputPathPrefix.startsWith(QStringLiteral("./"));
    const bool treatAsEmptyInputPrefix = inputFileContext
        && !inputPathPrefix.isEmpty()
        && normalizedInputPathPrefix.isEmpty();
    for (const QString &candidate : candidates) {
        if (inputFileContext && sameDirectoryHint && candidate.startsWith(QStringLiteral("../"))) {
            continue;
        }

        const bool matchesDefaultPrefix = treatAsEmptyInputPrefix
            || effectivePrefix.isEmpty()
            || candidate.startsWith(effectivePrefix, Qt::CaseInsensitive);
        const bool matchesNormalizedInputPrefix = inputFileContext
            && !normalizedInputPathPrefix.isEmpty()
            && candidate.startsWith(normalizedInputPathPrefix, Qt::CaseInsensitive);
        if (matchesDefaultPrefix || matchesNormalizedInputPrefix) {
            appendUnique(filtered, candidate);
        }
    }

    std::sort(filtered.begin(),
              filtered.end(),
              [](const QString &a, const QString &b) {
                  return QString::compare(a, b, Qt::CaseInsensitive) < 0;
              });

    return filtered;
}

void TextEditorTab::triggerCompletionPopup()
{
    if (editor_ == nullptr || completionCompleter_ == nullptr || completionModel_ == nullptr) {
        return;
    }
    const QString command = currentCompletionCommand();

    QString rawPathPrefix;
    if (command == QStringLiteral("input")) {
        const QTextCursor cursor = editor_->textCursor();
        const QTextBlock block = cursor.block();
        if (block.isValid()) {
            const QString blockText = block.text();
            int start = cursor.positionInBlock();
            int end = cursor.positionInBlock();
            while (start > 0 && !blockText.at(start - 1).isSpace()) {
                --start;
            }
            while (end < blockText.length() && !blockText.at(end).isSpace()) {
                ++end;
            }
            rawPathPrefix = QDir::fromNativeSeparators(blockText.mid(start, end - start).trimmed());
        }
    }

    const QString prefix = currentCompletionPrefix();
    QStringList suggestions = buildCompletionSuggestionsForCursor(prefix);
    if (command == QStringLiteral("input") && rawPathPrefix.startsWith(QStringLiteral("./"))) {
        QStringList sameDirectorySuggestions;
        for (const QString &candidate : suggestions) {
            if (!candidate.startsWith(QStringLiteral("../"))) {
                appendUnique(sameDirectorySuggestions, candidate);
            }
        }
        suggestions = sameDirectorySuggestions;
    }
    if (suggestions.isEmpty()) {
        if (completionCompleter_->popup() != nullptr) {
            completionCompleter_->popup()->hide();
        }
        const int requiredPositionalCount = qMax(0, commandRequiredPositionalCount_.value(command));
        if (!command.isEmpty() && requiredPositionalCount > 0) {
            const QTextCursor cursor = editor_->textCursor();
            const QTextBlock block = cursor.block();
            if (block.isValid()) {
                const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
                const int column = cursor.positionInBlock();

                int tokenIndexAtCursor = parsedLine.tokens.size();
                int tokenCounter = 0;
                for (const TherionParsedToken &tokenSpan : parsedLine.tokenSpans) {
                    if (tokenSpan.type == TherionTokenType::Comment) {
                        continue;
                    }
                    const int tokenEnd = tokenSpan.start + tokenSpan.length;
                    if (column >= tokenSpan.start && column <= tokenEnd) {
                        tokenIndexAtCursor = tokenCounter;
                        break;
                    }
                    if (column > tokenEnd) {
                        tokenIndexAtCursor = tokenCounter + 1;
                    }
                    ++tokenCounter;
                }

                const int positionalCountBeforeCursor = positionalTokenCountBeforeCursor(parsedLine, tokenIndexAtCursor);
                if (positionalCountBeforeCursor < requiredPositionalCount) {
                    const int nextArgumentIndex = positionalCountBeforeCursor + 1;
                    QToolTip::showText(editor_->mapToGlobal(editor_->cursorRect().bottomLeft()),
                                       tr("Enter required argument %1 before options.").arg(nextArgumentIndex),
                                       editor_,
                                       QRect(),
                                       1500);
                }
            }
        }
        return;
    }

    completionModel_->setStringList(suggestions);
    QString popupPrefix = prefix;
    if (command == QStringLiteral("input")) {
        popupPrefix = normalizeInputCompletionPrefix(rawPathPrefix);
    }
    completionCompleter_->setCompletionPrefix(popupPrefix);
    if (completionCompleter_->completionCount() <= 0) {
        return;
    }

    QRect cursorRect = editor_->cursorRect();
    if (completionCompleter_->popup() != nullptr) {
        const int popupWidth = completionCompleter_->popup()->sizeHintForColumn(0)
            + completionCompleter_->popup()->verticalScrollBar()->sizeHint().width() + 12;
        cursorRect.setWidth(qMax(cursorRect.width(), popupWidth));
    }
    completionCompleter_->complete(cursorRect);

    const QString scopeLabel = currentCompletionScopeLabel();
    if (!scopeLabel.isEmpty()) {
        QToolTip::showText(editor_->mapToGlobal(cursorRect.bottomLeft()),
                           tr("Completion scope: %1").arg(scopeLabel),
                           editor_,
                           QRect(),
                           1500);
    }
}

void TextEditorTab::insertCompletionToken(const QString &completion)
{
    if (editor_ == nullptr || completion.trimmed().isEmpty()) {
        return;
    }

    QTextCursor cursor = editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return;
    }

    const QString blockText = block.text();
    int start = cursor.positionInBlock();
    int end = cursor.positionInBlock();

    auto isCompletionCharacter = [](QChar ch) {
        return ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_');
    };

    while (start > 0 && isCompletionCharacter(blockText.at(start - 1))) {
        --start;
    }
    while (end < blockText.length() && isCompletionCharacter(blockText.at(end))) {
        ++end;
    }

    const QString normalizedCompletion = normalizeDirective(completion.toLower());
    const QString closingDirective = completionClosingDirectiveForOpening(normalizedCompletion);
    const QString leftTrimmed = blockText.left(start).trimmed();
    const QString rightTrimmed = blockText.mid(end).trimmed();
    const bool firstTokenOnlyLine = leftTrimmed.isEmpty() && rightTrimmed.isEmpty();
    bool shouldInsertClosingPair = !closingDirective.isEmpty() && firstTokenOnlyLine;

    QString lineIndent;
    for (int index = 0; index < blockText.length(); ++index) {
        const QChar ch = blockText.at(index);
        if (!ch.isSpace() || ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
            break;
        }
        lineIndent.append(ch);
    }

    if (shouldInsertClosingPair) {
        const QTextBlock nextBlock = block.next();
        if (nextBlock.isValid()) {
            const QString nextText = nextBlock.text().trimmed().toLower();
            if (normalizeDirective(nextText) == closingDirective) {
                shouldInsertClosingPair = false;
            }
        }
    }

    cursor.beginEditBlock();
    cursor.setPosition(block.position() + start);
    cursor.setPosition(block.position() + end, QTextCursor::KeepAnchor);
    cursor.insertText(completion);
    const int completionEndPos = cursor.position();

    if (shouldInsertClosingPair) {
        cursor.insertText(QStringLiteral("\n%1%2").arg(lineIndent, closingDirective));
        cursor.setPosition(completionEndPos);
    }

    cursor.endEditBlock();
    editor_->setTextCursor(cursor);
}

QStringList TextEditorTab::helpCandidateTokens() const
{
    QStringList candidates;
    const QString directToken = currentHelpTokenForCursor();
    if (!directToken.isEmpty()) {
        appendUnique(candidates, directToken);
    }

    const QTextCursor cursor = editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return candidates;
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
    if (!parsedLine.directive.isEmpty() && parsedLine.directive != directToken.toLower()) {
        appendUnique(candidates, parsedLine.directive);
        const QString normalizedDirective = normalizeDirective(parsedLine.directive);
        const QString openingDirective = completionOpeningDirectiveForClosing(normalizedDirective);
        if (!openingDirective.isEmpty()) {
            appendUnique(candidates, openingDirective);
        }
    }

    const QString resolvedCommand = currentCompletionCommand();
    if (!resolvedCommand.isEmpty()) {
        appendUnique(candidates, resolvedCommand);
    }

    return candidates;
}

QString TextEditorTab::currentHelpTokenForCursor() const
{
    const QTextCursor cursor = editor_->textCursor();
    if (!cursor.block().isValid()) {
        return QString();
    }

    const QTextBlock block = cursor.block();
    const int column = cursor.position() - block.position();
    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);

    for (const TherionParsedToken &tokenSpan : parsedLine.tokenSpans) {
        if (tokenSpan.type == TherionTokenType::Comment) {
            continue;
        }

        const int tokenEnd = tokenSpan.start + tokenSpan.length;
        if (column >= tokenSpan.start && column <= tokenEnd) {
            return tokenSpan.text.toLower();
        }
    }

    return QString();
}

QString TextEditorTab::validationHelpHtmlForCursor(QString *tooltipText, QString *tooltipKey) const
{
    if (editor_ == nullptr) {
        return QString();
    }

    const QTextCursor cursor = editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QString();
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString command = normalizeDirective(parsedLine.directive.toLower());
    if (command.isEmpty() || !commandOptionTokens_.contains(command)) {
        return QString();
    }

    const int column = cursor.position() - block.position();
    int tokenIndexAtCursor = -1;
    int tokenCounter = 0;
    for (const TherionParsedToken &tokenSpan : parsedLine.tokenSpans) {
        if (tokenSpan.type == TherionTokenType::Comment) {
            continue;
        }

        const int tokenEnd = tokenSpan.start + tokenSpan.length;
        if (column >= tokenSpan.start && column <= tokenEnd) {
            tokenIndexAtCursor = tokenCounter;
            break;
        }
        ++tokenCounter;
    }

    if (tokenIndexAtCursor <= 0 || tokenIndexAtCursor >= parsedLine.tokens.size()) {
        return QString();
    }

    const QString cursorToken = parsedLine.tokens.at(tokenIndexAtCursor).trimmed();
    const QString normalizedCursorToken = cursorToken.toLower();
    QStringList allowedValues;
    QString detailMessage;
    QString issueKey;

    if (looksLikeOptionToken(cursorToken)) {
        const QStringList knownOptions = commandOptionTokens_.value(command);
        if (knownOptions.contains(normalizedCursorToken, Qt::CaseInsensitive)) {
            return QString();
        }

        allowedValues = knownOptions;
        std::sort(allowedValues.begin(),
                  allowedValues.end(),
                  [](const QString &a, const QString &b) {
                      return QString::compare(a, b, Qt::CaseInsensitive) < 0;
                  });
        detailMessage = tr("Unknown option for command '%1'.").arg(command.toHtmlEscaped());
        issueKey = QStringLiteral("option|%1|%2").arg(command, normalizedCursorToken);
    } else {
        int optionIndex = -1;
        for (int index = tokenIndexAtCursor - 1; index >= 1; --index) {
            const QString token = parsedLine.tokens.at(index).trimmed();
            if (token.startsWith(QLatin1Char('-'))) {
                optionIndex = index;
                break;
            }
        }
        if (optionIndex < 1) {
            return QString();
        }

        const QString optionToken = parsedLine.tokens.at(optionIndex).trimmed().toLower();
        const QString arity = commandOptionValueArityTokens_
            .value(commandOptionValueKey(command, optionToken))
            .trimmed()
            .toUpper();
        const int valueOrdinal = tokenIndexAtCursor - optionIndex;
        if (canonicalOptionArityToken(arity) == QStringLiteral("EXACTLY_ONE") && valueOrdinal != 1) {
            return QString();
        }

        if (optionToken == QStringLiteral("-subtype")) {
            const QString symbolTypeToken = symbolTypeForSubtypeLookup(command, parsedLine);
            const QHash<QString, QStringList> subtypeByType = commandSubtypeByTypeTokens_.value(command);
            allowedValues = subtypeByType.value(symbolTypeToken);
            if (allowedValues.contains(QStringLiteral("*"), Qt::CaseInsensitive)) {
                return QString();
            }
            if (allowedValues.contains(normalizedCursorToken, Qt::CaseInsensitive)) {
                return QString();
            }

            if (!symbolTypeToken.isEmpty() && !allowedValues.isEmpty()) {
                detailMessage = tr("Subtype '%1' is not allowed for type '%2'.")
                    .arg(cursorToken.toHtmlEscaped(), symbolTypeToken.toHtmlEscaped());
                issueKey = QStringLiteral("subtype|%1|%2|%3")
                    .arg(command, symbolTypeToken, normalizedCursorToken);
            }
        } else {
            allowedValues = commandOptionValueTokens_.value(commandOptionValueKey(command, optionToken));
            if (allowedValues.isEmpty()
                || allowedValues.contains(normalizedCursorToken, Qt::CaseInsensitive)) {
                return QString();
            }
            detailMessage = tr("Value '%1' is not allowed for option '%2'.")
                .arg(cursorToken.toHtmlEscaped(), optionToken.toHtmlEscaped());
            issueKey = QStringLiteral("value|%1|%2|%3")
                .arg(command, optionToken, normalizedCursorToken);
        }
    }

    if (allowedValues.isEmpty()) {
        return QString();
    }

    QStringList normalizedAllowed;
    appendUniqueList(normalizedAllowed, allowedValues);
    std::sort(normalizedAllowed.begin(),
              normalizedAllowed.end(),
              [](const QString &a, const QString &b) {
                  return QString::compare(a, b, Qt::CaseInsensitive) < 0;
              });

    if (detailMessage.isEmpty()) {
        detailMessage = tr("Token '%1' is not allowed in current context.")
            .arg(cursorToken.toHtmlEscaped());
    }
    if (issueKey.isEmpty()) {
        issueKey = QStringLiteral("generic|%1|%2").arg(command, normalizedCursorToken);
    }

    if (tooltipText != nullptr) {
        QString plainDetail = detailMessage;
        plainDetail.remove(QStringLiteral("<strong>"));
        plainDetail.remove(QStringLiteral("</strong>"));
        plainDetail.remove(QStringLiteral("<code>"));
        plainDetail.remove(QStringLiteral("</code>"));
        plainDetail.remove(QStringLiteral("&apos;"));
        plainDetail.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
        plainDetail.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
        plainDetail.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
        plainDetail.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        *tooltipText = tr("%1 Allowed: %2")
            .arg(plainDetail, normalizedAllowed.join(QStringLiteral(", ")));
    }
    if (tooltipKey != nullptr) {
        *tooltipKey = issueKey;
    }

    QString html;
    html += QStringLiteral("<h3>Validation</h3>");
    html += QStringLiteral("<p><strong>Token:</strong> <code>%1</code></p>").arg(cursorToken.toHtmlEscaped());
    if (!detailMessage.isEmpty()) {
        html += QStringLiteral("<p>%1</p>").arg(detailMessage);
    }
    html += QStringLiteral("<h4>Allowed Values</h4>");
    html += renderList(normalizedAllowed);
    return html;
}

void TextEditorTab::updateValidationTooltipForCursor()
{
    if (editor_ == nullptr) {
        return;
    }

    QString tooltipText;
    QString tooltipKey;
    const QString validationHtml = validationHelpHtmlForCursor(&tooltipText, &tooltipKey);
    if (validationHtml.isEmpty() || tooltipText.trimmed().isEmpty()) {
        editor_->setToolTip(QString());
        lastValidationTooltipKey_.clear();
        return;
    }

    editor_->setToolTip(tooltipText);
    if (tooltipKey == lastValidationTooltipKey_) {
        return;
    }

    lastValidationTooltipKey_ = tooltipKey;
    QToolTip::showText(editor_->mapToGlobal(editor_->cursorRect().bottomLeft()),
                       tooltipText,
                       editor_,
                       QRect(),
                       2200);
}

void TextEditorTab::updateContextHelp()
{
    if (helpBrowser_ == nullptr) {
        return;
    }

    const QString validationHtml = validationHelpHtmlForCursor();
    if (!validationHtml.isEmpty()) {
        helpBrowser_->setHtml(validationHtml);
        return;
    }

    const QStringList candidates = helpCandidateTokens();
    for (const QString &candidate : candidates) {
        const auto entryIt = helpEntries_.constFind(candidate.toLower());
        if (entryIt == helpEntries_.constEnd()) {
            continue;
        }

        helpBrowser_->setHtml(renderHelpHtml(candidate, entryIt.value()));
        return;
    }

    helpBrowser_->setHtml(tr("<p>No contextual help is available for the current token.</p>"));
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

QString TextEditorTab::renderHelpHtml(const QString &token, const TherionHelpEntry &entry, bool includeSyntax) const
{
    QString html;
    html += QStringLiteral("<h3>%1</h3>").arg(token.toHtmlEscaped());

    if (!entry.summary.isEmpty()) {
        html += QStringLiteral("<p><strong>Summary:</strong> %1</p>").arg(entry.summary.toHtmlEscaped());
    }
    if (includeSyntax && !entry.syntax.isEmpty()) {
        html += QStringLiteral("<p><strong>Syntax:</strong> <code>%1</code></p>").arg(entry.syntax.toHtmlEscaped());
    }
    if (!entry.arguments.isEmpty()) {
        html += QStringLiteral("<h4>Arguments</h4>");
        html += renderList(entry.arguments);
    }
    if (!entry.acceptedValues.isEmpty()) {
        html += QStringLiteral("<h4>Accepted Values</h4>");
        html += renderList(entry.acceptedValues);
    }
    if (!entry.options.isEmpty()) {
        html += QStringLiteral("<h4>Options</h4>");
        html += renderList(entry.options);
    }

    return html;
}

QString TextEditorTab::renderHelpSummaryHtml(const QString &token, const TherionHelpEntry &entry) const
{
    QString html;
    html += QStringLiteral("<h3>%1</h3>").arg(token.toHtmlEscaped());
    if (!entry.summary.trimmed().isEmpty()) {
        html += QStringLiteral("<p><strong>Summary:</strong> %1</p>").arg(entry.summary.toHtmlEscaped());
    } else {
        html += QStringLiteral("<p><strong>Summary:</strong> %1</p>")
                    .arg(tr("No summary is available for this command.").toHtmlEscaped());
    }
    return html;
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

void TextEditorTab::updateSearchResults(const QString &message, bool error)
{
    searchStatusLabel_->setText(message);
    searchStatusLabel_->setStyleSheet(error ? QStringLiteral("color: #cc6666;") : QString());
}

void TextEditorTab::updateSearchVisibility(bool replaceMode)
{
    replaceRow_->setVisible(replaceMode);
    replaceButton_->setVisible(replaceMode);
    replaceAllButton_->setVisible(replaceMode);
}

bool TextEditorTab::performFind(bool forward, bool wrapSearch)
{
    const QString findText = currentFindText();
    if (findText.isEmpty()) {
        updateSearchResults(tr("Enter text to find."), true);
        return false;
    }

    QTextDocument::FindFlags flags = findFlags();
    if (!forward) {
        flags |= QTextDocument::FindBackward;
    }

    QTextCursor cursor = editor_->textCursor();
    if (cursor.hasSelection()) {
        cursor.setPosition(forward ? cursor.selectionEnd() : cursor.selectionStart());
    }

    QTextCursor foundCursor = editor_->document()->find(findText, cursor, flags);
    bool wrapped = false;

    if (foundCursor.isNull() && wrapSearch) {
        wrapped = true;
        QTextCursor wrapCursor(editor_->document());
        wrapCursor.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
        foundCursor = editor_->document()->find(findText, wrapCursor, flags);
    }

    if (foundCursor.isNull()) {
        updateSearchResults(tr("No matches found."), true);
        return false;
    }

    editor_->setTextCursor(foundCursor);
    editor_->ensureCursorVisible();
    updateSearchResults(wrapped ? tr("Wrapped search.") : tr("Match found."));
    return true;
}

QTextDocument::FindFlags TextEditorTab::findFlags() const
{
    QTextDocument::FindFlags flags;
    if (wholeWordCheck_->isChecked()) {
        flags |= QTextDocument::FindWholeWords;
    }
    if (caseSensitiveCheck_->isChecked()) {
        flags |= QTextDocument::FindCaseSensitively;
    }
    return flags;
}

QString TextEditorTab::currentFindText() const
{
    return findEdit_->text().trimmed();
}

QString TextEditorTab::currentReplaceText() const
{
    return replaceEdit_->text();
}
}
