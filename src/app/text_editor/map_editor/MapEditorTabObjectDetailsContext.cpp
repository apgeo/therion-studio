#include "MapEditorTab.h"

#include "MapEditorObjectDetailsContext.h"
#include "../TextEditorTab.h"

#include <QTabWidget>

namespace TherionStudio
{
namespace
{
QString normalizedPendingCommandKind(const QString &commandKind)
{
    const QString normalized = commandKind.trimmed().toLower();
    if (normalized == QStringLiteral("scrap")
        || normalized == QStringLiteral("point")
        || normalized == QStringLiteral("line")
        || normalized == QStringLiteral("area")) {
        return normalized;
    }
    return QString();
}

QString defaultPendingTypeForCommand(const QString &commandKind)
{
    if (commandKind == QStringLiteral("point")) {
        return QStringLiteral("station");
    }
    if (commandKind == QStringLiteral("line")) {
        return QStringLiteral("wall");
    }
    if (commandKind == QStringLiteral("area")) {
        return QStringLiteral("water");
    }
    return QString();
}

bool pendingPointNameVisible(const QString &type, const QString &name)
{
    return type.trimmed().compare(QStringLiteral("station"), Qt::CaseInsensitive) == 0
        || !name.trimmed().isEmpty();
}

bool pendingLabelTextVisible(const QString &commandKind, const QString &type, const QString &text)
{
    const QString normalizedCommand = commandKind.trimmed().toLower();
    return (normalizedCommand == QStringLiteral("point") || normalizedCommand == QStringLiteral("line"))
        && (type.trimmed().compare(QStringLiteral("label"), Qt::CaseInsensitive) == 0
            || !text.trimmed().isEmpty());
}
}

void MapEditorTab::beginPendingInsertObject(const QString &commandKind)
{
    const QString normalizedCommand = normalizedPendingCommandKind(commandKind);
    if (normalizedCommand.isEmpty()) {
        clearPendingInsertObject();
        return;
    }

    InspectorObjectQuickFields fields;
    fields.commandKind = normalizedCommand;
    fields.typeEditable = normalizedCommand != QStringLiteral("scrap");
    fields.type = defaultPendingTypeForCommand(normalizedCommand);
    if (normalizedCommand == QStringLiteral("scrap")) {
        fields.identifier = QStringLiteral("new-scrap");
    } else if (normalizedCommand == QStringLiteral("point")) {
        fields.name = QStringLiteral("draft-point");
        fields.nameVisible = true;
    }
    fields.textVisible = pendingLabelTextVisible(normalizedCommand, fields.type, fields.text);
    interactiveDrawState_.pendingInsertFields_ = fields;
}

void MapEditorTab::clearPendingInsertObject()
{
    interactiveDrawState_.pendingInsertFields_ = InspectorObjectQuickFields{};
}

std::optional<InspectorObjectQuickFields> MapEditorTab::pendingInsertQuickFields() const
{
    if (interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().isEmpty()) {
        return std::nullopt;
    }
    return interactiveDrawState_.pendingInsertFields_;
}

void MapEditorTab::setPendingInsertQuickFields(const InspectorObjectQuickFields &fields)
{
    const QString existingCommand = interactiveDrawState_.pendingInsertFields_.commandKind;
    const QString normalizedCommand = normalizedPendingCommandKind(fields.commandKind.isEmpty()
                                                                       ? existingCommand
                                                                       : fields.commandKind);
    if (normalizedCommand.isEmpty()) {
        clearPendingInsertObject();
        return;
    }

    interactiveDrawState_.pendingInsertFields_ = fields;
    interactiveDrawState_.pendingInsertFields_.commandKind = normalizedCommand;
    interactiveDrawState_.pendingInsertFields_.typeEditable = normalizedCommand != QStringLiteral("scrap");
    if (interactiveDrawState_.pendingInsertFields_.type.trimmed().isEmpty()) {
        interactiveDrawState_.pendingInsertFields_.type = defaultPendingTypeForCommand(normalizedCommand);
    }
    if (normalizedCommand == QStringLiteral("point")) {
        if (interactiveDrawState_.pendingInsertFields_.type.trimmed().compare(QStringLiteral("station"), Qt::CaseInsensitive) != 0
            && interactiveDrawState_.pendingInsertFields_.name.trimmed() == QStringLiteral("draft-point")) {
            interactiveDrawState_.pendingInsertFields_.name.clear();
        }
        interactiveDrawState_.pendingInsertFields_.nameVisible = pendingPointNameVisible(interactiveDrawState_.pendingInsertFields_.type,
                                                                   interactiveDrawState_.pendingInsertFields_.name);
    } else {
        interactiveDrawState_.pendingInsertFields_.nameVisible = false;
        interactiveDrawState_.pendingInsertFields_.name.clear();
    }
    interactiveDrawState_.pendingInsertFields_.textVisible =
        pendingLabelTextVisible(normalizedCommand,
                                interactiveDrawState_.pendingInsertFields_.type,
                                interactiveDrawState_.pendingInsertFields_.text);
}

TherionDraftObjectOptions MapEditorTab::pendingDraftObjectOptions(const QString &commandKind) const
{
    TherionDraftObjectOptions options;
    const QString normalizedCommand = normalizedPendingCommandKind(commandKind);
    if (normalizedCommand.isEmpty()
        || interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().toLower() != normalizedCommand) {
        return options;
    }

    options.type = interactiveDrawState_.pendingInsertFields_.type;
    options.subtype = interactiveDrawState_.pendingInsertFields_.subtype;
    options.identifier = interactiveDrawState_.pendingInsertFields_.identifier;
    if (normalizedCommand == QStringLiteral("point")) {
        options.name = interactiveDrawState_.pendingInsertFields_.name;
        options.nameEnabled = interactiveDrawState_.pendingInsertFields_.nameVisible;
    }
    options.text = interactiveDrawState_.pendingInsertFields_.text;
    options.textEnabled = interactiveDrawState_.pendingInsertFields_.textVisible;
    return options;
}

QString MapEditorTab::pendingScrapPreferredName() const
{
    if (interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().toLower() != QStringLiteral("scrap")) {
        return QStringLiteral("new-scrap");
    }

    const QString identifier = interactiveDrawState_.pendingInsertFields_.identifier.trimmed();
    return identifier.isEmpty() ? QStringLiteral("new-scrap") : identifier;
}

QString MapEditorTab::pendingScrapOptions(const QString &scaleOption) const
{
    QStringList options;
    if (interactiveDrawState_.pendingInsertFields_.commandKind.trimmed().toLower() == QStringLiteral("scrap")) {
        const QString projection = interactiveDrawState_.pendingInsertFields_.projection.trimmed();
        if (!projection.isEmpty()) {
            options.append(QStringLiteral("-projection %1").arg(projection));
        }
    }
    if (!scaleOption.trimmed().isEmpty()) {
        options.append(scaleOption.trimmed());
    }
    return options.join(QLatin1Char(' '));
}

void MapEditorTab::activateSelectionInspector()
{
    if (mapInspectorTabs_ != nullptr) {
        mapInspectorTabs_->setCurrentIndex(0);
    }
}

MapEditorObjectDetailsContext MapEditorTab::objectDetailsContext()
{
    return MapEditorObjectDetailsContext{
        .callbackContext = this,
        .textEditor = textEditor_,
        .inspectorSymbolCatalog = &inspectorSymbolCatalog_,
        .orientationApplicabilityByCommand = &orientationApplicabilityByCommand_,
        .updatingUi = &objectDetailsUiState_.updatingObjectDetailsUi_,
        .commandApplyInProgress = &mapCommandApplyInProgress_,
        .selectedObjectLineNumber = &objectSelectionState_.selectedObjectLineNumber_,
        .selectedObjectVertexIndex = &objectSelectionState_.selectedObjectVertexIndex_,
        .selectedObjectKind = &objectSelectionState_.selectedObjectKind_,
        .objectQuickCommandKind = &objectDetailsUiState_.objectQuickCommandKind_,
        .hiddenInspectorObjectLines = &hiddenInspectorObjectLines_,
        .lastInspectorClickedObjectLineNumber = &lastInspectorClickedObjectLineNumber_,
        .toolbarStatusNote = &toolbarStatusNote_,
        .selectionLabel = objectDetailsUiState_.objectDetailsSelectionLabel_,
        .selectionSection = objectDetailsUiState_.objectSelectionSection_,
        .selectionTitleLabel = objectDetailsUiState_.objectSelectionTitleLabel_,
        .vertexSection = objectDetailsUiState_.vertexSelectionSection_,
        .vertexTitleLabel = objectDetailsUiState_.vertexSelectionTitleLabel_,
        .geometrySection = objectDetailsUiState_.geometrySelectionSection_,
        .advancedSection = objectDetailsUiState_.advancedSelectionSection_,
        .deleteButton = objectDetailsUiState_.objectDeleteButton_,
        .areaReferenceLabel = objectDetailsUiState_.objectAreaReferenceLabel_,
        .quickFieldsEditor = objectDetailsUiState_.objectQuickFieldsEditor_,
        .quickIdentifierLabel = objectDetailsUiState_.objectQuickIdentifierLabel_,
        .quickNameLabel = objectDetailsUiState_.objectQuickNameLabel_,
        .quickTextLabel = objectDetailsUiState_.objectQuickTextLabel_,
        .quickProjectionLabel = objectDetailsUiState_.objectQuickProjectionLabel_,
        .quickTypeLabel = objectDetailsUiState_.objectQuickTypeLabel_,
        .quickSubtypeLabel = objectDetailsUiState_.objectQuickSubtypeLabel_,
        .stylePreviewLabel = objectDetailsUiState_.objectStylePreviewLabel_,
        .quickTypeCombo = objectDetailsUiState_.objectQuickTypeCombo_,
        .quickSubtypeCombo = objectDetailsUiState_.objectQuickSubtypeCombo_,
        .quickProjectionCombo = objectDetailsUiState_.objectQuickProjectionCombo_,
        .quickIdentifierEdit = objectDetailsUiState_.objectQuickIdentifierEdit_,
        .quickNameEdit = objectDetailsUiState_.objectQuickNameEdit_,
        .quickTextEdit = objectDetailsUiState_.objectQuickTextEdit_,
        .stylePreview = objectDetailsUiState_.objectStylePreview_,
        .vertexActionsEditor = objectDetailsUiState_.vertexActionsEditor_,
        .vertexInsertBeforeButton = objectDetailsUiState_.vertexInsertBeforeButton_,
        .vertexInsertAfterButton = objectDetailsUiState_.vertexInsertAfterButton_,
        .vertexDeleteButton = objectDetailsUiState_.vertexDeleteButton_,
        .vertexSplitButton = objectDetailsUiState_.vertexSplitButton_,
        .metadataLabel = objectDetailsUiState_.objectDetailsMetadataLabel_,
        .orientationEditor = objectDetailsUiState_.objectOrientationEditor_,
        .linePointPreviousControlCheck = objectDetailsUiState_.linePointPreviousControlCheck_,
        .linePointSmoothCheck = objectDetailsUiState_.linePointSmoothCheck_,
        .linePointNextControlCheck = objectDetailsUiState_.linePointNextControlCheck_,
        .orientationEnabledCheck = objectDetailsUiState_.objectOrientationEnabledCheck_,
        .orientationSpin = objectDetailsUiState_.objectOrientationSpin_,
        .linePointLeftSizeEnabledCheck = objectDetailsUiState_.linePointLeftSizeEnabledCheck_,
        .linePointLeftSizeSpin = objectDetailsUiState_.linePointLeftSizeSpin_,
        .lineOptionsEditor = objectDetailsUiState_.lineOptionsEditor_,
        .lineClosedCheck = objectDetailsUiState_.lineClosedCheck_,
        .lineReversedCheck = objectDetailsUiState_.lineReversedCheck_,
        .scrapScaleEditor = objectDetailsUiState_.scrapScaleEditor_,
        .scrapScaleSourceX1Spin = objectDetailsUiState_.scrapScaleSourceX1Spin_,
        .scrapScaleSourceY1Spin = objectDetailsUiState_.scrapScaleSourceY1Spin_,
        .scrapScaleSourceX2Spin = objectDetailsUiState_.scrapScaleSourceX2Spin_,
        .scrapScaleSourceY2Spin = objectDetailsUiState_.scrapScaleSourceY2Spin_,
        .scrapScaleRealX1Spin = objectDetailsUiState_.scrapScaleRealX1Spin_,
        .scrapScaleRealY1Spin = objectDetailsUiState_.scrapScaleRealY1Spin_,
        .scrapScaleRealX2Spin = objectDetailsUiState_.scrapScaleRealX2Spin_,
        .scrapScaleRealY2Spin = objectDetailsUiState_.scrapScaleRealY2Spin_,
        .scrapScaleUnitCombo = objectDetailsUiState_.scrapScaleUnitCombo_,
        .configureButton = objectDetailsUiState_.objectConfigureButton_,
        .translate = [this](const char *text) {
            return tr(text);
        },
        .mapSourceBoundsForCurrentDocument = [this]() {
            return mapSourceBoundsForCurrentDocument();
        },
        .refreshToolbarSummary = [this]() {
            refreshToolbarSummary();
        },
        .refreshObjectDetailsPanel = [this]() {
            refreshObjectDetailsPanel();
        },
        .pendingInsertQuickFields = [this]() -> std::optional<InspectorObjectQuickFields> {
            return pendingInsertQuickFields();
        },
        .setPendingInsertQuickFields = [this](const InspectorObjectQuickFields &fields) {
            setPendingInsertQuickFields(fields);
        },
        .clearInspectorObjectSelection = [this]() {
            clearInspectorObjectSelection();
        },
        .selectMapLine = [this](int lineNumber, bool centerOnSelection) {
            selectMapLine(lineNumber, centerOnSelection);
            syncInspectorObjectSelectionToLine(lineNumber);
        },
        .restorePointSelectionLater = [this](int lineNumber) {
            restorePointSelection(lineNumber);
        },
        .restoreLineAnchorSelectionLater = [this](int lineNumber, int sourceVertexIndex) {
            restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
        },
        .rewriteLineOptionToggle = [this](int lineNumber, const QString &optionName, bool enabled, QString *errorMessage) {
            return rewriteLineOptionToggle(lineNumber, optionName, enabled, errorMessage);
        },
        .recordSourceTextSnapshot = [this](const QString &label, const QString &beforeText, const QString &afterText, int insertedLineNumber) {
            recordSourceTextSnapshot(label, beforeText, afterText, insertedLineNumber);
        },
        .applySourceTextChangeWithSnapshot = [this](const QString &label,
                                                    const QString &beforeText,
                                                    const QString &afterText,
                                                    int insertedLineNumber) {
            applySourceTextChangeWithSnapshot(label, beforeText, afterText, insertedLineNumber);
        },
        .insertLineVertexFromSelection = [this](bool before) {
            return insertLineVertexFromSelection(before);
        },
        .splitLineAtSelection = [this]() {
            return splitLineAtSelection();
        },
        .removeLineVertexFromSelection = [this]() {
            return removeLineVertexFromSelection();
        },
        .toggleLineVertexSmoothFromSelection = [this]() {
            return toggleLineVertexSmoothFromSelection();
        },
        .setLineVertexSmoothForSelection = [this](bool smooth) {
            return setLineVertexSmoothForSelection(smooth);
        },
        .setLineVertexControlHandleForSelection = [this](bool incoming, bool enabled) {
            return setLineVertexControlHandleForSelection(incoming, enabled);
        },
    };
}
}
