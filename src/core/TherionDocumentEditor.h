#pragma once

#include <QPointF>
#include <QString>
#include <QStringList>
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
    static bool appendDraftLineGeometry(QString *contents,
                                        const QStringList &coordinateRows,
                                        int *insertedLineNumber = nullptr,
                                        QString *errorMessage = nullptr);
    static bool appendDraftAreaGeometry(QString *contents,
                                        const QStringList &coordinateRows,
                                        int *insertedLineNumber = nullptr,
                                        QString *errorMessage = nullptr);
    static bool rewritePointCoordinates(QString *contents,
                                        int lineNumber,
                                        const QPointF &point,
                                        QString *errorMessage = nullptr);
    static bool rewriteLineAreaVertex(QString *contents,
                                      int lineNumber,
                                      const QString &kind,
                                      int vertexIndex,
                                      const QPointF &point,
                                      QString *errorMessage = nullptr);
    static bool rewriteLineOptionToggle(QString *contents,
                                        int lineNumber,
                                        const QString &optionName,
                                        bool enabled,
                                        QString *errorMessage = nullptr);
    static bool rewriteLineCoordinateRows(QString *contents,
                                          int lineNumber,
                                          const QStringList &coordinateRows,
                                          QString *errorMessage = nullptr);
};
}
