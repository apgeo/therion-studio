#include "MainWindow.h"

#include "ProjectTemplateService.h"

#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>

namespace
{
struct NewProjectLocationSelection
{
    QString projectName;
    QString parentDirectoryPath;

    QString projectPath() const
    {
        return QDir(parentDirectoryPath).filePath(projectName);
    }
};

QString defaultProjectParentDirectory(const TherionStudio::ISessionStore *sessionStore)
{
    if (sessionStore != nullptr) {
        const QString persistedDirectoryPath = QDir::cleanPath(sessionStore->lastProjectParentDirectory().trimmed());
        if (!persistedDirectoryPath.isEmpty() && QDir(persistedDirectoryPath).exists()) {
            return persistedDirectoryPath;
        }
    }
    const QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return !documentsPath.isEmpty() ? documentsPath : QDir::homePath();
}

bool requestNewProjectLocation(QWidget *parent,
                               const QString &dialogTitle,
                               const QString &defaultProjectName,
                               const TherionStudio::ISessionStore *sessionStore,
                               NewProjectLocationSelection *selection)
{
    if (selection == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(dialogTitle);
    dialog.setModal(true);

    auto *rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    auto *formLayout = new QFormLayout;
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setHorizontalSpacing(12);
    formLayout->setVerticalSpacing(10);

    auto *projectNameEdit = new QLineEdit(&dialog);
    projectNameEdit->setText(defaultProjectName);
    projectNameEdit->selectAll();
    formLayout->addRow(QObject::tr("Project Name"), projectNameEdit);

    auto *locationRow = new QWidget(&dialog);
    auto *locationLayout = new QHBoxLayout(locationRow);
    locationLayout->setContentsMargins(0, 0, 0, 0);
    locationLayout->setSpacing(6);

    auto *locationEdit = new QLineEdit(locationRow);
    locationEdit->setText(defaultProjectParentDirectory(sessionStore));
    locationEdit->setCursorPosition(locationEdit->text().size());
    auto *browseButton = new QPushButton(QObject::tr("Browse..."), locationRow);
    locationLayout->addWidget(locationEdit, 1);
    locationLayout->addWidget(browseButton);
    formLayout->addRow(QObject::tr("Location"), locationRow);

    auto *pathPreviewLabel = new QLabel(&dialog);
    pathPreviewLabel->setWordWrap(true);
    pathPreviewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathPreviewLabel->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); }"));

    const auto updatePreview = [projectNameEdit, locationEdit, pathPreviewLabel]() {
        const QString previewPath = QDir(locationEdit->text().trimmed()).filePath(projectNameEdit->text().trimmed());
        pathPreviewLabel->setText(QDir::toNativeSeparators(QDir::cleanPath(previewPath)));
    };

    QObject::connect(projectNameEdit, &QLineEdit::textChanged, &dialog, updatePreview);
    QObject::connect(locationEdit, &QLineEdit::textChanged, &dialog, updatePreview);
    QObject::connect(browseButton, &QPushButton::clicked, &dialog, [&dialog, locationEdit, sessionStore]() {
        const QString selectedDirectory =
            QFileDialog::getExistingDirectory(&dialog,
                                              QObject::tr("Choose Project Location"),
                                              locationEdit != nullptr ? locationEdit->text() : defaultProjectParentDirectory(sessionStore));
        if (!selectedDirectory.isEmpty() && locationEdit != nullptr) {
            locationEdit->setText(QDir::cleanPath(selectedDirectory));
            locationEdit->setCursorPosition(locationEdit->text().size());
        }
    });
    updatePreview();

    rootLayout->addLayout(formLayout);

