#pragma once

#include <QString>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QStringList>

namespace TherionStudio
{
enum class TherionSourceDiagnosticSeverity
{
    Warning,
    Error,
};

struct TherionSourceDiagnosticFix
{
    int startOffset = 0;
    int length = 0;
    QString replacementText;
    QString description;
};

struct TherionSourceDiagnostic
{
    QString code;
    TherionSourceDiagnosticSeverity severity = TherionSourceDiagnosticSeverity::Warning;
    int lineNumber = 0;
    int columnNumber = 0;
    int columnLength = 0;
    QString title;
    QString message;
    QString currentText;
    QString suggestedText;
    bool hasFix = false;
    TherionSourceDiagnosticFix fix;
};

struct TherionSourceValidationResult
{
    QVector<TherionSourceDiagnostic> diagnostics;
};

struct TherionSourceValidationCatalog
{
    QSet<QString> commandNames;
    QHash<QString, QSet<QString>> commandOptionNames;
    QHash<QString, int> commandRequiredPositionalCount;
    QHash<QString, QStringList> commandArgumentAllowedValuesByKey;
    QHash<QString, QStringList> commandTypeValues;
    QHash<QString, QStringList> commandOptionAllowedValuesByKey;
    QHash<QString, QStringList> commandSubtypeValuesByTypeKey;
    QHash<QString, QString> commandOptionValueArityTokens;
    QHash<QString, int> commandOptionFixedArityByKey;
};

class TherionSourceValidator final
{
public:
    [[nodiscard]] static TherionSourceValidationResult validate(const QString &contents);
    [[nodiscard]] static TherionSourceValidationResult validate(const QString &contents,
                                                                const TherionSourceValidationCatalog &catalog);
    [[nodiscard]] static QString applyFixes(const QString &contents,
                                            const QVector<TherionSourceDiagnosticFix> &fixes);
};
}
