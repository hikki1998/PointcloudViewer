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
#include "osg/OsgPointCloudNode.h"
#include "pointcloud/LasReader.h"

namespace
{
constexpr std::size_t kProgressivePreviewThresholdPoints = 500000;
constexpr std::size_t kProgressivePreviewTargetPoints = 160000;
constexpr std::size_t kProgressivePreviewMaxScanPoints = 320000;

QString formatPointCount(std::size_t pointCount)
{
    return QLocale().toString(static_cast<qlonglong>(pointCount));
}

std::size_t progressivePreviewThresholdPoints()
{
    bool ok = false;
    const int overrideValue = qEnvironmentVariableIntValue("LAS_VIEWER_PREVIEW_THRESHOLD_POINTS", &ok);
    return ok && overrideValue > 0
        ? static_cast<std::size_t>(overrideValue)
        : kProgressivePreviewThresholdPoints;
}
}

bool PointCloudViewer::loadPointCloud(const QString& filePath, QString* errorMessage)
{
    return loadPointCloudFiles(QStringList { filePath }, errorMessage);
}

bool PointCloudViewer::loadPointCloudFilesAsync(const QStringList& filePaths, QString* errorMessage)
{
    bool allLasFiles = !filePaths.isEmpty();
    for (const QString& filePath : filePaths) {
        const QString suffix = QFileInfo(filePath).suffix();
        if (suffix.compare(QStringLiteral("las"), Qt::CaseInsensitive) != 0
            && suffix.compare(QStringLiteral("laz"), Qt::CaseInsensitive) != 0) {
            allLasFiles = false;
            break;
        }
    }
    if (allLasFiles) {
        if (pointCloudLoadingActive_) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("Another point cloud is still loading.");
            }
            return false;
        }
        if (filePaths.size() == 1) {
            startAsyncSingleFileLoad(filePaths.constFirst());
        } else {
            startAsyncPointCloudBatchLoad(filePaths, false);
        }
        if (errorMessage != nullptr) {
            *errorMessage = tr("Loading point cloud in background...");
        }
        return true;
    }
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

