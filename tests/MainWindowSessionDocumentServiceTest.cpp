#include "../src/app/MainWindowSessionDocumentService.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowSessionDocumentServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsRestoreEntries();
    void mergesOpenDocumentPaths();
    void buildsOpenDocumentsState();
    void detectsMapDocuments();
};

void MainWindowSessionDocumentServiceTest::buildsRestoreEntries()
{
    const QStringList openDocumentPaths = {
        QString(),
        QStringLiteral("/tmp/survey.th"),
        QStringLiteral("/tmp/map.TH2"),
        QStringLiteral("/tmp/log.txt")};

    const std::vector<MainWindowSessionDocumentService::RestoreEntry> entries =
        MainWindowSessionDocumentService::buildRestoreEntries(openDocumentPaths);

    QCOMPARE(entries.size(), size_t{3});
    QCOMPARE(entries.at(0).filePath, QStringLiteral("/tmp/survey.th"));
    QCOMPARE(entries.at(0).target, MainWindowSessionDocumentService::RestoreTarget::TextEditor);
    QCOMPARE(entries.at(1).filePath, QStringLiteral("/tmp/map.TH2"));
    QCOMPARE(entries.at(1).target, MainWindowSessionDocumentService::RestoreTarget::MapEditor);
    QCOMPARE(entries.at(2).filePath, QStringLiteral("/tmp/log.txt"));
    QCOMPARE(entries.at(2).target, MainWindowSessionDocumentService::RestoreTarget::TextEditor);
}

void MainWindowSessionDocumentServiceTest::mergesOpenDocumentPaths()
{
    const QStringList tabDocumentPaths = {
        QStringLiteral("/tmp/a.th"),
        QString(),
        QStringLiteral("/tmp/a.th"),
        QStringLiteral("/tmp/b.th")};
    const QStringList detachedMapDocumentPaths = {
        QStringLiteral("/tmp/b.th"),
        QString(),
        QStringLiteral("/tmp/c.th2"),
        QStringLiteral("/tmp/c.th2")};

    const QStringList merged = MainWindowSessionDocumentService::mergeOpenDocumentPaths(tabDocumentPaths,
                                                                                         detachedMapDocumentPaths);
    const QStringList expected = {
        QStringLiteral("/tmp/a.th"),
        QStringLiteral("/tmp/b.th"),
        QStringLiteral("/tmp/c.th2")};
    QCOMPARE(merged, expected);
}

void MainWindowSessionDocumentServiceTest::buildsOpenDocumentsState()
{
    const QStringList tabDocumentPaths = {
        QStringLiteral("/tmp/a.th"),
        QStringLiteral("/tmp/b.th")};
    const QStringList detachedMapDocumentPaths = {
        QStringLiteral("/tmp/b.th"),
        QStringLiteral("/tmp/c.th2")};

    const MainWindowSessionDocumentService::OpenDocumentsState detachedActiveState =
        MainWindowSessionDocumentService::buildOpenDocumentsState(tabDocumentPaths,
                                                                  detachedMapDocumentPaths,
                                                                  {QStringLiteral("/tmp/c.th2")},
                                                                  QStringLiteral("/tmp/b.th"));
    const QStringList expectedOpenPaths = {
        QStringLiteral("/tmp/a.th"),
        QStringLiteral("/tmp/b.th"),
        QStringLiteral("/tmp/c.th2")};
    QCOMPARE(detachedActiveState.openDocumentPaths, expectedOpenPaths);
    QCOMPARE(detachedActiveState.activeDocumentPath, QStringLiteral("/tmp/c.th2"));

    const MainWindowSessionDocumentService::OpenDocumentsState currentActiveState =
        MainWindowSessionDocumentService::buildOpenDocumentsState(tabDocumentPaths,
                                                                  detachedMapDocumentPaths,
                                                                  {},
                                                                  QStringLiteral("/tmp/b.th"));
    QCOMPARE(currentActiveState.activeDocumentPath, QStringLiteral("/tmp/b.th"));
}

void MainWindowSessionDocumentServiceTest::detectsMapDocuments()
{
    QVERIFY(MainWindowSessionDocumentService::isMapDocumentPath(QStringLiteral("/tmp/map.th2")));
    QVERIFY(MainWindowSessionDocumentService::isMapDocumentPath(QStringLiteral("/tmp/map.TH2")));
    QVERIFY(!MainWindowSessionDocumentService::isMapDocumentPath(QStringLiteral("/tmp/survey.th")));
}
}

int runMainWindowSessionDocumentServiceTest(int argc, char **argv)
{
    MainWindowSessionDocumentServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowSessionDocumentServiceTest.moc"
