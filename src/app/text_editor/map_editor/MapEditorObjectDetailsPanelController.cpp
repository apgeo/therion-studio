#include "MapEditorObjectDetailsPanelController.h"

#include "../TextEditorTab.h"
#include "MapEditorInspectorData.h"
#include "MapEditorAreaReferenceResolver.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "MapEditorStylePreviewWidget.h"
#include "../../../core/TherionDocumentParser.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
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
        const MapGeometryFeature::TH2LineVertex &vertex = lineFeature.lineVertices.at(index);
        if (vertex.anchorSourceVertexIndex == sourceVertexIndex
            || vertex.incomingSourceVertexIndex == sourceVertexIndex
            || vertex.outgoingSourceVertexIndex == sourceVertexIndex) {
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

QString MapEditorObjectDetailsPanelController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsPanelController", text);
}

const InspectorSymbolCatalog &MapEditorObjectDetailsPanelController::inspectorSymbolCatalog() const
{
    Q_ASSERT(context_.inspectorSymbolCatalog != nullptr);
    static const InspectorSymbolCatalog emptyCatalog;
    return context_.inspectorSymbolCatalog != nullptr ? *context_.inspectorSymbolCatalog : emptyCatalog;
}

const MapEditorOrientationApplicabilityByCommand &MapEditorObjectDetailsPanelController::orientationApplicabilityByCommand() const
{
    Q_ASSERT(context_.orientationApplicabilityByCommand != nullptr);
    static const MapEditorOrientationApplicabilityByCommand emptyApplicability;
    return context_.orientationApplicabilityByCommand != nullptr
        ? *context_.orientationApplicabilityByCommand
        : emptyApplicability;
}

