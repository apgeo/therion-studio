#include "TextEditorOptionValidation.h"

#include "../../core/TherionCommandSyntax.h"

#include <QObject>

namespace
{
QString rowSuffix(int rowNumber)
{
    if (rowNumber <= 0) {
        return QString();
    }
    return QObject::tr(" in row %1").arg(rowNumber);
}
}

namespace TherionStudio
{
TextEditorOptionValidationResult validateAndSerializeCommandOptions(
    const QString &commandToken,
    const QVector<TextEditorOptionRow> &rows,
    const QHash<QString, QString> &commandOptionValueArityTokens,
    const QHash<QString, int> &commandOptionFixedArityByKey,
    const QHash<QString, QStringList> &commandOptionArgumentLabelsByKey,
    const QHash<QString, QStringList> &commandOptionValueTokens,
    bool validateAllowedValues)
{
    TextEditorOptionValidationResult result;
    result.ok = false;

    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const TextEditorOptionRow &row = rows.at(rowIndex);
        const QString key = row.key.trimmed();
        const QString value = row.value.trimmed();
        if (key.isEmpty()) {
            continue;
        }

        if (!key.startsWith(QLatin1Char('-'))) {
            result.failingRow = rowIndex;
            result.errorMessage = QObject::tr("Option key `%1` must start with '-'%2.")
                                      .arg(key, rowSuffix(row.rowNumber));
            return result;
        }

        const QString normalizedKey = key.toLower();
        const QString arity = commandOptionValueArityTokens.value(commandOptionValueKey(commandToken, normalizedKey));
        const bool arityRequiresValue = optionArityRequiresValue(arity);
        const bool arityForbidsValue = optionArityForbidsValue(arity);
        if (arityRequiresValue && value.isEmpty()) {
            result.failingRow = rowIndex;
            result.errorMessage = QObject::tr("Option `%1`%2 requires a value.")
                                      .arg(key, rowSuffix(row.rowNumber));
            return result;
        }
        if (arityForbidsValue && !value.isEmpty()) {
            result.failingRow = rowIndex;
            result.errorMessage = QObject::tr("Option `%1`%2 does not accept a value.")
                                      .arg(key, rowSuffix(row.rowNumber));
            return result;
        }

        const QString optionKey = commandOptionValueKey(commandToken, normalizedKey);
        const int fixedArity = commandOptionFixedArityByKey.value(optionKey, -1);
        const QStringList providedValues = parseOptionValuesFromEditor(value, arity, fixedArity);

        if (fixedArity >= 0 && providedValues.size() != fixedArity) {
            result.failingRow = rowIndex;
            const QStringList labels = commandOptionArgumentLabelsByKey.value(optionKey);
            if (!labels.isEmpty()) {
                result.errorMessage = QObject::tr("Option `%1`%2 requires exactly %3 value(s): %4.")
                                          .arg(key)
                                          .arg(rowSuffix(row.rowNumber))
                                          .arg(fixedArity)
                                          .arg(labels.join(QStringLiteral(", ")));
            } else {
                result.errorMessage = QObject::tr("Option `%1`%2 requires exactly %3 value(s).")
                                          .arg(key)
                                          .arg(rowSuffix(row.rowNumber))
                                          .arg(fixedArity);
            }
            return result;
        }

        const QString canonicalArity = canonicalOptionArityToken(arity);
        if (canonicalArity == QStringLiteral("EXACTLY_ONE")
            && !providedValues.isEmpty()
            && providedValues.size() != 1) {
            result.failingRow = rowIndex;
            result.errorMessage = QObject::tr("Option `%1`%2 requires exactly one value.")
                                      .arg(key, rowSuffix(row.rowNumber));
            return result;
        }
        if (canonicalArity == QStringLiteral("ONE_OR_MORE")
            && providedValues.isEmpty()) {
            result.failingRow = rowIndex;
            result.errorMessage = QObject::tr("Option `%1`%2 requires at least one value.")
                                      .arg(key, rowSuffix(row.rowNumber));
            return result;
        }

        const QStringList allowedValues = commandOptionValueTokens.value(commandOptionValueKey(commandToken, normalizedKey));
        if (validateAllowedValues && !allowedValues.isEmpty() && !value.isEmpty()) {
            for (const QString &providedValue : providedValues) {
                if (allowedValues.contains(providedValue, Qt::CaseInsensitive)) {
                    continue;
                }
                result.failingRow = rowIndex;
                result.errorMessage = QObject::tr("Option `%1` value `%2` is not allowed. Allowed values: %3.")
                                          .arg(key, providedValue, allowedValues.join(QStringLiteral(", ")));
                return result;
            }
        }

        result.serializedOptions.append(key);
        if (!providedValues.isEmpty()) {
            QStringList serializedValues;
            serializedValues.reserve(providedValues.size());
            for (const QString &providedValue : providedValues) {
                serializedValues.append(serializeTherionArgumentToken(providedValue.trimmed()));
            }
            result.serializedOptions.append(serializedValues.join(QLatin1Char(' ')));
        }
    }

    result.ok = true;
    return result;
}
}
