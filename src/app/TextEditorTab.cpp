#include "TextEditorTab.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QFrame>
#include <QFileInfo>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTextBrowser>
#include <QTextCursor>
#include <QToolButton>
#include <QPushButton>
#include <QTextDocument>
#include <QVBoxLayout>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTextBlock>

#include "../core/DocumentFile.h"
#include "../core/TherionDocumentEditor.h"
#include "../core/TherionDocumentParser.h"
#include "../editor/TherionSyntaxHighlighter.h"

namespace
{
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

    editor_ = new QPlainTextEdit(this);
    editor_->setTabChangesFocus(false);
    editor_->setPlaceholderText(tr("Open a Therion text file to begin editing."));
    highlighter_ = new TherionSyntaxHighlighter(editor_->document());
    loadHelpMetadata();

    statusRow_ = new QWidget(this);
    auto *statusLayout = new QHBoxLayout(statusRow_);
    statusLayout->setContentsMargins(8, 0, 8, 8);
    statusLayout->setSpacing(12);

    pathLabel_ = new QLabel(tr("No file open"), statusRow_);
    pathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    encodingLabel_ = new QLabel(tr("Encoding: UTF-8"), statusRow_);
    encodingLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    convertEncodingButton_ = new QPushButton(tr("Convert to UTF-8"), statusRow_);
    convertEncodingButton_->setVisible(false);
    convertEncodingButton_->setEnabled(false);
    convertEncodingButton_->setAutoDefault(false);
    connect(convertEncodingButton_, &QPushButton::clicked, this, &TextEditorTab::handleConvertToUtf8Triggered);

    statusLayout->addWidget(pathLabel_, 1);
    statusLayout->addWidget(encodingLabel_);
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
    installHelpBorderToggle();

    layout->addWidget(searchBar_);
    layout->addWidget(editorHelpSplitter_, 1);
    layout->addWidget(statusRow_);

    connect(editor_, &QPlainTextEdit::textChanged, this, &TextEditorTab::handleTextChanged);
    connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, &TextEditorTab::handleCursorPositionChanged);

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
    loading_ = true;
    editor_->setPlainText(contents);
    loading_ = false;
    editor_->document()->setModified(false);
    dirty_ = false;
    refreshTitle();
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

    editor_->document()->setModified(false);
    dirty_ = false;
    refreshTitle();
    emit dirtyStateChanged(false);
    return true;
}

void TextEditorTab::goToLine(int lineNumber)
{
    if (lineNumber <= 0) {
        return;
    }

    const QTextBlock block = editor_->document()->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid()) {
        return;
    }

    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    editor_->setTextCursor(cursor);
    editor_->centerCursor();
    editor_->ensureCursorVisible();
    updateContextHelp();

    if (searchBar_->isVisible()) {
        findEdit_->clearFocus();
    }
}

void TextEditorTab::showFindBar(bool replaceMode)
{
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
    editor_->document()->setModified(true);
    dirty_ = true;
    refreshTitle();
    emit dirtyStateChanged(true);
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
    editor_->document()->setModified(true);
    dirty_ = true;
    refreshTitle();
    emit dirtyStateChanged(true);
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
    editor_->document()->setModified(true);
    dirty_ = true;
    refreshTitle();
    emit dirtyStateChanged(true);
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
    editor_->document()->setModified(true);
    dirty_ = true;
    refreshTitle();
    emit dirtyStateChanged(true);
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
    editor_->document()->setModified(true);
    dirty_ = true;
    refreshTitle();
    emit dirtyStateChanged(true);
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
    editor_->document()->setModified(true);
    dirty_ = true;
    refreshTitle();
    emit dirtyStateChanged(true);
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
    editor_->document()->setModified(true);
    dirty_ = true;
    refreshTitle();
    emit dirtyStateChanged(true);
    emit documentTextChanged();
    updateContextHelp();
    return true;
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
    if (statusRow_ != nullptr) {
        statusRow_->setVisible(visible);
    }
}

