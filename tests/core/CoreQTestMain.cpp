#include <QtTest/QtTest>

int runTherionTokenRulesTest(int argc, char **argv);

int main(int argc, char **argv)
{
    int status = 0;
    status |= runTherionTokenRulesTest(argc, argv);
    return status;
}
