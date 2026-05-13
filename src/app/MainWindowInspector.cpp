#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"
#include "MainWindowStructureRoles.h"

#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include "TextEditorTab.h"
#include "MapEditorTab.h"
#include "../core/DocumentFile.h"
#include "../core/TherionDocumentEditor.h"

namespace
{
QString inspectorObjectKindLabel(const QString &category)
{
    if (category == QStringLiteral("Stations")) {
        return QObject::tr("Station Point");
    }
    if (category == QStringLiteral("Points")) {
        return QObject::tr("Point");
    }
    if (category == QStringLiteral("Lines")) {
        return QObject::tr("Line");
    }
    if (category == QStringLiteral("Areas")) {
        return QObject::tr("Area");
    }
    if (category == QStringLiteral("Scraps")) {
        return QObject::tr("Scrap");
    }
    if (category == QStringLiteral("Maps")) {
        return QObject::tr("Map");
    }
    if (category == QStringLiteral("Surveys")) {
        return QObject::tr("Survey");
    }
    if (category == QStringLiteral("Centrelines")) {
        return QObject::tr("Centreline");
    }

    return category;
}

QString inspectorNameLabelForCategory(const QString &category)
{
    if (category == QStringLiteral("Stations")) {
        return QObject::tr("Station Name");
    }
    if (category == QStringLiteral("Points")) {
        return QObject::tr("Point Name");
    }
    if (category == QStringLiteral("Lines")) {
        return QObject::tr("Line Name");
    }
    if (category == QStringLiteral("Areas")) {
        return QObject::tr("Area Name");
    }
    if (category == QStringLiteral("Scraps")) {
        return QObject::tr("Scrap Name");
    }
    if (category == QStringLiteral("Maps")) {
        return QObject::tr("Map Name");
    }
    if (category == QStringLiteral("Surveys")) {
        return QObject::tr("Survey Name");
    }

    return QObject::tr("Name");
}
}

void MainWindow::buildInspector()
{
    if (mapEditorSidebarSplitter_ == nullptr) {
        return;
    }

    auto *inspectorFrame = new QFrame(mapEditorSidebarSplitter_);
    inspectorFrame->setFrameShape(QFrame::StyledPanel);

    auto *inspectorLayout = new QVBoxLayout(inspectorFrame);
    inspectorLayout->setContentsMargins(12, 12, 12, 12);
    inspectorLayout->setSpacing(8);

    inspectorSummary_ = new QLabel(tr("Select a structure item to inspect it."), inspectorFrame);
    inspectorSummary_->setWordWrap(true);
    inspectorSummary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    inspectorLayout->addWidget(inspectorSummary_);

    auto *form = new QFormLayout;

    inspectorCategoryLabel_ = new QLabel(tr("Category"), inspectorFrame);
    inspectorCategoryValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorNameLabel_ = new QLabel(tr("Name"), inspectorFrame);
    inspectorNameEdit_ = new QLineEdit(inspectorFrame);
    inspectorNameEdit_->setEnabled(false);
    connect(inspectorNameEdit_, &QLineEdit::textEdited, this, &MainWindow::handleInspectorNameEdited);

    inspectorProjectValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorRelativePathValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorObjectKindValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorEditabilityValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorSourceValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorLineValue_ = new QLabel(tr("-"), inspectorFrame);
    inspectorValidationLabel_ = new QLabel(tr("Select a structure item to edit its metadata."), inspectorFrame);
    inspectorValidationLabel_->setWordWrap(true);
    inspectorValidationLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    form->addRow(inspectorCategoryLabel_, inspectorCategoryValue_);
    form->addRow(inspectorNameLabel_, inspectorNameEdit_);
    form->addRow(tr("Project"), inspectorProjectValue_);
    form->addRow(tr("Relative Path"), inspectorRelativePathValue_);
    form->addRow(tr("Object Kind"), inspectorObjectKindValue_);
    form->addRow(tr("Editability"), inspectorEditabilityValue_);
    form->addRow(tr("Source File"), inspectorSourceValue_);
    form->addRow(tr("Line"), inspectorLineValue_);

    inspectorLayout->addLayout(form);
    inspectorLayout->addWidget(inspectorValidationLabel_);

    auto *buttonRow = new QHBoxLayout;
    inspectorOpenSourceButton_ = new QPushButton(tr("Open Source"), inspectorFrame);
    inspectorOpenSourceButton_->setEnabled(false);
    connect(inspectorOpenSourceButton_, &QPushButton::clicked, this, &MainWindow::openSelectedStructureSource);

    inspectorApplyButton_ = new QPushButton(tr("Apply"), inspectorFrame);
    inspectorApplyButton_->setEnabled(false);
    connect(inspectorApplyButton_, &QPushButton::clicked, this, &MainWindow::handleInspectorApplyTriggered);

    buttonRow->addWidget(inspectorOpenSourceButton_);
    buttonRow->addWidget(inspectorApplyButton_);
    buttonRow->addStretch(1);
    inspectorLayout->addLayout(buttonRow);

    mapEditorSidebarSplitter_->addWidget(inspectorFrame);
    mapEditorSidebarSplitter_->setStretchFactor(0, 3);
    mapEditorSidebarSplitter_->setStretchFactor(1, 2);
    mapEditorSidebarSplitter_->setCollapsible(1, true);
}

