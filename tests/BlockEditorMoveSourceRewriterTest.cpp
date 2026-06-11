#include "../src/app/text_editor/block_editor/BlockEditorMoveSourceRewriter.h"
#include "../src/app/text_editor/block_editor/BlockEditorSourceText.h"
#include "../src/core/TherionDocumentEditor.h"

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runMoveRewriteSourceEditTest()
{
    const QString contents = QStringLiteral(
        "survey cave\n"
        "scrap first\n"
        "endscrap\n"
        "scrap second\n"
        "endscrap\n"
        "endsurvey\n");
    const QStringList lines = blockEditorNormalizedSourceLines(contents);
    const BlockEditorDocumentEntry sourceEntry{QStringLiteral("scrap"), 2, 3, 1};

    const BlockEditorMoveSourceRewriter rewriter;
    const BlockEditorMoveRewriteResult rewriteResult = rewriter.rewriteMovedBlock(lines, sourceEntry, 6);
    if (!expect(rewriteResult.applied,
                "Block move rewriter should apply a valid move.")) {
        return 1;
    }

    const QString updatedContents = blockEditorJoinSourceLines(contents, rewriteResult.lines);
    if (!expect(updatedContents == QStringLiteral("survey cave\n"
                                                  "scrap second\n"
                                                  "endscrap\n"
                                                  "scrap first\n"
                                                  "endscrap\n"
                                                  "endsurvey\n"),
                "Block move rewriter should preserve the existing joined-output behavior.")) {
        return 1;
    }

    TherionSourceTextEdit edit;
    if (!expect(blockEditorSourceReplacementEdit(contents, updatedContents, &edit),
                "Block move source replacement should produce a source edit.")) {
        return 1;
    }
    if (!expect(edit.startOffset == QStringLiteral("survey cave\nscrap ").size()
                    && edit.length == QStringLiteral("first\nendscrap\nscrap second").size()
                    && edit.replacementText == QStringLiteral("second\nendscrap\nscrap first"),
                "Block move source replacement should expose the changed suffix as one edit.")) {
        return 1;
    }

    QString appliedContents = contents;
    appliedContents.replace(edit.startOffset, edit.length, edit.replacementText);
    if (!expect(appliedContents == updatedContents,
                "Block move source edit should apply to the rewritten contents.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    return runMoveRewriteSourceEditTest();
}
