#pragma once

#include <memory>

#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPointF>
#include <QRectF>

#include <osg/ref_ptr>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/Viewer>

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class GaussianRenderer;
struct GaussianModel;

namespace osgGA
{
class EventQueue;
}

struct InteractionOptions
{
    bool invertOrbitDrag = false;
    bool invertPanDrag = false;
    bool invertWheelZoom = false;
    int wheelZoomSensitivityPercent = 100;
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
    bool setGaussianModel(std::shared_ptr<const GaussianModel> model, QString* errorMessage = nullptr);
    void clearGaussianModel();
    bool hasGaussianModel() const;

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
    bool setGaussianOrbitCenterAt(const QPointF& localPos);
    static float toDevicePixels(float value, float devicePixelRatio);

    osg::ref_ptr<osgViewer::Viewer> viewer_;
    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> graphicsWindow_;
    std::shared_ptr<const GaussianModel> gaussianModel_;
    std::unique_ptr<GaussianRenderer> gaussianRenderer_;
    bool initialized_ = false;
    InteractionOptions interactionOptions_;
    bool sceneClickModeEnabled_ = false;
    bool sceneDragCaptureEnabled_ = false;
    bool leftButtonPressed_ = false;
    bool middleButtonPressed_ = false;
    bool rightButtonPressed_ = false;
    bool gaussianInteractionActive_ = false;
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
