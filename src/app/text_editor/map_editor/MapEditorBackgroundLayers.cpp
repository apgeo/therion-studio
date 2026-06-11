#include "MapEditorTab.h"

#include <QColor>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QFormLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QSet>
#include <QSpinBox>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QtMath>

#include <cmath>
#include <optional>

#include "MapEditorRasterBackgroundImage.h"
#include "MapEditorRasterBackgroundPlacement.h"
#include "MapEditorSceneSupport.h"
#include "../TextEditorTab.h"
#include "../../../core/MapBackgroundPlacement.h"
#include "../../../core/ISessionStore.h"
#include "../../../core/PocketTopoImport.h"
#include "../../../core/TherionBackgroundMetadata.h"
#include "../../../core/TherionCommandLineModel.h"
#include "../../../core/TherionDocumentParser.h"
#include "../../../core/TherionSourceText.h"
#include "../../../core/TherionTokenRules.h"
#include "../../../core/TherionXviParser.h"
#include "MapEditorXviBackgroundItem.h"

namespace TherionStudio
{
namespace
{
using XtherionBackgroundReference = TherionBackgroundReference;
using XviDocument = TherionXviDocument;

using XtherionAreaAdjust = TherionAreaAdjust;

constexpr int kBackgroundLayerXviGeometryKeyRole = 100;
constexpr int kBackgroundLayerUserVisibilityRole = 101;
constexpr int kBackgroundLayerSourceImageRole = 102;
constexpr int kBackgroundLayerXviExpectedTopLeftRole = 103;
constexpr int kBackgroundLayerRasterGammaRequestRole = 104;
constexpr int kBackgroundLayerRasterLoadRequestRole = 105;
constexpr int kBackgroundLayerXviBasePositionRole = 106;
constexpr int kBackgroundLayerXviRootStationRole = 107;
constexpr qreal kDefaultXviLayerOpacity = 1.0;
constexpr qreal kDefaultRasterLayerOpacity = 0.58;

struct CachedXviDocumentEntry
{
    QByteArray contentHash;
    XviDocument document;
};

struct XviSketchStrokeStyle
{
    QColor color;
    Qt::PenStyle penStyle = Qt::SolidLine;
};

struct PocketTopoGeneratedXvi
{
    QString path;
    XviDocument document;
};

struct XviBackgroundInsertionPlacement
{
    QPointF basePosition;
    QString rootStationName;
};

XviSketchStrokeStyle xviSketchStrokeStyleForToken(const QString &token)
{
    const QString normalizedToken = token.trimmed().toLower();
    if (normalizedToken == QStringLiteral("connect")) {
        return XviSketchStrokeStyle{QColor(102, 102, 102, 94), Qt::DotLine};
    }

    QColor parsedColor(normalizedToken);
    if (!parsedColor.isValid()) {
        parsedColor = QColor(0, 0, 0, 200);
    } else if (parsedColor.alpha() <= 0) {
        parsedColor.setAlpha(200);
    } else if (parsedColor.alpha() > 220) {
        parsedColor.setAlpha(220);
    }

    return XviSketchStrokeStyle{parsedColor, Qt::SolidLine};
}

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

QVector<XtherionBackgroundReference> parseXtherionBackgroundReferences(const QString &documentText, const QString &documentPath)
{
    return parseTherionBackgroundReferences(documentText, documentPath);
}

XtherionAreaAdjust parseXtherionAreaAdjust(const QString &documentText)
{
    return parseTherionAreaAdjust(documentText);
}

bool hasUserVisibilityOverride(const QGraphicsPixmapItem *item)
{
    return item != nullptr && item->data(kBackgroundLayerUserVisibilityRole).toBool();
}

void setBackgroundLayerVisibleFromMetadata(QGraphicsPixmapItem *item, bool visible)
{
    if (item == nullptr || hasUserVisibilityOverride(item)) {
        return;
    }

    item->setVisible(visible);
}

void setBackgroundLayerVisibleFromUser(QGraphicsPixmapItem *item, bool visible)
{
    if (item == nullptr) {
        return;
    }

    item->setVisible(visible);
    item->setData(kBackgroundLayerUserVisibilityRole, true);
}

QString quantizedNumberToken(qreal value)
{
    return QString::number(qRound64(value * 1000.0));
}

QString xviGeometryCacheKey(const QString &absolutePath,
                            const QPointF &anchoredBasePosition,
                            const QString &rootStationName,
                            const QRectF &modelBounds,
                            const QRectF &previewBounds)
{
    return QStringLiteral("xvi-geometry-v4|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11")
        .arg(normalizedPathKey(absolutePath),
             rootStationName.normalized(QString::NormalizationForm_C),
             quantizedNumberToken(anchoredBasePosition.x()),
             quantizedNumberToken(anchoredBasePosition.y()),
             quantizedNumberToken(modelBounds.left()),
             quantizedNumberToken(modelBounds.top()),
             quantizedNumberToken(modelBounds.right()),
             quantizedNumberToken(modelBounds.bottom()),
             quantizedNumberToken(previewBounds.left()),
             quantizedNumberToken(previewBounds.top()),
             QStringLiteral("%1x%2")
                 .arg(quantizedNumberToken(previewBounds.width()),
                      quantizedNumberToken(previewBounds.height())));
}

bool parseXviDocumentFileCached(const QString &absolutePath, XviDocument *document)
{
    if (document == nullptr || absolutePath.isEmpty()) {
        return false;
    }

    static QHash<QString, CachedXviDocumentEntry> cache;

    QFileInfo fileInfo(absolutePath);
    if (!fileInfo.exists()) {
        return false;
    }

    const QString pathKey = normalizedPathKey(fileInfo.absoluteFilePath());
    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QByteArray fileBytes = file.readAll();
    const QByteArray contentHash = QCryptographicHash::hash(fileBytes, QCryptographicHash::Sha256);

    const auto cacheIt = cache.constFind(pathKey);
    if (cacheIt != cache.constEnd() && cacheIt->contentHash == contentHash) {
        *document = cacheIt->document;
        return true;
    }

    XviDocument parsed;
    if (!parseTherionXviDocumentText(QString::fromUtf8(fileBytes), &parsed)) {
        return false;
    }

    cache.insert(pathKey, CachedXviDocumentEntry{contentHash, parsed});
    *document = parsed;
    return true;
}

void cacheRasterSourceImage(QGraphicsPixmapItem *item, const QImage &sourceImage)
{
    if (item == nullptr || sourceImage.isNull()) {
        return;
    }

    item->setData(kBackgroundLayerSourceImageRole, QVariant::fromValue(sourceImage));
}

QImage rasterSourceImageForItem(QGraphicsPixmapItem *item)
{
    if (item == nullptr) {
        return QImage();
    }

    const QVariant cachedValue = item->data(kBackgroundLayerSourceImageRole);
    if (cachedValue.canConvert<QImage>()) {
        const QImage cachedImage = cachedValue.value<QImage>();
        if (!cachedImage.isNull()) {
            return cachedImage;
        }
    }

    const QImage sourceImage = readMapEditorRasterSourceImage(item->data(0).toString());
    cacheRasterSourceImage(item, sourceImage);
    return sourceImage;
}

QString formatXtherionNumber(qreal value)
{
    if (std::fabs(value) < 1e-9) {
        return QStringLiteral("0");
    }

    const qreal nearestInteger = std::round(value);
    if (std::fabs(value - nearestInteger) < 1e-9) {
        return QString::number(static_cast<qlonglong>(nearestInteger));
    }

    return QString::number(value, 'g', 15);
}

QString xtherionPathToken(const QString &absolutePath, const QString &documentPath)
{
    QString path = absolutePath;
    const QString baseDirectory = QFileInfo(documentPath).absolutePath();
    if (!baseDirectory.isEmpty()) {
        path = QDir(baseDirectory).relativeFilePath(absolutePath);
    }
    path = QDir::fromNativeSeparators(path);
    if (path.contains(QRegularExpression(QStringLiteral(R"(\s|[{}])")))) {
        QString escaped = path;
        escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        escaped.replace(QLatin1Char('{'), QStringLiteral("\\{"));
        escaped.replace(QLatin1Char('}'), QStringLiteral("\\}"));
        return QStringLiteral("{%1}").arg(escaped);
    }
    return path;
}

QString xtherionMetadataToken(QString token)
{
    token = token.trimmed();
    if (token.isEmpty()) {
        return QStringLiteral("{}");
    }

    if (token.contains(QRegularExpression(QStringLiteral(R"(\s|[{}])")))) {
        token.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        token.replace(QLatin1Char('{'), QStringLiteral("\\{"));
        token.replace(QLatin1Char('}'), QStringLiteral("\\}"));
        return QStringLiteral("{%1}").arg(token);
    }
    return token;
}

QString xtherionImageInsertLine(const QString &absolutePath,
                                const QString &documentPath,
                                const QPointF &basePosition,
                                bool visible,
                                qreal gamma,
                                const QString &rootStationName = QString())
{
    return QStringLiteral("##XTHERION## xth_me_image_insert {%1 %2 %3} {%4 %5} %6 0 {}")
        .arg(formatXtherionNumber(basePosition.x()),
             visible ? QStringLiteral("1") : QStringLiteral("0"),
             formatXtherionNumber(qBound<qreal>(0.2, gamma, 2.5)),
             formatXtherionNumber(basePosition.y()),
             xtherionMetadataToken(rootStationName),
             xtherionPathToken(absolutePath, documentPath));
}

QString uniquePocketTopoXviPath(const QString &pocketTopoPath, PocketTopoProjection projection)
{
    const QFileInfo sourceInfo(pocketTopoPath);
    const QString basePath = sourceInfo.dir().absoluteFilePath(
        QStringLiteral("%1_%2").arg(sourceInfo.completeBaseName(), pocketTopoProjectionSuffix(projection)));
    QString candidatePath = QStringLiteral("%1.xvi").arg(basePath);
    int suffix = 0;
    while (QFileInfo::exists(candidatePath)) {
        candidatePath = QStringLiteral("%1%2.xvi").arg(basePath).arg(suffix);
        ++suffix;
    }
    return candidatePath;
}

bool requestPocketTopoXviOptions(QWidget *parent, PocketTopoXviImportOptions *options)
{
    if (options == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QCoreApplication::translate("TherionStudio::MapEditorTab", "XVI Properties"));

    auto *layout = new QVBoxLayout(&dialog);
    auto *formLayout = new QFormLayout;
    layout->addLayout(formLayout);

    auto *scaleSpin = new QSpinBox(&dialog);
    scaleSpin->setRange(1, 100000);
    scaleSpin->setValue(options->scale);
    scaleSpin->setPrefix(QStringLiteral("1 : "));
    formLayout->addRow(QCoreApplication::translate("TherionStudio::MapEditorTab", "Scale"), scaleSpin);

    auto *resolutionSpin = new QSpinBox(&dialog);
    resolutionSpin->setRange(1, 2400);
    resolutionSpin->setValue(options->resolutionDpi);
    resolutionSpin->setSuffix(QStringLiteral(" dpi"));
    formLayout->addRow(QCoreApplication::translate("TherionStudio::MapEditorTab", "Resolution"), resolutionSpin);

    auto *gridSpin = new QDoubleSpinBox(&dialog);
    gridSpin->setRange(0.01, 1000.0);
    gridSpin->setDecimals(2);
    gridSpin->setValue(options->gridSpacingMeters);
    gridSpin->setSuffix(QStringLiteral(" m"));
    formLayout->addRow(QCoreApplication::translate("TherionStudio::MapEditorTab", "Grid spacing"), gridSpin);

    auto *planRadio = new QRadioButton(QCoreApplication::translate("TherionStudio::MapEditorTab", "Plan"), &dialog);
    auto *elevationRadio = new QRadioButton(QCoreApplication::translate("TherionStudio::MapEditorTab", "Extended elevation"), &dialog);
    planRadio->setChecked(options->projection == PocketTopoProjection::Plan);
    elevationRadio->setChecked(options->projection == PocketTopoProjection::Elevation);
    layout->addWidget(planRadio);
    layout->addWidget(elevationRadio);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    options->scale = scaleSpin->value();
    options->resolutionDpi = resolutionSpin->value();
    options->gridSpacingMeters = gridSpin->value();
    options->projection = elevationRadio->isChecked() ? PocketTopoProjection::Elevation : PocketTopoProjection::Plan;
    return true;
}

std::optional<PocketTopoGeneratedXvi> generatePocketTopoXvi(QWidget *parent,
                                                           const QString &pocketTopoPath,
                                                           PocketTopoXviImportOptions *options,
                                                           QString *errorMessage)
{
    if (options == nullptr || pocketTopoPath.isEmpty()) {
        return std::nullopt;
    }
    if (!requestPocketTopoXviOptions(parent, options)) {
        return std::nullopt;
    }

    QFile inputFile(pocketTopoPath);
    if (!inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::MapEditorTab",
                                                        "Could not read %1.")
                                .arg(QDir::toNativeSeparators(pocketTopoPath));
        }
        return std::nullopt;
    }

