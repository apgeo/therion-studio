#include "MainWindow.h"

#include "MainWindowValidationFixApplyService.h"
#include "../editor/ValidationSeverityStyle.h"
#include "text_editor/TextEditorValidationCatalog.h"
#include "text_editor/TextEditorTab.h"
#include "text_editor/map_editor/MapEditorTab.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>

namespace
{
enum ValidationResultRole
{
    ValidationFilePathRole = Qt::UserRole + 90,
    ValidationLineNumberRole,
    ValidationColumnNumberRole,
    ValidationIsFindingRole,
    ValidationDiagnosticIndexRole,
    ValidationSeverityRankRole,
};

int severityRank(TherionStudio::TherionSourceDiagnosticSeverity severity)
{
    switch (severity) {
    case TherionStudio::TherionSourceDiagnosticSeverity::Error:
        return 2;
    case TherionStudio::TherionSourceDiagnosticSeverity::Warning:
        return 1;
    }
    return 1;
}

TherionStudio::TherionSourceDiagnosticSeverity higherSeverity(
    TherionStudio::TherionSourceDiagnosticSeverity left,
    TherionStudio::TherionSourceDiagnosticSeverity right)
{
    return severityRank(left) >= severityRank(right) ? left : right;
}

QColor validationSeverityColor(TherionStudio::TherionSourceDiagnosticSeverity severity, const QPalette &palette)
{
    return TherionStudio::validationSeverityForeground(severity, palette);
}

void applyValidationSeverityStyle(QStandardItem *item, TherionStudio::TherionSourceDiagnosticSeverity severity)
{
    if (item == nullptr) {
        return;
    }
    const QPalette palette = qApp != nullptr ? qApp->palette() : QPalette();
    item->setForeground(QBrush(validationSeverityColor(severity, palette)));
    item->setData(severityRank(severity), ValidationSeverityRankRole);
}

void updateValidationFileSeverityStyle(QStandardItem *fileItem, TherionStudio::TherionSourceDiagnosticSeverity severity)
{
    if (fileItem == nullptr) {
        return;
    }
    const int currentRank = fileItem->data(ValidationSeverityRankRole).toInt();
    if (severityRank(severity) > currentRank) {
        applyValidationSeverityStyle(fileItem, severity);
    }
}

TherionStudio::TherionSourceDiagnosticSeverity highestDiagnosticSeverity(
    const QVector<TherionStudio::TherionSourceDiagnostic> &diagnostics)
{
    TherionStudio::TherionSourceDiagnosticSeverity highest =
        TherionStudio::TherionSourceDiagnosticSeverity::Warning;
    for (const TherionStudio::TherionSourceDiagnostic &diagnostic : diagnostics) {
        highest = higherSeverity(highest, diagnostic.severity);
    }
    return highest;
}

QString severityLabel(TherionStudio::TherionSourceDiagnosticSeverity severity)
{
    switch (severity) {
    case TherionStudio::TherionSourceDiagnosticSeverity::Error:
        return QCoreApplication::translate("TherionStudio::MainWindow", "Error");
    case TherionStudio::TherionSourceDiagnosticSeverity::Warning:
        return QCoreApplication::translate("TherionStudio::MainWindow", "Warning");
    }
    return QCoreApplication::translate("TherionStudio::MainWindow", "Warning");
}

QString validationDocumentLabel(const QString &displayName, const QString &filePath)
{
    if (!displayName.isEmpty()) {
        return displayName;
    }
    if (!filePath.isEmpty()) {
        return QFileInfo(filePath).fileName();
    }
    return QCoreApplication::translate("TherionStudio::MainWindow", "Untitled document");
}

QString validationRelativeDisplayPath(const QString &projectRootPath, const QString &filePath)
{
    if (projectRootPath.trimmed().isEmpty() || filePath.trimmed().isEmpty()) {
        return validationDocumentLabel(QFileInfo(filePath).fileName(), filePath);
    }

    const QString relativePath = QDir(projectRootPath).relativeFilePath(filePath);
    if (!relativePath.startsWith(QStringLiteral(".."))) {
        return QDir::toNativeSeparators(relativePath);
    }
    return QFileInfo(filePath).fileName();
}

QString normalizedValidationPath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

void configureValidationSourcePreview(QPlainTextEdit *edit)
{
    if (edit == nullptr) {
        return;
    }
    edit->setReadOnly(true);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    edit->setMaximumHeight(72);
}

QModelIndex validationFindingIndex(QStandardItemModel *model, const QModelIndex &index)
{
    if (model == nullptr || !index.isValid()) {
        return {};
    }

    if (index.data(ValidationIsFindingRole).toBool()) {
        return index;
    }
    if (model->hasChildren(index)) {
        return model->index(0, 0, index);
    }
    return {};
}

int validationDiagnosticIndex(QStandardItemModel *model, const QModelIndex &index)
{
    const QModelIndex findingIndex = validationFindingIndex(model, index);
    if (!findingIndex.isValid()) {
        return -1;
    }
    return findingIndex.data(ValidationDiagnosticIndexRole).toInt();
}
}

