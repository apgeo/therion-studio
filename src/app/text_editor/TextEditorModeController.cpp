#include "TextEditorModeController.h"

#include "TextEditorTab.h"

#include "block_editor/BlockEditorDirectiveRules.h"
#include "block_editor/BlockEditorSourceText.h"

#include "../../core/TherionDocumentParser.h"

#include <QFileInfo>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

TextEditorModeController::TextEditorModeController(TextEditorTab *owner)
    : owner_(owner)
{
}

bool TextEditorModeController::isBlocksModeSupportedForCurrentFile() const
{
    if (owner_ == nullptr || owner_->filePath_.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(owner_->filePath_);
    const QString suffix = fileInfo.suffix().trimmed().toLower();
    const QString fileName = fileInfo.fileName().trimmed().toLower();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("thconfig")
        || fileName == QStringLiteral("thconfig");
}

void TextEditorModeController::refreshBlocksModeAvailability()
{
    if (owner_ == nullptr) {
        return;
    }

    const bool supported = isBlocksModeSupportedForCurrentFile();
    if (owner_->blocksModeButton_ != nullptr) {
        owner_->blocksModeButton_->setEnabled(supported);
    }
    if (!supported) {
        owner_->blocksModeActive_ = false;
    }
}

void TextEditorModeController::setBlocksModeActive(bool active)
{
    if (owner_ == nullptr) {
        return;
    }

    const bool targetActive = active && isBlocksModeSupportedForCurrentFile();
    if (owner_->blocksModeActive_ == targetActive) {
        refreshEditorModeUi();
        return;
    }

    owner_->blocksModeActive_ = targetActive;
    if (owner_->blocksModeActive_) {
        owner_->hideFindBar();
        if (!ensureEncodingRootDirectiveForBlocks()) {
            owner_->rebuildBlocksCanvasFromText();
        }
        owner_->populateBlockToolbox();
    } else if (owner_->editor_ != nullptr) {
        owner_->editor_->setFocus();
    }
    refreshEditorModeUi();
    emit owner_->editorModeChanged(owner_->editorMode());
}

bool TextEditorModeController::ensureEncodingRootDirectiveForBlocks()
{
    if (owner_ == nullptr || owner_->editor_ == nullptr || owner_->enforcingEncodingRootDirective_) {
        return false;
    }

    const QString contents = owner_->editor_->toPlainText();
    QStringList lines = blockEditorNormalizedSourceLines(contents);
    if (lines.size() == 1 && lines.first().isEmpty()) {
        lines.clear();
    }

    QString encodingName = owner_->fileEncodingName_.trimmed();
    if (encodingName.isEmpty()) {
        encodingName = QStringLiteral("utf-8");
    }
    const QString desiredEncodingLine =
        QStringLiteral("encoding %1").arg(encodingName.toLower());

    QStringList normalizedLines;
    normalizedLines.reserve(lines.size() + 1);
    normalizedLines.append(desiredEncodingLine);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        if (isEncodingDirective(parsedLine.directive)) {
            continue;
        }
        normalizedLines.append(lines.at(lineIndex));
    }

    const QString normalizedContents = blockEditorJoinSourceLines(contents, normalizedLines);
    if (normalizedContents == blockEditorJoinSourceLines(contents, lines)) {
        return false;
    }

    owner_->enforcingEncodingRootDirective_ = true;
    owner_->replaceTextForCommand(normalizedContents);
    owner_->enforcingEncodingRootDirective_ = false;
    return true;
}

void TextEditorModeController::refreshEditorModeUi()
{
    if (owner_ == nullptr
        || owner_->rawModeButton_ == nullptr
        || owner_->blocksModeButton_ == nullptr
        || owner_->editorModeStack_ == nullptr) {
        return;
    }

    owner_->rawModeButton_->setChecked(!owner_->blocksModeActive_);
    owner_->blocksModeButton_->setChecked(owner_->blocksModeActive_);
    if (owner_->blocksModeActive_) {
        owner_->editorModeStack_->setCurrentWidget(owner_->blocksPanel_);
    } else if (owner_->rawEditorPanel_ != nullptr) {
        owner_->editorModeStack_->setCurrentWidget(owner_->rawEditorPanel_);
    }
}
}
