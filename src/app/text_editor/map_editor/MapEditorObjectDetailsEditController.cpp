#include "MapEditorObjectDetailsEditController.h"

#include "../TextEditorTab.h"
#include "MapEditorAreaReferenceResolver.h"
#include "MapEditorInspectorData.h"
#include "MapEditorObjectDeletePlanner.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../../../core/TherionDocumentEditor.h"
#include "../../../core/TherionDocumentParser.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLineF>
#include <QPlainTextEdit>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QTimer>

#include <cmath>
#include <utility>

namespace TherionStudio
{
namespace
{
constexpr int kImmediatePointOptionApplyDelayMs = 0;

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

std::optional<std::pair<int, int>> selectedLineAndSourceVertex(const MapEditorObjectDetailsContext &context)
{
    if (context.selectedObjectLineNumber == nullptr
        || context.selectedObjectVertexIndex == nullptr
        || context.selectedObjectKind == nullptr
        || (*context.selectedObjectKind) != QStringLiteral("line")
        || (*context.selectedObjectLineNumber) <= 0
        || (*context.selectedObjectVertexIndex) < 0) {
        return std::nullopt;
    }

    return std::make_pair(*context.selectedObjectLineNumber, *context.selectedObjectVertexIndex);
}

QStringList trimmedLinePointStandaloneRows(const QString &rawRowsText)
{
    QStringList rows = rawRowsText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    QStringList trimmedRows;
    trimmedRows.reserve(rows.size());
    for (QString row : rows) {
        if (row.endsWith(QLatin1Char('\r'))) {
            row.chop(1);
        }
        row = row.trimmed();
        if (!row.isEmpty()) {
            trimmedRows.append(row);
        }
    }
    return trimmedRows;
}

qreal defaultLinePointOrientationDegrees(const MapGeometryFeature &feature, int vertexIndex)
{
    if (vertexIndex < 0 || vertexIndex >= feature.lineVertices.size()) {
        return 0.0;
    }

    const MapGeometryFeature::TH2LineVertex &vertex = feature.lineVertices.at(vertexIndex);
    QPointF previous = vertex.incomingControl.value_or(vertex.anchor);
    QPointF next = vertex.outgoingControl.value_or(vertex.anchor);

    qreal previousDistance = std::hypot(previous.x() - vertex.anchor.x(), previous.y() - vertex.anchor.y());
    qreal nextDistance = std::hypot(next.x() - vertex.anchor.x(), next.y() - vertex.anchor.y());
    if (previousDistance <= 1e-6 && vertexIndex > 0) {
        previous = feature.lineVertices.at(vertexIndex - 1).anchor;
        previousDistance = std::hypot(previous.x() - vertex.anchor.x(), previous.y() - vertex.anchor.y());
    }
    if (nextDistance <= 1e-6 && vertexIndex + 1 < feature.lineVertices.size()) {
        next = feature.lineVertices.at(vertexIndex + 1).anchor;
        nextDistance = std::hypot(next.x() - vertex.anchor.x(), next.y() - vertex.anchor.y());
    }

    if (previousDistance > 1e-6 && nextDistance > 1e-6) {
        if (previousDistance > nextDistance) {
            const QPointF delta = next - vertex.anchor;
            next = vertex.anchor + (delta * (previousDistance / nextDistance));
        } else {
            const QPointF delta = previous - vertex.anchor;
            previous = vertex.anchor + (delta * (nextDistance / previousDistance));
        }
    } else if (previousDistance <= 1e-6 && nextDistance <= 1e-6) {
        return 0.0;
    }

    const QPointF tangent = next - previous;
    if (std::hypot(tangent.x(), tangent.y()) <= 1e-6) {
        return 0.0;
    }

    constexpr qreal pi = 3.14159265358979323846;
    qreal orientation = 360.0 - (std::atan2(tangent.y(), tangent.x()) * 180.0 / pi);
    if (feature.reversed) {
        orientation += 180.0;
    }
    return normalizeOrientationDegreesForMapDetails(orientation);
}

std::optional<qreal> defaultLinePointOrientationForSourceVertex(const QString &documentText, int lineNumber, int sourceVertexIndex)
{
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(documentText, lineNumber);
    if (!lineFeature.has_value() || lineFeature->kind != MapGeometryFeature::Kind::Line) {
        return std::nullopt;
    }

    const int vertexIndex = lineVertexIndexForSourceVertex(lineFeature.value(), sourceVertexIndex);
    if (vertexIndex < 0) {
        return std::nullopt;
    }

    return defaultLinePointOrientationDegrees(lineFeature.value(), vertexIndex);
}

void scheduleObjectOrientationApply(const MapEditorObjectDetailsContext &context)
{
    if (context.callbackContext == nullptr) {
        MapEditorObjectDetailsEditController(context).applyObjectOrientationEdits();
        return;
    }

    QTimer::singleShot(kImmediatePointOptionApplyDelayMs, context.callbackContext, [context]() {
        if (context.updatingUi == nullptr || *context.updatingUi) {
            return;
        }
        MapEditorObjectDetailsEditController(context).applyObjectOrientationEdits();
    });
}
}

MapEditorObjectDetailsEditController::MapEditorObjectDetailsEditController(MapEditorObjectDetailsContext context)
    : context_(std::move(context))
{
}

QString MapEditorObjectDetailsEditController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::MapEditorObjectDetailsEditController", text);
}

