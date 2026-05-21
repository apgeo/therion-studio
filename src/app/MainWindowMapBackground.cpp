#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"

#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QSizePolicy>

#include "text_editor/map_editor/MapEditorTab.h"

TherionStudio::MapEditorTab *MainWindow::currentMapEditorTab() const
{
    return qobject_cast<TherionStudio::MapEditorTab *>(currentDocumentWidget());
}

void MainWindow::buildMapBackgroundPanel(QWidget *parent, QVBoxLayout *parentLayout)
{
    if (parent == nullptr || parentLayout == nullptr) {
        return;
    }

    mapBackgroundPanel_ = new QFrame(parent);
    mapBackgroundPanel_->setFrameShape(QFrame::StyledPanel);
    mapBackgroundPanel_->setMinimumWidth(0);
    mapBackgroundPanel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    auto *panelLayout = new QVBoxLayout(mapBackgroundPanel_);
    panelLayout->setContentsMargins(10, 10, 10, 10);
    panelLayout->setSpacing(8);

    auto *titleLabel = new QLabel(tr("Background Images"), mapBackgroundPanel_);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    panelLayout->addWidget(titleLabel);

    auto *layersRow = new QHBoxLayout;
    auto *layersLabel = new QLabel(tr("Layers"), mapBackgroundPanel_);
    QFont sectionFont = layersLabel->font();
    sectionFont.setBold(true);
    layersLabel->setFont(sectionFont);
    layersRow->addWidget(layersLabel);
    layersRow->addStretch(1);
    mapBackgroundAddButton_ = new QToolButton(mapBackgroundPanel_);
    mapBackgroundAddButton_->setText(QStringLiteral("+"));
    mapBackgroundAddButton_->setToolTip(tr("Add background images"));
    layersRow->addWidget(mapBackgroundAddButton_);
    panelLayout->addLayout(layersRow);

    mapBackgroundLayersList_ = new QListWidget(mapBackgroundPanel_);
    mapBackgroundLayersList_->setSelectionMode(QAbstractItemView::SingleSelection);
    mapBackgroundLayersList_->setMinimumHeight(88);
    mapBackgroundLayersList_->setMinimumWidth(0);
    panelLayout->addWidget(mapBackgroundLayersList_);

    auto *layerActionsRow = new QHBoxLayout;
    mapBackgroundRemoveButton_ = new QPushButton(tr("Remove"), mapBackgroundPanel_);
    mapBackgroundVisibilityButton_ = new QPushButton(tr("Hide"), mapBackgroundPanel_);
    mapBackgroundMoveUpButton_ = new QPushButton(tr("Up"), mapBackgroundPanel_);
    mapBackgroundMoveDownButton_ = new QPushButton(tr("Down"), mapBackgroundPanel_);
    mapBackgroundRemoveButton_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    mapBackgroundVisibilityButton_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    mapBackgroundMoveUpButton_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    mapBackgroundMoveDownButton_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layerActionsRow->addWidget(mapBackgroundRemoveButton_);
    layerActionsRow->addWidget(mapBackgroundVisibilityButton_);
    layerActionsRow->addWidget(mapBackgroundMoveUpButton_);
    layerActionsRow->addWidget(mapBackgroundMoveDownButton_);
    panelLayout->addLayout(layerActionsRow);

    auto *positionLabel = new QLabel(tr("Position"), mapBackgroundPanel_);
    positionLabel->setFont(sectionFont);
    panelLayout->addWidget(positionLabel);

    auto *positionFrame = new QFrame(mapBackgroundPanel_);
    positionFrame->setFrameShape(QFrame::StyledPanel);
    auto *positionLayout = new QVBoxLayout(positionFrame);
    positionLayout->setContentsMargins(8, 8, 8, 8);
    positionLayout->setSpacing(6);

    auto *xRow = new QHBoxLayout;
    xRow->addWidget(new QLabel(tr("X"), positionFrame));
    mapBackgroundPosXSpin_ = new QDoubleSpinBox(positionFrame);
    mapBackgroundPosXSpin_->setRange(-50000.0, 50000.0);
    mapBackgroundPosXSpin_->setDecimals(1);
    mapBackgroundPosXSpin_->setMinimumWidth(0);
    xRow->addWidget(mapBackgroundPosXSpin_, 1);
    positionLayout->addLayout(xRow);

    auto *yRow = new QHBoxLayout;
    yRow->addWidget(new QLabel(tr("Y"), positionFrame));
    mapBackgroundPosYSpin_ = new QDoubleSpinBox(positionFrame);
    mapBackgroundPosYSpin_->setRange(-50000.0, 50000.0);
    mapBackgroundPosYSpin_->setDecimals(1);
    mapBackgroundPosYSpin_->setMinimumWidth(0);
    yRow->addWidget(mapBackgroundPosYSpin_, 1);
    positionLayout->addLayout(yRow);

    auto *nudgeRow = new QHBoxLayout;
    mapBackgroundNudgeLeftButton_ = new QPushButton(QStringLiteral("←"), positionFrame);
    mapBackgroundNudgeRightButton_ = new QPushButton(QStringLiteral("→"), positionFrame);
    mapBackgroundNudgeUpButton_ = new QPushButton(QStringLiteral("↑"), positionFrame);
    mapBackgroundNudgeDownButton_ = new QPushButton(QStringLiteral("↓"), positionFrame);
    nudgeRow->addWidget(mapBackgroundNudgeLeftButton_);
    nudgeRow->addWidget(mapBackgroundNudgeRightButton_);
    nudgeRow->addWidget(mapBackgroundNudgeUpButton_);
    nudgeRow->addWidget(mapBackgroundNudgeDownButton_);
    positionLayout->addLayout(nudgeRow);

    panelLayout->addWidget(positionFrame);

    auto *adjustmentsLabel = new QLabel(tr("Adjustments"), mapBackgroundPanel_);
    adjustmentsLabel->setFont(sectionFont);
    panelLayout->addWidget(adjustmentsLabel);

    auto *opacityRow = new QHBoxLayout;
    opacityRow->addWidget(new QLabel(tr("Opacity"), mapBackgroundPanel_));
    opacityRow->addStretch(1);
    mapBackgroundOpacityResetButton_ = new QPushButton(tr("Reset"), mapBackgroundPanel_);
    opacityRow->addWidget(mapBackgroundOpacityResetButton_);
    panelLayout->addLayout(opacityRow);

    mapBackgroundOpacitySlider_ = new QSlider(Qt::Horizontal, mapBackgroundPanel_);
    mapBackgroundOpacitySlider_->setRange(5, 100);
    panelLayout->addWidget(mapBackgroundOpacitySlider_);

    auto *gammaRow = new QHBoxLayout;
    gammaRow->addWidget(new QLabel(tr("Gamma"), mapBackgroundPanel_));
    gammaRow->addStretch(1);
    mapBackgroundGammaResetButton_ = new QPushButton(tr("Reset"), mapBackgroundPanel_);
    gammaRow->addWidget(mapBackgroundGammaResetButton_);
    panelLayout->addLayout(gammaRow);

    mapBackgroundGammaSlider_ = new QSlider(Qt::Horizontal, mapBackgroundPanel_);
    mapBackgroundGammaSlider_->setRange(20, 250);
    panelLayout->addWidget(mapBackgroundGammaSlider_);

    parentLayout->addWidget(mapBackgroundPanel_);

    connect(mapBackgroundAddButton_, &QToolButton::clicked, this, [this]() {
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->browseAndAddBackgroundImages();
        }
    });
    connect(mapBackgroundRemoveButton_, &QPushButton::clicked, this, [this]() {
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->removeSelectedBackgroundLayer();
        }
    });
    connect(mapBackgroundMoveUpButton_, &QPushButton::clicked, this, [this]() {
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->moveSelectedBackgroundLayerUp();
        }
    });
    connect(mapBackgroundMoveDownButton_, &QPushButton::clicked, this, [this]() {
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->moveSelectedBackgroundLayerDown();
        }
    });
    connect(mapBackgroundVisibilityButton_, &QPushButton::clicked, this, [this]() {
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->toggleSelectedBackgroundLayerVisibility();
        }
    });
    connect(mapBackgroundLayersList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (updatingMapBackgroundPanel_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->setSelectedBackgroundLayerIndex(row);
        }
    });
    connect(mapBackgroundPosXSpin_, &QDoubleSpinBox::valueChanged, this, [this](double x) {
        if (updatingMapBackgroundPanel_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->setSelectedBackgroundLayerPosition(QPointF(x, mapBackgroundPosYSpin_->value()));
        }
    });
    connect(mapBackgroundPosYSpin_, &QDoubleSpinBox::valueChanged, this, [this](double y) {
        if (updatingMapBackgroundPanel_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->setSelectedBackgroundLayerPosition(QPointF(mapBackgroundPosXSpin_->value(), y));
        }
    });
    connect(mapBackgroundNudgeLeftButton_, &QPushButton::clicked, this, [this]() {
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->nudgeSelectedBackgroundLayer(QPointF(-10.0, 0.0));
        }
    });
    connect(mapBackgroundNudgeRightButton_, &QPushButton::clicked, this, [this]() {
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->nudgeSelectedBackgroundLayer(QPointF(10.0, 0.0));
        }
    });
    connect(mapBackgroundNudgeUpButton_, &QPushButton::clicked, this, [this]() {
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->nudgeSelectedBackgroundLayer(QPointF(0.0, -10.0));
        }
    });
    connect(mapBackgroundNudgeDownButton_, &QPushButton::clicked, this, [this]() {
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->nudgeSelectedBackgroundLayer(QPointF(0.0, 10.0));
        }
    });
    connect(mapBackgroundOpacitySlider_, &QSlider::valueChanged, this, [this](int value) {
        if (updatingMapBackgroundPanel_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->setSelectedBackgroundLayerOpacity(static_cast<qreal>(value) / 100.0);
        }
    });
    connect(mapBackgroundGammaSlider_, &QSlider::valueChanged, this, [this](int value) {
        if (updatingMapBackgroundPanel_) {
            return;
        }
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->setSelectedBackgroundLayerGamma(static_cast<qreal>(value) / 100.0);
        }
    });
    connect(mapBackgroundOpacityResetButton_, &QPushButton::clicked, this, [this]() {
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->resetSelectedBackgroundLayerOpacity();
        }
    });
    connect(mapBackgroundGammaResetButton_, &QPushButton::clicked, this, [this]() {
        if (auto *mapTab = currentMapEditorTab()) {
            mapTab->resetSelectedBackgroundLayerGamma();
        }
    });
}

