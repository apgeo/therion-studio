#include "MapEditorObjectDetailsEditController.h"

#include "../TextEditorTab.h"
#include "MapEditorInspectorData.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../../../core/TherionDocumentEditor.h"
#include "../../../core/TherionDocumentParser.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLineF>
#include <QScopedValueRollback>

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

MapEditorObjectDetailsEditController::MapEditorObjectDetailsEditController(MapEditorObjectDetailsContext context)
    : context_(std::move(context))
{
}

QString MapEditorObjectDetailsEditController::translate(const char *text) const
{
    return context_.translate ? context_.translate(text) : QString::fromUtf8(text);
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
        *context_.toolbarStatusNote = translate("Select a scrap to edit its scale.");
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
        *context_.toolbarStatusNote = translate("Scrap scale requires two distinct picture points and two distinct real points.");
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
            ? translate("Failed to update scrap scale.")
            : translate("Failed to update scrap scale: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    const QScopedValueRollback<bool> commandGuard(*context_.commandApplyInProgress, true);
    context_.textEditor->replaceTextForCommand(afterText);
    context_.recordSourceTextSnapshot(translate("Set Scrap Scale"), beforeText, afterText, targetLineNumber);
    *context_.toolbarStatusNote = translate("Updated scrap scale.");
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
        *context_.toolbarStatusNote = translate("Object settings are available for scrap, point, line, and area commands.");
        context_.refreshToolbarSummary();
        return;
    }

    context_.textEditor->configureCommandAtLine(targetKind, targetLineNumber);
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

    QString errorMessage;
    if (!context_.rewriteLineOptionToggle(*context_.selectedObjectLineNumber, QStringLiteral("close"), checked, &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? translate("Failed to update line closed state.")
            : translate("Failed to update line closed state: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
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

    QString errorMessage;
    if (!context_.rewriteLineOptionToggle(*context_.selectedObjectLineNumber, QStringLiteral("reverse"), checked, &errorMessage)) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? translate("Failed to update line reverse state.")
            : translate("Failed to update line reverse state: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
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
    if (*context_.selectedObjectKind != QStringLiteral("point") && *context_.selectedObjectKind != QStringLiteral("line")) {
        return;
    }

    const bool enabled = context_.orientationEnabledCheck->isChecked();
    const qreal orientation = normalizeOrientationDegreesForMapDetails(context_.orientationSpin->value());
    const bool leftSizeEnabled = context_.linePointLeftSizeEnabledCheck->isVisible()
        && context_.linePointLeftSizeEnabledCheck->isChecked();
    const qreal leftSize = qMax<qreal>(0.1, context_.linePointLeftSizeSpin->value());
    QStringList documentLines = context_.textEditor->text().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : documentLines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    QString errorMessage;
    bool rewritten = false;
    if (*context_.selectedObjectKind == QStringLiteral("point")) {
        if (*context_.selectedObjectLineNumber > documentLines.size()) {
            return;
        }
        const TherionParsedLine parsedLine =
            TherionDocumentParser::parseLine(documentLines.at(*context_.selectedObjectLineNumber - 1), *context_.selectedObjectLineNumber);
        if (!isOrientationSupportedForParsedLine(parsedLine)) {
            *context_.toolbarStatusNote = translate("Orientation is not supported for this point type.");
            context_.refreshToolbarSummary();
            context_.refreshObjectDetailsPanel();
            return;
        }
        rewritten = context_.textEditor->rewritePointOrientation(*context_.selectedObjectLineNumber,
                                                         enabled,
                                                         orientation,
                                                         &errorMessage);
    } else {
        if (*context_.selectedObjectVertexIndex < 0) {
            return;
        }

        const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context_.textEditor->text(), *context_.selectedObjectLineNumber);
        if (!lineFeature.has_value()
            || lineFeature->kind != MapGeometryFeature::Kind::Line
            || lineVertexIndexForSourceVertex(lineFeature.value(), *context_.selectedObjectVertexIndex) < 0) {
            *context_.toolbarStatusNote = translate("Orientation editing is available only for selected line anchor vertices.");
            context_.refreshToolbarSummary();
            context_.refreshObjectDetailsPanel();
            return;
        }
        if (*context_.selectedObjectLineNumber > documentLines.size()) {
            return;
        }
        const TherionParsedLine parsedLine =
            TherionDocumentParser::parseLine(documentLines.at(*context_.selectedObjectLineNumber - 1), *context_.selectedObjectLineNumber);
        const bool orientationSupported = isOrientationSupportedForParsedLine(parsedLine);
        const bool leftSizeSupported = isLinePointLeftSizeSupportedForParsedLine(parsedLine);
        if (!orientationSupported && !leftSizeSupported) {
            *context_.toolbarStatusNote = translate("Orientation is not supported for this line type.");
            context_.refreshToolbarSummary();
            context_.refreshObjectDetailsPanel();
            return;
        }

        rewritten = true;
        if (orientationSupported) {
            rewritten = context_.textEditor->rewriteLinePointOrientation(*context_.selectedObjectLineNumber,
                                                                 *context_.selectedObjectVertexIndex,
                                                                 enabled,
                                                                 orientation,
                                                                 &errorMessage);
        }
        if (rewritten && leftSizeSupported) {
            rewritten = context_.textEditor->rewriteLinePointLeftSize(*context_.selectedObjectLineNumber,
                                                             *context_.selectedObjectVertexIndex,
                                                             leftSizeEnabled,
                                                             leftSize,
                                                             &errorMessage);
        }
    }

    if (!rewritten) {
        *context_.toolbarStatusNote = errorMessage.isEmpty()
            ? translate("Failed to update orientation.")
            : translate("Failed to update orientation: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (*context_.selectedObjectKind == QStringLiteral("line")) {
        *context_.toolbarStatusNote = translate("Updated line point options.");
    } else {
        *context_.toolbarStatusNote = enabled
            ? translate("Updated orientation to %1 degrees.").arg(QString::number(orientation, 'f', 3))
            : translate("Cleared orientation override.");
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

    if (context_.textEditor->deleteCommandAtLine(targetLineNumber)) {
        context_.hiddenInspectorObjectLines->remove(targetLineNumber);
        *context_.lastInspectorClickedObjectLineNumber = 0;
        context_.clearInspectorObjectSelection();
        *context_.toolbarStatusNote = translate("Deleted selected object from source.");
        context_.refreshToolbarSummary();
    }
}

void MapEditorObjectDetailsEditController::applyObjectQuickFieldEdits()
{
    if (*context_.updatingUi
        || context_.textEditor == nullptr
        || context_.quickTypeCombo == nullptr
        || context_.quickSubtypeCombo == nullptr
        || context_.quickIdentifierEdit == nullptr
        || context_.quickNameEdit == nullptr) {
        return;
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
            ? translate("Failed to update object fields.")
            : translate("Failed to update object fields: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    const QScopedValueRollback<bool> commandGuard(*context_.commandApplyInProgress, true);
    context_.textEditor->replaceTextForCommand(afterText);
    context_.recordSourceTextSnapshot(translate("Edit Object Fields"), beforeText, afterText, targetLineNumber);
    *context_.toolbarStatusNote = translate("Updated object fields.");
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
            ? translate("Failed to update scrap projection.")
            : translate("Failed to update scrap projection: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        context_.refreshObjectDetailsPanel();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    const QScopedValueRollback<bool> commandGuard(*context_.commandApplyInProgress, true);
    context_.textEditor->replaceTextForCommand(afterText);
    context_.recordSourceTextSnapshot(translate("Edit Scrap Projection"), beforeText, afterText, targetLineNumber);
    *context_.toolbarStatusNote = translate("Updated scrap projection.");
    context_.refreshToolbarSummary();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::updateObjectQuickSubtypeChoices()
{
    if (context_.quickSubtypeCombo == nullptr || context_.quickTypeCombo == nullptr) {
        return;
    }

    const QString currentSubtype = context_.quickSubtypeCombo->currentText();
    setEditableComboValues(context_.quickSubtypeCombo,
                           inspectorSubtypeValuesForCommandType(*context_.objectQuickCommandKind, context_.quickTypeCombo->currentText()),
                           currentSubtype);
}

void MapEditorObjectDetailsEditController::insertVertexFromSelectionPanel()
{
    context_.insertLineVertexFromSelection();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::deleteVertexFromSelectionPanel()
{
    context_.removeLineVertexFromSelection();
    context_.refreshObjectDetailsPanel();
}

void MapEditorObjectDetailsEditController::toggleVertexSmoothFromSelectionPanel()
{
    context_.toggleLineVertexSmoothFromSelection();
    context_.refreshObjectDetailsPanel();
}
}