void TextEditorTab::handleTextChanged()
{
    if (loading_) {
        return;
    }

    if (!dirty_) {
        dirty_ = true;
        refreshTitle();
        emit dirtyStateChanged(true);
    }

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

void TextEditorTab::handleConvertToUtf8Triggered()
{
    if (fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        return;
    }

    fileEncodingName_ = QStringLiteral("UTF-8");
    fileEncodingLabel_ = QStringLiteral("UTF-8");

    if (!dirty_) {
        dirty_ = true;
        emit dirtyStateChanged(true);
    }

    refreshTitle();
}

void TextEditorTab::handleCursorPositionChanged()
{
    if (loading_) {
        return;
    }

    const int currentLineNumber = editor_->textCursor().blockNumber() + 1;
    if (currentLineNumber != currentLineNumber_) {
        currentLineNumber_ = currentLineNumber;
        emit currentLineChanged(currentLineNumber_);
    }

    updateContextHelp();
}

void TextEditorTab::refreshTitle()
{
    refreshStatus();
    emit titleChanged();
}

void TextEditorTab::refreshStatus()
{
    if (filePath_.isEmpty()) {
        pathLabel_->setText(tr("No file open"));
    } else {
        pathLabel_->setText(displayPath());
    }

    encodingLabel_->setText(tr("Encoding: %1").arg(fileEncodingLabel_));
    if (convertEncodingButton_ != nullptr) {
        const bool showConversion = fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) != 0;
        convertEncodingButton_->setVisible(showConversion);
        convertEncodingButton_->setEnabled(showConversion);
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

void TextEditorTab::installHelpBorderToggle()
{
    if (editorHelpSplitter_ == nullptr || helpBorderToggleButton_ != nullptr) {
        return;
    }

    auto *handle = editorHelpSplitter_->handle(1);
    if (handle == nullptr) {
        return;
    }

    auto *handleLayout = new QHBoxLayout(handle);
    handleLayout->setContentsMargins(0, 0, 4, 0);
    handleLayout->setSpacing(0);
    handleLayout->addStretch(1);

    helpBorderToggleButton_ = new QToolButton(handle);
    helpBorderToggleButton_->setAutoRaise(true);
    helpBorderToggleButton_->setFocusPolicy(Qt::NoFocus);
    helpBorderToggleButton_->setFixedSize(12, 12);
    helpBorderToggleButton_->setToolTip(tr("Collapse or expand help pane"));
    connect(helpBorderToggleButton_, &QToolButton::clicked, this, [this]() {
        setHelpCollapsed(!helpCollapsed_);
    });

    handleLayout->addWidget(helpBorderToggleButton_);
    refreshHelpBorderToggle();
}

void TextEditorTab::refreshHelpBorderToggle()
{
    if (helpBorderToggleButton_ == nullptr) {
        return;
    }

    helpBorderToggleButton_->setArrowType(helpCollapsed_ ? Qt::RightArrow : Qt::DownArrow);
}

void TextEditorTab::loadHelpMetadata()
{
    QFile helpFile(QStringLiteral(":/resources/therion_help_metadata.json"));
    if (!helpFile.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(helpFile.readAll());
    if (!document.isObject()) {
        return;
    }

    const QJsonObject commandsObject = document.object().value(QStringLiteral("commands")).toObject();
    for (auto iterator = commandsObject.begin(); iterator != commandsObject.end(); ++iterator) {
        const QJsonObject entryObject = iterator.value().toObject();
        TherionHelpEntry entry;
        entry.summary = entryObject.value(QStringLiteral("summary")).toString();
        entry.syntax = entryObject.value(QStringLiteral("syntax")).toString();

        const QJsonArray argumentArray = entryObject.value(QStringLiteral("arguments")).toArray();
        for (const QJsonValue &value : argumentArray) {
            entry.arguments.append(value.toString());
        }

        const QJsonArray acceptedValuesArray = entryObject.value(QStringLiteral("acceptedValues")).toArray();
        for (const QJsonValue &value : acceptedValuesArray) {
            entry.acceptedValues.append(value.toString());
        }

        const QJsonArray optionsArray = entryObject.value(QStringLiteral("options")).toArray();
        for (const QJsonValue &value : optionsArray) {
            entry.options.append(value.toString());
        }

        const QJsonArray relatedKeywordsArray = entryObject.value(QStringLiteral("relatedKeywords")).toArray();
        for (const QJsonValue &value : relatedKeywordsArray) {
            entry.relatedKeywords.append(value.toString());
        }

        helpEntries_.insert(iterator.key().toLower(), entry);
    }
}

QStringList TextEditorTab::helpCandidateTokens() const
{
    QStringList candidates;
    const QString directToken = currentHelpTokenForCursor();
    if (!directToken.isEmpty()) {
        candidates.append(directToken);
    }

    const QTextCursor cursor = editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return candidates;
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
    if (!parsedLine.directive.isEmpty() && parsedLine.directive != directToken.toLower()) {
        candidates.append(parsedLine.directive);
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

void TextEditorTab::updateContextHelp()
{
    if (helpBrowser_ == nullptr) {
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
    refreshHelpBorderToggle();
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
    if (!entry.relatedKeywords.isEmpty()) {
        html += QStringLiteral("<h4>Related Keywords</h4>");
        html += renderList(entry.relatedKeywords);
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
