#include "MapEditorTab.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSet>
#include <QtMath>

#include <cmath>

#include "MapEditorSceneSupport.h"
#include "TextEditorTab.h"
#include "../core/SessionStore.h"
#include "../core/TherionDocumentParser.h"

namespace TherionStudio
{
namespace
{
struct XtherionBackgroundReference
{
    QString absolutePath;
    QPointF basePosition;
    bool hasBasePosition = false;
    qreal imageScale = 1.0;
    bool hasImageScale = false;
    QString rootStationName;
    bool metadataTopEdgeAnchor = false;
    bool xviReference = false;
};

struct XviDocument
{
    QPointF gridOrigin;
    bool hasGridOrigin = false;
    QPointF gridVectorX;
    QPointF gridVectorY;
    int gridCountX = 0;
    int gridCountY = 0;
    bool hasGridDefinition = false;
    QHash<QString, QPointF> stations;
    QVector<QLineF> shots;
    QVector<QVector<QPointF>> sketchLines;
};

struct XtherionAreaAdjust
{
    QRectF modelRect;
    bool valid = false;
};

QString normalizedPathKey(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    QFileInfo fileInfo(path);
    QString resolvedPath = fileInfo.canonicalFilePath();
    if (resolvedPath.isEmpty()) {
        resolvedPath = fileInfo.absoluteFilePath();
    }
    if (resolvedPath.isEmpty()) {
        resolvedPath = path;
    }

    return QDir::cleanPath(resolvedPath)
        .normalized(QString::NormalizationForm_C)
        .toCaseFolded();
}

void skipWhitespace(const QString &text, int *position)
{
    if (position == nullptr) {
        return;
    }

    while (*position < text.size() && text.at(*position).isSpace()) {
        ++(*position);
    }
}

bool readBracedGroup(const QString &text, int *position, QString *groupText)
{
    if (position == nullptr || groupText == nullptr) {
        return false;
    }

    skipWhitespace(text, position);
    if (*position >= text.size() || text.at(*position) != QLatin1Char('{')) {
        return false;
    }

    ++(*position);
    const int contentStart = *position;
    int depth = 1;
    while (*position < text.size() && depth > 0) {
        const QChar character = text.at(*position);
        if (character == QLatin1Char('{')) {
            ++depth;
        } else if (character == QLatin1Char('}')) {
            --depth;
            if (depth == 0) {
                break;
            }
        }
        ++(*position);
    }

    if (*position >= text.size() || text.at(*position) != QLatin1Char('}')) {
        return false;
    }

    const int contentEnd = *position;
    ++(*position);
    *groupText = text.mid(contentStart, contentEnd - contentStart).trimmed();
    return true;
}

QString readSimpleToken(const QString &text, int *position)
{
    if (position == nullptr) {
        return QString();
    }

    skipWhitespace(text, position);
    if (*position >= text.size()) {
        return QString();
    }

    const QChar first = text.at(*position);
    if (first == QLatin1Char('"') || first == QLatin1Char('\'')) {
        const QChar quote = first;
        ++(*position);
        const int start = *position;
        while (*position < text.size() && text.at(*position) != quote) {
            ++(*position);
        }
        const int end = *position;
        if (*position < text.size() && text.at(*position) == quote) {
            ++(*position);
        }
        return text.mid(start, end - start).trimmed();
    }

    const int start = *position;
    while (*position < text.size() && !text.at(*position).isSpace()) {
        ++(*position);
    }
    return text.mid(start, *position - start).trimmed();
}

QStringList tokenizeWhitespace(const QString &text)
{
    return text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

bool tryParseLeadingNumber(QString token, qreal *value)
{
    if (value == nullptr) {
        return false;
    }

    token = token.trimmed();
    while (!token.isEmpty()) {
        const QChar character = token.front();
        if (character.isDigit() || character == QLatin1Char('+') || character == QLatin1Char('-') || character == QLatin1Char('.')) {
            break;
        }
        token.remove(0, 1);
    }

    while (!token.isEmpty()) {
        const QChar character = token.back();
        if (character.isDigit() || character == QLatin1Char('.')) {
            break;
        }
        token.chop(1);
    }

    if (token.isEmpty()) {
        return false;
    }

    bool ok = false;
    const qreal parsed = token.toDouble(&ok);
    if (!ok) {
        return false;
    }

    *value = parsed;
    return true;
}

QString normalizeStationToken(const QString &token)
{
    QString normalized = token.trimmed();
    while (normalized.startsWith(QLatin1Char('{')) || normalized.startsWith(QLatin1Char('}'))) {
        normalized.remove(0, 1);
    }
    while (normalized.endsWith(QLatin1Char('}')) || normalized.endsWith(QLatin1Char('{'))) {
        normalized.chop(1);
    }
    return normalized.trimmed();
}

QVector<QPointF> parsePointPairs(const QStringList &tokens, int startIndex)
{
    QVector<QPointF> points;
    QVector<qreal> numbers;
    for (int index = qMax(0, startIndex); index < tokens.size(); ++index) {
        qreal value = 0.0;
        if (tryParseLeadingNumber(tokens.at(index), &value)) {
            numbers.append(value);
        }
    }

    for (int index = 0; index + 1 < numbers.size(); index += 2) {
        points.append(QPointF(numbers.at(index), numbers.at(index + 1)));
    }
    return points;
}

bool parseXviDocument(const QString &xviPath, XviDocument *document)
{
    if (document == nullptr) {
        return false;
    }

    QFile file(xviPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QString content = QString::fromUtf8(file.readAll());
    const QStringList lines = content.split(QLatin1Char('\n'));
    enum class Block
    {
        None,
        Stations,
        Shots,
        SketchLines
    };
    Block block = Block::None;

    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        if (trimmed.startsWith(QStringLiteral("set XVIstations"))) {
            block = Block::Stations;
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("set XVIshots"))) {
            block = Block::Shots;
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("set XVIsketchlines"))) {
            block = Block::SketchLines;
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("set XVIgrid"))) {
            const int open = trimmed.indexOf(QLatin1Char('{'));
            const int close = trimmed.lastIndexOf(QLatin1Char('}'));
            if (open >= 0 && close > open) {
                const QString payload = trimmed.mid(open + 1, close - open - 1);
                const QStringList tokens = tokenizeWhitespace(payload);
                if (tokens.size() >= 2) {
                    qreal x = 0.0;
                    qreal y = 0.0;
                    if (tryParseLeadingNumber(tokens.at(0), &x) && tryParseLeadingNumber(tokens.at(1), &y)) {
                        document->gridOrigin = QPointF(x, y);
                        document->hasGridOrigin = true;
                    }
                }

                QVector<qreal> numbers;
                numbers.reserve(tokens.size());
                for (const QString &token : tokens) {
                    qreal value = 0.0;
                    if (tryParseLeadingNumber(token, &value)) {
                        numbers.append(value);
                    }
                }
                if (numbers.size() >= 8) {
                    document->gridVectorX = QPointF(numbers.at(2), numbers.at(3));
                    document->gridVectorY = QPointF(numbers.at(4), numbers.at(5));
                    document->gridCountX = qMax(0, qRound(numbers.at(6)));
                    document->gridCountY = qMax(0, qRound(numbers.at(7)));
                    document->hasGridDefinition = document->gridCountX > 0 && document->gridCountY > 0;
                }
            }
            block = Block::None;
            continue;
        }

