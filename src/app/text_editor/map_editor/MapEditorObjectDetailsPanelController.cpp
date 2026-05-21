#include "MapEditorObjectDetailsPanelController.h"

#include "../TextEditorTab.h"
#include "MapEditorInspectorData.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../../../core/TherionDocumentParser.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QWidget>

namespace TherionStudio
{
namespace
{
int lineVertexIndexForSourceVertex(const MapGeometryFeature &lineFeature, int sourceVertexIndex)
{
    if (sourceVertexIndex < 0) {
        return -1;
    }

    for (int index = 0; index < lineFeature.lineVertices.size(); ++index) {
        if (lineFeature.lineVertices.at(index).anchorSourceVertexIndex == sourceVertexIndex) {
            return index;
        }
    }

    return -1;
}
}

MapEditorObjectDetailsPanelController::MapEditorObjectDetailsPanelController(MapEditorTab *owner)
    : owner_(owner)
{
}

void MapEditorObjectDetailsPanelController::refreshObjectDetailsPanel()
{
    if (owner_->objectDetailsSelectionLabel_ == nullptr
        || owner_->objectSelectionSection_ == nullptr
        || owner_->objectSelectionTitleLabel_ == nullptr
        || owner_->vertexSelectionSection_ == nullptr
        || owner_->geometrySelectionSection_ == nullptr
        || owner_->advancedSelectionSection_ == nullptr
        || owner_->objectDeleteButton_ == nullptr
        || owner_->objectQuickFieldsEditor_ == nullptr
        || owner_->objectQuickIdentifierLabel_ == nullptr
        || owner_->objectQuickNameLabel_ == nullptr
        || owner_->objectQuickProjectionLabel_ == nullptr
        || owner_->objectQuickTypeLabel_ == nullptr
        || owner_->objectQuickSubtypeLabel_ == nullptr
        || owner_->objectQuickTypeCombo_ == nullptr
        || owner_->objectQuickSubtypeCombo_ == nullptr
        || owner_->objectQuickProjectionCombo_ == nullptr
        || owner_->objectQuickIdentifierEdit_ == nullptr
        || owner_->objectQuickNameEdit_ == nullptr
        || owner_->vertexActionsEditor_ == nullptr
        || owner_->vertexInsertButton_ == nullptr
        || owner_->vertexDeleteButton_ == nullptr
        || owner_->vertexToggleSmoothButton_ == nullptr
        || owner_->objectDetailsMetadataLabel_ == nullptr
        || owner_->objectOrientationEditor_ == nullptr
        || owner_->objectOrientationEnabledCheck_ == nullptr
        || owner_->objectOrientationSpin_ == nullptr
        || owner_->linePointLeftSizeEnabledCheck_ == nullptr
        || owner_->linePointLeftSizeSpin_ == nullptr
        || owner_->objectOrientationApplyButton_ == nullptr
        || owner_->lineOptionsEditor_ == nullptr
        || owner_->lineClosedCheck_ == nullptr
        || owner_->lineReversedCheck_ == nullptr
        || owner_->scrapScaleEditor_ == nullptr
        || owner_->scrapScaleSourceX1Spin_ == nullptr
        || owner_->scrapScaleSourceY1Spin_ == nullptr
        || owner_->scrapScaleSourceX2Spin_ == nullptr
        || owner_->scrapScaleSourceY2Spin_ == nullptr
        || owner_->scrapScaleRealX1Spin_ == nullptr
        || owner_->scrapScaleRealY1Spin_ == nullptr
        || owner_->scrapScaleRealX2Spin_ == nullptr
        || owner_->scrapScaleRealY2Spin_ == nullptr
        || owner_->scrapScaleUnitCombo_ == nullptr
        || owner_->objectConfigureButton_ == nullptr) {
        return;
    }

    const QScopedValueRollback<bool> uiGuard(owner_->updatingObjectDetailsUi_, true);

    int effectiveLineNumber = owner_->selectedObjectLineNumber_;
    QString effectiveKind = owner_->selectedObjectKind_.trimmed().toLower();

    if (!(effectiveLineNumber > 0 && isConfigurableMapObjectKind(effectiveKind)) && owner_->textEditor_ != nullptr) {
        const int cursorLineNumber = owner_->textEditor_->currentLineNumber();
        if (cursorLineNumber > 0) {
            QStringList lines = owner_->textEditor_->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
            for (QString &line : lines) {
                if (line.endsWith(QLatin1Char('\r'))) {
                    line.chop(1);
                }
            }
            if (cursorLineNumber <= lines.size()) {
                const TherionParsedLine parsedLine =
                    TherionDocumentParser::parseLine(lines.at(cursorLineNumber - 1), cursorLineNumber);
                const QString cursorKind = objectKindForDirective(parsedLine.directive);
                if (!cursorKind.isEmpty()) {
                    effectiveLineNumber = cursorLineNumber;
                    effectiveKind = cursorKind;
                }
            }
        }
    }

    if (effectiveLineNumber <= 0 || effectiveKind.isEmpty()) {
        owner_->objectDetailsSelectionLabel_->setText(owner_->tr("No map object selected."));
        owner_->objectDetailsSelectionLabel_->setVisible(true);
        owner_->objectSelectionTitleLabel_->setText(owner_->tr("Object"));
        owner_->objectSelectionSection_->setVisible(false);
        owner_->vertexSelectionSection_->setVisible(false);
        owner_->geometrySelectionSection_->setVisible(false);
        owner_->advancedSelectionSection_->setVisible(false);
        owner_->objectDeleteButton_->setEnabled(false);
        owner_->objectQuickFieldsEditor_->setVisible(false);
        owner_->objectQuickCommandKind_.clear();
        owner_->vertexActionsEditor_->setVisible(false);
        owner_->objectDetailsMetadataLabel_->setText(QStringLiteral("-"));
        owner_->objectOrientationEditor_->setVisible(false);
        owner_->linePointLeftSizeEnabledCheck_->setChecked(false);
        owner_->linePointLeftSizeSpin_->setEnabled(false);
        owner_->linePointLeftSizeSpin_->setValue(40.0);
        owner_->lineOptionsEditor_->setVisible(false);
        owner_->scrapScaleEditor_->setVisible(false);
        owner_->objectConfigureButton_->setVisible(false);
        owner_->objectConfigureButton_->setEnabled(false);
        owner_->lineClosedCheck_->setChecked(false);
        owner_->lineReversedCheck_->setChecked(false);
        return;
    }

    owner_->objectDetailsSelectionLabel_->setVisible(false);
    owner_->objectSelectionSection_->setVisible(true);
    owner_->advancedSelectionSection_->setVisible(true);
    QString objectSectionTitle = effectiveKind;
    if (effectiveKind == QStringLiteral("line")) {
        objectSectionTitle = owner_->tr("Line");
    } else if (effectiveKind == QStringLiteral("area")) {
        objectSectionTitle = owner_->tr("Area");
    } else if (effectiveKind == QStringLiteral("point")) {
        objectSectionTitle = owner_->tr("Point");
    } else if (effectiveKind == QStringLiteral("scrap")) {
        objectSectionTitle = owner_->tr("Scrap");
    }
    owner_->objectSelectionTitleLabel_->setText(objectSectionTitle);
    owner_->objectDetailsMetadataLabel_->setText(owner_->tr("Source line %1").arg(effectiveLineNumber));
    owner_->objectDeleteButton_->setEnabled(effectiveLineNumber > 0);

    owner_->objectQuickFieldsEditor_->setVisible(false);
    owner_->objectQuickCommandKind_.clear();
    if (owner_->textEditor_ != nullptr && effectiveLineNumber > 0) {
        QStringList lines = owner_->textEditor_->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        for (QString &line : lines) {
            if (line.endsWith(QLatin1Char('\r'))) {
                line.chop(1);
            }
        }
        if (effectiveLineNumber <= lines.size()) {
            const TherionParsedLine parsedLine =
                TherionDocumentParser::parseLine(lines.at(effectiveLineNumber - 1), effectiveLineNumber);
            if (const std::optional<InspectorObjectQuickFields> fields = inspectorObjectQuickFieldsFromParsedLine(parsedLine)) {
                const bool typeFieldsVisible = fields->commandKind != QStringLiteral("scrap");
                const bool projectionFieldVisible = fields->commandKind == QStringLiteral("scrap");
                owner_->objectQuickFieldsEditor_->setVisible(true);
                owner_->objectQuickNameLabel_->setVisible(fields->nameVisible);
                owner_->objectQuickProjectionLabel_->setVisible(projectionFieldVisible);
                owner_->objectQuickTypeLabel_->setVisible(typeFieldsVisible);
                owner_->objectQuickSubtypeLabel_->setVisible(typeFieldsVisible);
                owner_->objectQuickNameEdit_->setVisible(fields->nameVisible);
                owner_->objectQuickProjectionCombo_->setVisible(projectionFieldVisible);
                owner_->objectQuickTypeCombo_->setVisible(typeFieldsVisible);
                owner_->objectQuickSubtypeCombo_->setVisible(typeFieldsVisible);
                owner_->objectQuickProjectionCombo_->setEnabled(projectionFieldVisible);
                owner_->objectQuickTypeCombo_->setEnabled(typeFieldsVisible && fields->typeEditable);
                owner_->objectQuickSubtypeCombo_->setEnabled(typeFieldsVisible && fields->typeEditable);
                setEditableComboValues(owner_->objectQuickTypeCombo_, inspectorTypeValuesForCommand(fields->commandKind), fields->type);
                setEditableComboValues(owner_->objectQuickProjectionCombo_, inspectorProjectionValues(), fields->projection);
                owner_->objectQuickIdentifierEdit_->setText(fields->identifier);
                owner_->objectQuickIdentifierLabel_->setText(owner_->tr("ID"));
                owner_->objectQuickNameEdit_->setText(fields->name);
                owner_->objectQuickCommandKind_ = fields->commandKind;
                owner_->updateObjectQuickSubtypeChoices();
                setEditableComboValues(owner_->objectQuickSubtypeCombo_,
                                       inspectorSubtypeValuesForCommandType(fields->commandKind, fields->type),
                                       fields->subtype);
            }
        }
    }
    owner_->objectConfigureButton_->setVisible(true);
    owner_->objectConfigureButton_->setEnabled(isConfigurableMapObjectKind(effectiveKind));

    const bool lineVertexActionsAvailable = owner_->selectedObjectKind_ == QStringLiteral("line")
        && owner_->selectedObjectVertexIndex_ >= 0
        && owner_->textEditor_ != nullptr;
    owner_->vertexActionsEditor_->setVisible(lineVertexActionsAvailable);
    owner_->vertexInsertButton_->setEnabled(lineVertexActionsAvailable);
    owner_->vertexDeleteButton_->setEnabled(lineVertexActionsAvailable);
    owner_->vertexToggleSmoothButton_->setEnabled(lineVertexActionsAvailable);

    const bool lineOptionsVisible = owner_->selectedObjectLineNumber_ > 0
        && owner_->selectedObjectKind_ == QStringLiteral("line")
        && owner_->textEditor_ != nullptr;
    owner_->lineOptionsEditor_->setVisible(lineOptionsVisible);
    if (lineOptionsVisible) {
        if (const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(owner_->textEditor_->text(), owner_->selectedObjectLineNumber_);
            lineFeature.has_value() && lineFeature->kind == MapGeometryFeature::Kind::Line) {
            owner_->lineClosedCheck_->setChecked(lineFeature->closed);
            owner_->lineReversedCheck_->setChecked(lineFeature->reversed);
        } else {
            owner_->lineClosedCheck_->setChecked(false);
            owner_->lineReversedCheck_->setChecked(false);
        }
    } else {
        owner_->lineClosedCheck_->setChecked(false);
        owner_->lineReversedCheck_->setChecked(false);
    }

    owner_->geometrySelectionSection_->setVisible(lineOptionsVisible);

    const bool scrapScaleVisible = effectiveLineNumber > 0
        && effectiveKind == QStringLiteral("scrap")
        && owner_->textEditor_ != nullptr;
    owner_->scrapScaleEditor_->setVisible(scrapScaleVisible);
    if (scrapScaleVisible) {
        auto configureScaleSpin = [](QDoubleSpinBox *spin, int decimals) {
            if (spin == nullptr) {
                return;
            }
            spin->setDecimals(decimals);
            spin->setSingleStep(decimals == 0 ? 1.0 : 0.1);
        };
        configureScaleSpin(owner_->scrapScaleSourceX1Spin_, 0);
        configureScaleSpin(owner_->scrapScaleSourceY1Spin_, 0);
        configureScaleSpin(owner_->scrapScaleSourceX2Spin_, 0);
        configureScaleSpin(owner_->scrapScaleSourceY2Spin_, 0);
        configureScaleSpin(owner_->scrapScaleRealX1Spin_, 4);
        configureScaleSpin(owner_->scrapScaleRealY1Spin_, 4);
        configureScaleSpin(owner_->scrapScaleRealX2Spin_, 4);
        configureScaleSpin(owner_->scrapScaleRealY2Spin_, 4);

        InspectorScrapScale scale = defaultInspectorScrapScale(owner_->mapSourceBoundsForCurrentDocument());
        QStringList lines = owner_->textEditor_->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        for (QString &line : lines) {
            if (line.endsWith(QLatin1Char('\r'))) {
                line.chop(1);
            }
        }
        if (effectiveLineNumber <= lines.size()) {
            const TherionParsedLine parsedLine =
                TherionDocumentParser::parseLine(lines.at(effectiveLineNumber - 1), effectiveLineNumber);
            if (const std::optional<InspectorScrapScale> parsedScale = inspectorScrapScaleFromTokens(parsedLine.tokens)) {
                scale = parsedScale.value();
            }
        }

        owner_->scrapScaleSourceX1Spin_->setValue(scale.sourcePoint1.x());
        owner_->scrapScaleSourceY1Spin_->setValue(scale.sourcePoint1.y());
        owner_->scrapScaleSourceX2Spin_->setValue(scale.sourcePoint2.x());
        owner_->scrapScaleSourceY2Spin_->setValue(scale.sourcePoint2.y());
        owner_->scrapScaleRealX1Spin_->setValue(scale.realPoint1.x());
        owner_->scrapScaleRealY1Spin_->setValue(scale.realPoint1.y());
        owner_->scrapScaleRealX2Spin_->setValue(scale.realPoint2.x());
        owner_->scrapScaleRealY2Spin_->setValue(scale.realPoint2.y());
        const int unitIndex = owner_->scrapScaleUnitCombo_->findText(scale.unitToken);
        owner_->scrapScaleUnitCombo_->setCurrentIndex(unitIndex >= 0 ? unitIndex : 0);
    }

    bool orientationApplicable = false;
    bool linePointLeftSizeApplicable = false;
    std::optional<qreal> orientationDegrees;
    std::optional<qreal> linePointLeftSize;
    if (owner_->textEditor_ != nullptr && owner_->selectedObjectLineNumber_ > 0) {
        if (owner_->selectedObjectKind_ == QStringLiteral("point")) {
            QStringList lines = owner_->textEditor_->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
            for (QString &line : lines) {
                if (line.endsWith(QLatin1Char('\r'))) {
                    line.chop(1);
                }
            }
            if (owner_->selectedObjectLineNumber_ <= lines.size()) {
                const TherionParsedLine parsedLine =
                    TherionDocumentParser::parseLine(lines.at(owner_->selectedObjectLineNumber_ - 1), owner_->selectedObjectLineNumber_);
                if (parsedLine.directive == QStringLiteral("point")
                    && isOrientationSupportedForParsedLine(parsedLine)) {
                    orientationApplicable = true;
                    orientationDegrees = pointOrientationFromParsedLine(parsedLine);
                }
            }
        } else if (owner_->selectedObjectKind_ == QStringLiteral("line") && owner_->selectedObjectVertexIndex_ >= 0) {
            if (const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(owner_->textEditor_->text(), owner_->selectedObjectLineNumber_);
                lineFeature.has_value() && lineFeature->kind == MapGeometryFeature::Kind::Line) {
                if (lineVertexIndexForSourceVertex(lineFeature.value(), owner_->selectedObjectVertexIndex_) >= 0) {
                    QStringList lines = owner_->textEditor_->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
                    for (QString &line : lines) {
                        if (line.endsWith(QLatin1Char('\r'))) {
                            line.chop(1);
                        }
                    }
                    if (owner_->selectedObjectLineNumber_ > lines.size()) {
                        orientationApplicable = false;
                    } else {
                        const TherionParsedLine parsedLine =
                            TherionDocumentParser::parseLine(lines.at(owner_->selectedObjectLineNumber_ - 1), owner_->selectedObjectLineNumber_);
                        if (isOrientationSupportedForParsedLine(parsedLine)) {
                            orientationApplicable = true;
                            orientationDegrees = linePointOrientationForSourceVertex(owner_->textEditor_->text(),
                                                                                    owner_->selectedObjectLineNumber_,
                                                                                    owner_->selectedObjectVertexIndex_);
                        }
                        if (isLinePointLeftSizeSupportedForParsedLine(parsedLine)) {
                            linePointLeftSizeApplicable = true;
                            linePointLeftSize = linePointLeftSizeForSourceVertex(owner_->textEditor_->text(),
                                                                                 owner_->selectedObjectLineNumber_,
                                                                                 owner_->selectedObjectVertexIndex_);
                        }
                    }
                }
            }
        }
    }

    owner_->objectOrientationEditor_->setVisible(orientationApplicable || linePointLeftSizeApplicable);
    owner_->objectOrientationEnabledCheck_->setVisible(orientationApplicable);
    owner_->objectOrientationSpin_->setVisible(orientationApplicable);
    owner_->linePointLeftSizeEnabledCheck_->setVisible(linePointLeftSizeApplicable);
    owner_->linePointLeftSizeSpin_->setVisible(linePointLeftSizeApplicable);
    if (orientationApplicable) {
        owner_->objectOrientationEnabledCheck_->setChecked(orientationDegrees.has_value());
        owner_->objectOrientationSpin_->setEnabled(owner_->objectOrientationEnabledCheck_->isChecked());
        owner_->objectOrientationSpin_->setValue(orientationDegrees.value_or(0.0));
    } else {
        owner_->objectOrientationEnabledCheck_->setChecked(false);
        owner_->objectOrientationSpin_->setEnabled(false);
        owner_->objectOrientationSpin_->setValue(0.0);
    }
    if (linePointLeftSizeApplicable) {
        owner_->linePointLeftSizeEnabledCheck_->setChecked(linePointLeftSize.has_value());
        owner_->linePointLeftSizeSpin_->setEnabled(owner_->linePointLeftSizeEnabledCheck_->isChecked());
        owner_->linePointLeftSizeSpin_->setValue(linePointLeftSize.value_or(40.0));
    } else {
        owner_->linePointLeftSizeEnabledCheck_->setChecked(false);
        owner_->linePointLeftSizeSpin_->setEnabled(false);
        owner_->linePointLeftSizeSpin_->setValue(40.0);
    }
    owner_->objectOrientationApplyButton_->setText(linePointLeftSizeApplicable ? owner_->tr("Apply Line Point Options") : owner_->tr("Apply Orientation"));
    owner_->objectOrientationApplyButton_->setEnabled(orientationApplicable || linePointLeftSizeApplicable);
    owner_->vertexSelectionSection_->setVisible(lineVertexActionsAvailable || orientationApplicable || linePointLeftSizeApplicable);
}
}
