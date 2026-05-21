#include "TextEditorTab.h"
#include "block_editor/BlockEditorApplyExecutor.h"
#include "block_editor/BlockEditorApplyStateController.h"
#include "block_editor/BlockEditorCanvasSelectionController.h"
#include "block_editor/BlockEditorCanvasView.h"
#include "block_editor/BlockEditorCanvasBoundaryController.h"
#include "block_editor/BlockEditorCanvasItem.h"
#include "block_editor/BlockEditorCanvasRebuildController.h"
#include "block_editor/BlockEditorConfigureController.h"
#include "block_editor/BlockEditorDeleteExecutor.h"
#include "block_editor/BlockEditorDetailsHelpController.h"
#include "block_editor/BlockEditorDetailsPaneController.h"
#include "block_editor/BlockEditorDocumentOutlineBuilder.h"
#include "block_editor/BlockEditorDirectiveRules.h"
#include "block_editor/BlockEditorInsertionController.h"
#include "block_editor/BlockEditorLineBuildService.h"
#include "block_editor/BlockEditorMoveController.h"
#include "block_editor/BlockEditorMovePreviewController.h"
#include "block_editor/BlockEditorOptionTableDelegate.h"
#include "block_editor/BlockEditorOptionArgsController.h"
#include "block_editor/BlockEditorSelectionDetailsController.h"
#include "block_editor/BlockEditorSourceController.h"
#include "block_editor/BlockEditorSourceText.h"
#include "block_editor/BlockEditorToolboxController.h"
#include "block_editor/BlockEditorTokenTagEditor.h"
#include "block_editor/BlockEditorToolboxList.h"
#include "block_editor/BlockEditorToolboxDetailsController.h"
#include "raw_editor/RawEditorCompletionController.h"
#include "raw_editor/RawEditorFindController.h"
#include "raw_editor/RawEditorTextEdit.h"
#include "TextEditorAppearanceController.h"
#include "TextEditorContextHelpController.h"
#include "TextEditorCursorController.h"
#include "TextEditorModeController.h"
#include "TextEditorSourceRewriteController.h"
#include "TextEditorStatusController.h"
#include "TextEditorSurfaceStyler.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QFrame>
#include <QCompleter>
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
#include <QMouseEvent>
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
#include <QToolTip>
#include <QHeaderView>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyleHints>
#include <QStringListModel>
#include <QVBoxLayout>
#include <QMessageBox>

#include <QPen>

#include <algorithm>
#include <functional>
#include <limits>

#include "../../core/DocumentFile.h"
#include "../../core/TherionCommandSyntax.h"
#include "../../editor/TherionSyntaxHighlighter.h"