        if (trimmed == QStringLiteral("}")) {
            block = Block::None;
            continue;
        }

        const int open = trimmed.indexOf(QLatin1Char('{'));
        const int close = trimmed.lastIndexOf(QLatin1Char('}'));
        if (open < 0 || close <= open) {
            continue;
        }

        const QString payload = trimmed.mid(open + 1, close - open - 1).trimmed();
        const QStringList tokens = tokenizeWhitespace(payload);
        if (tokens.isEmpty()) {
            continue;
        }

        if (block == Block::Stations) {
            if (tokens.size() < 3) {
                continue;
            }
            qreal x = 0.0;
            qreal y = 0.0;
            if (!tryParseLeadingNumber(tokens.at(0), &x) || !tryParseLeadingNumber(tokens.at(1), &y)) {
                continue;
            }
            const QString stationName = normalizeStationToken(tokens.last());
            if (!stationName.isEmpty()) {
                document->stations.insert(stationName, QPointF(x, y));
            }
            continue;
        }

        if (block == Block::Shots) {
            const QVector<QPointF> points = parsePointPairs(tokens, 0);
            if (points.size() >= 2) {
                document->shots.append(QLineF(points.at(0), points.at(1)));
            }
            continue;
        }

        if (block == Block::SketchLines) {
            const QVector<QPointF> points = parsePointPairs(tokens, 1);
            if (points.size() >= 2) {
                document->sketchLines.append(points);
            }
            continue;
        }
    }

    return document->hasGridOrigin && (!document->shots.isEmpty() || !document->sketchLines.isEmpty() || !document->stations.isEmpty());
}

QVector<XtherionBackgroundReference> parseXtherionBackgroundReferences(const QString &documentText, const QString &documentPath)
{
    QVector<XtherionBackgroundReference> references;
    const QString baseDirectory = QFileInfo(documentPath).absolutePath();
    const QStringList lines = documentText.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }

        const QString trimmed = line.trimmed();
        if (!trimmed.contains(QStringLiteral("##XTHERION##")) || !trimmed.contains(QStringLiteral("xth_me_image_insert"))) {
            continue;
        }

        int keywordIndex = trimmed.indexOf(QStringLiteral("xth_me_image_insert"));
        if (keywordIndex < 0) {
            continue;
        }

        int position = keywordIndex + QStringLiteral("xth_me_image_insert").size();
        QString firstGroup;
        QString secondGroup;
        if (!readBracedGroup(trimmed, &position, &firstGroup) || !readBracedGroup(trimmed, &position, &secondGroup)) {
            continue;
        }

        const QString imageToken = readSimpleToken(trimmed, &position);
        if (imageToken.isEmpty()) {
            continue;
        }

        QString absolutePath = imageToken;
        if (QDir::isRelativePath(absolutePath) && !baseDirectory.isEmpty()) {
            absolutePath = QDir(baseDirectory).absoluteFilePath(absolutePath);
        }
        absolutePath = QFileInfo(absolutePath).absoluteFilePath();
        if (absolutePath.isEmpty()) {
            continue;
        }

        XtherionBackgroundReference reference{};
        reference.absolutePath = absolutePath;
        reference.xviReference = reference.absolutePath.endsWith(QStringLiteral(".xvi"), Qt::CaseInsensitive);
        reference.metadataTopEdgeAnchor = !reference.xviReference;

        const QStringList firstParts = tokenizeWhitespace(firstGroup);
        const QStringList secondParts = tokenizeWhitespace(secondGroup);
        if (!firstParts.isEmpty() && !secondParts.isEmpty()) {
            qreal baseX = 0.0;
            qreal baseY = 0.0;
            if (tryParseLeadingNumber(firstParts.first(), &baseX)
                && tryParseLeadingNumber(secondParts.first(), &baseY)) {
                reference.basePosition = QPointF(baseX, baseY);
                reference.hasBasePosition = true;
            }
        }

        if (firstParts.size() >= 3) {
            qreal imageScale = 0.0;
            if (tryParseLeadingNumber(firstParts.at(2), &imageScale) && imageScale > 0.0) {
                reference.imageScale = imageScale;
                reference.hasImageScale = true;
            }
        }

        if (secondParts.size() >= 2) {
            const QString candidateRoot = normalizeStationToken(secondParts.at(1));
            if (!candidateRoot.isEmpty() && candidateRoot != QStringLiteral("0")) {
                reference.rootStationName = candidateRoot;
            }
        }
        references.append(reference);
    }

    return references;
}

XtherionAreaAdjust parseXtherionAreaAdjust(const QString &documentText)
{
    const QStringList lines = documentText.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }

        const QString trimmed = line.trimmed();
        if (!trimmed.contains(QStringLiteral("##XTHERION##")) || !trimmed.contains(QStringLiteral("xth_me_area_adjust"))) {
            continue;
        }

        const int keywordIndex = trimmed.indexOf(QStringLiteral("xth_me_area_adjust"));
        if (keywordIndex < 0) {
            continue;
        }

        const QString payload = trimmed.mid(keywordIndex + QStringLiteral("xth_me_area_adjust").size());
        const QStringList tokens = tokenizeWhitespace(payload);
        QVector<qreal> numbers;
        numbers.reserve(tokens.size());
        for (const QString &token : tokens) {
            qreal value = 0.0;
            if (tryParseLeadingNumber(token, &value)) {
                numbers.append(value);
            }
        }

        if (numbers.size() < 4) {
            continue;
        }

        const qreal left = qMin(numbers.at(0), numbers.at(2));
        const qreal right = qMax(numbers.at(0), numbers.at(2));
        const qreal bottom = qMin(numbers.at(1), numbers.at(3));
        const qreal top = qMax(numbers.at(1), numbers.at(3));
        const QRectF rect(QPointF(left, bottom), QPointF(right, top));
        if (rect.isValid() && rect.width() > 0.0 && rect.height() > 0.0) {
            return XtherionAreaAdjust{rect, true};
        }
    }

    return XtherionAreaAdjust{};
}