const InspectorSymbolCatalog &MapEditorObjectDetailsEditController::inspectorSymbolCatalog() const
{
    Q_ASSERT(context_.inspectorSymbolCatalog != nullptr);
    static const InspectorSymbolCatalog emptyCatalog;
    return context_.inspectorSymbolCatalog != nullptr ? *context_.inspectorSymbolCatalog : emptyCatalog;
}

const MapEditorOrientationApplicabilityByCommand &MapEditorObjectDetailsEditController::orientationApplicabilityByCommand() const
{
    Q_ASSERT(context_.orientationApplicabilityByCommand != nullptr);
    static const MapEditorOrientationApplicabilityByCommand emptyApplicability;
    return context_.orientationApplicabilityByCommand != nullptr
        ? *context_.orientationApplicabilityByCommand
        : emptyApplicability;
}

void MapEditorObjectDetailsEditController::populateScrapScaleFromSourceBounds()
{
    if (*context_.updatingUi
        || context_.scrapScaleSourceX1Spin == nullptr
        || context_.scrapScaleSourceY1Spin == nullptr
        || context_.scrapScaleSourceX2Spin == nullptr
        || context_.scrapScaleSourceY2Spin == nullptr
        || context_.scrapScaleRealX1Spin == nullptr
        || context_.scrapScaleRealY1Spin == nullptr
        || context_.scrapScaleRealX2Spin == nullptr
        || context_.scrapScaleRealY2Spin == nullptr
        || context_.scrapScaleUnitCombo == nullptr) {
        return;
    }

    context_.scrapScaleSourceX1Spin->setDecimals(0);
    context_.scrapScaleSourceY1Spin->setDecimals(0);
    context_.scrapScaleSourceX2Spin->setDecimals(0);
    context_.scrapScaleSourceY2Spin->setDecimals(0);
    context_.scrapScaleRealX1Spin->setDecimals(4);
    context_.scrapScaleRealY1Spin->setDecimals(4);
    context_.scrapScaleRealX2Spin->setDecimals(4);
    context_.scrapScaleRealY2Spin->setDecimals(4);

    const InspectorScrapScale scale = defaultInspectorScrapScale(context_.mapSourceBoundsForCurrentDocument());
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

void MapEditorObjectDetailsEditController::applyScrapScaleEdits()
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || context_.scrapScaleSourceX1Spin == nullptr
        || context_.scrapScaleSourceY1Spin == nullptr
        || context_.scrapScaleSourceX2Spin == nullptr
        || context_.scrapScaleSourceY2Spin == nullptr
        || context_.scrapScaleRealX1Spin == nullptr
        || context_.scrapScaleRealY1Spin == nullptr
        || context_.scrapScaleRealX2Spin == nullptr
        || context_.scrapScaleRealY2Spin == nullptr
        || context_.scrapScaleUnitCombo == nullptr) {
        return;
    }

    int targetLineNumber = 0;
    if (*context_.selectedObjectLineNumber > 0 && *context_.selectedObjectKind == QStringLiteral("scrap")) {
        targetLineNumber = *context_.selectedObjectLineNumber;
    } else {
        const int cursorLineNumber = context_.textEditor->currentLineNumber();
        QStringList lines = context_.textEditor->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        for (QString &line : lines) {
            if (line.endsWith(QLatin1Char('\r'))) {
                line.chop(1);
            }
        }
        if (cursorLineNumber > 0 && cursorLineNumber <= lines.size()) {
            const TherionParsedLine parsedLine =
                TherionDocumentParser::parseLine(lines.at(cursorLineNumber - 1), cursorLineNumber);
            if (parsedLine.directive == QStringLiteral("scrap")) {
                targetLineNumber = cursorLineNumber;
            }
        }
    }

    if (targetLineNumber <= 0) {
        *context_.toolbarStatusNote = tr("Select a scrap to edit its scale.");
        context_.refreshToolbarSummary();
        return;
    }

    InspectorScrapScale scale;
    scale.sourcePoint1 = QPointF(context_.scrapScaleSourceX1Spin->value(), context_.scrapScaleSourceY1Spin->value());
    scale.sourcePoint2 = QPointF(context_.scrapScaleSourceX2Spin->value(), context_.scrapScaleSourceY2Spin->value());
    scale.realPoint1 = QPointF(context_.scrapScaleRealX1Spin->value(), context_.scrapScaleRealY1Spin->value());
    scale.realPoint2 = QPointF(context_.scrapScaleRealX2Spin->value(), context_.scrapScaleRealY2Spin->value());
    scale.unitToken = context_.scrapScaleUnitCombo->currentText().trimmed();
    if (scale.unitToken.isEmpty()) {
        scale.unitToken = QStringLiteral("m");
    }

    if (QLineF(scale.sourcePoint1, scale.sourcePoint2).length() <= 1e-6
        || QLineF(scale.realPoint1, scale.realPoint2).length() <= 1e-6) {
        *context_.toolbarStatusNote = tr("Scrap scale requires two distinct picture points and two distinct real points.");
        context_.refreshToolbarSummary();
        return;
    }

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewriteScrapScale(&afterText,
                                                  targetLineNumber,
                                                  scrapScaleExpression(scale),
                                                  &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update scrap scale.")
            : tr("Failed to update scrap scale: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (context_.applySourceTextChangeWithSnapshot) {
        context_.applySourceTextChangeWithSnapshot(tr("Set Scrap Scale"), beforeText, afterText, targetLineNumber);
    } else {
        const QScopedValueRollback<bool> commandGuard(*context_.commandApplyInProgress, true);
        context_.textEditor->replaceTextForCommand(afterText);
        context_.recordSourceTextSnapshot(tr("Set Scrap Scale"), beforeText, afterText, targetLineNumber);
    }
    *context_.toolbarStatusNote = tr("Updated scrap scale.");
    context_.refreshToolbarSummary();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleConfigureObjectSettingsTriggered()
{
    if (context_.textEditor == nullptr) {
        return;
    }

    int targetLineNumber = *context_.selectedObjectLineNumber;
    QString targetKind = context_.selectedObjectKind->trimmed().toLower();
    if (!(targetLineNumber > 0 && isConfigurableMapObjectKind(targetKind))) {
        targetLineNumber = context_.textEditor->currentLineNumber();
        if (targetLineNumber > 0) {
            QStringList lines = context_.textEditor->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
            for (QString &line : lines) {
                if (line.endsWith(QLatin1Char('\r'))) {
                    line.chop(1);
                }
            }
            if (targetLineNumber <= lines.size()) {
                const TherionParsedLine parsedLine =
                    TherionDocumentParser::parseLine(lines.at(targetLineNumber - 1), targetLineNumber);
                targetKind = objectKindForDirective(parsedLine.directive);
            }
        }
    }

    if (targetLineNumber <= 0 || !isConfigurableMapObjectKind(targetKind)) {
        *context_.toolbarStatusNote = tr("Object settings are available for scrap, point, line, and area commands.");
        context_.refreshToolbarSummary();
        return;
    }

    context_.textEditor->configureCommandAtLine(targetKind, targetLineNumber, true);
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleLineClosedToggled(bool checked)
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || *context_.selectedObjectLineNumber <= 0
        || *context_.selectedObjectKind != QStringLiteral("line")) {
        return;
    }

    const int targetLineNumber = *context_.selectedObjectLineNumber;
    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewriteLineOptionToggle(&afterText,
                                                        targetLineNumber,
                                                        QStringLiteral("close"),
                                                        checked,
                                                        &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update line closed state.")
            : tr("Failed to update line closed state: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (context_.applySourceTextChangeWithSnapshot) {
        context_.applySourceTextChangeWithSnapshot(tr("Edit Line Closed"),
                                                   beforeText,
                                                   afterText,
                                                   targetLineNumber);
    } else {
        const QScopedValueRollback<bool> commandGuard(*context_.commandApplyInProgress, true);
        context_.textEditor->replaceTextForCommand(afterText);
        context_.recordSourceTextSnapshot(tr("Edit Line Closed"),
                                          beforeText,
                                          afterText,
                                          targetLineNumber);
    }
}

void MapEditorObjectDetailsEditController::handleLineReversedToggled(bool checked)
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || *context_.selectedObjectLineNumber <= 0
        || *context_.selectedObjectKind != QStringLiteral("line")) {
        return;
    }

    const int targetLineNumber = *context_.selectedObjectLineNumber;
    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewriteLineOptionToggle(&afterText,
                                                        targetLineNumber,
                                                        QStringLiteral("reverse"),
                                                        checked,
                                                        &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update line reverse state.")
            : tr("Failed to update line reverse state: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (context_.applySourceTextChangeWithSnapshot) {
        context_.applySourceTextChangeWithSnapshot(tr("Edit Line Reversed"),
                                                   beforeText,
                                                   afterText,
                                                   targetLineNumber);
    } else {
        const QScopedValueRollback<bool> commandGuard(*context_.commandApplyInProgress, true);
        context_.textEditor->replaceTextForCommand(afterText);
        context_.recordSourceTextSnapshot(tr("Edit Line Reversed"),
                                          beforeText,
                                          afterText,
                                          targetLineNumber);
    }
}

