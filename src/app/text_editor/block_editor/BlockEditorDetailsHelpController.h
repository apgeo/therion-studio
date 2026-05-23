#pragma once

#include <functional>

#include <QChar>
#include <QString>

class QLabel;
class QLineEdit;
class QTextBrowser;

namespace TherionStudio
{
struct TextEditorCommandMetadata;
struct TherionHelpEntry;

struct BlockEditorDetailsHelpContext
{
    bool *tearingDown = nullptr;
    QTextBrowser *helpBrowser = nullptr;
    QLineEdit *idEdit = nullptr;
    QLabel *primaryFieldLabel = nullptr;
    QLineEdit *additionalPositionalEdit = nullptr;
    QLineEdit *commentEdit = nullptr;
    QChar *commentMarker = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
    std::function<QString()> selectedKind;
    std::function<QString(const QString &)> normalizeDirectiveToken;
    std::function<bool(const QString &)> isRequiredArgumentSignature;
    std::function<bool()> readingsTagEditorHasEditorFocus;
    std::function<bool()> isStructuredOptionsMode;
    std::function<bool()> isSimpleValueMode;
    std::function<bool()> isDataHeaderMode;
    std::function<QString(const char *)> translate;
};

class BlockEditorDetailsHelpController final
{
public:
    explicit BlockEditorDetailsHelpController(BlockEditorDetailsHelpContext context);

    void updateHelpForCurrentFocus();

private:
    QString tr(const char *text) const;
    bool hasRequiredContext() const;
    bool isDetailsModeWithEditableFields() const;
    QString buildIdHelpHtml(const TherionHelpEntry &commandHelpEntry) const;

    BlockEditorDetailsHelpContext context_;
};
}
