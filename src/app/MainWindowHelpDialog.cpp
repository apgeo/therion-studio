#include "MainWindowHelpDialog.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLocale>
#include <QSysInfo>
#include <QTextBrowser>
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
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../../docs"))};
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
    browser->setOpenExternalLinks(true);
    browser->setReadOnly(true);
    browser->document()->setMarkdown(markdown);
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
                       manualText,
                       QCoreApplication::translate("MainWindow", "Source: %1")
                           .arg(QDir::toNativeSeparators(manualPath)));
}
} // namespace TherionStudio