void MainWindow::buildValidationSidebar()
{
    if (sidebarPages_ == nullptr || validationResultsModel_ == nullptr) {
        return;
    }

    auto *validationPage = new QWidget(sidebarPages_);
    validationPage->setMinimumWidth(0);
    validationPage->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *validationLayout = new QVBoxLayout(validationPage);
    validationLayout->setContentsMargins(12, 12, 12, 12);
    validationLayout->setSpacing(8);

    auto *validationHeader = new QLabel(tr("Validate source files and review problems."), validationPage);
    validationHeader->setWordWrap(true);
    validationLayout->addWidget(validationHeader);

    validationStatusLabel_ = new QLabel(tr("Use Validate Document for the active file or Validate Project for the open project."), validationPage);
    validationStatusLabel_->setWordWrap(true);
    validationLayout->addWidget(validationStatusLabel_);

    validationScanProjectButton_ = new QPushButton(tr("Validate Project"), validationPage);
    connect(validationScanProjectButton_, &QPushButton::clicked, this, [this]() {
        requestProjectValidation();
    });
    validationLayout->addWidget(validationScanProjectButton_);

    validationResultsModel_->clear();
    validationResultsModel_->setHorizontalHeaderLabels({tr("Problems")});

    validationResultsTree_ = new QTreeView(validationPage);
    validationResultsTree_->setMinimumWidth(0);
    validationResultsTree_->setModel(validationResultsModel_);
    validationResultsTree_->setRootIsDecorated(true);
    validationResultsTree_->setAnimated(true);
    validationResultsTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    validationResultsTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    validationResultsTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    validationResultsTree_->setAlternatingRowColors(true);
    validationResultsTree_->header()->setStretchLastSection(true);
    connect(validationResultsTree_, &QTreeView::activated, this, &MainWindow::openValidationResult);
    connect(validationResultsTree_->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            &MainWindow::handleValidationSelectionChanged);
    validationLayout->addWidget(validationResultsTree_, 1);

    validationDetailTitleLabel_ = new QLabel(tr("Select a validation finding."), validationPage);
    validationDetailTitleLabel_->setWordWrap(true);
    validationLayout->addWidget(validationDetailTitleLabel_);

    validationDetailMessageLabel_ = new QLabel(validationPage);
    validationDetailMessageLabel_->setWordWrap(true);
    validationLayout->addWidget(validationDetailMessageLabel_);

    auto *currentLabel = new QLabel(tr("Current source line"), validationPage);
    validationLayout->addWidget(currentLabel);
    validationCurrentSourceEdit_ = new QPlainTextEdit(validationPage);
    configureValidationSourcePreview(validationCurrentSourceEdit_);
    validationLayout->addWidget(validationCurrentSourceEdit_);

    auto *suggestedLabel = new QLabel(tr("Suggested source line"), validationPage);
    validationLayout->addWidget(suggestedLabel);
    validationSuggestedSourceEdit_ = new QPlainTextEdit(validationPage);
    configureValidationSourcePreview(validationSuggestedSourceEdit_);
    validationLayout->addWidget(validationSuggestedSourceEdit_);

    auto *validationActionRow = new QHBoxLayout;
    validationActionRow->setContentsMargins(0, 0, 0, 0);
    validationActionRow->setSpacing(6);
    validationApplyFixButton_ = new QPushButton(tr("Apply Fix"), validationPage);
    validationApplyFixButton_->setEnabled(false);
    connect(validationApplyFixButton_, &QPushButton::clicked, this, &MainWindow::applySelectedValidationFix);
    validationActionRow->addWidget(validationApplyFixButton_);

    validationApplyAllFixesButton_ = new QPushButton(tr("Apply All Safe Fixes"), validationPage);
    validationApplyAllFixesButton_->setEnabled(false);
    connect(validationApplyAllFixesButton_, &QPushButton::clicked, this, &MainWindow::applyAllValidationFixes);
    validationActionRow->addWidget(validationApplyAllFixesButton_);
    validationLayout->addLayout(validationActionRow);

    sidebarPages_->addWidget(validationPage);
}

