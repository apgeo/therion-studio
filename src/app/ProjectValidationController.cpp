#include "ProjectValidationController.h"

namespace TherionStudio
{
ProjectValidationController::ProjectValidationController(QObject *parent)
    : QObject(parent)
    , scanner_(new ProjectValidationScanner(this))
{
    connect(scanner_,
            &ProjectValidationScanner::validationStarted,
            this,
            &ProjectValidationController::handleScannerStarted);
    connect(scanner_,
            &ProjectValidationScanner::validationFinished,
            this,
            &ProjectValidationController::handleScannerFinished);
}

void ProjectValidationController::requestValidation(const Request &request)
{
    pendingTrigger_ = request.trigger;
    scanner_->requestScan(request.projectRootPath,
                          request.validationCatalog,
                          request.inMemoryProjectContentsByPath);
}

void ProjectValidationController::setDebounceIntervalMs(int intervalMs)
{
    scanner_->setDebounceIntervalMs(intervalMs);
}

void ProjectValidationController::handleScannerStarted(quint64 generation, const QString &projectRootPath)
{
    triggersByGeneration_.insert(generation, pendingTrigger_);
    emit validationStarted(pendingTrigger_, generation, projectRootPath);
}

void ProjectValidationController::handleScannerFinished(const ProjectValidationScanner::Result &result)
{
    const Trigger trigger = triggersByGeneration_.take(result.generation);
    emit validationFinished(trigger, result);
}
}