void MapEditorObjectDetailsEditController::applyObjectOrientationEdits()
{
    if (*context_.updatingUi || context_.textEditor == nullptr || *context_.selectedObjectLineNumber <= 0) {
        return;
    }
    if (context_.orientationEnabledCheck == nullptr
        || context_.orientationSpin == nullptr
        || context_.linePointLeftSizeEnabledCheck == nullptr
        || context_.linePointLeftSizeSpin == nullptr) {
        return;
    }
    const QString selectedObjectKind = *context_.selectedObjectKind;
    const int selectedLineNumber = *context_.selectedObjectLineNumber;
    const int selectedSourceVertexIndex = *context_.selectedObjectVertexIndex;
    if (selectedObjectKind != QStringLiteral("point") && selectedObjectKind != QStringLiteral("line")) {
        return;
    }

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;

    const bool enabled = context_.orientationEnabledCheck->isChecked();
    qreal orientation = normalizeOrientationDegreesForMapDetails(context_.orientationSpin->value());
    if (enabled && selectedObjectKind == QStringLiteral("line")) {
        const std::optional<qreal> existingOrientation =
            linePointOrientationForSourceVertex(beforeText,
                                                selectedLineNumber,
                                                selectedSourceVertexIndex);
        if (!existingOrientation.has_value()) {
            if (const std::optional<qreal> defaultOrientation =
                    defaultLinePointOrientationForSourceVertex(beforeText,
                                                               selectedLineNumber,
                                                               selectedSourceVertexIndex)) {
                orientation = defaultOrientation.value();
                const QSignalBlocker blocker(context_.orientationSpin);
                context_.orientationSpin->setValue(orientation);
            }
        }
    }
    const bool leftSizeEnabled = context_.linePointLeftSizeEnabledCheck->isVisible()
        && context_.linePointLeftSizeEnabledCheck->isChecked();
    const qreal leftSize = qMax<qreal>(0.1, context_.linePointLeftSizeSpin->value());
    QStringList documentLines = beforeText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : documentLines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    QString errorMessage;
    bool rewritten = false;
    if (selectedObjectKind == QStringLiteral("point")) {
        if (selectedLineNumber > documentLines.size()) {
            return;
        }
        const TherionParsedLine parsedLine =
            TherionDocumentParser::parseLine(documentLines.at(selectedLineNumber - 1), selectedLineNumber);
        if (!isOrientationSupportedForParsedLine(parsedLine, orientationApplicabilityByCommand())) {
            *context_.toolbarStatusNote = tr("Orientation is not supported for this point type.");
            context_.refreshToolbarSummary();
            context_.refreshObjectDetailsPanel();
            return;
        }
        rewritten = TherionDocumentEditor::rewritePointOrientation(&afterText,
                                                                   selectedLineNumber,
                                                                   enabled,
                                                                   orientation,
                                                                   &errorMessage);
    } else {
        if (selectedSourceVertexIndex < 0) {
            return;
        }

        const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, selectedLineNumber);
        if (!lineFeature.has_value()
            || lineFeature->kind != MapGeometryFeature::Kind::Line
            || lineVertexIndexForSourceVertex(lineFeature.value(), selectedSourceVertexIndex) < 0) {
            *context_.toolbarStatusNote = tr("Orientation editing is available only for selected line anchor vertices.");
            context_.refreshToolbarSummary();
            context_.refreshObjectDetailsPanel();
            return;
        }
        if (selectedLineNumber > documentLines.size()) {
            return;
        }
        const TherionParsedLine parsedLine =
            TherionDocumentParser::parseLine(documentLines.at(selectedLineNumber - 1), selectedLineNumber);
        const bool orientationSupported = isOrientationSupportedForParsedLine(parsedLine, orientationApplicabilityByCommand());
        const bool leftSizeSupported = isLinePointLeftSizeSupportedForParsedLine(parsedLine);
        if (!orientationSupported && !leftSizeSupported) {
            *context_.toolbarStatusNote = tr("Orientation is not supported for this line type.");
            context_.refreshToolbarSummary();
            context_.refreshObjectDetailsPanel();
            return;
        }

        rewritten = true;
        if (orientationSupported) {
            rewritten = TherionDocumentEditor::rewriteLinePointOrientation(&afterText,
                                                                           selectedLineNumber,
                                                                           selectedSourceVertexIndex,
                                                                           enabled,
                                                                           orientation,
                                                                           &errorMessage);
        }
        if (rewritten && leftSizeSupported) {
            rewritten = TherionDocumentEditor::rewriteLinePointLeftSize(&afterText,
                                                                        selectedLineNumber,
                                                                        selectedSourceVertexIndex,
                                                                        leftSizeEnabled,
                                                                        leftSize,
                                                                        &errorMessage);
        }
    }

    if (!rewritten) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update orientation.")
            : tr("Failed to update orientation: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText != beforeText) {
        if (context_.applySourceTextChangeWithSnapshot) {
            context_.applySourceTextChangeWithSnapshot(tr("Edit Object Orientation"),
                                                       beforeText,
                                                       afterText,
                                                       selectedLineNumber);
        } else {
            const QScopedValueRollback<bool> commandGuard(*context_.commandApplyInProgress, true);
            context_.textEditor->replaceTextForCommand(afterText);
            context_.recordSourceTextSnapshot(tr("Edit Object Orientation"),
                                              beforeText,
                                              afterText,
                                              selectedLineNumber);
        }
    }

    if (selectedObjectKind == QStringLiteral("line")) {
        *context_.toolbarStatusNote = tr("Updated line point options.");
        if (context_.restoreLineAnchorSelectionLater) {
            context_.restoreLineAnchorSelectionLater(selectedLineNumber, selectedSourceVertexIndex);
            QTimer::singleShot(0,
                               context_.callbackContext,
                               [restoreLineAnchorSelectionLater = context_.restoreLineAnchorSelectionLater,
                                selectedLineNumber,
                                selectedSourceVertexIndex]() {
                restoreLineAnchorSelectionLater(selectedLineNumber, selectedSourceVertexIndex);
            });
        }
    } else {
        *context_.toolbarStatusNote = enabled
            ? tr("Updated orientation to %1 degrees.").arg(QString::number(orientation, 'f', 3))
            : tr("Cleared orientation override.");
        if (context_.restorePointSelectionLater) {
            context_.restorePointSelectionLater(selectedLineNumber);
            QTimer::singleShot(0,
                               context_.callbackContext,
                               [restorePointSelectionLater = context_.restorePointSelectionLater, selectedLineNumber]() {
                restorePointSelectionLater(selectedLineNumber);
            });
        }
    }
    context_.refreshToolbarSummary();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleObjectOrientationEnabledToggled(bool checked)
{
    if (*context_.updatingUi || context_.orientationSpin == nullptr) {
        return;
    }
    context_.orientationSpin->setEnabled(checked);
    applyObjectOrientationEdits();
}

