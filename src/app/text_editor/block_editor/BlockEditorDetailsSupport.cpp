#include "BlockEditorDetailsSupport.h"

#include "BlockEditorTokenTagEditor.h"

#include <QCompleter>
#include <QLineEdit>
#include <QObject>

#include <algorithm>

namespace TherionStudio
{
BlockEditorTokenTagEditor *blockEditorTokenTagEditor(QWidget *widget)
{
    return dynamic_cast<BlockEditorTokenTagEditor *>(widget);
}

void installBlockEditorLineEditCompleter(QLineEdit *lineEdit, const QStringList &values)
{
    if (lineEdit == nullptr) {
        return;
    }

    QStringList suggestions;
    for (const QString &value : values) {
        const QString normalized = value.trimmed();
        if (!normalized.isEmpty()) {
            suggestions.append(normalized);
        }
    }
    suggestions.removeDuplicates();
    std::sort(suggestions.begin(), suggestions.end(), [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });

    if (suggestions.isEmpty()) {
        lineEdit->setCompleter(nullptr);
        return;
    }

    auto *completer = new QCompleter(suggestions, lineEdit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    lineEdit->setCompleter(completer);
}

bool isRequiredBlockArgumentSignature(const QString &signature)
{
    const QString trimmed = signature.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    if (trimmed.startsWith(QLatin1Char('['))) {
        return false;
    }
    return trimmed.contains(QLatin1Char('<')) && trimmed.contains(QLatin1Char('>'));
}

QString blockArgumentSignatureFromHelpLine(const QString &argumentLine)
{
    const int equalsIndex = argumentLine.indexOf(QLatin1Char('='));
    if (equalsIndex >= 0) {
        return argumentLine.left(equalsIndex).trimmed();
    }
    return argumentLine.trimmed();
}

QString blockArgumentLabelFromSignature(const QString &signature)
{
    QString label = signature.trimmed();
    label.remove(QLatin1Char('|'));
    label.remove(QLatin1Char('['));
    label.remove(QLatin1Char(']'));
    label.remove(QLatin1Char('<'));
    label.remove(QLatin1Char('>'));
    label.replace(QLatin1Char('_'), QLatin1Char(' '));
    label.replace(QLatin1Char('-'), QLatin1Char(' '));
    label = label.simplified().trimmed();
    if (label.isEmpty()) {
        return QObject::tr("Value");
    }
    label = label.toLower();
    label[0] = label.at(0).toUpper();
    return label;
}

BlockEditorDataHeaderComponents parseBlockEditorDataHeaderComponents(const QStringList &tokens)
{
    BlockEditorDataHeaderComponents components;
    if (tokens.size() <= 1) {
        return components;
    }

    components.style = tokens.at(1).trimmed();
    if (tokens.size() > 2) {
        components.readingsOrder = tokens.mid(2).join(QLatin1Char(' ')).trimmed();
    }
    return components;
}
}
