#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

namespace TherionStudio
{
class TherionDocumentEditor final
{
public:
    static bool rewriteStructureEntryName(QString *contents,
                                          int lineNumber,
                                          const QString &category,
                                          const QString &newName,
                                          QString *errorMessage = nullptr);
    static bool appendScrapBlock(QString *contents,
                                 const QString &preferredName = QString(),
                                 int *insertedLineNumber = nullptr,
                                 QString *errorMessage = nullptr);
    static bool appendDraftGeometry(QString *contents,
                                    const QString &kind,
                                    const QVector<QPointF> &vertices,
                                    int *insertedLineNumber = nullptr,
                                    QString *errorMessage = nullptr);
    static bool rewritePointCoordinates(QString *contents,
                                        int lineNumber,
                                        const QPointF &point,
                                        QString *errorMessage = nullptr);
};
}
