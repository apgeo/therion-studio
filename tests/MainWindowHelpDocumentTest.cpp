#include "../src/app/MainWindowHelpDocument.h"

#include <iostream>

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}
}

int main()
{
    const QString markdown = QStringLiteral(
        "# Therion Studio User Manual\n"
        "\n"
        "## Contents\n"
        "\n"
        "## Visual Map Editing (`.th2`)\n"
        "\n"
        "### Navigation\n"
        "\n"
        "```text\n"
        "## Ignored Code Heading\n"
        "```\n"
        "\n"
        "## Visual Map Editing (`.th2`)\n");

    const QVector<TherionStudio::MainWindowHelpSection> sections =
        TherionStudio::parseMarkdownHelpSections(markdown);

    if (!expect(sections.size() == 5, "Expected five real markdown headings.")) {
        return 1;
    }
    if (!expect(sections.at(0).level == 1
                    && sections.at(0).title == QStringLiteral("Therion Studio User Manual")
                    && sections.at(0).anchor == QStringLiteral("therion-studio-user-manual"),
                "Unexpected top-level manual heading.")) {
        return 1;
    }
    if (!expect(sections.at(2).title == QStringLiteral("Visual Map Editing (.th2)")
                    && sections.at(2).anchor == QStringLiteral("visual-map-editing-th2"),
                "Expected cleaned title and stable anchor for first map heading.")) {
        return 1;
    }
    if (!expect(sections.at(3).level == 3
                    && sections.at(3).anchor == QStringLiteral("navigation"),
                "Expected nested Navigation heading.")) {
        return 1;
    }
    if (!expect(sections.at(4).anchor == QStringLiteral("visual-map-editing-th2-2"),
                "Expected duplicate heading anchor suffix.")) {
        return 1;
    }

    const QString html = TherionStudio::markdownToHtmlWithHeadingAnchors(markdown);
    if (!expect(html.contains(QStringLiteral("id=\"visual-map-editing-th2\""))
                    && html.contains(QStringLiteral("id=\"visual-map-editing-th2-2\""))
                    && !html.contains(QStringLiteral("ignored-code-heading")),
                "Expected HTML anchors for real headings only.")) {
        return 1;
    }

    return 0;
}