QRectF fittedModelBoundsInPreview(const QRectF &modelBounds, const QRectF &previewBounds)
{
    if (!modelBounds.isValid() || !previewBounds.isValid()) {
        return previewBounds;
    }

    const qreal modelWidth = qMax(1.0, modelBounds.width());
    const qreal modelHeight = qMax(1.0, modelBounds.height());
    const qreal previewWidth = qMax(1.0, previewBounds.width());
    const qreal previewHeight = qMax(1.0, previewBounds.height());
    const qreal zoom = qMin(previewWidth / modelWidth, previewHeight / modelHeight);
    const qreal fittedWidth = modelWidth * zoom;
    const qreal fittedHeight = modelHeight * zoom;
    const qreal left = previewBounds.left() + ((previewWidth - fittedWidth) / 2.0);
    const qreal top = previewBounds.top() + ((previewHeight - fittedHeight) / 2.0);
    return QRectF(left, top, fittedWidth, fittedHeight);
}

QPointF modelToPreviewPoint(const QPointF &modelPoint, const QRectF &modelBounds, const QRectF &previewBounds)
{
    if (!modelBounds.isValid() || !previewBounds.isValid()) {
        return modelPoint;
    }

    const QRectF fitted = fittedModelBoundsInPreview(modelBounds, previewBounds);
    const qreal zoom = qMin(fitted.width() / qMax(1.0, modelBounds.width()),
                            fitted.height() / qMax(1.0, modelBounds.height()));
    const qreal panX = fitted.left() - (modelBounds.left() * zoom);
    const qreal panY = fitted.top() + (modelBounds.bottom() * zoom);
    return QPointF((modelPoint.x() * zoom) + panX,
                   panY - (modelPoint.y() * zoom));
}

QSizeF rasterModelSize(const QString &layerPath, qreal imageScale)
{
    Q_UNUSED(imageScale);

    if (layerPath.isEmpty()) {
        return QSizeF();
    }

    QImageReader imageReader(layerPath);
    imageReader.setAutoTransform(true);
    const QSize imageSize = imageReader.size();
    if (!imageSize.isValid()) {
        return QSizeF();
    }

    return QSizeF(imageSize.width(), imageSize.height());
}

bool sizesNearlyEqual(const QSizeF &a, const QSizeF &b)
{
    if (!a.isValid() || !b.isValid()) {
        return false;
    }

    const qreal widthTolerance = qMax<qreal>(1.0, qMax(a.width(), b.width()) * 0.02);
    const qreal heightTolerance = qMax<qreal>(1.0, qMax(a.height(), b.height()) * 0.02);
    return qAbs(a.width() - b.width()) <= widthTolerance
        && qAbs(a.height() - b.height()) <= heightTolerance;
}