    auto *pathPreviewTitleLabel = new QLabel(QObject::tr("Project Folder"), &dialog);
    QFont pathPreviewTitleFont = pathPreviewTitleLabel->font();
    pathPreviewTitleFont.setBold(true);
    pathPreviewTitleLabel->setFont(pathPreviewTitleFont);
    rootLayout->addWidget(pathPreviewTitleLabel);
    rootLayout->addWidget(pathPreviewLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    rootLayout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, projectNameEdit, locationEdit]() {
        const QString projectName = projectNameEdit != nullptr ? projectNameEdit->text().trimmed() : QString();
        const QString parentDirectoryPath = locationEdit != nullptr ? QDir::cleanPath(locationEdit->text().trimmed()) : QString();
        if (projectName.isEmpty()) {
            QMessageBox::warning(&dialog, QObject::tr("New Project"), QObject::tr("Enter a project name."));
            return;
        }
        if (parentDirectoryPath.isEmpty()) {
            QMessageBox::warning(&dialog, QObject::tr("New Project"), QObject::tr("Choose a project location."));
            return;
        }
        if (!QDir(parentDirectoryPath).exists()) {
            QMessageBox::warning(&dialog,
                                 QObject::tr("New Project"),
                                 QObject::tr("The selected project location does not exist."));
            return;
        }
        dialog.accept();
    });

    dialog.resize(640, dialog.sizeHint().height());
    projectNameEdit->setFocus();
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    selection->projectName = projectNameEdit->text().trimmed();
    selection->parentDirectoryPath = QDir::cleanPath(locationEdit->text().trimmed());
    return true;
}
}

void MainWindow::createProjectFromDefaultTemplate()
{
    NewProjectLocationSelection selection;
    if (!requestNewProjectLocation(this,
                                   tr("New Project from Template"),
                                   tr("New Project"),
                                   sessionStore_,
                                   &selection)) {
        return;
    }

    const auto result = TherionStudio::ProjectTemplateService::createProjectFromTemplate(
        TherionStudio::ProjectTemplateService::defaultTemplateResourcePath(),
        selection.projectPath());
    if (!result.success) {
        QMessageBox::warning(this, tr("New Project from Template"), result.errorMessage);
        return;
    }
    if (sessionStore_ != nullptr) {
        sessionStore_->setLastProjectParentDirectory(selection.parentDirectoryPath);
    }

    if (sessionStore_ != nullptr) {
        sessionStore_->setTherionRunTargetMode(QStringLiteral("project"));
        sessionStore_->setTherionTargetConfigPath(result.targetConfigPath);
    }
    if (therionRunTargetCombo_ != nullptr) {
        const int projectIndex = therionRunTargetCombo_->findData(QStringLiteral("project"));
        if (projectIndex >= 0) {
            therionRunTargetCombo_->setCurrentIndex(projectIndex);
        }
    }
    if (therionTargetConfigEdit_ != nullptr) {
        therionTargetConfigEdit_->setText(result.targetConfigPath);
        therionTargetConfigEdit_->setCursorPosition(therionTargetConfigEdit_->text().size());
    }

    openProjectPath(result.projectRootPath);
    for (const QString &filePath : result.openFilePaths) {
        openProjectFilePath(filePath);
    }
    statusBar()->showMessage(tr("Created project from template"), 2000);
}

void MainWindow::createEmptyProject()
{
    NewProjectLocationSelection selection;
    if (!requestNewProjectLocation(this,
                                   tr("New Empty Project"),
                                   tr("New Project"),
                                   sessionStore_,
                                   &selection)) {
        return;
    }

    const QString targetProjectPath = selection.projectPath();
    const QDir targetDirectory(QDir::cleanPath(targetProjectPath));
    if (targetDirectory.exists() &&
        !targetDirectory.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty()) {
        QMessageBox::warning(this,
                             tr("New Empty Project"),
                             tr("The target project folder must be empty."));
        return;
    }
    if (!targetDirectory.exists() && !QDir().mkpath(targetProjectPath)) {
        QMessageBox::warning(this,
                             tr("New Empty Project"),
                             tr("The target project folder could not be created."));
        return;
    }
    if (sessionStore_ != nullptr) {
        sessionStore_->setLastProjectParentDirectory(selection.parentDirectoryPath);
    }

    openProjectPath(targetDirectory.absolutePath());
    statusBar()->showMessage(tr("Created empty project"), 2000);
}
