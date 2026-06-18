#include <QtTest/QtTest>

int runTherionSourceTextTest(int argc, char **argv);
int runTherionTokenRulesTest(int argc, char **argv);
int runThreeDViewerLoxLoaderTest(int argc, char **argv);
int runThreeDViewerCameraTest(int argc, char **argv);
int runThreeDViewerSceneModelTest(int argc, char **argv);
int runThreeDViewerSceneStatisticsTest(int argc, char **argv);
int runTherionFileTypesTest(int argc, char **argv);

int main(int argc, char **argv)
{
    int status = 0;
    status |= runTherionFileTypesTest(argc, argv);
    status |= runThreeDViewerCameraTest(argc, argv);
    status |= runThreeDViewerLoxLoaderTest(argc, argv);
    status |= runThreeDViewerSceneModelTest(argc, argv);
    status |= runThreeDViewerSceneStatisticsTest(argc, argv);
    status |= runTherionSourceTextTest(argc, argv);
    status |= runTherionTokenRulesTest(argc, argv);
    return status;
}
