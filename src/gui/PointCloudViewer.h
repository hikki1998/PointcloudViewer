#pragma once

#include <chrono>

#include <QColor>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QGridLayout>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include <osg/ref_ptr>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/Viewer>

#include "osg/PointCloudVisualization.h"
#include "pointcloud/PointCloudData.h"

class QLabel;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QWheelEvent;

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

struct TowerMarker
{
    QString name;
    PointRecord point;
};

enum class TowerEditMode
{
    None = 0,
    AddAfterLast,
    InsertBeforeSelected,
    MoveSelected
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
    bool appendPointCloudFiles(const QStringList& filePaths, QString* errorMessage = nullptr);
    void clearPointCloud();

    bool hasPointCloud() const;
    QString currentFilePath() const;
    QStringList currentFilePaths() const;
    const PointCloudData* pointCloudData() const;
    const PointCloudVisualizationOptions& visualizationOptions() const;
    const InteractionOptions& interactionOptions() const;
    bool measurementEnabled() const;
    const MeasurementResult& measurementResult() const;
    bool hasHoveredPoint() const;
    PointRecord hoveredPoint() const;
    const QList<TowerMarker>& towerMarkers() const;
    int selectedTowerIndex() const;
    TowerEditMode towerEditMode() const;
    int towerEditTargetIndex() const;
    bool focusOnPoint(const PointRecord& point, double distanceScale = 0.35);

public slots:
    void setPointSize(int pointSize);
    void setPointOpacity(int opacityPercent);
    void setColorMode(int colorModeIndex);
    void setColorMode(PointCloudColorMode colorMode);
    void setSingleColor(const QColor& color);
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
    void setTowerMarkers(const QList<TowerMarker>& towerMarkers);
    bool setTowerMarkerName(int index, const QString& name);
    void setSelectedTowerIndex(int index);
    bool removeTowerMarker(int index);
    bool moveTowerMarker(int index, const PointRecord& point);
    void clearTowerMarkers();
    void beginTowerAddMode();
    void beginTowerInsertMode(int beforeIndex);
    void beginTowerMoveMode(int towerIndex);
    void cancelTowerEditMode();

signals:
    void pointCloudLoaded();
    void pointCloudCleared();
    void visualizationOptionsChanged();
    void interactionOptionsChanged();
    void measurementChanged();
    void measurementModeChanged();
    void measurementMessage(const QString& message, bool error);
    void towerMarkersChanged();
    void selectedTowerChanged(int index);
    void towerEditModeChanged();
    void towerEditRequested(const PointRecord& point, int mode, int targetIndex);

private:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void createStatusPanel();
    void createMeasurementOverlayWidgets();
    void rebuildScene();
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
    osg::ref_ptr<osg::Node> buildMeasurementOverlay() const;
    osg::ref_ptr<osg::Node> buildTowerMarkersOverlay() const;
    QPointF projectPointToViewport(const PointRecord& point, bool* visible) const;
    void refreshMeasurementOverlay();
    void refreshTowerMarkersOverlay();
    void updateMeasurementOverlayWidgets();
    void updateTowerOverlayWidgets();
    void updateSceneClickCapture();
    void updateAxisIndicator();
    void positionAxisIndicator();
    void positionOverlayLabel(QLabel* label, const QPointF& anchor, const QPoint& offset) const;
    void recalculateMeasurementResult();
    bool undoLastMeasurementPoint();
    void resetMeasurementState(bool notifyChange = true);
    void retranslateUi();

    QGridLayout* layout_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* detailLabel_ = nullptr;
    QLabel* cursorLabel_ = nullptr;
    QLabel* measurementStartOverlayLabel_ = nullptr;
    QLabel* measurementEndOverlayLabel_ = nullptr;
    QLabel* measurementSummaryOverlayLabel_ = nullptr;
    QWidget* axisIndicatorOverlay_ = nullptr;
    OsgWidget* osgWidget_ = nullptr;
    QWidget* statusPanel_ = nullptr;

    PointCloudData currentPointCloud_;
    QString currentFilePath_;
    QStringList currentFilePaths_;
    PointCloudVisualizationOptions visualizationOptions_;
    InteractionOptions interactionOptions_;
    bool measurementEnabled_ = false;
    MeasurementResult measurementResult_;
    bool hoveredPointValid_ = false;
    PointRecord hoveredPoint_;
    QList<TowerMarker> towerMarkers_;
    int selectedTowerIndex_ = -1;
    TowerEditMode towerEditMode_ = TowerEditMode::None;
    int towerEditTargetIndex_ = -1;
    QList<QLabel*> towerOverlayLabels_;
    QPointF lastHoverQueryPosition_;
    std::chrono::steady_clock::time_point lastHoverQueryTime_{};

    osg::ref_ptr<osg::Group> rootGroup_;
    osg::ref_ptr<osg::Node> pointCloudNode_;
    osg::ref_ptr<osg::Node> towerMarkersNode_;
    osg::ref_ptr<osg::Node> measurementOverlayNode_;
};
