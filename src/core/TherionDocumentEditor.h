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
    [[nodiscard]] static bool rewriteStructureEntryName(QString *contents,
                                                        int lineNumber,
                                                        const QString &category,
                                                        const QString &newName,
                                                        QString *errorMessage = nullptr);
    [[nodiscard]] static bool appendScrapBlock(QString *contents,
                                               const QString &preferredName = QString(),
                                               int *insertedLineNumber = nullptr,
                                               QString *errorMessage = nullptr,
                                               const QString &options = QString());
    [[nodiscard]] static bool appendDraftGeometry(QString *contents,
                                                  const QString &kind,
                                                  const QVector<QPointF> &vertices,
                                                  int *insertedLineNumber = nullptr,
                                                  QString *errorMessage = nullptr);
    [[nodiscard]] static bool appendDraftLineGeometry(QString *contents,
                                                      const QStringList &coordinateRows,
                                                      int *insertedLineNumber = nullptr,
                                                      QString *errorMessage = nullptr);
    [[nodiscard]] static bool appendDraftAreaGeometry(QString *contents,
                                                      const QStringList &coordinateRows,
                                                      int *insertedLineNumber = nullptr,
                                                      QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewritePointCoordinates(QString *contents,
                                                      int lineNumber,
                                                      const QPointF &point,
                                                      QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewriteLineAreaVertex(QString *contents,
                                                    int lineNumber,
                                                    const QString &kind,
                                                    int vertexIndex,
                                                    const QPointF &point,
                                                    QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewriteLineOptionToggle(QString *contents,
                                                      int lineNumber,
                                                      const QString &optionName,
                                                      bool enabled,
                                                      QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewritePointOrientation(QString *contents,
                                                      int lineNumber,
                                                      bool enabled,
                                                      qreal orientationDegrees,
                                                      QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewriteLinePointOrientation(QString *contents,
                                                          int lineNumber,
                                                          int sourceVertexIndex,
                                                          bool enabled,
                                                          qreal orientationDegrees,
                                                          QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewriteLinePointLeftSize(QString *contents,
                                                       int lineNumber,
                                                       int sourceVertexIndex,
                                                       bool enabled,
                                                       qreal sizeValue,
                                                       QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewriteScrapScale(QString *contents,
                                                int lineNumber,
                                                const QString &scaleExpression,
                                                QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewriteScrapProjection(QString *contents,
                                                     int lineNumber,
                                                     const QString &projectionExpression,
                                                     QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewriteMapObjectQuickFields(QString *contents,
                                                          int lineNumber,
                                                          const QString &type,
                                                          const QString &subtype,
                                                          const QString &identifier,
                                                          const QString &name,
                                                          bool nameEnabled,
                                                          QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewriteLineCoordinateRows(QString *contents,
                                                        int lineNumber,
                                                        const QStringList &coordinateRows,
                                                        QString *errorMessage = nullptr);
};
}
