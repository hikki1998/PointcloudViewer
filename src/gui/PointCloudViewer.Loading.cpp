#include "gui/PointCloudViewer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QLocale>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <utility>

#include "domain/DataManager.h"
#include "gaussian/GaussianPlyReader.h"
#include "gaussian/GaussianRenderer.h"
#include "pointcloud/LasReader.h"

namespace
{
QString formatPointCount(std::size_t pointCount)
{
    return QLocale().toString(static_cast<qlonglong>(pointCount));
}
}

bool PointCloudViewer::loadPointCloud(const QString& filePath, QString* errorMessage)
{
    return loadPointCloudFiles(QStringList { filePath }, errorMessage);
}

bool PointCloudViewer::loadPointCloudFilesAsync(const QStringList& filePaths, QString* errorMessage)
{
    if (filePaths.size() != 1
        || QFileInfo(filePaths.constFirst()).suffix().compare(QStringLiteral("ply"), Qt::CaseInsensitive) != 0) {
        return loadPointCloudFiles(filePaths, errorMessage);
    }
    if (pointCloudLoadingActive_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Another point cloud is still loading.");
        }
        return false;
    }
    if (gaussianLoadThread_.joinable()) {
        gaussianLoadThread_.join();
    }

    const QString absolutePath = QFileInfo(filePaths.constFirst()).absoluteFilePath();
    const QString loadTitle = tr("Loading %1").arg(QFileInfo(absolutePath).fileName());
    setLoadingState(true, loadTitle, tr("Reading Gaussian model in parallel..."), -1);
    emit pointCloudLoadingStarted(loadTitle);
    emit pointCloudLoadingProgress(tr("Reading Gaussian model in parallel..."), 0, 0);
    if (errorMessage != nullptr) {
        *errorMessage = tr("Loading Gaussian model in background...");
    }

    QPointer<PointCloudViewer> self(this);
    gaussianLoadThread_ = std::thread([self, absolutePath]() {
        auto model = std::make_shared<GaussianModel>();
        GaussianPlyReader reader;
        QString localError;
        const bool success = reader.read(absolutePath, model.get(), &localError);
        if (self == nullptr) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, absolutePath, model, localError, success]() {
            if (self == nullptr) {
                return;
            }
            if (!success) {
                self->setLoadingState(false, QString(), QString(), -1);
                emit self->pointCloudLoadingFinished();
                self->updateMessage(PointCloudViewer::tr("Open failed"), localError);
                emit self->pointCloudLoadingFailed(localError);
                return;
            }

            self->clearPointCloud();
            self->currentGaussianModel_ = model;
            self->currentFilePath_ = absolutePath;
            self->currentFilePaths_ = QStringList { absolutePath };
            self->visualizationOptions_.backgroundColor = QColor(38, 43, 51);
            self->applyClearColor();
            PointCloudDatasetInfo datasetInfo;
            datasetInfo.datasetId = self->nextDatasetId_++;
            datasetInfo.filePath = absolutePath;
            datasetInfo.pointCount = model->size();
            datasetInfo.minBounds.x = model->minBounds.x();
            datasetInfo.minBounds.y = model->minBounds.y();
            datasetInfo.minBounds.z = model->minBounds.z();
            datasetInfo.maxBounds.x = model->maxBounds.x();
            datasetInfo.maxBounds.y = model->maxBounds.y();
            datasetInfo.maxBounds.z = model->maxBounds.z();
            datasetInfo.hasColor = true;
            DataManager::instance().setPointCloudDatasets(QList<PointCloudDatasetInfo> { datasetInfo });

            self->setLoadingState(true, self->pointCloudLoadingTitle_, PointCloudViewer::tr("Uploading Gaussian model to GPU..."), -1);
            emit self->pointCloudLoadingProgress(PointCloudViewer::tr("Uploading Gaussian model to GPU..."), 0, 0);
            QString uploadError;
            if (self->osgWidget_ == nullptr || !self->osgWidget_->setGaussianModel(model, &uploadError)) {
                self->currentGaussianModel_.reset();
                self->currentFilePath_.clear();
                self->currentFilePaths_.clear();
                DataManager::instance().clear();
                self->setLoadingState(false, QString(), QString(), -1);
                emit self->pointCloudLoadingFinished();
                self->updateMessage(
                    PointCloudViewer::tr("Open failed"),
                    uploadError.isEmpty() ? PointCloudViewer::tr("Failed to initialize Gaussian rendering.") : uploadError);
                emit self->pointCloudLoadingFailed(
                    uploadError.isEmpty() ? PointCloudViewer::tr("Failed to initialize Gaussian rendering.") : uploadError);
                return;
            }

            self->applyViewPreset(PointCloudViewPreset::Isometric);
            self->updateFooter();
            self->updateWelcomeOverlayVisibility();
            self->setLoadingState(false, QString(), QString(), -1);
            emit self->pointCloudLoadingFinished();
            emit self->pointCloudLoaded();
        }, Qt::QueuedConnection);
    });
    return true;
}

