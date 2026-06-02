#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class ContextHelpController final
{
public:
    static QString renderList(const QStringList &items);
    static QString renderHelpHtml(const QString &token,
                                  const QString &summary,
                                  const QString &syntax,
                                  const QStringList &arguments,
                                  const QStringList &acceptedValues,
                                  const QStringList &options,
                                  bool includeSyntax,
                                  bool includeTitle = true);
    static QString renderValidationHtml(const QString &cursorToken,
                                        const QString &detailMessage,
                                        const QStringList &allowedValues,
                                        bool includeTitle = true);
};
}