void MainWindow::triggerValidateDocumentForActiveDocument()
{
    TherionStudio::TherionSourceValidationResult validation;
    QString displayName;
    QString filePath;

    if (auto *textTab = currentTextTab(); textTab != nullptr) {
        validation = textTab->validateDocument();
        displayName = textTab->displayName();
        filePath = textTab->filePath();
    } else if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        validation = mapTab->validateDocument();
        displayName = mapTab->displayName();
        filePath = mapTab->filePath();
    } else {
        return;
    }

    if (validationResultsModel_ == nullptr) {
        return;
    }

    validationResultsModel_->clear();
    validationResultsModel_->setHorizontalHeaderLabels({tr("Problems")});
    validationDiagnostics_ = validation.diagnostics;
    validationDiagnosticFilePaths_.clear();
    validationDiagnosticFilePaths_.reserve(validation.diagnostics.size());
    for (qsizetype index = 0; index < validation.diagnostics.size(); ++index) {
        validationDiagnosticFilePaths_.append(filePath);
    }
    validationDocumentPath_ = filePath;
    validationProjectMode_ = false;

    const QString documentLabel = validationDocumentLabel(displayName, filePath);
    if (validation.diagnostics.isEmpty()) {
        clearValidationRailIndicator();
        if (validationStatusLabel_ != nullptr) {
            validationStatusLabel_->setText(tr("No validation problems found in %1.").arg(documentLabel));
        }
        handleValidationSelectionChanged({}, {});
        if (validationApplyAllFixesButton_ != nullptr) {
            validationApplyAllFixesButton_->setEnabled(false);
        }
        showSidebarPane(SidebarPane::Validation);
        return;
    }

    validationProblemCount_ = validation.diagnostics.size();
    validationHighestSeverity_ = highestDiagnosticSeverity(validation.diagnostics);
    updateValidationRailIndicator();

    auto *fileItem = new QStandardItem(tr("%1 (%2)").arg(documentLabel).arg(validation.diagnostics.size()));
    fileItem->setEditable(false);
    fileItem->setData(filePath, ValidationFilePathRole);
    fileItem->setData(false, ValidationIsFindingRole);
    applyValidationSeverityStyle(fileItem, validationHighestSeverity_);

    bool hasAnySafeFix = false;
    for (int diagnosticIndex = 0; diagnosticIndex < validation.diagnostics.size(); ++diagnosticIndex) {
        const TherionStudio::TherionSourceDiagnostic &diagnostic = validation.diagnostics.at(diagnosticIndex);
        hasAnySafeFix = hasAnySafeFix || diagnostic.hasFix;
        const QString fixSuffix = diagnostic.hasFix ? tr(" (safe fix available)") : QString();
        const QString label = tr("Line %1: %2: %3%4")
                                  .arg(diagnostic.lineNumber)
                                  .arg(severityLabel(diagnostic.severity))
                                  .arg(diagnostic.title)
                                  .arg(fixSuffix);
        auto *findingItem = new QStandardItem(label);
        findingItem->setEditable(false);
        applyValidationSeverityStyle(findingItem, diagnostic.severity);
        findingItem->setToolTip(diagnostic.message);
        findingItem->setData(filePath, ValidationFilePathRole);
        findingItem->setData(qMax(1, diagnostic.lineNumber), ValidationLineNumberRole);
        findingItem->setData(qMax(1, diagnostic.columnNumber), ValidationColumnNumberRole);
        findingItem->setData(true, ValidationIsFindingRole);
        findingItem->setData(diagnosticIndex, ValidationDiagnosticIndexRole);
        fileItem->appendRow(findingItem);
    }

    validationResultsModel_->appendRow(fileItem);

    if (validationStatusLabel_ != nullptr) {
        validationStatusLabel_->setText(tr("%1 validation problem(s) found in %2.")
                                            .arg(validation.diagnostics.size())
                                            .arg(documentLabel));
    }
    if (validationResultsTree_ != nullptr) {
        validationResultsTree_->expandAll();
        validationResultsTree_->resizeColumnToContents(0);
        const QModelIndex firstFinding = validationResultsModel_->index(0, 0, validationResultsModel_->index(0, 0));
        if (firstFinding.isValid()) {
            validationResultsTree_->setCurrentIndex(firstFinding);
            handleValidationSelectionChanged(firstFinding, {});
        }
    }
    if (validationApplyAllFixesButton_ != nullptr) {
        validationApplyAllFixesButton_->setEnabled(hasAnySafeFix);
    }
    showSidebarPane(SidebarPane::Validation);
}

