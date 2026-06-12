#pragma once

#include "TherionSourceDocument.h"
#include "TherionSourceValidationCatalog.h"

#include <QString>
#include <QVector>

namespace TherionStudio
{
struct TherionSourceTextEdit;

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

class TherionSourceValidator final
{
public:
    [[nodiscard]] static TherionSourceValidationResult validate(const QString &contents);
    [[nodiscard]] static TherionSourceValidationResult validate(const QString &contents,
                                                                const TherionSourceValidationCatalog &catalog);
    [[nodiscard]] static TherionSourceValidationResult validate(const QString &contents,
                                                                const TherionSourceValidationCatalog &catalog,
                                                                const TherionSourceDocumentMetadata &metadata);
    [[nodiscard]] static QVector<TherionSourceTextEdit> validationFixEdits(const QString &contents,
                                                                           const QVector<TherionSourceDiagnosticFix> &fixes);
    [[nodiscard]] static QString applyFixes(const QString &contents,
                                            const QVector<TherionSourceDiagnosticFix> &fixes);
};
}
