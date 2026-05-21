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
#include "TextEditorContextHelpController.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
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
#include <QTextDocument>
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

void applyThinSplitterStyle(QSplitter *splitter, const QString &objectName)
{
    if (splitter == nullptr) {
        return;
    }

    splitter->setObjectName(objectName);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QString());
}

void syncPanelSurfaceToBaseTone(QWidget *panelWidget);
void syncPanelSurfaceToPalette(QWidget *panelWidget, const QPalette &sourcePalette);
void syncTextBrowserSurfaceToParent(QWidget *browserWidget);
void syncTextBrowserSurfaceToPalette(QWidget *browserWidget, const QPalette &sourcePalette);

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
    QStringList lines = blockEditorNormalizedSourceLines(contents);
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

    const QString normalizedContents = blockEditorJoinSourceLines(contents, normalizedLines);
    if (normalizedContents == blockEditorJoinSourceLines(contents, lines)) {
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

    if (auto *highlightEditor = dynamic_cast<RawEditorTextEdit *>(editor_)) {
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
