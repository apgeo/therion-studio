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

    objectDetailsSelectionLabel_ = new QLabel(tr("No map object selected."), selectionPanel);
    objectDetailsSelectionLabel_->setWordWrap(true);
    selectionLayout->addWidget(objectDetailsSelectionLabel_);

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
    objectSelectionSection_ = createSelectionSection(tr("Object"), &objectSelectionLayout, &objectSelectionTitleLabel_);

    objectDetailsMetadataLabel_ = new QLabel(QStringLiteral("-"), objectSelectionSection_);
    objectDetailsMetadataLabel_->setTextFormat(Qt::PlainText);
    objectDetailsMetadataLabel_->setStyleSheet(QStringLiteral("QLabel { color: palette(midlight); }"));
    objectSelectionLayout->addWidget(objectDetailsMetadataLabel_);

    objectAreaReferenceLabel_ = new QLabel(objectSelectionSection_);
    objectAreaReferenceLabel_->setTextFormat(Qt::RichText);
    objectAreaReferenceLabel_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    objectAreaReferenceLabel_->setOpenExternalLinks(false);
    objectAreaReferenceLabel_->setWordWrap(true);
    objectAreaReferenceLabel_->setStyleSheet(QStringLiteral("QLabel { color: palette(midlight); }"));
    connect(objectAreaReferenceLabel_, &QLabel::linkActivated, this, [this](const QString &link) {
        bool ok = false;
        const int areaLineNumber = link.toInt(&ok);
        if (ok && areaLineNumber > 0) {
            selectMapLine(areaLineNumber);
            syncInspectorObjectSelectionToLine(areaLineNumber);
        }
    });
    objectSelectionLayout->addWidget(objectAreaReferenceLabel_);

    objectQuickFieldsEditor_ = new QWidget(objectSelectionSection_);
    auto *objectQuickForm = new QFormLayout(objectQuickFieldsEditor_);
    objectQuickForm->setContentsMargins(0, 0, 0, 0);
    objectQuickForm->setSpacing(6);
    objectQuickTypeCombo_ = new QComboBox(objectQuickFieldsEditor_);
    configureSelectionEditableCombo(objectQuickTypeCombo_, QStringLiteral("mapObjectQuickTypeCombo"));
    objectQuickSubtypeCombo_ = new QComboBox(objectQuickFieldsEditor_);
    configureSelectionEditableCombo(objectQuickSubtypeCombo_, QStringLiteral("mapObjectQuickSubtypeCombo"));
    objectQuickProjectionCombo_ = new QComboBox(objectQuickFieldsEditor_);
    configureSelectionEditableCombo(objectQuickProjectionCombo_, QStringLiteral("mapObjectQuickProjectionCombo"));
    objectQuickIdentifierEdit_ = new QLineEdit(objectQuickFieldsEditor_);
    objectQuickNameEdit_ = new QLineEdit(objectQuickFieldsEditor_);
    objectQuickIdentifierLabel_ = new QLabel(tr("ID"), objectQuickFieldsEditor_);
    objectQuickNameLabel_ = new QLabel(tr("Name"), objectQuickFieldsEditor_);
    objectQuickProjectionLabel_ = new QLabel(tr("Projection"), objectQuickFieldsEditor_);
    objectQuickTypeLabel_ = new QLabel(tr("Type"), objectQuickFieldsEditor_);
    objectQuickSubtypeLabel_ = new QLabel(tr("Subtype"), objectQuickFieldsEditor_);
    objectStylePreviewLabel_ = new QLabel(tr("Style preview"), objectQuickFieldsEditor_);
    objectStylePreview_ = new MapEditorStylePreviewWidget(objectQuickFieldsEditor_);
    objectStylePreview_->setObjectName(QStringLiteral("mapObjectStylePreview"));
    objectStylePreview_->clearStyleSelection();
    connect(objectQuickIdentifierEdit_, &QLineEdit::editingFinished, this, &MapEditorTab::applyObjectQuickFieldEdits);
    connect(objectQuickNameEdit_, &QLineEdit::editingFinished, this, &MapEditorTab::applyObjectQuickFieldEdits);
    connect(objectQuickTypeCombo_, qOverload<int>(&QComboBox::activated), this, [this]() {
        updateObjectQuickSubtypeChoices();
        applyObjectQuickFieldEdits();
    });
    connect(objectQuickSubtypeCombo_, qOverload<int>(&QComboBox::activated), this, &MapEditorTab::applyObjectQuickFieldEdits);
    connect(objectQuickProjectionCombo_, qOverload<int>(&QComboBox::activated), this, &MapEditorTab::applyScrapProjectionEdit);
    if (objectQuickTypeCombo_->lineEdit() != nullptr) {
        connect(objectQuickTypeCombo_->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
            updateObjectQuickSubtypeChoices();
            applyObjectQuickFieldEdits();
        });
    }
    if (objectQuickSubtypeCombo_->lineEdit() != nullptr) {
        connect(objectQuickSubtypeCombo_->lineEdit(), &QLineEdit::editingFinished, this, &MapEditorTab::applyObjectQuickFieldEdits);
    }
    if (objectQuickProjectionCombo_->lineEdit() != nullptr) {
        connect(objectQuickProjectionCombo_->lineEdit(), &QLineEdit::editingFinished, this, &MapEditorTab::applyScrapProjectionEdit);
    }
    objectQuickForm->addRow(objectQuickIdentifierLabel_, objectQuickIdentifierEdit_);
    objectQuickForm->addRow(objectQuickProjectionLabel_, objectQuickProjectionCombo_);
    objectQuickForm->addRow(objectQuickTypeLabel_, objectQuickTypeCombo_);
    objectQuickForm->addRow(objectQuickSubtypeLabel_, objectQuickSubtypeCombo_);
    objectQuickForm->addRow(objectStylePreviewLabel_);
    objectQuickForm->addRow(objectStylePreview_);
    objectQuickForm->addRow(objectQuickNameLabel_, objectQuickNameEdit_);
    objectSelectionLayout->addWidget(objectQuickFieldsEditor_);
    selectionLayout->addWidget(objectSelectionSection_);

    QVBoxLayout *geometrySelectionLayout = nullptr;
    geometrySelectionSection_ = createSelectionSection(tr("Geometry"), &geometrySelectionLayout);

    lineOptionsEditor_ = new QWidget(geometrySelectionSection_);
    auto *lineOptionsLayout = new QVBoxLayout(lineOptionsEditor_);
    lineOptionsLayout->setContentsMargins(0, 0, 0, 0);
    lineOptionsLayout->setSpacing(6);
    lineClosedCheck_ = new QCheckBox(tr("Closed (-close)"), lineOptionsEditor_);
    lineReversedCheck_ = new QCheckBox(tr("Reversed (-reverse)"), lineOptionsEditor_);
    connect(lineClosedCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLineClosedToggled);
    connect(lineReversedCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLineReversedToggled);
    lineOptionsLayout->addWidget(lineClosedCheck_);
    lineOptionsLayout->addWidget(lineReversedCheck_);
    geometrySelectionLayout->addWidget(lineOptionsEditor_);
    selectionLayout->addWidget(geometrySelectionSection_);

    QVBoxLayout *vertexSelectionLayout = nullptr;
    vertexSelectionSection_ = createSelectionSection(tr("Point Details"), &vertexSelectionLayout, &vertexSelectionTitleLabel_);

    objectOrientationEditor_ = new QWidget(vertexSelectionSection_);
    auto *orientationLayout = new QVBoxLayout(objectOrientationEditor_);
    orientationLayout->setContentsMargins(0, 0, 0, 0);
    orientationLayout->setSpacing(6);
    objectOrientationEnabledCheck_ = new QCheckBox(tr("Orientation override (-orientation)"), objectOrientationEditor_);
    objectOrientationSpin_ = new QDoubleSpinBox(objectOrientationEditor_);
    objectOrientationSpin_->setDecimals(3);
    objectOrientationSpin_->setRange(0.0, 359.999);
    objectOrientationSpin_->setSingleStep(1.0);
    objectOrientationSpin_->setSuffix(tr(" deg"));
    auto *linePointControlEditor = new QWidget(objectOrientationEditor_);
    auto *linePointControlLayout = new QHBoxLayout(linePointControlEditor);
    linePointControlLayout->setContentsMargins(0, 0, 0, 0);
    linePointControlLayout->setSpacing(10);
    linePointPreviousControlCheck_ = new QCheckBox(tr("<<"), linePointControlEditor);
    linePointSmoothCheck_ = new QCheckBox(tr("Smooth (-smooth)"), linePointControlEditor);
    linePointNextControlCheck_ = new QCheckBox(tr(">>"), linePointControlEditor);
    linePointLeftSizeEnabledCheck_ = new QCheckBox(tr("Left size (-l-size)"), objectOrientationEditor_);
    linePointLeftSizeSpin_ = new QDoubleSpinBox(objectOrientationEditor_);
    linePointLeftSizeSpin_->setDecimals(1);
    linePointLeftSizeSpin_->setRange(0.1, 100000.0);
    linePointLeftSizeSpin_->setSingleStep(1.0);
    connect(objectOrientationEnabledCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleObjectOrientationEnabledToggled);
    connect(objectOrientationSpin_, &QDoubleSpinBox::valueChanged, this, &MapEditorTab::handleObjectOrientationValueChanged);
    connect(linePointPreviousControlCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointPreviousControlToggled);
    connect(linePointSmoothCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointSmoothToggled);
    connect(linePointNextControlCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointNextControlToggled);
    connect(linePointLeftSizeEnabledCheck_, &QCheckBox::toggled, this, &MapEditorTab::handleLinePointLeftSizeEnabledToggled);
    connect(linePointLeftSizeSpin_, &QDoubleSpinBox::valueChanged, this, &MapEditorTab::handleLinePointLeftSizeValueChanged);
    linePointControlLayout->addWidget(linePointPreviousControlCheck_);
    linePointControlLayout->addWidget(linePointSmoothCheck_);
    linePointControlLayout->addWidget(linePointNextControlCheck_);
    linePointControlLayout->addStretch(1);
    orientationLayout->addWidget(linePointControlEditor);
    orientationLayout->addWidget(objectOrientationEnabledCheck_);
    orientationLayout->addWidget(objectOrientationSpin_);
    orientationLayout->addWidget(linePointLeftSizeEnabledCheck_);
    orientationLayout->addWidget(linePointLeftSizeSpin_);
    vertexSelectionLayout->addWidget(objectOrientationEditor_);

    vertexActionsEditor_ = new QWidget(vertexSelectionSection_);
    auto *vertexActionsLayout = new QGridLayout(vertexActionsEditor_);
    vertexActionsLayout->setContentsMargins(0, 0, 0, 0);
    vertexActionsLayout->setSpacing(6);
    vertexInsertBeforeButton_ = new QPushButton(tr("Insert Before"), vertexActionsEditor_);
    vertexInsertAfterButton_ = new QPushButton(tr("Insert After"), vertexActionsEditor_);
    vertexDeleteButton_ = new QPushButton(tr("Delete Vertex"), vertexActionsEditor_);
    vertexSplitButton_ = new QPushButton(tr("Split Here"), vertexActionsEditor_);
    vertexInsertBeforeButton_->setAutoDefault(false);
    vertexInsertAfterButton_->setAutoDefault(false);
    vertexDeleteButton_->setAutoDefault(false);
    vertexSplitButton_->setAutoDefault(false);
    connect(vertexInsertBeforeButton_, &QPushButton::clicked, this, &MapEditorTab::insertVertexBeforeFromSelectionPanel);
    connect(vertexInsertAfterButton_, &QPushButton::clicked, this, &MapEditorTab::insertVertexAfterFromSelectionPanel);
    connect(vertexSplitButton_, &QPushButton::clicked, this, &MapEditorTab::splitLineFromSelectionPanel);
    connect(vertexDeleteButton_, &QPushButton::clicked, this, &MapEditorTab::deleteVertexFromSelectionPanel);
    vertexActionsLayout->addWidget(vertexInsertBeforeButton_, 0, 0);
    vertexActionsLayout->addWidget(vertexInsertAfterButton_, 0, 1);
    vertexActionsLayout->addWidget(vertexDeleteButton_, 1, 0);
    vertexActionsLayout->addWidget(vertexSplitButton_, 1, 1);
    vertexSelectionLayout->addWidget(vertexActionsEditor_);
    selectionLayout->addWidget(vertexSelectionSection_);

    scrapScaleEditor_ = new QFrame(objectSelectionSection_);
    static_cast<QFrame *>(scrapScaleEditor_)->setFrameShape(QFrame::StyledPanel);
    auto *scrapScaleLayout = new QVBoxLayout(scrapScaleEditor_);
    scrapScaleLayout->setContentsMargins(8, 8, 8, 8);
    scrapScaleLayout->setSpacing(6);
    auto *scrapScaleTitle = new QLabel(tr("Scrap Scale"), scrapScaleEditor_);
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

    scrapScaleSourceX1Spin_ = createScrapScaleSpin(scrapScaleEditor_, 0);
    scrapScaleSourceY1Spin_ = createScrapScaleSpin(scrapScaleEditor_, 0);
    scrapScaleSourceX2Spin_ = createScrapScaleSpin(scrapScaleEditor_, 0);
    scrapScaleSourceY2Spin_ = createScrapScaleSpin(scrapScaleEditor_, 0);
    scrapScaleRealX1Spin_ = createScrapScaleSpin(scrapScaleEditor_, 4);
    scrapScaleRealY1Spin_ = createScrapScaleSpin(scrapScaleEditor_, 4);
    scrapScaleRealX2Spin_ = createScrapScaleSpin(scrapScaleEditor_, 4);
    scrapScaleRealY2Spin_ = createScrapScaleSpin(scrapScaleEditor_, 4);
    scrapScaleLayout->addWidget(createScrapScalePointBlock(scrapScaleEditor_, tr("Picture point 1 (px)"), scrapScaleSourceX1Spin_, scrapScaleSourceY1Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(scrapScaleEditor_, tr("Picture point 2 (px)"), scrapScaleSourceX2Spin_, scrapScaleSourceY2Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(scrapScaleEditor_, tr("Real point 1"), scrapScaleRealX1Spin_, scrapScaleRealY1Spin_));
    scrapScaleLayout->addWidget(createScrapScalePointBlock(scrapScaleEditor_, tr("Real point 2"), scrapScaleRealX2Spin_, scrapScaleRealY2Spin_));

    auto *scrapScaleUnitRow = new QWidget(scrapScaleEditor_);
    auto *scrapScaleUnitLayout = new QHBoxLayout(scrapScaleUnitRow);
    scrapScaleUnitLayout->setContentsMargins(0, 0, 0, 0);
    scrapScaleUnitLayout->setSpacing(6);
    scrapScaleUnitLayout->addWidget(new QLabel(tr("Unit"), scrapScaleUnitRow));
    scrapScaleUnitCombo_ = new QComboBox(scrapScaleEditor_);
    scrapScaleUnitCombo_->addItems({QStringLiteral("m"), QStringLiteral("cm"), QStringLiteral("mm"), QStringLiteral("ft"), QStringLiteral("in")});
    scrapScaleUnitLayout->addWidget(scrapScaleUnitCombo_, 1);
    scrapScaleLayout->addWidget(scrapScaleUnitRow);

    auto *scrapScaleButtons = new QHBoxLayout;
    scrapScaleButtons->setContentsMargins(0, 0, 0, 0);
    scrapScaleUseBoundsButton_ = new QPushButton(tr("Use Bounds"), scrapScaleEditor_);
    scrapScaleUseBoundsButton_->setAutoDefault(false);
    scrapScaleUseBoundsButton_->setToolTip(tr("Use current source bounds as the XTherion default scrap scale."));
    scrapScaleApplyButton_ = new QPushButton(tr("Apply Scale"), scrapScaleEditor_);
    scrapScaleApplyButton_->setAutoDefault(false);
    connect(scrapScaleUseBoundsButton_, &QPushButton::clicked, this, &MapEditorTab::populateScrapScaleFromSourceBounds);
    connect(scrapScaleApplyButton_, &QPushButton::clicked, this, &MapEditorTab::applyScrapScaleEdits);
    scrapScaleButtons->addWidget(scrapScaleUseBoundsButton_);
    scrapScaleButtons->addWidget(scrapScaleApplyButton_);
    scrapScaleLayout->addLayout(scrapScaleButtons);
    objectSelectionLayout->addWidget(scrapScaleEditor_);

    QVBoxLayout *advancedSelectionLayout = nullptr;
    advancedSelectionSection_ = createSelectionSection(tr("Object Actions"), &advancedSelectionLayout);
    objectConfigureButton_ = new QPushButton(tr("Edit Object Settings..."), advancedSelectionSection_);
    objectConfigureButton_->setAutoDefault(false);
    connect(objectConfigureButton_, &QPushButton::clicked, this, &MapEditorTab::handleConfigureObjectSettingsTriggered);
    advancedSelectionLayout->addWidget(objectConfigureButton_);
    objectDeleteButton_ = new QPushButton(tr("Delete Object"), advancedSelectionSection_);
    objectDeleteButton_->setAutoDefault(false);
    connect(objectDeleteButton_, &QPushButton::clicked, this, &MapEditorTab::deleteSelectedObjectFromSelection);
    advancedSelectionLayout->addWidget(objectDeleteButton_);
    selectionLayout->addWidget(advancedSelectionSection_);
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
