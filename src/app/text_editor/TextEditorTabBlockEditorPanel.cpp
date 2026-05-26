#include "TextEditorTab.h"

#include "block_editor/BlockEditorCanvasView.h"
#include "block_editor/BlockEditorDetailsSupport.h"
#include "block_editor/BlockEditorDirectiveRules.h"
#include "block_editor/BlockEditorOptionTableDelegate.h"
#include "block_editor/BlockEditorTokenTagEditor.h"
#include "block_editor/BlockEditorToolboxList.h"
#include "TextEditorSurfaceStyler.h"

#include "../../core/TherionCommandSyntax.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFormLayout>
#include <QFont>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QModelIndex>
#include <QPalette>
#include <QPointF>
#include <QPointer>
#include <QPushButton>
#include <QRectF>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace
{
constexpr int kPanelPadding = 12;
constexpr int kPanelSpacing = 8;
constexpr int kBlocksSidePaneMinWidth = 320;
constexpr int kBlocksSidePaneMaxWidth = 460;

QStringList filteredOptionTokenSuggestions(const QStringList &source)
{
    QStringList filtered;
    for (const QString &candidate : source) {
        const QString normalized = candidate.trimmed().toLower();
        if (!TherionStudio::looksLikeOptionToken(normalized)) {
            continue;
        }
        if (!filtered.contains(normalized, Qt::CaseInsensitive)) {
            filtered.append(normalized);
        }
    }
    filtered.sort(Qt::CaseInsensitive);
    return filtered;
}

void applyThinSplitterStyle(QSplitter *splitter, const QString &objectName)
{
    if (splitter == nullptr) {
        return;
    }

    splitter->setObjectName(objectName);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QString());
}

}

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

void TextEditorTab::buildBlockEditorPanel()
{
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
    blockCanvasScene_->setSceneRect(QRectF(0.0, 0.0, 1.0, 1.0));
    typedCanvasView->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    typedCanvasView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    typedCanvasView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    typedCanvasView->setBackgroundBrush(palette().color(QPalette::Base));
    QPointer<TextEditorTab> tabGuard(this);
    typedCanvasView->onDropBlock = [tabGuard](const QString &kind, const QPointF &scenePos) {
        if (tabGuard.isNull() || tabGuard->tearingDown_) {
            return;
        }
        tabGuard->handleCanvasDrop(kind, scenePos);
    };
    typedCanvasView->onDragPreview = [tabGuard](const QString &, const QPointF &scenePos, bool active) {
        if (tabGuard.isNull() || tabGuard->tearingDown_) {
            return;
        }
        if (active) {
            tabGuard->updateBlockMovePreview(-1, scenePos);
        } else {
            tabGuard->clearBlockMovePreview();
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
    blockDetailsOptionsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    blockDetailsOptionsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    blockDetailsOptionsTable_->setColumnWidth(0, 170);
    blockDetailsOptionsTable_->verticalHeader()->setVisible(false);
    blockDetailsOptionsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    blockDetailsOptionsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    blockDetailsOptionsTable_->setAlternatingRowColors(true);
    blockDetailsOptionsTable_->setMinimumHeight(140);
    blockDetailsOptionsTable_->setItemDelegate(
        new BlockEditorOptionTableDelegate(
            [this](const QModelIndex &index) {
                if (index.column() == 0) {
                    return filteredOptionTokenSuggestions(
                        commandMetadata().commandOptionTokens.value(blockDetailsSelectedKind_));
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
                    return commandMetadata().commandOptionValueTokens.value(
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
    if (auto *readingsTagEditor = blockEditorTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
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
}
}
