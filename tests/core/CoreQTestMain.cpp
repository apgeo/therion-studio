#include <QtTest/QtTest>

int runTherionSourceTextTest(int argc, char **argv);
int runTherionTokenRulesTest(int argc, char **argv);
int runThreeDViewerLoxLoaderTest(int argc, char **argv);
int runTherionFileTypesTest(int argc, char **argv);

int main(int argc, char **argv)
{
    int status = 0;
    status |= runTherionFileTypesTest(argc, argv);
    status |= runThreeDViewerLoxLoaderTest(argc, argv);
    status |= runTherionSourceTextTest(argc, argv);
    status |= runTherionTokenRulesTest(argc, argv);
    return status;
}
