#include "MainWindowHelpDocument.h"

#include <QHash>
#include <QRegularExpression>
#include <QTextDocument>

namespace TherionStudio
{
namespace
{
QString plainHeadingText(QString headingText)
{
    headingText = headingText.trimmed();
    headingText.replace(QRegularExpression(QStringLiteral("`([^`]*)`")), QStringLiteral("\\1"));
    headingText.replace(QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]\\([^\\)]*\\)")),
                        QStringLiteral("\\1"));
    headingText.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    headingText.remove(QRegularExpression(QStringLiteral("[*_~]")));
    return headingText.simplified();
}

QString anchorSlugForHeading(const QString &headingText)
{
    const QString normalized = plainHeadingText(headingText).toLower();
    QString slug;
    bool pendingDash = false;
    for (const QChar character : normalized) {
        if (character.isLetterOrNumber()) {
            if (pendingDash && !slug.isEmpty()) {
                slug.append(QLatin1Char('-'));
            }
            slug.append(character);
            pendingDash = false;
        } else if (character.isSpace()
                   || character == QLatin1Char('-')
                   || character == QLatin1Char('_')
                   || character == QLatin1Char('/')) {
            pendingDash = true;
        }
    }

    return !slug.isEmpty() ? slug : QStringLiteral("section");
}
}

QVector<MainWindowHelpSection> parseMarkdownHelpSections(const QString &markdown)
{
    static const QRegularExpression headingExpression(QStringLiteral("^(#{1,6})\\s+(.+?)\\s*#*\\s*$"));

    const QStringList lines = markdown.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    QVector<MainWindowHelpSection> sections;
    QHash<QString, int> anchorCounts;
    bool inCodeFence = false;
    QString fenceMarker;

    sections.reserve(lines.size());
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("```")) || trimmed.startsWith(QStringLiteral("~~~"))) {
            const QString marker = trimmed.left(3);
            if (!inCodeFence) {
                inCodeFence = true;
                fenceMarker = marker;
            } else if (marker == fenceMarker) {
                inCodeFence = false;
                fenceMarker.clear();
            }
            continue;
        }

        if (inCodeFence) {
            continue;
        }

        const QRegularExpressionMatch match = headingExpression.match(line);
        if (!match.hasMatch()) {
            continue;
        }

        const QString baseSlug = anchorSlugForHeading(match.captured(2));
        const int occurrence = anchorCounts.value(baseSlug) + 1;
        anchorCounts.insert(baseSlug, occurrence);

        MainWindowHelpSection section;
        section.level = match.captured(1).size();
        section.title = plainHeadingText(match.captured(2));
        section.anchor = occurrence == 1
                             ? baseSlug
                             : QStringLiteral("%1-%2").arg(baseSlug).arg(occurrence);
        sections.append(section);
    }

    return sections;
}

QString markdownToHtmlWithHeadingAnchors(const QString &markdown)
{
    static const QRegularExpression headingTagExpression(QStringLiteral("<h[1-6]\\b[^>]*>"),
                                                         QRegularExpression::CaseInsensitiveOption);

    QTextDocument document;
    document.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
    QString html = document.toHtml();

    const QVector<MainWindowHelpSection> sections = parseMarkdownHelpSections(markdown);
    QRegularExpressionMatchIterator matches = headingTagExpression.globalMatch(html);
    int insertedLength = 0;
    int sectionIndex = 0;
    while (matches.hasNext() && sectionIndex < sections.size()) {
        const QRegularExpressionMatch match = matches.next();
        const QString anchorHtml = QStringLiteral("<a name=\"%1\" id=\"%1\"></a>")
                                       .arg(sections.at(sectionIndex).anchor.toHtmlEscaped());
        html.insert(match.capturedEnd() + insertedLength, anchorHtml);
        insertedLength += anchorHtml.size();
        ++sectionIndex;
    }

    return html;
}
}
