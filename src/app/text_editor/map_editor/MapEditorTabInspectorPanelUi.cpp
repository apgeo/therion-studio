#include "MapEditorTab.h"

#include "MapEditorStylePreviewWidget.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace TherionStudio
{
namespace
{
void configureSelectionEditableCombo(QComboBox *combo, const QString &objectName)
{
    if (combo == nullptr) {
        return;
    }

    combo->setObjectName(objectName);
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    if (QCompleter *completer = combo->completer(); completer != nullptr) {
        completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
    }
}
}

void MapEditorTab::buildInspectorPanelUi()
{
    objectDetailsPanel_ = new QFrame(mapDetailsSplitter_);
    objectDetailsPanel_->setObjectName(QStringLiteral("mapObjectDetailsPanel"));
    objectDetailsPanel_->setFrameShape(QFrame::NoFrame);
    objectDetailsPanel_->setAttribute(Qt::WA_StyledBackground, true);
    objectDetailsPanel_->setStyleSheet(QStringLiteral(
        "QFrame#mapObjectDetailsPanel {"
        " background-color: palette(base);"
        " border: none;"
        "}"));
    objectDetailsPanel_->setMinimumWidth(280);
    auto *objectDetailsLayout = new QVBoxLayout(objectDetailsPanel_);
    objectDetailsLayout->setContentsMargins(8, 8, 8, 8);
    objectDetailsLayout->setSpacing(8);

    mapInspectorTabs_ = new QTabWidget(objectDetailsPanel_);
    mapInspectorTabs_->installEventFilter(this);
    mapInspectorLeftEdge_ = new QFrame(mapInspectorTabs_);
    mapInspectorLeftEdge_->setObjectName(QStringLiteral("mapInspectorLeftEdge"));
    mapInspectorLeftEdge_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    mapInspectorLeftEdge_->setFrameShape(QFrame::NoFrame);
    mapInspectorLeftEdge_->setFixedWidth(1);
    mapInspectorLeftEdge_->setStyleSheet(QStringLiteral(
        "QFrame#mapInspectorLeftEdge {"
        " background-color: palette(mid);"
        " border: none;"
        "}"));
    mapInspectorLeftEdge_->raise();
    objectDetailsLayout->addWidget(mapInspectorTabs_, 1);

    auto *objectsTab = new QWidget(mapInspectorTabs_);
    auto *objectsLayout = new QVBoxLayout(objectsTab);
    objectsLayout->setContentsMargins(4, 4, 4, 4);
    objectsLayout->setSpacing(8);

    mapObjectsTree_ = new QTreeView(objectsTab);
    mapObjectsTree_->setObjectName(QStringLiteral("mapObjectsTree"));
    mapObjectsTree_->setRootIsDecorated(true);
    mapObjectsTree_->setAnimated(true);
    mapObjectsTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mapObjectsTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    mapObjectsTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mapObjectsTree_->setAlternatingRowColors(true);
    mapObjectsTree_->setHeaderHidden(true);
    mapObjectsTree_->setIconSize(QSize(16, 16));
    mapObjectsModel_ = new QStandardItemModel(mapObjectsTree_);
    mapObjectsTree_->setModel(mapObjectsModel_);
    configureInspectorObjectTreeColumns();
    if (mapObjectsTree_->viewport() != nullptr) {
        mapObjectsTree_->viewport()->installEventFilter(this);
    }
    connect(mapObjectsTree_, &QTreeView::clicked, this, &MapEditorTab::handleInspectorObjectClicked);
    objectsLayout->addWidget(mapObjectsTree_, 1);
    if (mapObjectsTree_->selectionModel() != nullptr) {
        connect(mapObjectsTree_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &) {
            handleInspectorObjectSelectionChanged(current);
        });
    }

    QFont sectionFont = objectsTab->font();
    sectionFont.setBold(true);

    auto *selectionTab = new QWidget(mapInspectorTabs_);
    auto *selectionTabLayout = new QVBoxLayout(selectionTab);
    selectionTabLayout->setContentsMargins(4, 4, 4, 4);
    selectionTabLayout->setSpacing(8);

    auto *selectionPanel = new QWidget(selectionTab);
    auto *selectionLayout = new QVBoxLayout(selectionPanel);
    selectionLayout->setContentsMargins(0, 0, 0, 0);
    selectionLayout->setSpacing(8);

    objectDetailsUiState_.objectDetailsSelectionLabel_ = new QLabel(tr("No map object selected."), selectionPanel);
    objectDetailsUiState_.objectDetailsSelectionLabel_->setWordWrap(true);
    selectionLayout->addWidget(objectDetailsUiState_.objectDetailsSelectionLabel_);

    auto createSelectionSection = [selectionPanel](const QString &title,
                                                   QVBoxLayout **contentLayout,
                                                   QLabel **titleLabelOut = nullptr) {
        auto *section = new QFrame(selectionPanel);
        section->setFrameShape(QFrame::StyledPanel);
        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(8, 8, 8, 8);
        sectionLayout->setSpacing(6);
        auto *titleLabel = new QLabel(title, section);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        sectionLayout->addWidget(titleLabel);
        if (titleLabelOut != nullptr) {
            *titleLabelOut = titleLabel;
        }
        if (contentLayout != nullptr) {
            *contentLayout = sectionLayout;
        }
        return section;
    };

    QVBoxLayout *objectSelectionLayout = nullptr;
    objectDetailsUiState_.objectSelectionSection_ = createSelectionSection(tr("Object"), &objectSelectionLayout, &objectDetailsUiState_.objectSelectionTitleLabel_);

    objectDetailsUiState_.objectDetailsMetadataLabel_ = new QLabel(QStringLiteral("-"), objectDetailsUiState_.objectSelectionSection_);
    objectDetailsUiState_.objectDetailsMetadataLabel_->setTextFormat(Qt::PlainText);
    objectDetailsUiState_.objectDetailsMetadataLabel_->setStyleSheet(QStringLiteral("QLabel { color: palette(midlight); }"));
    objectSelectionLayout->addWidget(objectDetailsUiState_.objectDetailsMetadataLabel_);

    objectDetailsUiState_.objectAreaReferenceLabel_ = new QLabel(objectDetailsUiState_.objectSelectionSection_);
    objectDetailsUiState_.objectAreaReferenceLabel_->setTextFormat(Qt::RichText);
    objectDetailsUiState_.objectAreaReferenceLabel_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    objectDetailsUiState_.objectAreaReferenceLabel_->setOpenExternalLinks(false);
    objectDetailsUiState_.objectAreaReferenceLabel_->setWordWrap(true);
    objectDetailsUiState_.objectAreaReferenceLabel_->setStyleSheet(QStringLiteral("QLabel { color: palette(midlight); }"));
    connect(objectDetailsUiState_.objectAreaReferenceLabel_, &QLabel::linkActivated, this, [this](const QString &link) {
        bool ok = false;
        const int areaLineNumber = link.toInt(&ok);
        if (ok && areaLineNumber > 0) {
            selectMapLine(areaLineNumber);
            syncInspectorObjectSelectionToLine(areaLineNumber);
        }
    });
    objectSelectionLayout->addWidget(objectDetailsUiState_.objectAreaReferenceLabel_);

    objectDetailsUiState_.objectQuickFieldsEditor_ = new QWidget(objectDetailsUiState_.objectSelectionSection_);
    auto *objectQuickForm = new QFormLayout(objectDetailsUiState_.objectQuickFieldsEditor_);
    objectQuickForm->setContentsMargins(0, 0, 0, 0);
    objectQuickForm->setSpacing(6);
    objectDetailsUiState_.objectQuickTypeCombo_ = new QComboBox(objectDetailsUiState_.objectQuickFieldsEditor_);
    configureSelectionEditableCombo(objectDetailsUiState_.objectQuickTypeCombo_, QStringLiteral("mapObjectQuickTypeCombo"));
    objectDetailsUiState_.objectQuickSubtypeCombo_ = new QComboBox(objectDetailsUiState_.objectQuickFieldsEditor_);
    configureSelectionEditableCombo(objectDetailsUiState_.objectQuickSubtypeCombo_, QStringLiteral("mapObjectQuickSubtypeCombo"));
    objectDetailsUiState_.objectQuickProjectionCombo_ = new QComboBox(objectDetailsUiState_.objectQuickFieldsEditor_);
    configureSelectionEditableCombo(objectDetailsUiState_.objectQuickProjectionCombo_, QStringLiteral("mapObjectQuickProjectionCombo"));
    objectDetailsUiState_.objectQuickIdentifierEdit_ = new QLineEdit(objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickNameEdit_ = new QLineEdit(objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickIdentifierLabel_ = new QLabel(tr("ID"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickNameLabel_ = new QLabel(tr("Name"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickProjectionLabel_ = new QLabel(tr("Projection"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickTypeLabel_ = new QLabel(tr("Type"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectQuickSubtypeLabel_ = new QLabel(tr("Subtype"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectStylePreviewLabel_ = new QLabel(tr("Style preview"), objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectStylePreview_ = new MapEditorStylePreviewWidget(objectDetailsUiState_.objectQuickFieldsEditor_);
    objectDetailsUiState_.objectStylePreview_->setObjectName(QStringLiteral("mapObjectStylePreview"));
    objectDetailsUiState_.objectStylePreview_->clearStyleSelection();
    connect(objectDetailsUiState_.objectQuickIdentifierEdit_, &QLineEdit::editingFinished, this, &MapEditorTab::applyObjectQuickFieldEdits);
    connect(objectDetailsUiState_.objectQuickNameEdit_, &QLineEdit::editingFinished, this, &MapEditorTab::applyObjectQuickFieldEdits);
    connect(objectDetailsUiState_.objectQuickTypeCombo_, qOverload<int>(&QComboBox::activated), this, [this]() {
        updateObjectQuickSubtypeChoices();
        applyObjectQuickFieldEdits();
    });
    connect(objectDetailsUiState_.objectQuickSubtypeCombo_, qOverload<int>(&QComboBox::activated), this, &MapEditorTab::applyObjectQuickFieldEdits);
    connect(objectDetailsUiState_.objectQuickProjectionCombo_, qOverload<int>(&QComboBox::activated), this, &MapEditorTab::applyScrapProjectionEdit);
    if (objectDetailsUiState_.objectQuickTypeCombo_->lineEdit() != nullptr) {
        connect(objectDetailsUiState_.objectQuickTypeCombo_->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
            updateObjectQuickSubtypeChoices();
            applyObjectQuickFieldEdits();
        });
    }
    if (objectDetailsUiState_.objectQuickSubtypeCombo_->lineEdit() != nullptr) {
        connect(objectDetailsUiState_.objectQuickSubtypeCombo_->lineEdit(), &QLineEdit::editingFinished, this, &MapEditorTab::applyObjectQuickFieldEdits);
    }
    if (objectDetailsUiState_.objectQuickProjectionCombo_->lineEdit() != nullptr) {
        connect(objectDetailsUiState_.objectQuickProjectionCombo_->lineEdit(), &QLineEdit::editingFinished, this, &MapEditorTab::applyScrapProjectionEdit);
    }
    objectQuickForm->addRow(objectDetailsUiState_.objectQuickIdentifierLabel_, objectDetailsUiState_.objectQuickIdentifierEdit_);
    objectQuickForm->addRow(objectDetailsUiState_.objectQuickProjectionLabel_, objectDetailsUiState_.objectQuickProjectionCombo_);
    objectQuickForm->addRow(objectDetailsUiState_.objectQuickTypeLabel_, objectDetailsUiState_.objectQuickTypeCombo_);
    objectQuickForm->addRow(objectDetailsUiState_.objectQuickSubtypeLabel_, objectDetailsUiState_.objectQuickSubtypeCombo_);
    objectQuickForm->addRow(objectDetailsUiState_.objectStylePreviewLabel_);
    objectQuickForm->addRow(objectDetailsUiState_.objectStylePreview_);
    objectQuickForm->addRow(objectDetailsUiState_.objectQuickNameLabel_, objectDetailsUiState_.objectQuickNameEdit_);
    objectSelectionLayout->addWidget(objectDetailsUiState_.objectQuickFieldsEditor_);
    selectionLayout->addWidget(objectDetailsUiState_.objectSelectionSection_);

    QVBoxLayout *geometrySelectionLayout = nullptr;
    objectDetailsUiState_.geometrySelectionSection_ = createSelectionSection(tr("Geometry"), &geometrySelectionLayout);

    objectDetailsUiState_.lineOptionsEditor_ = new QWidget(objectDetailsUiState_.geometrySelectionSection_);
    auto *lineOptionsLayout = new QVBoxLayout(objectDetailsUiState_.lineOptionsEditor_);
    lineOptionsLayout->setContentsMargins(0, 0, 0, 0);
    lineOptionsLayout->setSpacing(6);
    objectDetailsUiState_.lineClosedCheck_ = new QCheckBox(tr("Closed (-close)"), objectDetailsUiState_.lineOptionsEditor_);
    objectDetailsUiState_.lineReversedCheck_ = new QCheckBox(tr("Reversed (-reverse)"), objectDetailsUiState_.lineOptionsEditor_);
    connect(objectDetailsUiState_.lineClosedCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLineClosedToggled);
    connect(objectDetailsUiState_.lineReversedCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLineReversedToggled);
    lineOptionsLayout->addWidget(objectDetailsUiState_.lineClosedCheck_);
    lineOptionsLayout->addWidget(objectDetailsUiState_.lineReversedCheck_);
    geometrySelectionLayout->addWidget(objectDetailsUiState_.lineOptionsEditor_);
    selectionLayout->addWidget(objectDetailsUiState_.geometrySelectionSection_);

    QVBoxLayout *vertexSelectionLayout = nullptr;
    objectDetailsUiState_.vertexSelectionSection_ = createSelectionSection(tr("Point Details"), &vertexSelectionLayout, &objectDetailsUiState_.vertexSelectionTitleLabel_);

    objectDetailsUiState_.objectOrientationEditor_ = new QWidget(objectDetailsUiState_.vertexSelectionSection_);
    auto *orientationLayout = new QVBoxLayout(objectDetailsUiState_.objectOrientationEditor_);
    orientationLayout->setContentsMargins(0, 0, 0, 0);
    orientationLayout->setSpacing(6);
    objectDetailsUiState_.objectOrientationEnabledCheck_ = new QCheckBox(tr("Orientation override (-orientation)"), objectDetailsUiState_.objectOrientationEditor_);
    objectDetailsUiState_.objectOrientationSpin_ = new QDoubleSpinBox(objectDetailsUiState_.objectOrientationEditor_);
    objectDetailsUiState_.objectOrientationSpin_->setDecimals(3);
    objectDetailsUiState_.objectOrientationSpin_->setRange(0.0, 359.999);
    objectDetailsUiState_.objectOrientationSpin_->setSingleStep(1.0);
    objectDetailsUiState_.objectOrientationSpin_->setSuffix(tr(" deg"));
    auto *linePointControlEditor = new QWidget(objectDetailsUiState_.objectOrientationEditor_);
    auto *linePointControlLayout = new QHBoxLayout(linePointControlEditor);
    linePointControlLayout->setContentsMargins(0, 0, 0, 0);
    linePointControlLayout->setSpacing(10);
    objectDetailsUiState_.linePointPreviousControlCheck_ = new QCheckBox(tr("<<"), linePointControlEditor);
    objectDetailsUiState_.linePointSmoothCheck_ = new QCheckBox(tr("Smooth (-smooth)"), linePointControlEditor);
    objectDetailsUiState_.linePointNextControlCheck_ = new QCheckBox(tr(">>"), linePointControlEditor);
    objectDetailsUiState_.linePointLeftSizeEnabledCheck_ = new QCheckBox(tr("Left size (-l-size)"), objectDetailsUiState_.objectOrientationEditor_);
    objectDetailsUiState_.linePointLeftSizeSpin_ = new QDoubleSpinBox(objectDetailsUiState_.objectOrientationEditor_);
    objectDetailsUiState_.linePointLeftSizeSpin_->setDecimals(1);
    objectDetailsUiState_.linePointLeftSizeSpin_->setRange(0.1, 100000.0);
    objectDetailsUiState_.linePointLeftSizeSpin_->setSingleStep(1.0);
    connect(objectDetailsUiState_.objectOrientationEnabledCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleObjectOrientationEnabledToggled);
    connect(objectDetailsUiState_.objectOrientationSpin_, &QDoubleSpinBox::valueChanged, this, &MapEditorTab::handleObjectOrientationValueChanged);
    connect(objectDetailsUiState_.linePointPreviousControlCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointPreviousControlToggled);
    connect(objectDetailsUiState_.linePointSmoothCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointSmoothToggled);
    connect(objectDetailsUiState_.linePointNextControlCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointNextControlToggled);
    connect(objectDetailsUiState_.linePointLeftSizeEnabledCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointLeftSizeEnabledToggled);
    connect(objectDetailsUiState_.linePointLeftSizeSpin_, &QDoubleSpinBox::valueChanged, this, &MapEditorTab::handleLinePointLeftSizeValueChanged);
    linePointControlLayout->addWidget(objectDetailsUiState_.linePointPreviousControlCheck_);
    linePointControlLayout->addWidget(objectDetailsUiState_.linePointSmoothCheck_);
    linePointControlLayout->addWidget(objectDetailsUiState_.linePointNextControlCheck_);
    linePointControlLayout->addStretch(1);
    orientationLayout->addWidget(linePointControlEditor);
    orientationLayout->addWidget(objectDetailsUiState_.objectOrientationEnabledCheck_);
    orientationLayout->addWidget(objectDetailsUiState_.objectOrientationSpin_);
    orientationLayout->addWidget(objectDetailsUiState_.linePointLeftSizeEnabledCheck_);
    orientationLayout->addWidget(objectDetailsUiState_.linePointLeftSizeSpin_);
    vertexSelectionLayout->addWidget(objectDetailsUiState_.objectOrientationEditor_);

    objectDetailsUiState_.vertexActionsEditor_ = new QWidget(objectDetailsUiState_.vertexSelectionSection_);
    auto *vertexActionsLayout = new QGridLayout(objectDetailsUiState_.vertexActionsEditor_);
    vertexActionsLayout->setContentsMargins(0, 0, 0, 0);
    vertexActionsLayout->setSpacing(6);
    objectDetailsUiState_.vertexInsertBeforeButton_ = new QPushButton(tr("Insert Before"), objectDetailsUiState_.vertexActionsEditor_);
    objectDetailsUiState_.vertexInsertAfterButton_ = new QPushButton(tr("Insert After"), objectDetailsUiState_.vertexActionsEditor_);
    objectDetailsUiState_.vertexDeleteButton_ = new QPushButton(tr("Delete Vertex"), objectDetailsUiState_.vertexActionsEditor_);
    objectDetailsUiState_.vertexSplitButton_ = new QPushButton(tr("Split Here"), objectDetailsUiState_.vertexActionsEditor_);
    objectDetailsUiState_.vertexInsertBeforeButton_->setAutoDefault(false);
    objectDetailsUiState_.vertexInsertAfterButton_->setAutoDefault(false);
    objectDetailsUiState_.vertexDeleteButton_->setAutoDefault(false);
    objectDetailsUiState_.vertexSplitButton_->setAutoDefault(false);
    connect(objectDetailsUiState_.vertexInsertBeforeButton_, &QPushButton::clicked, this, &MapEditorTab::insertVertexBeforeFromSelectionPanel);
    connect(objectDetailsUiState_.vertexInsertAfterButton_, &QPushButton::clicked, this, &MapEditorTab::insertVertexAfterFromSelectionPanel);
    connect(objectDetailsUiState_.vertexSplitButton_, &QPushButton::clicked, this, &MapEditorTab::splitLineFromSelectionPanel);
    connect(objectDetailsUiState_.vertexDeleteButton_, &QPushButton::clicked, this, &MapEditorTab::deleteVertexFromSelectionPanel);
    vertexActionsLayout->addWidget(objectDetailsUiState_.vertexInsertBeforeButton_, 0, 0);
    vertexActionsLayout->addWidget(objectDetailsUiState_.vertexInsertAfterButton_, 0, 1);
    vertexActionsLayout->addWidget(objectDetailsUiState_.vertexDeleteButton_, 1, 0);
    vertexActionsLayout->addWidget(objectDetailsUiState_.vertexSplitButton_, 1, 1);
    vertexSelectionLayout->addWidget(objectDetailsUiState_.vertexActionsEditor_);
    selectionLayout->addWidget(objectDetailsUiState_.vertexSelectionSection_);

    objectDetailsUiState_.scrapScaleEditor_ = new QFrame(objectDetailsUiState_.objectSelectionSection_);
    static_cast<QFrame *>(objectDetailsUiState_.scrapScaleEditor_)->setFrameShape(QFrame::StyledPanel);
    auto *scrapScaleLayout = new QVBoxLayout(objectDetailsUiState_.scrapScaleEditor_);
    scrapScaleLayout->setContentsMargins(8, 8, 8, 8);
    scrapScaleLayout->setSpacing(6);
    auto *scrapScaleTitle = new QLabel(tr("Scrap Scale"), objectDetailsUiState_.scrapScaleEditor_);
    QFont scrapScaleTitleFont = scrapScaleTitle->font();
    scrapScaleTitleFont.setBold(true);
    scrapScaleTitle->setFont(scrapScaleTitleFont);
    scrapScaleLayout->addWidget(scrapScaleTitle);

    auto createScrapScaleSpin = [](QWidget *parent, int decimals) {
        auto *spin = new QDoubleSpinBox(parent);
        spin->setDecimals(decimals);
        spin->setRange(-10000000.0, 10000000.0);
        spin->setSingleStep(decimals == 0 ? 1.0 : 0.1);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setMinimumWidth(72);
        return spin;
    };
    auto createScrapScalePointBlock = [](QWidget *parent,
                                         const QString &title,
                                         QDoubleSpinBox *xSpin,
                                         QDoubleSpinBox *ySpin) {
        auto *block = new QWidget(parent);
        auto *blockLayout = new QVBoxLayout(block);
        blockLayout->setContentsMargins(0, 0, 0, 0);
        blockLayout->setSpacing(3);

        auto *titleLabel = new QLabel(title, block);
        titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        blockLayout->addWidget(titleLabel);

        auto *row = new QWidget(block);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        rowLayout->addWidget(new QLabel(QStringLiteral("X"), row));
        rowLayout->addWidget(xSpin, 1);
        rowLayout->addWidget(new QLabel(QStringLiteral("Y"), row));
        rowLayout->addWidget(ySpin, 1);
        blockLayout->addWidget(row);
        return block;
    };

    objectDetailsUiState_.scrapScaleSourceX1Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 0);
    objectDetailsUiState_.scrapScaleSourceY1Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 0);
    objectDetailsUiState_.scrapScaleSourceX2Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 0);
    objectDetailsUiState_.scrapScaleSourceY2Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 0);
    objectDetailsUiState_.scrapScaleRealX1Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 4);
    objectDetailsUiState_.scrapScaleRealY1Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 4);
    objectDetailsUiState_.scrapScaleRealX2Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 4);
    objectDetailsUiState_.scrapScaleRealY2Spin_ = createScrapScaleSpin(objectDetailsUiState_.scrapScaleEditor_, 4);
    scrapScaleLayout->addWidget(createScrapScalePointBlock(objectDetailsUiState_.scrapScaleEditor_, tr("Picture point 1 (px)"), objectDetailsUiState_.scrapScaleSourceX1Spin_, objectDetailsUiState_.scrapScaleSourceY1Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(objectDetailsUiState_.scrapScaleEditor_, tr("Picture point 2 (px)"), objectDetailsUiState_.scrapScaleSourceX2Spin_, objectDetailsUiState_.scrapScaleSourceY2Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(objectDetailsUiState_.scrapScaleEditor_, tr("Real point 1"), objectDetailsUiState_.scrapScaleRealX1Spin_, objectDetailsUiState_.scrapScaleRealY1Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(objectDetailsUiState_.scrapScaleEditor_, tr("Real point 2"), objectDetailsUiState_.scrapScaleRealX2Spin_, objectDetailsUiState_.scrapScaleRealY2Spin_));

    auto *scrapScaleUnitRow = new QWidget(objectDetailsUiState_.scrapScaleEditor_);
    auto *scrapScaleUnitLayout = new QHBoxLayout(scrapScaleUnitRow);
    scrapScaleUnitLayout->setContentsMargins(0, 0, 0, 0);
    scrapScaleUnitLayout->setSpacing(6);
    scrapScaleUnitLayout->addWidget(new QLabel(tr("Unit"), scrapScaleUnitRow));
    objectDetailsUiState_.scrapScaleUnitCombo_ = new QComboBox(objectDetailsUiState_.scrapScaleEditor_);
    objectDetailsUiState_.scrapScaleUnitCombo_->addItems({QStringLiteral("m"), QStringLiteral("cm"), QStringLiteral("mm"), QStringLiteral("ft"), QStringLiteral("in")});
    scrapScaleUnitLayout->addWidget(objectDetailsUiState_.scrapScaleUnitCombo_, 1);
    scrapScaleLayout->addWidget(scrapScaleUnitRow);

    auto *scrapScaleButtons = new QHBoxLayout;
    scrapScaleButtons->setContentsMargins(0, 0, 0, 0);
    objectDetailsUiState_.scrapScaleUseBoundsButton_ = new QPushButton(tr("Use Bounds"), objectDetailsUiState_.scrapScaleEditor_);
    objectDetailsUiState_.scrapScaleUseBoundsButton_->setAutoDefault(false);
    objectDetailsUiState_.scrapScaleUseBoundsButton_->setToolTip(tr("Use current source bounds as the XTherion default scrap scale."));
    objectDetailsUiState_.scrapScaleApplyButton_ = new QPushButton(tr("Apply Scale"), objectDetailsUiState_.scrapScaleEditor_);
    objectDetailsUiState_.scrapScaleApplyButton_->setAutoDefault(false);
    connect(objectDetailsUiState_.scrapScaleUseBoundsButton_, &QPushButton::clicked, this, &MapEditorTab::populateScrapScaleFromSourceBounds);
    connect(objectDetailsUiState_.scrapScaleApplyButton_, &QPushButton::clicked, this, &MapEditorTab::applyScrapScaleEdits);
    scrapScaleButtons->addWidget(objectDetailsUiState_.scrapScaleUseBoundsButton_);
    scrapScaleButtons->addWidget(objectDetailsUiState_.scrapScaleApplyButton_);
    scrapScaleLayout->addLayout(scrapScaleButtons);
    objectSelectionLayout->addWidget(objectDetailsUiState_.scrapScaleEditor_);

    QVBoxLayout *advancedSelectionLayout = nullptr;
    objectDetailsUiState_.advancedSelectionSection_ = createSelectionSection(tr("Object Actions"), &advancedSelectionLayout);
    objectDetailsUiState_.objectConfigureButton_ = new QPushButton(tr("Edit Object Settings..."), objectDetailsUiState_.advancedSelectionSection_);
    objectDetailsUiState_.objectConfigureButton_->setAutoDefault(false);
    connect(objectDetailsUiState_.objectConfigureButton_, &QPushButton::clicked, this, &MapEditorTab::handleConfigureObjectSettingsTriggered);
    advancedSelectionLayout->addWidget(objectDetailsUiState_.objectConfigureButton_);
    objectDetailsUiState_.objectDeleteButton_ = new QPushButton(tr("Delete Object"), objectDetailsUiState_.advancedSelectionSection_);
    objectDetailsUiState_.objectDeleteButton_->setAutoDefault(false);
    connect(objectDetailsUiState_.objectDeleteButton_, &QPushButton::clicked, this, &MapEditorTab::deleteSelectedObjectFromSelection);
    advancedSelectionLayout->addWidget(objectDetailsUiState_.objectDeleteButton_);
    selectionLayout->addWidget(objectDetailsUiState_.advancedSelectionSection_);
    selectionTabLayout->addWidget(selectionPanel);
    selectionTabLayout->addStretch(1);
    mapInspectorTabs_->addTab(selectionTab, tr("Selection"));
    mapInspectorTabs_->addTab(objectsTab, tr("Objects"));

    auto *backgroundTab = new QWidget(mapInspectorTabs_);
    auto *backgroundLayout = new QVBoxLayout(backgroundTab);
    backgroundLayout->setContentsMargins(4, 4, 4, 4);
    backgroundLayout->setSpacing(8);

    auto createBackgroundSection = [backgroundTab](const QString &title, QVBoxLayout **contentLayout) {
        auto *section = new QFrame(backgroundTab);
        section->setFrameShape(QFrame::StyledPanel);
        auto *sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(8, 8, 8, 8);
        sectionLayout->setSpacing(6);
        auto *titleLabel = new QLabel(title, section);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        sectionLayout->addWidget(titleLabel);
        if (contentLayout != nullptr) {
            *contentLayout = sectionLayout;
        }
        return section;
    };

    auto *layersFrame = new QFrame(backgroundTab);
    layersFrame->setFrameShape(QFrame::StyledPanel);
    auto *layersLayout = new QVBoxLayout(layersFrame);
    layersLayout->setContentsMargins(8, 8, 8, 8);
    layersLayout->setSpacing(6);

    auto *layersRow = new QHBoxLayout;
    auto *layersLabel = new QLabel(tr("Layers"), layersFrame);
    sectionFont = layersLabel->font();
    sectionFont.setBold(true);
    layersLabel->setFont(sectionFont);
    layersRow->addWidget(layersLabel);
    layersRow->addStretch(1);
    mapBackgroundAddButton_ = new QToolButton(layersFrame);
    mapBackgroundAddButton_->setText(QStringLiteral("+"));
    mapBackgroundAddButton_->setToolTip(tr("Add background images"));
    layersRow->addWidget(mapBackgroundAddButton_);
    layersLayout->addLayout(layersRow);

    mapBackgroundLayersTree_ = new QTreeView(layersFrame);
    mapBackgroundLayersTree_->setRootIsDecorated(false);
    mapBackgroundLayersTree_->setAnimated(false);
    mapBackgroundLayersTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mapBackgroundLayersTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    mapBackgroundLayersTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mapBackgroundLayersTree_->setAlternatingRowColors(true);
    mapBackgroundLayersTree_->setHeaderHidden(true);
    mapBackgroundLayersTree_->setIconSize(QSize(16, 16));
    mapBackgroundLayersTree_->setMinimumHeight(88);
    mapBackgroundLayersModel_ = new QStandardItemModel(mapBackgroundLayersTree_);
    mapBackgroundLayersTree_->setModel(mapBackgroundLayersModel_);
    configureInspectorBackgroundLayerTreeColumns();
    connect(mapBackgroundLayersTree_, &QTreeView::clicked, this, &MapEditorTab::handleInspectorBackgroundLayerClicked);
    layersLayout->addWidget(mapBackgroundLayersTree_);

    auto *layerActionsRow = new QHBoxLayout;
    mapBackgroundMoveUpButton_ = new QPushButton(tr("Up"), layersFrame);
    mapBackgroundMoveDownButton_ = new QPushButton(tr("Down"), layersFrame);
    mapBackgroundMoveUpButton_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    mapBackgroundMoveDownButton_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layerActionsRow->addWidget(mapBackgroundMoveUpButton_);
    layerActionsRow->addWidget(mapBackgroundMoveDownButton_);
    layersLayout->addLayout(layerActionsRow);
    backgroundLayout->addWidget(layersFrame);

    QVBoxLayout *positionLayout = nullptr;
    auto *positionFrame = createBackgroundSection(tr("Position"), &positionLayout);

    auto *xRow = new QHBoxLayout;
    xRow->addWidget(new QLabel(tr("X"), positionFrame));
    mapBackgroundPosXSpin_ = new QDoubleSpinBox(positionFrame);
    mapBackgroundPosXSpin_->setRange(-50000.0, 50000.0);
    mapBackgroundPosXSpin_->setDecimals(1);
    xRow->addWidget(mapBackgroundPosXSpin_, 1);
    positionLayout->addLayout(xRow);

    auto *yRow = new QHBoxLayout;
    yRow->addWidget(new QLabel(tr("Y"), positionFrame));
    mapBackgroundPosYSpin_ = new QDoubleSpinBox(positionFrame);
    mapBackgroundPosYSpin_->setRange(-50000.0, 50000.0);
    mapBackgroundPosYSpin_->setDecimals(1);
    yRow->addWidget(mapBackgroundPosYSpin_, 1);
    positionLayout->addLayout(yRow);

    backgroundLayout->addWidget(positionFrame);

    QVBoxLayout *adjustmentsLayout = nullptr;
    auto *adjustmentsFrame = createBackgroundSection(tr("Adjustments"), &adjustmentsLayout);

    auto *opacityRow = new QHBoxLayout;
    opacityRow->addWidget(new QLabel(tr("Opacity"), adjustmentsFrame));
    opacityRow->addStretch(1);
    mapBackgroundOpacityResetButton_ = new QPushButton(tr("Reset"), adjustmentsFrame);
    opacityRow->addWidget(mapBackgroundOpacityResetButton_);
    adjustmentsLayout->addLayout(opacityRow);

    mapBackgroundOpacitySlider_ = new QSlider(Qt::Horizontal, adjustmentsFrame);
    mapBackgroundOpacitySlider_->setRange(5, 100);
    adjustmentsLayout->addWidget(mapBackgroundOpacitySlider_);

    auto *gammaRow = new QHBoxLayout;
    gammaRow->addWidget(new QLabel(tr("Gamma"), adjustmentsFrame));
    gammaRow->addStretch(1);
    mapBackgroundGammaResetButton_ = new QPushButton(tr("Reset"), adjustmentsFrame);
    gammaRow->addWidget(mapBackgroundGammaResetButton_);
    adjustmentsLayout->addLayout(gammaRow);

    mapBackgroundGammaSlider_ = new QSlider(Qt::Horizontal, adjustmentsFrame);
    mapBackgroundGammaSlider_->setRange(20, 250);
    adjustmentsLayout->addWidget(mapBackgroundGammaSlider_);
    backgroundLayout->addWidget(adjustmentsFrame);

    backgroundLayout->addStretch(1);
    mapInspectorTabs_->addTab(backgroundTab, tr("Backgrounds"));
    updateMapInspectorLeftEdgeGeometry();

    connect(mapBackgroundAddButton_, &QToolButton::clicked, this, [this]() {
        browseAndAddBackgroundImages();
    });
    connect(mapBackgroundMoveUpButton_, &QPushButton::clicked, this, [this]() {
        moveSelectedBackgroundLayerUp();
    });
    connect(mapBackgroundMoveDownButton_, &QPushButton::clicked, this, [this]() {
        moveSelectedBackgroundLayerDown();
    });
    if (mapBackgroundLayersTree_->selectionModel() != nullptr) {
        connect(mapBackgroundLayersTree_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current, const QModelIndex &) {
            handleInspectorBackgroundLayerSelectionChanged(current);
        });
    }
    connect(mapBackgroundPosXSpin_, &QDoubleSpinBox::valueChanged, this, [this](double x) {
        if (updatingMapInspectorBackgroundUi_ || mapBackgroundPosYSpin_ == nullptr) {
            return;
        }
        setSelectedBackgroundLayerPosition(QPointF(x, mapBackgroundPosYSpin_->value()));
    });
    connect(mapBackgroundPosYSpin_, &QDoubleSpinBox::valueChanged, this, [this](double y) {
        if (updatingMapInspectorBackgroundUi_ || mapBackgroundPosXSpin_ == nullptr) {
            return;
        }
        setSelectedBackgroundLayerPosition(QPointF(mapBackgroundPosXSpin_->value(), y));
    });
    connect(mapBackgroundOpacitySlider_, &QSlider::valueChanged, this, [this](int value) {
        if (updatingMapInspectorBackgroundUi_) {
            return;
        }
        setSelectedBackgroundLayerOpacity(static_cast<qreal>(value) / 100.0);
    });
    connect(mapBackgroundGammaSlider_, &QSlider::valueChanged, this, [this](int value) {
        if (updatingMapInspectorBackgroundUi_) {
            return;
        }
        setSelectedBackgroundLayerGamma(static_cast<qreal>(value) / 100.0);
    });
    connect(mapBackgroundOpacityResetButton_, &QPushButton::clicked, this, [this]() {
        resetSelectedBackgroundLayerOpacity();
    });
    connect(mapBackgroundGammaResetButton_, &QPushButton::clicked, this, [this]() {
        resetSelectedBackgroundLayerGamma();
    });
}

} // namespace TherionStudio
