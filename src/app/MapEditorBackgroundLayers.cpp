#include "MapEditorTab.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QRegularExpression>
#include <QSet>

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
    QPointF anchor;
    bool hasAnchor = false;
};

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

        XtherionBackgroundReference reference;
        reference.absolutePath = absolutePath;
        const QStringList anchorParts = secondGroup.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (anchorParts.size() >= 2) {
            bool xOk = false;
            bool yOk = false;
            const qreal x = anchorParts.at(0).toDouble(&xOk);
            const qreal y = anchorParts.at(1).toDouble(&yOk);
            if (xOk && yOk) {
                reference.anchor = QPointF(x, y);
                reference.hasAnchor = true;
            }
        }
        references.append(reference);
    }

    return references;
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

    for (const QJsonValue &value : layersValue.toArray()) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject layerObject = value.toObject();
        const QString layerPath = layerObject.value(QStringLiteral("path")).toString();
        if (layerPath.isEmpty() || !QFileInfo::exists(layerPath)) {
            continue;
        }

        addBackgroundImage(layerPath);
        if (backgroundImageItems_.isEmpty()) {
            continue;
        }

        QGraphicsPixmapItem *item = backgroundImageItems_.last();
        item->setVisible(layerObject.value(QStringLiteral("visible")).toBool(true));
        const qreal opacity = layerObject.value(QStringLiteral("opacity")).toDouble(0.58);
        item->setOpacity(qBound(0.05, opacity, 1.0));
        const qreal layerX = layerObject.value(QStringLiteral("x")).toDouble(item->pos().x());
        const qreal layerY = layerObject.value(QStringLiteral("y")).toDouble(item->pos().y());
        item->setPos(layerX, layerY);
        const qreal gamma = layerObject.value(QStringLiteral("gamma")).toDouble(1.0);
        applyBackgroundLayerGamma(item, qBound(0.2, gamma, 2.5));
    }

    setSelectedBackgroundLayerIndexInternal(0);
    refreshBackgroundLayerControls();
}

void MapEditorTab::loadBackgroundLayersFromDocumentMetadata()
{
    if (!backgroundImageItems_.isEmpty() || textEditor_ == nullptr) {
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

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(textEditor_->text());
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    const QRectF sourceBounds = geometryBoundsForFeatures(features);
    const QRectF previewBounds = mapPreviewBounds();

    QSet<QString> existingLayerPaths;
    for (QGraphicsPixmapItem *existingLayer : std::as_const(backgroundImageItems_)) {
        if (existingLayer == nullptr) {
            continue;
        }
        const QString layerPath = QFileInfo(existingLayer->data(0).toString()).absoluteFilePath();
        if (!layerPath.isEmpty()) {
            existingLayerPaths.insert(layerPath);
        }
    }

    int addedCount = 0;
    for (const XtherionBackgroundReference &reference : references) {
        const QString referencePath = QFileInfo(reference.absolutePath).absoluteFilePath();
        if (referencePath.isEmpty() || !QFileInfo::exists(referencePath) || existingLayerPaths.contains(referencePath)) {
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

        if (reference.hasAnchor && sourceBounds.isValid() && previewBounds.isValid() && sourceBounds.width() > 0.001 && sourceBounds.height() > 0.001) {
            const QPointF previewAnchor = mapGeometryPointToPreview(reference.anchor, sourceBounds, previewBounds);
            const QRectF bounds = item->boundingRect();
            item->setPos(previewAnchor - QPointF(bounds.width() / 2.0, bounds.height() / 2.0));
        }

        existingLayerPaths.insert(referencePath);
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
        scaledImage = sourceImage.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    item->setPixmap(QPixmap::fromImage(scaledImage));
    item->setData(2, boundedGamma);
}

}
