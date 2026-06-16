#include <QtTest/QtTest>

int runTherionSourceTextTest(int argc, char **argv);
int runTherionTokenRulesTest(int argc, char **argv);

int main(int argc, char **argv)
{
    int status = 0;
    status |= runTherionSourceTextTest(argc, argv);
    status |= runTherionTokenRulesTest(argc, argv);
    return status;
}