bool placeRasterLayerByModelRect(QGraphicsPixmapItem *item,
                                 const QRectF &modelRect,
                                 const QRectF &modelBounds,
                                 const QRectF &previewBounds)
{
    if (item == nullptr || !modelRect.isValid() || !modelBounds.isValid() || !previewBounds.isValid()) {
        return false;
    }

    const QString layerPath = item->data(0).toString();
    if (layerPath.isEmpty()) {
        return false;
    }

    QImageReader imageReader(layerPath);
    imageReader.setAutoTransform(true);
    const QImage sourceImage = imageReader.read();
    if (sourceImage.isNull()) {
        return false;
    }

    const QPointF modelUpperLeft(modelRect.left(), modelRect.bottom());
    const QPointF modelLowerRight(modelRect.right(), modelRect.top());
    const QPointF viewA = modelToPreviewPoint(modelUpperLeft, modelBounds, previewBounds);
    const QPointF viewB = modelToPreviewPoint(modelLowerRight, modelBounds, previewBounds);
    const QRectF viewRect(QPointF(qMin(viewA.x(), viewB.x()), qMin(viewA.y(), viewB.y())),
                          QPointF(qMax(viewA.x(), viewB.x()), qMax(viewA.y(), viewB.y())));
    if (!viewRect.isValid() || viewRect.width() < 1.0 || viewRect.height() < 1.0) {
        return false;
    }

    QImage scaled = sourceImage.scaled(qMax(2, qRound(viewRect.width())),
                                       qMax(2, qRound(viewRect.height())),
                                       Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
    item->setPixmap(QPixmap::fromImage(scaled));
    item->setPos(viewRect.topLeft());
    return true;
}

bool placeRasterLayerByModelAnchor(QGraphicsPixmapItem *item,
                                   const QPointF &basePosition,
                                   bool topEdgeAnchor,
                                   qreal imageScale,
                                   const QRectF &modelBounds,
                                   const QRectF &previewBounds);

bool placeRasterLayerFromMetadata(QGraphicsPixmapItem *item,
                                  const XtherionBackgroundReference &reference,
                                  const XtherionAreaAdjust &areaAdjust,
                                  const QRectF &modelBounds,
                                  const QRectF &previewBounds)
{
    if (item == nullptr || !reference.hasBasePosition) {
        return false;
    }

    const QString layerPath = item->data(0).toString();
    const QSizeF modelSize = rasterModelSize(layerPath, reference.hasImageScale ? reference.imageScale : 1.0);
    if (areaAdjust.valid && sizesNearlyEqual(modelSize, areaAdjust.modelRect.size())) {
        return placeRasterLayerByModelRect(item, areaAdjust.modelRect, modelBounds, previewBounds);
    }

    return placeRasterLayerByModelAnchor(item,
                                         reference.basePosition,
                                         reference.metadataTopEdgeAnchor,
                                         reference.hasImageScale ? reference.imageScale : 1.0,
                                         modelBounds,
                                         previewBounds);
}

const XtherionBackgroundReference *findMetadataReferenceForPath(
    const QString &layerPath,
    const QHash<QString, XtherionBackgroundReference> &metadataByPath,
    const QHash<QString, QVector<XtherionBackgroundReference>> &metadataByFileName)
{
    const QString pathKey = normalizedPathKey(layerPath);
    if (!pathKey.isEmpty()) {
        const auto pathIt = metadataByPath.constFind(pathKey);
        if (pathIt != metadataByPath.constEnd()) {
            return &pathIt.value();
        }
    }

    const QString fileNameKey = QFileInfo(layerPath).fileName().trimmed().toCaseFolded();
    if (fileNameKey.isEmpty()) {
        return nullptr;
    }

    const auto fileNameIt = metadataByFileName.constFind(fileNameKey);
    if (fileNameIt == metadataByFileName.constEnd()) {
        return nullptr;
    }

    const QVector<XtherionBackgroundReference> &candidates = fileNameIt.value();
    if (candidates.size() == 1) {
        return &candidates.first();
    }

    return nullptr;
}

bool placeRasterLayerByModelAnchor(QGraphicsPixmapItem *item,
                                   const QPointF &basePosition,
                                   bool topEdgeAnchor,
                                   qreal imageScale,
                                   const QRectF &modelBounds,
                                   const QRectF &previewBounds)
{
    if (item == nullptr || !modelBounds.isValid() || !previewBounds.isValid()) {
        return false;
    }

    const QString layerPath = item->data(0).toString();
    if (layerPath.isEmpty()) {
        return false;
    }

    QImageReader imageReader(layerPath);
    imageReader.setAutoTransform(true);
    const QImage sourceImage = imageReader.read();
    if (sourceImage.isNull()) {
        return false;
    }

    Q_UNUSED(imageScale);

    const QSizeF modelSize(sourceImage.width(), sourceImage.height());
    const QPointF modelTopLeft(basePosition.x(),
                               topEdgeAnchor ? (basePosition.y() - modelSize.height()) : basePosition.y());
    const QPointF modelBottomRight(modelTopLeft.x() + modelSize.width(),
                                   modelTopLeft.y() + modelSize.height());

    const QPointF viewA = modelToPreviewPoint(modelTopLeft, modelBounds, previewBounds);
    const QPointF viewB = modelToPreviewPoint(modelBottomRight, modelBounds, previewBounds);
    const QRectF viewRect(QPointF(qMin(viewA.x(), viewB.x()), qMin(viewA.y(), viewB.y())),
                          QPointF(qMax(viewA.x(), viewB.x()), qMax(viewA.y(), viewB.y())));
    if (!viewRect.isValid() || viewRect.width() < 1.0 || viewRect.height() < 1.0) {
        return false;
    }

    QImage scaled = sourceImage.scaled(qMax(2, qRound(viewRect.width())),
                                       qMax(2, qRound(viewRect.height())),
                                       Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
    item->setPixmap(QPixmap::fromImage(scaled));
    item->setPos(viewRect.topLeft());
    return true;
}

bool buildXviLayerPixmap(const XviDocument &xvi,
                         const QPointF &anchoredBasePosition,
                         const QString &rootStationName,
                         const QRectF &modelBounds,
                         const QRectF &previewBounds,
                         QPixmap *pixmap,
                         QPointF *topLeft)
{
    if (pixmap == nullptr || topLeft == nullptr || !xvi.hasGridOrigin) {
        return false;
    }

    QPointF anchoredPosition = anchoredBasePosition;
    if (!rootStationName.isEmpty()) {
        const auto stationIt = xvi.stations.constFind(rootStationName);
        if (stationIt != xvi.stations.constEnd()) {
            anchoredPosition = xvi.gridOrigin + anchoredBasePosition - stationIt.value();
        }
    }

    const QPointF offset = anchoredPosition - xvi.gridOrigin;
    QVector<QVector<QPointF>> projectedPolylines;
    projectedPolylines.reserve(xvi.shots.size() + xvi.sketchLines.size());

    auto appendLine = [&](const QPointF &modelA, const QPointF &modelB) {
        QVector<QPointF> polyline;
        polyline.reserve(2);
        polyline.append(modelToPreviewPoint(modelA, modelBounds, previewBounds));
        polyline.append(modelToPreviewPoint(modelB, modelBounds, previewBounds));
        projectedPolylines.append(polyline);
    };

    for (const QLineF &shot : xvi.shots) {
        appendLine(shot.p1() + offset, shot.p2() + offset);
    }

    for (const QVector<QPointF> &line : xvi.sketchLines) {
        QVector<QPointF> polyline;
        polyline.reserve(line.size());
        for (const QPointF &point : line) {
            polyline.append(modelToPreviewPoint(point + offset, modelBounds, previewBounds));
        }
        if (polyline.size() >= 2) {
            projectedPolylines.append(polyline);
        }
    }

    if (projectedPolylines.isEmpty()) {
        return false;
    }

    QRectF bounds;
    bool hasBounds = false;
    auto includePoint = [&bounds, &hasBounds](const QPointF &point) {
        const QRectF pointRect(point, QSizeF(1.0, 1.0));
        if (!hasBounds) {
            bounds = pointRect;
            hasBounds = true;
        } else {
            bounds = bounds.united(pointRect);
        }
    };

    if (xvi.hasGridDefinition) {
        const int spanX = qMax(0, xvi.gridCountX - 1);
        const int spanY = qMax(0, xvi.gridCountY - 1);
        const QPointF gridP00 = xvi.gridOrigin + offset;
        const QPointF gridP10 = gridP00 + (xvi.gridVectorX * spanX);
        const QPointF gridP01 = gridP00 + (xvi.gridVectorY * spanY);
        const QPointF gridP11 = gridP10 + (xvi.gridVectorY * spanY);
        includePoint(modelToPreviewPoint(gridP00, modelBounds, previewBounds));
        includePoint(modelToPreviewPoint(gridP10, modelBounds, previewBounds));
        includePoint(modelToPreviewPoint(gridP01, modelBounds, previewBounds));
        includePoint(modelToPreviewPoint(gridP11, modelBounds, previewBounds));
    }

    for (const QVector<QPointF> &polyline : std::as_const(projectedPolylines)) {
        for (const QPointF &point : polyline) {
            includePoint(point);
        }
    }

    if (!hasBounds || !bounds.isValid()) {
        return false;
    }

    const qreal padding = 2.0;
    const QRectF paddedBounds = bounds.adjusted(-padding, -padding, padding, padding);
    QImage layerImage(qMax(2, qCeil(paddedBounds.width())),
                      qMax(2, qCeil(paddedBounds.height())),
                      QImage::Format_ARGB32_Premultiplied);
    layerImage.fill(Qt::transparent);

    QPainter painter(&layerImage);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(-paddedBounds.left(), -paddedBounds.top());

    QPen sketchPen(QColor(0, 0, 0, 72));
    sketchPen.setWidthF(0.9);
    sketchPen.setCapStyle(Qt::RoundCap);
    sketchPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(sketchPen);
    for (const QVector<QPointF> &polyline : std::as_const(projectedPolylines)) {
        for (int index = 1; index < polyline.size(); ++index) {
            painter.drawLine(polyline.at(index - 1), polyline.at(index));
        }
    }

    *pixmap = QPixmap::fromImage(layerImage);
    *topLeft = paddedBounds.topLeft();
    return !pixmap->isNull();
}
}

int MapEditorTab::backgroundLayerCount() const
{
    return backgroundImageItems_.size();
}

QString MapEditorTab::backgroundLayerLabel(int index) const
{
    QGraphicsPixmapItem *item = backgroundLayerItemAt(index);
    if (item == nullptr) {
        return QString();
    }

    const QString layerPath = item->data(0).toString();
    const QString layerName = QFileInfo(layerPath).fileName().isEmpty() ? tr("Background") : QFileInfo(layerPath).fileName();
    return layerName;
}

bool MapEditorTab::isBackgroundLayerVisible(int index) const
{
    QGraphicsPixmapItem *item = backgroundLayerItemAt(index);
    return item != nullptr && item->isVisible();
}

qreal MapEditorTab::backgroundLayerOpacity(int index) const
{
    QGraphicsPixmapItem *item = backgroundLayerItemAt(index);
    return item != nullptr ? item->opacity() : 0.0;
}

qreal MapEditorTab::backgroundLayerGamma(int index) const
{
    return backgroundLayerGammaValue(backgroundLayerItemAt(index));
}

QPointF MapEditorTab::backgroundLayerPosition(int index) const
{
    QGraphicsPixmapItem *item = backgroundLayerItemAt(index);
    return item != nullptr ? item->pos() : QPointF();
}

int MapEditorTab::selectedBackgroundLayerIndex() const
{
    return selectedBackgroundLayerIndex_;
}

void MapEditorTab::setSelectedBackgroundLayerIndex(int index)
{
    setSelectedBackgroundLayerIndexInternal(index);
    refreshBackgroundLayerControls();
}

void MapEditorTab::browseAndAddBackgroundImages()
{
    const QString initialDirectory = filePath().isEmpty()
        ? QString()
        : QFileInfo(filePath()).absolutePath();
    const QStringList imagePaths = QFileDialog::getOpenFileNames(this,
                                                                 tr("Add Background Images"),
                                                                 initialDirectory,
                                                                 tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.gif *.webp)"));
    if (imagePaths.isEmpty()) {
        return;
    }

    const int previousLayerCount = backgroundImageItems_.size();
    for (const QString &imagePath : imagePaths) {
        addBackgroundImage(imagePath);
    }

    const int addedLayerCount = backgroundImageItems_.size() - previousLayerCount;
    if (addedLayerCount > 0) {
        toolbarStatusNote_ = tr("Added %1 background image layer(s).").arg(addedLayerCount);
        saveBackgroundLayersToSession();
    } else {
        toolbarStatusNote_ = tr("No background images were added.");
    }

    updateCommandSurfaceState();
}

void MapEditorTab::removeSelectedBackgroundLayer()
{
    if (selectedBackgroundLayerIndex_ < 0 || selectedBackgroundLayerIndex_ >= backgroundImageItems_.size()) {
        return;
    }

    QGraphicsPixmapItem *item = backgroundImageItems_.takeAt(selectedBackgroundLayerIndex_);
    if (item != nullptr && mapScene_ != nullptr) {
        mapScene_->removeItem(item);
    }
    delete item;

    applyBackgroundLayerStackingOrder();
    setSelectedBackgroundLayerIndexInternal(selectedBackgroundLayerIndex_);
    toolbarStatusNote_ = tr("Removed selected background layer.");
    saveBackgroundLayersToSession();
    updateCommandSurfaceState();
}

void MapEditorTab::moveSelectedBackgroundLayerUp()
{
    const int currentIndex = selectedBackgroundLayerIndex_;
    if (currentIndex <= 0 || currentIndex >= backgroundImageItems_.size()) {
        return;
    }

    backgroundImageItems_.swapItemsAt(currentIndex, currentIndex - 1);
    applyBackgroundLayerStackingOrder();
    saveBackgroundLayersToSession();
    setSelectedBackgroundLayerIndexInternal(currentIndex - 1);
}

void MapEditorTab::moveSelectedBackgroundLayerDown()
{
    const int currentIndex = selectedBackgroundLayerIndex_;
    if (currentIndex < 0 || currentIndex >= backgroundImageItems_.size() - 1) {
        return;
    }

    backgroundImageItems_.swapItemsAt(currentIndex, currentIndex + 1);
    applyBackgroundLayerStackingOrder();
    saveBackgroundLayersToSession();
    setSelectedBackgroundLayerIndexInternal(currentIndex + 1);
}

void MapEditorTab::toggleSelectedBackgroundLayerVisibility()
{
    QGraphicsPixmapItem *item = selectedBackgroundLayerItem();
    if (item == nullptr) {
        return;
    }

    item->setVisible(!item->isVisible());
    saveBackgroundLayersToSession();
    refreshBackgroundLayerControls();
}

void MapEditorTab::setSelectedBackgroundLayerOpacity(qreal opacity)
{
    QGraphicsPixmapItem *item = selectedBackgroundLayerItem();
    if (item == nullptr) {
        return;
    }

    item->setOpacity(qBound(0.05, opacity, 1.0));
    saveBackgroundLayersToSession();
    refreshBackgroundLayerControls();
}

void MapEditorTab::resetSelectedBackgroundLayerOpacity()
{
    setSelectedBackgroundLayerOpacity(0.58);
}

void MapEditorTab::setSelectedBackgroundLayerGamma(qreal gamma)
{
    QGraphicsPixmapItem *item = selectedBackgroundLayerItem();
    if (item == nullptr) {
        return;
    }

    applyBackgroundLayerGamma(item, qBound(0.2, gamma, 2.5));
    saveBackgroundLayersToSession();
    refreshBackgroundLayerControls();
}

void MapEditorTab::resetSelectedBackgroundLayerGamma()
{
    setSelectedBackgroundLayerGamma(1.0);
}

void MapEditorTab::setSelectedBackgroundLayerPosition(const QPointF &position)
{
    QGraphicsPixmapItem *item = selectedBackgroundLayerItem();
    if (item == nullptr) {
        return;
    }

    item->setPos(position);
    saveBackgroundLayersToSession();
    refreshBackgroundLayerControls();
}

void MapEditorTab::nudgeSelectedBackgroundLayer(const QPointF &delta)
{
    QGraphicsPixmapItem *item = selectedBackgroundLayerItem();
    if (item == nullptr) {
        return;
    }

    item->setPos(item->pos() + delta);
    saveBackgroundLayersToSession();
    refreshBackgroundLayerControls();
}

void MapEditorTab::handleFitWithBackgroundTriggered()
{
    fitBackgroundRequested_ = !backgroundImageItems_.isEmpty();
    toolbarStatusNote_ = backgroundImageItems_.isEmpty()
        ? tr("Fit + BG: no background layers loaded, fitting geometry only.")
        : tr("Fit + BG: fitting geometry plus %1 background layer(s).").arg(backgroundImageItems_.size());
    fitMapToView(true);
    refreshToolbarSummary();
}

QRectF MapEditorTab::mapBackgroundFitBounds() const
{
    QRectF combinedBounds;
    bool hasBounds = false;

    for (QGraphicsPixmapItem *backgroundItem : backgroundImageItems_) {
        if (backgroundItem == nullptr || !backgroundItem->isVisible()) {
            continue;
        }

        const QRectF itemBounds = backgroundItem->sceneBoundingRect();
        if (!itemBounds.isValid()) {
            continue;
        }

        if (!hasBounds) {
            combinedBounds = itemBounds;
            hasBounds = true;
        } else {
            combinedBounds = combinedBounds.united(itemBounds);
        }
    }

    return combinedBounds;
}

void MapEditorTab::refreshBackgroundLayerControls()
{
    updatingBackgroundLayerControls_ = true;
    setSelectedBackgroundLayerIndexInternal(selectedBackgroundLayerIndex_);
    updatingBackgroundLayerControls_ = false;
    emit backgroundLayersChanged();
}

void MapEditorTab::applyBackgroundLayerStackingOrder()
{
    for (int index = 0; index < backgroundImageItems_.size(); ++index) {
        QGraphicsPixmapItem *item = backgroundImageItems_.at(index);
        if (item == nullptr) {
            continue;
        }

        item->setZValue(1.5 + (index * 0.1));
    }
}

QGraphicsPixmapItem *MapEditorTab::backgroundLayerItemAt(int index) const
{
    if (index < 0 || index >= backgroundImageItems_.size()) {
        return nullptr;
    }

    return backgroundImageItems_.at(index);
}

QGraphicsPixmapItem *MapEditorTab::selectedBackgroundLayerItem() const
{
    return backgroundLayerItemAt(selectedBackgroundLayerIndex_);
}

qreal MapEditorTab::backgroundLayerGammaValue(const QGraphicsPixmapItem *item) const
{
    if (item == nullptr) {
        return 1.0;
    }

    const QVariant gammaValue = item->data(2);
    if (!gammaValue.isValid()) {
        return 1.0;
    }

    bool ok = false;
    const qreal parsedGamma = gammaValue.toDouble(&ok);
    return qBound(0.2, ok ? parsedGamma : 1.0, 2.5);
}

void MapEditorTab::setSelectedBackgroundLayerIndexInternal(int index)
{
    if (backgroundImageItems_.isEmpty()) {
        selectedBackgroundLayerIndex_ = -1;
        return;
    }

    selectedBackgroundLayerIndex_ = qBound(0, index, backgroundImageItems_.size() - 1);
}

QString MapEditorTab::canonicalDocumentSessionKey() const
{
    const QString path = filePath();
    if (path.isEmpty()) {
        return QString();
    }

    QFileInfo fileInfo(path);
    const QString canonical = fileInfo.canonicalFilePath();
    if (!canonical.isEmpty()) {
        return canonical;
    }

    return fileInfo.absoluteFilePath();
}

void MapEditorTab::saveBackgroundLayersToSession() const
{
    const QString documentKey = canonicalDocumentSessionKey();
    if (documentKey.isEmpty()) {
        return;
    }

    QJsonObject rootObject;
    const QString existingJson = SessionStore::therionMapBackgroundLayers();
    if (!existingJson.isEmpty()) {
        const QJsonDocument existingDocument = QJsonDocument::fromJson(existingJson.toUtf8());
        if (existingDocument.isObject()) {
            rootObject = existingDocument.object();
        }
    }

    if (backgroundImageItems_.isEmpty()) {
        rootObject.remove(documentKey);
        const QByteArray jsonBytes = QJsonDocument(rootObject).toJson(QJsonDocument::Compact);
        SessionStore::setTherionMapBackgroundLayers(QString::fromUtf8(jsonBytes));
        return;
    }

    QJsonArray layersArray;
    for (QGraphicsPixmapItem *item : backgroundImageItems_) {
        if (item == nullptr) {
            continue;
        }

        const QString layerPath = item->data(0).toString();
        if (layerPath.isEmpty()) {
            continue;
        }

        QJsonObject layerObject;
        layerObject.insert(QStringLiteral("path"), layerPath);
        layerObject.insert(QStringLiteral("visible"), item->isVisible());
        layerObject.insert(QStringLiteral("opacity"), item->opacity());
        layerObject.insert(QStringLiteral("gamma"), backgroundLayerGammaValue(item));
        layerObject.insert(QStringLiteral("x"), item->pos().x());
        layerObject.insert(QStringLiteral("y"), item->pos().y());
        layersArray.append(layerObject);
    }

    rootObject.insert(documentKey, layersArray);
    const QByteArray jsonBytes = QJsonDocument(rootObject).toJson(QJsonDocument::Compact);
    SessionStore::setTherionMapBackgroundLayers(QString::fromUtf8(jsonBytes));
}

void MapEditorTab::loadBackgroundLayersFromSession()
{
    if (!backgroundImageItems_.isEmpty()) {
        return;
    }

    const QString documentKey = canonicalDocumentSessionKey();
    if (documentKey.isEmpty()) {
        return;
    }

    const QString json = SessionStore::therionMapBackgroundLayers();
    if (json.isEmpty()) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isObject()) {
        return;
    }

    const QJsonValue layersValue = document.object().value(documentKey);
    if (!layersValue.isArray()) {
        return;
    }

    const QVector<XtherionBackgroundReference> metadataReferences = textEditor_ != nullptr
        ? parseXtherionBackgroundReferences(textEditor_->text(), filePath())
        : QVector<XtherionBackgroundReference>{};
    const XtherionAreaAdjust areaAdjust = textEditor_ != nullptr
        ? parseXtherionAreaAdjust(textEditor_->text())
        : XtherionAreaAdjust{};
    QHash<QString, XtherionBackgroundReference> metadataByPath;
    QHash<QString, QVector<XtherionBackgroundReference>> metadataByFileName;
    for (const XtherionBackgroundReference &reference : metadataReferences) {
        const QString pathKey = normalizedPathKey(reference.absolutePath);
        if (!pathKey.isEmpty()) {
            metadataByPath.insert(pathKey, reference);
        }
        const QString fileNameKey = QFileInfo(reference.absolutePath).fileName().trimmed().toCaseFolded();
        if (!fileNameKey.isEmpty()) {
            metadataByFileName[fileNameKey].append(reference);
        }
    }

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(textEditor_ != nullptr ? textEditor_->text() : QString());
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const QRectF sourceBounds = geometryBoundsForFeatures(features);
    const QRectF previewBounds = mapPreviewBounds();

    for (const QJsonValue &value : layersValue.toArray()) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject layerObject = value.toObject();
        const QString layerPath = QFileInfo(layerObject.value(QStringLiteral("path")).toString()).absoluteFilePath();
        if (layerPath.isEmpty() || !QFileInfo::exists(layerPath)) {
            continue;
        }

        const XtherionBackgroundReference *metadataReference = findMetadataReferenceForPath(layerPath, metadataByPath, metadataByFileName);
        const bool hasMetadata = metadataReference != nullptr;

        if (layerPath.endsWith(QStringLiteral(".xvi"), Qt::CaseInsensitive)) {
            XviDocument xviDocument;
            QPixmap pixmap;
            QPointF topLeft;
            const QPointF basePosition = hasMetadata && metadataReference->hasBasePosition
                ? metadataReference->basePosition
                : QPointF(0.0, 0.0);
            const QString rootStationName = hasMetadata ? metadataReference->rootStationName : QString();
            if (!parseXviDocument(layerPath, &xviDocument)
                || !buildXviLayerPixmap(xviDocument, basePosition, rootStationName, sourceBounds, previewBounds, &pixmap, &topLeft)) {
                continue;
            }

            auto *backgroundItem = new QGraphicsPixmapItem(pixmap);
            backgroundItem->setTransformationMode(Qt::SmoothTransformation);
            backgroundItem->setOpacity(0.58);
            backgroundItem->setData(0, QFileInfo(layerPath).absoluteFilePath());
            backgroundItem->setData(2, 1.0);
            backgroundItem->setToolTip(QFileInfo(layerPath).fileName());
            backgroundItem->setPos(topLeft);
            mapScene_->addItem(backgroundItem);
            backgroundImageItems_.append(backgroundItem);
        } else {
            addBackgroundImage(layerPath);
            if (backgroundImageItems_.isEmpty()) {
                continue;
            }
        }

        QGraphicsPixmapItem *item = backgroundImageItems_.last();
        item->setVisible(layerObject.value(QStringLiteral("visible")).toBool(true));
        const qreal opacity = layerObject.value(QStringLiteral("opacity")).toDouble(0.58);
        item->setOpacity(qBound(0.05, opacity, 1.0));
        if (hasMetadata && metadataReference->hasBasePosition && !metadataReference->xviReference) {
            placeRasterLayerFromMetadata(item,
                                         *metadataReference,
                                         areaAdjust,
                                         sourceBounds,
                                         previewBounds);
            item->setData(4, true);
        } else {
            const qreal layerX = layerObject.value(QStringLiteral("x")).toDouble(item->pos().x());
            const qreal layerY = layerObject.value(QStringLiteral("y")).toDouble(item->pos().y());
            item->setPos(layerX, layerY);
        }
        const qreal gamma = layerObject.value(QStringLiteral("gamma")).toDouble(1.0);
        applyBackgroundLayerGamma(item, qBound(0.2, gamma, 2.5));
    }

    applyBackgroundLayerStackingOrder();
    setSelectedBackgroundLayerIndexInternal(0);
    refreshBackgroundLayerControls();
}

