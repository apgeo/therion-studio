#include "TextEditorTab.h"

#include "block_editor/BlockEditorCanvasBoundaryController.h"
#include "block_editor/BlockEditorCanvasRebuildController.h"

#include <QApplication>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyleHints>
#include <QVBoxLayout>

namespace
{
constexpr int kPanelPadding = 12;
constexpr int kPanelSpacing = 8;
}

namespace TherionStudio
{
void TextEditorTab::buildAll()
{
    buildContextHelpController();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    buildRawEditorPanel();
    buildCursorController();
    buildDocumentController();
    buildEncodingController();
    buildSourceRewriteController();
    buildStatusController();
    buildModeSelectorRow();

    buildBlockToolboxController();
    buildBlockEditorPanel();
    buildBlockDetailsOptionArgsController();
    buildBlockDetailsHelpController();
    buildBlockDetailsLineBuildService();
    buildBlockDetailsApplyStateController();
    buildBlockDetailsApplyExecutor();
    buildBlockDetailsSelectionDetailsController();
    buildBlockDetailsPaneController();
    buildBlockDetailsToolboxDetailsController();
    buildBlockDetailsCanvasSelectionController();
    buildBlockDetailsMovePreviewController();
    blockDetailsCanvasBoundaryController_ = std::make_unique<BlockEditorCanvasBoundaryController>(blockEditorCanvasBoundaryContext());
    blockDetailsCanvasRebuildController_ = std::make_unique<BlockEditorCanvasRebuildController>(blockEditorCanvasRebuildContext());
    buildAppearanceController();

    editorModeStack_ = new QStackedWidget(this);
    editorModeStack_->addWidget(rawEditorPanel_);
    editorModeStack_->addWidget(blocksPanel_);
    buildModeController();

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

    initializeEditorUiState();
}

void TextEditorTab::buildModeSelectorRow()
{
    modeRow_ = new QWidget(this);
    auto *modeLayout = new QHBoxLayout(modeRow_);
    modeLayout->setContentsMargins(kPanelPadding, kPanelSpacing, kPanelPadding, kPanelSpacing);
    modeLayout->setSpacing(kPanelSpacing);
    modeLayout->addWidget(new QLabel(tr("Mode:"), modeRow_));
    rawModeButton_ = new QPushButton(tr("Raw"), modeRow_);
    rawModeButton_->setCheckable(true);
    blocksModeButton_ = new QPushButton(tr("Blocks"), modeRow_);
    blocksModeButton_->setCheckable(true);
    blocksModeButton_->setToolTip(tr("Structured block canvas for .th and Therion config files."));
    modeLayout->addWidget(rawModeButton_);
    modeLayout->addWidget(blocksModeButton_);
    modeLayout->addStretch(1);
    modeRow_->setMaximumHeight(modeSelectorRequestedVisible_ ? QWIDGETSIZE_MAX : 0);
}

void TextEditorTab::initializeEditorUiState()
{
    refreshBlocksModeAvailability();
    refreshEditorModeUi();
    rebuildBlocksCanvasFromText();
    clearBlockDetailsPane();
    refreshStatus();
    refreshCurrentLineHighlight();
    updateContextHelp();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QStyleHints *styleHints = QGuiApplication::styleHints()) {
        connect(styleHints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            handleApplicationAppearanceChanged();
        });
    }
#endif
    if (qApp != nullptr) {
        qApp->installEventFilter(this);
    }
    handleApplicationAppearanceChanged();
}
}
