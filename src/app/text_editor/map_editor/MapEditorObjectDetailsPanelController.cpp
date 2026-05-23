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

#include <utility>

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

MapEditorObjectDetailsPanelController::MapEditorObjectDetailsPanelController(MapEditorObjectDetailsContext context)
    : context_(std::move(context))
{
}

QString MapEditorObjectDetailsPanelController::translate(const char *text) const
{
    return context_.translate ? context_.translate(text) : QString::fromUtf8(text);
}

void MapEditorObjectDetailsPanelController::refreshObjectDetailsPanel()
{
    if (context_.selectionLabel == nullptr
        || context_.selectionSection == nullptr
        || context_.selectionTitleLabel == nullptr
        || context_.vertexSection == nullptr
        || context_.geometrySection == nullptr
        || context_.advancedSection == nullptr
        || context_.deleteButton == nullptr
        || context_.quickFieldsEditor == nullptr
        || context_.quickIdentifierLabel == nullptr
        || context_.quickNameLabel == nullptr
        || context_.quickProjectionLabel == nullptr
        || context_.quickTypeLabel == nullptr
        || context_.quickSubtypeLabel == nullptr
        || context_.quickTypeCombo == nullptr
        || context_.quickSubtypeCombo == nullptr
        || context_.quickProjectionCombo == nullptr
        || context_.quickIdentifierEdit == nullptr
        || context_.quickNameEdit == nullptr
        || context_.vertexActionsEditor == nullptr
        || context_.vertexInsertButton == nullptr
        || context_.vertexDeleteButton == nullptr
        || context_.vertexToggleSmoothButton == nullptr
        || context_.metadataLabel == nullptr
        || context_.orientationEditor == nullptr
        || context_.orientationEnabledCheck == nullptr
        || context_.orientationSpin == nullptr
        || context_.linePointLeftSizeEnabledCheck == nullptr
        || context_.linePointLeftSizeSpin == nullptr
        || context_.orientationApplyButton == nullptr
        || context_.lineOptionsEditor == nullptr
        || context_.lineClosedCheck == nullptr
        || context_.lineReversedCheck == nullptr
        || context_.scrapScaleEditor == nullptr
        || context_.scrapScaleSourceX1Spin == nullptr
        || context_.scrapScaleSourceY1Spin == nullptr
        || context_.scrapScaleSourceX2Spin == nullptr
        || context_.scrapScaleSourceY2Spin == nullptr
        || context_.scrapScaleRealX1Spin == nullptr
        || context_.scrapScaleRealY1Spin == nullptr
        || context_.scrapScaleRealX2Spin == nullptr
        || context_.scrapScaleRealY2Spin == nullptr
        || context_.scrapScaleUnitCombo == nullptr
        || context_.configureButton == nullptr) {
        return;
    }

    const QScopedValueRollback<bool> uiGuard(*context_.updatingUi, true);

    int effectiveLineNumber = *context_.selectedObjectLineNumber;
    QString effectiveKind = context_.selectedObjectKind->trimmed().toLower();

    if (!(effectiveLineNumber > 0 && isConfigurableMapObjectKind(effectiveKind)) && context_.textEditor != nullptr) {
        const int cursorLineNumber = context_.textEditor->currentLineNumber();
        if (cursorLineNumber > 0) {
            QStringList lines = context_.textEditor->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
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
        context_.selectionLabel->setText(translate("No map object selected."));
        context_.selectionLabel->setVisible(true);
        context_.selectionTitleLabel->setText(translate("Object"));
        context_.selectionSection->setVisible(false);
        context_.vertexSection->setVisible(false);
        context_.geometrySection->setVisible(false);
        context_.advancedSection->setVisible(false);
        context_.deleteButton->setEnabled(false);
        context_.quickFieldsEditor->setVisible(false);
        context_.objectQuickCommandKind->clear();
        context_.vertexActionsEditor->setVisible(false);
        context_.metadataLabel->setText(QStringLiteral("-"));
        context_.orientationEditor->setVisible(false);
        context_.linePointLeftSizeEnabledCheck->setChecked(false);
        context_.linePointLeftSizeSpin->setEnabled(false);
        context_.linePointLeftSizeSpin->setValue(40.0);
        context_.lineOptionsEditor->setVisible(false);
        context_.scrapScaleEditor->setVisible(false);
        context_.configureButton->setVisible(false);
        context_.configureButton->setEnabled(false);
        context_.lineClosedCheck->setChecked(false);
        context_.lineReversedCheck->setChecked(false);
        return;
    }

    context_.selectionLabel->setVisible(false);
    context_.selectionSection->setVisible(true);
    context_.advancedSection->setVisible(true);
    QString objectSectionTitle = effectiveKind;
    if (effectiveKind == QStringLiteral("line")) {
        objectSectionTitle = translate("Line");
    } else if (effectiveKind == QStringLiteral("area")) {
        objectSectionTitle = translate("Area");
    } else if (effectiveKind == QStringLiteral("point")) {
        objectSectionTitle = translate("Point");
    } else if (effectiveKind == QStringLiteral("scrap")) {
        objectSectionTitle = translate("Scrap");
    }
    context_.selectionTitleLabel->setText(objectSectionTitle);
    context_.metadataLabel->setText(translate("Source line %1").arg(effectiveLineNumber));
    context_.deleteButton->setEnabled(effectiveLineNumber > 0);

    context_.quickFieldsEditor->setVisible(false);
    context_.objectQuickCommandKind->clear();
    if (context_.textEditor != nullptr && effectiveLineNumber > 0) {
        QStringList lines = context_.textEditor->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
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
                context_.quickFieldsEditor->setVisible(true);
                context_.quickNameLabel->setVisible(fields->nameVisible);
                context_.quickProjectionLabel->setVisible(projectionFieldVisible);
                context_.quickTypeLabel->setVisible(typeFieldsVisible);
                context_.quickSubtypeLabel->setVisible(typeFieldsVisible);
                context_.quickNameEdit->setVisible(fields->nameVisible);
                context_.quickProjectionCombo->setVisible(projectionFieldVisible);
                context_.quickTypeCombo->setVisible(typeFieldsVisible);
                context_.quickSubtypeCombo->setVisible(typeFieldsVisible);
                context_.quickProjectionCombo->setEnabled(projectionFieldVisible);
                context_.quickTypeCombo->setEnabled(typeFieldsVisible && fields->typeEditable);
                context_.quickSubtypeCombo->setEnabled(typeFieldsVisible && fields->typeEditable);
                setEditableComboValues(context_.quickTypeCombo, inspectorTypeValuesForCommand(fields->commandKind), fields->type);
                setEditableComboValues(context_.quickProjectionCombo, inspectorProjectionValues(), fields->projection);
                context_.quickIdentifierEdit->setText(fields->identifier);
                context_.quickIdentifierLabel->setText(translate("ID"));
                context_.quickNameEdit->setText(fields->name);
                *context_.objectQuickCommandKind = fields->commandKind;
                setEditableComboValues(context_.quickSubtypeCombo,
                                       inspectorSubtypeValuesForCommandType(fields->commandKind, fields->type),
                                       fields->subtype);
            }
        }
    }
    context_.configureButton->setVisible(true);
    context_.configureButton->setEnabled(isConfigurableMapObjectKind(effectiveKind));

    const bool lineVertexActionsAvailable = *context_.selectedObjectKind == QStringLiteral("line")
        && *context_.selectedObjectVertexIndex >= 0
        && context_.textEditor != nullptr;
    context_.vertexActionsEditor->setVisible(lineVertexActionsAvailable);
    context_.vertexInsertButton->setEnabled(lineVertexActionsAvailable);
    context_.vertexDeleteButton->setEnabled(lineVertexActionsAvailable);
    context_.vertexToggleSmoothButton->setEnabled(lineVertexActionsAvailable);

    const bool lineOptionsVisible = *context_.selectedObjectLineNumber > 0
        && *context_.selectedObjectKind == QStringLiteral("line")
        && context_.textEditor != nullptr;
    context_.lineOptionsEditor->setVisible(lineOptionsVisible);
    if (lineOptionsVisible) {
        if (const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context_.textEditor->text(), *context_.selectedObjectLineNumber);
            lineFeature.has_value() && lineFeature->kind == MapGeometryFeature::Kind::Line) {
            context_.lineClosedCheck->setChecked(lineFeature->closed);
            context_.lineReversedCheck->setChecked(lineFeature->reversed);
        } else {
            context_.lineClosedCheck->setChecked(false);
            context_.lineReversedCheck->setChecked(false);
        }
    } else {
        context_.lineClosedCheck->setChecked(false);
        context_.lineReversedCheck->setChecked(false);
    }

    context_.geometrySection->setVisible(lineOptionsVisible);

    const bool scrapScaleVisible = effectiveLineNumber > 0
        && effectiveKind == QStringLiteral("scrap")
        && context_.textEditor != nullptr;
    context_.scrapScaleEditor->setVisible(scrapScaleVisible);
    if (scrapScaleVisible) {
        auto configureScaleSpin = [](QDoubleSpinBox *spin, int decimals) {
            if (spin == nullptr) {
                return;
            }
            spin->setDecimals(decimals);
            spin->setSingleStep(decimals == 0 ? 1.0 : 0.1);
        };
        configureScaleSpin(context_.scrapScaleSourceX1Spin, 0);
        configureScaleSpin(context_.scrapScaleSourceY1Spin, 0);
        configureScaleSpin(context_.scrapScaleSourceX2Spin, 0);
        configureScaleSpin(context_.scrapScaleSourceY2Spin, 0);
        configureScaleSpin(context_.scrapScaleRealX1Spin, 4);
        configureScaleSpin(context_.scrapScaleRealY1Spin, 4);
        configureScaleSpin(context_.scrapScaleRealX2Spin, 4);
        configureScaleSpin(context_.scrapScaleRealY2Spin, 4);

        InspectorScrapScale scale = defaultInspectorScrapScale(context_.mapSourceBoundsForCurrentDocument());
        QStringList lines = context_.textEditor->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
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

        context_.scrapScaleSourceX1Spin->setValue(scale.sourcePoint1.x());
        context_.scrapScaleSourceY1Spin->setValue(scale.sourcePoint1.y());
        context_.scrapScaleSourceX2Spin->setValue(scale.sourcePoint2.x());
        context_.scrapScaleSourceY2Spin->setValue(scale.sourcePoint2.y());
        context_.scrapScaleRealX1Spin->setValue(scale.realPoint1.x());
        context_.scrapScaleRealY1Spin->setValue(scale.realPoint1.y());
        context_.scrapScaleRealX2Spin->setValue(scale.realPoint2.x());
        context_.scrapScaleRealY2Spin->setValue(scale.realPoint2.y());
        const int unitIndex = context_.scrapScaleUnitCombo->findText(scale.unitToken);
        context_.scrapScaleUnitCombo->setCurrentIndex(unitIndex >= 0 ? unitIndex : 0);
    }

    bool orientationApplicable = false;
    bool linePointLeftSizeApplicable = false;
    std::optional<qreal> orientationDegrees;
    std::optional<qreal> linePointLeftSize;
    if (context_.textEditor != nullptr && *context_.selectedObjectLineNumber > 0) {
        if (*context_.selectedObjectKind == QStringLiteral("point")) {
            QStringList lines = context_.textEditor->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
            for (QString &line : lines) {
                if (line.endsWith(QLatin1Char('\r'))) {
                    line.chop(1);
                }
            }
            if (*context_.selectedObjectLineNumber <= lines.size()) {
                const TherionParsedLine parsedLine =
                    TherionDocumentParser::parseLine(lines.at(*context_.selectedObjectLineNumber - 1), *context_.selectedObjectLineNumber);
                if (parsedLine.directive == QStringLiteral("point")
                    && isOrientationSupportedForParsedLine(parsedLine)) {
                    orientationApplicable = true;
                    orientationDegrees = pointOrientationFromParsedLine(parsedLine);
                }
            }
        } else if (*context_.selectedObjectKind == QStringLiteral("line") && *context_.selectedObjectVertexIndex >= 0) {
            if (const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context_.textEditor->text(), *context_.selectedObjectLineNumber);
                lineFeature.has_value() && lineFeature->kind == MapGeometryFeature::Kind::Line) {
                if (lineVertexIndexForSourceVertex(lineFeature.value(), *context_.selectedObjectVertexIndex) >= 0) {
                    QStringList lines = context_.textEditor->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
                    for (QString &line : lines) {
                        if (line.endsWith(QLatin1Char('\r'))) {
                            line.chop(1);
                        }
                    }
                    if (*context_.selectedObjectLineNumber > lines.size()) {
                        orientationApplicable = false;
                    } else {
                        const TherionParsedLine parsedLine =
                            TherionDocumentParser::parseLine(lines.at(*context_.selectedObjectLineNumber - 1), *context_.selectedObjectLineNumber);
                        if (isOrientationSupportedForParsedLine(parsedLine)) {
                            orientationApplicable = true;
                            orientationDegrees = linePointOrientationForSourceVertex(context_.textEditor->text(),
                                                                                    *context_.selectedObjectLineNumber,
                                                                                    *context_.selectedObjectVertexIndex);
                        }
                        if (isLinePointLeftSizeSupportedForParsedLine(parsedLine)) {
                            linePointLeftSizeApplicable = true;
                            linePointLeftSize = linePointLeftSizeForSourceVertex(context_.textEditor->text(),
                                                                                 *context_.selectedObjectLineNumber,
                                                                                 *context_.selectedObjectVertexIndex);
                        }
                    }
                }
            }
        }
    }

    context_.orientationEditor->setVisible(orientationApplicable || linePointLeftSizeApplicable);
    context_.orientationEnabledCheck->setVisible(orientationApplicable);
    context_.orientationSpin->setVisible(orientationApplicable);
    context_.linePointLeftSizeEnabledCheck->setVisible(linePointLeftSizeApplicable);
    context_.linePointLeftSizeSpin->setVisible(linePointLeftSizeApplicable);
    if (orientationApplicable) {
        context_.orientationEnabledCheck->setChecked(orientationDegrees.has_value());
        context_.orientationSpin->setEnabled(context_.orientationEnabledCheck->isChecked());
        context_.orientationSpin->setValue(orientationDegrees.value_or(0.0));
    } else {
        context_.orientationEnabledCheck->setChecked(false);
        context_.orientationSpin->setEnabled(false);
        context_.orientationSpin->setValue(0.0);
    }
    if (linePointLeftSizeApplicable) {
        context_.linePointLeftSizeEnabledCheck->setChecked(linePointLeftSize.has_value());
        context_.linePointLeftSizeSpin->setEnabled(context_.linePointLeftSizeEnabledCheck->isChecked());
        context_.linePointLeftSizeSpin->setValue(linePointLeftSize.value_or(40.0));
    } else {
        context_.linePointLeftSizeEnabledCheck->setChecked(false);
        context_.linePointLeftSizeSpin->setEnabled(false);
        context_.linePointLeftSizeSpin->setValue(40.0);
    }
    context_.orientationApplyButton->setText(linePointLeftSizeApplicable ? translate("Apply Line Point Options") : translate("Apply Orientation"));
    context_.orientationApplyButton->setEnabled(orientationApplicable || linePointLeftSizeApplicable);
    context_.vertexSection->setVisible(lineVertexActionsAvailable || orientationApplicable || linePointLeftSizeApplicable);
}
}