void MainWindow::requestProjectValidation()
{
    requestProjectValidation(TherionStudio::ProjectValidationController::Trigger::ManualRefresh,
                             true);
}

void MainWindow::requestRestoredProjectValidation()
{
    if (!projectRootPath_.trimmed().isEmpty() && QDir(projectRootPath_).exists()) {
        requestProjectValidation(TherionStudio::ProjectValidationController::Trigger::ProjectOpened, false);
    }
}

void MainWindow::requestProjectValidation(TherionStudio::ProjectValidationController::Trigger trigger,
                                          bool revealPanel)
{
    if (projectValidationController_ == nullptr) {
        return;
    }

    if (projectRootPath_.isEmpty() || !QDir(projectRootPath_).exists()) {
        if (revealPanel && validationStatusLabel_ != nullptr) {
            validationStatusLabel_->setText(tr("Open a project before validating."));
        }
        if (revealPanel) {
            showSidebarPane(SidebarPane::Validation);
        }
        return;
    }

    QHash<QString, QString> inMemoryProjectContentsByPath;
    auto captureInMemorySource = [&inMemoryProjectContentsByPath](QWidget *widget) {
        if (widget == nullptr) {
            return;
        }

        QString sourceFile;
        QString sourceText;
        if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(widget)) {
            sourceFile = textTab->filePath();
            sourceText = textTab->text();
        } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(widget)) {
            sourceFile = mapTab->filePath();
            sourceText = mapTab->text();
        } else {
            return;
        }

        const QFileInfo info(sourceFile);
        const QString normalizedPath = info.canonicalFilePath().isEmpty()
            ? info.absoluteFilePath()
            : info.canonicalFilePath();
        if (!normalizedPath.isEmpty()) {
            inMemoryProjectContentsByPath.insert(normalizedPath, sourceText);
        }
    };

    for (int index = 0; editorTabs_ != nullptr && index < editorTabs_->count(); ++index) {
        captureInMemorySource(editorTabs_->widget(index));
    }
    for (TherionStudio::MapEditorTab *detachedTab : detachedMapEditorTabs()) {
        captureInMemorySource(detachedTab);
    }

    TherionStudio::ProjectValidationController::Request request;
    request.trigger = trigger;
    request.projectRootPath = projectRootPath_;
    request.validationCatalog = TherionStudio::validationCatalogFromCommandCatalog(commandCatalogStore_.catalogObject());
    request.inMemoryProjectContentsByPath = inMemoryProjectContentsByPath;
    pendingProjectValidationRevealPanel_ = revealPanel;
    projectValidationController_->requestValidation(request);
}

void MainWindow::handleProjectValidationStarted(TherionStudio::ProjectValidationController::Trigger trigger,
                                                quint64 generation,
                                                const QString &projectRootPath)
{
    if (normalizedValidationPath(projectRootPath) != normalizedValidationPath(projectRootPath_)) {
        return;
    }

    const bool revealPanel = pendingProjectValidationRevealPanel_;
    validationRevealByGeneration_.insert(generation, revealPanel);
    const bool replaceVisibleResults = revealPanel
        || trigger == TherionStudio::ProjectValidationController::Trigger::ManualRefresh
        || trigger == TherionStudio::ProjectValidationController::Trigger::ProjectOpened;
    if (replaceVisibleResults) {
        validationDiagnostics_.clear();
        validationDiagnosticFilePaths_.clear();
        validationDocumentPath_.clear();
        clearValidationRailIndicator();
        validationProjectMode_ = true;
        if (validationResultsModel_ != nullptr) {
            validationResultsModel_->clear();
            validationResultsModel_->setHorizontalHeaderLabels({tr("Problems")});
        }
        handleValidationSelectionChanged({}, {});
    }
    if (validationStatusLabel_ != nullptr) {
        validationStatusLabel_->setText(tr("Validating project..."));
    }
    if (validationScanProjectButton_ != nullptr) {
        validationScanProjectButton_->setEnabled(false);
    }
    if (validationApplyAllFixesButton_ != nullptr) {
        validationApplyAllFixesButton_->setEnabled(false);
    }
    if (revealPanel) {
        showSidebarPane(SidebarPane::Validation);
    }
}

