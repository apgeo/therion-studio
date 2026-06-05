#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>

class QTimer;

template <typename T>
class QFutureWatcher;

namespace TherionStudio
{
class ProjectSearchScanner final : public QObject
{
    Q_OBJECT

public:
    struct Match
    {
        QString filePath;
        int lineNumber = 0;
        int columnNumber = 0;
        QString lineText;
    };

    struct Result
    {
        quint64 generation = 0;
        QString projectRootPath;
        QString query;
        bool wholeWord = false;
        bool matchCase = false;
        QString errorMessage;
        QVector<Match> matches;
        int searchedFileCount = 0;
        bool limitReached = false;
    };

    explicit ProjectSearchScanner(QObject *parent = nullptr);

    void requestSearch(const QString &projectRootPath,
                       const QString &query,
                       bool wholeWord,
                       bool matchCase,
                       const QHash<QString, QString> &inMemoryProjectContentsByPath);
    void setDebounceIntervalMs(int intervalMs);

signals:
    void searchFinished(const TherionStudio::ProjectSearchScanner::Result &result);

private slots:
    void startSearch();
    void handleSearchFinished();

private:
    struct Request
    {
        QString projectRootPath;
        QString query;
        bool wholeWord = false;
        bool matchCase = false;
        QHash<QString, QString> inMemoryProjectContentsByPath;
    };

    Request pendingRequest_;
    bool hasPendingRequest_ = false;
    bool queuedSearch_ = false;
    quint64 generation_ = 0;
    QTimer *debounceTimer_ = nullptr;
    QFutureWatcher<Result> *searchWatcher_ = nullptr;
};
}
