#pragma once

#include "DocumentFileInspector.h"
#include "InspectorPanel.h"

namespace TherionStudio
{

class DocumentInspectorPanel final : public InspectorPanel
{
    Q_OBJECT

public:
    explicit DocumentInspectorPanel(QWidget *parent = nullptr);

    DocumentFileInspector *addFileTab(DocumentFileInspectorContext context);
    DocumentFileInspector *fileInspector() const;

private:
    DocumentFileInspector *fileInspector_ = nullptr;
};

}
