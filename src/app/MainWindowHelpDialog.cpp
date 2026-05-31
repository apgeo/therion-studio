#include "MainWindowHelpDialog.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLabel>
#include <QLocale>
#include <QPalette>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTextBrowser>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

#ifndef THERION_STUDIO_VERSION_STRING
#define THERION_STUDIO_VERSION_STRING "unknown"
#endif

#ifndef THERION_STUDIO_PACKAGE_LABEL_STRING
#define THERION_STUDIO_PACKAGE_LABEL_STRING THERION_STUDIO_VERSION_STRING
#endif

namespace TherionStudio
{
namespace
{
QString buildString(const char *value)
{
    return QString::fromUtf8(value);
}

QString aboutMarkdown()
{
    const QString version = buildString(THERION_STUDIO_VERSION_STRING);
    const QString packageLabel = buildString(THERION_STUDIO_PACKAGE_LABEL_STRING);
    const QString qtVersion = QString::fromLatin1(QT_VERSION_STR);
    const QString platform = QStringLiteral("%1 (%2)")
                                 .arg(QSysInfo::prettyProductName(),
                                      QSysInfo::currentCpuArchitecture());

    return QCoreApplication::translate(
               "MainWindowHelpDialog",
               "# Therion Studio\n"
               "\n"
               "Cross-platform Qt desktop editor for Therion cave surveying software.\n"
               "\n"
               "- **Version:** `%1`\n"
               "- **Build:** `%2`\n"
               "- **Qt:** `%3`\n"
               "- **Platform:** `%4`\n"
               "- **License:** GNU General Public License v3.0 or later (`GPL-3.0-or-later`)\n"
               "- **Maintainer:** Ladislav Blazek\n"
               "\n"
               "Repository: <https://github.com/ladislavb/therion-studio>\n"
               "\n"
               "Third-party notices: <https://github.com/ladislavb/therion-studio/blob/main/docs/THIRD_PARTY_NOTICES.md>\n"
               "\n"
               "Therion Studio does not bundle the external Therion compiler. Install Therion separately and configure it in Settings.\n")
        .arg(version, packageLabel, qtVersion, platform);
}

void appendUnique(QStringList &values, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty() && !values.contains(trimmed)) {
        values.append(trimmed);
    }
}

QStringList userManualLocaleTags()
{
    QStringList tags;
    const QLocale locale;
    for (const QString &uiLanguage : locale.uiLanguages()) {
        const QString normalized = uiLanguage.trimmed();
        appendUnique(tags, normalized);
        appendUnique(tags, normalized.toLower());
        appendUnique(tags, QString(normalized).replace(QLatin1Char('-'), QLatin1Char('_')));
        appendUnique(tags, QString(normalized).toLower().replace(QLatin1Char('-'), QLatin1Char('_')));

        const int separatorIndex = normalized.indexOf(QLatin1Char('-'));
        if (separatorIndex > 0) {
            appendUnique(tags, normalized.left(separatorIndex).toLower());
        }
    }

    const QString localeName = locale.name();
    if (localeName != QStringLiteral("C")) {
        appendUnique(tags, localeName);
        const int separatorIndex = localeName.indexOf(QLatin1Char('_'));
        if (separatorIndex > 0) {
            appendUnique(tags, localeName.left(separatorIndex).toLower());
        }
    }
    return tags;
}

QStringList userManualFileNames()
{
    QStringList fileNames;
    for (const QString &localeTag : userManualLocaleTags()) {
        appendUnique(fileNames, QStringLiteral("USER_MANUAL.%1.md").arg(localeTag));
    }
    appendUnique(fileNames, QStringLiteral("USER_MANUAL.md"));
    return fileNames;
}

QStringList userManualRootCandidates()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    return {
        QDir(cwd).absoluteFilePath(QStringLiteral("docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../../docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../share/doc/therion-studio")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../share/doc/therion-studio")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../share/doc/therion-studio")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../../share/doc/therion-studio"))};
}

QStringList userManualPathCandidates()
{
    QStringList candidates;
    const QStringList fileNames = userManualFileNames();
    const QStringList roots = userManualRootCandidates();
    for (const QString &fileName : fileNames) {
        for (const QString &root : roots) {
            appendUnique(candidates, QDir(root).absoluteFilePath(fileName));
        }
    }
    return candidates;
}

QString resolveUserManualPath()
{
    const QStringList candidates = userManualPathCandidates();
    for (const QString &candidatePath : candidates) {
        const QFileInfo info(candidatePath);
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return QString();
}

QString missingUserManualMarkdown()
{
    return QCoreApplication::translate(
        "MainWindowHelpDialog",
        "# User Manual\n"
        "\n"
        "The user manual file was not found.\n"
        "\n"
        "Expected files are `docs/USER_MANUAL.<language>.md` or `docs/USER_MANUAL.md`.\n");
}

QString loadUtf8TextFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

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

QStringList markdownHeadingAnchorSlugs(const QString &markdown)
{
    static const QRegularExpression headingExpression(QStringLiteral("^(#{1,6})\\s+(.+?)\\s*#*\\s*$"));

    const QStringList lines = markdown.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    QStringList anchors;
    QHash<QString, int> anchorCounts;
    bool inCodeFence = false;
    QString fenceMarker;

    anchors.reserve(lines.size());
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

        if (!inCodeFence) {
            const QRegularExpressionMatch match = headingExpression.match(line);
            if (match.hasMatch()) {
                const QString baseSlug = anchorSlugForHeading(match.captured(2));
                const int occurrence = anchorCounts.value(baseSlug) + 1;
                anchorCounts.insert(baseSlug, occurrence);
                const QString slug = occurrence == 1
                                         ? baseSlug
                                         : QStringLiteral("%1-%2").arg(baseSlug).arg(occurrence);
                anchors.append(slug);
            }
        }
    }

    return anchors;
}

QString markdownToHtmlWithHeadingAnchors(const QString &markdown)
{
    static const QRegularExpression headingTagExpression(QStringLiteral("<h[1-6]\\b[^>]*>"),
                                                         QRegularExpression::CaseInsensitiveOption);

    QTextDocument document;
    document.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
    QString html = document.toHtml();

    const QStringList anchors = markdownHeadingAnchorSlugs(markdown);
    QRegularExpressionMatchIterator matches = headingTagExpression.globalMatch(html);
    int insertedLength = 0;
    int anchorIndex = 0;
    while (matches.hasNext() && anchorIndex < anchors.size()) {
        const QRegularExpressionMatch match = matches.next();
        const QString anchorHtml = QStringLiteral("<a name=\"%1\" id=\"%1\"></a>")
                                       .arg(anchors.at(anchorIndex).toHtmlEscaped());
        html.insert(match.capturedEnd() + insertedLength, anchorHtml);
        insertedLength += anchorHtml.size();
        ++anchorIndex;
    }

    return html;
}

QString markdownStyleSheet(const QPalette &palette)
{
    const QString textColor = palette.color(QPalette::Text).name(QColor::HexRgb);
    const QString linkColor = palette.color(QPalette::Link).name(QColor::HexRgb);
    const QString borderColor = palette.color(QPalette::Mid).name(QColor::HexRgb);
    const QString codeBackground = palette.color(QPalette::AlternateBase).name(QColor::HexRgb);

    return QStringLiteral(
               "body { color: %1; font-size: 14px; line-height: 1.45; }"
               "h1 { font-size: 26px; margin-top: 0; margin-bottom: 18px; }"
               "h2 { font-size: 22px; margin-top: 28px; margin-bottom: 12px; }"
               "h3 { font-size: 18px; margin-top: 20px; margin-bottom: 8px; }"
               "h4 { font-size: 15px; margin-top: 16px; margin-bottom: 6px; }"
               "p { margin-top: 0; margin-bottom: 12px; }"
               "ul, ol { margin-top: 6px; margin-bottom: 14px; margin-left: 22px; }"
               "li { margin-bottom: 6px; }"
               "table { border-collapse: collapse; margin-top: 10px; margin-bottom: 18px; }"
               "th, td { border: 1px solid %3; padding: 6px 8px; }"
               "th { font-weight: bold; }"
               "code { background-color: %4; padding: 1px 3px; }"
               "pre { background-color: %4; margin-top: 10px; margin-bottom: 16px; padding: 10px; }"
               "a { color: %2; text-decoration: none; }")
        .arg(textColor, linkColor, borderColor, codeBackground);
}

void openHelpLink(QTextBrowser *browser, const QUrl &url)
{
    const QString fragment = url.fragment(QUrl::FullyDecoded);
    if (!fragment.isEmpty() && (url.isRelative() || url.scheme().isEmpty())) {
        browser->scrollToAnchor(fragment);
        return;
    }

    QDesktopServices::openUrl(url);
}

void showMarkdownDialog(QWidget *parent,
                        const QString &title,
                        const QString &markdown,
                        const QString &sourceLabel = QString())
{
    auto *dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle(title);
    dialog->resize(860, 680);

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    if (!sourceLabel.trimmed().isEmpty()) {
        auto *sourceInfo = new QLabel(sourceLabel, dialog);
        sourceInfo->setWordWrap(true);
        sourceInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(sourceInfo);
    }

    auto *browser = new QTextBrowser(dialog);
    browser->setOpenExternalLinks(false);
    browser->setOpenLinks(false);
    browser->setReadOnly(true);
    browser->document()->setDocumentMargin(18.0);
    browser->document()->setDefaultStyleSheet(markdownStyleSheet(browser->palette()));
    browser->setHtml(markdownToHtmlWithHeadingAnchors(markdown));
    QObject::connect(browser, &QTextBrowser::anchorClicked, browser, [browser](const QUrl &url) {
        openHelpLink(browser, url);
    });
    layout->addWidget(browser, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::close);
    layout->addWidget(buttons);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}
}

void showAboutDialog(QWidget *parent)
{
    showMarkdownDialog(parent,
                       QCoreApplication::translate("MainWindow", "About Therion Studio"),
                       aboutMarkdown());
}

void showUserManualDialog(QWidget *parent)
{
    const QString manualPath = resolveUserManualPath();
    if (manualPath.isEmpty()) {
        showMarkdownDialog(
            parent,
            QCoreApplication::translate("MainWindow", "User Manual"),
            missingUserManualMarkdown(),
            QCoreApplication::translate("MainWindow", "User manual file was not found in expected locations."));
        return;
    }

    const QString manualText = loadUtf8TextFile(manualPath);
    if (manualText.trimmed().isEmpty()) {
        showMarkdownDialog(
            parent,
            QCoreApplication::translate("MainWindow", "User Manual"),
            missingUserManualMarkdown(),
            QCoreApplication::translate("MainWindow", "Failed to load `%1`.")
                .arg(QDir::toNativeSeparators(manualPath)));
        return;
    }

    showMarkdownDialog(parent,
                       QCoreApplication::translate("MainWindow", "User Manual"),
                       manualText);
}
} // namespace TherionStudio
