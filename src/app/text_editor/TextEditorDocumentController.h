#pragma once

#include <QString>

namespace TherionStudio
{
class TextEditorTab;

class TextEditorDocumentController final
{
public:
    explicit TextEditorDocumentController(TextEditorTab *owner);

    bool loadFile(const QString &filePath, QString *errorMessage = nullptr);
    bool save(QString *errorMessage = nullptr);
    void setProjectRootPath(const QString &projectRootPath);

private:
    TextEditorTab *owner_ = nullptr;
};
}
