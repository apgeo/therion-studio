#pragma once

#include <QString>
#include <QVector>

namespace TherionStudio
{
struct MainWindowHelpSection
{
    int level = 1;
    QString title;
    QString anchor;
};

QVector<MainWindowHelpSection> parseMarkdownHelpSections(const QString &markdown);
QString markdownToHtmlWithHeadingAnchors(const QString &markdown);
}