void MapEditorObjectDetailsEditController::handleObjectOrientationValueChanged(double value)
{
    Q_UNUSED(value);
    if (*context_.updatingUi
        || context_.orientationEnabledCheck == nullptr
        || !context_.orientationEnabledCheck->isChecked()) {
        return;
    }
    scheduleObjectOrientationApply(context_);
}

void MapEditorObjectDetailsEditController::handleLinePointLeftSizeEnabledToggled(bool checked)
{
    if (*context_.updatingUi || context_.linePointLeftSizeSpin == nullptr) {
        return;
    }
    context_.linePointLeftSizeSpin->setEnabled(checked);
    if (checked && context_.linePointLeftSizeSpin->value() <= 0.0) {
        context_.linePointLeftSizeSpin->setValue(40.0);
    }
    applyObjectOrientationEdits();
}

void MapEditorObjectDetailsEditController::handleLinePointLeftSizeValueChanged(double value)
{
    Q_UNUSED(value);
    if (*context_.updatingUi
        || context_.linePointLeftSizeEnabledCheck == nullptr
        || !context_.linePointLeftSizeEnabledCheck->isChecked()) {
        return;
    }
    scheduleObjectOrientationApply(context_);
}

void MapEditorObjectDetailsEditController::deleteSelectedObjectFromSelection()
{
    if (context_.textEditor == nullptr) {
        return;
    }

    int targetLineNumber = *context_.selectedObjectLineNumber;
    if (targetLineNumber <= 0) {
        targetLineNumber = context_.textEditor->currentLineNumber();
    }
    if (targetLineNumber <= 0) {
        return;
    }

    const QVector<MapEditorAreaReference> areaReferences =
        mapEditorAreaReferencesForBorderLine(context_.textEditor->text(), targetLineNumber);
    if (!areaReferences.isEmpty()) {
        *context_.toolbarStatusNote = tr("This line is used as an area border. Delete the area instead.");
        context_.refreshToolbarSummary();
        return;
    }

    const QString beforeText = context_.textEditor->text();
    const MapEditorObjectDeletePlan deletePlan = planMapEditorObjectDelete(beforeText, targetLineNumber);
    if (!deletePlan.resolved || !deletePlan.changed) {
        *context_.toolbarStatusNote = deletePlan.errorMessage.isEmpty()
            ? tr("Object deletion failed.")
            : tr("Object deletion failed: %1").arg(deletePlan.errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    if (context_.applySourceTextChangeWithSnapshot) {
        context_.applySourceTextChangeWithSnapshot(tr("Delete Map Object"),
                                                   beforeText,
                                                   deletePlan.updatedText,
                                                   deletePlan.focusLineAfterDelete);
    } else {
        const QScopedValueRollback<bool> commandGuard(*context_.commandApplyInProgress, true);
        context_.textEditor->replaceTextForCommand(deletePlan.updatedText);
        context_.recordSourceTextSnapshot(tr("Delete Map Object"),
                                          beforeText,
                                          deletePlan.updatedText,
                                          deletePlan.focusLineAfterDelete);
    }
    for (int removedLineNumber : deletePlan.removedLineNumbers) {
        context_.hiddenInspectorObjectLines->remove(removedLineNumber);
    }
    *context_.lastInspectorClickedObjectLineNumber = 0;
    context_.clearInspectorObjectSelection();
    *context_.toolbarStatusNote = tr("Deleted selected object from source.");
    context_.refreshToolbarSummary();
}

void MapEditorObjectDetailsEditController::applyObjectQuickFieldEdits()
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || context_.quickTypeCombo == nullptr
        || context_.quickSubtypeCombo == nullptr
        || context_.quickIdentifierEdit == nullptr
        || context_.quickNameEdit == nullptr
        || context_.quickTextEdit == nullptr) {
        return;
    }

    if (context_.pendingInsertQuickFields && context_.setPendingInsertQuickFields) {
        std::optional<InspectorObjectQuickFields> pendingFields = context_.pendingInsertQuickFields();
        if (pendingFields.has_value() && !pendingFields->commandKind.trimmed().isEmpty()) {
            pendingFields->type = context_.quickTypeCombo->currentText();
            pendingFields->subtype = context_.quickSubtypeCombo->currentText();
            pendingFields->identifier = context_.quickIdentifierEdit->text();
            pendingFields->name = context_.quickNameEdit->text();
            pendingFields->text = context_.quickTextEdit->text();
            pendingFields->textVisible = context_.quickTextEdit->isVisible();
            context_.setPendingInsertQuickFields(pendingFields.value());
            context_.refreshObjectDetailsPanel();
            return;
        }
    }

    int targetLineNumber = *context_.selectedObjectLineNumber;
    if (targetLineNumber <= 0) {
        targetLineNumber = context_.textEditor->currentLineNumber();
    }
    if (targetLineNumber <= 0) {
        return;
    }

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewriteMapObjectQuickFields(&afterText,
                                                            targetLineNumber,
                                                            context_.quickTypeCombo->currentText(),
                                                            context_.quickSubtypeCombo->currentText(),
                                                            context_.quickIdentifierEdit->text(),
                                                            context_.quickNameEdit->text(),
                                                            context_.quickNameEdit->isVisible(),
                                                            &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update object fields.")
            : tr("Failed to update object fields: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    const bool currentTypeIsLabel =
        context_.quickTypeCombo->currentText().trimmed().compare(QStringLiteral("label"), Qt::CaseInsensitive) == 0;
    if (context_.quickTextEdit->isVisible()
        && !TherionDocumentEditor::rewriteMapObjectTextOption(&afterText,
                                                              targetLineNumber,
                                                              currentTypeIsLabel ? context_.quickTextEdit->text() : QString(),
                                                              &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update label text.")
            : tr("Failed to update label text: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (context_.applySourceTextChangeWithSnapshot) {
        context_.applySourceTextChangeWithSnapshot(tr("Edit Object Fields"), beforeText, afterText, targetLineNumber);
    } else {
        const QScopedValueRollback<bool> commandGuard(*context_.commandApplyInProgress, true);
        context_.textEditor->replaceTextForCommand(afterText);
        context_.recordSourceTextSnapshot(tr("Edit Object Fields"), beforeText, afterText, targetLineNumber);
    }
    *context_.toolbarStatusNote = tr("Updated object fields.");
    context_.refreshToolbarSummary();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::applyScrapProjectionEdit()
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || context_.quickProjectionCombo == nullptr) {
        return;
    }

    if (context_.pendingInsertQuickFields && context_.setPendingInsertQuickFields) {
        std::optional<InspectorObjectQuickFields> pendingFields = context_.pendingInsertQuickFields();
        if (pendingFields.has_value() && pendingFields->commandKind.trimmed().toLower() == QStringLiteral("scrap")) {
            pendingFields->projection = context_.quickProjectionCombo->currentText();
            pendingFields->identifier = context_.quickIdentifierEdit != nullptr ? context_.quickIdentifierEdit->text() : pendingFields->identifier;
            context_.setPendingInsertQuickFields(pendingFields.value());
            context_.refreshObjectDetailsPanel();
            return;
        }
    }

    int targetLineNumber = *context_.selectedObjectLineNumber;
    if (targetLineNumber <= 0) {
        targetLineNumber = context_.textEditor->currentLineNumber();
    }
    if (targetLineNumber <= 0) {
        return;
    }

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewriteScrapProjection(&afterText,
                                                       targetLineNumber,
                                                       context_.quickProjectionCombo->currentText(),
                                                       &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? tr("Failed to update scrap projection.")
            : tr("Failed to update scrap projection: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    if (context_.applySourceTextChangeWithSnapshot) {
        context_.applySourceTextChangeWithSnapshot(tr("Edit Scrap Projection"), beforeText, afterText, targetLineNumber);
    } else {
        const QScopedValueRollback<bool> commandGuard(*context_.commandApplyInProgress, true);
        context_.textEditor->replaceTextForCommand(afterText);
        context_.recordSourceTextSnapshot(tr("Edit Scrap Projection"), beforeText, afterText, targetLineNumber);
    }
    *context_.toolbarStatusNote = tr("Updated scrap projection.");
    context_.refreshToolbarSummary();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::updateObjectQuickSubtypeChoices()
{
    if (context_.quickSubtypeCombo == nullptr
        || context_.quickTypeCombo == nullptr
        || context_.objectQuickCommandKind == nullptr) {
        return;
    }

    const QString currentSubtype = context_.quickSubtypeCombo->currentText();
    const InspectorSymbolCatalog &catalog = inspectorSymbolCatalog();
    setEditableComboValues(context_.quickSubtypeCombo,
                           inspectorSubtypeValuesForCommandType(catalog,
                                                                *context_.objectQuickCommandKind,
                                                                context_.quickTypeCombo->currentText()),
                           currentSubtype);
}

void MapEditorObjectDetailsEditController::insertVertexBeforeFromSelectionPanel()
{
    context_.insertLineVertexFromSelection(true);
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::insertVertexAfterFromSelectionPanel()
{
    context_.insertLineVertexFromSelection(false);
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::splitLineFromSelectionPanel()
{
    context_.splitLineAtSelection();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::deleteVertexFromSelectionPanel()
{
    context_.removeLineVertexFromSelection();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleLinePointPreviousControlToggled(bool checked)
{
    if (*context_.updatingUi || !context_.setLineVertexControlHandleForSelection) {
        return;
    }
    context_.setLineVertexControlHandleForSelection(true, checked);
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleLinePointSmoothToggled(bool checked)
{
    if (*context_.updatingUi || !context_.setLineVertexSmoothForSelection) {
        return;
    }
    context_.setLineVertexSmoothForSelection(checked);
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::handleLinePointNextControlToggled(bool checked)
{
    if (*context_.updatingUi || !context_.setLineVertexControlHandleForSelection) {
        return;
    }
    context_.setLineVertexControlHandleForSelection(false, checked);
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::applyLinePointFlagsEdits()
{
    if (context_.textEditor == nullptr
        || context_.linePointFlagsEdit == nullptr
        || context_.toolbarStatusNote == nullptr
        || context_.refreshToolbarSummary == nullptr) {
        return;
    }
    const std::optional<std::pair<int, int>> lineAndVertex = selectedLineAndSourceVertex(context_);
    if (!lineAndVertex.has_value()) {
        return;
    }

    const int lineNumber = lineAndVertex->first;
    const int sourceVertexIndex = lineAndVertex->second;
    const QString beforeText = context_.textEditor->text();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, lineNumber);
    if (!lineFeature.has_value() || lineFeature->kind != MapGeometryFeature::Kind::Line) {
        (*context_.toolbarStatusNote) = tr("Edit line-point options failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return;
    }

    const int vertexIndex = lineVertexIndexForSourceVertex(lineFeature.value(), sourceVertexIndex);
    if (vertexIndex < 0 || vertexIndex >= lineFeature->lineVertices.size()) {
        (*context_.toolbarStatusNote) = tr("Edit line-point options failed: selected line point could not be resolved.");
        context_.refreshToolbarSummary();
        return;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    const QStringList updatedRows = trimmedLinePointStandaloneRows(context_.linePointFlagsEdit->toPlainText());
    if (editedVertices.at(vertexIndex).standaloneOptionRows == updatedRows) {
        return;
    }
    editedVertices[vertexIndex].standaloneOptionRows = updatedRows;
    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices, lineFeature->closed);

    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewriteLineCoordinateRows(&afterText, lineNumber, coordinateRows, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Edit line-point options failed.")
            : tr("Edit line-point options failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return;
    }
    if (afterText == beforeText) {
        return;
    }

    if (context_.applySourceTextChangeWithSnapshot) {
        context_.applySourceTextChangeWithSnapshot(tr("Edit Line Point Options"), beforeText, afterText, lineNumber);
    } else {
        const QScopedValueRollback<bool> commandGuard(*context_.commandApplyInProgress, true);
        context_.textEditor->replaceTextForCommand(afterText);
        context_.recordSourceTextSnapshot(tr("Edit Line Point Options"), beforeText, afterText, lineNumber);
    }

    (*context_.toolbarStatusNote) = updatedRows.isEmpty()
        ? tr("Cleared additional line-point options for line %1, point %2.").arg(lineNumber).arg(vertexIndex + 1)
        : tr("Updated additional line-point options for line %1, point %2.").arg(lineNumber).arg(vertexIndex + 1);
    context_.refreshToolbarSummary();
    if (context_.restoreLineAnchorSelectionLater != nullptr) {
        const int restoredVertex = editedVertices.at(vertexIndex).anchorSourceVertexIndex >= 0
            ? editedVertices.at(vertexIndex).anchorSourceVertexIndex
            : sourceVertexIndex;
        context_.restoreLineAnchorSelectionLater(lineNumber, restoredVertex);
    }
    context_.refreshObjectDetailsPanel();
}

}