void MapEditorObjectDetailsPanelController::refreshObjectDetailsPanel()
{
    if (context_.selectionLabel == nullptr
        || context_.selectionSection == nullptr
        || context_.selectionTitleLabel == nullptr
        || context_.vertexSection == nullptr
        || context_.vertexTitleLabel == nullptr
        || context_.geometrySection == nullptr
        || context_.advancedSection == nullptr
        || context_.deleteButton == nullptr
        || context_.areaReferenceLabel == nullptr
        || context_.quickFieldsEditor == nullptr
        || context_.quickIdentifierLabel == nullptr
        || context_.quickNameLabel == nullptr
        || context_.quickProjectionLabel == nullptr
        || context_.quickTypeLabel == nullptr
        || context_.quickSubtypeLabel == nullptr
        || context_.stylePreviewLabel == nullptr
        || context_.quickTypeCombo == nullptr
        || context_.quickSubtypeCombo == nullptr
        || context_.quickProjectionCombo == nullptr
        || context_.quickIdentifierEdit == nullptr
        || context_.quickNameEdit == nullptr
        || context_.stylePreview == nullptr
        || context_.vertexActionsEditor == nullptr
        || context_.vertexInsertBeforeButton == nullptr
        || context_.vertexInsertAfterButton == nullptr
        || context_.vertexDeleteButton == nullptr
        || context_.vertexSplitButton == nullptr
        || context_.metadataLabel == nullptr
        || context_.orientationEditor == nullptr
        || context_.linePointPreviousControlCheck == nullptr
        || context_.linePointSmoothCheck == nullptr
        || context_.linePointNextControlCheck == nullptr
        || context_.orientationEnabledCheck == nullptr
        || context_.orientationSpin == nullptr
        || context_.linePointLeftSizeEnabledCheck == nullptr
        || context_.linePointLeftSizeSpin == nullptr
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

    auto clearStylePreview = [this]() {
        context_.stylePreviewLabel->setVisible(false);
        context_.stylePreview->clearStyleSelection();
    };
    auto showStylePreview = [this](const QString &commandKind, const QString &type, const QString &subtype) {
        const QString normalizedCommand = commandKind.trimmed().toLower();
        const bool visible = normalizedCommand == QStringLiteral("point")
            || normalizedCommand == QStringLiteral("line")
            || normalizedCommand == QStringLiteral("area");
        context_.stylePreviewLabel->setVisible(visible);
        if (visible) {
            context_.stylePreview->setStyleSelection(normalizedCommand, type, subtype);
        } else {
            context_.stylePreview->clearStyleSelection();
        }
    };

    if (context_.pendingInsertQuickFields) {
        const std::optional<InspectorObjectQuickFields> pendingFields = context_.pendingInsertQuickFields();
        if (pendingFields.has_value() && !pendingFields->commandKind.trimmed().isEmpty()) {
            const QString commandKind = pendingFields->commandKind.trimmed().toLower();
            QString objectSectionTitle = commandKind;
            if (commandKind == QStringLiteral("line")) {
                objectSectionTitle = tr("New Line");
            } else if (commandKind == QStringLiteral("area")) {
                objectSectionTitle = tr("New Area");
            } else if (commandKind == QStringLiteral("point")) {
                objectSectionTitle = tr("New Point");
            } else if (commandKind == QStringLiteral("scrap")) {
                objectSectionTitle = tr("New Scrap");
            }

            context_.selectionLabel->setVisible(false);
            context_.selectionSection->setVisible(true);
            context_.selectionTitleLabel->setText(objectSectionTitle);
            context_.vertexTitleLabel->setText(tr("Point Details"));
            context_.metadataLabel->setText(tr("Pending insert"));
            context_.deleteButton->setEnabled(false);
            context_.deleteButton->setToolTip(QString());
            context_.areaReferenceLabel->setVisible(false);
            context_.areaReferenceLabel->clear();
            context_.quickFieldsEditor->setVisible(true);
            context_.objectQuickCommandKind->clear();
            *context_.objectQuickCommandKind = commandKind;

            const bool typeFieldsVisible = commandKind != QStringLiteral("scrap");
            const bool projectionFieldVisible = commandKind == QStringLiteral("scrap");
            context_.quickNameLabel->setVisible(pendingFields->nameVisible);
            context_.quickProjectionLabel->setVisible(projectionFieldVisible);
            context_.quickTypeLabel->setVisible(typeFieldsVisible);
            context_.quickSubtypeLabel->setVisible(typeFieldsVisible);
            context_.quickNameEdit->setVisible(pendingFields->nameVisible);
            context_.quickProjectionCombo->setVisible(projectionFieldVisible);
            context_.quickTypeCombo->setVisible(typeFieldsVisible);
            context_.quickSubtypeCombo->setVisible(typeFieldsVisible);
            context_.quickProjectionCombo->setEnabled(projectionFieldVisible);
            context_.quickTypeCombo->setEnabled(typeFieldsVisible && pendingFields->typeEditable);
            context_.quickSubtypeCombo->setEnabled(typeFieldsVisible && pendingFields->typeEditable);

            const InspectorSymbolCatalog &catalog = inspectorSymbolCatalog();
            setEditableComboValues(context_.quickTypeCombo,
                                   inspectorTypeValuesForCommand(catalog, commandKind),
                                   pendingFields->type);
            setEditableComboValues(context_.quickProjectionCombo,
                                   inspectorProjectionValues(catalog),
                                   pendingFields->projection);
            setEditableComboValues(context_.quickSubtypeCombo,
                                   inspectorSubtypeValuesForCommandType(catalog, commandKind, pendingFields->type),
                                   pendingFields->subtype);
            context_.quickIdentifierLabel->setText(tr("ID"));
            context_.quickIdentifierEdit->setText(pendingFields->identifier);
            context_.quickNameEdit->setText(pendingFields->name);
            showStylePreview(commandKind, pendingFields->type, pendingFields->subtype);

            context_.vertexSection->setVisible(false);
            context_.geometrySection->setVisible(false);
            context_.advancedSection->setVisible(false);
            context_.vertexActionsEditor->setVisible(false);
            context_.orientationEditor->setVisible(false);
            context_.lineOptionsEditor->setVisible(false);
            context_.scrapScaleEditor->setVisible(false);
            context_.configureButton->setVisible(false);
            context_.configureButton->setEnabled(false);
            context_.lineClosedCheck->setChecked(false);
            context_.lineReversedCheck->setChecked(false);
            return;
        }
    }

    const int effectiveLineNumber = *context_.selectedObjectLineNumber;
    const QString effectiveKind = context_.selectedObjectKind->trimmed().toLower();

    if (effectiveLineNumber <= 0 || effectiveKind.isEmpty()) {
        context_.selectionLabel->setText(tr("No map object selected."));
        context_.selectionLabel->setVisible(true);
        context_.selectionTitleLabel->setText(tr("Object"));
        context_.selectionSection->setVisible(false);
        clearStylePreview();
        context_.vertexSection->setVisible(false);
        context_.geometrySection->setVisible(false);
        context_.advancedSection->setVisible(false);
        context_.deleteButton->setEnabled(false);
        context_.deleteButton->setToolTip(QString());
        context_.areaReferenceLabel->setVisible(false);
        context_.areaReferenceLabel->clear();
        context_.quickFieldsEditor->setVisible(false);
        context_.objectQuickCommandKind->clear();
        context_.vertexActionsEditor->setVisible(false);
        context_.metadataLabel->setText(QStringLiteral("-"));
        context_.orientationEditor->setVisible(false);
        context_.linePointPreviousControlCheck->setChecked(false);
        context_.linePointPreviousControlCheck->setVisible(false);
        context_.linePointSmoothCheck->setChecked(false);
        context_.linePointSmoothCheck->setVisible(false);
        context_.linePointNextControlCheck->setChecked(false);
        context_.linePointNextControlCheck->setVisible(false);
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
        objectSectionTitle = tr("Line");
    } else if (effectiveKind == QStringLiteral("area")) {
        objectSectionTitle = tr("Area");
    } else if (effectiveKind == QStringLiteral("point")) {
        objectSectionTitle = tr("Point");
    } else if (effectiveKind == QStringLiteral("scrap")) {
        objectSectionTitle = tr("Scrap");
    }
    context_.selectionTitleLabel->setText(objectSectionTitle);
    context_.vertexTitleLabel->setText(effectiveKind == QStringLiteral("line")
                                           ? tr("Line Point")
                                           : tr("Point Details"));
    context_.metadataLabel->setText(tr("Source line %1").arg(effectiveLineNumber));
    QVector<MapEditorAreaReference> areaReferences;
    if (context_.textEditor != nullptr
        && effectiveLineNumber > 0
        && effectiveKind == QStringLiteral("line")) {
        areaReferences = mapEditorAreaReferencesForBorderLine(context_.textEditor->text(), effectiveLineNumber);
    }
    const bool deleteBlockedByAreaReference = !areaReferences.isEmpty();
    context_.deleteButton->setEnabled(effectiveLineNumber > 0 && !deleteBlockedByAreaReference);
    context_.deleteButton->setToolTip(deleteBlockedByAreaReference
        ? tr("This line is used as an area border. Select or delete the area instead.")
        : QString());
    context_.areaReferenceLabel->setVisible(deleteBlockedByAreaReference);
    if (deleteBlockedByAreaReference) {
        QStringList areaLinks;
        areaLinks.reserve(areaReferences.size());
        for (const MapEditorAreaReference &reference : areaReferences) {
            const QString label = reference.areaLabel.toHtmlEscaped();
            areaLinks.append(QStringLiteral("<a href=\"%1\">%2</a>").arg(reference.areaLineNumber).arg(label));
        }
        context_.areaReferenceLabel->setText(tr("Used by area: %1").arg(areaLinks.join(QStringLiteral(", "))));
        context_.areaReferenceLabel->setToolTip(tr("Click the area name to select the area that owns this border line."));
    } else {
        context_.areaReferenceLabel->clear();
        context_.areaReferenceLabel->setToolTip(QString());
    }

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
                const InspectorSymbolCatalog &catalog = inspectorSymbolCatalog();
                setEditableComboValues(context_.quickTypeCombo,
                                       inspectorTypeValuesForCommand(catalog, fields->commandKind),
                                       fields->type);
                setEditableComboValues(context_.quickProjectionCombo,
                                       inspectorProjectionValues(catalog),
                                       fields->projection);
                context_.quickIdentifierEdit->setText(fields->identifier);
                context_.quickIdentifierLabel->setText(tr("ID"));
                context_.quickNameEdit->setText(fields->name);
                *context_.objectQuickCommandKind = fields->commandKind;
                setEditableComboValues(context_.quickSubtypeCombo,
                                       inspectorSubtypeValuesForCommandType(catalog, fields->commandKind, fields->type),
                                       fields->subtype);
                showStylePreview(fields->commandKind, fields->type, fields->subtype);
            }
        }
    }
    if (!context_.quickFieldsEditor->isVisible()) {
        clearStylePreview();
    }
    context_.configureButton->setVisible(true);
    context_.configureButton->setEnabled(isConfigurableMapObjectKind(effectiveKind));

    const bool lineVertexActionsAvailable = *context_.selectedObjectKind == QStringLiteral("line")
        && *context_.selectedObjectVertexIndex >= 0
        && context_.textEditor != nullptr;
    context_.vertexActionsEditor->setVisible(lineVertexActionsAvailable);
    context_.vertexInsertBeforeButton->setEnabled(lineVertexActionsAvailable);
    context_.vertexInsertAfterButton->setEnabled(lineVertexActionsAvailable);
    context_.vertexDeleteButton->setEnabled(lineVertexActionsAvailable);
    context_.vertexSplitButton->setEnabled(false);
    context_.vertexSplitButton->setToolTip(tr("Split Here is available for interior vertices of open lines."));
    if (lineVertexActionsAvailable) {
        bool firstVertex = false;
        bool lastVertex = false;
        if (const std::optional<MapGeometryFeature> lineFeature =
                lineFeatureForLineNumber(context_.textEditor->text(), *context_.selectedObjectLineNumber);
            lineFeature.has_value() && lineFeature->kind == MapGeometryFeature::Kind::Line) {
            const int lineVertexIndex = lineVertexIndexForSourceVertex(lineFeature.value(), *context_.selectedObjectVertexIndex);
            firstVertex = lineVertexIndex == 0;
            lastVertex = lineVertexIndex == lineFeature->lineVertices.size() - 1;
            const bool interiorVertex = lineVertexIndex > 0 && lineVertexIndex < lineFeature->lineVertices.size() - 1;
            context_.vertexSplitButton->setEnabled(interiorVertex && !lineFeature->closed);
            if (!interiorVertex) {
                context_.vertexSplitButton->setToolTip(tr("Cannot split at the first or last vertex."));
            } else if (lineFeature->closed) {
                context_.vertexSplitButton->setToolTip(tr("Cannot split closed lines yet."));
            } else {
                context_.vertexSplitButton->setToolTip(tr("Split the selected line at this vertex."));
            }
        }
        context_.vertexInsertBeforeButton->setText(firstVertex ? tr("Extend Before") : tr("Insert Before"));
        context_.vertexInsertAfterButton->setText(lastVertex ? tr("Extend After") : tr("Insert After"));
        context_.vertexInsertBeforeButton->setToolTip(firstVertex
                                                          ? tr("Extend the line before the first vertex.")
                                                          : tr("Insert a vertex before the selected vertex."));
        context_.vertexInsertAfterButton->setToolTip(lastVertex
                                                         ? tr("Extend the line after the last vertex.")
                                                         : tr("Insert a vertex after the selected vertex."));
    } else {
        context_.vertexInsertBeforeButton->setText(tr("Insert Before"));
        context_.vertexInsertAfterButton->setText(tr("Insert After"));
        context_.vertexInsertBeforeButton->setToolTip(QString());
        context_.vertexInsertAfterButton->setToolTip(QString());
        context_.vertexSplitButton->setToolTip(QString());
    }

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
    bool linePointSmoothApplicable = false;
    bool linePointSmooth = false;
    bool linePointPreviousControl = false;
    bool linePointNextControl = false;
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
                    && isOrientationSupportedForParsedLine(parsedLine, orientationApplicabilityByCommand())) {
                    orientationApplicable = true;
                    orientationDegrees = pointOrientationFromParsedLine(parsedLine);
                }
            }
        } else if (*context_.selectedObjectKind == QStringLiteral("line") && *context_.selectedObjectVertexIndex >= 0) {
            if (const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context_.textEditor->text(), *context_.selectedObjectLineNumber);
                lineFeature.has_value() && lineFeature->kind == MapGeometryFeature::Kind::Line) {
                const int lineVertexIndex = lineVertexIndexForSourceVertex(lineFeature.value(), *context_.selectedObjectVertexIndex);
                if (lineVertexIndex >= 0) {
                    linePointSmoothApplicable = true;
                    const MapGeometryFeature::TH2LineVertex &lineVertex = lineFeature->lineVertices.at(lineVertexIndex);
                    linePointPreviousControl = lineVertex.incomingControl.has_value();
                    linePointNextControl = lineVertex.outgoingControl.has_value();
                    linePointSmooth = lineVertex.isSmooth && linePointPreviousControl && linePointNextControl;
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
                        if (isOrientationSupportedForParsedLine(parsedLine, orientationApplicabilityByCommand())) {
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

    context_.orientationEditor->setVisible(linePointSmoothApplicable || orientationApplicable || linePointLeftSizeApplicable);
    context_.linePointPreviousControlCheck->setVisible(linePointSmoothApplicable);
    context_.linePointPreviousControlCheck->setEnabled(linePointSmoothApplicable);
    context_.linePointPreviousControlCheck->setChecked(linePointSmoothApplicable && linePointPreviousControl);
    context_.linePointSmoothCheck->setVisible(linePointSmoothApplicable);
    context_.linePointSmoothCheck->setEnabled(linePointSmoothApplicable);
    context_.linePointSmoothCheck->setChecked(linePointSmoothApplicable && linePointSmooth);
    context_.linePointNextControlCheck->setVisible(linePointSmoothApplicable);
    context_.linePointNextControlCheck->setEnabled(linePointSmoothApplicable);
    context_.linePointNextControlCheck->setChecked(linePointSmoothApplicable && linePointNextControl);
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
    context_.vertexSection->setVisible(lineVertexActionsAvailable
                                       || linePointSmoothApplicable
                                       || orientationApplicable
                                       || linePointLeftSizeApplicable);
}
}
