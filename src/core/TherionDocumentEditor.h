#pragma once

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
struct TherionDraftObjectOptions
{
    QString type;
    QString subtype;
    QString identifier;
    QString name;
    QString text;
    QString value;
    QString targetScrapIdentifier;
    bool nameEnabled = false;
    bool textEnabled = false;
    bool valueEnabled = false;
};

struct TherionReferencedAreaBoundaryLine
{
    int lineNumber = 0;
    QString identifier;
};

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
                                                  QString *errorMessage = nullptr,
                                                  const TherionDraftObjectOptions &objectOptions = {});
    [[nodiscard]] static bool appendDraftLineGeometry(QString *contents,
                                                      const QStringList &coordinateRows,
                                                      int *insertedLineNumber = nullptr,
                                                      QString *errorMessage = nullptr,
                                                      const QString &lineOptions = QString(),
                                                      const TherionDraftObjectOptions &objectOptions = {});
    [[nodiscard]] static bool appendDraftAreaGeometry(QString *contents,
                                                      const QStringList &coordinateRows,
                                                      int *insertedLineNumber = nullptr,
                                                      QString *errorMessage = nullptr,
                                                      const TherionDraftObjectOptions &objectOptions = {});
    [[nodiscard]] static bool appendReferencedArea(QString *contents,
                                                   int scrapLineNumber,
                                                   const QVector<TherionReferencedAreaBoundaryLine> &boundaryLines,
                                                   int *insertedLineNumber = nullptr,
                                                   QString *errorMessage = nullptr,
                                                   const TherionDraftObjectOptions &objectOptions = {});
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
    [[nodiscard]] static bool rewriteMapObjectTextOption(QString *contents,
                                                         int lineNumber,
                                                         const QString &text,
                                                         QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewriteMapObjectValueOption(QString *contents,
                                                          int lineNumber,
                                                          const QString &value,
                                                          QString *errorMessage = nullptr);
    [[nodiscard]] static bool rewriteLineCoordinateRows(QString *contents,
                                                        int lineNumber,
                                                        const QStringList &coordinateRows,
                                                        QString *errorMessage = nullptr);
};
}