void MainWindow::updateInspectorFromStructureItem(const QModelIndex &index)
{
    if (!index.isValid()) {
        inspectorSummary_->setText(projectStructureSummary_.isEmpty() ? tr("Open a project to view structure details.") : projectStructureSummary_);
        inspectorCategoryLabel_->setText(tr("Category"));
        inspectorNameLabel_->setText(tr("Name"));
        inspectorCategoryValue_->setText(tr("-"));
        inspectorNameEdit_->setText(QString());
        inspectorNameEdit_->setEnabled(false);
        inspectorProjectValue_->setText(tr("-"));
        inspectorRelativePathValue_->setText(tr("-"));
        inspectorObjectKindValue_->setText(tr("-"));
        inspectorEditabilityValue_->setText(tr("-"));
        inspectorSourceValue_->setText(tr("-"));
        inspectorLineValue_->setText(tr("-"));
        inspectorValidationLabel_->setText(tr("Select a structure item to edit its metadata."));
        inspectorApplyButton_->setEnabled(false);
        inspectorOpenSourceButton_->setEnabled(false);
        selectedStructureIndex_ = QPersistentModelIndex();
        selectedStructureSourceFile_.clear();
        selectedStructureName_.clear();
        selectedStructureLine_ = 0;
        return;
    }

    const QString category = index.data(CategoryRole).toString();
    const QString name = index.data(NameRole).toString();
    const QString sourceFile = index.data(SourceFileRole).toString();
    const int lineNumber = index.data(LineNumberRole).toInt();

    const bool isProjectNode = category == QStringLiteral("Project");
    const bool isContainerNode = sourceFile.isEmpty() || lineNumber <= 0 || isProjectNode;

    if (isProjectNode) {
        selectedStructureIndex_ = QPersistentModelIndex(index);
        inspectorSummary_->setText(projectStructureSummary_);
        inspectorCategoryLabel_->setText(tr("Node Type"));
        inspectorNameLabel_->setText(tr("Project"));
        inspectorCategoryValue_->setText(tr("Project"));
        inspectorNameEdit_->setEnabled(false);
        inspectorNameEdit_->setText(name);
        inspectorSourceValue_->setText(QDir(projectRootPath_).absolutePath());
        inspectorLineValue_->setText(tr("-"));
        inspectorValidationLabel_->setText(tr("Project nodes summarize the current project and are not directly editable."));
        inspectorApplyButton_->setEnabled(false);
        inspectorOpenSourceButton_->setText(tr("Open First Item"));
        inspectorOpenSourceButton_->setEnabled(selectedStructureIndex_.isValid() ? firstStructureSourceIndex(selectedStructureIndex_).isValid() : false);
        return;
    }

    if (category.isEmpty() || name.isEmpty()) {
        inspectorSummary_->setText(projectStructureSummary_.isEmpty() ? tr("Open a project to view structure details.") : projectStructureSummary_);
        inspectorCategoryLabel_->setText(tr("Category"));
        inspectorNameLabel_->setText(tr("Name"));
        inspectorCategoryValue_->setText(tr("-"));
        inspectorNameEdit_->setText(QString());
        inspectorNameEdit_->setEnabled(false);
        inspectorProjectValue_->setText(tr("-"));
        inspectorRelativePathValue_->setText(tr("-"));
        inspectorObjectKindValue_->setText(tr("-"));
        inspectorEditabilityValue_->setText(tr("-"));
        inspectorSourceValue_->setText(tr("-"));
        inspectorLineValue_->setText(tr("-"));
        inspectorValidationLabel_->setText(tr("The selected row is not a structure object that can be edited."));
        inspectorApplyButton_->setEnabled(false);
        inspectorOpenSourceButton_->setText(tr("Open Source"));
        inspectorOpenSourceButton_->setEnabled(false);
        selectedStructureIndex_ = QPersistentModelIndex();
        selectedStructureSourceFile_.clear();
        selectedStructureName_.clear();
        selectedStructureLine_ = 0;
        return;
    }

    selectedStructureIndex_ = QPersistentModelIndex(index);
    selectedStructureSourceFile_ = sourceFile;
    selectedStructureName_ = name;
    selectedStructureLine_ = lineNumber;

    const QString objectKind = inspectorObjectKindLabel(category);
    const QString nameLabel = inspectorNameLabelForCategory(category);
    const QString relativePath = QDir(projectRootPath_).relativeFilePath(sourceFile);
    const QString projectName = QFileInfo(projectRootPath_).fileName().isEmpty() ? projectRootPath_ : QFileInfo(projectRootPath_).fileName();

    inspectorSummary_->setText(tr("%1: %2\nSource: %3:%4")
                                   .arg(objectKind)
                                   .arg(name)
                                   .arg(relativePath)
                                   .arg(lineNumber));
    inspectorCategoryLabel_->setText(tr("Object Kind"));
    inspectorNameLabel_->setText(nameLabel);
    inspectorCategoryValue_->setText(category);
    inspectorNameEdit_->setEnabled(!isContainerNode);
    inspectorNameEdit_->setText(name);
    inspectorProjectValue_->setText(projectName);
    inspectorRelativePathValue_->setText(relativePath);
    inspectorObjectKindValue_->setText(objectKind);
    inspectorEditabilityValue_->setText(isContainerNode ? tr("Container") : tr("Editable in-memory"));
    inspectorSourceValue_->setText(sourceFile);
    inspectorLineValue_->setText(lineNumber > 0 ? QString::number(lineNumber) : tr("-"));
    inspectorOpenSourceButton_->setText(isContainerNode ? tr("Open First Item") : tr("Open Source"));
    inspectorOpenSourceButton_->setEnabled(isContainerNode ? firstStructureSourceIndex(index).isValid() : true);
    inspectorValidationLabel_->setText(isContainerNode
                                           ? tr("This row groups child objects; use Open First Item or double-click to drill down.")
                                           : tr("Editing is currently in-memory for the selected structure entry."));
    inspectorApplyButton_->setEnabled(false);
}

