#include "TextEditorTab.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDrag>
#include <QFrame>
#include <QCompleter>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QFont>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QGraphicsObject>
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
#include <QHeaderView>
#include <QScreen>
#include <QScrollBar>
#include <QStyle>
#include <QStringListModel>
#include <QTimer>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QResizeEvent>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTextBlock>
#include <QPen>

#include <algorithm>
#include <functional>
#include <limits>

#include "../core/DocumentFile.h"
#include "../core/TherionDocumentEditor.h"
#include "../core/TherionDocumentParser.h"
#include "../editor/TherionSyntaxHighlighter.h"

namespace
{
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
            event->acceptProposedAction();
            return;
        }
        QGraphicsView::dragMoveEvent(event);
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
        event->acceptProposedAction();
    }
};

class BlockCanvasItem final : public QGraphicsObject
{
public:
    BlockCanvasItem(const QString &kind, const QString &name, int lineNumber, QGraphicsItem *parent = nullptr)
        : QGraphicsObject(parent)
        , kind_(kind)
        , name_(name)
        , lineNumber_(lineNumber)
    {
        setFlag(QGraphicsItem::ItemIsSelectable, true);
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setCursor(Qt::OpenHandCursor);
    }

    std::function<void(const QString &, int)> onConfigure;
    std::function<void(int)> onDelete;
    std::function<void(int, const QPointF &)> onMoveRequest;

    QRectF boundingRect() const override
    {
        return QRectF(0.0, 0.0, 260.0, 42.0);
    }

    void setName(const QString &name)
    {
        name_ = name;
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

        QColor baseColor(QStringLiteral("#d8e9ff"));
        const QString normalizedKind = kind_.toLower();
        if (normalizedKind == QStringLiteral("survey")) {
            baseColor = QColor(QStringLiteral("#d7f7d7"));
        } else if (normalizedKind == QStringLiteral("centerline")) {
            baseColor = QColor(QStringLiteral("#ffe9cc"));
        } else if (normalizedKind == QStringLiteral("data")) {
            baseColor = QColor(QStringLiteral("#e8dbff"));
        } else if (normalizedKind == QStringLiteral("map")) {
            baseColor = QColor(QStringLiteral("#d5f3f0"));
        }

        painter->setPen(QPen(isSelected() ? QColor(QStringLiteral("#2f6fed")) : QColor(QStringLiteral("#8c8c8c")), isSelected() ? 2.0 : 1.0));
        painter->setBrush(baseColor);
        painter->drawRoundedRect(boundingRect(), 6.0, 6.0);

        const QRectF configureButtonRect = configureIconRect();
        const QRectF deleteButtonRect = deleteIconRect();

        painter->setPen(QPen(QColor(QStringLiteral("#6a6a6a")), 1.0));
        painter->setBrush(QColor(QStringLiteral("#f2f2f2")));
        painter->drawRoundedRect(configureButtonRect, 4.0, 4.0);
        painter->drawRoundedRect(deleteButtonRect, 4.0, 4.0);

        const QRect configureIconBounds = configureButtonRect.adjusted(4.0, 4.0, -4.0, -4.0).toRect();
        const QRect deleteIconBounds = deleteButtonRect.adjusted(4.0, 4.0, -4.0, -4.0).toRect();
        if (QStyle *style = QApplication::style(); style != nullptr) {
            style->standardIcon(QStyle::SP_FileDialogDetailedView).paint(painter, configureIconBounds);
            style->standardIcon(QStyle::SP_TrashIcon).paint(painter, deleteIconBounds);
        }

        const QString title = name_.isEmpty() ? kind_ : QStringLiteral("%1: %2").arg(kind_, name_);
        painter->setPen(QColor(QStringLiteral("#1f1f1f")));
        painter->drawText(QRectF(10.0, 0.0, configureButtonRect.left() - 16.0, boundingRect().height()),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          title);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (event != nullptr && configureIconRect().contains(event->pos())) {
            if (onConfigure) {
                onConfigure(kind_, lineNumber_);
            }
            event->accept();
            return;
        }
        if (event != nullptr && deleteIconRect().contains(event->pos())) {
            if (onDelete) {
                onDelete(lineNumber_);
            }
            event->accept();
            return;
        }

        dragStartScenePos_ = event != nullptr ? event->scenePos() : QPointF();
        dragStartItemPos_ = pos();
        dragging_ = false;
        QGraphicsObject::mousePressEvent(event);
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (event != nullptr && (event->buttons() & Qt::LeftButton)) {
            if (!dragging_ && (event->scenePos() - dragStartScenePos_).manhattanLength() >= 4.0) {
                dragging_ = true;
                setCursor(Qt::ClosedHandCursor);
            }
        }
        QGraphicsObject::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        QGraphicsObject::mouseReleaseEvent(event);
        setCursor(Qt::OpenHandCursor);
        const bool moved = (pos() - dragStartItemPos_).manhattanLength() >= 2.0;
        if (!moved) {
            dragging_ = false;
            return;
        }

        const QPointF dropScenePos = mapToScene(boundingRect().center());
        setPos(dragStartItemPos_);
        dragging_ = false;
        if (onMoveRequest) {
            onMoveRequest(lineNumber_, dropScenePos);
        }
    }

private:
    QRectF configureIconRect() const
    {
        const qreal iconSize = 24.0;
        const qreal margin = 8.0;
        const qreal gap = 6.0;
        const qreal x = boundingRect().right() - margin - (iconSize * 2.0) - gap;
        return QRectF(x, 9.0, iconSize, iconSize);
    }

    QRectF deleteIconRect() const
    {
        const qreal iconSize = 24.0;
        const qreal margin = 8.0;
        const qreal x = boundingRect().right() - margin - iconSize;
        return QRectF(x, 9.0, iconSize, iconSize);
    }

    QString kind_;
    QString name_;
    int lineNumber_ = 0;
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

QString blockDisplayName(const TherionStudio::TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.size() >= 2) {
        return parsedLine.tokens.at(1);
    }
    return QString();
}

bool isBlockOpeningDirective(const QString &directive)
{
    return directive == QStringLiteral("survey")
        || directive == QStringLiteral("centerline")
        || directive == QStringLiteral("centreline")
        || directive == QStringLiteral("map")
        || directive == QStringLiteral("scrap")
        || directive == QStringLiteral("data");
}

bool isContainerBlockDirective(const QString &directive)
{
    return directive == QStringLiteral("survey")
        || directive == QStringLiteral("centerline")
        || directive == QStringLiteral("centreline")
        || directive == QStringLiteral("map")
        || directive == QStringLiteral("scrap");
}

bool isBlockClosingDirective(const QString &directive)
{
    return directive == QStringLiteral("endsurvey")
        || directive == QStringLiteral("endcenterline")
        || directive == QStringLiteral("endcentreline")
        || directive == QStringLiteral("endmap")
        || directive == QStringLiteral("endscrap");
}

QString closingDirectiveFor(const QString &openingDirective)
{
    if (openingDirective == QStringLiteral("survey")) {
        return QStringLiteral("endsurvey");
    }
    if (openingDirective == QStringLiteral("centerline") || openingDirective == QStringLiteral("centreline")) {
        return QStringLiteral("endcenterline");
    }
    if (openingDirective == QStringLiteral("map")) {
        return QStringLiteral("endmap");
    }
    if (openingDirective == QStringLiteral("scrap")) {
        return QStringLiteral("endscrap");
    }
    return QString();
}

bool legacyIsCompatibleChildKind(const QString &parentKind, const QString &childKind)
{
    const QString normalizedParent = parentKind.toLower();
    const QString normalizedChild = childKind.toLower();
    if (normalizedChild == QStringLiteral("survey")) {
        return normalizedParent.isEmpty();
    }
    if (normalizedChild == QStringLiteral("centerline")) {
        return normalizedParent == QStringLiteral("survey");
    }
    if (normalizedChild == QStringLiteral("data")) {
        return normalizedParent == QStringLiteral("centerline");
    }
    if (normalizedChild == QStringLiteral("map")) {
        return normalizedParent == QStringLiteral("survey");
    }
    if (normalizedChild == QStringLiteral("scrap")) {
        return normalizedParent == QStringLiteral("survey");
    }
    if (normalizedChild == QStringLiteral("team") || normalizedChild == QStringLiteral("explo-date")) {
        return normalizedParent == QStringLiteral("centerline");
    }
    return false;
}