void MainWindow::handleProjectValidationFinished(TherionStudio::ProjectValidationController::Trigger trigger,
                                                 const TherionStudio::ProjectValidationScanner::Result &result)
{
    Q_UNUSED(trigger)

    const bool revealPanel = validationRevealByGeneration_.take(result.generation);
    if (validationScanProjectButton_ != nullptr) {
        validationScanProjectButton_->setEnabled(true);
    }
    if (normalizedValidationPath(result.projectRootPath) != normalizedValidationPath(projectRootPath_)) {
        return;
    }
    if (validationResultsModel_ == nullptr) {
        return;
    }

    validationResultsModel_->clear();
    validationResultsModel_->setHorizontalHeaderLabels({tr("Problems")});
    validationDiagnostics_.clear();
    validationDiagnosticFilePaths_.clear();
    validationDocumentPath_.clear();
    validationProjectMode_ = true;

    if (!result.errorMessage.isEmpty()) {
        clearValidationRailIndicator();
        if (validationStatusLabel_ != nullptr) {
            validationStatusLabel_->setText(result.errorMessage);
        }
        handleValidationSelectionChanged({}, {});
        return;
    }

    if (result.findings.isEmpty()) {
        clearValidationRailIndicator();
        if (validationStatusLabel_ != nullptr) {
            validationStatusLabel_->setText(tr("No validation problems found in %1 searched file(s).")
                                                .arg(result.searchedFileCount));
        }
        handleValidationSelectionChanged({}, {});
        if (validationApplyAllFixesButton_ != nullptr) {
            validationApplyAllFixesButton_->setEnabled(false);
        }
        return;
    }

    validationProblemCount_ = result.findings.size();
    validationHighestSeverity_ = TherionStudio::TherionSourceDiagnosticSeverity::Warning;
    for (const TherionStudio::ProjectValidationScanner::Finding &finding : result.findings) {
        validationHighestSeverity_ = higherSeverity(validationHighestSeverity_, finding.diagnostic.severity);
    }
    updateValidationRailIndicator();

    bool hasAnySafeFix = false;
    QHash<QString, QStandardItem *> fileItemsByPath;
    for (const TherionStudio::ProjectValidationScanner::Finding &finding : result.findings) {
        QStandardItem *fileItem = fileItemsByPath.value(finding.filePath, nullptr);
        if (fileItem == nullptr) {
            fileItem = new QStandardItem(validationRelativeDisplayPath(projectRootPath_, finding.filePath));
            fileItem->setEditable(false);
            fileItem->setData(finding.filePath, ValidationFilePathRole);
            fileItem->setData(false, ValidationIsFindingRole);
            validationResultsModel_->appendRow(fileItem);
            fileItemsByPath.insert(finding.filePath, fileItem);
        }

        const int diagnosticIndex = validationDiagnostics_.size();
        validationDiagnostics_.append(finding.diagnostic);
        validationDiagnosticFilePaths_.append(finding.filePath);
        hasAnySafeFix = hasAnySafeFix || finding.diagnostic.hasFix;

        const QString fixSuffix = finding.diagnostic.hasFix ? tr(" (safe fix available)") : QString();
        const QString label = tr("Line %1: %2: %3%4")
                                  .arg(finding.diagnostic.lineNumber)
                                  .arg(severityLabel(finding.diagnostic.severity))
                                  .arg(finding.diagnostic.title)
                                  .arg(fixSuffix);
        auto *findingItem = new QStandardItem(label);
        findingItem->setEditable(false);
        applyValidationSeverityStyle(findingItem, finding.diagnostic.severity);
        updateValidationFileSeverityStyle(fileItem, finding.diagnostic.severity);
        findingItem->setToolTip(finding.diagnostic.message);
        findingItem->setData(finding.filePath, ValidationFilePathRole);
        findingItem->setData(qMax(1, finding.diagnostic.lineNumber), ValidationLineNumberRole);
        findingItem->setData(qMax(1, finding.diagnostic.columnNumber), ValidationColumnNumberRole);
        findingItem->setData(true, ValidationIsFindingRole);
        findingItem->setData(diagnosticIndex, ValidationDiagnosticIndexRole);
        fileItem->appendRow(findingItem);
    }

    if (validationStatusLabel_ != nullptr) {
        QString status = tr("%1 validation problem(s) found in %2 searched file(s).")
                             .arg(result.findings.size())
                             .arg(result.searchedFileCount);
        if (result.limitReached) {
            status += QLatin1Char(' ');
            status += tr("Showing the first %1 problem(s).").arg(result.findings.size());
        }
        validationStatusLabel_->setText(status);
    }
    if (validationResultsTree_ != nullptr) {
        validationResultsTree_->expandAll();
        validationResultsTree_->resizeColumnToContents(0);
        const QModelIndex firstFinding = validationResultsModel_->index(0, 0, validationResultsModel_->index(0, 0));
        if (firstFinding.isValid()) {
            validationResultsTree_->setCurrentIndex(firstFinding);
            handleValidationSelectionChanged(firstFinding, {});
        }
    }
    if (validationApplyAllFixesButton_ != nullptr) {
        validationApplyAllFixesButton_->setEnabled(hasAnySafeFix);
    }
    if (revealPanel) {
        showSidebarPane(SidebarPane::Validation);
    }
}

