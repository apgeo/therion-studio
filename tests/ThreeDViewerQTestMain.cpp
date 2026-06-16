#include <QtTest/QtTest>

int runThreeDViewerProjectionTest(int argc, char **argv);
int runThreeDViewerViewportControllerTest(int argc, char **argv);

int main(int argc, char **argv)
{
    int status = 0;
    status |= runThreeDViewerProjectionTest(argc, argv);
    status |= runThreeDViewerViewportControllerTest(argc, argv);
    return status;
}