    bool hasProjectedData = false;
    const QString xviText = convertPocketTopoTextToXvi(QString::fromUtf8(inputFile.readAll()),
                                                       *options,
                                                       &hasProjectedData);
    if (!hasProjectedData || xviText.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::MapEditorTab",
                                                        "No PocketTopo %1 data was found in %2.")
                                .arg(options->projection == PocketTopoProjection::Elevation
                                         ? QCoreApplication::translate("TherionStudio::MapEditorTab", "extended elevation")
                                         : QCoreApplication::translate("TherionStudio::MapEditorTab", "plan"),
                                     QDir::toNativeSeparators(pocketTopoPath));
        }
        return std::nullopt;
    }

    const QString xviPath = uniquePocketTopoXviPath(pocketTopoPath, options->projection);
    QFile outputFile(xviPath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::MapEditorTab",
                                                        "Could not write %1.")
                                .arg(QDir::toNativeSeparators(xviPath));
        }
        return std::nullopt;
    }
    outputFile.write(xviText.toUtf8());
    outputFile.close();

    XviDocument document;
    if (!parseTherionXviDocumentText(xviText, &document)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::MapEditorTab",
                                                        "Generated XVI could not be parsed: %1.")
                                .arg(QDir::toNativeSeparators(xviPath));
        }
        return std::nullopt;
    }

    return PocketTopoGeneratedXvi{QFileInfo(xviPath).absoluteFilePath(), document};
}

QString canonicalStationToken(QString token)
{
    token = token.trimmed();
    while (token.endsWith(QLatin1Char('.'))) {
        token.chop(1);
    }
    return token;
}

QString unqualifiedStationToken(const QString &token)
{
    const QString canonical = canonicalStationToken(token);
    const int namespaceSeparator = canonical.indexOf(QLatin1Char('@'));
    if (namespaceSeparator <= 0) {
        return canonical;
    }
    return canonical.left(namespaceSeparator);
}

QString pointTypeTokenFromParsedLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive != QStringLiteral("point")) {
        return QString();
    }

    int numericCoordinateTokens = 0;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (TherionTokenRules::tokenStartsOption(token)) {
            break;
        }
        if (TherionTokenRules::isNumericToken(token)) {
            ++numericCoordinateTokens;
            continue;
        }
        if (numericCoordinateTokens < 2) {
            continue;
        }
        return token.toLower();
    }

    return QString();
}

QString stationNameFromPointLine(const TherionParsedLine &parsedLine)
{
    const QString optionName = commandOptionValue(parsedLine.tokens, QStringLiteral("-name")).trimmed();
    if (!optionName.isEmpty()) {
        return optionName;
    }

    if (pointTypeTokenFromParsedLine(parsedLine) != QStringLiteral("station")) {
        return QString();
    }

    bool sawStationType = false;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (TherionTokenRules::tokenStartsOption(token)) {
            break;
        }
        if (!sawStationType) {
            if (token.toLower() == QStringLiteral("station")) {
                sawStationType = true;
            }
            continue;
        }
        if (!TherionTokenRules::isNumericToken(token)) {
            return token;
        }
    }
    return QString();
}

std::optional<QPointF> pointPositionFromParsedLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive != QStringLiteral("point")) {
        return std::nullopt;
    }

    QVector<qreal> coordinates;
    coordinates.reserve(2);
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (TherionTokenRules::tokenStartsOption(token)) {
            break;
        }
        if (!TherionTokenRules::isNumericToken(token)) {
            continue;
        }
        bool ok = false;
        const qreal value = token.toDouble(&ok);
        if (!ok) {
            continue;
        }
        coordinates.append(value);
        if (coordinates.size() == 2) {
            return QPointF(coordinates.at(0), coordinates.at(1));
        }
    }
    return std::nullopt;
}

const TherionXviStation *matchingXviStation(const XviDocument &xviDocument, const QString &stationName)
{
    const QString requested = stationName.trimmed();
    if (requested.isEmpty()) {
        return nullptr;
    }

    for (const TherionXviStation &station : xviDocument.stationEntries) {
        if (station.name == requested) {
            return &station;
        }
    }

    const QString canonicalRequested = canonicalStationToken(requested);
    for (const TherionXviStation &station : xviDocument.stationEntries) {
        if (canonicalStationToken(station.name) == canonicalRequested) {
            return &station;
        }
    }

    const QString unqualifiedRequested = unqualifiedStationToken(requested);
    const TherionXviStation *match = nullptr;
    for (const TherionXviStation &station : xviDocument.stationEntries) {
        if (unqualifiedStationToken(station.name) != unqualifiedRequested) {
            continue;
        }
        if (match != nullptr) {
            return nullptr;
        }
        match = &station;
    }
    return match;
}

XviBackgroundInsertionPlacement pocketTopoXviInsertionPlacement(const XviDocument &xviDocument,
                                                                const QString &documentText)
{
    for (const QString &line : TherionSourceText::splitTextLines(documentText)) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(line);
        if (pointTypeTokenFromParsedLine(parsedLine) != QStringLiteral("station")) {
            continue;
        }

        const QString stationName = stationNameFromPointLine(parsedLine);
        const TherionXviStation *xviStation = matchingXviStation(xviDocument, stationName);
        if (xviStation == nullptr) {
            continue;
        }

        const std::optional<QPointF> position = pointPositionFromParsedLine(parsedLine);
        if (!position.has_value()) {
            continue;
        }

        return XviBackgroundInsertionPlacement{position.value(), xviStation->name};
    }

    if (!xviDocument.stationEntries.isEmpty()) {
        const TherionXviStation &station = xviDocument.stationEntries.constFirst();
        return XviBackgroundInsertionPlacement{QPointF(0.0, 0.0), station.name};
    }

    return XviBackgroundInsertionPlacement{QPointF(0.0, 0.0), QString()};
}

QString backgroundImageDialogInitialDirectory(const QString &documentPath, const QString &projectRootPath)
{
    if (!documentPath.trimmed().isEmpty()) {
        const QString documentDirectory = QFileInfo(documentPath).absolutePath();
        if (!documentDirectory.isEmpty() && QDir(documentDirectory).exists()) {
            return documentDirectory;
        }
    }

    if (!projectRootPath.trimmed().isEmpty()) {
        const QString projectDirectory = QDir(projectRootPath).absolutePath();
        if (!projectDirectory.isEmpty() && QDir(projectDirectory).exists()) {
            return projectDirectory;
        }
    }

    return QString();
}

QRectF xviPlacedModelBounds(const XviDocument &xviDocument,
                            const XviBackgroundInsertionPlacement &placement)
{
    XviPlacementMetadata metadata;
    metadata.basePosition = placement.basePosition;
    metadata.hasBasePosition = true;
    metadata.rootStationName = placement.rootStationName;

    QVector<XviStationPlacementEntry> stationEntries;
    stationEntries.reserve(xviDocument.stationEntries.size());
    for (const TherionXviStation &station : xviDocument.stationEntries) {
        stationEntries.append(XviStationPlacementEntry{station.name, station.position});
    }

    const XviPlacementResult placementResult = stationEntries.isEmpty()
        ? resolveXviModelOffset(xviDocument.gridOrigin, xviDocument.stations, metadata)
        : resolveXviModelOffset(xviDocument.gridOrigin, stationEntries, metadata);
    const QPointF offset = placementResult.modelOffset;

    QRectF bounds;
    bool hasBounds = false;
    auto includePoint = [&](const QPointF &point) {
        const QRectF pointRect(point, QSizeF(1.0, 1.0));
        if (!hasBounds) {
            bounds = pointRect;
            hasBounds = true;
            return;
        }
        bounds = bounds.united(pointRect);
    };

    if (xviDocument.hasGridDefinition) {
        const int spanX = qMax(0, xviDocument.gridCountX);
        const int spanY = qMax(0, xviDocument.gridCountY);
        const QPointF gridP00 = xviDocument.gridOrigin + offset;
        includePoint(gridP00);
        includePoint(gridP00 + (xviDocument.gridVectorX * spanX));
        includePoint(gridP00 + (xviDocument.gridVectorY * spanY));
        includePoint(gridP00 + (xviDocument.gridVectorX * spanX) + (xviDocument.gridVectorY * spanY));
    }
    for (const QLineF &shot : xviDocument.shots) {
        includePoint(shot.p1() + offset);
        includePoint(shot.p2() + offset);
    }
    for (const auto &line : xviDocument.sketchLines) {
        for (const QPointF &point : line.points) {
            includePoint(point + offset);
        }
    }
    for (const TherionXviStation &station : xviDocument.stationEntries) {
        includePoint(station.position + offset);
    }

    if (!hasBounds || !bounds.isValid()) {
        return QRectF();
    }
    return bounds.normalized().adjusted(-128.0, -128.0, 128.0, 128.0);
}

