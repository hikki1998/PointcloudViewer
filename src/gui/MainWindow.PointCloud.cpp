#include "gui/MainWindow.h"

#include <QFileInfo>

#include "domain/DataManager.h"
#include "gui/PointCloudViewer.h"
#include "gui/MainWindowInternal.h"
#include "gui/support/UiHelpers.h"

using namespace mainwindow_internal;
using lasviewer::gui::showStyledOpenFileNamesDialog;

void MainWindow::addPointCloudFiles()
{
    hideBackstageView();
    const QStringList filePaths = showStyledOpenFileNamesDialog(
        this,
        tr("Add LAS Point Clouds"),
        QString(),
        tr("LAS Files (*.las *.laz);;All Files (*.*)"));

    if (filePaths.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Add datasets cancelled."), 2000);
        return;
    }

    appendPointCloudFiles(filePaths);
}

bool MainWindow::loadPointCloudFile(const QString& filePath)
{
    return loadPointCloudFiles(QStringList { filePath });
}

bool MainWindow::loadPointCloudFiles(const QStringList& filePaths)
{
    if (viewer_ == nullptr) {
        return false;
    }

    QString errorMessage;
    if (viewer_->loadPointCloudFiles(filePaths, &errorMessage)) {
        currentProjectFilePath_.clear();
        linkedTowerFilePath_.clear();
        linkedRouteFilePath_.clear();
        setTowerEditingEnabled(false);
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        currentPowerlineRoute_ = PowerlineRouteDocument();
        selectedRouteWaypointIndex_ = -1;
        const QString successMessage = filePaths.size() == 1
            ? tr("Loaded %1. %2").arg(QFileInfo(filePaths.constFirst()).fileName(), errorMessage)
            : tr("Loaded %1 datasets. %2")
                  .arg(QLocale().toString(filePaths.size()))
                  .arg(errorMessage);
        showUserMessage(LogLevel::Info, successMessage, 4500);
        return true;
    }

    syncUiFromViewer();
    showUserMessage(
        LogLevel::Error,
        errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
        6000);
    return false;
}

bool MainWindow::appendPointCloudFiles(const QStringList& filePaths)
{
    if (viewer_ == nullptr) {
        return false;
    }
    QString errorMessage;
    if (!viewer_->appendPointCloudFiles(filePaths, &errorMessage)) {
        syncUiFromViewer();
        showUserMessage(
            LogLevel::Error,
            errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
            6000);
        return false;
    }

    currentProjectFilePath_.clear();
    linkedTowerFilePath_.clear();
    linkedRouteFilePath_.clear();
    setTowerEditingEnabled(false);
    vegetationRiskResults_.clear();
    selectedVegetationRiskIndex_ = -1;
    currentPowerlineRoute_ = PowerlineRouteDocument();
    selectedRouteWaypointIndex_ = -1;
    syncUiFromViewer();
    showUserMessage(
        LogLevel::Info,
        errorMessage.isEmpty() ? tr("Datasets added.") : errorMessage,
        4500);
    return true;
}

void MainWindow::clearPointCloud()
{
    currentProjectFilePath_.clear();
    linkedTowerFilePath_.clear();
    linkedRouteFilePath_.clear();
    classificationEditsDirty_ = false;
    setTowerEditingEnabled(false);
    vegetationRiskResults_.clear();
    selectedVegetationRiskIndex_ = -1;
    currentPowerlineRoute_ = PowerlineRouteDocument();
    selectedRouteWaypointIndex_ = -1;
    viewer_->clearPointCloud();
}

void MainWindow::removeSelectedDataset()
{
    if (viewer_ == nullptr || projectTreeWidget_ == nullptr) {
        return;
    }

    const QString datasetPath = selectedDatasetPath();
    if (datasetPath.isEmpty()) {
        showUserMessage(LogLevel::Warning, tr("Select a dataset in the project tree before removing it."), 3000);
        return;
    }
    QStringList remainingFilePaths = viewer_->currentFilePaths();
    remainingFilePaths.removeAll(datasetPath);

    if (remainingFilePaths.isEmpty()) {
        clearPointCloud();
        showUserMessage(LogLevel::Info, tr("Dataset removed. The project is now empty."), 3000);
        return;
    }

    const QList<TowerMarker> towerMarkers = viewer_->towerMarkers();
    const QList<InspectionIssue> inspectionIssues = viewer_->inspectionIssues();
    const int selectedTowerIndex = viewer_->selectedTowerIndex();
    const int selectedIssueIndex = viewer_->selectedIssueIndex();
    QHash<QString, bool> datasetVisibility;
    for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
        datasetVisibility.insert(datasetInfo.filePath.toLower(), datasetInfo.visible);
    }
    QString errorMessage;
    if (viewer_->loadPointCloudFiles(remainingFilePaths, &errorMessage)) {
        currentProjectFilePath_.clear();
        linkedRouteFilePath_.clear();
        setTowerEditingEnabled(false);
        for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
            const auto visibilityIt = datasetVisibility.constFind(datasetInfo.filePath.toLower());
            if (visibilityIt != datasetVisibility.constEnd() && !visibilityIt.value()) {
                viewer_->setPointCloudDatasetVisible(datasetInfo.filePath, false);
            }
        }
        viewer_->setTowerMarkers(towerMarkers);
        viewer_->setInspectionIssues(inspectionIssues);
        viewer_->setSelectedTowerIndex(selectedTowerIndex);
        viewer_->setSelectedIssueIndex(selectedIssueIndex);
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        currentPowerlineRoute_ = PowerlineRouteDocument();
        selectedRouteWaypointIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        showUserMessage(LogLevel::Info, tr("Dataset removed from the project."), 3000);
    } else {
        syncUiFromViewer();
        showUserMessage(
            LogLevel::Error,
            errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
            6000);
    }
}

void MainWindow::choosePointColor()
{
    const QColor initialColor = viewer_->visualizationOptions().singleColor;
    const QColor chosenColor = showStyledColorDialog(this, initialColor, tr("Choose Single Point Color"));
    if (chosenColor.isValid()) {
        viewer_->setSingleColor(chosenColor);
        if (viewer_->visualizationOptions().colorMode != PointCloudColorMode::SingleColor) {
            viewer_->setColorMode(PointCloudColorMode::SingleColor);
        }
    }
}

void MainWindow::chooseBackgroundColor()
{
    const QColor initialColor = viewer_->visualizationOptions().backgroundColor;
    const QColor chosenColor = showStyledColorDialog(this, initialColor, tr("Choose Background Color"));
    if (chosenColor.isValid()) {
        viewer_->setBackgroundColor(chosenColor);
    }
}
