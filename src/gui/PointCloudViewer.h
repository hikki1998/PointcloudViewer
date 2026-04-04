#pragma once

#include <chrono>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include <QColor>
#include <QList>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QGridLayout>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include <osg/ref_ptr>
#include <osg/Matrix>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/Viewer>

#include "domain/DataManager.h"
#include "domain/InspectionData.h"
#include "osg/PointCloudVisualization.h"
#include "pointcloud/PointCloudData.h"
#include "pointcloud/PointCloudTileStore.h"

class QLabel;
class QEvent;
class QKeyEvent;
class QMouseEvent;
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

struct InteractionOptions
{
    bool invertOrbitDrag = false;
    bool invertPanDrag = false;
    bool invertWheelZoom = false;
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

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void leaveEvent(QEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

signals:
    void sceneClicked(const QPointF& localPos);
    void sceneSecondaryClicked(const QPointF& localPos);
    void sceneHovered(const QPointF& localPos);
    void sceneHoverEnded();
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
    bool leftButtonPressed_ = false;
    bool middleButtonPressed_ = false;
    bool rightButtonPressed_ = false;
    bool leftButtonDragDetected_ = false;
    bool leftButtonEventDispatched_ = false;
    bool rightButtonDragDetected_ = false;
    QPointF leftButtonAnchor_;
    QPointF middleButtonAnchor_;
    QPointF rightButtonAnchor_;
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
    const MeasurementResult& measurementResult() const;
    bool hasHoveredPoint() const;
    PointRecord hoveredPoint() const;
    const QList<TowerRecord>& towerMarkers() const;
    const QList<InspectionIssue>& inspectionIssues() const;
    const QList<PointRecord>& inspectionRouteWaypoints() const;
    int selectedTowerIndex() const;
    int selectedIssueIndex() const;
    int selectedInspectionRouteWaypointIndex() const;
    TowerEditMode towerEditMode() const;
    int towerEditTargetIndex() const;
    IssueEditMode issueEditMode() const;
    bool focusOnPoint(const PointRecord& point, double distanceScale = 0.35);
    bool focusOnBounds(const PointRecord& minBounds, const PointRecord& maxBounds, double distanceScale = 1.0);
    bool setPointCloudDatasetVisible(const QString& filePath, bool visible);
    bool isInspectionIssueVisible(int index) const;
    void setInspectionIssueVisible(int index, bool visible);
    bool inspectionRouteVisible() const;
    void setInspectionRouteVisible(bool visible);

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
    void setMeasurementEnabled(bool enabled);
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
    void setInspectionRouteWaypoints(const QList<PointRecord>& waypoints, const QStringList& labels = QStringList());
    void clearInspectionRouteWaypoints();
    void setSelectedInspectionRouteWaypointIndex(int index);
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

private:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void createStatusPanel();
    void createMeasurementOverlayWidgets();
    void createWelcomeOverlay();
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
    void updateFooter();
    void updateMessage(const QString& title, const QString& detail);
    void applyClearColor();
    void applyViewPreset(PointCloudViewPreset viewPreset);
    void handleSceneClick(const QPointF& localPos);
    void handleSceneSecondaryClick(const QPointF& localPos);
    void handleSceneHover(const QPointF& localPos);
    void clearHoveredPoint();
    void updateHoveredPoint(const PointRecord* hoveredPoint);
    bool pickPointAtScreenPosition(const QPointF& localPos, PointRecord* pickedPoint, float tolerancePixels = 14.0f) const;
    int pickTowerMarkerAtScreenPosition(const QPointF& localPos, float tolerancePixels = 18.0f) const;
    int pickInspectionIssueAtScreenPosition(const QPointF& localPos, float tolerancePixels = 18.0f) const;
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
    void updateSceneClickCapture();
    void updateAxisIndicator();
    void positionAxisIndicator();
    void positionOverlayLabel(QLabel* label, const QPointF& anchor, const QPoint& offset) const;
    void setLoadingState(bool active, const QString& title, const QString& detail, int progressPercent);
    void updateWelcomeOverlayVisibility();
    void syncCurrentFilePath();
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
    OsgWidget* osgWidget_ = nullptr;
    QWidget* statusPanel_ = nullptr;

    std::shared_ptr<PointCloudData> currentPointCloud_;
    std::shared_ptr<PointCloudData> previewPointCloud_;
    mutable std::shared_ptr<PointCloudData> fullResolutionPointCloudCache_;
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
    MeasurementResult measurementResult_;
    bool hoveredPointValid_ = false;
    PointRecord hoveredPoint_;
    QList<TowerRecord> towerMarkers_;
    QList<InspectionIssue> inspectionIssues_;
    QList<PointRecord> inspectionRouteWaypoints_;
    QStringList inspectionRouteLabels_;
    QSet<int> hiddenInspectionIssueIndices_;
    bool inspectionRouteVisible_ = true;
    int selectedTowerIndex_ = -1;
    int selectedIssueIndex_ = -1;
    int selectedInspectionRouteWaypointIndex_ = -1;
    TowerEditMode towerEditMode_ = TowerEditMode::None;
    int towerEditTargetIndex_ = -1;
    int towerAddModeStartCount_ = 0;
    IssueEditMode issueEditMode_ = IssueEditMode::None;
    QList<QLabel*> towerOverlayLabels_;
    QList<QLabel*> issueOverlayLabels_;
    QList<QLabel*> inspectionRouteOverlayLabels_;
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