void MainWindow::refreshMapBackgroundPanel()
{
    if (mapBackgroundPanel_ == nullptr || mapBackgroundLayersList_ == nullptr) {
        return;
    }

    updatingMapBackgroundPanel_ = true;

    mapBackgroundLayersList_->clear();
    auto *mapTab = currentMapEditorTab();
    const bool hasMapTab = mapTab != nullptr;
    mapBackgroundPanel_->setEnabled(hasMapTab);

    if (!hasMapTab) {
        mapBackgroundLayersList_->addItem(tr("Open a TH2 map tab to manage backgrounds"));
        mapBackgroundLayersList_->setCurrentRow(-1);
    } else {
        const int layerCount = mapTab->backgroundLayerCount();
        for (int index = 0; index < layerCount; ++index) {
            const QString visibility = mapTab->isBackgroundLayerVisible(index) ? tr("shown") : tr("hidden");
            mapBackgroundLayersList_->addItem(tr("%1 (%2)").arg(mapTab->backgroundLayerLabel(index), visibility));
        }
        mapBackgroundLayersList_->setCurrentRow(mapTab->selectedBackgroundLayerIndex());
    }

    const int selectedIndex = hasMapTab ? mapTab->selectedBackgroundLayerIndex() : -1;
    const bool hasLayer = hasMapTab && selectedIndex >= 0 && selectedIndex < mapTab->backgroundLayerCount();

    mapBackgroundRemoveButton_->setEnabled(hasLayer);
    mapBackgroundMoveUpButton_->setEnabled(hasLayer && selectedIndex > 0);
    mapBackgroundMoveDownButton_->setEnabled(hasLayer && selectedIndex >= 0 && selectedIndex < mapTab->backgroundLayerCount() - 1);
    mapBackgroundVisibilityButton_->setEnabled(hasLayer);
    mapBackgroundPosXSpin_->setEnabled(hasLayer);
    mapBackgroundPosYSpin_->setEnabled(hasLayer);
    mapBackgroundNudgeLeftButton_->setEnabled(hasLayer);
    mapBackgroundNudgeRightButton_->setEnabled(hasLayer);
    mapBackgroundNudgeUpButton_->setEnabled(hasLayer);
    mapBackgroundNudgeDownButton_->setEnabled(hasLayer);
    mapBackgroundOpacitySlider_->setEnabled(hasLayer);
    mapBackgroundGammaSlider_->setEnabled(hasLayer);
    mapBackgroundOpacityResetButton_->setEnabled(hasLayer);
    mapBackgroundGammaResetButton_->setEnabled(hasLayer);

    if (!hasLayer) {
        mapBackgroundVisibilityButton_->setText(tr("Hide"));
        mapBackgroundPosXSpin_->setValue(0.0);
        mapBackgroundPosYSpin_->setValue(0.0);
        mapBackgroundOpacitySlider_->setValue(58);
        mapBackgroundGammaSlider_->setValue(100);
    } else {
        mapBackgroundVisibilityButton_->setText(mapTab->isBackgroundLayerVisible(selectedIndex) ? tr("Hide") : tr("Show"));
        const QPointF position = mapTab->backgroundLayerPosition(selectedIndex);
        mapBackgroundPosXSpin_->setValue(position.x());
        mapBackgroundPosYSpin_->setValue(position.y());
        mapBackgroundOpacitySlider_->setValue(qBound(5, qRound(mapTab->backgroundLayerOpacity(selectedIndex) * 100.0), 100));
        mapBackgroundGammaSlider_->setValue(qBound(20, qRound(mapTab->backgroundLayerGamma(selectedIndex) * 100.0), 250));
    }

    updatingMapBackgroundPanel_ = false;
}