void MainWindow::handleInspectorNameEdited(const QString &text)
{
    const bool canApply = selectedStructureIndex_.isValid() && !text.trimmed().isEmpty() && text.trimmed() != selectedStructureName_;
    if (!text.trimmed().isEmpty()) {
        inspectorValidationLabel_->setText(tr("Editing is currently in-memory for the selected structure entry."));
    } else {
        inspectorValidationLabel_->setText(tr("Name is required."));
    }
    inspectorApplyButton_->setEnabled(canApply);
}

void MainWindow::handleInspectorApplyTriggered()
{
    if (!selectedStructureIndex_.isValid()) {
        return;
    }

    const QString newName = inspectorNameEdit_->text().trimmed();
    if (newName.isEmpty()) {
        inspectorValidationLabel_->setText(tr("Name is required."));
        inspectorApplyButton_->setEnabled(false);
        return;
    }

    if (newName == selectedStructureName_) {
        inspectorValidationLabel_->setText(tr("No changes to apply."));
        inspectorApplyButton_->setEnabled(false);
        return;
    }

    const QString category = selectedStructureIndex_.data(CategoryRole).toString();
    const QString sourceFile = selectedStructureIndex_.data(SourceFileRole).toString();
    const int lineNumber = selectedStructureIndex_.data(LineNumberRole).toInt();
    if (category.isEmpty() || sourceFile.isEmpty()) {
        inspectorValidationLabel_->setText(tr("The selected structure item cannot be updated."));
        inspectorApplyButton_->setEnabled(false);
        return;
    }

    QString errorMessage;
    QWidget *tabWidget = documentWidgetForFilePath(sourceFile);
    if (auto *textTab = qobject_cast<TherionStudio::TextEditorTab *>(tabWidget)) {
        if (!textTab->rewriteStructureEntryName(lineNumber, category, newName, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!textTab->save(&errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }
    } else if (auto *mapTab = qobject_cast<TherionStudio::MapEditorTab *>(tabWidget)) {
        if (!mapTab->rewriteStructureEntryName(lineNumber, category, newName, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!mapTab->save(&errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }
    } else {
        QString contents;
        if (!TherionStudio::DocumentFile::readUtf8TextFile(sourceFile, &contents, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!TherionStudio::TherionDocumentEditor::rewriteStructureEntryName(&contents, lineNumber, category, newName, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }

        if (!TherionStudio::DocumentFile::writeUtf8TextFile(sourceFile, contents, &errorMessage)) {
            inspectorValidationLabel_->setText(errorMessage);
            inspectorApplyButton_->setEnabled(false);
            return;
        }
    }

    const QString overrideKey = structureOverrideKey(sourceFile, lineNumber);
    structureNameOverrides_.remove(overrideKey);
    saveStructureNameOverrides();
    rebuildStructureSidebar();
    rebuildMapObjectsTree();
    updateStructureSelectionFromEditorLocation(sourceFile, lineNumber);
    updateMapObjectSelectionFromEditorLocation(sourceFile, lineNumber);
    inspectorSummary_->setText(tr("%1: %2\nSource: %3:%4")
                                   .arg(inspectorObjectKindLabel(category))
                                   .arg(newName)
                                   .arg(QDir(projectRootPath_).relativeFilePath(sourceFile))
                                   .arg(lineNumber));
    inspectorValidationLabel_->setText(tr("Updated the selected structure item in the source document."));
    inspectorApplyButton_->setEnabled(false);
}

void MainWindow::openSelectedStructureSource()
{
    if (!selectedStructureIndex_.isValid()) {
        return;
    }

    QModelIndex sourceIndex = selectedStructureIndex_;
    if (isStructureContainerIndex(sourceIndex)) {
        sourceIndex = firstStructureSourceIndex(sourceIndex);
    }

    if (!sourceIndex.isValid()) {
        return;
    }

    const QString sourceFile = sourceIndex.data(SourceFileRole).toString();
    const int lineNumber = sourceIndex.data(LineNumberRole).toInt();
    if (sourceFile.isEmpty()) {
        return;
    }

    QWidget *tabWidget = nullptr;
    if (QFileInfo(sourceFile).suffix().toLower() == QStringLiteral("th2")) {
        tabWidget = openMapEditorTab(sourceFile);
    } else {
        tabWidget = openTextTab(sourceFile);
    }

    if (tabWidget != nullptr && lineNumber > 0) {
        documentGoToLineForWidget(tabWidget, lineNumber);
    }
}