QString normalizeDirective(const QString &directive)
{
    const QString normalized = directive.toLower();
    if (normalized == QStringLiteral("centreline")) {
        return QStringLiteral("centerline");
    }
    if (normalized == QStringLiteral("endcentreline")) {
        return QStringLiteral("endcenterline");
    }
    return normalized;
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
    if (normalized == QStringLiteral("survey")
        || normalized == QStringLiteral("scrap")
        || normalized == QStringLiteral("map")
        || normalized == QStringLiteral("centerline")
        || normalized == QStringLiteral("surface")
        || normalized == QStringLiteral("layout")) {
        return normalized;
    }
    return QString();
}

QString completionOpeningDirectiveForClosing(const QString &directive)
{
    if (directive == QStringLiteral("endsurvey")) {
        return QStringLiteral("survey");
    }
    if (directive == QStringLiteral("endcenterline")) {
        return QStringLiteral("centerline");
    }
    if (directive == QStringLiteral("endmap")) {
        return QStringLiteral("map");
    }
    if (directive == QStringLiteral("endscrap")) {
        return QStringLiteral("scrap");
    }
    if (directive == QStringLiteral("endsurface")) {
        return QStringLiteral("surface");
    }
    if (directive == QStringLiteral("endlayout")) {
        return QStringLiteral("layout");
    }
    if (directive == QStringLiteral("endlookup")) {
        return QStringLiteral("lookup");
    }
    if (directive == QStringLiteral("endline")) {
        return QStringLiteral("line");
    }
    if (directive == QStringLiteral("endarea")) {
        return QStringLiteral("area");
    }
    return QString();
}

QString completionClosingDirectiveForOpening(const QString &directive)
{
    if (directive == QStringLiteral("survey")) {
        return QStringLiteral("endsurvey");
    }
    if (directive == QStringLiteral("centerline")) {
        return QStringLiteral("endcenterline");
    }
    if (directive == QStringLiteral("map")) {
        return QStringLiteral("endmap");
    }
    if (directive == QStringLiteral("scrap")) {
        return QStringLiteral("endscrap");
    }
    if (directive == QStringLiteral("surface")) {
        return QStringLiteral("endsurface");
    }
    if (directive == QStringLiteral("layout")) {
        return QStringLiteral("endlayout");
    }
    if (directive == QStringLiteral("lookup")) {
        return QStringLiteral("endlookup");
    }
    if (directive == QStringLiteral("line")) {
        return QStringLiteral("endline");
    }
    if (directive == QStringLiteral("area")) {
        return QStringLiteral("endarea");
    }
    return QString();
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

bool isLikelyCenterlineCommand(const QString &directive)
{
    static const QStringList knownCommands = {
        QStringLiteral("altitude"),
        QStringLiteral("calibrate"),
        QStringLiteral("cs"),
        QStringLiteral("date"),
        QStringLiteral("declination"),
        QStringLiteral("equate"),
        QStringLiteral("explo-date"),
        QStringLiteral("flags"),
        QStringLiteral("fix"),
        QStringLiteral("grade"),
        QStringLiteral("infer"),
        QStringLiteral("instrument"),
        QStringLiteral("mark"),
        QStringLiteral("sd"),
        QStringLiteral("station"),
        QStringLiteral("team"),
        QStringLiteral("units"),
    };
    return knownCommands.contains(directive);
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
    searchLayout->setContentsMargins(8, 8, 8, 8);
    searchLayout->setSpacing(6);

    auto *findRow = new QHBoxLayout;
    findRow->setSpacing(6);

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
    optionRow->setSpacing(6);

    wholeWordCheck_ = new QCheckBox(tr("Whole word"), searchBar_);
    caseSensitiveCheck_ = new QCheckBox(tr("Case sensitive"), searchBar_);

    optionRow->addWidget(wholeWordCheck_);
    optionRow->addWidget(caseSensitiveCheck_);
    optionRow->addStretch(1);

    replaceRow_ = new QWidget(searchBar_);
    auto *replaceLayout = new QHBoxLayout(replaceRow_);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(6);

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
    statusLayout->setContentsMargins(8, 0, 8, 8);
    statusLayout->setSpacing(12);

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

    editorHelpSplitter_ = new QSplitter(Qt::Vertical, this);
    editorHelpSplitter_->setChildrenCollapsible(false);
    editorHelpSplitter_->setHandleWidth(12);
    editorHelpSplitter_->addWidget(editor_);

    buildHelpPanel();
    editorHelpSplitter_->addWidget(helpPanel_);
    editorHelpSplitter_->setStretchFactor(0, 1);
    editorHelpSplitter_->setStretchFactor(1, 0);
    editorHelpSplitter_->setCollapsible(1, true);

    modeRow_ = new QWidget(this);
    auto *modeLayout = new QHBoxLayout(modeRow_);
    modeLayout->setContentsMargins(8, 6, 8, 6);
    modeLayout->setSpacing(6);
    modeLayout->addWidget(new QLabel(tr("Mode:"), modeRow_));
    rawModeButton_ = new QPushButton(tr("Raw"), modeRow_);
    rawModeButton_->setCheckable(true);
    blocksModeButton_ = new QPushButton(tr("Blocks"), modeRow_);
    blocksModeButton_->setCheckable(true);
    blocksModeButton_->setToolTip(tr("Experimental block canvas for .th files."));
    modeLayout->addWidget(rawModeButton_);
    modeLayout->addWidget(blocksModeButton_);
    modeLayout->addStretch(1);

    blocksPanel_ = new QWidget(this);
    auto *blocksLayout = new QHBoxLayout(blocksPanel_);
    blocksLayout->setContentsMargins(8, 8, 8, 8);
    blocksLayout->setSpacing(8);

    auto *toolboxColumn = new QWidget(blocksPanel_);
    auto *toolboxColumnLayout = new QVBoxLayout(toolboxColumn);
    toolboxColumnLayout->setContentsMargins(0, 0, 0, 0);
    toolboxColumnLayout->setSpacing(6);
    blockToolboxFilterEdit_ = new QLineEdit(toolboxColumn);
    blockToolboxFilterEdit_->setPlaceholderText(tr("Filter commands..."));

    blockToolboxList_ = new BlockToolboxList(toolboxColumn);
    blockToolboxList_->setMinimumWidth(180);
    toolboxColumnLayout->addWidget(blockToolboxFilterEdit_);
    toolboxColumnLayout->addWidget(blockToolboxList_, 1);
    populateBlockToolbox();
    connect(blockToolboxFilterEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        populateBlockToolbox();
    });

    blockCanvasScene_ = new QGraphicsScene(blocksPanel_);
    auto *typedCanvasView = new BlockCanvasView(blocksPanel_);
    typedCanvasView->setScene(blockCanvasScene_);
    typedCanvasView->setSceneRect(QRectF(0.0, 0.0, 1400.0, 2000.0));
    typedCanvasView->setBackgroundBrush(palette().color(QPalette::Base));
    typedCanvasView->onDropBlock = [this](const QString &kind, const QPointF &scenePos) {
        handleCanvasDrop(kind, scenePos);
    };
    blockCanvasView_ = typedCanvasView;

    blocksLayout->addWidget(toolboxColumn);
    blocksLayout->addWidget(blockCanvasView_, 1);

    editorModeStack_ = new QStackedWidget(this);
    editorModeStack_->addWidget(editorHelpSplitter_);
    editorModeStack_->addWidget(blocksPanel_);

    layout->addWidget(searchBar_);
    layout->addWidget(modeRow_);
    layout->addWidget(editorModeStack_, 1);
    layout->addWidget(statusRow_);

    connect(editor_, &QPlainTextEdit::textChanged, this, &TextEditorTab::handleTextChanged);
    connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, &TextEditorTab::handleCursorPositionChanged);
    connect(rawModeButton_, &QPushButton::clicked, this, &TextEditorTab::handleRawModeRequested);
    connect(blocksModeButton_, &QPushButton::clicked, this, &TextEditorTab::handleBlocksModeRequested);

    refreshBlocksModeAvailability();
    refreshEditorModeUi();
    rebuildBlocksCanvasFromText();
    refreshStatus();
    refreshCurrentLineHighlight();
    updateContextHelp();
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
    rebuildBlocksCanvasFromText();
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
                                     QString *errorMessage)
{
    QString contents = editor_->toPlainText();
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendScrapBlock(&contents, preferredName, &resolvedLineNumber, errorMessage)) {
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

QString TextEditorTab::statusPathText() const
{
    return displayPath();
}

QString TextEditorTab::statusEncodingText() const
{
    return fileEncodingLabel_;
}

void TextEditorTab::setInlineStatusVisible(bool visible)
{
    inlineStatusRequestedVisible_ = visible;
    refreshStatus();
}

bool TextEditorTab::isBlocksModeSupportedForCurrentFile() const
{
    if (filePath_.trimmed().isEmpty()) {
        return false;
    }

    const QString suffix = QFileInfo(filePath_).suffix().toLower();
    return suffix == QStringLiteral("th");
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
        rebuildBlocksCanvasFromText();
    } else if (editor_ != nullptr) {
        editor_->setFocus();
    }
    refreshEditorModeUi();
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
        editorModeStack_->setCurrentWidget(editorHelpSplitter_);
    }
}

