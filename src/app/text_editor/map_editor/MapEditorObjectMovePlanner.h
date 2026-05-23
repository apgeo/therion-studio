#pragma once

#include <QString>

namespace TherionStudio
{
enum class MapEditorObjectMovePosition
{
    BeforeTarget,
    AfterTarget,
    IntoTargetScrap
};

struct MapEditorObjectMovePlan
{
    bool resolved = false;
    bool changed = false;
    int sourceStartLine = 0;
    int sourceEndLine = 0;
    int insertBeforeLineOriginal = 0;
    int insertBeforeLineAfterRemoval = 0;
    QString movedText;
    QString errorMessage;
};

class MapEditorObjectMovePlanner final
{
public:
    [[nodiscard]] static MapEditorObjectMovePlan planMove(const QString &text,
                                                          int sourceLineNumber,
                                                          int targetLineNumber,
                                                          MapEditorObjectMovePosition position);

    [[nodiscard]] static bool applyMove(QString *text,
                                        int sourceLineNumber,
                                        int targetLineNumber,
                                        MapEditorObjectMovePosition position,
                                        QString *errorMessage = nullptr);
};
}
