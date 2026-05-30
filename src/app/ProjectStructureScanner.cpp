#include "ProjectStructureScanner.h"

#include <QDir>
#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent>

namespace TherionStudio
{
ProjectStructureScanner::ProjectStructureScanner(QObject *parent)
    : QObject(parent)
    , debounceTimer_(new QTimer(this))
    , scanWatcher_(new QFutureWatcher<Result>(this))
{
    debounceTimer_->setSingleShot(true);
    debounceTimer_->setInterval(180);
    connect(debounceTimer_, &QTimer::timeout, this, &ProjectStructureScanner::startScan);
    connect(scanWatcher_, &QFutureWatcher<Result>::finished, this, &ProjectStructureScanner::handleScanFinished);
}

void ProjectStructureScanner::requestScan(const QString &projectRootPath,
                                          const QHash<QString, QString> &inMemoryProjectContentsByPath)
{
    pendingRequest_.projectRootPath = projectRootPath;
    pendingRequest_.inMemoryProjectContentsByPath = inMemoryProjectContentsByPath;
    hasPendingRequest_ = true;
    debounceTimer_->start();
}

void ProjectStructureScanner::setDebounceIntervalMs(int intervalMs)
{
    debounceTimer_->setInterval(intervalMs);
}

void ProjectStructureScanner::startScan()
{
    if (!hasPendingRequest_) {
        return;
    }

    if (scanWatcher_->isRunning()) {
        queuedScan_ = true;
        return;
    }

    const Request request = pendingRequest_;
    hasPendingRequest_ = false;
    const quint64 generation = ++generation_;

    auto future = QtConcurrent::run([request, generation]() {
        Result result;
        result.generation = generation;
        result.projectRootPath = request.projectRootPath;
        result.projectIndex = ProjectStructureIndex::scanProjectIndex(request.projectRootPath,
                                                                      request.inMemoryProjectContentsByPath,
                                                                      &result.errorMessage);
        result.entries = result.projectIndex.entries;
        return result;
    });
    scanWatcher_->setFuture(future);
}

void ProjectStructureScanner::handleScanFinished()
{
    const Result result = scanWatcher_->result();
    emit scanFinished(result);

    if (queuedScan_) {
        queuedScan_ = false;
        startScan();
    }
}
}
