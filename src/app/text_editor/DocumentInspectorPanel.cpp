#include "DocumentInspectorPanel.h"

#include <QVBoxLayout>

#include <utility>

namespace TherionStudio
{

DocumentInspectorPanel::DocumentInspectorPanel(QWidget *parent)
    : InspectorPanel(parent)
{
}

DocumentFileInspector *DocumentInspectorPanel::addFileTab(DocumentFileInspectorContext context)
{
    auto *fileTabPanel = addScrollTab(tr("File"));
    auto *fileTabLayout = qobject_cast<QVBoxLayout *>(fileTabPanel->layout());
    fileInspector_ = new DocumentFileInspector(std::move(context), fileTabPanel);
    if (fileTabLayout != nullptr) {
        fileTabLayout->addWidget(fileInspector_);
    }
    return fileInspector_;
}

DocumentFileInspector *DocumentInspectorPanel::fileInspector() const
{
    return fileInspector_;
}

}
