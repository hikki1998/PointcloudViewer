#pragma once

#include <chrono>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include <QColor>
#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QGridLayout>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include <osg/ref_ptr>
#include <osg/Matrix>
#include <osg/Vec3>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/Viewer>

#include "domain/ClassificationEditStore.h"
#include "domain/DataManager.h"
#include "domain/InspectionData.h"
#include "osg/PointCloudVisualization.h"
#include "pointcloud/PointCloudData.h"
#include "pointcloud/PointCloudTileStore.h"

class QLabel;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QRubberBand;
class QResizeEvent;
class QTimer;
class QWheelEvent;
class QFrame;
class OsgPointCloudNode;
struct LasFileMetadata;

namespace osg
{
class Group;
class Node;
}

namespace osgGA
{
class EventQueue;
class CameraManipulator;
}

struct InteractionOptions
{
    bool invertOrbitDrag = false;
    bool invertPanDrag = false;
    bool invertWheelZoom = false;
    int wheelZoomSensitivityPercent = 100;
};

struct MeasurementResult
{
    QList<PointRecord> points;
    bool hasStartPoint = false;
    bool hasEndPoint = false;
    PointRecord startPoint;
    PointRecord endPoint;
    float distance3d = 0.0f;
    float deltaZ = 0.0f;

    [[nodiscard]] int pointCount() const
    {
        return points.size();
    }

    [[nodiscard]] bool isComplete() const
    {
        return points.size() >= 2;
    }
};

struct InspectionRouteDisplayData
{
    QList<PointRecord> waypoints;
    QStringList labels;
    QList<PointRecord> partPoints;
    QStringList partLabels;
    QList<int> partPointIndices;
    QList<PointRecord> waypointTargetPoints;
    QList<bool> waypointHasTargetPoints;
    QList<double> waypointAircraftYawDegs;
    QList<double> waypointGimbalPitchDegs;
    QList<double> waypointCameraYawDegs;
    QList<double> waypointCameraPitchDegs;
    QStringList waypointTargetLabels;
    QList<QList<PointRecord>> waypointAllTargetPoints;
    QList<QList<int>> waypointAllTargetPartIndices;
    QList<QList<double>> waypointAllCameraYawDegs;
    QList<QList<double>> waypointAllCameraPitchDegs;
    QList<QStringList> waypointAllTargetLabels;
};

enum class RouteLabelDisplayMode
{
    Sequence = 0,
    Name
};

enum class RouteRoamViewMode
{
    FirstPerson = 0,
    ThirdPerson
};

enum class TowerEditMode
{
    None = 0,
    AddAfterLast,
    InsertBeforeSelected,
    MoveSelected
};

enum class IssueEditMode
{
    None = 0,
    Add
};

enum class ProfileClassificationSelectionMode
{
    Rectangle = 0,
    Polygon
};

class OsgWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit OsgWidget(QWidget* parent = nullptr);
    ~OsgWidget() override;

    osgViewer::Viewer* getViewer() { return viewer_.get(); }
    const InteractionOptions& interactionOptions() const { return interactionOptions_; }
    void setInteractionOptions(const InteractionOptions& options);
    void setSceneClickModeEnabled(bool enabled);
    void setRectangleSelectionEnabled(bool enabled);
    void setSceneDragCaptureEnabled(bool enabled);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void leaveEvent(QEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

signals:
    void scenePressed(const QPointF& localPos);
    void sceneClicked(const QPointF& localPos);
    void sceneDoubleClicked(const QPointF& localPos);
    void sceneDragged(const QPointF& localPos);
    void sceneDragReleased(const QPointF& localPos);
    void sceneEscapePressed();
    void sceneSecondaryClicked(const QPointF& localPos);
    void sceneHovered(const QPointF& localPos);
    void sceneHoverEnded();
    void selectionRectangleChanged(const QRectF& localRect, bool active);
    void selectionRectangleFinished(const QRectF& localRect);
    void selectionEscapePressed();
    void frameRendered();

private:
    osgGA::EventQueue* eventQueue() const;
    void updateViewport(int width, int height);
    int mapMouseButton(Qt::MouseButton button) const;
    void dispatchMouseButtonEvent(const QPointF& localPos, Qt::MouseButton button, bool pressed);
    void dispatchMouseMotion(const QPointF& localPos);
    static float toDevicePixels(float value, float devicePixelRatio);

    osg::ref_ptr<osgViewer::Viewer> viewer_;
    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> graphicsWindow_;
    bool initialized_ = false;
    InteractionOptions interactionOptions_;
    bool sceneClickModeEnabled_ = false;
    bool sceneDragCaptureEnabled_ = false;
    bool leftButtonPressed_ = false;
    bool middleButtonPressed_ = false;
    bool rightButtonPressed_ = false;
    bool leftButtonDragDetected_ = false;
    bool leftButtonEventDispatched_ = false;
    bool rightButtonDragDetected_ = false;
    bool rectangleSelectionEnabled_ = false;
    bool selectionDragActive_ = false;
    QPointF leftButtonAnchor_;
    QPointF middleButtonAnchor_;
    QPointF rightButtonAnchor_;
    QPointF selectionAnchor_;
    QPointF lastOrbitCursorPosition_;
    QPointF lastOrbitEventPosition_;
    QPointF lastPanCursorPosition_;
    QPointF lastPanEventPosition_;
};

