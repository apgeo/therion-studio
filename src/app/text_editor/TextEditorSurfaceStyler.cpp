#include "TextEditorSurfaceStyler.h"

#include <QApplication>
#include <QColor>
#include <QTextBrowser>
#include <QTextDocument>
#include <QWidget>

namespace TherionStudio
{
void syncTextBrowserSurfaceToParent(QWidget *browserWidget)
{
    QWidget *parent = browserWidget != nullptr ? browserWidget->parentWidget() : nullptr;
    syncTextBrowserSurfaceToPalette(browserWidget,
                                    parent != nullptr ? parent->palette() : QApplication::palette(browserWidget));
}

void syncTextBrowserSurfaceToPalette(QWidget *browserWidget, const QPalette &sourcePalette)
{
    auto *browser = qobject_cast<QTextBrowser *>(browserWidget);
    if (browser == nullptr) {
        return;
    }

    QColor surfaceColor = sourcePalette.color(QPalette::Base);
    if (!surfaceColor.isValid()) {
        surfaceColor = sourcePalette.color(QPalette::Window);
    }
    QColor textColor = sourcePalette.color(QPalette::Text);
    if (!textColor.isValid()) {
        textColor = sourcePalette.color(QPalette::WindowText);
    }
    QColor linkColor = sourcePalette.color(QPalette::Link);
    if (!linkColor.isValid()) {
        linkColor = textColor;
    }
    QPalette browserPalette = sourcePalette;
    browserPalette.setColor(QPalette::Base, surfaceColor);
    browserPalette.setColor(QPalette::Window, surfaceColor);
    browserPalette.setColor(QPalette::AlternateBase, surfaceColor);
    browserPalette.setColor(QPalette::Text, textColor);
    browserPalette.setColor(QPalette::WindowText, textColor);
    browserPalette.setColor(QPalette::ButtonText, textColor);
    browserPalette.setColor(QPalette::Link, linkColor);
    browser->setPalette(browserPalette);
    browser->setAutoFillBackground(true);
    browser->setStyleSheet(QStringLiteral(
                                "QTextBrowser {"
                                " background-color: %1;"
                                " color: %2;"
                                " border: none;"
                                "}"
                                "QTextBrowser:viewport {"
                                " background-color: %1;"
                                "}").arg(surfaceColor.name(QColor::HexRgb),
                                          textColor.name(QColor::HexRgb)));

    if (QWidget *viewport = browser->viewport(); viewport != nullptr) {
        QPalette viewportPalette = sourcePalette;
        viewportPalette.setColor(QPalette::Base, surfaceColor);
        viewportPalette.setColor(QPalette::Window, surfaceColor);
        viewportPalette.setColor(QPalette::AlternateBase, surfaceColor);
        viewportPalette.setColor(QPalette::Text, textColor);
        viewportPalette.setColor(QPalette::WindowText, textColor);
        viewportPalette.setColor(QPalette::ButtonText, textColor);
        viewportPalette.setColor(QPalette::Link, linkColor);
        viewport->setPalette(viewportPalette);
        viewport->setAutoFillBackground(true);
        viewport->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                     .arg(surfaceColor.name(QColor::HexRgb),
                                          textColor.name(QColor::HexRgb)));
    }

    if (QTextDocument *document = browser->document(); document != nullptr) {
        document->setDefaultStyleSheet(
            QStringLiteral(
                "body {"
                " color: %1;"
                " background-color: %2;"
                " margin: 0;"
                " line-height: 1.35;"
                "}"
                "h1, h2, h3, h4 { margin: 0 0 8px 0; }"
                "p { margin: 0 0 10px 0; }"
                "ul, ol { margin: 0 0 10px 20px; }"
                "li { margin: 0 0 4px 0; }"
                "a { color: %3; }"
                "code { color: %1; }")
                .arg(textColor.name(QColor::HexRgb),
                     surfaceColor.name(QColor::HexRgb),
                     linkColor.name(QColor::HexRgb)));
    }
}

void syncPanelSurfaceToBaseTone(QWidget *panelWidget)
{
    syncPanelSurfaceToPalette(panelWidget, QApplication::palette(panelWidget));
}

void syncPanelSurfaceToPalette(QWidget *panelWidget, const QPalette &sourcePalette)
{
    if (panelWidget == nullptr) {
        return;
    }

    QPalette panelPalette = sourcePalette;
    QColor surfaceColor = sourcePalette.color(QPalette::Base);
    if (!surfaceColor.isValid()) {
        surfaceColor = sourcePalette.color(QPalette::Window);
    }

    panelPalette.setColor(QPalette::Window, surfaceColor);
    panelPalette.setColor(QPalette::Base, surfaceColor);
    panelPalette.setColor(QPalette::AlternateBase, surfaceColor);
    panelWidget->setPalette(panelPalette);
    panelWidget->setAutoFillBackground(true);
    if (panelWidget->property("preserveNativeChildControls").toBool()) {
        panelWidget->setStyleSheet(QString());
        panelWidget->update();
        return;
    }
    if (panelWidget->property("leftBorderOnly").toBool()) {
        if (panelWidget->objectName().isEmpty()) {
            panelWidget->setObjectName(QStringLiteral("leftBorderPanel"));
        }
        panelWidget->setStyleSheet(QStringLiteral(
                                       "QWidget#%1 {"
                                       " background-color: %2;"
                                       " color: %3;"
                                       " border-left: 1px solid palette(mid);"
                                       " border-right: none;"
                                       " border-top: none;"
                                       " border-bottom: none;"
                                       "}")
                                       .arg(panelWidget->objectName(),
                                            surfaceColor.name(QColor::HexRgb),
                                            panelPalette.color(QPalette::Text).name(QColor::HexRgb)));
        return;
    }

    panelWidget->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                   .arg(surfaceColor.name(QColor::HexRgb),
                                        panelPalette.color(QPalette::Text).name(QColor::HexRgb)));
}
}