void PointCloudViewer::startAsyncSingleFileLoad(const QString& filePath)
{
    cancelAsyncPointCloudLoad();
    const std::uint64_t token = ++asyncLoadToken_;
    asyncLoadCancellationRequested_.store(false);
    const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
    const QString loadTitle = tr("Loading %1").arg(QFileInfo(absolutePath).fileName());
    setLoadingState(true, loadTitle, tr("Reading point cloud in background..."), 0);
    pointCloudLoadingCancellable_ = true;
    emit pointCloudLoadingStarted(loadTitle);
    emit pointCloudLoadingProgress(tr("Reading point cloud in background..."), 0, 1000);

    const std::size_t previewThreshold = progressivePreviewThresholdPoints();
    const PointCloudVisualizationOptions preparedVisualizationOptions = visualizationOptions_;
    QPointer<PointCloudViewer> self(this);
    asyncLoadThread_ = std::thread([self, absolutePath, token, previewThreshold, preparedVisualizationOptions]() {
        auto pointCloud = std::make_shared<PointCloudData>();
        LasFileMetadata metadata;
        LasReader reader;
        QString localError;
        const auto cancellationCallback = [self]() {
            return self == nullptr || self->asyncLoadCancellationRequested_.load();
        };

        LasFileMetadata headerMetadata;
        const bool metadataLoaded = reader.readMetadata(absolutePath, &headerMetadata, nullptr);
        const bool largeEnoughForPreview =
            (metadataLoaded && headerMetadata.pointCount >= previewThreshold)
            || QFileInfo(absolutePath).size() >= static_cast<qint64>(previewThreshold * 24u);
        if (largeEnoughForPreview) {
            auto preview = std::make_shared<PointCloudData>();
            QString previewError;
            if (reader.readPreview(
                    absolutePath,
                    preview.get(),
                    kProgressivePreviewTargetPoints,
                    kProgressivePreviewMaxScanPoints,
                    &previewError,
                    cancellationCallback)
                && self != nullptr
                && !self->asyncLoadCancellationRequested_.load()) {
                preview->finalizeImport(
                    headerMetadata.minBounds,
                    headerMetadata.maxBounds,
                    headerMetadata.hasColor,
                    headerMetadata.hasIntensity,
                    headerMetadata.hasClassification,
                    headerMetadata.hasReturnInfo,
                    headerMetadata.hasGpsTime);
                QMetaObject::invokeMethod(self, [self, absolutePath, token, preview, headerMetadata]() {
                    if (self == nullptr || token != self->asyncLoadToken_) {
                        return;
                    }
                    self->clearPointCloud();
                    self->previewPointCloud_ = preview;
                    self->currentPointCloud_ = preview;
                    self->currentFilePath_ = absolutePath;
                    self->currentFilePaths_ = QStringList { absolutePath };
                    self->tiledPointCloudModeActive_ = true;
                    self->setLoadingState(
                        true,
                        self->pointCloudLoadingTitle_,
                        PointCloudViewer::tr("Preview ready with %1 points. Loading full resolution...")
                            .arg(formatPointCount(preview->size())),
                        0);
                    self->rebuildScene();
                    self->applyViewPreset(PointCloudViewPreset::Isometric);
                    self->updateFooter();
                    self->updateWelcomeOverlayVisibility();
                    emit self->pointCloudPreviewReady();
                }, Qt::QueuedConnection);
            }
        }

        const auto progressCallback = [self, absolutePath, token](const LasReadProgress& progress) {
            if (self == nullptr) {
                return;
            }
            const int value = progress.totalPoints > 0
                ? std::clamp(static_cast<int>(std::lround(
                      static_cast<double>(progress.pointsRead) * 1000.0 / static_cast<double>(progress.totalPoints))), 0, 1000)
                : 0;
            const QString detail = progress.totalPoints > 0
                ? PointCloudViewer::tr("Reading %1 (%2/%3 points)")
                      .arg(QFileInfo(absolutePath).fileName())
                      .arg(formatPointCount(progress.pointsRead))
                      .arg(formatPointCount(progress.totalPoints))
                : PointCloudViewer::tr("Reading %1 (%2 points)")
                      .arg(QFileInfo(absolutePath).fileName())
                      .arg(formatPointCount(progress.pointsRead));
            QMetaObject::invokeMethod(self, [self, token, detail, value]() {
                if (self != nullptr && token == self->asyncLoadToken_) {
                    self->setLoadingState(true, self->pointCloudLoadingTitle_, detail, value / 10);
                    emit self->pointCloudLoadingProgress(detail, value, 1000);
                }
            }, Qt::QueuedConnection);
        };
        const bool success = reader.read(
            absolutePath,
            pointCloud.get(),
            &localError,
            &metadata,
            progressCallback,
            cancellationCallback,
            1);
        if (self == nullptr) {
            return;
        }
        LoadedPointCloudDataset preparedDataset;
        PointCloudDatasetInfo preparedDatasetInfo;
        if (success) {
            preparedDatasetInfo.datasetId = 1;
            preparedDatasetInfo.filePath = absolutePath;
            preparedDatasetInfo.pointCount = metadata.pointCount;
            preparedDatasetInfo.minBounds = metadata.minBounds;
            preparedDatasetInfo.maxBounds = metadata.maxBounds;
            preparedDatasetInfo.hasColor = metadata.hasColor;
            preparedDatasetInfo.hasIntensity = metadata.hasIntensity;
            preparedDatasetInfo.hasClassification = metadata.hasClassification;
            preparedDatasetInfo.hasReturnInfo = metadata.hasReturnInfo;
            preparedDatasetInfo.hasGpsTime = metadata.hasGpsTime;
            preparedDatasetInfo.visible = true;
            preparedDatasetInfo.projectionText = metadata.projectionText;
            preparedDataset.info = preparedDatasetInfo;
            preparedDataset.pointCloud = pointCloud;
            buildDatasetSpatialIndex(&preparedDataset);
            buildDatasetInteractionPreview(&preparedDataset);
            PointCloudVisualizationOptions preparedOptions = preparedVisualizationOptions;
            preparedOptions.showAxes = false;
            preparedOptions.showBoundingBox = false;
            preparedOptions.sharedElevationRangeValid = true;
            preparedOptions.sharedElevationMin = metadata.minBounds.z;
            preparedOptions.sharedElevationMax = metadata.maxBounds.z;
            if (!pointCloud->hasColor() && preparedOptions.colorMode == PointCloudColorMode::Rgb) {
                preparedOptions.colorMode = PointCloudColorMode::Elevation;
            }
            preparedDataset.fullSceneNode = OsgPointCloudNode::build(*pointCloud, preparedOptions);
            preparedDataset.previewSceneNode = preparedDataset.interactionPreview != nullptr
                ? OsgPointCloudNode::build(*preparedDataset.interactionPreview, preparedOptions)
                : nullptr;
        }
        QMetaObject::invokeMethod(self, [self, absolutePath, token, pointCloud, metadata, localError, success,
                                           preparedDataset = std::move(preparedDataset), preparedDatasetInfo]() mutable {
            if (self == nullptr || token != self->asyncLoadToken_) {
                return;
            }
            if (!success) {
                self->setLoadingState(false, QString(), QString(), -1);
                emit self->pointCloudLoadingFinished();
                if (self->asyncLoadCancellationRequested_.load()) {
                    self->updateMessage(PointCloudViewer::tr("Open cancelled"), PointCloudViewer::tr("The previous scene was kept unchanged."));
                    emit self->pointCloudLoadingCancelled();
                } else {
                    self->updateMessage(PointCloudViewer::tr("Open failed"), localError);
                    emit self->pointCloudLoadingFailed(localError);
                }
                return;
            }

            self->clearPointCloud();
            self->previewPointCloud_.reset();
            self->tiledPointCloudModeActive_ = false;
            self->loadedPointCloudDatasets_.append(std::move(preparedDataset));
            self->invalidateMergedPointCloudCache();
            self->currentPointCloud_ = pointCloud;
            self->currentFilePath_ = absolutePath;
            self->currentFilePaths_ = QStringList { absolutePath };
            self->nextDatasetId_ = 2;
            DataManager::instance().setPointCloudDatasets(QList<PointCloudDatasetInfo> { preparedDatasetInfo });
            self->syncVisualizationClassificationState();
            if (!pointCloud->hasColor() && self->visualizationOptions_.colorMode == PointCloudColorMode::Rgb) {
                self->visualizationOptions_.colorMode = PointCloudColorMode::Elevation;
            }
            self->rebuildScene(true);
            self->applyViewPreset(PointCloudViewPreset::Isometric);
            self->updateFooter();
            self->updateWelcomeOverlayVisibility();
            self->setLoadingState(false, QString(), QString(), -1);
            emit self->pointCloudLoadingFinished();
            emit self->pointCloudLoaded();
            emit self->visualizationOptionsChanged();
            emit self->measurementChanged();
        }, Qt::QueuedConnection);
    });
}

