#include "gui/MainWindow.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLabel>
#include <QLocale>

#include <algorithm>

#include "domain/DataManager.h"
#include "gui/PointCloudViewer.h"
#include "gui/MainWindowInternal.h"
#include "gui/support/UiHelpers.h"

using namespace mainwindow_internal;
using lasviewer::gui::showStyledOpenFileNamesDialog;

namespace
{
QString colorModeName(PointCloudColorMode colorMode)
{
    switch (colorMode) {
    case PointCloudColorMode::Elevation: return QCoreApplication::translate("MainWindow", "Elevation ramp");
    case PointCloudColorMode::SingleColor: return QCoreApplication::translate("MainWindow", "Single color");
    case PointCloudColorMode::Classification: return QCoreApplication::translate("MainWindow", "Classification");
    case PointCloudColorMode::Rgb:
    default: return QCoreApplication::translate("MainWindow", "RGB");
    }
}

QString datasetPathSummary(const QStringList& filePaths)
{
    QStringList lines;
    const int visibleCount = std::min(4, filePaths.size());
    for (int index = 0; index < visibleCount; ++index) {
        lines.append(filePaths.at(index));
    }
    if (filePaths.size() > visibleCount) {
        lines.append(QCoreApplication::translate("MainWindow", "... and %1 more")
            .arg(QLocale().toString(filePaths.size() - visibleCount)));
    }
    return lines.join(QLatin1Char('\n'));
}
}

void MainWindow::addPointCloudFiles()
{
    hideBackstageView();
    const QStringList filePaths = showStyledOpenFileNamesDialog(
        this,
        tr("Add Point Clouds or Gaussian Models"),
        QString(),
        tr("Supported Files (*.las *.laz *.ply);;LAS Files (*.las *.laz);;Gaussian PLY (*.ply);;All Files (*.*)"));

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
    pendingRecentDataFiles_ = filePaths;
    pendingDataLoadResetsProject_ = true;
    if (viewer_->loadPointCloudFilesAsync(filePaths, &errorMessage)) {
        if (viewer_->isPointCloudLoadingInProgress()) {
            showUserMessage(LogLevel::Info, errorMessage, 4500);
            return true;
        }
        pendingDataLoadResetsProject_ = false;
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

    pendingRecentDataFiles_.clear();
    pendingDataLoadResetsProject_ = false;
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
    pendingRecentDataFiles_ = filePaths;
    pendingDataLoadResetsProject_ = true;
    QString errorMessage;
    if (!viewer_->appendPointCloudFiles(filePaths, &errorMessage)) {
        pendingRecentDataFiles_.clear();
        pendingDataLoadResetsProject_ = false;
        syncUiFromViewer();
        showUserMessage(
            LogLevel::Error,
            errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
            6000);
        return false;
    }

    if (viewer_->isPointCloudLoadingInProgress()) {
        pendingRecentDataFiles_ = filePaths;
        showUserMessage(LogLevel::Info, errorMessage, 4500);
        return true;
    }

    pendingRecentDataFiles_.clear();
    pendingDataLoadResetsProject_ = false;
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
    if (viewer_ == nullptr || viewer_->isPointCloudLoadingInProgress()) {
        return;
    }
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

void MainWindow::openPointCloud()
{
    hideBackstageView();
    const QStringList filePaths = showStyledOpenFileNamesDialog(
        this,
        tr("Open Point Clouds or Gaussian Models"),
        QString(),
        tr("Supported Files (*.las *.laz *.ply);;LAS Files (*.las *.laz);;Gaussian PLY (*.ply);;All Files (*.*)"));

    if (filePaths.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Open cancelled."), 2000);
        return;
    }

    loadPointCloudFiles(filePaths);
}


void MainWindow::updateDatasetPanel()
{
    const PointCloudData* pointCloudData = viewer_->pointCloudData();
    if (pointCloudData == nullptr && viewer_->currentFilePaths().isEmpty()) {
        datasetNameValueLabel_->setText(tr("No dataset loaded"));
        datasetPathValueLabel_->setText(tr("Open, add, or drag LAS/LAZ files into the window."));
        datasetPointsValueLabel_->setText(QStringLiteral("0"));
        datasetBoundsValueLabel_->setText(tr("N/A"));
        datasetExtentValueLabel_->setText(tr("N/A"));
        datasetColorValueLabel_->setText(colorModeName(viewer_->visualizationOptions().colorMode));
        return;
    }

    if (pointCloudData == nullptr) {
        datasetNameValueLabel_->setText(tr("All datasets hidden"));
        datasetPathValueLabel_->setText(datasetPathSummary(viewer_->currentFilePaths()));
        datasetPointsValueLabel_->setText(QStringLiteral("0"));
        datasetBoundsValueLabel_->setText(tr("N/A"));
        datasetExtentValueLabel_->setText(tr("N/A"));
        datasetColorValueLabel_->setText(colorModeName(viewer_->visualizationOptions().colorMode));
        return;
    }

    const QStringList filePaths = viewer_->currentFilePaths();
    const PointRecord& minBounds = pointCloudData->minBounds();
    const PointRecord& maxBounds = pointCloudData->maxBounds();

    datasetNameValueLabel_->setText(
        filePaths.size() == 1
            ? QFileInfo(filePaths.constFirst()).fileName()
            : tr("%1 datasets").arg(QLocale().toString(filePaths.size())));
    datasetPathValueLabel_->setText(datasetPathSummary(filePaths));
    datasetPointsValueLabel_->setText(QLocale().toString(static_cast<qlonglong>(pointCloudData->size())));
    datasetBoundsValueLabel_->setText(
        tr("Min (%1)\nMax (%2)")
            .arg(formatTriplet(minBounds.x, minBounds.y, minBounds.z))
            .arg(formatTriplet(maxBounds.x, maxBounds.y, maxBounds.z)));
    datasetExtentValueLabel_->setText(formatTriplet(
        maxBounds.x - minBounds.x,
        maxBounds.y - minBounds.y,
        maxBounds.z - minBounds.z));
    datasetColorValueLabel_->setText(
        tr("%1 | Native RGB: %2")
            .arg(colorModeName(viewer_->visualizationOptions().colorMode))
            .arg(pointCloudData->hasColor() ? tr("yes") : tr("no")));
}
