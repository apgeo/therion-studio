#include "../src/app/three_d_viewer/ThreeDViewerViewportWidget.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

class ThreeDViewerViewportWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsTheQmlViewportSurface();
};

void ThreeDViewerViewportWidgetTest::loadsTheQmlViewportSurface()
{
    ThreeDViewerViewportWidget widget;
    widget.resize(640, 480);
    widget.show();

    QTRY_COMPARE(widget.status(), QQuickWidget::Ready);
    QVERIFY(widget.rootObject() != nullptr);
}

int runThreeDViewerViewportWidgetTest(int argc, char **argv)
{
    ThreeDViewerViewportWidgetTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerViewportWidgetTest.moc"