void PointCloudViewer::startAsyncPointCloudBatchLoad(const QStringList& filePaths, bool append)
{
    cancelAsyncPointCloudLoad();

    QStringList normalizedFilePaths;
    for (const QString& filePath : filePaths) {
        const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
        if (absolutePath.isEmpty()
            || normalizedFilePaths.contains(absolutePath, Qt::CaseInsensitive)
            || (append && currentFilePaths_.contains(absolutePath, Qt::CaseInsensitive))) {
            continue;
        }
        normalizedFilePaths.append(absolutePath);
    }
    if (normalizedFilePaths.isEmpty()) {
        return;
    }

    const std::uint64_t token = ++asyncLoadToken_;
    asyncLoadCancellationRequested_.store(false);
    const QString loadTitle = append
        ? tr("Adding %1 datasets").arg(QLocale().toString(normalizedFilePaths.size()))
        : tr("Loading %1 datasets").arg(QLocale().toString(normalizedFilePaths.size()));
    setLoadingState(true, loadTitle, tr("Reading point clouds in background..."), 0);
    pointCloudLoadingCancellable_ = true;
    emit pointCloudLoadingStarted(loadTitle);
    emit pointCloudLoadingProgress(tr("Reading point clouds in background..."), 0, 1000);

    const int firstDatasetId = append ? nextDatasetId_ : 1;
    const PointCloudVisualizationOptions preparedVisualizationOptions = visualizationOptions_;
    QPointer<PointCloudViewer> self(this);
    asyncLoadThread_ = std::thread([self, normalizedFilePaths, token, append, firstDatasetId, preparedVisualizationOptions]() {
        QList<LoadedPointCloudDataset> datasets;
        QList<PointCloudDatasetInfo> datasetInfos;
        QString localError;
        for (int fileIndex = 0; fileIndex < normalizedFilePaths.size(); ++fileIndex) {
            if (self == nullptr || self->asyncLoadCancellationRequested_.load()) {
                break;
            }
            const QString filePath = normalizedFilePaths.at(fileIndex);
            auto pointCloud = std::make_shared<PointCloudData>();
            LasFileMetadata metadata;
            LasReader reader;
            const auto cancellationCallback = [self]() {
                return self == nullptr || self->asyncLoadCancellationRequested_.load();
            };
            const auto progressCallback = [self, filePath, token, fileIndex, fileCount = normalizedFilePaths.size()](const LasReadProgress& progress) {
                if (self == nullptr) {
                    return;
                }
                const double fileFraction = progress.totalPoints > 0
                    ? std::clamp(static_cast<double>(progress.pointsRead) / static_cast<double>(progress.totalPoints), 0.0, 1.0)
                    : 0.0;
                const int overallValue = std::clamp(static_cast<int>(std::lround(
                    (static_cast<double>(fileIndex) + fileFraction) * 1000.0 / static_cast<double>(fileCount))), 0, 1000);
                const QString detail = PointCloudViewer::tr("Reading %1 (%2/%3 points)")
                    .arg(QFileInfo(filePath).fileName())
                    .arg(formatPointCount(progress.pointsRead))
                    .arg(formatPointCount(progress.totalPoints));
                QMetaObject::invokeMethod(self, [self, token, detail, overallValue]() {
                    if (self != nullptr && token == self->asyncLoadToken_) {
                        self->setLoadingState(true, self->pointCloudLoadingTitle_, detail, overallValue / 10);
                        emit self->pointCloudLoadingProgress(detail, overallValue, 1000);
                    }
                }, Qt::QueuedConnection);
            };
            const int datasetId = firstDatasetId + fileIndex;
            if (!reader.read(
                    filePath,
                    pointCloud.get(),
                    &localError,
                    &metadata,
                    progressCallback,
                    cancellationCallback,
                    datasetId)) {
                break;
            }

            PointCloudDatasetInfo info;
            info.datasetId = datasetId;
            info.filePath = filePath;
            info.pointCount = metadata.pointCount;
            info.minBounds = metadata.minBounds;
            info.maxBounds = metadata.maxBounds;
            info.hasColor = metadata.hasColor;
            info.hasIntensity = metadata.hasIntensity;
            info.hasClassification = metadata.hasClassification;
            info.hasReturnInfo = metadata.hasReturnInfo;
            info.hasGpsTime = metadata.hasGpsTime;
            info.visible = true;
            info.projectionText = metadata.projectionText;

            LoadedPointCloudDataset dataset;
            dataset.info = info;
            dataset.pointCloud = std::move(pointCloud);
            buildDatasetSpatialIndex(&dataset);
            buildDatasetInteractionPreview(&dataset);
            datasets.append(std::move(dataset));
            datasetInfos.append(info);
        }

        if (self == nullptr) {
            return;
        }
        if (datasets.size() == normalizedFilePaths.size() && !datasets.isEmpty()) {
            double sharedMinZ = datasets.constFirst().info.minBounds.z;
            double sharedMaxZ = datasets.constFirst().info.maxBounds.z;
            for (const LoadedPointCloudDataset& dataset : datasets) {
                sharedMinZ = std::min(sharedMinZ, dataset.info.minBounds.z);
                sharedMaxZ = std::max(sharedMaxZ, dataset.info.maxBounds.z);
            }
            PointCloudVisualizationOptions preparedOptions = preparedVisualizationOptions;
            preparedOptions.showAxes = false;
            preparedOptions.showBoundingBox = false;
            preparedOptions.sharedElevationRangeValid = true;
            preparedOptions.sharedElevationMin = sharedMinZ;
            preparedOptions.sharedElevationMax = sharedMaxZ;
            for (LoadedPointCloudDataset& dataset : datasets) {
                dataset.fullSceneNode = OsgPointCloudNode::build(*dataset.pointCloud, preparedOptions);
                dataset.previewSceneNode = dataset.interactionPreview != nullptr
                    ? OsgPointCloudNode::build(*dataset.interactionPreview, preparedOptions)
                    : nullptr;
            }
        }
        QMetaObject::invokeMethod(self, [self, normalizedFilePaths, token, append, datasets = std::move(datasets), datasetInfos, localError]() mutable {
            if (self == nullptr || token != self->asyncLoadToken_) {
                return;
            }
            if (self->asyncLoadCancellationRequested_.load() || datasets.size() != normalizedFilePaths.size()) {
                self->setLoadingState(false, QString(), QString(), -1);
                emit self->pointCloudLoadingFinished();
                if (self->asyncLoadCancellationRequested_.load()) {
                    self->updateMessage(PointCloudViewer::tr("Open cancelled"), PointCloudViewer::tr("The previous scene was kept unchanged."));
                    emit self->pointCloudLoadingCancelled();
                } else {
                    self->updateMessage(append ? PointCloudViewer::tr("Add failed") : PointCloudViewer::tr("Open failed"), localError);
                    emit self->pointCloudLoadingFailed(localError);
                }
                return;
            }

            if (!append) {
                self->clearPointCloud();
            }
            for (LoadedPointCloudDataset& dataset : datasets) {
                self->loadedPointCloudDatasets_.append(std::move(dataset));
            }
            QList<PointCloudDatasetInfo> combinedInfos = append
                ? DataManager::instance().pointCloudDatasets()
                : QList<PointCloudDatasetInfo>();
            combinedInfos.append(datasetInfos);
            DataManager::instance().setPointCloudDatasets(combinedInfos);
            if (append) {
                self->currentFilePaths_.append(normalizedFilePaths);
            } else {
                self->currentFilePaths_ = normalizedFilePaths;
            }
            self->nextDatasetId_ = combinedInfos.size() + 1;
            self->rebuildMergedPointCloud();
            self->syncVisualizationClassificationState();
            const bool sharedElevationRangeChanged =
                append && self->visualizationOptions_.colorMode == PointCloudColorMode::Elevation;
            self->rebuildScene(!sharedElevationRangeChanged);
            if (!append) {
                self->applyViewPreset(PointCloudViewPreset::Isometric);
            }
            self->updateFooter();
            self->updateWelcomeOverlayVisibility();
            self->setLoadingState(false, QString(), QString(), -1);
            emit self->pointCloudLoadingFinished();
            emit self->pointCloudLoaded();
            emit self->visualizationOptionsChanged();
            emit self->measurementChanged();
        }, Qt::QueuedConnection);
    });
}

