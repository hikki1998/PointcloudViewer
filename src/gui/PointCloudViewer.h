#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
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
    bool hasStartPoint = false;
    bool hasEndPoint = false;
    PointRecord startPoint;
    PointRecord endPoint;
    float distance3d = 0.0f;
    float deltaZ = 0.0f;

    [[nodiscard]] bool isComplete() const
    {
        return hasStartPoint && hasEndPoint;
    }
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
    void setMeasurementModeEnabled(bool enabled);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

signals:
    void sceneClicked(const QPointF& localPos);
    void frameRendered();

private:
    osgGA::EventQueue* eventQueue() const;
    void updateViewport(int width, int height);
    int mapMouseButton(Qt::MouseButton button) const;
    static float toDevicePixels(float value, float devicePixelRatio);

    osg::ref_ptr<osgViewer::Viewer> viewer_;
    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> graphicsWindow_;
    bool initialized_ = false;
    InteractionOptions interactionOptions_;
    bool measurementModeEnabled_ = false;
    bool leftButtonPressed_ = false;
    bool middleButtonPressed_ = false;
    bool rightButtonPressed_ = false;
    bool leftButtonDragDetected_ = false;
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
    void clearPointCloud();

    bool hasPointCloud() const;
    QString currentFilePath() const;
    const PointCloudData* pointCloudData() const;
    const PointCloudVisualizationOptions& visualizationOptions() const;
    const InteractionOptions& interactionOptions() const;
    bool measurementEnabled() const;
    const MeasurementResult& measurementResult() const;

public slots:
    void setPointSize(int pointSize);
    void setColorMode(int colorModeIndex);
    void setColorMode(PointCloudColorMode colorMode);
    void setSingleColor(const QColor& color);
    void setBackgroundColor(const QColor& color);
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

signals:
    void pointCloudLoaded();
    void pointCloudCleared();
    void visualizationOptionsChanged();
    void interactionOptionsChanged();
    void measurementChanged();
    void measurementModeChanged();
    void measurementMessage(const QString& message, bool error);

private:
    void changeEvent(QEvent* event) override;
    void createStatusPanel();
    void createMeasurementOverlayWidgets();
    void rebuildScene();
    void updateFooter();
    void updateMessage(const QString& title, const QString& detail);
    void applyClearColor();
    void applyViewPreset(PointCloudViewPreset viewPreset);
    void handleSceneClick(const QPointF& localPos);
    bool pickPointAtScreenPosition(const QPointF& localPos, PointRecord* pickedPoint) const;
    osg::ref_ptr<osg::Node> buildMeasurementOverlay() const;
    QPointF projectPointToViewport(const PointRecord& point, bool* visible) const;
    void updateMeasurementOverlayWidgets();
    void positionOverlayLabel(QLabel* label, const QPointF& anchor, const QPoint& offset) const;
    void resetMeasurementState(bool notifyChange = true);
    void retranslateUi();

    QGridLayout* layout_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* detailLabel_ = nullptr;
    QLabel* measurementStartOverlayLabel_ = nullptr;
    QLabel* measurementEndOverlayLabel_ = nullptr;
    QLabel* measurementSummaryOverlayLabel_ = nullptr;
    OsgWidget* osgWidget_ = nullptr;
    QWidget* statusPanel_ = nullptr;

    PointCloudData currentPointCloud_;
    QString currentFilePath_;
    PointCloudVisualizationOptions visualizationOptions_;
    InteractionOptions interactionOptions_;
    bool measurementEnabled_ = false;
    MeasurementResult measurementResult_;

    osg::ref_ptr<osg::Group> rootGroup_;
    osg::ref_ptr<osg::Node> pointCloudNode_;
    osg::ref_ptr<osg::Node> measurementOverlayNode_;
};
