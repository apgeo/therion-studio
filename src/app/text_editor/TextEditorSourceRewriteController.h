#pragma once

#include <functional>

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

#include "../../core/TherionDocumentEditor.h"

class QPlainTextEdit;

namespace TherionStudio
{
struct TextEditorSourceRewriteContext
{
    QPlainTextEdit *editor = nullptr;
    bool *loading = nullptr;
    int *currentLineNumber = nullptr;
    std::function<void()> applyDirtyStateFromCurrentState;
    std::function<void()> rebuildBlocksCanvasFromText;
    std::function<void()> documentTextChanged;
    std::function<void()> updateContextHelp;
};

class TextEditorSourceRewriteController final
{
public:
    explicit TextEditorSourceRewriteController(TextEditorSourceRewriteContext context);

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
                             QString *errorMessage = nullptr,
                             const TherionDraftObjectOptions &objectOptions = {},
                             const std::optional<QRectF> &initialAreaAdjustRect = std::nullopt);
    bool insertDraftLineGeometry(const QStringList &coordinateRows,
                                 int *insertedLineNumber = nullptr,
                                 QString *errorMessage = nullptr,
                                 const QString &lineOptions = QString(),
                                 const TherionDraftObjectOptions &objectOptions = {},
                                 const std::optional<QRectF> &initialAreaAdjustRect = std::nullopt);
    bool insertDraftAreaGeometry(const QStringList &coordinateRows,
                                 int *insertedLineNumber = nullptr,
                                 QString *errorMessage = nullptr,
                                 const TherionDraftObjectOptions &objectOptions = {},
                                 const std::optional<QRectF> &initialAreaAdjustRect = std::nullopt);
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
    void replaceTextForCommandWithUndo(const QString &contents);
    void replaceTextForSystemNormalization(const QString &contents);

private:
    void replaceEditorText(const QString &contents, bool recordUndoStep);
    void replaceTextPreservingCursor(const QString &contents,
                                     bool emitDocumentTextChanged,
                                     bool rebuildBlocksCanvas,
                                     bool applyDirtyState,
                                     bool recordUndoStep);
    void replaceTextSelectingLine(const QString &contents, int lineNumber, bool recordUndoStep);

    TextEditorSourceRewriteContext context_;
};
}