void MapEditorTab::loadBackgroundLayersFromDocumentMetadata()
{
    if (textEditor_ == nullptr) {
        return;
    }

    syncAutoBackgroundLayersFromCurrentDocument();
}

void MapEditorTab::syncAutoBackgroundLayersFromCurrentDocument()
{
    if (mapScene_ == nullptr || textEditor_ == nullptr) {
        return;
    }

    const QVector<XtherionBackgroundReference> references = parseXtherionBackgroundReferences(textEditor_->text(), filePath());
    if (references.isEmpty()) {
        return;
    }
    const XtherionAreaAdjust areaAdjust = parseXtherionAreaAdjust(textEditor_->text());
    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(textEditor_->text());
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const QRectF sourceBounds = geometryBoundsForFeatures(features);
    const QRectF previewBounds = mapPreviewBounds();

    QHash<QString, XtherionBackgroundReference> metadataByPath;
    QHash<QString, QVector<XtherionBackgroundReference>> metadataByFileName;
    for (const XtherionBackgroundReference &reference : references) {
        const QString pathKey = normalizedPathKey(reference.absolutePath);
        if (!pathKey.isEmpty()) {
            metadataByPath.insert(pathKey, reference);
        }
        const QString fileNameKey = QFileInfo(reference.absolutePath).fileName().trimmed().toCaseFolded();
        if (!fileNameKey.isEmpty()) {
            metadataByFileName[fileNameKey].append(reference);
        }
    }

    QSet<QString> existingLayerPaths;
    for (QGraphicsPixmapItem *existingLayer : std::as_const(backgroundImageItems_)) {
        if (existingLayer == nullptr) {
            continue;
        }
        const QString layerPath = QFileInfo(existingLayer->data(0).toString()).absoluteFilePath();
        const QString pathKey = normalizedPathKey(layerPath);
        if (!pathKey.isEmpty()) {
            existingLayerPaths.insert(pathKey);
        }

        const XtherionBackgroundReference *existingMetadata =
            findMetadataReferenceForPath(layerPath, metadataByPath, metadataByFileName);
        if (existingMetadata != nullptr && existingMetadata->hasBasePosition && !existingMetadata->xviReference) {
            placeRasterLayerFromMetadata(existingLayer,
                                         *existingMetadata,
                                         areaAdjust,
                                         sourceBounds,
                                         previewBounds);
            existingLayer->setData(4, true);
        }
    }

    int addedCount = 0;
    for (const XtherionBackgroundReference &reference : references) {
        const QString referencePath = QFileInfo(reference.absolutePath).absoluteFilePath();
        const QString referencePathKey = normalizedPathKey(referencePath);
        if (referencePath.isEmpty() || !QFileInfo::exists(referencePath) || existingLayerPaths.contains(referencePathKey)) {
            continue;
        }

        if (reference.xviReference) {
            XviDocument xviDocument;
            if (!parseXviDocument(referencePath, &xviDocument)) {
                continue;
            }

            const QPointF anchoredBase = reference.hasBasePosition ? reference.basePosition : QPointF(0.0, 0.0);
            QPixmap pixmap;
            QPointF topLeft;
            if (!buildXviLayerPixmap(xviDocument,
                                     anchoredBase,
                                     reference.rootStationName,
                                     sourceBounds,
                                     previewBounds,
                                     &pixmap,
                                     &topLeft)) {
                continue;
            }

            auto *backgroundItem = new QGraphicsPixmapItem(pixmap);
            backgroundItem->setTransformationMode(Qt::SmoothTransformation);
            backgroundItem->setOpacity(0.58);
            backgroundItem->setData(0, referencePath);
            backgroundItem->setData(2, 1.0);
            backgroundItem->setData(4, true);
            backgroundItem->setToolTip(QFileInfo(referencePath).fileName());
            backgroundItem->setPos(topLeft);
            mapScene_->addItem(backgroundItem);
            backgroundImageItems_.append(backgroundItem);
            applyBackgroundLayerStackingOrder();
            setSelectedBackgroundLayerIndexInternal(backgroundImageItems_.size() - 1);
            refreshBackgroundLayerControls();
            if (!referencePathKey.isEmpty()) {
                existingLayerPaths.insert(referencePathKey);
            }
            ++addedCount;
            continue;
        }

        const int previousCount = backgroundImageItems_.size();
        addBackgroundImage(referencePath);
        if (backgroundImageItems_.size() <= previousCount) {
            continue;
        }

        QGraphicsPixmapItem *item = backgroundImageItems_.last();
        if (item == nullptr) {
            continue;
        }

        item->setData(4, true);
        if (reference.hasBasePosition && sourceBounds.isValid() && previewBounds.isValid()) {
            placeRasterLayerFromMetadata(item,
                                         reference,
                                         areaAdjust,
                                         sourceBounds,
                                         previewBounds);
        }

        if (!referencePathKey.isEmpty()) {
            existingLayerPaths.insert(referencePathKey);
        }
        ++addedCount;
    }

    if (addedCount > 0) {
        toolbarStatusNote_ = tr("Auto-loaded %1 background layer(s) from xth_me_image_insert metadata.").arg(addedCount);
        saveBackgroundLayersToSession();
        refreshToolbarSummary();
    }
}

