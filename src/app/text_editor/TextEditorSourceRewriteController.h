#pragma once

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
class TextEditorTab;

class TextEditorSourceRewriteController final
{
public:
    explicit TextEditorSourceRewriteController(TextEditorTab *owner);

    bool rewriteStructureEntryName(int lineNumber,
                                   const QString &category,
                                   const QString &newName,
                                   QString *errorMessage = nullptr);
    bool insertScrapBlock(const QString &preferredName = QString(),
                          int *insertedLineNumber = nullptr,
                          QString *errorMessage = nullptr,
                          const QString &options = QString());
    bool insertDraftGeometry(const QString &kind,
                             const QVector<QPointF> &vertices,
                             int *insertedLineNumber = nullptr,
                             QString *errorMessage = nullptr);
    bool insertDraftLineGeometry(const QStringList &coordinateRows,
                                 int *insertedLineNumber = nullptr,
                                 QString *errorMessage = nullptr);
    bool insertDraftAreaGeometry(const QStringList &coordinateRows,
                                 int *insertedLineNumber = nullptr,
                                 QString *errorMessage = nullptr);
    bool rewritePointCoordinates(int lineNumber,
                                 const QPointF &point,
                                 QString *errorMessage = nullptr);
    bool rewriteLineAreaVertex(int lineNumber,
                               const QString &kind,
                               int vertexIndex,
                               const QPointF &point,
                               QString *errorMessage = nullptr);
    bool rewriteLineOptionToggle(int lineNumber,
                                 const QString &optionName,
                                 bool enabled,
                                 QString *errorMessage = nullptr);
    bool rewritePointOrientation(int lineNumber,
                                 bool enabled,
                                 qreal orientationDegrees,
                                 QString *errorMessage = nullptr);
    bool rewriteLinePointOrientation(int lineNumber,
                                     int sourceVertexIndex,
                                     bool enabled,
                                     qreal orientationDegrees,
                                     QString *errorMessage = nullptr);
    bool rewriteLinePointLeftSize(int lineNumber,
                                  int sourceVertexIndex,
                                  bool enabled,
                                  qreal sizeValue,
                                  QString *errorMessage = nullptr);
    bool rewriteLineCoordinateRows(int lineNumber,
                                   const QStringList &coordinateRows,
                                   QString *errorMessage = nullptr);
    void replaceTextForCommand(const QString &contents);

private:
    void replaceTextPreservingCursor(const QString &contents, bool emitDocumentTextChanged, bool rebuildBlocksCanvas);
    void replaceTextSelectingLine(const QString &contents, int lineNumber);

    TextEditorTab *owner_ = nullptr;
};
}