void PointCloudViewer::showTransientPreviewPointCloud(const QString& filePath, const PointCloudData& pointCloudPreview, const QString& detailMessage)
{
    if (pointCloudPreview.empty()) {
        return;
    }

    currentPointCloud_ = std::make_shared<PointCloudData>(pointCloudPreview);
    currentFilePath_ = QFileInfo(filePath).absoluteFilePath();
    hoveredPointValid_ = false;
    lastHoverQueryPosition_ = QPointF();
    lastHoverQueryTime_ = {};
    if (currentPointCloud_ != nullptr && !currentPointCloud_->hasColor() && visualizationOptions_.colorMode == PointCloudColorMode::Rgb) {
        visualizationOptions_.colorMode = PointCloudColorMode::Elevation;
    }

    updateMessage(tr("Preview Ready"), detailMessage);
    rebuildScene();
    updateFooter();
    updateWelcomeOverlayVisibility();
}

bool PointCloudViewer::loadPointCloudFiles(const QStringList& filePaths, QString* errorMessage)
{
    if (pointCloudLoadingActive_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Another point cloud is still loading.");
        }
        return false;
    }
    if (filePaths.size() == 1 && QFileInfo(filePaths.constFirst()).suffix().compare(QStringLiteral("ply"), Qt::CaseInsensitive) == 0) {
        const QString absolutePath = QFileInfo(filePaths.constFirst()).absoluteFilePath();
        setLoadingState(true, tr("Loading %1").arg(QFileInfo(absolutePath).fileName()), tr("Reading Gaussian model..."), -1);
        emit pointCloudLoadingStarted(pointCloudLoadingTitle_);
        emit pointCloudLoadingProgress(tr("Reading Gaussian model..."), 0, 0);

        GaussianModel loadedModel;
        GaussianPlyReader reader;
        QString localError;
        if (!reader.read(absolutePath, &loadedModel, &localError)) {
            setLoadingState(false, QString(), QString(), -1);
            emit pointCloudLoadingFinished();
            updateMessage(tr("Open failed"), localError);
            if (errorMessage != nullptr) {
                *errorMessage = localError;
            }
            return false;
        }

        clearPointCloud();
        currentGaussianModel_ = std::make_shared<GaussianModel>(std::move(loadedModel));
        currentFilePath_ = absolutePath;
        currentFilePaths_ = QStringList { absolutePath };
        visualizationOptions_.backgroundColor = QColor(38, 43, 51);
        applyClearColor();
        PointCloudDatasetInfo datasetInfo;
        datasetInfo.datasetId = nextDatasetId_++;
        datasetInfo.filePath = absolutePath;
        datasetInfo.pointCount = currentGaussianModel_->size();
        datasetInfo.minBounds.x = currentGaussianModel_->minBounds.x();
        datasetInfo.minBounds.y = currentGaussianModel_->minBounds.y();
        datasetInfo.minBounds.z = currentGaussianModel_->minBounds.z();
        datasetInfo.maxBounds.x = currentGaussianModel_->maxBounds.x();
        datasetInfo.maxBounds.y = currentGaussianModel_->maxBounds.y();
        datasetInfo.maxBounds.z = currentGaussianModel_->maxBounds.z();
        datasetInfo.hasColor = true;
        DataManager::instance().setPointCloudDatasets(QList<PointCloudDatasetInfo> { datasetInfo });

        setLoadingState(true, pointCloudLoadingTitle_, tr("Uploading Gaussian model to GPU..."), -1);
        emit pointCloudLoadingProgress(tr("Uploading Gaussian model to GPU..."), 0, 0);
        if (osgWidget_ == nullptr || !osgWidget_->setGaussianModel(currentGaussianModel_, &localError)) {
            currentGaussianModel_.reset();
            currentFilePath_.clear();
            currentFilePaths_.clear();
            DataManager::instance().clear();
            setLoadingState(false, QString(), QString(), -1);
            emit pointCloudLoadingFinished();
            if (errorMessage != nullptr) {
                *errorMessage = localError.isEmpty() ? tr("Failed to initialize Gaussian rendering.") : localError;
            }
            return false;
        }

        applyViewPreset(PointCloudViewPreset::Isometric);
        updateFooter();
        updateWelcomeOverlayVisibility();
        setLoadingState(false, QString(), QString(), -1);
        emit pointCloudLoadingFinished();
        if (errorMessage != nullptr) {
            *errorMessage = tr("Loaded Gaussian model with %1 splats.").arg(formatPointCount(currentGaussianModel_->size()));
        }
        emit pointCloudLoaded();
        return true;
    }
    for (const QString& filePath : filePaths) {
        if (QFileInfo(filePath).suffix().compare(QStringLiteral("ply"), Qt::CaseInsensitive) == 0) {
            const QString localError = tr("Gaussian PLY files must be opened one at a time and cannot be mixed with LAS/LAZ datasets.");
            if (errorMessage != nullptr) {
                *errorMessage = localError;
            }
            return false;
        }
    }

    LasReader reader;
    QString localError;
    QStringList normalizedFilePaths;
    QList<LoadedPointCloudDataset> loadedDatasets;
    QList<PointCloudDatasetInfo> datasetInfos;
    nextDatasetId_ = 1;

    for (const QString& filePath : filePaths) {
        const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
        if (!absolutePath.isEmpty() && !normalizedFilePaths.contains(absolutePath, Qt::CaseInsensitive)) {
            normalizedFilePaths.append(absolutePath);
        }
    }

    if (normalizedFilePaths.isEmpty()) {
        localError = tr("No point cloud files were specified.");
        updateMessage(tr("Open failed"), localError);
        if (errorMessage != nullptr) {
            *errorMessage = localError;
        }
        return false;
    }

    const QString loadTitle = normalizedFilePaths.size() == 1
        ? tr("Loading %1").arg(QFileInfo(normalizedFilePaths.constFirst()).fileName())
        : tr("Loading %1 datasets").arg(QLocale().toString(normalizedFilePaths.size()));
    setLoadingState(true, loadTitle, tr("Preparing point cloud import..."), 0);
    emit pointCloudLoadingStarted(loadTitle);
    emit pointCloudLoadingProgress(tr("Preparing point cloud import..."), 0, 1000);

    for (int fileIndex = 0; fileIndex < normalizedFilePaths.size(); ++fileIndex) {
        const QString& filePath = normalizedFilePaths.at(fileIndex);
        PointCloudData datasetPointCloud;
        LasFileMetadata metadata;
        QElapsedTimer progressThrottle;
        progressThrottle.start();
        const auto progressCallback = [this, &normalizedFilePaths, fileIndex, &filePath, &progressThrottle](const LasReadProgress& progress) {
            const double fileFraction = progress.totalPoints > 0
                ? std::clamp(static_cast<double>(progress.pointsRead) / static_cast<double>(progress.totalPoints), 0.0, 1.0)
                : 0.0;
            const bool finishedFile = progress.totalPoints > 0 && progress.pointsRead >= progress.totalPoints;
            if (!finishedFile && progressThrottle.isValid() && progressThrottle.elapsed() < 40) {
                return;
            }

            progressThrottle.restart();
            const double overallFraction = normalizedFilePaths.isEmpty()
                ? 0.0
                : (static_cast<double>(fileIndex) + fileFraction) / static_cast<double>(normalizedFilePaths.size());
            const int overallValue = std::clamp(static_cast<int>(std::lround(overallFraction * 1000.0)), 0, 1000);
            const int percent = std::clamp(static_cast<int>(std::lround(overallFraction * 100.0)), 0, 100);
            const QString detail = progress.totalPoints > 0
                ? tr("Reading %1 (%2/%3 points, %4%)")
                      .arg(QFileInfo(filePath).fileName())
                      .arg(formatPointCount(progress.pointsRead))
                      .arg(formatPointCount(progress.totalPoints))
                      .arg(QLocale().toString(percent))
                : tr("Reading %1 (%2 points)")
                      .arg(QFileInfo(filePath).fileName())
                      .arg(formatPointCount(progress.pointsRead));
            setLoadingState(true, pointCloudLoadingTitle_, detail, percent);
            emit pointCloudLoadingProgress(detail, overallValue, 1000);
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        };
        if (!reader.read(filePath, &datasetPointCloud, &localError, &metadata, progressCallback)) {
            setLoadingState(false, QString(), QString(), -1);
            emit pointCloudLoadingFinished();
            updateMessage(tr("Open failed"), localError);
            if (errorMessage != nullptr) {
                *errorMessage = localError;
            }
            return false;
        }

        if (datasetPointCloud.empty()) {
            localError = tr("Point cloud file is empty: %1").arg(QFileInfo(filePath).fileName());
            updateMessage(tr("Open failed"), localError);
            if (errorMessage != nullptr) {
                *errorMessage = localError;
            }
            return false;
        }
        PointCloudDatasetInfo datasetInfo;
        datasetInfo.datasetId = nextDatasetId_++;
        datasetInfo.filePath = filePath;
        datasetInfo.pointCount = metadata.pointCount;
        datasetInfo.minBounds = metadata.minBounds;
        datasetInfo.maxBounds = metadata.maxBounds;
        datasetInfo.hasColor = metadata.hasColor;
        datasetInfo.hasIntensity = metadata.hasIntensity;
        datasetInfo.hasClassification = metadata.hasClassification;
        datasetInfo.hasReturnInfo = metadata.hasReturnInfo;
        datasetInfo.hasGpsTime = metadata.hasGpsTime;
        datasetInfo.visible = true;
        datasetInfo.projectionText = metadata.projectionText;
        datasetInfos.append(datasetInfo);

        LoadedPointCloudDataset loadedDataset;
        loadedDataset.info = datasetInfo;
        loadedDataset.pointCloud = std::make_shared<PointCloudData>(std::move(datasetPointCloud));
        std::vector<PointRecord>& sourcePoints = loadedDataset.pointCloud->mutablePoints();
        for (std::size_t pointIndex = 0; pointIndex < sourcePoints.size(); ++pointIndex) {
            sourcePoints[pointIndex].sourceDatasetId = datasetInfo.datasetId;
            sourcePoints[pointIndex].sourcePointIndex = static_cast<std::uint32_t>(pointIndex);
        }
        loadedDatasets.append(std::move(loadedDataset));
    }

    currentGaussianModel_.reset();
    if (osgWidget_ != nullptr) {
        osgWidget_->clearGaussianModel();
    }
    loadedPointCloudDatasets_ = std::move(loadedDatasets);
    DataManager::instance().setPointCloudDatasets(datasetInfos);
    currentFilePaths_ = normalizedFilePaths;
    syncCurrentFilePath();
    hoveredPointValid_ = false;
    lastHoverQueryPosition_ = QPointF();
    lastHoverQueryTime_ = {};
    towerMarkers_.clear();
    selectedTowerIndex_ = -1;
    towerEditMode_ = TowerEditMode::None;
    towerEditTargetIndex_ = -1;
    inspectionIssues_.clear();
    hiddenInspectionIssueIndices_.clear();
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    selectedIssueIndex_ = -1;
    issueEditMode_ = IssueEditMode::None;
    inspectionRouteWaypoints_.clear();
    inspectionRouteLabels_.clear();
    inspectionRoutePartPoints_.clear();
    inspectionRouteWaypointTargetPoints_.clear();
    inspectionRouteWaypointHasTargetPoints_.clear();
    inspectionRouteWaypointAircraftYawDegs_.clear();
    inspectionRouteWaypointGimbalPitchDegs_.clear();
    inspectionRouteWaypointCameraYawDegs_.clear();
    inspectionRouteWaypointCameraPitchDegs_.clear();
    inspectionRouteWaypointTargetLabels_.clear();
    inspectionRouteVisible_ = true;
    routeWaypointDragActive_ = false;
    routeWaypointDragIndex_ = -1;
    routeWaypointDragPreviewValid_ = false;
    DataManager::instance().clearTrajectory();
    selectedInspectionRouteWaypointIndex_ = -1;
    classificationEditStore_.clear();
    classificationUndoStack_.clear();
    classificationRedoStack_.clear();
    updateSceneClickCapture();
    syncVisualizationClassificationState();
    resetMeasurementState(false);

    if (currentPointCloud_ != nullptr && !currentPointCloud_->hasColor() && visualizationOptions_.colorMode == PointCloudColorMode::Rgb) {
        visualizationOptions_.colorMode = PointCloudColorMode::Elevation;
    }

    rebuildMergedPointCloud();
    rebuildScene();
    applyViewPreset(PointCloudViewPreset::Isometric);
    updateFooter();
    updateWelcomeOverlayVisibility();
    setLoadingState(false, QString(), QString(), -1);
    emit pointCloudLoadingFinished();

    if (errorMessage != nullptr) {
        *errorMessage = normalizedFilePaths.size() == 1
            ? tr("Loaded point cloud with %1 points.")
                  .arg(formatPointCount(currentPointCloud_ != nullptr ? currentPointCloud_->size() : 0))
            : tr("Loaded %1 datasets with %2 points.")
                  .arg(QLocale().toString(normalizedFilePaths.size()))
                  .arg(formatPointCount(currentPointCloud_ != nullptr ? currentPointCloud_->size() : 0));
    }

    emit pointCloudLoaded();
    emit visualizationOptionsChanged();
    emit measurementChanged();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerEditModeChanged();
    emit towerMarkersChanged();
    emit selectedIssueChanged(selectedIssueIndex_);
    emit issueEditModeChanged();
    emit inspectionIssuesChanged();
    return true;
}