void MapEditorTab::addBackgroundImage(const QString &imagePath)
{
    if (mapScene_ == nullptr || imagePath.isEmpty()) {
        return;
    }

    QImageReader imageReader(imagePath);
    imageReader.setAutoTransform(true);
    const QImage image = imageReader.read();
    if (image.isNull()) {
        return;
    }

    const QRectF previewBounds = mapPreviewBounds();
    if (!previewBounds.isValid() || previewBounds.width() < 2.0 || previewBounds.height() < 2.0) {
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    if (pixmap.isNull()) {
        return;
    }

    QSize targetSize = previewBounds.size().toSize();
    targetSize.setWidth(qMax(targetSize.width(), 2));
    targetSize.setHeight(qMax(targetSize.height(), 2));
    pixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    auto *backgroundItem = new QGraphicsPixmapItem(pixmap);
    backgroundItem->setTransformationMode(Qt::SmoothTransformation);
    backgroundItem->setOpacity(0.58);
    backgroundItem->setData(0, QFileInfo(imagePath).absoluteFilePath());
    backgroundItem->setData(2, 1.0);
    backgroundItem->setToolTip(QFileInfo(imagePath).fileName());

    const QPointF topLeft(previewBounds.center().x() - (pixmap.width() / 2.0),
                          previewBounds.center().y() - (pixmap.height() / 2.0));
    backgroundItem->setPos(topLeft);
    mapScene_->addItem(backgroundItem);
    backgroundImageItems_.append(backgroundItem);
    applyBackgroundLayerStackingOrder();
    setSelectedBackgroundLayerIndexInternal(backgroundImageItems_.size() - 1);
    refreshBackgroundLayerControls();
}

void MapEditorTab::applyBackgroundLayerGamma(QGraphicsPixmapItem *item, qreal gamma)
{
    if (item == nullptr) {
        return;
    }

    const QString layerPath = item->data(0).toString();
    if (layerPath.isEmpty()) {
        return;
    }

    QImageReader imageReader(layerPath);
    imageReader.setAutoTransform(true);
    QImage sourceImage = imageReader.read();
    if (sourceImage.isNull()) {
        return;
    }

    sourceImage = sourceImage.convertToFormat(QImage::Format_RGBA8888);
    const qreal boundedGamma = qBound(0.2, gamma, 2.5);
    unsigned char lookupTable[256];
    for (int value = 0; value < 256; ++value) {
        const qreal normalized = static_cast<qreal>(value) / 255.0;
        const qreal corrected = std::pow(normalized, 1.0 / boundedGamma);
        lookupTable[value] = static_cast<unsigned char>(qBound(0, qRound(corrected * 255.0), 255));
    }

    for (int y = 0; y < sourceImage.height(); ++y) {
        uchar *scanLine = sourceImage.scanLine(y);
        for (int x = 0; x < sourceImage.width(); ++x) {
            uchar *pixel = scanLine + (x * 4);
            pixel[0] = lookupTable[pixel[0]];
            pixel[1] = lookupTable[pixel[1]];
            pixel[2] = lookupTable[pixel[2]];
        }
    }

    const QRectF currentBounds = item->boundingRect();
    const QSize targetSize = currentBounds.size().toSize();
    QImage scaledImage = sourceImage;
    if (targetSize.width() > 1 && targetSize.height() > 1) {
        scaledImage = sourceImage.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    item->setPixmap(QPixmap::fromImage(scaledImage));
    item->setData(2, boundedGamma);
}

}