int insertionIndexAfterEncoding(const QStringList &lines)
{
    for (int index = 0; index < lines.size(); ++index) {
        QString line = lines.at(index);
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        if (line.trimmed().startsWith(QStringLiteral("encoding "))) {
            return index + 1;
        }
    }
    return 0;
}

int insertionIndexForXtherionImageMetadata(const QStringList &lines)
{
    int insertionIndex = 0;
    for (int index = 0; index < lines.size(); ++index) {
        QString line = lines.at(index);
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("encoding "))
            || trimmed.startsWith(QStringLiteral("##XTHERION##"))) {
            insertionIndex = index + 1;
            continue;
        }
        break;
    }
    return insertionIndex;
}

QString upsertXtherionSimpleCommandLine(const QString &documentText,
                                        const QString &command,
                                        const QString &metadataLine)
{
    const QString lineEnding = TherionSourceText::detectedLineEnding(documentText);
    QStringList lines = TherionSourceText::splitTextLines(documentText);
    if (!lines.isEmpty() && lines.last().isEmpty()) {
        lines.removeLast();
    }

    for (int index = 0; index < lines.size(); ++index) {
        if (lines.at(index).contains(QStringLiteral("##XTHERION##")) && lines.at(index).contains(command)) {
            lines[index] = metadataLine;
            QString updated = lines.join(lineEnding);
            if (documentText.endsWith(QLatin1Char('\n')) || !updated.isEmpty()) {
                updated += lineEnding;
            }
            return updated;
        }
    }

    int insertionIndex = insertionIndexAfterEncoding(lines);
    if (command == QStringLiteral("xth_me_area_zoom_to")) {
        for (int index = 0; index < lines.size(); ++index) {
            if (lines.at(index).contains(QStringLiteral("##XTHERION##"))
                && lines.at(index).contains(QStringLiteral("xth_me_area_adjust"))) {
                insertionIndex = index + 1;
                break;
            }
        }
    }

    lines.insert(insertionIndex, metadataLine);
    QString updated = lines.join(lineEnding);
    if (documentText.endsWith(QLatin1Char('\n')) || !updated.isEmpty()) {
        updated += lineEnding;
    }
    return updated;
}

QString upsertXtherionImageMetadataLine(const QString &documentText,
                                        const QString &documentPath,
                                        const QString &absolutePath,
                                        const QString &metadataLine,
                                        bool remove)
{
    const QString lineEnding = TherionSourceText::detectedLineEnding(documentText);
    QStringList lines = TherionSourceText::splitTextLines(documentText);
    if (!lines.isEmpty() && lines.last().isEmpty()) {
        lines.removeLast();
    }

    const QVector<XtherionBackgroundReference> references = parseXtherionBackgroundReferences(documentText, documentPath);
    const QString targetPathKey = normalizedPathKey(absolutePath);
    int existingIndex = -1;
    for (const XtherionBackgroundReference &reference : references) {
        if (reference.lineNumber <= 0 || reference.lineNumber > lines.size()) {
            continue;
        }
        if (normalizedPathKey(reference.absolutePath) == targetPathKey) {
            existingIndex = reference.lineNumber - 1;
            break;
        }
    }

    if (existingIndex >= 0) {
        if (remove) {
            lines.removeAt(existingIndex);
        } else {
            lines[existingIndex] = metadataLine;
        }
    } else if (!remove) {
        lines.insert(insertionIndexForXtherionImageMetadata(lines), metadataLine);
    }

    QString updated = lines.join(lineEnding);
    if (documentText.endsWith(QLatin1Char('\n')) || !updated.isEmpty()) {
        updated += lineEnding;
    }
    return updated;
}

bool findFirstBracedGroupRange(const QString &line, int startPosition, int *openBraceIndex, int *closeBraceIndex)
{
    if (openBraceIndex == nullptr || closeBraceIndex == nullptr) {
        return false;
    }

    int position = startPosition;
    while (position < line.size() && line.at(position).isSpace()) {
        ++position;
    }
    if (position >= line.size() || line.at(position) != QLatin1Char('{')) {
        return false;
    }

    const int groupStart = position;
    int depth = 1;
    ++position;
    while (position < line.size() && depth > 0) {
        const QChar character = line.at(position);
        if (character == QLatin1Char('{')) {
            ++depth;
        } else if (character == QLatin1Char('}')) {
            --depth;
            if (depth == 0) {
                break;
            }
        }
        ++position;
    }

    if (position >= line.size() || line.at(position) != QLatin1Char('}')) {
        return false;
    }

    *openBraceIndex = groupStart;
    *closeBraceIndex = position;
    return true;
}

QString imageMetadataLineWithGamma(const QString &line, qreal gamma, bool *updated)
{
    if (updated != nullptr) {
        *updated = false;
    }

    const int keywordIndex = line.indexOf(QStringLiteral("xth_me_image_insert"));
    if (keywordIndex < 0) {
        return line;
    }

    int openBraceIndex = -1;
    int closeBraceIndex = -1;
    if (!findFirstBracedGroupRange(line,
                                   keywordIndex + QStringLiteral("xth_me_image_insert").size(),
                                   &openBraceIndex,
                                   &closeBraceIndex)) {
        return line;
    }

    const QString groupText = line.mid(openBraceIndex + 1, closeBraceIndex - openBraceIndex - 1).trimmed();
    QStringList tokens = groupText.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        return line;
    }
    if (tokens.size() == 1) {
        tokens.append(QStringLiteral("1"));
    }
    if (tokens.size() == 2) {
        tokens.append(formatXtherionNumber(qBound<qreal>(0.2, gamma, 2.5)));
    } else {
        tokens[2] = formatXtherionNumber(qBound<qreal>(0.2, gamma, 2.5));
    }

    const QString replacement = QStringLiteral("{%1}").arg(tokens.join(QLatin1Char(' ')));
    const QString result = line.left(openBraceIndex) + replacement + line.mid(closeBraceIndex + 1);
    if (updated != nullptr) {
        *updated = result != line;
    }
    return result;
}

