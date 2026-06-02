#include "TextEditorTab.h"

#include "block_editor/BlockEditorCanvasView.h"
#include "block_editor/BlockEditorDetailsSupport.h"
#include "block_editor/BlockEditorDirectiveRules.h"
#include "block_editor/BlockEditorOptionTableDelegate.h"
#include "block_editor/BlockEditorTokenTagEditor.h"
#include "block_editor/BlockEditorToolboxList.h"
#include "ContextHelpInspector.h"
#include "DocumentFileInspector.h"
#include "DocumentInspectorPanel.h"
#include "InspectorPanel.h"
#include "TextEditorSurfaceStyler.h"

#include "../../core/TherionCommandSyntax.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFormLayout>
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
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <utility>

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

    blockEditorSplitter_ = new QSplitter(Qt::Horizontal, blocksPanel_);
    blockEditorSplitter_->setChildrenCollapsible(false);
    applyThinSplitterStyle(blockEditorSplitter_, QStringLiteral("textBlocksSplitter"));

    auto *toolboxColumn = new QWidget(blockEditorSplitter_);
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

    blockCanvasScene_ = new QGraphicsScene(blockEditorSplitter_);
    auto *typedCanvasView = new BlockEditorCanvasView(blockEditorSplitter_);
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

    auto *inspectorPanel = new DocumentInspectorPanel(blockEditorSplitter_);
    blockDetailsPanel_ = inspectorPanel;
    blockDetailsPanel_->setObjectName(QStringLiteral("blockInspectorPanel"));
    blockDetailsPanel_->setMinimumWidth(kBlocksSidePaneMinWidth);
    blockDetailsPanel_->setMaximumWidth(kBlocksSidePaneMaxWidth);
    syncPanelSurfaceToBaseTone(blockDetailsPanel_);

    auto *detailsTabPanel = inspectorPanel->addScrollTab(tr("Selection"));
    auto *detailsTabLayout = qobject_cast<QVBoxLayout *>(detailsTabPanel->layout());

    blockDetailsEditPanel_ = new QWidget(detailsTabPanel);
    blockDetailsEditPanel_->setProperty("preserveNativeChildControls", true);
    syncPanelSurfaceToBaseTone(blockDetailsEditPanel_);
    auto *blockDetailsEditLayout = new QVBoxLayout(blockDetailsEditPanel_);
    blockDetailsEditLayout->setContentsMargins(0, 0, 0, 0);
    blockDetailsEditLayout->setSpacing(kPanelSpacing);

    QVBoxLayout *blockDetailsSectionLayout = nullptr;
    auto *blockDetailsSection = InspectorPanel::createSection(blockDetailsEditPanel_,
                                                              tr("Selection"),
                                                              &blockDetailsSectionLayout,
                                                              &blockDetailsTitleLabel_);

    blockDetailsStatusLabel_ = new QLabel(blockDetailsSection);
    blockDetailsStatusLabel_->setObjectName(QStringLiteral("blockDetailsStatusLabel"));
    blockDetailsStatusLabel_->setWordWrap(true);
    blockDetailsStatusLabel_->setStyleSheet(QStringLiteral("color: palette(mid);"));
    blockDetailsSectionLayout->addWidget(blockDetailsStatusLabel_);

    auto *blockDetailsFormLayout = new QFormLayout;
    blockDetailsFormLayout->setContentsMargins(0, 0, 0, 0);
    blockDetailsFormLayout->setSpacing(kPanelSpacing);
    blockDetailsPrimaryFieldLabel_ = new QLabel(tr("ID"), blockDetailsSection);
    blockDetailsPrimaryFieldLabel_->setObjectName(QStringLiteral("blockDetailsPrimaryLabel"));
    blockDetailsPrimaryFieldStack_ = new QStackedWidget(blockDetailsSection);
    blockDetailsPrimaryFieldStack_->setObjectName(QStringLiteral("blockDetailsPrimaryFieldStack"));
    blockDetailsIdEdit_ = new QLineEdit(blockDetailsPrimaryFieldStack_);
    blockDetailsIdEdit_->setObjectName(QStringLiteral("blockDetailsPrimaryEdit"));
    blockDetailsReadOnlyValueLabel_ = new QLabel(blockDetailsPrimaryFieldStack_);
    blockDetailsReadOnlyValueLabel_->setObjectName(QStringLiteral("blockDetailsReadOnlyValueLabel"));
    blockDetailsReadOnlyValueLabel_->setWordWrap(true);
    blockDetailsPrimaryFieldStack_->addWidget(blockDetailsIdEdit_);
    blockDetailsPrimaryFieldStack_->addWidget(blockDetailsReadOnlyValueLabel_);
    blockDetailsPrimaryFieldStack_->setCurrentWidget(blockDetailsIdEdit_);
    blockDetailsFormLayout->addRow(blockDetailsPrimaryFieldLabel_, blockDetailsPrimaryFieldStack_);
    blockDetailsSecondaryFieldLabel_ = new QLabel(tr("Extra Arguments (Advanced)"), blockDetailsSection);
    blockDetailsSecondaryFieldLabel_->setObjectName(QStringLiteral("blockDetailsSecondaryLabel"));
    blockDetailsSecondaryFieldStack_ = new QStackedWidget(blockDetailsSection);
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
    blockDetailsCommentFieldLabel_ = new QLabel(tr("Comment"), blockDetailsSection);
    blockDetailsCommentFieldLabel_->setObjectName(QStringLiteral("blockDetailsCommentLabel"));
    blockDetailsCommentEdit_ = new QLineEdit(blockDetailsSection);
    blockDetailsCommentEdit_->setObjectName(QStringLiteral("blockDetailsCommentEdit"));
    blockDetailsCommentEdit_->setPlaceholderText(tr("optional"));
    blockDetailsFormLayout->addRow(blockDetailsCommentFieldLabel_, blockDetailsCommentEdit_);
    blockDetailsSectionLayout->addLayout(blockDetailsFormLayout);

    blockDetailsAddOptionButton_ = new QPushButton(QStringLiteral("+"), blockDetailsSection);
    blockDetailsRemoveOptionButton_ = new QPushButton(QStringLiteral("-"), blockDetailsSection);
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
    blockDetailsOptionsLabel_ = new QLabel(tr("Options"), blockDetailsSection);
    blockDetailsOptionsLabel_->setObjectName(QStringLiteral("blockDetailsOptionsLabel"));
    blockDetailsOptionsHeaderRow->addWidget(blockDetailsOptionsLabel_);
    blockDetailsOptionsHeaderRow->addStretch(1);
    blockDetailsOptionsHeaderRow->addWidget(blockDetailsAddOptionButton_);
    blockDetailsOptionsHeaderRow->addWidget(blockDetailsRemoveOptionButton_);
    blockDetailsSectionLayout->addLayout(blockDetailsOptionsHeaderRow);

    blockDetailsOptionsTable_ = new QTableWidget(blockDetailsSection);
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
    blockDetailsOptionsTable_->setMinimumHeight(120);
    blockDetailsOptionsTable_->setMaximumHeight(260);
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
    blockDetailsSectionLayout->addWidget(blockDetailsOptionsTable_);

    blockDetailsOptionArgsLabel_ = new QLabel(tr("Selected Option Parameters"), blockDetailsSection);
    blockDetailsOptionArgsLabel_->setObjectName(QStringLiteral("blockDetailsOptionArgsLabel"));
    blockDetailsSectionLayout->addWidget(blockDetailsOptionArgsLabel_);

    blockDetailsOptionArgsPanel_ = new QWidget(blockDetailsSection);
    blockDetailsOptionArgsPanel_->setObjectName(QStringLiteral("blockDetailsOptionArgsPanel"));
    blockDetailsOptionArgsFormLayout_ = new QFormLayout(blockDetailsOptionArgsPanel_);
    blockDetailsOptionArgsFormLayout_->setContentsMargins(0, 0, 0, 0);
    blockDetailsOptionArgsFormLayout_->setSpacing(kPanelSpacing);
    blockDetailsOptionArgsLabel_->setVisible(false);
    blockDetailsOptionArgsPanel_->setVisible(false);
    blockDetailsSectionLayout->addWidget(blockDetailsOptionArgsPanel_);

    auto *blockDetailsButtonsRow = new QHBoxLayout;
    blockDetailsButtonsRow->setContentsMargins(0, 0, 0, 0);
    blockDetailsButtonsRow->setSpacing(kPanelSpacing);
    blockDetailsDataRowsButton_ = new QPushButton(tr("Edit Data Rows..."), blockDetailsSection);
    blockDetailsDataRowsButton_->setObjectName(QStringLiteral("blockDetailsDataRowsButton"));
    blockDetailsDataRowsButton_->setAutoDefault(false);
    blockDetailsDataRowsButton_->setVisible(false);
    blockDetailsDataRowsButton_->setEnabled(false);
    blockDetailsButtonsRow->addStretch(1);
    blockDetailsButtonsRow->addWidget(blockDetailsDataRowsButton_);
    blockDetailsSectionLayout->addLayout(blockDetailsButtonsRow);

    blockDetailsEditLayout->addWidget(blockDetailsSection);

    detailsTabLayout->addWidget(blockDetailsEditPanel_);
    detailsTabLayout->addStretch(1);
    auto *helpTabPanel = inspectorPanel->addScrollTab(tr("Context Help"));
    auto *helpTabLayout = qobject_cast<QVBoxLayout *>(helpTabPanel->layout());

    blockDetailsHelpInspector_ = new ContextHelpInspector(helpTabPanel, tr("Context Help"));
    blockDetailsHelpPanel_ = blockDetailsHelpInspector_;
    blockDetailsHelpBrowser_ = blockDetailsHelpInspector_->browser();
    blockDetailsHelpBrowser_->setObjectName(QStringLiteral("blockDetailsHelpBrowser"));
    helpTabLayout->addWidget(blockDetailsHelpInspector_, 1);

    DocumentFileInspectorContext fileContext;
    fileContext.filePath = [this]() {
        return filePath_;
    };
    fileContext.encodingName = [this]() {
        return fileEncodingName_;
    };
    fileContext.encodingLabel = [this]() {
        return fileEncodingLabel_;
    };
    fileContext.convertToUtf8 = [this]() {
        triggerConvertToUtf8();
    };
    blockFileInspector_ = inspectorPanel->addFileTab(std::move(fileContext));

    blockEditorSplitter_->addWidget(toolboxColumn);
    blockEditorSplitter_->addWidget(blockCanvasView_);
    blockEditorSplitter_->addWidget(blockDetailsPanel_);
    blockEditorSplitter_->setCollapsible(2, true);
    blockEditorSplitter_->setStretchFactor(0, 0);
    blockEditorSplitter_->setStretchFactor(1, 1);
    blockEditorSplitter_->setStretchFactor(2, 0);
    blockEditorSplitter_->setSizes({220, 980, 380});
    blocksLayout->addWidget(blockEditorSplitter_, 1);

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
            applyBlockDetailsChanges();
        }
    });
    connect(blockDetailsIdEdit_, &QLineEdit::selectionChanged, this, [this]() {
        updateBlockDetailsHelpForCurrentFocus();
    });
    connect(blockDetailsIdEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        updateBlockDetailsHelpForCurrentFocus();
        refreshBlockDetailsApplyState();
    });
    connect(blockDetailsIdEdit_, &QLineEdit::editingFinished, this, [this]() {
        applyBlockDetailsChanges();
    });
    connect(blockDetailsAdditionalPositionalEdit_, &QLineEdit::selectionChanged, this, [this]() {
        updateBlockDetailsHelpForCurrentFocus();
    });
    connect(blockDetailsAdditionalPositionalEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        updateBlockDetailsHelpForCurrentFocus();
        refreshBlockDetailsApplyState();
    });
    connect(blockDetailsAdditionalPositionalEdit_, &QLineEdit::editingFinished, this, [this]() {
        applyBlockDetailsChanges();
    });
    if (auto *readingsTagEditor = blockEditorTokenTagEditor(blockDetailsReadingsTagEditor_); readingsTagEditor != nullptr) {
        readingsTagEditor->onTokensChanged = [this, readingsTagEditor]() {
            if (blockDetailsAdditionalPositionalEdit_ != nullptr) {
                blockDetailsAdditionalPositionalEdit_->setText(readingsTagEditor->tokens().join(QLatin1Char(' ')));
            }
            updateBlockDetailsHelpForCurrentFocus();
            refreshBlockDetailsApplyState();
            applyBlockDetailsChanges();
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
    connect(blockDetailsCommentEdit_, &QLineEdit::editingFinished, this, [this]() {
        applyBlockDetailsChanges();
    });
    connect(blockDetailsAddOptionButton_, &QPushButton::clicked, this, [this]() {
        if (blockDetailsOptionsTable_ == nullptr
            || blockDetailsPopulating_
            || blockDetailsMode_ != BlockDetailsMode::StructuredOptions) {
            return;
        }
        const int row = blockDetailsOptionsTable_->rowCount();
        {
            const QSignalBlocker blocker(blockDetailsOptionsTable_);
            blockDetailsOptionsTable_->insertRow(row);
            blockDetailsOptionsTable_->setItem(row, 0, new QTableWidgetItem(QString()));
            blockDetailsOptionsTable_->setItem(row, 1, new QTableWidgetItem(QString()));
        }
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
        applyBlockDetailsChanges();
    });
    connect(blockDetailsDataRowsButton_, &QPushButton::clicked, this, [this]() {
        if (blockDetailsSelectedLineNumber_ <= 0 || blockDetailsSelectedKind_.isEmpty()) {
            return;
        }
        handleBlockConfigureRequest(blockDetailsSelectedKind_, blockDetailsSelectedLineNumber_);
    });
}
}