class PointCloudViewer final : public QWidget
{
    Q_OBJECT

public:
    explicit PointCloudViewer(QWidget* parent = nullptr);
    ~PointCloudViewer() override;

    bool loadPointCloud(const QString& filePath, QString* errorMessage = nullptr);
    bool loadPointCloudFiles(const QStringList& filePaths, QString* errorMessage = nullptr);
    bool loadPointCloudFilesAsync(const QStringList& filePaths, QString* errorMessage = nullptr);
    bool appendPointCloudFiles(const QStringList& filePaths, QString* errorMessage = nullptr);
    void clearPointCloud();
    void showTransientPreviewPointCloud(const QString& filePath, const PointCloudData& pointCloudPreview, const QString& detailMessage);

    bool hasPointCloud() const;
    bool hasLoadedPointClouds() const;
    bool isPointCloudLoadingInProgress() const;
    bool hasFullResolutionPointCloud() const;
    QString currentFilePath() const;
    QStringList currentFilePaths() const;
    const PointCloudData* pointCloudData() const;
    const PointCloudData* fullResolutionPointCloudData(QString* errorMessage = nullptr);
    const QList<PointCloudDatasetInfo>& pointCloudDatasets() const;
    const PointCloudVisualizationOptions& visualizationOptions() const;
    const InteractionOptions& interactionOptions() const;
    bool measurementEnabled() const;
    bool profileClassificationModeEnabled() const;
    bool profileClassificationTaskActive() const;
    const MeasurementResult& measurementResult() const;
    bool hasHoveredPoint() const;
    PointRecord hoveredPoint() const;
    const QList<TowerRecord>& towerMarkers() const;
    const QList<InspectionIssue>& inspectionIssues() const;
    const QList<PointRecord>& inspectionRouteWaypoints() const;
    int selectedTowerIndex() const;
    int selectedIssueIndex() const;
    int selectedInspectionRouteWaypointIndex() const;
    int selectedInspectionRouteWaypointTargetIndex() const;
    TowerEditMode towerEditMode() const;
    int towerEditTargetIndex() const;
    IssueEditMode issueEditMode() const;
    const QSet<int>& profileClassificationSourceClasses() const;
    int profileClassificationTargetClass() const;
    ProfileClassificationSelectionMode profileClassificationSelectionMode() const;
    bool canUndoClassificationEdits() const;
    bool canRedoClassificationEdits() const;
    int classificationEditedPointCount() const;
    const ClassificationEditStore& classificationEditStore() const;
    bool focusOnPoint(const PointRecord& point, double distanceScale = 0.35);
    bool focusOnBounds(const PointRecord& minBounds, const PointRecord& maxBounds, double distanceScale = 1.0);
    bool setPointCloudDatasetVisible(const QString& filePath, bool visible);
    bool isInspectionIssueVisible(int index) const;
    void setInspectionIssueVisible(int index, bool visible);
    bool inspectionRouteVisible() const;
    void setInspectionRouteVisible(bool visible);
    bool inspectionRouteRoamActive() const;
    bool inspectionRouteRoamPlaying() const;
    bool inspectionRouteRoamPaused() const;
    double inspectionRouteRoamSpeedMetersPerSecond() const;
    RouteRoamViewMode inspectionRouteRoamViewMode() const;
    QColor inspectionRouteWaypointColor() const;
    QColor inspectionRoutePartPointColor() const;
    QColor inspectionRouteTrajectoryColor() const;

public slots:
    void setPointSize(int pointSize);
    void setPointOpacity(int opacityPercent);
    void setColorMode(int colorModeIndex);
    void setColorMode(PointCloudColorMode colorMode);
    void setSingleColor(const QColor& color);
    void setClassificationColor(int classification, const QColor& color);
    void setClassificationVisible(int classification, bool visible);
    void setClassificationColorMap(const QMap<int, QColor>& colorMap);
    void setClassificationVisibilityMap(const QMap<int, bool>& visibilityMap);
    void setClassificationFallbackColor(const QColor& color);
    void resetClassificationColors();
    void setBackgroundColor(const QColor& color);
    void setDepthCueStrength(int strengthPercent);
    void setEdlStrength(int strengthPercent);
    void setUseRoundSplats(bool enabled);
    void setShowAxes(bool showAxes);
    void setShowBoundingBox(bool showBoundingBox);
    void resetView();
    void setViewPreset(PointCloudViewPreset viewPreset);
    void setInteractionOptions(const InteractionOptions& options);
    void setInvertOrbitDrag(bool invert);
    void setInvertPanDrag(bool invert);
    void setInvertWheelZoom(bool invert);
    void setWheelZoomSensitivityPercent(int percent);
    void setMeasurementEnabled(bool enabled);
    void setProfileClassificationModeEnabled(bool enabled);
    void setProfileClassificationSelectionMode(ProfileClassificationSelectionMode mode);
    void setProfileClassificationSourceClasses(const QSet<int>& classifications);
    void setProfileClassificationTargetClass(int classification);
    void undoClassificationEdit();
    void redoClassificationEdit();
    void clearClassificationEdits();
    void setClassificationEditStore(const ClassificationEditStore& store);
    void commitClassificationEditsToPointCloudData();
    void clearMeasurement();
    bool addTowerMarker(const QString& name, const PointRecord& point);
    bool insertTowerMarker(int index, const QString& name, const PointRecord& point);
    bool addTowerMarkerFromHoveredPoint(const QString& name, QString* errorMessage = nullptr);
    void setTowerMarkers(const QList<TowerRecord>& towerMarkers);
    bool setTowerMarkerName(int index, const QString& name);
    bool setTowerRecord(int index, const TowerRecord& towerRecord);
    void setSelectedTowerIndex(int index);
    bool removeTowerMarker(int index);
    bool moveTowerMarker(int index, const PointRecord& point);
    void clearTowerMarkers();
    void beginTowerAddMode();
    void beginTowerInsertMode(int beforeIndex);
    void beginTowerMoveMode(int towerIndex);
    void cancelTowerEditMode();
    void setInspectionIssues(const QList<InspectionIssue>& issues);
    bool addInspectionIssue(const InspectionIssue& issue);
    bool updateInspectionIssue(int index, const InspectionIssue& issue);
    bool removeInspectionIssue(int index);
    void clearInspectionIssues();
    void setSelectedIssueIndex(int index);
    void setInspectionRouteDisplayData(const InspectionRouteDisplayData& displayData);
    void setInspectionRouteWaypoints(const QList<PointRecord>& waypoints, const QStringList& labels = QStringList());
    RouteLabelDisplayMode inspectionRouteWaypointLabelDisplayMode() const;
    RouteLabelDisplayMode inspectionRoutePartLabelDisplayMode() const;
    void setInspectionRouteWaypointLabelDisplayMode(RouteLabelDisplayMode mode);
    void setInspectionRoutePartLabelDisplayMode(RouteLabelDisplayMode mode);
    void setInspectionRouteWaypointColor(const QColor& color);
    void setInspectionRoutePartPointColor(const QColor& color);
    void setInspectionRouteTrajectoryColor(const QColor& color);
    void clearInspectionRouteWaypoints();
    void setSelectedInspectionRouteWaypointIndex(int index);
    void setSelectedInspectionRouteWaypointTargetIndex(int index);
    bool inspectionRouteEditingEnabled() const;
    void setInspectionRouteEditingEnabled(bool enabled);
    void setInspectionRouteRoamSpeedMetersPerSecond(double speedMetersPerSecond);
    void setInspectionRouteRoamViewMode(RouteRoamViewMode mode);
    void startInspectionRouteRoam(int startWaypointIndex = -1);
    void pauseInspectionRouteRoam();
    void resumeInspectionRouteRoam();
    void stopInspectionRouteRoam(bool restoreManualView = false);
    void beginIssueAddMode();
    void cancelIssueEditMode();

signals:
    void pointCloudLoaded();
    void pointCloudCleared();
    void pointCloudLoadingStarted(const QString& message);
    void pointCloudLoadingProgress(const QString& message, int value, int maximum);
    void pointCloudLoadingFinished();
    void visualizationOptionsChanged();
    void interactionOptionsChanged();
    void measurementChanged();
    void measurementModeChanged();
    void measurementMessage(const QString& message, bool error);
    void profileClassificationModeChanged(bool enabled);
    void profileClassificationStateChanged();
    void classificationEditsChanged();
    void towerMarkersChanged();
    void selectedTowerChanged(int index);
    void towerEditModeChanged();
    void towerEditRequested(const PointRecord& point, int mode, int targetIndex);
    void inspectionIssuesChanged();
    void selectedIssueChanged(int index);
    void issueEditModeChanged();
    void issueEditRequested(const PointRecord& point);
    void inspectionRouteChanged();
    void selectedInspectionRouteWaypointChanged(int index);
    void inspectionRouteWaypointDoubleClicked(int index);
    void inspectionRouteWaypointDragFinished(int index, const PointRecord& point);
    void inspectionRouteRoamStateChanged();
    void inspectionRouteRoamPhotoCaptured(int waypointIndex, int targetIndex, const QString& targetLabel, int captureCount);

private:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void createStatusPanel();
    void createMeasurementOverlayWidgets();
    void createWelcomeOverlay();
    void createRouteCameraPreviewOverlay();
    void startAsyncSingleFileLoad(const QString& filePath);
    void cancelAsyncPointCloudLoad();
    void completeAsyncLoadFailure(std::uint64_t token, const QString& errorMessage);
    void applyAsyncPreview(
        std::uint64_t token,
        const QString& filePath,
        const LasFileMetadata& metadata,
        const std::shared_ptr<PointCloudData>& previewPointCloud,
        const std::shared_ptr<PointCloudTileSet>& previewTiles);
    void applyAsyncFullTiles(
        std::uint64_t token,
        const QString& filePath,
        const LasFileMetadata& metadata,
        const std::shared_ptr<PointCloudTileSet>& fullTiles);
    void handleAsyncLoadProgress(
        std::uint64_t token,
        const QString& stageTitle,
        const QString& detail,
        int overallValue,
        int overallMaximum);
    void refreshAsyncVisiblePointCloudState();
    void updateTiledPointCloudScene();
    void scheduleTileRefinement();
    void promoteNextTileBatch();
    void handleFrameRendered();
    QList<PointCloudTileId> prioritizedTileOrder() const;
    bool tileBoundsProjectToViewport(
        const PointRecord& minBounds,
        const PointRecord& maxBounds,
        QRectF* viewportBounds,
        double* depthHint = nullptr) const;
    const PointCloudData* activePointCloudDataForTile(const PointCloudTileId& tileId) const;
    const PointCloudTileData* findTileData(const PointCloudTileSet& tileSet, const PointCloudTileId& tileId) const;
    const PointCloudData* ensureFullResolutionPointCloudCache(QString* errorMessage = nullptr);
    void rebuildScene();
    void rebuildMergedPointCloud();
    void updateSceneOriginFromCurrentPointCloud();
    osg::Vec3d overlaySceneOrigin() const;
    void updateFooter();
    void updateMessage(const QString& title, const QString& detail);
    void applyClearColor();
    void applyViewPreset(PointCloudViewPreset viewPreset);
    void handleScenePress(const QPointF& localPos);
    void handleSceneClick(const QPointF& localPos);
    void handleSceneDoubleClick(const QPointF& localPos);
    void handleSceneDrag(const QPointF& localPos);
    void handleSceneDragRelease(const QPointF& localPos);
    void handleSceneEscapePressed();
    void handleSceneSecondaryClick(const QPointF& localPos);
    void handleSceneHover(const QPointF& localPos);
    void handleSelectionRectangleChanged(const QRectF& localRect, bool active);
    void handleSelectionRectangleFinished(const QRectF& localRect);
    void handleSelectionEscapePressed();
    void clearHoveredPoint();
    void updateHoveredPoint(const PointRecord* hoveredPoint);
    void syncVisualizationClassificationState();
    void clearSelectionRubberBand();
    void clearProfileClassificationPolygonSelection();
    void updateProfileClassificationPolygonOverlay();
    void tryFinishProfileClassificationPolygonSelection();
    void beginProfileClassificationSelection(const QRectF& viewportRect, const QPolygonF& viewportPolygon = QPolygonF());
    void finalizeProfileClassificationTask(
        std::uint64_t token,
        const ClassificationEditBatch& batch,
        std::uint64_t scannedPointCount,
        std::uint64_t elapsedMilliseconds);
    int effectiveClassificationForPoint(const QString& datasetPath, const PointRecord& point) const;
    bool pickPointAtScreenPosition(const QPointF& localPos, PointRecord* pickedPoint, float tolerancePixels = 14.0f) const;
    int pickTowerMarkerAtScreenPosition(const QPointF& localPos, float tolerancePixels = 18.0f) const;
    int pickInspectionIssueAtScreenPosition(const QPointF& localPos, float tolerancePixels = 18.0f) const;
    int pickInspectionRouteWaypointAtScreenPosition(const QPointF& localPos, float tolerancePixels = 18.0f) const;
    osg::ref_ptr<osg::Node> buildMeasurementOverlay() const;
    osg::ref_ptr<osg::Node> buildTowerMarkersOverlay() const;
    osg::ref_ptr<osg::Node> buildInspectionIssuesOverlay() const;
    osg::ref_ptr<osg::Node> buildInspectionRouteOverlay() const;
    QPointF projectPointToViewport(const PointRecord& point, bool* visible) const;
    void refreshMeasurementOverlay();
    void refreshTowerMarkersOverlay();
    void refreshInspectionIssuesOverlay();
    void refreshInspectionRouteOverlay();
    void updateMeasurementOverlayWidgets();
    void updateTowerOverlayWidgets();
    void updateInspectionIssueOverlayWidgets();
    void updateInspectionRouteOverlayWidgets();
    void updateRouteCameraPreviewOverlay();
    void updateInspectionRouteRoam();
    void updateSceneClickCapture();
    void updateAxisIndicator();
    void positionAxisIndicator();
    void positionRouteCameraPreviewOverlay();
    QString inspectionRouteWaypointLabelText(int index) const;
    QString inspectionRoutePartLabelText(int index) const;
    int normalizeInspectionRouteWaypointTargetIndex(int waypointIndex, int targetIndex) const;
    void routeRoamUpdateSelectionState(int waypointIndex);
    void routeRoamTriggerPhotoCapture(int waypointIndex, int targetIndex);
    bool routeRoamComputeWaypointPose(
        int waypointIndex,
        const QList<PointRecord>& waypoints,
        osg::Vec3d* position,
        osg::Vec3d* forward,
        osg::Vec3d* up) const;
    bool routeRoamApplyPose(
        const osg::Vec3d& position,
        const osg::Vec3d& forward,
        const osg::Vec3d& up);
    void routeRoamCaptureManualView();
    void routeRoamRestoreManualView();
    void routeRoamStopInternal(bool restoreManualView);
    void positionOverlayLabel(QLabel* label, const QPointF& anchor, const QPoint& offset) const;
    void setLoadingState(bool active, const QString& title, const QString& detail, int progressPercent);
    void updateWelcomeOverlayVisibility();
    void syncCurrentFilePath();
    void normalizeTowerMarkerIndices();
    void recalculateMeasurementResult();
    bool undoLastMeasurementPoint();
    void resetMeasurementState(bool notifyChange = true);
    void retranslateUi();