bool PointCloudViewer::appendPointCloudFiles(const QStringList& filePaths, QString* errorMessage)
{
    if (pointCloudLoadingActive_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Another point cloud is still loading.");
        }
        return false;
    }
    if (filePaths.size() == 1 && QFileInfo(filePaths.constFirst()).suffix().compare(QStringLiteral("ply"), Qt::CaseInsensitive) == 0) {
        return loadPointCloudFilesAsync(filePaths, errorMessage);
    }
    for (const QString& filePath : filePaths) {
        if (QFileInfo(filePath).suffix().compare(QStringLiteral("ply"), Qt::CaseInsensitive) == 0) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("Gaussian PLY files must be opened one at a time and cannot be mixed with LAS/LAZ datasets.");
            }
            return false;
        }
    }
    if (!hasLoadedPointClouds()) {
        return loadPointCloudFiles(filePaths, errorMessage);
    }

    LasReader reader;
    QString localError;
    QStringList newFilePaths;
    QList<LoadedPointCloudDataset> newDatasets;
    QList<PointCloudDatasetInfo> newDatasetInfos;

    for (const QString& filePath : filePaths) {
        const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
        if (absolutePath.isEmpty()) {
            continue;
        }
        if (currentFilePaths_.contains(absolutePath, Qt::CaseInsensitive)
            || newFilePaths.contains(absolutePath, Qt::CaseInsensitive)) {
            continue;
        }
        newFilePaths.append(absolutePath);
    }

    if (newFilePaths.isEmpty()) {
        localError = tr("All selected datasets are already loaded.");
        if (errorMessage != nullptr) {
            *errorMessage = localError;
        }
        return true;
    }

    const QString loadTitle = newFilePaths.size() == 1
        ? tr("Adding %1").arg(QFileInfo(newFilePaths.constFirst()).fileName())
        : tr("Adding %1 datasets").arg(QLocale().toString(newFilePaths.size()));
    setLoadingState(true, loadTitle, tr("Preparing point cloud import..."), 0);
    emit pointCloudLoadingStarted(loadTitle);
    emit pointCloudLoadingProgress(tr("Preparing point cloud import..."), 0, 1000);

    for (int fileIndex = 0; fileIndex < newFilePaths.size(); ++fileIndex) {
        const QString& filePath = newFilePaths.at(fileIndex);
        PointCloudData datasetPointCloud;
        LasFileMetadata metadata;
        QElapsedTimer progressThrottle;
        progressThrottle.start();
        const auto progressCallback = [this, &newFilePaths, fileIndex, &filePath, &progressThrottle](const LasReadProgress& progress) {
            const double fileFraction = progress.totalPoints > 0
                ? std::clamp(static_cast<double>(progress.pointsRead) / static_cast<double>(progress.totalPoints), 0.0, 1.0)
                : 0.0;
            const bool finishedFile = progress.totalPoints > 0 && progress.pointsRead >= progress.totalPoints;
            if (!finishedFile && progressThrottle.isValid() && progressThrottle.elapsed() < 40) {
                return;
            }

            progressThrottle.restart();
            const double overallFraction = newFilePaths.isEmpty()
                ? 0.0
                : (static_cast<double>(fileIndex) + fileFraction) / static_cast<double>(newFilePaths.size());
            const int overallValue = std::clamp(static_cast<int>(std::lround(overallFraction * 1000.0)), 0, 1000);
            const int percent = std::clamp(static_cast<int>(std::lround(overallFraction * 100.0)), 0, 100);
            const QString detail = progress.totalPoints > 0
                ? tr("Reading %1 (%2/%3 points, %4%)")
                      .arg(QFileInfo(filePath).fileName())
                      .arg(formatPointCount(progress.pointsRead))
                      .arg(formatPointCount(progress.totalPoints))
                      .arg(QLocale().toString(percent))
                : tr("Reading %1 (%2 points)")
                      .arg(QFileInfo(filePath).fileName())
                      .arg(formatPointCount(progress.pointsRead));
            setLoadingState(true, pointCloudLoadingTitle_, detail, percent);
            emit pointCloudLoadingProgress(detail, overallValue, 1000);
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        };
        if (!reader.read(filePath, &datasetPointCloud, &localError, &metadata, progressCallback)) {
            setLoadingState(false, QString(), QString(), -1);
            emit pointCloudLoadingFinished();
            updateMessage(tr("Add failed"), localError);
            if (errorMessage != nullptr) {
                *errorMessage = localError;
            }
            return false;
        }

        if (datasetPointCloud.empty()) {
            localError = tr("Point cloud file is empty: %1").arg(QFileInfo(filePath).fileName());
            updateMessage(tr("Add failed"), localError);
            if (errorMessage != nullptr) {
                *errorMessage = localError;
            }
            return false;
        }

        PointCloudDatasetInfo datasetInfo;
        datasetInfo.datasetId = nextDatasetId_++;
        datasetInfo.filePath = filePath;
        datasetInfo.pointCount = metadata.pointCount;
        datasetInfo.minBounds = metadata.minBounds;
        datasetInfo.maxBounds = metadata.maxBounds;
        datasetInfo.hasColor = metadata.hasColor;
        datasetInfo.hasIntensity = metadata.hasIntensity;
        datasetInfo.hasClassification = metadata.hasClassification;
        datasetInfo.hasReturnInfo = metadata.hasReturnInfo;
        datasetInfo.hasGpsTime = metadata.hasGpsTime;
        datasetInfo.visible = true;
        datasetInfo.projectionText = metadata.projectionText;
        newDatasetInfos.append(datasetInfo);

        LoadedPointCloudDataset loadedDataset;
        loadedDataset.info = datasetInfo;
        loadedDataset.pointCloud = std::make_shared<PointCloudData>(std::move(datasetPointCloud));
        std::vector<PointRecord>& sourcePoints = loadedDataset.pointCloud->mutablePoints();
        for (std::size_t pointIndex = 0; pointIndex < sourcePoints.size(); ++pointIndex) {
            sourcePoints[pointIndex].sourceDatasetId = datasetInfo.datasetId;
            sourcePoints[pointIndex].sourcePointIndex = static_cast<std::uint32_t>(pointIndex);
        }
        newDatasets.append(std::move(loadedDataset));
    }

    for (LoadedPointCloudDataset& dataset : newDatasets) {
        loadedPointCloudDatasets_.append(std::move(dataset));
    }
    QList<PointCloudDatasetInfo> combinedDatasets = DataManager::instance().pointCloudDatasets();
    combinedDatasets.append(newDatasetInfos);
    DataManager::instance().setPointCloudDatasets(combinedDatasets);
    currentFilePaths_.append(newFilePaths);
    syncCurrentFilePath();
    hoveredPointValid_ = false;
    lastHoverQueryPosition_ = QPointF();
    lastHoverQueryTime_ = {};

    syncVisualizationClassificationState();
    rebuildMergedPointCloud();
    rebuildScene();
    updateFooter();
    updateWelcomeOverlayVisibility();
    setLoadingState(false, QString(), QString(), -1);
    emit pointCloudLoadingFinished();

    if (errorMessage != nullptr) {
        *errorMessage = newFilePaths.size() == 1
            ? tr("Added %1. Total datasets: %2, total points: %3.")
                  .arg(QFileInfo(newFilePaths.constFirst()).fileName())
                  .arg(QLocale().toString(currentFilePaths_.size()))
                  .arg(formatPointCount(currentPointCloud_ != nullptr ? currentPointCloud_->size() : 0))
            : tr("Added %1 datasets. Total datasets: %2, total points: %3.")
                  .arg(QLocale().toString(newFilePaths.size()))
                  .arg(QLocale().toString(currentFilePaths_.size()))
                  .arg(formatPointCount(currentPointCloud_ != nullptr ? currentPointCloud_->size() : 0));
    }

    emit pointCloudLoaded();
    emit visualizationOptionsChanged();
    return true;
}