void TextEditorTab::populateBlockToolbox()
{
    if (blockToolboxList_ == nullptr) {
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

    const struct ToolboxSection {
        QString title;
        QStringList activeContexts;
    } sections[] = {
        {QStringLiteral("Top-level"), {QStringLiteral("none"), QStringLiteral("all")}},
        {QStringLiteral("Inside Survey"), {QStringLiteral("survey"), QStringLiteral("all")}},
        {QStringLiteral("Inside Centerline"), {QStringLiteral("centerline"), QStringLiteral("all")}},
    };

    int insertedRows = 0;
    for (const ToolboxSection &section : sections) {
        QStringList sectionCommands;
        for (const QString &activeContext : section.activeContexts) {
            appendUniqueList(sectionCommands, contextCommandTokens_.value(activeContext));
        }

        QStringList visibleCommands;
        for (const QString &command : sectionCommands) {
            const QString normalizedCommand = normalizeDirective(command);
            if (normalizedCommand.isEmpty()) {
                continue;
            }
            if (normalizedCommand == QStringLiteral("all")
                || normalizedCommand == QStringLiteral("none")
                || normalizedCommand.startsWith(QStringLiteral("end"))) {
                continue;
            }
            if (!isCompatibleChildKindForBlocks(section.activeContexts.contains(QStringLiteral("survey"))
                                                    ? QStringLiteral("survey")
                                                    : (section.activeContexts.contains(QStringLiteral("centerline"))
                                                           ? QStringLiteral("centerline")
                                                           : QString()),
                                                normalizedCommand)) {
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

        auto *categoryItem = new QListWidgetItem(QStringLiteral("[%1]").arg(section.title), blockToolboxList_);
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
}

void TextEditorTab::rebuildBlocksCanvasFromText()
{
    if (blockCanvasScene_ == nullptr) {
        return;
    }

    blockCanvasScene_->clear();

    if (!isBlocksModeSupportedForCurrentFile()) {
        auto *note = blockCanvasScene_->addText(
            tr("Blocks mode is currently available only for .th files."));
        note->setPos(16.0, 16.0);
        return;
    }

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(editor_->toPlainText());
    if (parsedLines.isEmpty()) {
        auto *note = blockCanvasScene_->addText(tr("No Therion directives found."));
        note->setPos(16.0, 16.0);
        return;
    }

    struct StackEntry
    {
        QString directive;
        BlockCanvasItem *item = nullptr;
    };

    QVector<StackEntry> stack;
    QVector<BlockCanvasItem *> roots;
    QVector<BlockCanvasItem *> allItems;

    for (const TherionParsedLine &parsedLine : parsedLines) {
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
        auto *item = new BlockCanvasItem(directive, name, parsedLine.lineNumber, parentItem);
        item->onConfigure = [this](const QString &kind, int lineNumber) {
            handleBlockConfigureRequest(kind, lineNumber);
        };
        item->onDelete = [this](int lineNumber) {
            handleBlockDeleteRequest(lineNumber);
        };
        item->onMoveRequest = [this](int lineNumber, const QPointF &scenePos) {
            handleBlockMoveRequest(lineNumber, scenePos);
        };
        if (parentItem == nullptr) {
            roots.append(item);
        }
        allItems.append(item);
        blockCanvasScene_->addItem(item);

        if (!commandDirective && isContainerBlockDirective(directive)) {
            stack.append(StackEntry{directive, item});
        }
    }

    qreal y = 16.0;
    std::function<void(BlockCanvasItem *, int)> layoutTree = [&](BlockCanvasItem *item, int depth) {
        if (item == nullptr) {
            return;
        }
        const QPointF scenePosition(24.0 + (depth * 28.0), y);
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
    blockCanvasScene_->setSceneRect(0.0, 0.0, 1400.0, qMax<qreal>(y + 40.0, 600.0));
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
        QMessageBox::information(this, tr("Blocks Mode"), tr("Blocks mode is available only for .th files."));
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
    if (blockCanvasScene_ != nullptr) {
        selectedBlock = resolveBlockFromItem(blockCanvasScene_->itemAt(scenePos, QTransform()));
    }
    if (selectedBlock == nullptr && blockCanvasScene_ != nullptr) {
        selectedBlock = resolveBlockFromItem(blockCanvasScene_->focusItem());
    }
    if (selectedBlock == nullptr && blockCanvasScene_ != nullptr && !blockCanvasScene_->selectedItems().isEmpty()) {
        selectedBlock = resolveBlockFromItem(blockCanvasScene_->selectedItems().first());
    }

    QString parentKind;
    int parentLine = 0;
    if (selectedBlock != nullptr) {
        parentKind = selectedBlock->kind().toLower();
        parentLine = selectedBlock->lineNumber();
    }

    const QString normalizedKind = kind.trimmed().toLower();
    if (!isCompatibleChildKindForBlocks(parentKind, normalizedKind)) {
        QMessageBox::warning(this,
                             tr("Incompatible Drop"),
                             tr("`%1` cannot be inserted inside `%2`.")
                                 .arg(normalizedKind, parentKind.isEmpty() ? tr("root") : parentKind));
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

    auto insertionAnchorForContainer = [&existingLines](int openingLineNumber, const QString &openingDirective) {
        const QString closingDirective = closingDirectiveFor(openingDirective);
        const int endLine = findMatchingBlockEndLine(existingLines, openingLineNumber, openingDirective, closingDirective);
        return endLine > openingLineNumber ? endLine : (openingLineNumber + 1);
    };

    QStringList linesToInsert;
    int insertBeforeLine = qMax(1, editor_->document()->blockCount() + 1);
    if (normalizedKind == QStringLiteral("survey")) {
        linesToInsert << QStringLiteral("survey new-survey")
                      << QStringLiteral("endsurvey");
    } else if (normalizedKind == QStringLiteral("centerline")) {
        if (parentLine <= 0) {
            return;
        }
        insertBeforeLine = insertionAnchorForContainer(parentLine, QStringLiteral("survey"));
        const QString indent = lineIndent(parentLine) + QStringLiteral("  ");
        linesToInsert << QStringLiteral("%1centerline").arg(indent)
                      << QStringLiteral("%1endcenterline").arg(indent);
    } else if (normalizedKind == QStringLiteral("data")) {
        if (parentLine <= 0) {
            return;
        }
        insertBeforeLine = insertionAnchorForContainer(parentLine, QStringLiteral("centerline"));
        const QString indent = lineIndent(parentLine) + QStringLiteral("  ");
        linesToInsert << QStringLiteral("%1data normal from to compass clino tape").arg(indent)
                      << QStringLiteral("%1  1 2 0 0 1").arg(indent);
    } else if (normalizedKind == QStringLiteral("map")) {
        if (parentLine <= 0) {
            return;
        }
        insertBeforeLine = insertionAnchorForContainer(parentLine, QStringLiteral("survey"));
        const QString indent = lineIndent(parentLine) + QStringLiteral("  ");
        linesToInsert << QStringLiteral("%1map new-map").arg(indent)
                      << QStringLiteral("%1endmap").arg(indent);
    } else if (normalizedKind == QStringLiteral("scrap")) {
        if (parentLine <= 0) {
            return;
        }
        insertBeforeLine = insertionAnchorForContainer(parentLine, QStringLiteral("survey"));
        const QString indent = lineIndent(parentLine) + QStringLiteral("  ");
        linesToInsert << QStringLiteral("%1scrap new-scrap").arg(indent)
                      << QStringLiteral("%1endscrap").arg(indent);
    } else if (normalizedKind == QStringLiteral("team") || normalizedKind == QStringLiteral("explo-date")) {
        if (parentLine > 0 && isContainerBlockDirective(parentKind)) {
            insertBeforeLine = insertionAnchorForContainer(parentLine, parentKind);
        } else if (parentLine > 0) {
            insertBeforeLine = parentLine + 1;
        }
        const QString indent = parentLine > 0
            ? lineIndent(parentLine) + (isContainerBlockDirective(parentKind) ? QStringLiteral("  ") : QString())
            : QString();
        linesToInsert << QStringLiteral("%1%2 value").arg(indent, normalizedKind);
    } else {
        if (parentLine > 0 && isContainerBlockDirective(parentKind)) {
            insertBeforeLine = insertionAnchorForContainer(parentLine, parentKind);
        } else if (parentLine > 0) {
            insertBeforeLine = parentLine + 1;
        }

        const QString indent = parentLine > 0
            ? lineIndent(parentLine) + (isContainerBlockDirective(parentKind) ? QStringLiteral("  ") : QString())
            : QString();
        QStringList placeholderTokens;
        const int requiredArgumentCount = qMax(0, commandRequiredPositionalCount_.value(normalizedKind));
        for (int argumentIndex = 0; argumentIndex < requiredArgumentCount; ++argumentIndex) {
            placeholderTokens.append(QStringLiteral("value%1").arg(argumentIndex + 1));
        }
        if (normalizedKind == QStringLiteral("input") && placeholderTokens.isEmpty()) {
            placeholderTokens.append(QStringLiteral("./path/file.th"));
        }
        if (placeholderTokens.isEmpty()) {
            const QStringList suggestedValues = commandValueTokens_.value(normalizedKind);
            if (!suggestedValues.isEmpty()) {
                placeholderTokens.append(suggestedValues.first());
            }
        }

        QString line = QStringLiteral("%1%2").arg(indent, normalizedKind);
        if (!placeholderTokens.isEmpty()) {
            line += QStringLiteral(" ") + placeholderTokens.join(QLatin1Char(' '));
        }
        linesToInsert << line;
    }

    QString errorMessage;
    if (!insertLinesBefore(insertBeforeLine, linesToInsert, &errorMessage)) {
        QMessageBox::warning(this, tr("Block Insert Failed"), errorMessage);
    }
}

void TextEditorTab::handleBlockMoveRequest(int lineNumber, const QPointF &scenePos)
{
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

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(contents);
    QVector<BlockEntry> entries;
    entries.reserve(parsedLines.size());
    QHash<int, int> entryIndexByStartLine;

    struct OpenContainer
    {
        QString directive;
        int lineNumber = 0;
    };
    QVector<OpenContainer> openStack;

    auto openingDirectiveForClosing = [](const QString &directive) {
        if (directive == QStringLiteral("endsurvey")) {
            return QStringLiteral("survey");
        }
        if (directive == QStringLiteral("endcenterline")) {
            return QStringLiteral("centerline");
        }
        if (directive == QStringLiteral("endmap")) {
            return QStringLiteral("map");
        }
        if (directive == QStringLiteral("endscrap")) {
            return QStringLiteral("scrap");
        }
        return QString();
    };

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
        const QString directive = normalizeDirective(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }

        if (isBlockClosingDirective(directive)) {
            const QString openingDirective = openingDirectiveForClosing(directive);
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
                const int centerlineParentLine = nearestOpenContainerLine(QStringLiteral("centerline"));
                if (centerlineParentLine > 0) {
                    parentLine = centerlineParentLine;
                }
            }

            BlockEntry entry;
            entry.kind = directive;
            entry.startLine = currentLineNumber;
            entry.endLine = currentLineNumber;
            entry.parentLine = parentLine;

            if (isContainerBlockDirective(directive)) {
                const QString closingDirective = closingDirectiveFor(directive);
                const int endLine = findMatchingBlockEndLine(lines, currentLineNumber, directive, closingDirective);
                if (endLine > currentLineNumber) {
                    entry.endLine = endLine;
                }
                openStack.append(OpenContainer{directive, currentLineNumber});
            } else if (directive == QStringLiteral("data")) {
                const int centerlineParentLine = nearestOpenContainerLine(QStringLiteral("centerline"));
                int centerlineEndLine = lines.size() + 1;
                if (centerlineParentLine > 0) {
                    const int resolvedEndLine = findMatchingBlockEndLine(lines,
                                                                         centerlineParentLine,
                                                                         QStringLiteral("centerline"),
                                                                         QStringLiteral("endcenterline"));
                    if (resolvedEndLine > centerlineParentLine) {
                        centerlineEndLine = resolvedEndLine;
                    }
                }

                int dataBodyLastLine = centerlineEndLine - 1;
                for (int scanLine = currentLineNumber + 1; scanLine <= centerlineEndLine - 1; ++scanLine) {
                    const TherionParsedLine scanParsedLine = TherionDocumentParser::parseLine(lines.at(scanLine - 1), scanLine);
                    const QString scanDirective = normalizeDirective(scanParsedLine.directive);
                    if (scanDirective.isEmpty() || scanDirective == QStringLiteral("extend")) {
                        continue;
                    }
                    if (scanDirective == QStringLiteral("data")
                        || scanDirective == QStringLiteral("endcenterline")
                        || isLikelyCenterlineCommand(scanDirective)) {
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

        if (directive == QStringLiteral("team") || directive == QStringLiteral("explo-date")) {
            BlockEntry entry;
            entry.kind = directive;
            entry.startLine = currentLineNumber;
            entry.endLine = currentLineNumber;
            const int centerlineParentLine = nearestOpenContainerLine(QStringLiteral("centerline"));
            entry.parentLine = centerlineParentLine > 0
                ? centerlineParentLine
                : (openStack.isEmpty() ? 0 : openStack.last().lineNumber);
            entryIndexByStartLine.insert(entry.startLine, entries.size());
            entries.append(entry);
        }
    }

    const auto sourceIndexIt = entryIndexByStartLine.constFind(lineNumber);
    if (sourceIndexIt == entryIndexByStartLine.cend()) {
        return;
    }
    const BlockEntry sourceEntry = entries.at(*sourceIndexIt);

    BlockCanvasItem *targetBlockItem = resolveBlockFromItem(blockCanvasScene_->itemAt(scenePos, QTransform()));
    if (targetBlockItem != nullptr && targetBlockItem->lineNumber() == sourceEntry.startLine) {
        targetBlockItem = nullptr;
    }
    if (targetBlockItem == nullptr) {
        qreal bestDistance = std::numeric_limits<qreal>::max();
        for (const BlockEntry &entry : entries) {
            if (entry.startLine == sourceEntry.startLine) {
                continue;
            }
            const auto sceneItemIt = sceneBlocksByLine.constFind(entry.startLine);
            if (sceneItemIt == sceneBlocksByLine.cend() || sceneItemIt.value() == nullptr) {
                continue;
            }
            const qreal candidateY = sceneItemIt.value()->sceneBoundingRect().center().y();
            const qreal distance = qAbs(candidateY - scenePos.y());
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

    int destinationParentLine = targetEntry.parentLine;
    int insertBeforeLineOriginal = targetEntry.startLine;

    const bool targetAcceptsSourceAsChild = isContainerBlockDirective(targetEntry.kind)
        && isCompatibleChildKindForBlocks(targetEntry.kind, sourceEntry.kind);
    if (targetAcceptsSourceAsChild && targetEntry.startLine != sourceEntry.startLine) {
        destinationParentLine = targetEntry.startLine;
        insertBeforeLineOriginal = targetEntry.endLine;
    } else {
        const qreal targetCenterY = targetBlockItem->sceneBoundingRect().center().y();
        const bool insertAfterTarget = scenePos.y() > targetCenterY;
        insertBeforeLineOriginal = insertAfterTarget ? (targetEntry.endLine + 1) : targetEntry.startLine;
        destinationParentLine = targetEntry.parentLine;
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

    auto buildInsertableCommandsForContext = [this](const QString &context, const QString &currentDirective) {
        QStringList actions = contextCommandTokens_.value(context);
        const QString normalizedCurrentDirective = normalizeDirective(currentDirective);
        const QString currentClosingDirective = completionClosingDirectiveForOpening(normalizedCurrentDirective);
        actions.removeAll(normalizedCurrentDirective);
        if (!currentClosingDirective.isEmpty()) {
            actions.removeAll(currentClosingDirective);
        }
        actions.removeAll(QStringLiteral("all"));
        actions.removeAll(QStringLiteral("none"));
        actions.removeAll(QStringLiteral("centreline"));
        actions.removeAll(QStringLiteral("endcentreline"));
        actions.removeAll(QStringLiteral("centerline"));
        actions.removeAll(QStringLiteral("endcenterline"));

        QStringList filtered;
        for (const QString &action : actions) {
            const QString normalizedAction = normalizeDirective(action);
            if (normalizedAction.isEmpty() || normalizedAction.startsWith(QStringLiteral("end"))) {
                continue;
            }
            appendUnique(filtered, normalizedAction);
        }
        std::sort(filtered.begin(), filtered.end(), [](const QString &a, const QString &b) {
            return QString::compare(a, b, Qt::CaseInsensitive) < 0;
        });
        return filtered;
    };

    auto buildInsertionTemplate = [this](const QString &commandToken,
                                         const QString &childIndent) {
        QStringList linesToInsert;
        const QString normalizedCommand = normalizeDirective(commandToken.trimmed().toLower());
        if (normalizedCommand.isEmpty()) {
            return linesToInsert;
        }

        if (normalizedCommand == QStringLiteral("data")) {
            linesToInsert << QStringLiteral("%1data normal from to compass clino tape").arg(childIndent)
                          << QStringLiteral("%1  1 2 0 0 1").arg(childIndent);
            return linesToInsert;
        }

        if (normalizedCommand == QStringLiteral("team")) {
            linesToInsert << QStringLiteral("%1team \"Team Name\"").arg(childIndent);
            return linesToInsert;
        }

        if (normalizedCommand == QStringLiteral("explo-date")
            || normalizedCommand == QStringLiteral("date")) {
            linesToInsert << QStringLiteral("%1%2 2000").arg(childIndent, normalizedCommand);
            return linesToInsert;
        }

        if (normalizedCommand == QStringLiteral("group")
            || normalizedCommand == QStringLiteral("endgroup")
            || normalizedCommand == QStringLiteral("break")) {
            linesToInsert << QStringLiteral("%1%2").arg(childIndent, normalizedCommand);
            return linesToInsert;
        }

        const int requiredArgumentCount = qMax(0, commandRequiredPositionalCount_.value(normalizedCommand));
        QStringList placeholderTokens;
        for (int argumentIndex = 0; argumentIndex < requiredArgumentCount; ++argumentIndex) {
            placeholderTokens.append(QStringLiteral("value%1").arg(argumentIndex + 1));
        }
        if (placeholderTokens.isEmpty()) {
            const QStringList suggestedValues = commandValueTokens_.value(normalizedCommand);
            if (!suggestedValues.isEmpty()) {
                placeholderTokens.append(suggestedValues.first());
            }
        }

        QString line = QStringLiteral("%1%2").arg(childIndent, normalizedCommand);
        if (!placeholderTokens.isEmpty()) {
            line += QStringLiteral(" ") + placeholderTokens.join(QLatin1Char(' '));
        }
        linesToInsert << line;

        const QString closingDirective = completionClosingDirectiveForOpening(normalizedCommand);
        if (!closingDirective.isEmpty()) {
            linesToInsert << QStringLiteral("%1%2").arg(childIndent, closingDirective);
        }

        return linesToInsert;
    };

    if (normalizedKind == QStringLiteral("survey")
        || normalizedKind == QStringLiteral("map")
        || normalizedKind == QStringLiteral("scrap")) {
        const QStringList operations = {
            tr("Rename"),
            tr("Insert Command"),
        };
        bool ok = false;
        const QString selectedOperation = QInputDialog::getItem(this,
                                                                tr("Configure Block"),
                                                                tr("Action"),
                                                                operations,
                                                                0,
                                                                false,
                                                                &ok);
        if (!ok || selectedOperation.isEmpty()) {
            return;
        }

        if (selectedOperation == operations.at(0)) {
            bool renameOk = false;
            const QString currentName = parsedLine.tokens.value(1);
            const QString updatedName = QInputDialog::getText(this,
                                                              tr("Configure Block"),
                                                              tr("Name"),
                                                              QLineEdit::Normal,
                                                              currentName,
                                                              &renameOk);
            if (!renameOk || updatedName.trimmed().isEmpty()) {
                return;
            }
            const QString category = normalizedKind == QStringLiteral("survey")
                ? QStringLiteral("surveys")
                : (normalizedKind == QStringLiteral("map") ? QStringLiteral("maps") : QStringLiteral("scraps"));
            QString errorMessage;
            if (!rewriteStructureEntryName(lineNumber, category, updatedName, &errorMessage)) {
                QMessageBox::warning(this, tr("Update Failed"), errorMessage);
            }
            return;
        }

        const QStringList actions = buildInsertableCommandsForContext(normalizedKind, normalizedKind);
        if (actions.isEmpty()) {
            QMessageBox::information(this,
                                     tr("Configure Block"),
                                     tr("No catalog-driven insert commands are available for this context."));
            return;
        }

        bool selectOk = false;
        const QString selectedAction = QInputDialog::getItem(this,
                                                             tr("Configure %1").arg(normalizedKind),
                                                             tr("Insert Command"),
                                                             actions,
                                                             0,
                                                             false,
                                                             &selectOk);
        if (!selectOk || selectedAction.isEmpty()) {
            return;
        }

        const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
        const auto match = indentPattern.match(lines.at(lineNumber - 1));
        const QString indent = match.hasMatch() ? match.captured(0) : QString();
        const QString childIndent = indent + QStringLiteral("  ");
        const QStringList linesToInsert = buildInsertionTemplate(selectedAction, childIndent);
        if (linesToInsert.isEmpty()) {
            return;
        }

        const QString containerClosingDirective = closingDirectiveFor(normalizedKind);
        const int insertBeforeLine = findMatchingBlockEndLine(lines,
                                                              lineNumber,
                                                              normalizedKind,
                                                              containerClosingDirective);
        if (insertBeforeLine <= lineNumber) {
            QMessageBox::warning(this, tr("Configure Block"), tr("Unable to resolve closing directive for this block."));
            return;
        }

        QString errorMessage;
        if (!insertLinesBefore(insertBeforeLine, linesToInsert, &errorMessage)) {
            QMessageBox::warning(this, tr("Update Failed"), errorMessage);
        }
        return;
    }

    if (normalizedKind == QStringLiteral("centerline")) {
        const int centerlineEndLine = findMatchingBlockEndLine(lines,
                                                               lineNumber,
                                                               QStringLiteral("centerline"),
                                                               QStringLiteral("endcenterline"));
        if (centerlineEndLine <= lineNumber) {
            QMessageBox::warning(this, tr("Configure Block"), tr("Unable to resolve endcenterline for this block."));
            return;
        }

        QStringList actions = buildInsertableCommandsForContext(QStringLiteral("centerline"), normalizedKind);
        if (actions.isEmpty()) {
            actions = {QStringLiteral("team"), QStringLiteral("explo-date"), QStringLiteral("data")};
        }

        bool ok = false;
        const QString selectedAction = QInputDialog::getItem(this,
                                                             tr("Configure Centerline"),
                                                             tr("Insert Command"),
                                                             actions,
                                                             0,
                                                             false,
                                                             &ok);
        if (!ok || selectedAction.isEmpty()) {
            return;
        }

        const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
        const auto match = indentPattern.match(lines.at(lineNumber - 1));
        const QString indent = match.hasMatch() ? match.captured(0) : QString();
        const QString childIndent = indent + QStringLiteral("  ");

        const QStringList linesToInsert = buildInsertionTemplate(selectedAction, childIndent);
        if (linesToInsert.isEmpty()) {
            return;
        }

        QString errorMessage;
        if (!insertLinesBefore(centerlineEndLine, linesToInsert, &errorMessage)) {
            QMessageBox::warning(this, tr("Update Failed"), errorMessage);
        }
        return;
    }

    if (normalizedKind == QStringLiteral("data")) {
        int centerlineStartLine = -1;
        int centerlineDepth = 0;
        for (int currentLine = lineNumber; currentLine >= 1; --currentLine) {
            const TherionParsedLine currentParsedLine = TherionDocumentParser::parseLine(lines.at(currentLine - 1), currentLine);
            const QString directive = normalizeDirective(currentParsedLine.directive);
            if (directive == QStringLiteral("endcenterline")) {
                ++centerlineDepth;
                continue;
            }
            if (directive != QStringLiteral("centerline")) {
                continue;
            }
            if (centerlineDepth == 0) {
                centerlineStartLine = currentLine;
                break;
            }
            --centerlineDepth;
        }

        if (centerlineStartLine <= 0) {
            QMessageBox::warning(this, tr("Configure Data"), tr("Unable to resolve parent centerline block."));
            return;
        }

        const int centerlineEndLine = findMatchingBlockEndLine(lines,
                                                               centerlineStartLine,
                                                               QStringLiteral("centerline"),
                                                               QStringLiteral("endcenterline"));
        if (centerlineEndLine <= lineNumber) {
            QMessageBox::warning(this, tr("Configure Data"), tr("Unable to resolve endcenterline for this block."));
            return;
        }

        int dataBodyLastLine = centerlineEndLine - 1;
        for (int currentLine = lineNumber + 1; currentLine <= centerlineEndLine - 1; ++currentLine) {
            const TherionParsedLine currentParsedLine = TherionDocumentParser::parseLine(lines.at(currentLine - 1), currentLine);
            const QString directive = normalizeDirective(currentParsedLine.directive);
            if (directive.isEmpty() || directive == QStringLiteral("extend")) {
                continue;
            }
            if (directive == QStringLiteral("data")
                || directive == QStringLiteral("endcenterline")
                || isLikelyCenterlineCommand(directive)) {
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

        QStringList currentRows;
        for (int currentLine = lineNumber + 1; currentLine <= dataBodyLastLine; ++currentLine) {
            QString rowText = lines.at(currentLine - 1);
            if (rowText.startsWith(rowIndent)) {
                rowText = rowText.mid(rowIndent.size());
            } else {
                rowText = rowText.trimmed();
            }
            currentRows.append(rowText);
        }

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

        struct MixedRowEntry
        {
            bool directive = false;
            QString directiveText;
            QStringList dataValues;
        };
        QVector<MixedRowEntry> initialRows;
        for (const QString &rowText : currentRows) {
            const QStringList rowTokens = TherionDocumentParser::tokenizeLine(rowText);
            const bool looksLikeMeasurementRow = currentFieldCount > 0
                && rowTokens.size() == currentFieldCount
                && (rowTokens.isEmpty() || rowTokens.first().toLower() != QStringLiteral("extend"));
            if (looksLikeMeasurementRow) {
                initialRows.append(MixedRowEntry{false, QString(), rowTokens});
                continue;
            }

            initialRows.append(MixedRowEntry{true, rowText, {}});
        }

        QDialog dialog(this);
        dialog.setWindowTitle(tr("Configure Data Block"));
        dialog.setModal(true);
        auto *dialogLayout = new QVBoxLayout(&dialog);
        dialogLayout->setContentsMargins(12, 12, 12, 12);
        dialogLayout->setSpacing(8);

        auto *formLayout = new QFormLayout;
        auto *columnsEdit = new QLineEdit(currentColumns, &dialog);
        columnsEdit->setPlaceholderText(tr("normal from to compass clino tape"));
        formLayout->addRow(tr("Columns"), columnsEdit);
        dialogLayout->addLayout(formLayout);

        auto *tableHeaderLabel = new QLabel(tr("Rows (mix measurement rows and directives)"), &dialog);
        dialogLayout->addWidget(tableHeaderLabel);

        auto *rowActionsLayout = new QHBoxLayout;
        rowActionsLayout->setContentsMargins(0, 0, 0, 0);
        rowActionsLayout->setSpacing(6);
        auto *addDataRowButton = new QPushButton(tr("Add Data Row"), &dialog);
        auto *addDirectiveRowButton = new QPushButton(tr("Add Directive Row"), &dialog);
        auto *removeRowButton = new QPushButton(tr("Remove Row"), &dialog);
        addDataRowButton->setAutoDefault(false);
        addDirectiveRowButton->setAutoDefault(false);
        removeRowButton->setAutoDefault(false);
        rowActionsLayout->addWidget(addDataRowButton);
        rowActionsLayout->addWidget(addDirectiveRowButton);
        rowActionsLayout->addWidget(removeRowButton);
        rowActionsLayout->addStretch(1);
        dialogLayout->addLayout(rowActionsLayout);

        auto *rowsTable = new DataRowsTableWidget(&dialog);
        rowsTable->setMinimumHeight(220);
        rowsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        rowsTable->setSelectionMode(QAbstractItemView::SingleSelection);
        rowsTable->horizontalHeader()->setStretchLastSection(false);
        rowsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        rowsTable->verticalHeader()->setVisible(false);
        rowsTable->setAlternatingRowColors(true);
        dialogLayout->addWidget(rowsTable, 1);

        bool resizingDialogFromTable = false;
        auto refreshTableWidth = [&dialog, rowsTable, this, &resizingDialogFromTable]() {
            if (resizingDialogFromTable) {
                return;
            }
            if (rowsTable->columnCount() <= 0) {
                return;
            }

            rowsTable->resizeColumnsToContents();
            QVector<int> baseWidths;
            baseWidths.reserve(rowsTable->columnCount());
            int contentWidth = 0;
            for (int column = 0; column < rowsTable->columnCount(); ++column) {
                const int hintedWidth = qMax(rowsTable->horizontalHeader()->sectionSizeHint(column), rowsTable->columnWidth(column));
                const int columnWidth = qMax(hintedWidth, 80);
                baseWidths.append(columnWidth);
                contentWidth += columnWidth;
            }

            const int tableChromeWidth = rowsTable->frameWidth() * 2
                + rowsTable->verticalHeader()->width()
                + 28;
            const int requiredTableWidth = contentWidth + tableChromeWidth;
            rowsTable->setMinimumWidth(requiredTableWidth);

            int maxDialogWidth = 0;
            if (const QWidget *hostWindow = this->window(); hostWindow != nullptr) {
                maxDialogWidth = qMax(maxDialogWidth, hostWindow->width() - 32);
            }
            QScreen *screen = dialog.screen();
            if (screen == nullptr) {
                screen = QGuiApplication::primaryScreen();
            }
            if (screen != nullptr) {
                maxDialogWidth = qMax(maxDialogWidth, int(screen->availableGeometry().width() * 0.95));
            }
            if (maxDialogWidth <= 0) {
                maxDialogWidth = requiredTableWidth + 96;
            }

            const int desiredDialogWidth = requiredTableWidth + 96;
            const int targetDialogWidth = qMin(desiredDialogWidth, maxDialogWidth);
            dialog.setMinimumWidth(qMin(desiredDialogWidth, maxDialogWidth));
            if (dialog.width() < targetDialogWidth) {
                resizingDialogFromTable = true;
                dialog.resize(targetDialogWidth, qMax(dialog.height(), dialog.sizeHint().height()));
                resizingDialogFromTable = false;
            }

            const int viewportWidth = rowsTable->viewport()->width();
            if (viewportWidth <= 0 || contentWidth <= 0) {
                return;
            }

            if (contentWidth < viewportWidth) {
                const int extra = viewportWidth - contentWidth;
                const int perColumn = extra / rowsTable->columnCount();
                int remainder = extra % rowsTable->columnCount();
                for (int column = 0; column < rowsTable->columnCount(); ++column) {
                    int expandedWidth = baseWidths.at(column) + perColumn;
                    if (remainder > 0) {
                        ++expandedWidth;
                        --remainder;
                    }
                    rowsTable->setColumnWidth(column, expandedWidth);
                }
                return;
            }

            for (int column = 0; column < rowsTable->columnCount(); ++column) {
                rowsTable->setColumnWidth(column, baseWidths.at(column));
            }
        };

        auto applyColumnsToTable = [rowsTable, parseDataFields](const QString &columnsText) {
            const QStringList fieldNames = parseDataFields(columnsText);

            const int oldRowCount = rowsTable->rowCount();
            const int oldColumnCount = rowsTable->columnCount();
            QVector<QStringList> oldCells;
            oldCells.reserve(oldRowCount);
            for (int row = 0; row < oldRowCount; ++row) {
                QStringList rowValues;
                rowValues.reserve(oldColumnCount);
                for (int column = 0; column < oldColumnCount; ++column) {
                    const QTableWidgetItem *item = rowsTable->item(row, column);
                    rowValues.append(item != nullptr ? item->text() : QString());
                }
                oldCells.append(rowValues);
            }

            const int dataColumnCount = qMax(1, fieldNames.size());
            const int targetColumnCount = dataColumnCount + 2; // Type + data columns + Directive
            rowsTable->clear();
            rowsTable->setColumnCount(targetColumnCount);
            QStringList headers;
            headers.append(QObject::tr("Type"));
            if (fieldNames.isEmpty()) {
                headers.append(QObject::tr("value"));
            } else {
                headers.append(fieldNames);
            }
            headers.append(QObject::tr("Directive"));
            rowsTable->setHorizontalHeaderLabels(headers);

            if (rowsTable->rowCount() == 0) {
                rowsTable->setRowCount(qMax(1, oldRowCount));
            }

            const int oldDataColumnCount = qMax(1, oldColumnCount - 2);
            for (int row = 0; row < oldCells.size(); ++row) {
                if (row >= rowsTable->rowCount()) {
                    rowsTable->insertRow(rowsTable->rowCount());
                }
                const QStringList &oldRow = oldCells.at(row);

                QString typeValue = oldRow.value(0).trimmed();
                if (typeValue.isEmpty()) {
                    typeValue = QStringLiteral("data");
                }
                rowsTable->setItem(row, 0, new QTableWidgetItem(typeValue));

                for (int column = 0; column < qMin(oldDataColumnCount, dataColumnCount); ++column) {
                    const QString value = oldRow.value(1 + column);
                    if (!value.isEmpty()) {
                        rowsTable->setItem(row, 1 + column, new QTableWidgetItem(value));
                    }
                }

                const QString directiveValue = oldRow.value(oldColumnCount - 1);
                if (!directiveValue.isEmpty()) {
                    rowsTable->setItem(row, targetColumnCount - 1, new QTableWidgetItem(directiveValue));
                }
            }

            rowsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
            const int lastDataColumn = targetColumnCount - 2;
            const int directiveColumn = targetColumnCount - 1;
            rowsTable->setAdvanceColumns({lastDataColumn, directiveColumn});
        };

        applyColumnsToTable(currentColumns);
        rowsTable->setRowCount(qMax(1, initialRows.size()));
        for (int row = 0; row < initialRows.size(); ++row) {
            const MixedRowEntry &entry = initialRows.at(row);
            rowsTable->setItem(row, 0, new QTableWidgetItem(entry.directive ? QStringLiteral("directive") : QStringLiteral("data")));
            if (entry.directive) {
                rowsTable->setItem(row, rowsTable->columnCount() - 1, new QTableWidgetItem(entry.directiveText));
            } else {
                const int dataColumnCount = rowsTable->columnCount() - 2;
                for (int column = 0; column < qMin(dataColumnCount, entry.dataValues.size()); ++column) {
                    rowsTable->setItem(row, 1 + column, new QTableWidgetItem(entry.dataValues.at(column)));
                }
            }
        }
        rowsTable->onViewportResized = [refreshTableWidth]() {
            refreshTableWidth();
        };
        refreshTableWidth();
        QTimer::singleShot(0, &dialog, [refreshTableWidth]() {
            refreshTableWidth();
        });

        connect(columnsEdit, &QLineEdit::textChanged, &dialog, [applyColumnsToTable](const QString &text) {
            applyColumnsToTable(text);
        });
        connect(columnsEdit, &QLineEdit::textChanged, &dialog, [refreshTableWidth](const QString &) {
            refreshTableWidth();
        });
        connect(addDataRowButton, &QPushButton::clicked, &dialog, [rowsTable, refreshTableWidth] {
            const int insertRow = rowsTable->currentRow() >= 0 ? rowsTable->currentRow() + 1 : rowsTable->rowCount();
            rowsTable->insertRow(insertRow);
            rowsTable->setItem(insertRow, 0, new QTableWidgetItem(QStringLiteral("data")));
            rowsTable->setCurrentCell(insertRow, 1);
            refreshTableWidth();
        });
        connect(addDirectiveRowButton, &QPushButton::clicked, &dialog, [rowsTable, refreshTableWidth] {
            const int insertRow = rowsTable->currentRow() >= 0 ? rowsTable->currentRow() + 1 : rowsTable->rowCount();
            rowsTable->insertRow(insertRow);
            rowsTable->setItem(insertRow, 0, new QTableWidgetItem(QStringLiteral("directive")));
            rowsTable->setItem(insertRow, rowsTable->columnCount() - 1, new QTableWidgetItem(QStringLiteral("extend right")));
            rowsTable->setCurrentCell(insertRow, rowsTable->columnCount() - 1);
            rowsTable->editItem(rowsTable->item(insertRow, rowsTable->columnCount() - 1));
            refreshTableWidth();
        });
        connect(removeRowButton, &QPushButton::clicked, &dialog, [rowsTable, refreshTableWidth] {
            if (rowsTable->rowCount() <= 1) {
                for (int column = 0; column < rowsTable->columnCount(); ++column) {
                    delete rowsTable->takeItem(0, column);
                }
                refreshTableWidth();
                return;
            }
            const int rowToRemove = rowsTable->currentRow() >= 0 ? rowsTable->currentRow() : (rowsTable->rowCount() - 1);
            rowsTable->removeRow(rowToRemove);
            refreshTableWidth();
        });

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        dialogLayout->addWidget(buttonBox);
        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        const QString updatedColumns = columnsEdit->text().trimmed();
        if (updatedColumns.isEmpty()) {
            QMessageBox::warning(this, tr("Configure Data"), tr("Columns cannot be empty."));
            return;
        }

        const QStringList updatedFieldNames = parseDataFields(updatedColumns);
        if (updatedFieldNames.isEmpty()) {
            QMessageBox::warning(this, tr("Configure Data"), tr("Columns must define at least one data field."));
            return;
        }

        QStringList orderedRows;
        const int dataColumnCount = rowsTable->columnCount() - 2;
        const int directiveColumn = rowsTable->columnCount() - 1;
        for (int row = 0; row < rowsTable->rowCount(); ++row) {
            const QString typeValue = (rowsTable->item(row, 0) != nullptr
                                           ? rowsTable->item(row, 0)->text()
                                           : QString())
                                          .trimmed()
                                          .toLower();
            const QString directiveText = (rowsTable->item(row, directiveColumn) != nullptr
                                               ? rowsTable->item(row, directiveColumn)->text()
                                               : QString())
                                              .trimmed();

            QStringList dataValues;
            dataValues.reserve(dataColumnCount);
            bool hasDataValues = false;
            for (int column = 0; column < dataColumnCount; ++column) {
                const QString value = (rowsTable->item(row, 1 + column) != nullptr
                                           ? rowsTable->item(row, 1 + column)->text()
                                           : QString())
                                          .trimmed();
                if (!value.isEmpty()) {
                    hasDataValues = true;
                }
                dataValues.append(value);
            }

            const bool preferDirective = typeValue.startsWith(QStringLiteral("dir"));
            const bool treatAsDirective = preferDirective || (!directiveText.isEmpty() && !hasDataValues);

            if (!hasDataValues && directiveText.isEmpty()) {
                continue;
            }

            if (treatAsDirective) {
                if (directiveText.isEmpty()) {
                    QMessageBox::warning(this,
                                         tr("Configure Data"),
                                         tr("Row %1 is marked as directive but directive text is empty.").arg(row + 1));
                    return;
                }
                orderedRows.append(directiveText);
                continue;
            }

            for (int column = 0; column < dataValues.size(); ++column) {
                if (dataValues.at(column).isEmpty()) {
                    QMessageBox::warning(this,
                                         tr("Configure Data"),
                                         tr("Row %1 has an empty value in column %2.")
                                             .arg(row + 1)
                                             .arg(column + 1));
                    return;
                }
            }
            orderedRows.append(dataValues.join(QLatin1Char(' ')));
        }

        QStringList replacementLines;
        replacementLines.append(QStringLiteral("%1data %2").arg(dataIndent, updatedColumns));
        for (const QString &rowText : orderedRows) {
            if (rowText.trimmed().isEmpty()) {
                continue;
            }
            replacementLines.append(QStringLiteral("%1%2").arg(rowIndent, rowText));
        }

        const int replaceStartIndex = lineNumber - 1;
        const int replaceEndIndex = dataBodyLastLine - 1;
        for (int index = replaceEndIndex; index >= replaceStartIndex; --index) {
            lines.removeAt(index);
        }
        for (int offset = 0; offset < replacementLines.size(); ++offset) {
            lines.insert(replaceStartIndex + offset, replacementLines.at(offset));
        }

        const QString lineEnding = contents.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
        replaceTextForCommand(lines.join(lineEnding));
        return;
    }

    const bool hasCatalogOptions = !commandOptionTokens_.value(normalizedKind).isEmpty();
    if (!isContainerBlockDirective(normalizedKind)
        && normalizedKind != QStringLiteral("data")
        && !hasCatalogOptions) {
        bool ok = false;
        const QString currentValue = parsedLine.tokens.size() > 1
            ? parsedLine.tokens.mid(1).join(QLatin1Char(' '))
            : QString();
        const QString updatedValue = QInputDialog::getText(this,
                                                           tr("Configure Block"),
                                                           tr("Value"),
                                                           QLineEdit::Normal,
                                                           currentValue,
                                                           &ok);
        if (!ok || updatedValue.trimmed().isEmpty()) {
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

void TextEditorTab::handleBlockDeleteRequest(int lineNumber)
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
    const QString directive = normalizeDirective(parsedLine.directive);
    if (directive.isEmpty()) {
        return;
    }

    int removeStartLine = lineNumber;
    int removeEndLine = lineNumber;

    if (isContainerBlockDirective(directive)) {
        const QString closingDirective = closingDirectiveFor(directive);
        const int endLine = findMatchingBlockEndLine(lines, lineNumber, directive, closingDirective);
        if (endLine <= lineNumber) {
            QMessageBox::warning(this,
                                 tr("Delete Block"),
                                 tr("Unable to resolve closing directive for `%1`.").arg(directive));
            return;
        }
        removeEndLine = endLine;
    } else if (directive == QStringLiteral("data")) {
        int centerlineStartLine = -1;
        int centerlineDepth = 0;
        for (int currentLine = lineNumber; currentLine >= 1; --currentLine) {
            const TherionParsedLine currentParsedLine = TherionDocumentParser::parseLine(lines.at(currentLine - 1), currentLine);
            const QString currentDirective = normalizeDirective(currentParsedLine.directive);
            if (currentDirective == QStringLiteral("endcenterline")) {
                ++centerlineDepth;
                continue;
            }
            if (currentDirective != QStringLiteral("centerline")) {
                continue;
            }
            if (centerlineDepth == 0) {
                centerlineStartLine = currentLine;
                break;
            }
            --centerlineDepth;
        }

        if (centerlineStartLine <= 0) {
            QMessageBox::warning(this, tr("Delete Block"), tr("Unable to resolve parent centerline block."));
            return;
        }

        const int centerlineEndLine = findMatchingBlockEndLine(lines,
                                                               centerlineStartLine,
                                                               QStringLiteral("centerline"),
                                                               QStringLiteral("endcenterline"));
        if (centerlineEndLine <= lineNumber) {
            QMessageBox::warning(this, tr("Delete Block"), tr("Unable to resolve endcenterline for this block."));
            return;
        }

        int dataBodyLastLine = centerlineEndLine - 1;
        for (int currentLine = lineNumber + 1; currentLine <= centerlineEndLine - 1; ++currentLine) {
            const TherionParsedLine currentParsedLine = TherionDocumentParser::parseLine(lines.at(currentLine - 1), currentLine);
            const QString currentDirective = normalizeDirective(currentParsedLine.directive);
            if (currentDirective.isEmpty() || currentDirective == QStringLiteral("extend")) {
                continue;
            }
            if (currentDirective == QStringLiteral("data")
                || currentDirective == QStringLiteral("endcenterline")
                || isLikelyCenterlineCommand(currentDirective)) {
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
        return;
    }

    const int removeStartIndex = removeStartLine - 1;
    const int removeEndIndex = qMin(lines.size() - 1, removeEndLine - 1);
    for (int index = removeEndIndex; index >= removeStartIndex; --index) {
        lines.removeAt(index);
    }

    const QString lineEnding = contents.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    replaceTextForCommand(lines.join(lineEnding));
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
    framedHelpPanel->setFrameShape(QFrame::StyledPanel);
    helpPanel_ = framedHelpPanel;
    auto *panelLayout = new QVBoxLayout(helpPanel_);
    panelLayout->setContentsMargins(8, 8, 8, 8);
    panelLayout->setSpacing(6);

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
    commandTypeValueTokens_.clear();
    commandSubtypeByTypeTokens_.clear();
    commandRequiredPositionalCount_.clear();
    contextCommandTokens_.clear();
    blockCommandContextsByKind_.clear();
    loadHelpMetadataFromCommandCatalog();
    rebuildCompletionModel();
}

void TextEditorTab::loadHelpMetadataFromCommandCatalog()
{
    QFile catalogFile(QStringLiteral(":/resources/therion_command_catalog.json"));
    if (!catalogFile.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(catalogFile.readAll());
    if (!document.isObject()) {
        return;
    }

    const QJsonArray commands = document.object().value(QStringLiteral("commands")).toArray();
    for (const QJsonValue &commandValue : commands) {
        const QJsonObject commandObject = commandValue.toObject();
        const QString commandName = commandObject.value(QStringLiteral("name")).toString().trimmed().toLower();
        if (commandName.isEmpty()) {
            continue;
        }

        TherionHelpEntry entry;
        entry.summary = commandObject.value(QStringLiteral("summary")).toString().trimmed();
        int requiredPositionalCount = 0;

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
            const QString description = argumentObject.value(QStringLiteral("description")).toString().trimmed();
            const QString argumentLine = description.isEmpty() ? signature : QStringLiteral("%1 = %2").arg(signature, description);
            appendUnique(entry.arguments, argumentLine);
            if (isRequiredArgumentSignature(signature)) {
                ++requiredPositionalCount;
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
            const QString valueArity = optionObject.value(QStringLiteral("value_arity")).toString().trimmed().toUpper();
            const QString optionLine = description.isEmpty() ? signature : QStringLiteral("%1 = %2").arg(signature, description);
            appendUnique(entry.options, optionLine);
            const QStringList normalizedOptionKeys = extractOptionKeys(optionKey);
            QStringList normalizedOptionValues;
            for (const QString &normalizedOptionKey : normalizedOptionKeys) {
                appendUnique(entry.relatedKeywords, normalizedOptionKey);
                appendUnique(commandOptionTokens_[commandName], normalizedOptionKey);
                if (!valueArity.isEmpty()) {
                    commandOptionValueArityTokens_.insert(commandOptionValueKey(commandName, normalizedOptionKey), valueArity);
                }
                registerCompletionToken(normalizedOptionKey);
            }
            const QJsonArray optionValues = optionObject.value(QStringLiteral("allowed_values")).toArray();
            for (const QJsonValue &optionValue : optionValues) {
                const QString normalizedValue = optionValue.toString().trimmed();
                appendUnique(normalizedOptionValues, normalizedValue);
            }
            if (!normalizedOptionValues.isEmpty()) {
                for (const QString &normalizedOptionKey : normalizedOptionKeys) {
                    appendUniqueList(commandOptionValueTokens_[commandOptionValueKey(commandName, normalizedOptionKey)],
                                     normalizedOptionValues);
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

        if (commandName == QStringLiteral("centreline")
            || commandName == QStringLiteral("centerline")) {
            QStringList inlineCommands;
            const QJsonArray inlineCommandArray = commandObject.value(QStringLiteral("inline_commands")).toArray();
            for (const QJsonValue &inlineCommandValue : inlineCommandArray) {
                appendUnique(inlineCommands, inlineCommandValue.toString());
            }

            for (const QString &inlineCommand : inlineCommands) {
                appendUnique(contextCommandTokens_[QStringLiteral("centerline")], inlineCommand.toLower());
                registerCompletionToken(inlineCommand);
            }
        }

        appendUnique(commandCompletionTokens_, commandName);
        commandRequiredPositionalCount_.insert(commandName, requiredPositionalCount);
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

    return legacyIsCompatibleChildKind(normalizedParent, normalizedChild);
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

        if (!completionClosingDirectiveForOpening(directive).isEmpty()) {
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

    const QString scope = scopeStack.constLast();
    if (scope == QStringLiteral("centerline")) {
        return QStringLiteral("centerline");
    }
    return scope;
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
                            const bool multiValueOption = arity == QStringLiteral("N");
                            const bool singleValueOption = arity == QStringLiteral("1");
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

            if (activeValueContext.arity == QStringLiteral("N")
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
        if (arity == QStringLiteral("1") && valueOrdinal != 1) {
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
        if (!collapsed && helpPanelHeight_ > 0) {
            const QList<int> sizes = editorHelpSplitter_->sizes();
            editorHelpSplitter_->setSizes(QList<int>{sizes.value(0, 1), helpPanelHeight_});
        } else if (collapsed) {
            const QList<int> sizes = editorHelpSplitter_->sizes();
            if (sizes.size() >= 2) {
                helpPanelHeight_ = qMax(sizes.at(1), helpPanel_->minimumSizeHint().height());
            }
            editorHelpSplitter_->setSizes(QList<int>{1, 0});
        }
    }
}

QString TextEditorTab::renderHelpHtml(const QString &token, const TherionHelpEntry &entry) const
{
    QString html;
    html += QStringLiteral("<h3>%1</h3>").arg(token.toHtmlEscaped());

    if (!entry.summary.isEmpty()) {
        html += QStringLiteral("<p><strong>Summary:</strong> %1</p>").arg(entry.summary.toHtmlEscaped());
    }
    if (!entry.syntax.isEmpty()) {
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
