#include "TextEditorTab.h"

#include "TextEditorSourceRewriteController.h"

namespace TherionStudio
{
bool TextEditorTab::rewriteStructureEntryName(int lineNumber, const QString &category, const QString &newName, QString *errorMessage)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->rewriteStructureEntryName(lineNumber, category, newName, errorMessage);
}

bool TextEditorTab::insertScrapBlock(const QString &preferredName,
                                     int *insertedLineNumber,
                                     QString *errorMessage,
                                     const QString &options)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertScrapBlock(preferredName, insertedLineNumber, errorMessage, options);
}

bool TextEditorTab::insertDraftGeometry(const QString &kind,
                                        const QVector<QPointF> &vertices,
                                        int *insertedLineNumber,
                                        QString *errorMessage,
                                        const TherionDraftObjectOptions &objectOptions,
                                        const std::optional<QRectF> &initialAreaAdjustRect)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertDraftGeometry(kind,
                                                         vertices,
                                                         insertedLineNumber,
                                                         errorMessage,
                                                         objectOptions,
                                                         initialAreaAdjustRect);
}

bool TextEditorTab::insertDraftLineGeometry(const QStringList &coordinateRows,
                                            int *insertedLineNumber,
                                            QString *errorMessage,
                                            const QString &lineOptions,
                                            const TherionDraftObjectOptions &objectOptions,
                                            const std::optional<QRectF> &initialAreaAdjustRect)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertDraftLineGeometry(coordinateRows,
                                                             insertedLineNumber,
                                                             errorMessage,
                                                             lineOptions,
                                                             objectOptions,
                                                             initialAreaAdjustRect);
}

bool TextEditorTab::insertDraftAreaGeometry(const QStringList &coordinateRows,
                                            int *insertedLineNumber,
                                            QString *errorMessage,
                                            const TherionDraftObjectOptions &objectOptions,
                                            const std::optional<QRectF> &initialAreaAdjustRect)
{
    return sourceRewriteController_ != nullptr
        && sourceRewriteController_->insertDraftAreaGeometry(coordinateRows,
                                                             insertedLineNumber,
                                                             errorMessage,
                                                             objectOptions,
                                                             initialAreaAdjustRect);
}

bool TextEditorTab::configureCommandAtLine(const QString &kind, int lineNumber, bool showCommandHelpOnly)
{
    if (lineNumber <= 0 || editor_ == nullptr) {
        return false;
    }

    handleBlockConfigureRequest(kind, lineNumber, showCommandHelpOnly);
    return true;
}

bool TextEditorTab::deleteCommandAtLine(int lineNumber)
{
    return handleBlockDeleteRequest(lineNumber);
}

void TextEditorTab::replaceTextForCommand(const QString &contents)
{
    if (sourceRewriteController_ != nullptr) {
        sourceRewriteController_->replaceTextForCommand(contents);
    }
}

void TextEditorTab::replaceTextForSystemNormalization(const QString &contents)
{
    if (sourceRewriteController_ != nullptr) {
        sourceRewriteController_->replaceTextForSystemNormalization(contents);
    }
}
}