void PointCloudViewer::clearPointCloud()
{
    routeRoamStopInternal(true);
    currentGaussianModel_.reset();
    if (osgWidget_ != nullptr) {
        osgWidget_->clearGaussianModel();
    }
    currentPointCloud_.reset();
    currentFilePath_.clear();
    currentFilePaths_.clear();
    loadedPointCloudDatasets_.clear();
    DataManager::instance().clear();
    hoveredPointValid_ = false;
    lastHoverQueryPosition_ = QPointF();
    lastHoverQueryTime_ = {};
    towerMarkers_.clear();
    selectedTowerIndex_ = -1;
    towerEditMode_ = TowerEditMode::None;
    towerEditTargetIndex_ = -1;
    inspectionIssues_.clear();
    hiddenInspectionIssueIndices_.clear();
    selectedIssueIndex_ = -1;
    issueEditMode_ = IssueEditMode::None;
    inspectionRouteWaypoints_.clear();
    inspectionRouteLabels_.clear();
    inspectionRoutePartPoints_.clear();
    inspectionRouteWaypointTargetPoints_.clear();
    inspectionRouteWaypointHasTargetPoints_.clear();
    inspectionRouteWaypointAircraftYawDegs_.clear();
    inspectionRouteWaypointGimbalPitchDegs_.clear();
    inspectionRouteWaypointCameraYawDegs_.clear();
    inspectionRouteWaypointCameraPitchDegs_.clear();
    inspectionRouteWaypointTargetLabels_.clear();
    inspectionRouteVisible_ = true;
    routeWaypointDragActive_ = false;
    routeWaypointDragIndex_ = -1;
    routeWaypointDragPreviewValid_ = false;
    selectedInspectionRouteWaypointIndex_ = -1;
    profileClassificationModeEnabled_ = false;
    profileClassificationSelectionMode_ = ProfileClassificationSelectionMode::Rectangle;
    profileClassificationTaskActive_ = false;
    profileClassificationSelectionActive_ = false;
    profileClassificationSelectionRect_ = QRectF();
    clipActiveDatasetPath_.clear();
    clipActiveDatasetId_ = -1;
    clipScope_ = ClipRegion::VisibleDatasets;
    clipBoxAlignment_ = ClipRegion::WorldAligned;
    clipKeepInside_ = true;
    resetClipEditingState();
    visualizationOptions_.clipRegion = ClipRegion();
    classificationTaskStartTime_ = {};
    classificationTaskScannedPoints_.store(0);
    lastClassificationTaskScannedPoints_ = 0;
    lastClassificationTaskElapsedMilliseconds_ = 0;
    if (classificationTaskStatusTimer_ != nullptr) {
        classificationTaskStatusTimer_->stop();
    }
    classificationEditStore_.clear();
    classificationUndoStack_.clear();
    classificationRedoStack_.clear();
    nextDatasetId_ = 1;
    clearSelectionRubberBand();
    clearProfileClassificationPolygonSelection();
    updateClipPolygonOverlay();
    updateSceneClickCapture();
    syncVisualizationClassificationState();
    resetMeasurementState(false);

    if (rootGroup_.valid()) {
        rootGroup_->removeChildren(0, rootGroup_->getNumChildren());
    }

    pointCloudNode_ = nullptr;
    towerMarkersNode_ = nullptr;
    inspectionIssuesNode_ = nullptr;
    inspectionRouteNode_ = nullptr;
    measurementOverlayNode_ = nullptr;
    clipOverlayNode_ = nullptr;
    updateTowerOverlayWidgets();
    updateInspectionIssueOverlayWidgets();
    updateInspectionRouteOverlayWidgets();
    updateMessage(
        tr("Scene cleared"),
        tr("Open one or more LAS or LAZ files to continue."));
    updateWelcomeOverlayVisibility();

    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }

    emit pointCloudCleared();
    emit measurementChanged();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerEditModeChanged();
    emit towerMarkersChanged();
    emit selectedIssueChanged(selectedIssueIndex_);
    emit issueEditModeChanged();
    emit inspectionIssuesChanged();
    emit selectedInspectionRouteWaypointChanged(selectedInspectionRouteWaypointIndex_);
    emit inspectionRouteChanged();
}
