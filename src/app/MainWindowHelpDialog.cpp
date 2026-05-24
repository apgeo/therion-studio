#include "MainWindowHelpDialog.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace TherionStudio
{
namespace
{
QString mainWindowText(const char *text)
{
    return QCoreApplication::translate("MainWindow", text);
}

QString quickUserManualMarkdown()
{
    return QStringLiteral(
        "# Therion Studio Quick User Manual\n"
        "\n"
        "## Core workflow\n"
        "\n"
        "1. Open a project with `File -> Open Project...`.\n"
        "2. Open `.th`/`.th2` files from the Files sidebar.\n"
        "3. Use the Structure sidebar to navigate project hierarchy and source context.\n"
        "4. Use the map workspace for TH2 geometry edits and keep source text in sync.\n"
        "5. Save with `Save` or `Save All`.\n"
        "\n"
        "## Text workspace essentials\n"
        "\n"
        "- `.th` / `.thconfig`: use the document toolbar row above tabs for `Raw`/`Blocks` mode switching\n"
        "\n"
        "## Map workspace essentials\n"
        "\n"
        "- `Visual` and `Raw` workspace modes for TH2 tabs are in the document toolbar row above tabs\n"
        "- `Separate Map` / `Return Map` is available in the same document toolbar row\n"
        "- For TH2 tabs, map view controls `Zoom In`, `Zoom Out`, `Fit`, `Fit + BG` are in the same top toolbar (after `Undo`/`Redo`)\n"
        "- `Select`, `Point`, `Line`, `Freehand`, `Area`\n"
        "- `Insert Scrap`, `Complete Draft`, `Undo`, `Redo`\n"
        "\n"
        "### Line-vertex shortcuts\n"
        "\n"
        "- Split selected segment: `Insert` or `I`\n"
        "- Remove selected middle anchor: `Delete` or `Backspace`\n"
        "- Toggle smooth/corner: `S`\n"
        "\n"
        "## Full manual\n"
        "\n"
        "Use `Help -> User Manual (Full)` when you need complete workflow details.\n");
}

QStringList fullUserManualPathCandidates()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    return {
        QDir(cwd).absoluteFilePath(QStringLiteral("docs/USER_MANUAL.md")),
        QDir(appDir).absoluteFilePath(QStringLiteral("docs/USER_MANUAL.md")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../docs/USER_MANUAL.md")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../docs/USER_MANUAL.md")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../docs/USER_MANUAL.md")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../../docs/USER_MANUAL.md"))};
}

QString resolveFullUserManualPath()
{
    const QStringList candidates = fullUserManualPathCandidates();
    for (const QString &candidatePath : candidates) {
        const QFileInfo info(candidatePath);
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return QString();
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

void showQuickUserManualDialog(QWidget *parent)
{
    showMarkdownDialog(parent, mainWindowText("Quick User Manual"), quickUserManualMarkdown());
}

void showFullUserManualDialog(QWidget *parent)
{
    const QString manualPath = resolveFullUserManualPath();
    if (manualPath.isEmpty()) {
        showMarkdownDialog(
            parent,
            mainWindowText("User Manual (Full)"),
            quickUserManualMarkdown(),
            mainWindowText("Full manual file `docs/USER_MANUAL.md` was not found in expected locations. Showing quick manual instead."));
        return;
    }

    const QString manualText = loadUtf8TextFile(manualPath);
    if (manualText.trimmed().isEmpty()) {
        showMarkdownDialog(
            parent,
            mainWindowText("User Manual (Full)"),
            quickUserManualMarkdown(),
            mainWindowText("Failed to load `%1`. Showing quick manual instead.").arg(QDir::toNativeSeparators(manualPath)));
        return;
    }

    showMarkdownDialog(parent,
                       mainWindowText("User Manual (Full)"),
                       manualText,
                       mainWindowText("Source: %1").arg(QDir::toNativeSeparators(manualPath)));
}
} // namespace TherionStudio
