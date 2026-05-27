#include "../src/app/text_editor/map_editor/MapEditorInspectorData.h"
#include "../src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h"
#include "../src/app/text_editor/map_editor/MapEditorStylePreviewWidget.h"
#include "../src/core/CommandCatalogStore.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <iostream>

using namespace TherionStudio;

namespace
{
struct GalleryEntry
{
    QString commandKind;
    QString type;
    QString subtype;
    QString coverage;
    QString source;
    QString imagePath;
};

QString normalizedToken(QString value)
{
    return value.trimmed().toLower();
}

QString galleryKey(const QString &commandKind, const QString &type, const QString &subtype)
{
    return QStringLiteral("%1|%2|%3").arg(normalizedToken(commandKind),
                                          normalizedToken(type),
                                          normalizedToken(subtype));
}

QString htmlEscaped(const QString &value)
{
    return value.toHtmlEscaped();
}

QString slugPart(QString value)
{
    value = normalizedToken(value);
    if (value.isEmpty()) {
        return QStringLiteral("default");
    }
    static const QRegularExpression unsafe(QStringLiteral("[^a-z0-9._-]+"));
    value.replace(unsafe, QStringLiteral("_"));
    return value;
}

QString previewFileName(const GalleryEntry &entry)
{
    QString fileName = slugPart(entry.commandKind) + QLatin1Char('.') + slugPart(entry.type);
    if (!entry.subtype.trimmed().isEmpty()) {
        fileName += QLatin1Char('.') + slugPart(entry.subtype);
    }
    return fileName + QStringLiteral(".png");
}

QSize previewSizeForCommand(const QString &commandKind)
{
    const QString normalized = normalizedToken(commandKind);
    if (normalized == QStringLiteral("line")) {
        return QSize(520, 128);
    }
    if (normalized == QStringLiteral("area")) {
        return QSize(320, 160);
    }
    return QSize(260, 128);
}

bool pointRuleExact(const MapEditorPointStyleRule &rule, const QString &type, const QString &subtype)
{
    return rule.selector.rawType == type && rule.selector.subtype == subtype;
}

bool lineRuleExact(const MapEditorLineStyleRule &rule, const QString &type, const QString &subtype)
{
    return rule.selector.rawType == type && rule.selector.subtype == subtype;
}

bool areaRuleExact(const MapEditorAreaStyleRule &rule, const QString &type, const QString &subtype)
{
    return rule.selector.rawType == type && rule.selector.subtype == subtype;
}

bool pointTypeRule(const MapEditorPointStyleRule &rule, const QString &type)
{
    return rule.selector.rawType == type && rule.selector.subtype.isEmpty();
}

bool lineTypeRule(const MapEditorLineStyleRule &rule, const QString &type)
{
    return rule.selector.rawType == type && rule.selector.subtype.isEmpty();
}

bool areaTypeRule(const MapEditorAreaStyleRule &rule, const QString &type)
{
    return rule.selector.rawType == type && rule.selector.subtype.isEmpty();
}

QString pointCoverage(const MapEditorObjectStyleCatalog &catalog, const QString &type, const QString &subtype)
{
    const QString normalizedType = normalizedToken(type);
    const QString normalizedSubtype = normalizedToken(subtype);
    for (const MapEditorPointStyleRule &rule : catalog.pointStyles) {
        if (pointRuleExact(rule, normalizedType, normalizedSubtype)) {
            return normalizedSubtype.isEmpty() ? QStringLiteral("type style") : QStringLiteral("exact subtype style");
        }
    }
    if (!normalizedSubtype.isEmpty()) {
        for (const MapEditorPointStyleRule &rule : catalog.pointStyles) {
            if (pointTypeRule(rule, normalizedType)) {
                return QStringLiteral("inherited type style");
            }
        }
    }
    return QStringLiteral("global default");
}

QString lineCoverage(const MapEditorObjectStyleCatalog &catalog, const QString &type, const QString &subtype)
{
    const QString normalizedType = normalizedToken(type);
    const QString normalizedSubtype = normalizedToken(subtype);
    for (const MapEditorLineStyleRule &rule : catalog.lineStyles) {
        if (lineRuleExact(rule, normalizedType, normalizedSubtype)) {
            return normalizedSubtype.isEmpty() ? QStringLiteral("type style") : QStringLiteral("exact subtype style");
        }
    }
    if (!normalizedSubtype.isEmpty()) {
        for (const MapEditorLineStyleRule &rule : catalog.lineStyles) {
            if (lineTypeRule(rule, normalizedType)) {
                return QStringLiteral("inherited type style");
            }
        }
    }
    return QStringLiteral("global default");
}

QString areaCoverage(const MapEditorObjectStyleCatalog &catalog, const QString &type, const QString &subtype)
{
    const QString normalizedType = normalizedToken(type);
    const QString normalizedSubtype = normalizedToken(subtype);
    for (const MapEditorAreaStyleRule &rule : catalog.areaStyles) {
        if (areaRuleExact(rule, normalizedType, normalizedSubtype)) {
            return normalizedSubtype.isEmpty() ? QStringLiteral("type style") : QStringLiteral("exact subtype style");
        }
    }
    if (!normalizedSubtype.isEmpty()) {
        for (const MapEditorAreaStyleRule &rule : catalog.areaStyles) {
            if (areaTypeRule(rule, normalizedType)) {
                return QStringLiteral("inherited type style");
            }
        }
    }
    return QStringLiteral("global default");
}

QString coverageForEntry(const MapEditorObjectStyleCatalog &catalog, const GalleryEntry &entry)
{
    const QString kind = normalizedToken(entry.commandKind);
    if (kind == QStringLiteral("point")) {
        return pointCoverage(catalog, entry.type, entry.subtype);
    }
    if (kind == QStringLiteral("line")) {
        return lineCoverage(catalog, entry.type, entry.subtype);
    }
    return areaCoverage(catalog, entry.type, entry.subtype);
}

void addEntry(QVector<GalleryEntry> *entries,
              QSet<QString> *seen,
              const QString &commandKind,
              const QString &type,
              const QString &subtype,
              const QString &source)
{
    if (entries == nullptr || seen == nullptr) {
        return;
    }
    const QString normalizedCommand = normalizedToken(commandKind);
    const QString normalizedType = normalizedToken(type);
    const QString normalizedSubtype = normalizedToken(subtype);
    if (normalizedCommand.isEmpty() || normalizedType.isEmpty()) {
        return;
    }

    const QString key = galleryKey(normalizedCommand, normalizedType, normalizedSubtype);
    if (seen->contains(key)) {
        return;
    }

    GalleryEntry entry;
    entry.commandKind = normalizedCommand;
    entry.type = normalizedType;
    entry.subtype = normalizedSubtype;
    entry.source = source;
    entries->append(entry);
    seen->insert(key);
}

void addCatalogEntries(QVector<GalleryEntry> *entries,
                       QSet<QString> *seen,
                       const InspectorSymbolCatalog &catalog,
                       const QString &commandKind)
{
    const QStringList types = inspectorTypeValuesForCommand(catalog, commandKind);
    for (const QString &type : types) {
        addEntry(entries, seen, commandKind, type, QString(), QStringLiteral("catalog"));
        const QStringList subtypes = inspectorSubtypeValuesForCommandType(catalog, commandKind, type);
        for (const QString &subtype : subtypes) {
            addEntry(entries, seen, commandKind, type, subtype, QStringLiteral("catalog"));
        }
    }
}

void addStyleOnlyEntries(QVector<GalleryEntry> *entries,
                         QSet<QString> *seen,
                         const MapEditorObjectStyleCatalog &catalog)
{
    for (const MapEditorPointStyleRule &rule : catalog.pointStyles) {
        addEntry(entries, seen, QStringLiteral("point"), rule.selector.rawType, rule.selector.subtype, QStringLiteral("style-only"));
    }
    for (const MapEditorLineStyleRule &rule : catalog.lineStyles) {
        addEntry(entries, seen, QStringLiteral("line"), rule.selector.rawType, rule.selector.subtype, QStringLiteral("style-only"));
    }
    for (const MapEditorAreaStyleRule &rule : catalog.areaStyles) {
        addEntry(entries, seen, QStringLiteral("area"), rule.selector.rawType, rule.selector.subtype, QStringLiteral("style-only"));
    }
}

QVector<GalleryEntry> galleryEntries(const InspectorSymbolCatalog &symbolCatalog,
                                     const MapEditorObjectStyleCatalog &styleCatalog)
{
    QVector<GalleryEntry> entries;
    QSet<QString> seen;
    addCatalogEntries(&entries, &seen, symbolCatalog, QStringLiteral("point"));
    addCatalogEntries(&entries, &seen, symbolCatalog, QStringLiteral("line"));
    addCatalogEntries(&entries, &seen, symbolCatalog, QStringLiteral("area"));
    addStyleOnlyEntries(&entries, &seen, styleCatalog);

    std::sort(entries.begin(), entries.end(), [](const GalleryEntry &left, const GalleryEntry &right) {
        if (left.commandKind != right.commandKind) {
            return left.commandKind < right.commandKind;
        }
        if (left.type != right.type) {
            return left.type < right.type;
        }
        return left.subtype < right.subtype;
    });
    return entries;
}

bool renderPreview(const GalleryEntry &entry, const QString &outputPath)
{
    const QSize size = previewSizeForCommand(entry.commandKind);
    MapEditorStylePreviewWidget preview;
    preview.resize(size);
    preview.setStyleSelection(entry.commandKind, entry.type, entry.subtype);
    preview.ensurePolished();

    QImage image(size * qApp->devicePixelRatio(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(qApp->devicePixelRatio());
    image.fill(Qt::transparent);

    QPainter painter(&image);
    preview.render(&painter);
    painter.end();
    return image.save(outputPath, "PNG");
}

QString defaultOutputDirectory()
{
    return QDir::current().absoluteFilePath(QStringLiteral("style_gallery"));
}

QString outputDirectoryFromArguments(const QStringList &arguments)
{
    for (int index = 1; index < arguments.size(); ++index) {
        const QString argument = arguments.at(index);
        if (argument == QStringLiteral("--output") && index + 1 < arguments.size()) {
            return arguments.at(index + 1);
        }
        if (argument.startsWith(QStringLiteral("--output="))) {
            return argument.mid(QStringLiteral("--output=").size());
        }
    }
    return defaultOutputDirectory();
}

bool writeHtmlIndex(const QString &outputDirectory, const QVector<GalleryEntry> &entries)
{
    QFile file(QDir(outputDirectory).filePath(QStringLiteral("index.html")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << "<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n";
    out << "<title>Therion Studio Map Object Style Gallery</title>\n";
    out << "<style>\n"
        << "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;margin:24px;background:#f5f2ea;color:#1c1c1e;}\n"
        << "h1{margin:0 0 8px;} h2{margin-top:32px;border-bottom:2px solid #222;padding-bottom:6px;}\n"
        << ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(280px,1fr));gap:14px;}\n"
        << ".card{background:white;border:1px solid #d3cec3;border-radius:10px;padding:12px;box-shadow:0 1px 3px rgba(0,0,0,.08);}\n"
        << ".preview{width:100%;height:auto;border-radius:7px;display:block;margin-bottom:8px;}\n"
        << ".name{font-weight:700;font-size:15px;margin-bottom:4px;}\n"
        << ".meta{font-size:12px;color:#555;display:flex;gap:6px;flex-wrap:wrap;}\n"
        << ".pill{border:1px solid #c7c0b3;border-radius:999px;padding:2px 7px;background:#faf8f2;}\n"
        << ".default{background:#fff0c2;border-color:#d9aa31;}\n"
        << ".exact{background:#dff3df;border-color:#76b476;}\n"
        << ".inherited{background:#e2eefc;border-color:#7ea7df;}\n"
        << ".styleonly{background:#f2e5ff;border-color:#b487d9;}\n"
        << "</style>\n</head>\n<body>\n";
    out << "<h1>Therion Studio Map Object Style Gallery</h1>\n";
    out << "<p>Generated from the Therion command catalog plus bundled/user map object style selectors. "
        << "Entries marked <strong>global default</strong> currently have no type/subtype-specific style.</p>\n";

    const QStringList sections{QStringLiteral("point"), QStringLiteral("line"), QStringLiteral("area")};
    for (const QString &section : sections) {
        const int count = std::count_if(entries.begin(), entries.end(), [&section](const GalleryEntry &entry) {
            return entry.commandKind == section;
        });
        out << "<h2>" << htmlEscaped(section) << " symbols (" << count << ")</h2>\n<div class=\"grid\">\n";
        for (const GalleryEntry &entry : entries) {
            if (entry.commandKind != section) {
                continue;
            }
            QString coverageClass = QStringLiteral("default");
            if (entry.coverage.startsWith(QStringLiteral("exact")) || entry.coverage == QStringLiteral("type style")) {
                coverageClass = QStringLiteral("exact");
            } else if (entry.coverage.startsWith(QStringLiteral("inherited"))) {
                coverageClass = QStringLiteral("inherited");
            }
            const QString sourceClass = entry.source == QStringLiteral("style-only")
                ? QStringLiteral(" styleonly")
                : QString();
            out << "<article class=\"card\">\n";
            out << "<img class=\"preview\" src=\"" << htmlEscaped(entry.imagePath) << "\" alt=\""
                << htmlEscaped(entry.commandKind + QLatin1Char(' ') + entry.type + QLatin1Char(' ') + entry.subtype) << "\">\n";
            out << "<div class=\"name\">" << htmlEscaped(entry.type);
            if (!entry.subtype.isEmpty()) {
                out << " : " << htmlEscaped(entry.subtype);
            }
            out << "</div>\n<div class=\"meta\">";
            out << "<span class=\"pill " << coverageClass << "\">" << htmlEscaped(entry.coverage) << "</span>";
            out << "<span class=\"pill" << sourceClass << "\">" << htmlEscaped(entry.source) << "</span>";
            out << "</div>\n</article>\n";
        }
        out << "</div>\n";
    }

    out << "</body>\n</html>\n";
    return true;
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString outputDirectory = QDir::cleanPath(outputDirectoryFromArguments(app.arguments()));
    QDir outputRoot;
    if (!outputRoot.mkpath(outputDirectory)) {
        std::cerr << "Failed to create output directory: " << outputDirectory.toStdString() << '\n';
        return 1;
    }

    const CommandCatalogStore commandCatalog;
    const InspectorSymbolCatalog symbolCatalog = inspectorSymbolCatalogFromCommandCatalog(commandCatalog.catalogObject());
    const MapEditorObjectStyleCatalog styleCatalog = mapEditorObjectStyleCatalog();

    QVector<GalleryEntry> entries = galleryEntries(symbolCatalog, styleCatalog);
    QDir outputDir(outputDirectory);
    for (GalleryEntry &entry : entries) {
        entry.coverage = coverageForEntry(styleCatalog, entry);
        const QString commandDirectory = outputDir.filePath(entry.commandKind);
        if (!outputDir.mkpath(entry.commandKind)) {
            std::cerr << "Failed to create category directory: " << commandDirectory.toStdString() << '\n';
            return 1;
        }

        const QString fileName = previewFileName(entry);
        const QString absoluteImagePath = QDir(commandDirectory).filePath(fileName);
        if (!renderPreview(entry, absoluteImagePath)) {
            std::cerr << "Failed to render preview: " << absoluteImagePath.toStdString() << '\n';
            return 1;
        }
        entry.imagePath = entry.commandKind + QLatin1Char('/') + fileName;
    }

    if (!writeHtmlIndex(outputDirectory, entries)) {
        std::cerr << "Failed to write style gallery index.\n";
        return 1;
    }

    std::cout << "Generated " << entries.size() << " map object style previews in "
              << outputDirectory.toStdString() << '\n';
    return 0;
}
