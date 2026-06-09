#include "../src/app/text_editor/TextEditorOptionValidation.h"

#include <QCoreApplication>

#include <cstdio>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "TextEditorOptionValidationTest failed: %s\n", message);
        std::exit(1);
    }
}

void deduplicatesIdenticalSerializedOptions()
{
    const TherionStudio::TextEditorOptionValidationResult result =
        TherionStudio::validateAndSerializeCommandOptions(
            QStringLiteral("line"),
            {TherionStudio::TextEditorOptionRow{QStringLiteral("-clip"), QStringLiteral("off"), 1},
             TherionStudio::TextEditorOptionRow{QStringLiteral("-clip"), QStringLiteral("off"), 2},
             TherionStudio::TextEditorOptionRow{QStringLiteral("-close"), QStringLiteral("on"), 3}},
            {},
            {},
            {},
            {},
            false);

    require(result.ok, "duplicate valid option rows should still validate");
    require(result.serializedOptions == QStringList({QStringLiteral("-clip"),
                                                     QStringLiteral("off"),
                                                     QStringLiteral("-close"),
                                                     QStringLiteral("on")}),
            "duplicate identical option rows should serialize only once");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    deduplicatesIdenticalSerializedOptions();
    return 0;
}