void MainWindow::handleValidationSelectionChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous)

    const int diagnosticIndex = validationDiagnosticIndex(validationResultsModel_, current);
    if (diagnosticIndex < 0 || diagnosticIndex >= validationDiagnostics_.size()) {
        if (validationDetailTitleLabel_ != nullptr) {
            validationDetailTitleLabel_->setText(tr("Select a validation finding."));
        }
        if (validationDetailMessageLabel_ != nullptr) {
            validationDetailMessageLabel_->clear();
        }
        if (validationCurrentSourceEdit_ != nullptr) {
            validationCurrentSourceEdit_->clear();
        }
        if (validationSuggestedSourceEdit_ != nullptr) {
            validationSuggestedSourceEdit_->clear();
        }
        if (validationApplyFixButton_ != nullptr) {
            validationApplyFixButton_->setEnabled(false);
        }
        return;
    }

    const TherionStudio::TherionSourceDiagnostic &diagnostic = validationDiagnostics_.at(diagnosticIndex);
    if (validationDetailTitleLabel_ != nullptr) {
        validationDetailTitleLabel_->setText(tr("Line %1: %2").arg(diagnostic.lineNumber).arg(diagnostic.title));
    }
    if (validationDetailMessageLabel_ != nullptr) {
        validationDetailMessageLabel_->setText(diagnostic.message);
    }
    if (validationCurrentSourceEdit_ != nullptr) {
        validationCurrentSourceEdit_->setPlainText(diagnostic.currentText);
    }
    if (validationSuggestedSourceEdit_ != nullptr) {
        validationSuggestedSourceEdit_->setPlainText(diagnostic.hasFix
                                                         ? diagnostic.suggestedText
                                                         : tr("No automatic fix is available for this finding."));
    }
    if (validationApplyFixButton_ != nullptr) {
        validationApplyFixButton_->setEnabled(diagnostic.hasFix);
    }
}

void MainWindow::openValidationResult(const QModelIndex &index)
{
    if (validationResultsModel_ == nullptr || !index.isValid()) {
        return;
    }

    QModelIndex findingIndex = validationFindingIndex(validationResultsModel_, index);
    if (!findingIndex.isValid() || !findingIndex.data(ValidationIsFindingRole).toBool()) {
        return;
    }

    const int lineNumber = qMax(1, findingIndex.data(ValidationLineNumberRole).toInt());
    const int columnNumber = qMax(1, findingIndex.data(ValidationColumnNumberRole).toInt());
    const QString filePath = findingIndex.data(ValidationFilePathRole).toString();

    if (!filePath.isEmpty()) {
        const auto openPlan = TherionStudio::MainWindowDocumentOpenController::planOpenProjectSearchResult(filePath);
        if (openPlan.action == TherionStudio::MainWindowDocumentOpenController::OpenProjectSearchResultAction::OpenMapDocument) {
            auto *mapTab = openMapEditorTab(filePath);
            if (mapTab == nullptr) {
                return;
            }

            mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
            mapTab->goToLine(lineNumber);
            return;
        }

        auto *textTab = openTextTab(filePath);
        if (textTab == nullptr) {
            return;
        }

        textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
        textTab->goToLineColumn(lineNumber, columnNumber);
        return;
    }

    if (auto *mapTab = currentMapEditorTab(); mapTab != nullptr) {
        mapTab->setWorkspaceMode(TherionStudio::MapEditorTab::WorkspaceMode::Raw);
        mapTab->goToLine(lineNumber);
        return;
    }

    if (auto *textTab = currentTextTab(); textTab != nullptr) {
        textTab->setEditorMode(TherionStudio::TextEditorTab::EditorMode::Raw);
        textTab->goToLineColumn(lineNumber, columnNumber);
    }
}

