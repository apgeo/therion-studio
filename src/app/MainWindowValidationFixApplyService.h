#pragma once

#include "MainWindowDocumentOpenController.h"
#include "../core/TherionSourceValidator.h"

#include <QString>
#include <QVector>

#include <functional>

namespace TherionStudio
{
struct MainWindowValidationFixApplyContext
{
    std::function<bool(const QString &, const QVector<TherionSourceDiagnosticFix> &)> applyFixesToMapPath;
    std::function<bool(const QString &, const QVector<TherionSourceDiagnosticFix> &)> applyFixesToTextPath;
    std::function<bool(const QVector<TherionSourceDiagnosticFix> &)> applyFixesToCurrentMap;
    std::function<bool(const QVector<TherionSourceDiagnosticFix> &)> applyFixesToCurrentText;
};

class MainWindowValidationFixApplyService final
{
public:
    static bool applyValidationFixes(const QString &filePath,
                                     const QString &validationDocumentPath,
                                     const QVector<TherionSourceDiagnosticFix> &fixes,
                                     const MainWindowValidationFixApplyContext &context);
};
}
