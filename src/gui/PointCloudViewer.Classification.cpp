#include "gui/PointCloudViewer.h"
#include "gui/PointCloudViewerOverlays.h"

#include <QMetaObject>
#include <QLocale>
#include <QRubberBand>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <thread>

#include <osg/Camera>
#include <osg/Matrix>
#include <osg/Viewport>

#include "domain/DataManager.h"

bool PointCloudViewer::profileClassificationModeEnabled() const
{
    return profileClassificationModeEnabled_;
}

bool PointCloudViewer::profileClassificationTaskActive() const
{
    return profileClassificationTaskActive_;
}

const QSet<int>& PointCloudViewer::profileClassificationSourceClasses() const
{
    return profileClassificationSourceClasses_;
}

int PointCloudViewer::profileClassificationTargetClass() const
{
    return profileClassificationTargetClass_;
}

ProfileClassificationSelectionMode PointCloudViewer::profileClassificationSelectionMode() const
{
    return profileClassificationSelectionMode_;
}

bool PointCloudViewer::canUndoClassificationEdits() const
{
    return !classificationUndoStack_.isEmpty();
}

bool PointCloudViewer::canRedoClassificationEdits() const
{
    return !classificationRedoStack_.isEmpty();
}

int PointCloudViewer::classificationEditedPointCount() const
{
    return classificationEditStore_.editedPointCount();
}

const ClassificationEditStore& PointCloudViewer::classificationEditStore() const
{
    return classificationEditStore_;
}