void MainWindow::applySelectedValidationFix()
{
    const int diagnosticIndex = validationResultsTree_ != nullptr
        ? validationDiagnosticIndex(validationResultsModel_, validationResultsTree_->currentIndex())
        : -1;
    if (diagnosticIndex < 0 || diagnosticIndex >= validationDiagnostics_.size()) {
        return;
    }

    const TherionStudio::TherionSourceDiagnostic &diagnostic = validationDiagnostics_.at(diagnosticIndex);
    if (!diagnostic.hasFix) {
        return;
    }

    const QString filePath = diagnosticIndex < validationDiagnosticFilePaths_.size()
        ? validationDiagnosticFilePaths_.at(diagnosticIndex)
        : QString();
    if (applyValidationFixesToValidatedDocument(filePath, {diagnostic.fix})) {
        if (validationProjectMode_) {
            requestProjectValidation();
        } else {
            triggerValidateDocumentForActiveDocument();
        }
    }
}

void MainWindow::applyAllValidationFixes()
{
    QHash<QString, QVector<TherionStudio::TherionSourceDiagnosticFix>> fixesByFilePath;
    for (int index = 0; index < validationDiagnostics_.size(); ++index) {
        const TherionStudio::TherionSourceDiagnostic &diagnostic = validationDiagnostics_.at(index);
        if (diagnostic.hasFix) {
            const QString filePath = index < validationDiagnosticFilePaths_.size()
                ? validationDiagnosticFilePaths_.at(index)
                : QString();
            fixesByFilePath[filePath].append(diagnostic.fix);
        }
    }
    if (fixesByFilePath.isEmpty()) {
        return;
    }

    bool changed = false;
    for (auto iterator = fixesByFilePath.cbegin(); iterator != fixesByFilePath.cend(); ++iterator) {
        changed = applyValidationFixesToValidatedDocument(iterator.key(), iterator.value()) || changed;
    }

    if (changed) {
        if (validationProjectMode_) {
            requestProjectValidation();
            return;
        }
        triggerValidateDocumentForActiveDocument();
    }
}

bool MainWindow::applyValidationFixesToValidatedDocument(const QString &filePath,
                                                        const QVector<TherionStudio::TherionSourceDiagnosticFix> &fixes)
{
    TherionStudio::MainWindowValidationFixApplyContext context;
    context.applyFixesToMapPath = [this](const QString &targetPath,
                                         const QVector<TherionStudio::TherionSourceDiagnosticFix> &targetFixes) {
        auto *mapTab = openMapEditorTab(targetPath);
        return mapTab != nullptr && mapTab->applyValidationFixes(targetFixes);
    };
    context.applyFixesToTextPath = [this](const QString &targetPath,
                                          const QVector<TherionStudio::TherionSourceDiagnosticFix> &targetFixes) {
        auto *textTab = openTextTab(targetPath);
        return textTab != nullptr && textTab->applyValidationFixes(targetFixes);
    };
    context.applyFixesToCurrentMap = [this](const QVector<TherionStudio::TherionSourceDiagnosticFix> &targetFixes) {
        auto *mapTab = currentMapEditorTab();
        return mapTab != nullptr && mapTab->applyValidationFixes(targetFixes);
    };
    context.applyFixesToCurrentText = [this](const QVector<TherionStudio::TherionSourceDiagnosticFix> &targetFixes) {
        auto *textTab = currentTextTab();
        return textTab != nullptr && textTab->applyValidationFixes(targetFixes);
    };

    return TherionStudio::MainWindowValidationFixApplyService::applyValidationFixes(filePath,
                                                                                     validationDocumentPath_,
                                                                                     fixes,
                                                                                     context);
}