void PointCloudViewer::cancelPointCloudLoading()
{
    if (!canCancelPointCloudLoading()) {
        return;
    }

    pointCloudLoadingCancellable_ = false;
    setLoadingState(true, pointCloudLoadingTitle_, tr("Cancelling point cloud loading..."), -1);
    asyncLoadCancellationRequested_.store(true);
}

void PointCloudViewer::cancelAsyncPointCloudLoad()
{
    asyncLoadCancellationRequested_.store(true);
    ++asyncLoadToken_;
    if (asyncLoadThread_.joinable()) {
        asyncLoadThread_.join();
    }
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
        const int datasetId = nextDatasetId_;
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
        if (!reader.read(filePath, &datasetPointCloud, &localError, &metadata, progressCallback, {}, datasetId)) {
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
        datasetInfo.datasetId = datasetId;
        ++nextDatasetId_;
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
        buildDatasetSpatialIndex(&loadedDataset);
        buildDatasetInteractionPreview(&loadedDataset);
        loadedDatasets.append(std::move(loadedDataset));
    }

    currentGaussianModel_.reset();
    if (osgWidget_ != nullptr) {
        osgWidget_->clearGaussianModel();
    }
    loadedPointCloudDatasets_ = std::move(loadedDatasets);
    invalidateMergedPointCloudCache();
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
                  .arg(formatPointCount(visiblePointCount()))
            : tr("Loaded %1 datasets with %2 points.")
                  .arg(QLocale().toString(normalizedFilePaths.size()))
                  .arg(formatPointCount(visiblePointCount()));
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

bool PointCloudViewer::appendPointCloudFilesAsync(const QStringList& filePaths, QString* errorMessage)
{
    if (pointCloudLoadingActive_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Another point cloud is still loading.");
        }
        return false;
    }
    for (const QString& filePath : filePaths) {
        const QString suffix = QFileInfo(filePath).suffix();
        if (suffix.compare(QStringLiteral("las"), Qt::CaseInsensitive) != 0
            && suffix.compare(QStringLiteral("laz"), Qt::CaseInsensitive) != 0) {
            return appendPointCloudFiles(filePaths, errorMessage);
        }
    }
    if (!hasLoadedPointClouds()) {
        return loadPointCloudFilesAsync(filePaths, errorMessage);
    }

    QStringList newFilePaths;
    for (const QString& filePath : filePaths) {
        const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
        if (!absolutePath.isEmpty()
            && !currentFilePaths_.contains(absolutePath, Qt::CaseInsensitive)
            && !newFilePaths.contains(absolutePath, Qt::CaseInsensitive)) {
            newFilePaths.append(absolutePath);
        }
    }
    if (newFilePaths.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("All selected datasets are already loaded.");
        }
        return true;
    }

    startAsyncPointCloudBatchLoad(newFilePaths, true);
    if (errorMessage != nullptr) {
        *errorMessage = tr("Adding point clouds in background...");
    }
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
        const int datasetId = nextDatasetId_;
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
        if (!reader.read(filePath, &datasetPointCloud, &localError, &metadata, progressCallback, {}, datasetId)) {
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
        datasetInfo.datasetId = datasetId;
        ++nextDatasetId_;
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
        buildDatasetSpatialIndex(&loadedDataset);
        buildDatasetInteractionPreview(&loadedDataset);
        newDatasets.append(std::move(loadedDataset));
    }

    for (LoadedPointCloudDataset& dataset : newDatasets) {
        loadedPointCloudDatasets_.append(std::move(dataset));
    }
    invalidateMergedPointCloudCache();
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
                  .arg(formatPointCount(visiblePointCount()))
            : tr("Added %1 datasets. Total datasets: %2, total points: %3.")
                  .arg(QLocale().toString(newFilePaths.size()))
                  .arg(QLocale().toString(currentFilePaths_.size()))
                  .arg(formatPointCount(visiblePointCount()));
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
    previewPointCloud_.reset();
    tiledPointCloudModeActive_ = false;
    fullResolutionTilesReady_ = false;
    previewTileSet_ = PointCloudTileSet();
    fullTileSet_ = PointCloudTileSet();
    promotedFullResolutionTiles_.clear();
    frameCameraStateValid_ = false;
    cameraMoving_ = false;
    if (refineIdleTimer_ != nullptr) {
        refineIdleTimer_->stop();
    }
    invalidateMergedPointCloudCache();
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