QString updateExistingXtherionImageMetadataGamma(const QString &documentText,
                                                 const QString &documentPath,
                                                 const QString &absolutePath,
                                                 qreal gamma,
                                                 bool *updated)
{
    if (updated != nullptr) {
        *updated = false;
    }

    const QString lineEnding = TherionSourceText::detectedLineEnding(documentText);
    QStringList lines = TherionSourceText::splitTextLines(documentText);
    if (!lines.isEmpty() && lines.last().isEmpty()) {
        lines.removeLast();
    }

    const QVector<XtherionBackgroundReference> references = parseXtherionBackgroundReferences(documentText, documentPath);
    const QString targetPathKey = normalizedPathKey(absolutePath);
    if (targetPathKey.isEmpty()) {
        return documentText;
    }

    for (const XtherionBackgroundReference &reference : references) {
        if (reference.lineNumber <= 0 || reference.lineNumber > lines.size()) {
            continue;
        }
        if (normalizedPathKey(reference.absolutePath) != targetPathKey) {
            continue;
        }

        bool lineUpdated = false;
        lines[reference.lineNumber - 1] = imageMetadataLineWithGamma(lines.at(reference.lineNumber - 1),
                                                                      gamma,
                                                                      &lineUpdated);
        if (!lineUpdated) {
            return documentText;
        }

        QString result = lines.join(lineEnding);
        if (documentText.endsWith(QLatin1Char('\n')) || !result.isEmpty()) {
            result += lineEnding;
        }
        if (updated != nullptr) {
            *updated = true;
        }
        return result;
    }

    return documentText;
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

bool buildXviLayerGeometry(const XviDocument &xvi,
                           const QPointF &anchoredBasePosition,
                           const QString &rootStationName,
                           const QRectF &modelBounds,
                           const QRectF &previewBounds,
                           MapEditorXviLayerGeometryData *geometry,
                           QPointF *topLeft)
{
    if (geometry == nullptr || topLeft == nullptr || !xvi.hasGridOrigin || !previewBounds.isValid()) {
        return false;
    }

    XviPlacementMetadata placement;
    placement.basePosition = anchoredBasePosition;
    placement.hasBasePosition = true;
    placement.rootStationName = rootStationName;
    QVector<XviStationPlacementEntry> stationEntries;
    stationEntries.reserve(xvi.stationEntries.size());
    for (const TherionXviStation &station : xvi.stationEntries) {
        stationEntries.append(XviStationPlacementEntry{station.name, station.position});
    }
    const XviPlacementResult placementResult = stationEntries.isEmpty()
        ? resolveXviModelOffset(xvi.gridOrigin, xvi.stations, placement)
        : resolveXviModelOffset(xvi.gridOrigin, stationEntries, placement);
    const QPointF offset = placementResult.modelOffset;

    QRectF effectiveModelBounds = modelBounds;
    if (!effectiveModelBounds.isValid() || effectiveModelBounds.width() <= 0.0 || effectiveModelBounds.height() <= 0.0) {
        bool hasSourceBounds = false;
        QRectF sourceBounds;
        auto includeModelPoint = [&](const QPointF &point) {
            const QRectF pointRect(point, QSizeF(1.0, 1.0));
            if (!hasSourceBounds) {
                sourceBounds = pointRect;
                hasSourceBounds = true;
            } else {
                sourceBounds = sourceBounds.united(pointRect);
            }
        };

        if (xvi.hasGridDefinition) {
            const int spanX = qMax(0, xvi.gridCountX);
            const int spanY = qMax(0, xvi.gridCountY);
            const QPointF gridP00 = xvi.gridOrigin + offset;
            const QPointF gridP10 = gridP00 + (xvi.gridVectorX * spanX);
            const QPointF gridP01 = gridP00 + (xvi.gridVectorY * spanY);
            const QPointF gridP11 = gridP10 + (xvi.gridVectorY * spanY);
            includeModelPoint(gridP00);
            includeModelPoint(gridP10);
            includeModelPoint(gridP01);
            includeModelPoint(gridP11);
        }
        for (const QLineF &shot : xvi.shots) {
            includeModelPoint(shot.p1() + offset);
            includeModelPoint(shot.p2() + offset);
        }
        for (const auto &line : xvi.sketchLines) {
            for (const QPointF &point : line.points) {
                includeModelPoint(point + offset);
            }
        }

        if (hasSourceBounds) {
            effectiveModelBounds = sourceBounds.normalized().adjusted(-128.0, -128.0, 128.0, 128.0);
        }
    }
    if (!effectiveModelBounds.isValid() || effectiveModelBounds.width() <= 0.0 || effectiveModelBounds.height() <= 0.0) {
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

    QVector<QLineF> gridLines;
    if (xvi.hasGridDefinition) {
        const int spanX = qMax(0, xvi.gridCountX);
        const int spanY = qMax(0, xvi.gridCountY);
        const QPointF gridP00 = xvi.gridOrigin + offset;
        for (int xIndex = 0; xIndex <= spanX; ++xIndex) {
            const QPointF start = gridP00 + (xvi.gridVectorX * xIndex);
            const QPointF end = start + (xvi.gridVectorY * spanY);
            const QPointF projectedStart = mapEditorModelToPreviewPoint(start, effectiveModelBounds, previewBounds);
            const QPointF projectedEnd = mapEditorModelToPreviewPoint(end, effectiveModelBounds, previewBounds);
            includePoint(projectedStart);
            includePoint(projectedEnd);
            gridLines.append(QLineF(projectedStart, projectedEnd));
        }
        for (int yIndex = 0; yIndex <= spanY; ++yIndex) {
            const QPointF start = gridP00 + (xvi.gridVectorY * yIndex);
            const QPointF end = start + (xvi.gridVectorX * spanX);
            const QPointF projectedStart = mapEditorModelToPreviewPoint(start, effectiveModelBounds, previewBounds);
            const QPointF projectedEnd = mapEditorModelToPreviewPoint(end, effectiveModelBounds, previewBounds);
            includePoint(projectedStart);
            includePoint(projectedEnd);
            gridLines.append(QLineF(projectedStart, projectedEnd));
        }
    }

    auto stationKey = [](const QPointF &point) {
        const qint64 ix = qRound64(point.x() * 1000.0);
        const qint64 iy = qRound64(point.y() * 1000.0);
        return QStringLiteral("%1:%2").arg(ix).arg(iy);
    };
    QSet<QString> stationPointKeys;
    stationPointKeys.reserve(!xvi.stationEntries.isEmpty() ? xvi.stationEntries.size() : xvi.stations.size());
    if (!xvi.stationEntries.isEmpty()) {
        for (const TherionXviStation &station : xvi.stationEntries) {
            stationPointKeys.insert(stationKey(station.position));
        }
    } else {
        for (auto it = xvi.stations.constBegin(); it != xvi.stations.constEnd(); ++it) {
            stationPointKeys.insert(stationKey(it.value()));
        }
    }
    auto matchesStation = [&](const QPointF &point) {
        return stationPointKeys.contains(stationKey(point));
    };

    QVector<QLineF> traverseShotLines;
    QVector<QLineF> splayShotLines;
    for (const QLineF &shot : xvi.shots) {
        const QPointF rawStart = shot.p1();
        const QPointF rawEnd = shot.p2();
        const bool fromStation = matchesStation(rawStart);
        const bool toStation = matchesStation(rawEnd);
        const bool isSplay = fromStation != toStation;

        const QPointF projectedStart = mapEditorModelToPreviewPoint(rawStart + offset, effectiveModelBounds, previewBounds);
        const QPointF projectedEnd = mapEditorModelToPreviewPoint(rawEnd + offset, effectiveModelBounds, previewBounds);
        includePoint(projectedStart);
        includePoint(projectedEnd);
        QVector<QLineF> &targetLines = isSplay ? splayShotLines : traverseShotLines;
        targetLines.append(QLineF(projectedStart, projectedEnd));
    }

    QHash<QString, int> sketchPathIndexByStyle;
    QVector<MapEditorXviSketchPathData> sketchPaths;
    for (const auto &sketchLine : xvi.sketchLines) {
        const QVector<QPointF> &polyline = sketchLine.points;
        if (polyline.size() < 2) {
            continue;
        }

        const XviSketchStrokeStyle strokeStyle = xviSketchStrokeStyleForToken(sketchLine.colorToken);
        const QString styleKey = QStringLiteral("%1|%2|%3")
                                     .arg(strokeStyle.penStyle)
                                     .arg(strokeStyle.color.rgba())
                                     .arg(strokeStyle.color.alpha());
        int targetPathIndex = sketchPathIndexByStyle.value(styleKey, -1);
        if (targetPathIndex < 0) {
            MapEditorXviSketchPathData sketchPathData;
            sketchPathData.color = strokeStyle.color;
            sketchPathData.style = strokeStyle.penStyle;
            sketchPaths.append(sketchPathData);
            targetPathIndex = sketchPaths.size() - 1;
            sketchPathIndexByStyle.insert(styleKey, targetPathIndex);
        }
        QVector<QLineF> &targetLines = sketchPaths[targetPathIndex].lines;

        QPointF previousPoint = mapEditorModelToPreviewPoint(polyline.first() + offset, effectiveModelBounds, previewBounds);
        includePoint(previousPoint);
        for (int index = 1; index < polyline.size(); ++index) {
            const QPointF point = mapEditorModelToPreviewPoint(polyline.at(index) + offset, effectiveModelBounds, previewBounds);
            includePoint(point);
            targetLines.append(QLineF(previousPoint, point));
            previousPoint = point;
        }
    }

    bool hasSketchPaths = false;
    for (const MapEditorXviSketchPathData &sketchPath : sketchPaths) {
        if (!sketchPath.lines.isEmpty()) {
            hasSketchPaths = true;
            break;
        }
    }

    if (gridLines.isEmpty() && traverseShotLines.isEmpty() && splayShotLines.isEmpty() && !hasSketchPaths) {
        return false;
    }

    if (!hasBounds || !bounds.isValid()) {
        return false;
    }

    const qreal padding = 2.0;
    const QRectF layerBounds = bounds.adjusted(-padding, -padding, padding, padding);
    if (!layerBounds.isValid() || layerBounds.width() < 1.0 || layerBounds.height() < 1.0) {
        return false;
    }

    const QPointF layerTopLeft = layerBounds.topLeft();

    auto normalizedLines = [&](const QVector<QLineF> &sourceLines) {
        QVector<QLineF> lines;
        lines.reserve(sourceLines.size());
        for (const QLineF &line : sourceLines) {
            lines.append(QLineF(line.p1() - layerTopLeft, line.p2() - layerTopLeft));
        }
        return lines;
    };

    const QVector<QLineF> normalizedTraverseShots = normalizedLines(traverseShotLines);
    const QVector<QLineF> normalizedSplayShots = normalizedLines(splayShotLines);
    const QVector<QLineF> normalizedGridLines = normalizedLines(gridLines);
    QVector<MapEditorXviSketchPathData> normalizedSketchPaths;
    normalizedSketchPaths.reserve(sketchPaths.size());
    for (const MapEditorXviSketchPathData &sketchPath : sketchPaths) {
        if (sketchPath.lines.isEmpty()) {
            continue;
        }
        MapEditorXviSketchPathData normalizedSketch;
        normalizedSketch.color = sketchPath.color;
        normalizedSketch.style = sketchPath.style;
        normalizedSketch.lines = normalizedLines(sketchPath.lines);
        if (!normalizedSketch.lines.isEmpty()) {
            normalizedSketchPaths.append(normalizedSketch);
        }
    }

    MapEditorXviLayerGeometryData result;
    result.gridLines = normalizedGridLines;
    result.traverseShotLines = normalizedTraverseShots;
    result.splayShotLines = normalizedSplayShots;
    result.sketchPaths = normalizedSketchPaths;
    result.contentBounds = QRectF(QPointF(0.0, 0.0), layerBounds.size());
    if (!result.hasContent()) {
        return false;
    }

    *geometry = result;
    *topLeft = layerTopLeft;
    return true;
}

MapEditorXviBackgroundItem *createXviBackgroundItem(const XviDocument &xvi,
                                                    const QPointF &anchoredBasePosition,
                                                    const QString &rootStationName,
                                                    const QRectF &modelBounds,
                                                    const QRectF &previewBounds,
                                                    const QString &absolutePath)
{
    MapEditorXviLayerGeometryData geometry;
    QPointF topLeft;
    if (!buildXviLayerGeometry(xvi,
                               anchoredBasePosition,
                               rootStationName,
                               modelBounds,
                               previewBounds,
                               &geometry,
                               &topLeft)) {
        return nullptr;
    }

    auto *item = new MapEditorXviBackgroundItem();
    item->setGeometryData(geometry);
    item->setOpacity(kDefaultXviLayerOpacity);
    item->setData(0, absolutePath);
    item->setData(2, 1.0);
    item->setData(kBackgroundLayerXviGeometryKeyRole,
                  xviGeometryCacheKey(absolutePath,
                                      anchoredBasePosition,
                                      rootStationName,
                                      modelBounds,
                                      previewBounds));
    item->setData(kBackgroundLayerXviExpectedTopLeftRole, topLeft);
    item->setData(kBackgroundLayerXviBasePositionRole, anchoredBasePosition);
    item->setData(kBackgroundLayerXviRootStationRole, rootStationName);
    item->setPos(topLeft);
    return item;
}

bool updateXviBackgroundItemGeometry(MapEditorXviBackgroundItem *item,
                                     const QString &absolutePath,
                                     const XviDocument &xvi,
                                     const QPointF &anchoredBasePosition,
                                     const QString &rootStationName,
                                     const QRectF &modelBounds,
                                     const QRectF &previewBounds)
{
    if (item == nullptr) {
        return false;
    }

    const QString cacheKey = xviGeometryCacheKey(absolutePath,
                                                 anchoredBasePosition,
                                                 rootStationName,
                                                 modelBounds,
                                                 previewBounds);
    if (item->data(kBackgroundLayerXviGeometryKeyRole).toString() == cacheKey) {
        const QVariant expectedTopLeft = item->data(kBackgroundLayerXviExpectedTopLeftRole);
        if (expectedTopLeft.canConvert<QPointF>()) {
            item->setPos(expectedTopLeft.toPointF());
            return true;
        }
    }

    MapEditorXviLayerGeometryData geometry;
    QPointF topLeft;
    if (!buildXviLayerGeometry(xvi,
                               anchoredBasePosition,
                               rootStationName,
                               modelBounds,
                               previewBounds,
                               &geometry,
                               &topLeft)) {
        return false;
    }

    item->setGeometryData(geometry);
    item->setPos(topLeft);
    item->setData(kBackgroundLayerXviGeometryKeyRole, cacheKey);
    item->setData(kBackgroundLayerXviExpectedTopLeftRole, topLeft);
    item->setData(kBackgroundLayerXviBasePositionRole, anchoredBasePosition);
    item->setData(kBackgroundLayerXviRootStationRole, rootStationName);
    return true;
}

bool createAndAppendXviBackgroundItem(QGraphicsScene *scene,
                                      QVector<QGraphicsPixmapItem *> *layers,
                                      const QString &absolutePath,
                                      const XviDocument &xvi,
                                      const QPointF &anchoredBasePosition,
                                      const QString &rootStationName,
                                      const QRectF &modelBounds,
                                      const QRectF &previewBounds)
{
    if (scene == nullptr || layers == nullptr) {
        return false;
    }

    MapEditorXviBackgroundItem *backgroundItem = createXviBackgroundItem(xvi,
                                                                          anchoredBasePosition,
                                                                          rootStationName,
                                                                          modelBounds,
                                                                          previewBounds,
                                                                          absolutePath);
    if (backgroundItem == nullptr) {
        return false;
    }

    scene->addItem(backgroundItem);
    layers->append(backgroundItem);
    return true;
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

bool MapEditorTab::backgroundLayerSupportsGamma(int index) const
{
    const QGraphicsPixmapItem *item = backgroundLayerItemAt(index);
    if (item == nullptr) {
        return false;
    }
    return !isMapEditorXviBackgroundPath(item->data(0).toString());
}

bool MapEditorTab::backgroundLayerSupportsPositionEditing(int index) const
{
    const QGraphicsPixmapItem *item = backgroundLayerItemAt(index);
    if (item == nullptr) {
        return false;
    }
    return !isMapEditorXviBackgroundPath(item->data(0).toString());
}

QPointF MapEditorTab::backgroundLayerPosition(int index) const
{
    QGraphicsPixmapItem *item = backgroundLayerItemAt(index);
    if (item == nullptr) {
        return QPointF();
    }
    if (isMapEditorXviBackgroundPath(item->data(0).toString())) {
        const QVariant basePosition = item->data(kBackgroundLayerXviBasePositionRole);
        if (basePosition.canConvert<QPointF>()) {
            return basePosition.toPointF();
        }
    }
    return item->pos();
}

QRectF MapEditorTab::backgroundLayerSceneBounds(int index) const
{
    const QGraphicsPixmapItem *item = backgroundLayerItemAt(index);
    return item != nullptr ? item->sceneBoundingRect() : QRectF();
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
    const QString initialDirectory = backgroundImageDialogInitialDirectory(filePath(), projectRootPath_);
    const QStringList imagePaths = QFileDialog::getOpenFileNames(this,
                                                                 tr("Add Background Layers"),
                                                                 initialDirectory,
                                                                 tr("Background layers (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.gif *.webp *.xvi *.txt *.TXT)"));
    if (imagePaths.isEmpty()) {
        return;
    }
    if (mapScene_ == nullptr) {
        toolbarStatusNote_ = tr("No background layers were added.");
        updateCommandSurfaceState();
        return;
    }

    const int previousLayerCount = backgroundImageItems_.size();
    int pendingRasterLayerCount = 0;
    bool addedPocketTopoXviLayer = false;
    bool pocketTopoMetadataSkipped = false;
    PocketTopoXviImportOptions pocketTopoOptions;
    for (const QString &imagePath : imagePaths) {
        QString xviPath = imagePath;
        std::optional<XviDocument> generatedXviDocument;
        const bool pocketTopoImport = QFileInfo(imagePath).suffix().compare(QStringLiteral("txt"), Qt::CaseInsensitive) == 0;
        if (pocketTopoImport) {
            QString errorMessage;
            const std::optional<PocketTopoGeneratedXvi> generatedXvi =
                generatePocketTopoXvi(this, imagePath, &pocketTopoOptions, &errorMessage);
            if (!generatedXvi.has_value()) {
                if (!errorMessage.isEmpty()) {
                    QMessageBox::warning(this, tr("Import PocketTopo Background"), errorMessage);
                }
                continue;
            }
            xviPath = generatedXvi->path;
            generatedXviDocument = generatedXvi->document;
        }

        if (isMapEditorXviBackgroundPath(xviPath)) {
            const QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
            const QRectF previewBounds = mapPreviewBounds();
            const XtherionAreaAdjust areaAdjust = textEditor_ != nullptr
                ? parseXtherionAreaAdjust(textEditor_->text())
                : XtherionAreaAdjust{};
            XviDocument xviDocument;
            const QRectF xviModelBounds = areaAdjust.valid && areaAdjust.modelRect.isValid()
                ? areaAdjust.modelRect
                : sourceBounds;
            if (!generatedXviDocument.has_value()
                && !parseXviDocumentFileCached(QFileInfo(xviPath).absoluteFilePath(), &xviDocument)) {
                continue;
            }
            if (generatedXviDocument.has_value()) {
                xviDocument = generatedXviDocument.value();
            }
            const QString absoluteXviPath = QFileInfo(xviPath).absoluteFilePath();
            const XviBackgroundInsertionPlacement insertionPlacement =
                pocketTopoImport && textEditor_ != nullptr
                    ? pocketTopoXviInsertionPlacement(xviDocument, textEditor_->text())
                    : XviBackgroundInsertionPlacement{QPointF(0.0, 0.0), QString()};
            if (!createAndAppendXviBackgroundItem(mapScene_,
                                                  &backgroundImageItems_,
                                                  absoluteXviPath,
                                                  xviDocument,
                                                  insertionPlacement.basePosition,
                                                  insertionPlacement.rootStationName,
                                                  xviModelBounds,
                                                  previewBounds)) {
                continue;
            }
            if (pocketTopoImport && textEditor_ != nullptr) {
                const QString beforeText = textEditor_->text();
                const QString metadataLine = xtherionImageInsertLine(absoluteXviPath,
                                                                     filePath(),
                                                                     insertionPlacement.basePosition,
                                                                     true,
                                                                     1.0,
                                                                     insertionPlacement.rootStationName);
                QString afterMetadataText = beforeText;
                const TherionAreaAdjust existingAreaAdjust = parseTherionAreaAdjust(beforeText);
                if (!existingAreaAdjust.valid || !existingAreaAdjust.modelRect.isValid()) {
                    const QRectF placedBounds = xviPlacedModelBounds(xviDocument, insertionPlacement);
                    if (placedBounds.isValid()) {
                        afterMetadataText = upsertXtherionSimpleCommandLine(afterMetadataText,
                                                                            QStringLiteral("xth_me_area_adjust"),
                                                                            therionAreaAdjustMetadataLine(placedBounds));
                        afterMetadataText = upsertXtherionSimpleCommandLine(afterMetadataText,
                                                                            QStringLiteral("xth_me_area_zoom_to"),
                                                                            therionAreaZoomToMetadataLine());
                    }
                }
                const QString afterText = upsertXtherionImageMetadataLine(afterMetadataText,
                                                                          filePath(),
                                                                          absoluteXviPath,
                                                                          metadataLine,
                                                                          false);
                if (afterText != beforeText) {
                    bool metadataApplied = false;
                    const QScopedValueRollback<bool> refreshGuard(suppressSourceDrivenMapRefresh_, true);
                    applySourceTextChangeWithSnapshot(tr("Import PocketTopo Background"),
                                                      beforeText,
                                                      afterText,
                                                      0,
                                                      [&metadataApplied]() {
                                                          metadataApplied = true;
                                                      });
                    if (!metadataApplied) {
                        pocketTopoMetadataSkipped = true;
                    }
                }
            }
            if (pocketTopoImport) {
                addedPocketTopoXviLayer = true;
            }
            continue;
        }
        ++pendingRasterLayerCount;
        addBackgroundImageAsync(imagePath, true);
    }

    const int addedLayerCount = backgroundImageItems_.size() - previousLayerCount;
    if (addedLayerCount > 0) {
        toolbarStatusNote_ = pocketTopoMetadataSkipped
            ? tr("Added %1 background layer(s), but PocketTopo metadata sync was skipped because the document changed.")
                  .arg(addedLayerCount)
            : tr("Added %1 background layer(s).").arg(addedLayerCount);
        saveBackgroundLayersToSession();
    } else if (pendingRasterLayerCount > 0) {
        toolbarStatusNote_ = tr("Adding %1 background layer(s)...").arg(pendingRasterLayerCount);
    } else {
        toolbarStatusNote_ = tr("No background layers were added.");
    }

    updateCommandSurfaceState();
    if (addedPocketTopoXviLayer) {
        QTimer::singleShot(0, this, [this]() {
            fitBackgroundRequested_ = true;
            fitMapToView(true);
        });
    }
}

void MapEditorTab::removeSelectedBackgroundLayer()
{
    if (selectedBackgroundLayerIndex_ < 0 || selectedBackgroundLayerIndex_ >= backgroundImageItems_.size()) {
        return;
    }

    QGraphicsPixmapItem *item = backgroundImageItems_.takeAt(selectedBackgroundLayerIndex_);
    invalidateBackgroundLayerRasterJobs(item);
    const QString layerPath = item != nullptr ? item->data(0).toString() : QString();
    if (item != nullptr && mapScene_ != nullptr) {
        mapScene_->removeItem(item);
    }
    delete item;

    applyBackgroundLayerStackingOrder();
    setSelectedBackgroundLayerIndexInternal(selectedBackgroundLayerIndex_);
    toolbarStatusNote_ = tr("Removed selected background layer.");
    removeBackgroundLayerXtherionMetadata(layerPath, tr("Remove Background Image"));
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

    setBackgroundLayerVisibleFromUser(item, !item->isVisible());
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
    refreshBackgroundLayerPropertyControls();
}

void MapEditorTab::resetSelectedBackgroundLayerOpacity()
{
    QGraphicsPixmapItem *item = selectedBackgroundLayerItem();
    const qreal defaultOpacity = item != nullptr && isMapEditorXviBackgroundPath(item->data(0).toString())
        ? kDefaultXviLayerOpacity
        : kDefaultRasterLayerOpacity;
    setSelectedBackgroundLayerOpacity(defaultOpacity);
}

void MapEditorTab::setSelectedBackgroundLayerGamma(qreal gamma)
{
    QGraphicsPixmapItem *item = selectedBackgroundLayerItem();
    if (item == nullptr) {
        return;
    }
    if (isMapEditorXviBackgroundPath(item->data(0).toString())) {
        item->setData(2, 1.0);
        refreshBackgroundLayerPropertyControls();
        return;
    }

    applyBackgroundLayerGamma(item, qBound(0.2, gamma, 2.5));
    if (!syncBackgroundLayerXtherionGammaMetadata(item, tr("Set Background Gamma"))) {
        syncBackgroundLayerXtherionMetadata(item, tr("Set Background Gamma"), true);
    }
    saveBackgroundLayersToSession();
    refreshBackgroundLayerPropertyControls();
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
    syncBackgroundLayerXtherionMetadata(item, tr("Move Background Image"));
    saveBackgroundLayersToSession();
    refreshBackgroundLayerPropertyControls();
}

void MapEditorTab::nudgeSelectedBackgroundLayer(const QPointF &delta)
{
    QGraphicsPixmapItem *item = selectedBackgroundLayerItem();
    if (item == nullptr) {
        return;
    }

    item->setPos(item->pos() + delta);
    syncBackgroundLayerXtherionMetadata(item, tr("Move Background Image"));
    saveBackgroundLayersToSession();
    refreshBackgroundLayerPropertyControls();
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

void MapEditorTab::refreshBackgroundLayerPropertyControls()
{
    updatingBackgroundLayerControls_ = true;
    setSelectedBackgroundLayerIndexInternal(selectedBackgroundLayerIndex_);
    updatingBackgroundLayerControls_ = false;
    refreshInspectorBackgroundSelectionControls();
    emit backgroundLayerPropertiesChanged();
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
    if (isMapEditorXviBackgroundPath(item->data(0).toString())) {
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
    const QString existingJson = sessionStore_->therionMapBackgroundLayers();
    if (!existingJson.isEmpty()) {
        const QJsonDocument existingDocument = QJsonDocument::fromJson(existingJson.toUtf8());
        if (existingDocument.isObject()) {
            rootObject = existingDocument.object();
        }
    }

    if (backgroundImageItems_.isEmpty()) {
        rootObject.remove(documentKey);
        const QByteArray jsonBytes = QJsonDocument(rootObject).toJson(QJsonDocument::Compact);
        sessionStore_->setTherionMapBackgroundLayers(QString::fromUtf8(jsonBytes));
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
        if (hasUserVisibilityOverride(item)) {
            layerObject.insert(QStringLiteral("visibility_override"), true);
        }
        layerObject.insert(QStringLiteral("opacity"), item->opacity());
        layerObject.insert(QStringLiteral("gamma"), backgroundLayerGammaValue(item));
        layerObject.insert(QStringLiteral("x"), item->pos().x());
        layerObject.insert(QStringLiteral("y"), item->pos().y());
        layersArray.append(layerObject);
    }

    rootObject.insert(documentKey, layersArray);
    const QByteArray jsonBytes = QJsonDocument(rootObject).toJson(QJsonDocument::Compact);
    sessionStore_->setTherionMapBackgroundLayers(QString::fromUtf8(jsonBytes));
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

    const QString json = sessionStore_->therionMapBackgroundLayers();
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

    const QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
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
            const QPointF basePosition = hasMetadata && metadataReference->hasBasePosition
                ? metadataReference->basePosition
                : QPointF(0.0, 0.0);
            const QString rootStationName = hasMetadata ? metadataReference->rootStationName : QString();
            const QRectF xviModelBounds = areaAdjust.valid && areaAdjust.modelRect.isValid()
                ? areaAdjust.modelRect
                : sourceBounds;
            if (!parseXviDocumentFileCached(QFileInfo(layerPath).absoluteFilePath(), &xviDocument)
                || !createAndAppendXviBackgroundItem(mapScene_,
                                                     &backgroundImageItems_,
                                                     QFileInfo(layerPath).absoluteFilePath(),
                                                     xviDocument,
                                                     basePosition,
                                                     rootStationName,
                                                     xviModelBounds,
                                                     previewBounds)) {
                continue;
            }
        } else {
            if (addBackgroundImagePlaceholder(layerPath) == nullptr) {
                continue;
            }
        }

        QGraphicsPixmapItem *item = backgroundImageItems_.last();
        const bool metadataHasVisibility = hasMetadata && metadataReference->hasVisibility;
        const bool metadataVisible = metadataHasVisibility ? metadataReference->visible : true;
        const QJsonValue sessionVisibleValue = layerObject.value(QStringLiteral("visible"));
        const bool sessionVisible = sessionVisibleValue.isBool()
            ? sessionVisibleValue.toBool()
            : metadataVisible;
        const bool legacyHiddenXviOverride = !layerObject.contains(QStringLiteral("visibility_override"))
            && isMapEditorXviBackgroundPath(layerPath)
            && layerObject.contains(QStringLiteral("visible"))
            && !sessionVisible;
        const QJsonValue visibilityOverrideValue = layerObject.value(QStringLiteral("visibility_override"));
        const bool hasVisibilityOverride = visibilityOverrideValue.isBool()
            ? visibilityOverrideValue.toBool()
            : legacyHiddenXviOverride;
        item->setVisible((hasVisibilityOverride || !metadataHasVisibility) ? sessionVisible : metadataVisible);
        item->setData(kBackgroundLayerUserVisibilityRole, hasVisibilityOverride);
        const qreal defaultOpacity = isMapEditorXviBackgroundPath(layerPath) ? kDefaultXviLayerOpacity : kDefaultRasterLayerOpacity;
        const qreal opacity = layerObject.value(QStringLiteral("opacity")).toDouble(defaultOpacity);
        item->setOpacity(qBound(0.05, opacity, 1.0));
        if (hasMetadata && metadataReference->hasBasePosition) {
            if (metadataReference->xviReference) {
                item->setData(4, true);
            } else {
                placeMapEditorRasterLayerPlaceholderFromMetadata(item,
                                                        *metadataReference,
                                                        areaAdjust,
                                                        sourceBounds,
                                                        previewBounds);
                item->setData(4, true);
            }
        } else {
            const qreal layerX = layerObject.value(QStringLiteral("x")).toDouble(item->pos().x());
            const qreal layerY = layerObject.value(QStringLiteral("y")).toDouble(item->pos().y());
            item->setPos(layerX, layerY);
        }
        if (isMapEditorXviBackgroundPath(layerPath)) {
            item->setData(2, 1.0);
        } else {
            loadBackgroundImageSourceAsync(item);
            const qreal gamma = layerObject.value(QStringLiteral("gamma")).toDouble(1.0);
            applyBackgroundLayerGamma(item, qBound(0.2, gamma, 2.5));
        }
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
    const QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
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
            const bool pendingRasterLoad = existingLayer->data(kBackgroundLayerRasterLoadRequestRole).toULongLong() != 0
                && !existingLayer->data(kBackgroundLayerSourceImageRole).canConvert<QImage>();
            if (pendingRasterLoad) {
                placeMapEditorRasterLayerPlaceholderFromMetadata(existingLayer,
                                                        *existingMetadata,
                                                        areaAdjust,
                                                        sourceBounds,
                                                        previewBounds);
            } else {
                placeMapEditorRasterLayerFromMetadata(existingLayer,
                                                      rasterSourceImageForItem(existingLayer),
                                                      *existingMetadata,
                                                      areaAdjust,
                                                      sourceBounds,
                                                      previewBounds);
            }
            existingLayer->setData(4, true);
            if (existingMetadata->hasImageScale) {
                applyBackgroundLayerGamma(existingLayer, qBound(0.2, existingMetadata->imageScale, 2.5));
            }
            if (existingMetadata->hasVisibility) {
                setBackgroundLayerVisibleFromMetadata(existingLayer, existingMetadata->visible);
            }
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
            if (!parseXviDocumentFileCached(referencePath, &xviDocument)) {
                continue;
            }

            const QPointF anchoredBase = reference.hasBasePosition ? reference.basePosition : QPointF(0.0, 0.0);
            const QRectF xviModelBounds = areaAdjust.valid && areaAdjust.modelRect.isValid()
                ? areaAdjust.modelRect
                : sourceBounds;
            if (!createAndAppendXviBackgroundItem(mapScene_,
                                                  &backgroundImageItems_,
                                                  referencePath,
                                                  xviDocument,
                                                  anchoredBase,
                                                  reference.rootStationName,
                                                  xviModelBounds,
                                                  previewBounds)) {
                continue;
            }
            QGraphicsPixmapItem *backgroundItem = backgroundImageItems_.last();
            backgroundItem->setData(4, true);
            if (reference.hasVisibility) {
                setBackgroundLayerVisibleFromMetadata(backgroundItem, reference.visible);
            }
            applyBackgroundLayerStackingOrder();
            setSelectedBackgroundLayerIndexInternal(backgroundImageItems_.size() - 1);
            refreshBackgroundLayerControls();
            if (!referencePathKey.isEmpty()) {
                existingLayerPaths.insert(referencePathKey);
            }
            ++addedCount;
            continue;
        }

        QGraphicsPixmapItem *item = addBackgroundImagePlaceholder(referencePath);
        if (item == nullptr) {
            continue;
        }

        item->setData(4, true);
        if (reference.hasVisibility) {
            setBackgroundLayerVisibleFromMetadata(item, reference.visible);
        }
        if (reference.hasBasePosition && sourceBounds.isValid() && previewBounds.isValid()) {
            placeMapEditorRasterLayerPlaceholderFromMetadata(item,
                                                    reference,
                                                    areaAdjust,
                                                    sourceBounds,
                                                    previewBounds);
        }
        if (reference.hasImageScale) {
            loadBackgroundImageSourceAsync(item);
            applyBackgroundLayerGamma(item, qBound(0.2, reference.imageScale, 2.5));
        } else {
            loadBackgroundImageSourceAsync(item);
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

void MapEditorTab::reprojectMetadataBackgroundLayersForCurrentDocument()
{
    if (mapScene_ == nullptr || textEditor_ == nullptr || backgroundImageItems_.isEmpty()) {
        return;
    }

    const QVector<XtherionBackgroundReference> references = parseXtherionBackgroundReferences(textEditor_->text(), filePath());
    if (references.isEmpty()) {
        return;
    }

    const XtherionAreaAdjust areaAdjust = parseXtherionAreaAdjust(textEditor_->text());
    const QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
    const QRectF previewBounds = mapPreviewBounds();
    if (!sourceBounds.isValid() || !previewBounds.isValid()) {
        return;
    }

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

    bool updatedAnyLayer = false;
    for (QGraphicsPixmapItem *existingLayer : std::as_const(backgroundImageItems_)) {
        if (existingLayer == nullptr) {
            continue;
        }

        const QString layerPath = QFileInfo(existingLayer->data(0).toString()).absoluteFilePath();
        const XtherionBackgroundReference *metadataReference =
            findMetadataReferenceForPath(layerPath, metadataByPath, metadataByFileName);
        if (metadataReference == nullptr || !metadataReference->hasBasePosition) {
            continue;
        }

        if (metadataReference->xviReference) {
            XviDocument xviDocument;
            if (!parseXviDocumentFileCached(layerPath, &xviDocument)) {
                continue;
            }

            const QRectF xviModelBounds = areaAdjust.valid && areaAdjust.modelRect.isValid()
                ? areaAdjust.modelRect
                : sourceBounds;
            auto *xviItem = dynamic_cast<MapEditorXviBackgroundItem *>(existingLayer);
            if (xviItem == nullptr
                || !updateXviBackgroundItemGeometry(xviItem,
                                                    layerPath,
                                                    xviDocument,
                                                    metadataReference->basePosition,
                                                    metadataReference->rootStationName,
                                                    xviModelBounds,
                                                    previewBounds)) {
                continue;
            }
            existingLayer->setData(4, true);
            if (metadataReference->hasVisibility) {
                setBackgroundLayerVisibleFromMetadata(existingLayer, metadataReference->visible);
            }
            updatedAnyLayer = true;
            continue;
        }

        const bool pendingRasterLoad = existingLayer->data(kBackgroundLayerRasterLoadRequestRole).toULongLong() != 0
            && !existingLayer->data(kBackgroundLayerSourceImageRole).canConvert<QImage>();
        const bool placedLayer = pendingRasterLoad
            ? placeMapEditorRasterLayerPlaceholderFromMetadata(existingLayer,
                                                      *metadataReference,
                                                      areaAdjust,
                                                      sourceBounds,
                                                      previewBounds)
            : placeMapEditorRasterLayerFromMetadata(existingLayer,
                                                    rasterSourceImageForItem(existingLayer),
                                                    *metadataReference,
                                                    areaAdjust,
                                                    sourceBounds,
                                                    previewBounds);
        if (placedLayer) {
            existingLayer->setData(4, true);
            const qreal gamma = metadataReference->hasImageScale
                ? qBound(0.2, metadataReference->imageScale, 2.5)
                : backgroundLayerGammaValue(existingLayer);
            applyBackgroundLayerGamma(existingLayer, gamma);
            if (metadataReference->hasVisibility) {
                setBackgroundLayerVisibleFromMetadata(existingLayer, metadataReference->visible);
            }
            updatedAnyLayer = true;
        }
    }

    if (updatedAnyLayer) {
        applyBackgroundLayerStackingOrder();
        refreshBackgroundLayerControls();
    }
}

QRectF MapEditorTab::xtherionAutoAreaAdjustRect() const
{
    QRectF limits;
    bool hasLimits = false;

    auto includeRect = [&](const QRectF &rect) {
        if (!rect.isValid()) {
            return;
        }
        if (!hasLimits) {
            limits = rect.normalized();
            hasLimits = true;
            return;
        }
        limits = limits.united(rect.normalized());
    };

    const QRectF currentSourceBounds = textEditor_ != nullptr
        ? parseXtherionAreaAdjust(textEditor_->text()).modelRect
        : QRectF();
    const QRectF previewBounds = mapPreviewBounds();
    for (QGraphicsPixmapItem *item : backgroundImageItems_) {
        if (item == nullptr || item->data(0).toString().endsWith(QStringLiteral(".xvi"), Qt::CaseInsensitive)) {
            continue;
        }
        includeRect(mapEditorRasterModelRectForItem(item, currentSourceBounds, previewBounds));
    }

    if (textEditor_ != nullptr) {
        const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseTokenLines(textEditor_->text());
        const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
        if (!features.isEmpty()) {
            includeRect(geometryBoundsForFeatures(features));
        }
    }

    if (!hasLimits) {
        limits = QRectF(QPointF(128.0, 128.0), QSizeF(0.0, 0.0));
    }

    return limits.adjusted(-128.0, -128.0, 128.0, 128.0);
}

void MapEditorTab::syncBackgroundLayerXtherionMetadata(QGraphicsPixmapItem *item,
                                                       const QString &label,
                                                       bool preserveExistingPlacement)
{
    if (item == nullptr || textEditor_ == nullptr) {
        return;
    }

    const QString layerPath = QFileInfo(item->data(0).toString()).absoluteFilePath();
    if (layerPath.isEmpty() || layerPath.endsWith(QStringLiteral(".xvi"), Qt::CaseInsensitive)) {
        return;
    }

    const QString beforeText = textEditor_->text();
    std::optional<XtherionBackgroundReference> existingReference;
    if (preserveExistingPlacement) {
        const QString layerPathKey = normalizedPathKey(layerPath);
        for (const XtherionBackgroundReference &reference : parseXtherionBackgroundReferences(beforeText, filePath())) {
            if (!layerPathKey.isEmpty() && normalizedPathKey(reference.absolutePath) == layerPathKey) {
                existingReference = reference;
                break;
            }
        }
    }
    const TherionAreaAdjust existingAreaAdjust = parseTherionAreaAdjust(beforeText);
    const bool hasExistingPlacementMetadata = preserveExistingPlacement
        && existingReference.has_value()
        && existingReference->hasBasePosition
        && existingAreaAdjust.valid
        && existingAreaAdjust.modelRect.isValid();

    QPointF basePosition;
    if (preserveExistingPlacement && existingReference.has_value() && existingReference->hasBasePosition) {
        basePosition = existingReference->basePosition;
    } else {
        QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
        const QRectF previewBounds = mapPreviewBounds();
        if (!sourceBounds.isValid()) {
            sourceBounds = xtherionAutoAreaAdjustRect();
        }
        if (!sourceBounds.isValid() || !previewBounds.isValid()) {
            return;
        }

        const QSizeF modelSize = mapEditorRasterModelSize(layerPath, 1.0);
        if (!modelSize.isValid() || modelSize.width() <= 0.0 || modelSize.height() <= 0.0) {
            return;
        }

        const QPointF modelTopLeft = mapEditorPreviewToModelPoint(item->pos(), sourceBounds, previewBounds);
        basePosition = QPointF(modelTopLeft.x(), modelTopLeft.y() + modelSize.height());
    }

    const QString metadataLine = xtherionImageInsertLine(layerPath,
                                                        filePath(),
                                                        basePosition,
                                                        item->isVisible(),
                                                        backgroundLayerGammaValue(item));

    QString afterMetadataText = beforeText;
    if (!hasExistingPlacementMetadata) {
        afterMetadataText = upsertXtherionSimpleCommandLine(afterMetadataText,
                                                            QStringLiteral("xth_me_area_adjust"),
                                                            therionAreaAdjustMetadataLine(xtherionAutoAreaAdjustRect()));
        afterMetadataText = upsertXtherionSimpleCommandLine(afterMetadataText,
                                                            QStringLiteral("xth_me_area_zoom_to"),
                                                            therionAreaZoomToMetadataLine());
    }
    const QString afterText = upsertXtherionImageMetadataLine(afterMetadataText,
                                                             filePath(),
                                                             layerPath,
                                                             metadataLine,
                                                             false);
    if (afterText == beforeText) {
        return;
    }

    bool metadataApplied = false;
    const QScopedValueRollback<bool> refreshGuard(suppressSourceDrivenMapRefresh_, true);
    applySourceTextChangeWithSnapshot(label,
                                      beforeText,
                                      afterText,
                                      0,
                                      [this, &metadataApplied]() {
                                          metadataApplied = true;
                                          if (!toolbarStatusNote_.isEmpty()) {
                                              refreshToolbarSummary();
                                          }
                                      });
    if (!metadataApplied) {
        toolbarStatusNote_ = tr("Background metadata sync skipped: document changed.");
        refreshToolbarSummary();
    }
}

bool MapEditorTab::syncBackgroundLayerXtherionGammaMetadata(QGraphicsPixmapItem *item, const QString &label)
{
    if (item == nullptr || textEditor_ == nullptr) {
        return false;
    }

    const QString layerPath = QFileInfo(item->data(0).toString()).absoluteFilePath();
    if (layerPath.isEmpty() || layerPath.endsWith(QStringLiteral(".xvi"), Qt::CaseInsensitive)) {
        return false;
    }

    const QString beforeText = textEditor_->text();
    bool updated = false;
    const QString afterText = updateExistingXtherionImageMetadataGamma(beforeText,
                                                                       filePath(),
                                                                       layerPath,
                                                                       backgroundLayerGammaValue(item),
                                                                       &updated);
    if (!updated || afterText == beforeText) {
        return false;
    }

    bool metadataApplied = false;
    const QScopedValueRollback<bool> refreshGuard(suppressSourceDrivenMapRefresh_, true);
    applySourceTextChangeWithSnapshot(label,
                                      beforeText,
                                      afterText,
                                      0,
                                      [this, &metadataApplied]() {
                                          metadataApplied = true;
                                          if (!toolbarStatusNote_.isEmpty()) {
                                              refreshToolbarSummary();
                                          }
                                      });
    if (!metadataApplied) {
        toolbarStatusNote_ = tr("Background gamma metadata sync skipped: document changed.");
        refreshToolbarSummary();
    }
    return metadataApplied;
}

void MapEditorTab::removeBackgroundLayerXtherionMetadata(const QString &layerPath, const QString &label)
{
    if (textEditor_ == nullptr || layerPath.isEmpty()) {
        return;
    }

    const QString absolutePath = QFileInfo(layerPath).absoluteFilePath();
    const QString beforeText = textEditor_->text();
    const QString afterRemoveText = upsertXtherionImageMetadataLine(beforeText,
                                                                    filePath(),
                                                                    absolutePath,
                                                                    QString(),
                                                                    true);
    const QString afterAreaText = upsertXtherionSimpleCommandLine(afterRemoveText,
                                                                  QStringLiteral("xth_me_area_adjust"),
                                                                  therionAreaAdjustMetadataLine(xtherionAutoAreaAdjustRect()));
    const QString afterText = upsertXtherionSimpleCommandLine(afterAreaText,
                                                              QStringLiteral("xth_me_area_zoom_to"),
                                                              therionAreaZoomToMetadataLine());
    if (afterText == beforeText) {
        return;
    }

    bool metadataApplied = false;
    const QScopedValueRollback<bool> refreshGuard(suppressSourceDrivenMapRefresh_, true);
    applySourceTextChangeWithSnapshot(label,
                                      beforeText,
                                      afterText,
                                      0,
                                      [this, &metadataApplied]() {
                                          metadataApplied = true;
                                          if (!toolbarStatusNote_.isEmpty()) {
                                              refreshToolbarSummary();
                                          }
                                      });
    if (!metadataApplied) {
        toolbarStatusNote_ = tr("Background metadata removal skipped: document changed.");
        refreshToolbarSummary();
    }
}

void MapEditorTab::invalidateBackgroundLayerRasterJobs(QGraphicsPixmapItem *item)
{
    if (item == nullptr) {
        return;
    }

    item->setData(kBackgroundLayerRasterLoadRequestRole, QVariant::fromValue<qulonglong>(nextMapEditorRasterLoadRequestId()));
    item->setData(kBackgroundLayerRasterGammaRequestRole, QVariant::fromValue<qulonglong>(nextMapEditorRasterGammaRequestId()));
}

void MapEditorTab::invalidateBackgroundRasterJobs()
{
    ++backgroundRasterJobGeneration_;
    for (QGraphicsPixmapItem *item : std::as_const(backgroundImageItems_)) {
        invalidateBackgroundLayerRasterJobs(item);
    }
}

void MapEditorTab::addBackgroundImage(const QString &imagePath, bool writeXtherionMetadata)
{
    if (mapScene_ == nullptr || imagePath.isEmpty()) {
        return;
    }

    const QImage image = readMapEditorRasterSourceImage(imagePath);
    addBackgroundImageFromSourceImage(imagePath, image, writeXtherionMetadata);
}

void MapEditorTab::addBackgroundImageAsync(const QString &imagePath, bool writeXtherionMetadata)
{
    if (mapScene_ == nullptr || imagePath.isEmpty()) {
        return;
    }

    if (const std::optional<QImage> cachedImage = cachedMapEditorRasterSourceImage(imagePath); cachedImage.has_value()) {
        addBackgroundImageFromSourceImage(imagePath, cachedImage.value(), writeXtherionMetadata);
        return;
    }

    const quint64 requestGeneration = backgroundRasterJobGeneration_;
    auto *watcher = new QFutureWatcher<MapEditorRasterSourceImageLoadResult>(this);
    connect(watcher, &QFutureWatcher<MapEditorRasterSourceImageLoadResult>::finished, this, [this, watcher, writeXtherionMetadata, requestGeneration]() {
        const MapEditorRasterSourceImageLoadResult result = watcher->result();
        watcher->deleteLater();

        if (backgroundRasterJobGeneration_ != requestGeneration) {
            return;
        }
        if (result.image.isNull()) {
            toolbarStatusNote_ = tr("Could not load background image.");
            updateCommandSurfaceState();
            return;
        }

        rememberMapEditorRasterSourceImage(result.imagePath, result.image);
        if (addBackgroundImageFromSourceImage(result.imagePath, result.image, writeXtherionMetadata)) {
            toolbarStatusNote_ = tr("Added background layer.");
            saveBackgroundLayersToSession();
        }
        updateCommandSurfaceState();
    });
    watcher->setFuture(QtConcurrent::run(readMapEditorRasterSourceImageUncached, imagePath));
}

QGraphicsPixmapItem *MapEditorTab::addBackgroundImagePlaceholder(const QString &imagePath)
{
    if (mapScene_ == nullptr || imagePath.isEmpty()) {
        return nullptr;
    }

    const QRectF previewBounds = mapPreviewBounds();
    if (!previewBounds.isValid() || previewBounds.width() < 2.0 || previewBounds.height() < 2.0) {
        return nullptr;
    }

    const QSizeF modelSize = mapEditorRasterModelSize(imagePath, 1.0);
    if (!modelSize.isValid() || modelSize.width() <= 0.0 || modelSize.height() <= 0.0) {
        return nullptr;
    }

    QSize targetSize = modelSize.toSize();
    targetSize.scale(previewBounds.size().toSize(), Qt::KeepAspectRatio);
    targetSize.setWidth(qMax(targetSize.width(), 2));
    targetSize.setHeight(qMax(targetSize.height(), 2));

    QImage placeholder(targetSize, QImage::Format_ARGB32_Premultiplied);
    placeholder.fill(Qt::transparent);

    auto *backgroundItem = new QGraphicsPixmapItem(QPixmap::fromImage(placeholder));
    backgroundItem->setTransformationMode(Qt::SmoothTransformation);
    backgroundItem->setOpacity(kDefaultRasterLayerOpacity);
    backgroundItem->setData(0, QFileInfo(imagePath).absoluteFilePath());
    backgroundItem->setData(2, 1.0);

    const QPointF topLeft(previewBounds.center().x() - (targetSize.width() / 2.0),
                          previewBounds.center().y() - (targetSize.height() / 2.0));
    backgroundItem->setPos(topLeft);

    mapScene_->addItem(backgroundItem);
    backgroundImageItems_.append(backgroundItem);
    applyBackgroundLayerStackingOrder();
    setSelectedBackgroundLayerIndexInternal(backgroundImageItems_.size() - 1);
    refreshBackgroundLayerControls();
    return backgroundItem;
}

void MapEditorTab::loadBackgroundImageSourceAsync(QGraphicsPixmapItem *item)
{
    if (item == nullptr) {
        return;
    }

    const QString imagePath = item->data(0).toString();
    if (imagePath.isEmpty() || isMapEditorXviBackgroundPath(imagePath)) {
        return;
    }

    if (const std::optional<QImage> cachedImage = cachedMapEditorRasterSourceImage(imagePath); cachedImage.has_value()) {
        cacheRasterSourceImage(item, cachedImage.value());
        applyBackgroundLayerGamma(item, backgroundLayerGammaValue(item));
        return;
    }

    const quint64 requestId = nextMapEditorRasterLoadRequestId();
    const quint64 requestGeneration = backgroundRasterJobGeneration_;
    item->setData(kBackgroundLayerRasterLoadRequestRole, QVariant::fromValue<qulonglong>(requestId));

    auto *watcher = new QFutureWatcher<MapEditorRasterSourceImageLoadResult>(this);
    connect(watcher, &QFutureWatcher<MapEditorRasterSourceImageLoadResult>::finished, this, [this, watcher, item, requestId, requestGeneration]() {
        const MapEditorRasterSourceImageLoadResult result = watcher->result();
        watcher->deleteLater();

        if (backgroundRasterJobGeneration_ != requestGeneration) {
            return;
        }
        if (!backgroundImageItems_.contains(item)) {
            return;
        }
        if (item->data(kBackgroundLayerRasterLoadRequestRole).toULongLong() != requestId) {
            return;
        }
        if (result.image.isNull()) {
            const QString fileName = QFileInfo(result.imagePath).fileName();
            toolbarStatusNote_ = fileName.isEmpty()
                ? tr("Could not load background image.")
                : tr("Could not load background image: %1.").arg(fileName);
            refreshToolbarSummary();
            return;
        }

        rememberMapEditorRasterSourceImage(result.imagePath, result.image);
        cacheRasterSourceImage(item, result.image);
        applyBackgroundLayerGamma(item, backgroundLayerGammaValue(item));
    });
    watcher->setFuture(QtConcurrent::run(readMapEditorRasterSourceImageUncached, imagePath));
}

bool MapEditorTab::addBackgroundImageFromSourceImage(const QString &imagePath,
                                                    const QImage &image,
                                                    bool writeXtherionMetadata)
{
    if (mapScene_ == nullptr || imagePath.isEmpty() || image.isNull()) {
        return false;
    }

    rememberMapEditorRasterSourceImage(imagePath, image);

    const QRectF previewBounds = mapPreviewBounds();
    if (!previewBounds.isValid() || previewBounds.width() < 2.0 || previewBounds.height() < 2.0) {
        return false;
    }

    QPixmap pixmap = QPixmap::fromImage(image);
    if (pixmap.isNull()) {
        return false;
    }

    QSize targetSize = previewBounds.size().toSize();
    targetSize.setWidth(qMax(targetSize.width(), 2));
    targetSize.setHeight(qMax(targetSize.height(), 2));
    pixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    auto *backgroundItem = new QGraphicsPixmapItem(pixmap);
    backgroundItem->setTransformationMode(Qt::SmoothTransformation);
    backgroundItem->setOpacity(kDefaultRasterLayerOpacity);
    backgroundItem->setData(0, QFileInfo(imagePath).absoluteFilePath());
    backgroundItem->setData(2, 1.0);
    cacheRasterSourceImage(backgroundItem, image);

    const QPointF topLeft(previewBounds.center().x() - (pixmap.width() / 2.0),
                          previewBounds.center().y() - (pixmap.height() / 2.0));
    backgroundItem->setPos(topLeft);
    if (writeXtherionMetadata && !mapSourceBoundsForCurrentDocument().isValid()) {
        const QSizeF modelSize = mapEditorRasterModelSize(imagePath, 1.0);
        const QRectF modelRect(QPointF(0.0, -modelSize.height()), modelSize);
        const QRectF modelBounds = modelRect.adjusted(-128.0, -128.0, 128.0, 128.0);
        placeMapEditorRasterLayerByModelRect(backgroundItem, image, modelRect, modelBounds, previewBounds);
    }
    mapScene_->addItem(backgroundItem);
    backgroundImageItems_.append(backgroundItem);
    applyBackgroundLayerStackingOrder();
    setSelectedBackgroundLayerIndexInternal(backgroundImageItems_.size() - 1);
    if (writeXtherionMetadata) {
        syncBackgroundLayerXtherionMetadata(backgroundItem, tr("Add Background Image"));
    }
    refreshBackgroundLayerControls();
    return true;
}

void MapEditorTab::applyBackgroundLayerGamma(QGraphicsPixmapItem *item, qreal gamma)
{
    if (item == nullptr) {
        return;
    }

    const qreal boundedGamma = qBound(0.2, gamma, 2.5);
    const QString layerPath = item->data(0).toString();
    if (layerPath.isEmpty()) {
        return;
    }
    if (isMapEditorXviBackgroundPath(layerPath)) {
        item->setData(2, 1.0);
        return;
    }

    if (!item->data(kBackgroundLayerSourceImageRole).canConvert<QImage>()
        && !cachedMapEditorRasterSourceImage(layerPath).has_value()
        && item->data(kBackgroundLayerRasterLoadRequestRole).toULongLong() != 0) {
        item->setData(2, boundedGamma);
        return;
    }

    QImage sourceImage = rasterSourceImageForItem(item);
    if (sourceImage.isNull()) {
        return;
    }

    const QRectF currentBounds = item->boundingRect();
    const QSize targetSize = currentBounds.size().toSize();
    const quint64 requestId = nextMapEditorRasterGammaRequestId();
    item->setData(kBackgroundLayerRasterGammaRequestRole, QVariant::fromValue<qulonglong>(requestId));
    item->setData(2, boundedGamma);

    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, item, requestId]() {
        const QImage adjustedImage = watcher->result();
        watcher->deleteLater();

        if (adjustedImage.isNull()) {
            return;
        }
        if (!backgroundImageItems_.contains(item)) {
            return;
        }
        if (item->data(kBackgroundLayerRasterGammaRequestRole).toULongLong() != requestId) {
            return;
        }

        item->setPixmap(QPixmap::fromImage(adjustedImage));
    });
    watcher->setFuture(QtConcurrent::run(gammaCorrectAndScaleMapEditorRasterSourceImage,
                                         layerPath,
                                         sourceImage,
                                         targetSize,
                                         boundedGamma));
}

}
