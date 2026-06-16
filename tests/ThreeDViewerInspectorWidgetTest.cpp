#include "../src/app/three_d_viewer/ThreeDViewerInspectorWidget.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

class ThreeDViewerInspectorWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsTheQmlInspectorSurface();
};

void ThreeDViewerInspectorWidgetTest::loadsTheQmlInspectorSurface()
{
    ThreeDViewerInspectorWidget widget;
    widget.resize(640, 480);
    widget.show();

    QTRY_COMPARE(widget.status(), QQuickWidget::Ready);
    QVERIFY(widget.rootObject() != nullptr);
}

int runThreeDViewerInspectorWidgetTest(int argc, char **argv)
{
    ThreeDViewerInspectorWidgetTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerInspectorWidgetTest.moc"
