#include "RawEditorCompletionTokenContext.h"

#include "../../../core/TherionSourceLogicalDocument.h"

namespace TherionStudio
{
RawEditorCompletionTokenContext rawEditorCompletionTokenContextAtPosition(
    const TherionSourceLogicalDocument &logicalDocument,
    int lineNumber,
    int columnNumber)
{
    RawEditorCompletionTokenContext context;
    const TherionSourceLogicalCommand *command = logicalDocument.commandAtPhysicalLine(lineNumber);
    if (command == nullptr) {
        return context;
    }

    context.parsedLine = command->parsed;
    context.tokenIndexAtCursor = context.parsedLine.tokens.size();
    if (const TherionSourceLogicalTokenRange *tokenRange =
            logicalDocument.tokenAtPhysicalPosition(lineNumber, columnNumber)) {
        context.tokenIndexAtCursor = tokenRange->tokenIndex;
        context.cursorInsideToken = true;
        return context;
    }

    for (const TherionSourceLogicalTokenRange &tokenRange : command->tokenRanges) {
        if (tokenRange.type == TherionTokenType::Comment) {
            continue;
        }
        const TherionSourcePhysicalRange &range = tokenRange.physicalRange;
        if (range.lineNumber < lineNumber
            || (range.lineNumber == lineNumber
                && columnNumber > range.columnNumber + range.columnLength)) {
            context.tokenIndexAtCursor = tokenRange.tokenIndex + 1;
        }
    }

    return context;
}
}
