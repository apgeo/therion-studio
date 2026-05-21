#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
struct TextEditorOptionRow
{
    QString key;
    QString value;
    int rowNumber = 0;
};

struct TextEditorOptionValidationResult
{
    bool ok = false;
    int failingRow = -1;
    QString errorMessage;
    QStringList serializedOptions;
};

TextEditorOptionValidationResult validateAndSerializeCommandOptions(
    const QString &commandToken,
    const QVector<TextEditorOptionRow> &rows,
    const QHash<QString, QString> &commandOptionValueArityTokens,
    const QHash<QString, int> &commandOptionFixedArityByKey,
    const QHash<QString, QStringList> &commandOptionArgumentLabelsByKey,
    const QHash<QString, QStringList> &commandOptionValueTokens,
    bool validateAllowedValues);
}