    struct LoadedPointCloudDataset
    {
        PointCloudDatasetInfo info;
        std::shared_ptr<PointCloudData> pointCloud;
    };

    QGridLayout* layout_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* detailLabel_ = nullptr;
    QLabel* cursorLabel_ = nullptr;
    QFrame* welcomeOverlay_ = nullptr;
    QLabel* welcomeImageLabel_ = nullptr;
    QLabel* welcomeStatusLabel_ = nullptr;
    QLabel* measurementStartOverlayLabel_ = nullptr;
    QLabel* measurementEndOverlayLabel_ = nullptr;
    QLabel* measurementSummaryOverlayLabel_ = nullptr;
    QWidget* axisIndicatorOverlay_ = nullptr;
    QRubberBand* selectionRubberBand_ = nullptr;
    QWidget* profileClassificationPolygonOverlay_ = nullptr;
    OsgWidget* osgWidget_ = nullptr;
    QWidget* statusPanel_ = nullptr;
    QWidget* routeCameraPreviewOverlay_ = nullptr;

    std::shared_ptr<PointCloudData> currentPointCloud_;
    std::shared_ptr<PointCloudData> previewPointCloud_;
    mutable std::shared_ptr<PointCloudData> fullResolutionPointCloudCache_;
    osg::Vec3d sceneOriginWorld_;
    bool sceneOriginValid_ = false;
    QString currentFilePath_;
    QStringList currentFilePaths_;
    QList<LoadedPointCloudDataset> loadedPointCloudDatasets_;
    PointCloudVisualizationOptions visualizationOptions_;
    InteractionOptions interactionOptions_;
    bool pointCloudLoadingActive_ = false;
    QString pointCloudLoadingTitle_;
    QString pointCloudLoadingDetail_;
    int pointCloudLoadingProgressPercent_ = -1;
    bool measurementEnabled_ = false;
    bool profileClassificationModeEnabled_ = false;
    ProfileClassificationSelectionMode profileClassificationSelectionMode_ = ProfileClassificationSelectionMode::Rectangle;
    bool profileClassificationTaskActive_ = false;
    bool profileClassificationSelectionActive_ = false;
    MeasurementResult measurementResult_;
    bool hoveredPointValid_ = false;
    PointRecord hoveredPoint_;
    QList<TowerRecord> towerMarkers_;
    QList<InspectionIssue> inspectionIssues_;
    QList<PointRecord> inspectionRouteWaypoints_;
    QStringList inspectionRouteLabels_;
    QList<PointRecord> inspectionRoutePartPoints_;
    QStringList inspectionRoutePartLabels_;
    QList<int> inspectionRoutePartPointIndices_;
    QList<PointRecord> inspectionRouteWaypointTargetPoints_;
    QList<bool> inspectionRouteWaypointHasTargetPoints_;
    QList<double> inspectionRouteWaypointAircraftYawDegs_;
    QList<double> inspectionRouteWaypointGimbalPitchDegs_;
    QList<double> inspectionRouteWaypointCameraYawDegs_;
    QList<double> inspectionRouteWaypointCameraPitchDegs_;
    QStringList inspectionRouteWaypointTargetLabels_;
    QList<QList<PointRecord>> inspectionRouteWaypointAllTargetPoints_;
    QList<QList<int>> inspectionRouteWaypointAllTargetPartIndices_;
    QList<QList<double>> inspectionRouteWaypointAllCameraYawDegs_;
    QList<QList<double>> inspectionRouteWaypointAllCameraPitchDegs_;
    QList<QStringList> inspectionRouteWaypointAllTargetLabels_;
    QSet<int> hiddenInspectionIssueIndices_;
    bool inspectionRouteVisible_ = true;
    bool inspectionRouteEditingEnabled_ = false;
    QTimer* routeRoamTimer_ = nullptr;
    enum class InspectionRouteRoamPlaybackState
    {
        Stopped = 0,
        Playing,
        Paused
    };
    InspectionRouteRoamPlaybackState inspectionRouteRoamPlaybackState_ = InspectionRouteRoamPlaybackState::Stopped;
    RouteRoamViewMode inspectionRouteRoamViewMode_ = RouteRoamViewMode::ThirdPerson;
    double inspectionRouteRoamSpeedMetersPerSecond_ = 2.0;
    int inspectionRouteRoamCurrentSegmentIndex_ = 0;
    double inspectionRouteRoamSegmentProgressMeters_ = 0.0;
    bool inspectionRouteRoamDwelling_ = false;
    double inspectionRouteRoamDwellRemainingSeconds_ = 0.0;
    bool inspectionRouteRoamCurrentPositionValid_ = false;
    osg::Vec3d inspectionRouteRoamCurrentPosition_;
    bool inspectionRouteRoamThirdPersonFollowInitialized_ = false;
    osg::Vec3d inspectionRouteRoamLastFollowPosition_;
    int inspectionRouteRoamLastCaptureWaypointIndex_ = -1;
    int inspectionRouteRoamCaptureCount_ = 0;
    double inspectionRouteRoamCaptureFlashRemainingSeconds_ = 0.0;
    std::chrono::steady_clock::time_point inspectionRouteRoamLastUpdateTime_ {};
    bool inspectionRouteRoamManualViewCaptured_ = false;
    osg::Vec3d inspectionRouteRoamSavedEye_;
    osg::Vec3d inspectionRouteRoamSavedCenter_;
    osg::Vec3d inspectionRouteRoamSavedUp_;
    osg::ref_ptr<osgGA::CameraManipulator> inspectionRouteRoamSavedManipulator_;
    RouteLabelDisplayMode routeWaypointLabelDisplayMode_ = RouteLabelDisplayMode::Name;
    RouteLabelDisplayMode routePartLabelDisplayMode_ = RouteLabelDisplayMode::Name;
    QColor inspectionRouteWaypointColor_ = QColor(38, 189, 245);
    QColor inspectionRoutePartPointColor_ = QColor(245, 115, 31);
    QColor inspectionRouteTrajectoryColor_ = QColor(41, 209, 242);
    int selectedTowerIndex_ = -1;
    int selectedIssueIndex_ = -1;
    int selectedInspectionRouteWaypointIndex_ = -1;
    int selectedInspectionRouteWaypointTargetIndex_ = -1;
    TowerEditMode towerEditMode_ = TowerEditMode::None;
    int towerEditTargetIndex_ = -1;
    int towerAddModeStartCount_ = 0;
    IssueEditMode issueEditMode_ = IssueEditMode::None;
    QSet<int> profileClassificationSourceClasses_;
    int profileClassificationTargetClass_ = 16;
    QRectF profileClassificationSelectionRect_;
    QPolygonF profileClassificationPolygonPoints_;
    QPointF profileClassificationPolygonPreviewPoint_;
    bool profileClassificationPolygonPreviewActive_ = false;
    ClassificationEditStore classificationEditStore_;
    QList<ClassificationEditBatch> classificationUndoStack_;
    QList<ClassificationEditBatch> classificationRedoStack_;
    QList<QLabel*> towerOverlayLabels_;
    QList<QLabel*> issueOverlayLabels_;
    QList<QLabel*> inspectionRouteOverlayLabels_;
    QList<QLabel*> inspectionRoutePartOverlayLabels_;
    bool routeWaypointDragActive_ = false;
    int routeWaypointDragIndex_ = -1;
    QPointF routeWaypointDragAnchor_;
    PointRecord routeWaypointDragPreviewPoint_;
    bool routeWaypointDragPreviewValid_ = false;
    QPointF lastHoverQueryPosition_;
    std::chrono::steady_clock::time_point lastHoverQueryTime_{};
    PointCloudTileSet previewTileSet_;
    PointCloudTileSet fullTileSet_;
    QSet<PointCloudTileId> promotedFullResolutionTiles_;
    bool tiledPointCloudModeActive_ = false;
    bool fullResolutionTilesReady_ = false;
    bool frameCameraStateValid_ = false;
    bool cameraMoving_ = false;
    osg::Matrixd lastCameraViewMatrix_;
    std::uint64_t asyncLoadToken_ = 0;
    std::atomic_bool asyncLoadCancellationRequested_ = false;
    std::thread asyncLoadThread_;
    std::uint64_t classificationTaskToken_ = 0;
    std::chrono::steady_clock::time_point classificationTaskStartTime_{};
    std::atomic<std::uint64_t> classificationTaskScannedPoints_ { 0 };
    std::uint64_t lastClassificationTaskScannedPoints_ = 0;
    std::uint64_t lastClassificationTaskElapsedMilliseconds_ = 0;
    std::thread classificationTaskThread_;
    int nextDatasetId_ = 1;
    QTimer* classificationTaskStatusTimer_ = nullptr;
    QTimer* refineIdleTimer_ = nullptr;
    QTimer* refineBatchTimer_ = nullptr;

    osg::ref_ptr<osg::Group> rootGroup_;
    osg::ref_ptr<osg::Node> pointCloudNode_;
    osg::ref_ptr<osg::Node> towerMarkersNode_;
    osg::ref_ptr<osg::Node> inspectionIssuesNode_;
    osg::ref_ptr<osg::Node> inspectionRouteNode_;
    osg::ref_ptr<osg::Node> measurementOverlayNode_;
    std::unique_ptr<OsgPointCloudNode> tiledPointCloudNode_;
};
