#pragma once

#include <QString>
#include <QStringList>

class QLineEdit;
class QWidget;

namespace TherionStudio
{
class BlockEditorTokenTagEditor;

struct BlockEditorDataHeaderComponents
{
    QString style;
    QString readingsOrder;
};

BlockEditorTokenTagEditor *blockEditorTokenTagEditor(QWidget *widget);
void installBlockEditorLineEditCompleter(QLineEdit *lineEdit, const QStringList &values);
bool isRequiredBlockArgumentSignature(const QString &signature);
QString blockArgumentSignatureFromHelpLine(const QString &argumentLine);
QString blockArgumentLabelFromSignature(const QString &signature);
BlockEditorDataHeaderComponents parseBlockEditorDataHeaderComponents(const QStringList &tokens);
}
