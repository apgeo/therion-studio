#include "BlockEditorApplyStateController.h"

#include "../TextEditorTab.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

namespace TherionStudio
{
BlockEditorApplyStateController::BlockEditorApplyStateController(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorApplyStateController::refreshApplyState()
{
    if (owner_ == nullptr
        || owner_->tearingDown_
        || owner_->blockDetailsPopulating_
        || owner_->blockDetailsApplyButton_ == nullptr
        || owner_->editor_ == nullptr) {
        return;
    }

    QString validationError;
    QString candidateLine;
    const bool buildOk = owner_->buildUpdatedLineFromBlockDetails(&candidateLine, &validationError);

    QString currentLine;
    bool hasCurrentLine = false;
    if (owner_->blockDetailsSelectedLineNumber_ > 0) {
        QStringList lines = owner_->editor_->toPlainText().split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        for (QString &line : lines) {
            if (line.endsWith(QLatin1Char('\r'))) {
                line.chop(1);
            }
        }
        if (owner_->blockDetailsSelectedLineNumber_ <= lines.size()) {
            currentLine = lines.at(owner_->blockDetailsSelectedLineNumber_ - 1);
            hasCurrentLine = true;
        }
    }

    const bool hasChanges = buildOk && hasCurrentLine && candidateLine != currentLine;
    owner_->blockDetailsApplyButton_->setEnabled(hasChanges);

    if (owner_->blockDetailsStatusLabel_ == nullptr) {
        return;
    }
    if (!validationError.isEmpty()) {
        owner_->blockDetailsStatusLabel_->setStyleSheet(QStringLiteral("color: #c0392b;"));
        owner_->blockDetailsStatusLabel_->setText(
            QStringLiteral("%1 — %2").arg(owner_->blockDetailsBaseStatusText_, validationError));
        return;
    }
    owner_->blockDetailsStatusLabel_->setStyleSheet(QString());
    owner_->blockDetailsStatusLabel_->setText(owner_->blockDetailsBaseStatusText_);
}
}