void PointCloudViewer::setClassificationColor(int classification, const QColor& color)
{
    if (classification > 255 || !color.isValid()) {
        return;
    }

    if (classification < 0) {
        setClassificationFallbackColor(color);
        return;
    }

    const auto colorIt = visualizationOptions_.classificationColors.constFind(classification);
    if (colorIt != visualizationOptions_.classificationColors.constEnd() && colorIt.value() == color) {
        return;
    }

    visualizationOptions_.classificationColors.insert(classification, color);
    if (visualizationOptions_.colorMode == PointCloudColorMode::Classification && hasPointCloud()) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClassificationVisible(int classification, bool visible)
{
    if (classification < -1 || classification > 255) {
        return;
    }

    const bool currentVisible = visualizationOptions_.classificationVisibility.value(
        classification,
        visualizationOptions_.classificationVisibility.value(-1, true));
    if (currentVisible == visible) {
        return;
    }

    visualizationOptions_.classificationVisibility.insert(classification, visible);
    if (hasPointCloud()) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClassificationColorMap(const QMap<int, QColor>& colorMap)
{
    if (visualizationOptions_.classificationColors == colorMap) {
        return;
    }

    visualizationOptions_.classificationColors = colorMap;
    if (visualizationOptions_.colorMode == PointCloudColorMode::Classification && hasPointCloud()) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClassificationVisibilityMap(const QMap<int, bool>& visibilityMap)
{
    if (visualizationOptions_.classificationVisibility == visibilityMap) {
        return;
    }

    visualizationOptions_.classificationVisibility = visibilityMap;
    if (!visualizationOptions_.classificationVisibility.contains(-1)) {
        visualizationOptions_.classificationVisibility.insert(-1, true);
    }
    if (hasPointCloud()) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClassificationFallbackColor(const QColor& color)
{
    if (!color.isValid() || visualizationOptions_.classificationFallbackColor == color) {
        return;
    }

    visualizationOptions_.classificationFallbackColor = color;
    if (visualizationOptions_.colorMode == PointCloudColorMode::Classification && hasPointCloud()) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::resetClassificationColors()
{
    visualizationOptions_.classificationColors = defaultPointClassificationColors();
    visualizationOptions_.classificationVisibility = defaultPointClassificationVisibility();
    visualizationOptions_.classificationFallbackColor = defaultPointClassificationFallbackColor();

    if (hasPointCloud()) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setProfileClassificationModeEnabled(bool enabled)
{
    if (enabled && (!hasPointCloud() || pointCloudLoadingActive_ || tiledPointCloudModeActive_)) {
        emit measurementMessage(tr("Wait until the current point cloud is fully ready before starting profile classification."), true);
        return;
    }

    if (profileClassificationModeEnabled_ == enabled) {
        return;
    }

    if (enabled) {
        if (measurementEnabled_) {
            setMeasurementEnabled(false);
        }
        if (towerEditMode_ != TowerEditMode::None) {
            cancelTowerEditMode();
        }
        if (issueEditMode_ != IssueEditMode::None) {
            cancelIssueEditMode();
        }
    } else {
        clearSelectionRubberBand();
        clearProfileClassificationPolygonSelection();
    }

    profileClassificationModeEnabled_ = enabled;
    profileClassificationSelectionActive_ = false;
    profileClassificationSelectionRect_ = QRectF();
    if (enabled) {
        clearProfileClassificationPolygonSelection();
    }
    updateSceneClickCapture();
    updateProfileClassificationPolygonOverlay();
    updateFooter();
    emit profileClassificationModeChanged(profileClassificationModeEnabled_);
    emit profileClassificationStateChanged();
    emit measurementMessage(
        !profileClassificationModeEnabled_
            ? tr("Profile classification mode disabled.")
            : (profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
                ? tr("Profile classification mode enabled (polygon). Left-click to add vertices, double-click to apply, right-click to undo one vertex, drag to adjust view, and press Esc to clear or exit.")
                : tr("Profile classification mode enabled (rectangle). Drag a rectangle to classify source classes, hold Alt and drag left mouse to adjust view, right-click to exit, and press Esc to cancel.")),
        false);
}

void PointCloudViewer::setProfileClassificationSelectionMode(ProfileClassificationSelectionMode mode)
{
    if (profileClassificationSelectionMode_ == mode || profileClassificationTaskActive_) {
        return;
    }

    profileClassificationSelectionMode_ = mode;
    clearSelectionRubberBand();
    clearProfileClassificationPolygonSelection();
    updateSceneClickCapture();
    updateProfileClassificationPolygonOverlay();
    updateFooter();
    emit profileClassificationStateChanged();

    if (!profileClassificationModeEnabled_) {
        return;
    }

    emit measurementMessage(
        profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
            ? tr("Switched to polygon selection. Left-click to add vertices, double-click to apply, and right-click to undo one vertex.")
            : tr("Switched to rectangle selection. Drag a rectangle to apply profile classification."),
        false);
}

void PointCloudViewer::setProfileClassificationSourceClasses(const QSet<int>& classifications)
{
    if (profileClassificationSourceClasses_ == classifications) {
        return;
    }

    profileClassificationSourceClasses_ = classifications;
    emit profileClassificationStateChanged();
}

void PointCloudViewer::setProfileClassificationTargetClass(int classification)
{
    if (profileClassificationTargetClass_ == classification) {
        return;
    }

    profileClassificationTargetClass_ = classification;
    emit profileClassificationStateChanged();
}

void PointCloudViewer::undoClassificationEdit()
{
    if (classificationUndoStack_.isEmpty() || profileClassificationTaskActive_) {
        return;
    }

    const ClassificationEditBatch batch = classificationUndoStack_.takeLast();
    classificationEditStore_.revertBatch(batch);
    classificationRedoStack_.append(batch);
    syncVisualizationClassificationState();
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
    emit classificationEditsChanged();
    emit profileClassificationStateChanged();
    emit measurementMessage(
        tr("Reverted %1 profile classification point(s).")
            .arg(QLocale().toString(batch.changedCount)),
        false);
}

void PointCloudViewer::redoClassificationEdit()
{
    if (classificationRedoStack_.isEmpty() || profileClassificationTaskActive_) {
        return;
    }

    const ClassificationEditBatch batch = classificationRedoStack_.takeLast();
    classificationEditStore_.applyBatch(batch);
    classificationUndoStack_.append(batch);
    syncVisualizationClassificationState();
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
    emit classificationEditsChanged();
    emit profileClassificationStateChanged();
    emit measurementMessage(
        tr("Reapplied %1 profile classification point(s).")
            .arg(QLocale().toString(batch.changedCount)),
        false);
}

void PointCloudViewer::clearClassificationEdits()
{
    if (classificationEditStore_.isEmpty() && classificationUndoStack_.isEmpty() && classificationRedoStack_.isEmpty()) {
        return;
    }

    classificationEditStore_.clear();
    classificationUndoStack_.clear();
    classificationRedoStack_.clear();
    syncVisualizationClassificationState();
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
    emit classificationEditsChanged();
    emit profileClassificationStateChanged();
    emit measurementMessage(tr("Cleared all project profile classification edits."), false);
}

void PointCloudViewer::setClassificationEditStore(const ClassificationEditStore& store)
{
    classificationEditStore_ = store;
    classificationUndoStack_.clear();
    classificationRedoStack_.clear();
    syncVisualizationClassificationState();
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
    emit classificationEditsChanged();
    emit profileClassificationStateChanged();
}

void PointCloudViewer::commitClassificationEditsToPointCloudData()
{
    if (classificationEditStore_.isEmpty()) {
        return;
    }

    const ClassificationEditStore::StoreMap editsByDataset = classificationEditStore_.editsByDataset();
    bool changedAnyPoint = false;
    QList<PointCloudDatasetInfo> datasetInfos = DataManager::instance().pointCloudDatasets();

    for (LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (dataset.pointCloud == nullptr) {
            continue;
        }

        auto editsIt = editsByDataset.constFind(dataset.info.filePath);
        if (editsIt == editsByDataset.constEnd()) {
            for (auto probeIt = editsByDataset.constBegin(); probeIt != editsByDataset.constEnd(); ++probeIt) {
                if (probeIt.key().compare(dataset.info.filePath, Qt::CaseInsensitive) == 0) {
                    editsIt = probeIt;
                    break;
                }
            }
        }
        if (editsIt == editsByDataset.constEnd()) {
            continue;
        }

        std::vector<PointRecord>& points = dataset.pointCloud->mutablePoints();
        bool datasetHasClassification = dataset.info.hasClassification;
        for (auto pointIt = editsIt->constBegin(); pointIt != editsIt->constEnd(); ++pointIt) {
            const std::size_t pointIndex = static_cast<std::size_t>(pointIt.key());
            if (pointIndex >= points.size()) {
                continue;
            }

            PointRecord& point = points[pointIndex];
            point.classification = static_cast<std::uint8_t>(std::clamp(pointIt.value(), 0, 255));
            point.hasClassification = true;
            datasetHasClassification = true;
            changedAnyPoint = true;
        }

        dataset.info.hasClassification = datasetHasClassification;
        for (PointCloudDatasetInfo& info : datasetInfos) {
            if (info.filePath.compare(dataset.info.filePath, Qt::CaseInsensitive) == 0) {
                info.hasClassification = datasetHasClassification;
                break;
            }
        }
    }

    if (changedAnyPoint) {
        DataManager::instance().setPointCloudDatasets(datasetInfos);
    }

    classificationEditStore_.clear();
    classificationUndoStack_.clear();
    classificationRedoStack_.clear();
    syncVisualizationClassificationState();
    rebuildMergedPointCloud();
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
    emit classificationEditsChanged();
    emit profileClassificationStateChanged();
}

void PointCloudViewer::syncVisualizationClassificationState()
{
    visualizationOptions_.classificationEditStore =
        std::make_shared<ClassificationEditStore>(classificationEditStore_);
    visualizationOptions_.classificationDatasetPathsById.clear();
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        visualizationOptions_.classificationDatasetPathsById.insert(
            dataset.info.datasetId,
            dataset.info.filePath);
    }
}

void PointCloudViewer::clearSelectionRubberBand()
{
    profileClassificationSelectionActive_ = false;
    profileClassificationSelectionRect_ = QRectF();
    if (selectionRubberBand_ != nullptr) {
        selectionRubberBand_->hide();
    }
}

void PointCloudViewer::clearProfileClassificationPolygonSelection()
{
    profileClassificationPolygonPoints_.clear();
    profileClassificationPolygonPreviewPoint_ = QPointF();
    profileClassificationPolygonPreviewActive_ = false;
    updateProfileClassificationPolygonOverlay();
}

void PointCloudViewer::updateProfileClassificationPolygonOverlay()
{
    if (profileClassificationPolygonOverlay_ == nullptr || osgWidget_ == nullptr) {
        return;
    }

    profileClassificationPolygonOverlay_->setGeometry(osgWidget_->rect());
    auto* overlay = static_cast<PolygonSelectionOverlay*>(profileClassificationPolygonOverlay_);

    const bool polygonVisible =
        profileClassificationModeEnabled_
        && profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
        && !profileClassificationTaskActive_
        && !profileClassificationPolygonPoints_.isEmpty();

    if (!polygonVisible) {
        overlay->setPolygonState(QPolygonF(), false, QPointF());
        return;
    }

    overlay->setPolygonState(
        profileClassificationPolygonPoints_,
        profileClassificationPolygonPreviewActive_,
        profileClassificationPolygonPreviewPoint_);
}

void PointCloudViewer::tryFinishProfileClassificationPolygonSelection()
{
    if (!profileClassificationModeEnabled_
        || profileClassificationSelectionMode_ != ProfileClassificationSelectionMode::Polygon
        || profileClassificationTaskActive_) {
        return;
    }

    if (profileClassificationPolygonPoints_.size() < 3) {
        emit measurementMessage(tr("Add at least three polygon vertices before applying profile classification."), true);
        return;
    }

    const QPolygonF polygon = profileClassificationPolygonPoints_;
    clearProfileClassificationPolygonSelection();
    beginProfileClassificationSelection(polygon.boundingRect(), polygon);
}

void PointCloudViewer::handleSelectionRectangleChanged(const QRectF& localRect, bool active)
{
    if (profileClassificationSelectionMode_ != ProfileClassificationSelectionMode::Rectangle) {
        return;
    }

    profileClassificationSelectionActive_ = active;
    profileClassificationSelectionRect_ = active ? localRect.normalized() : QRectF();
    if (selectionRubberBand_ == nullptr) {
        return;
    }

    if (!active || localRect.isNull()) {
        selectionRubberBand_->hide();
        return;
    }

    selectionRubberBand_->setGeometry(localRect.normalized().toRect());
    selectionRubberBand_->show();
    selectionRubberBand_->raise();
}

void PointCloudViewer::handleSelectionRectangleFinished(const QRectF& localRect)
{
    if (profileClassificationSelectionMode_ != ProfileClassificationSelectionMode::Rectangle) {
        return;
    }

    clearSelectionRubberBand();
    beginProfileClassificationSelection(localRect.normalized(), QPolygonF());
}

void PointCloudViewer::handleSelectionEscapePressed()
{
    if (!profileClassificationModeEnabled_
        || profileClassificationSelectionMode_ != ProfileClassificationSelectionMode::Rectangle) {
        return;
    }

    clearSelectionRubberBand();
    setProfileClassificationModeEnabled(false);
}

void PointCloudViewer::beginProfileClassificationSelection(const QRectF& viewportRect, const QPolygonF& viewportPolygon)
{
    if (!profileClassificationModeEnabled_ || profileClassificationTaskActive_) {
        return;
    }
    if (profileClassificationSourceClasses_.isEmpty()) {
        emit measurementMessage(tr("Choose at least one source classification before profile classification."), true);
        return;
    }
    if (!hasPointCloud() || osgWidget_ == nullptr) {
        return;
    }

    if (classificationTaskThread_.joinable()) {
        classificationTaskThread_.join();
    }

    struct DatasetSnapshot
    {
        QString datasetPath;
        std::shared_ptr<PointCloudData> pointCloud;
    };

    QList<DatasetSnapshot> datasets;
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (!dataset.info.visible || dataset.pointCloud == nullptr) {
            continue;
        }
        datasets.append({ dataset.info.filePath, dataset.pointCloud });
    }

    if (datasets.isEmpty()) {
        emit measurementMessage(tr("No visible datasets are available for profile classification."), true);
        return;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCamera() == nullptr || viewer->getCamera()->getViewport() == nullptr) {
        return;
    }

    const osg::Matrixd worldToWindow =
        viewer->getCamera()->getViewMatrix()
        * viewer->getCamera()->getProjectionMatrix()
        * viewer->getCamera()->getViewport()->computeWindowMatrix();
    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    const osg::Matrixd localToWindow = osg::Matrixd::translate(sceneOrigin) * worldToWindow;
    const QSet<int> sourceClasses = profileClassificationSourceClasses_;
    const int targetClassification = profileClassificationTargetClass_;
    const bool usePolygon = viewportPolygon.size() >= 3;
    const QPolygonF selectionPolygon = usePolygon ? viewportPolygon : QPolygonF();
    const QRectF selectionRect = usePolygon ? selectionPolygon.boundingRect() : viewportRect.normalized();
    if (selectionRect.width() < 2.0 || selectionRect.height() < 2.0) {
        emit measurementMessage(tr("Selection region is too small for profile classification."), true);
        return;
    }
    const std::shared_ptr<const ClassificationEditStore> editSnapshot =
        std::make_shared<ClassificationEditStore>(classificationEditStore_);
    const qreal devicePixelRatio = osgWidget_->devicePixelRatioF();
    const int widgetHeight = osgWidget_->height();
    const std::uint64_t token = ++classificationTaskToken_;
    const auto taskStartTime = std::chrono::steady_clock::now();

    profileClassificationTaskActive_ = true;
    classificationTaskStartTime_ = taskStartTime;
    classificationTaskScannedPoints_.store(0);
    updateFooter();
    if (classificationTaskStatusTimer_ != nullptr) {
        classificationTaskStatusTimer_->start();
    }
    emit profileClassificationStateChanged();
    emit measurementMessage(
        usePolygon
            ? tr("Applying profile classification polygon selection...")
            : tr("Applying profile classification selection..."),
        false);

    classificationTaskThread_ = std::thread([this, token, datasets, sourceClasses, targetClassification, selectionRect, selectionPolygon, usePolygon, localToWindow, sceneOrigin, editSnapshot, devicePixelRatio, widgetHeight, taskStartTime]() {
        ClassificationEditBatch batch;
        batch.targetClassification = targetClassification;
        std::uint64_t scannedPointCount = 0;

        const double minX = selectionRect.left();
        const double maxX = selectionRect.right();
        const double minY = selectionRect.top();
        const double maxY = selectionRect.bottom();

        for (const DatasetSnapshot& dataset : datasets) {
            if (dataset.pointCloud == nullptr) {
                continue;
            }

            const std::vector<PointRecord>& points = dataset.pointCloud->points();
            for (const PointRecord& point : points) {
                ++scannedPointCount;
                if ((scannedPointCount & 0x1FFFu) == 0u) {
                    classificationTaskScannedPoints_.store(scannedPointCount);
                }
                const osg::Vec3d projected = osg::Vec3d(
                    point.x - sceneOrigin.x(),
                    point.y - sceneOrigin.y(),
                    point.z - sceneOrigin.z())
                    * localToWindow;
                if (projected.z() < 0.0 || projected.z() > 1.0) {
                    continue;
                }

                const QPointF viewportPoint(
                    projected.x() / devicePixelRatio,
                    static_cast<double>(widgetHeight) - projected.y() / devicePixelRatio);
                if (viewportPoint.x() < minX || viewportPoint.x() > maxX || viewportPoint.y() < minY || viewportPoint.y() > maxY) {
                    continue;
                }
                if (usePolygon && !selectionPolygon.containsPoint(viewportPoint, Qt::WindingFill)) {
                    continue;
                }

                const int rawClassification = point.hasClassification ? static_cast<int>(point.classification) : -1;
                const int effectiveClassification = editSnapshot != nullptr
                    ? editSnapshot->effectiveClassification(dataset.datasetPath, point.sourcePointIndex, rawClassification)
                    : rawClassification;
                if (!sourceClasses.contains(effectiveClassification)) {
                    continue;
                }

                ++batch.hitCount;
                if (effectiveClassification == targetClassification) {
                    continue;
                }

                ClassificationEditBatchItem item;
                item.datasetPath = dataset.datasetPath;
                item.pointIndex = point.sourcePointIndex;
                item.previousEffectiveClassification = effectiveClassification;
                item.targetClassification = targetClassification;
                item.hadPreviousOverride = editSnapshot != nullptr
                    && editSnapshot->tryGetOverride(dataset.datasetPath, point.sourcePointIndex, &item.previousOverrideClassification);
                batch.items.append(item);
            }
        }

        batch.changedCount = batch.items.size();
        classificationTaskScannedPoints_.store(scannedPointCount);
        const auto elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - taskStartTime);
        QMetaObject::invokeMethod(
            this,
            [this, token, batch, scannedPointCount, elapsedMilliseconds]() {
                finalizeProfileClassificationTask(
                    token,
                    batch,
                    scannedPointCount,
                    static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsedMilliseconds.count())));
            },
            Qt::QueuedConnection);
    });
}

void PointCloudViewer::finalizeProfileClassificationTask(
    std::uint64_t token,
    const ClassificationEditBatch& batch,
    std::uint64_t scannedPointCount,
    std::uint64_t elapsedMilliseconds)
{
    if (token != classificationTaskToken_) {
        return;
    }

    if (classificationTaskThread_.joinable()) {
        classificationTaskThread_.join();
    }

    profileClassificationTaskActive_ = false;
    lastClassificationTaskScannedPoints_ = scannedPointCount;
    lastClassificationTaskElapsedMilliseconds_ = elapsedMilliseconds;
    if (classificationTaskStatusTimer_ != nullptr) {
        classificationTaskStatusTimer_->stop();
    }
    if (!batch.isEmpty()) {
        classificationEditStore_.applyBatch(batch);
        classificationUndoStack_.append(batch);
        classificationRedoStack_.clear();
        syncVisualizationClassificationState();
        rebuildScene();
        emit visualizationOptionsChanged();
        emit classificationEditsChanged();
    }

    updateFooter();
    emit profileClassificationStateChanged();
    const QString completionStats = QStringLiteral(" (%1, %2ms)")
        .arg(QLocale().toString(static_cast<qlonglong>(scannedPointCount)))
        .arg(QLocale().toString(static_cast<qlonglong>(elapsedMilliseconds)));
    emit measurementMessage(
        tr("Profile classification completed. %1 point(s) hit, %2 point(s) changed to class %3.")
            .arg(QLocale().toString(batch.hitCount))
            .arg(QLocale().toString(batch.changedCount))
            .arg(QLocale().toString(batch.targetClassification))
            + completionStats,
        false);
}

int PointCloudViewer::effectiveClassificationForPoint(const QString& datasetPath, const PointRecord& point) const
{
    const int rawClassification = point.hasClassification ? static_cast<int>(point.classification) : -1;
    return classificationEditStore_.effectiveClassification(datasetPath, point.sourcePointIndex, rawClassification);
}