namespace
{
using namespace TherionStudio::BlockEditorDirectiveRules;

constexpr int kPanelPadding = 12;
constexpr int kPanelSpacing = 8;
constexpr int kBlocksSidePaneMinWidth = 320;
constexpr int kBlocksSidePaneMaxWidth = 460;

void applyThinSplitterStyle(QSplitter *splitter, const QString &objectName)
{
    if (splitter == nullptr) {
        return;
    }

    splitter->setObjectName(objectName);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QString());
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

TherionStudio::BlockEditorTokenTagEditor *asTokenTagEditor(QWidget *widget)
{
    return dynamic_cast<TherionStudio::BlockEditorTokenTagEditor *>(widget);
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

}

namespace TherionStudio
{
TextEditorTab::TextEditorTab(QWidget *parent)
    : QWidget(parent)
{
    appearanceController_ = std::make_unique<TextEditorAppearanceController>(this);
    contextHelpController_ = std::make_unique<TextEditorContextHelpController>(this);
    cursorController_ = std::make_unique<TextEditorCursorController>(this);
    rawEditorFindController_ = std::make_unique<RawEditorFindController>(this);
    rawEditorCompletionController_ = std::make_unique<RawEditorCompletionController>(this);
    editorModeController_ = std::make_unique<TextEditorModeController>(this);
    sourceRewriteController_ = std::make_unique<TextEditorSourceRewriteController>(this);
    statusController_ = std::make_unique<TextEditorStatusController>(this);
    blockDetailsOptionArgsController_ = std::make_unique<BlockEditorOptionArgsController>(this);
    blockDetailsHelpController_ = std::make_unique<BlockEditorDetailsHelpController>(this);
    blockDetailsLineBuildService_ = std::make_unique<BlockEditorLineBuildService>(this);
    blockDetailsApplyStateController_ = std::make_unique<BlockEditorApplyStateController>(this);
    blockDetailsApplyExecutor_ = std::make_unique<BlockEditorApplyExecutor>(this);
    blockDetailsSelectionDetailsController_ = std::make_unique<BlockEditorSelectionDetailsController>(this);
    blockDetailsPaneController_ = std::make_unique<BlockEditorDetailsPaneController>(this);
    blockToolboxController_ = std::make_unique<BlockEditorToolboxController>(this);
    blockDetailsToolboxDetailsController_ = std::make_unique<BlockEditorToolboxDetailsController>(this);
    blockDetailsCanvasSelectionController_ = std::make_unique<BlockEditorCanvasSelectionController>(this);
    blockDetailsMovePreviewController_ = std::make_unique<BlockEditorMovePreviewController>(this);
    blockDetailsCanvasBoundaryController_ = std::make_unique<BlockEditorCanvasBoundaryController>(this);
    blockDetailsCanvasRebuildController_ = std::make_unique<BlockEditorCanvasRebuildController>(this);

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

    editor_ = new RawEditorTextEdit(this);
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

    blockToolboxList_ = new BlockEditorToolboxList(toolboxColumn);
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
    auto *typedCanvasView = new BlockEditorCanvasView(blocksSplitter);
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
    blockDetailsReadingsTagEditor_ = new BlockEditorTokenTagEditor(blockDetailsSecondaryFieldStack_);
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
        new BlockEditorOptionTableDelegate(
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
    if (appearanceController_ != nullptr) {
        appearanceController_->handleApplicationAppearanceChanged();
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
    if (cursorController_ != nullptr) {
        cursorController_->goToLine(lineNumber);
    }
}

void TextEditorTab::goToLineColumn(int lineNumber, int columnNumber)
{
    if (cursorController_ != nullptr) {
        cursorController_->goToLineColumn(lineNumber, columnNumber);
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
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteStructureEntryName(lineNumber, category, newName, errorMessage);
}

bool TextEditorTab::insertScrapBlock(const QString &preferredName,
                                     int *insertedLineNumber,
                                     QString *errorMessage,
                                     const QString &options)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertScrapBlock(preferredName, insertedLineNumber, errorMessage, options);
}

bool TextEditorTab::insertDraftGeometry(const QString &kind,
                                        const QVector<QPointF> &vertices,
                                        int *insertedLineNumber,
                                        QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertDraftGeometry(kind, vertices, insertedLineNumber, errorMessage);
}

bool TextEditorTab::insertDraftLineGeometry(const QStringList &coordinateRows,
                                            int *insertedLineNumber,
                                            QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertDraftLineGeometry(coordinateRows, insertedLineNumber, errorMessage);
}

bool TextEditorTab::insertDraftAreaGeometry(const QStringList &coordinateRows,
                                            int *insertedLineNumber,
                                            QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertDraftAreaGeometry(coordinateRows, insertedLineNumber, errorMessage);
}

bool TextEditorTab::rewritePointCoordinates(int lineNumber,
                                            const QPointF &point,
                                            QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewritePointCoordinates(lineNumber, point, errorMessage);
}

bool TextEditorTab::rewriteLineAreaVertex(int lineNumber,
                                          const QString &kind,
                                          int vertexIndex,
                                          const QPointF &point,
                                          QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteLineAreaVertex(lineNumber, kind, vertexIndex, point, errorMessage);
}

bool TextEditorTab::rewriteLineOptionToggle(int lineNumber,
                                            const QString &optionName,
                                            bool enabled,
                                            QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteLineOptionToggle(lineNumber, optionName, enabled, errorMessage);
}

bool TextEditorTab::rewritePointOrientation(int lineNumber,
                                            bool enabled,
                                            qreal orientationDegrees,
                                            QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewritePointOrientation(lineNumber, enabled, orientationDegrees, errorMessage);
}

bool TextEditorTab::rewriteLinePointOrientation(int lineNumber,
                                                int sourceVertexIndex,
                                                bool enabled,
                                                qreal orientationDegrees,
                                                QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteLinePointOrientation(lineNumber,
                                                                 sourceVertexIndex,
                                                                 enabled,
                                                                 orientationDegrees,
                                                                 errorMessage);
}

bool TextEditorTab::rewriteLinePointLeftSize(int lineNumber,
                                             int sourceVertexIndex,
                                             bool enabled,
                                             qreal sizeValue,
                                             QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteLinePointLeftSize(lineNumber,
                                                              sourceVertexIndex,
                                                              enabled,
                                                              sizeValue,
                                                              errorMessage);
}

bool TextEditorTab::rewriteLineCoordinateRows(int lineNumber,
                                              const QStringList &coordinateRows,
                                              QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteLineCoordinateRows(lineNumber, coordinateRows, errorMessage);
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
    if (sourceRewriteController_ != nullptr) {
        sourceRewriteController_->replaceTextForCommand(contents);
    }
}

QString TextEditorTab::filePath() const
{
    return filePath_;
}

QString TextEditorTab::displayName() const
{
    return statusController_ != nullptr ? statusController_->displayName() : tr("Untitled");
}

bool TextEditorTab::isDirty() const
{
    return dirty_;
}

int TextEditorTab::currentLineNumber() const
{
    return cursorController_ != nullptr ? cursorController_->currentLineNumber() : 1;
}

int TextEditorTab::currentColumnNumber() const
{
    return cursorController_ != nullptr ? cursorController_->currentColumnNumber() : 1;
}

QString TextEditorTab::text() const
{
    return editor_->toPlainText();
}

QColor TextEditorTab::sourceSurfaceColor() const
{
    if (appearanceController_ == nullptr) {
        return palette().color(QPalette::Window);
    }
    return appearanceController_->sourceSurfaceColor();
}

QString TextEditorTab::statusPathText() const
{
    return statusController_ != nullptr ? statusController_->statusPathText() : tr("No file open");
}

QString TextEditorTab::statusEncodingText() const
{
    return statusController_ != nullptr ? statusController_->statusEncodingText() : QStringLiteral("UTF-8");
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
    if (statusController_ != nullptr) {
        statusController_->setInlineStatusVisible(visible);
    }
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
    return editorModeController_ != nullptr
        && editorModeController_->isBlocksModeSupportedForCurrentFile();
}

void TextEditorTab::refreshBlocksModeAvailability()
{
    if (editorModeController_ != nullptr) {
        editorModeController_->refreshBlocksModeAvailability();
    }
}

void TextEditorTab::setBlocksModeActive(bool active)
{
    if (editorModeController_ != nullptr) {
        editorModeController_->setBlocksModeActive(active);
    }
}

bool TextEditorTab::ensureEncodingRootDirectiveForBlocks()
{
    return editorModeController_ != nullptr
        && editorModeController_->ensureEncodingRootDirectiveForBlocks();
}

void TextEditorTab::refreshEditorModeUi()
{
    if (editorModeController_ != nullptr) {
        editorModeController_->refreshEditorModeUi();
    }
}

void TextEditorTab::populateBlockToolbox()
{
    if (blockToolboxController_ != nullptr) {
        blockToolboxController_->populateBlockToolbox();
    }
}

void TextEditorTab::populateBlockToolboxScopeCombo()
{
    if (blockToolboxController_ != nullptr) {
        blockToolboxController_->populateBlockToolboxScopeCombo();
    }
}

QString TextEditorTab::selectedBlockInsertionContextToken() const
{
    return blockToolboxController_ != nullptr
        ? blockToolboxController_->selectedBlockInsertionContextToken()
        : QStringLiteral("none");
}

void TextEditorTab::updateBlockToolboxScopeLabel()
{
    if (blockToolboxController_ != nullptr) {
        blockToolboxController_->updateBlockToolboxScopeLabel();
    }
}

void TextEditorTab::rebuildBlocksCanvasFromText()
{
    if (blockDetailsCanvasRebuildController_ != nullptr) {
        blockDetailsCanvasRebuildController_->rebuildBlocksCanvasFromText();
    }
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
    return blockDetailsCanvasSelectionController_ != nullptr
        ? blockDetailsCanvasSelectionController_->resolveBlockCanvasItem(item)
        : nullptr;
}

int TextEditorTab::blockCanvasItemLineNumber(const QGraphicsItem *item) const
{
    return blockDetailsCanvasSelectionController_ != nullptr
        ? blockDetailsCanvasSelectionController_->blockCanvasItemLineNumber(item)
        : 0;
}

QString TextEditorTab::blockCanvasItemKind(const QGraphicsItem *item) const
{
    return blockDetailsCanvasSelectionController_ != nullptr
        ? blockDetailsCanvasSelectionController_->blockCanvasItemKind(item)
        : QString();
}

void TextEditorTab::selectBlockCanvasItem(QGraphicsItem *item, bool centerView)
{
    if (blockDetailsCanvasSelectionController_ != nullptr) {
        blockDetailsCanvasSelectionController_->selectBlockCanvasItem(item, centerView);
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

void TextEditorTab::handleCanvasDrop(const QString &kind, const QPointF &scenePos)
{
    BlockEditorInsertionController(this).insertFromDrop(kind, scenePos);
}

void TextEditorTab::handleBlockMoveRequest(int lineNumber, const QPointF &scenePos)
{
    BlockEditorMoveController(this).moveBlock(lineNumber, scenePos);
}

void TextEditorTab::handleBlockConfigureRequest(const QString &kind, int lineNumber)
{
    BlockEditorConfigureController(this).configureBlock(kind, lineNumber);
}

bool TextEditorTab::handleBlockDeleteRequest(int lineNumber)
{
    BlockEditorDeleteExecutor deleteExecutor(this);
    return deleteExecutor.deleteCommandAtLine(lineNumber);
}

bool TextEditorTab::isCurrentStateDirty() const
{
    return statusController_ != nullptr && statusController_->isCurrentStateDirty();
}

void TextEditorTab::applyDirtyStateFromCurrentState()
{
    if (statusController_ != nullptr) {
        statusController_->applyDirtyStateFromCurrentState();
    }
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
    if (cursorController_ != nullptr) {
        cursorController_->handleCursorPositionChanged();
    }
}

void TextEditorTab::refreshCurrentLineHighlight()
{
    if (cursorController_ != nullptr) {
        cursorController_->refreshCurrentLineHighlight();
    }
}

void TextEditorTab::refreshTitle()
{
    if (statusController_ != nullptr) {
        statusController_->refreshTitle();
    }
}

void TextEditorTab::refreshStatus()
{
    if (statusController_ != nullptr) {
        statusController_->refreshStatus();
    }
}

void TextEditorTab::buildHelpPanel()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->buildHelpPanel();
    }
}

void TextEditorTab::loadHelpMetadata()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->loadHelpMetadata();
    }
}

void TextEditorTab::loadHelpMetadataFromCommandCatalog()
{
    if (contextHelpController_ != nullptr) {
        contextHelpController_->loadHelpMetadataFromCommandCatalog();
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
    if (contextHelpController_ != nullptr) {
        contextHelpController_->setHelpCollapsed(collapsed);
    }
}

QString TextEditorTab::displayPath() const
{
    return statusController_ != nullptr ? statusController_->displayPath() : tr("No file open");
}

}
