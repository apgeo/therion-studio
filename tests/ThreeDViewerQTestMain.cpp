#include <QApplication>
#include <QtTest/QtTest>

int runThreeDViewerProjectionTest(int argc, char **argv);
int runThreeDViewerLayerListModelTest(int argc, char **argv);
int runThreeDViewerInspectorStateTest(int argc, char **argv);
int runThreeDViewerInspectorWidgetTest(int argc, char **argv);
int runThreeDViewerViewportControllerTest(int argc, char **argv);

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    int status = 0;
    status |= runThreeDViewerProjectionTest(argc, argv);
    status |= runThreeDViewerLayerListModelTest(argc, argv);
    status |= runThreeDViewerInspectorStateTest(argc, argv);
    status |= runThreeDViewerInspectorWidgetTest(argc, argv);
    status |= runThreeDViewerViewportControllerTest(argc, argv);
    return status;
}
