#include "gui/OsgWidget.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QRubberBand>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include <osg/Camera>
#include <osg/State>
#include <osg/Viewport>
#include <osgGA/TrackballManipulator>

#include "gaussian/GaussianRenderer.h"

namespace
{
constexpr int kMinWheelZoomSensitivityPercent = 50;
constexpr int kMaxWheelZoomSensitivityPercent = 200;
constexpr double kDefaultWheelZoomFactor = 0.45;

int clampWheelZoomSensitivityPercent(int percent)
{
    return std::clamp(percent, kMinWheelZoomSensitivityPercent, kMaxWheelZoomSensitivityPercent);
}

double wheelZoomFactorForSensitivity(int percent)
{
    return kDefaultWheelZoomFactor * static_cast<double>(clampWheelZoomSensitivityPercent(percent)) / 100.0;
}

int wheelZoomStepMultiplierForSensitivity(int percent)
{
    const double scale = static_cast<double>(clampWheelZoomSensitivityPercent(percent)) / 100.0;
    return std::clamp(static_cast<int>(std::lround(scale * 2.0)), 1, 4);
}

class PointCloudTrackballManipulator final : public osgGA::TrackballManipulator
{
public:
    explicit PointCloudTrackballManipulator(int flags = DEFAULT_SETTINGS)
        : osgGA::TrackballManipulator(flags)
    {
    }

protected:
    bool handleMousePush(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us) override
    {
        if (ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON
            || ea.getButton() == osgGA::GUIEventAdapter::MIDDLE_MOUSE_BUTTON
            || ea.getButton() == osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON) {
            setCenterByMousePointerIntersection(ea, us);
        }
        return osgGA::TrackballManipulator::handleMousePush(ea, us);
    }

    bool handleMouseWheel(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& us) override
    {
        setCenterByMousePointerIntersection(ea, us);
        return osgGA::TrackballManipulator::handleMouseWheel(ea, us);
    }
};

}

OsgWidget::OsgWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    viewer_ = new osgViewer::Viewer();
    viewer_->setThreadingModel(osgViewer::Viewer::SingleThreaded);
    viewer_->setReleaseContextAtEndOfFrameHint(false);
    viewer_->setKeyEventSetsDone(0);

    const int manipulatorFlags =
        osgGA::StandardManipulator::DEFAULT_SETTINGS
        | osgGA::StandardManipulator::SET_CENTER_ON_WHEEL_FORWARD_MOVEMENT;
    auto* manipulator = new PointCloudTrackballManipulator(manipulatorFlags);
    manipulator->setAllowThrow(false);
    manipulator->setAnimationTime(0.0);
    manipulator->setVerticalAxisFixed(true);
    manipulator->setTrackballSize(1.0);
    manipulator->setWheelZoomFactor(wheelZoomFactorForSensitivity(interactionOptions_.wheelZoomSensitivityPercent));
    manipulator->setMinimumDistance(0.0, false);
    viewer_->setCameraManipulator(manipulator);
    viewer_->getCamera()->setNearFarRatio(0.000001);
    viewer_->getCamera()->setSmallFeatureCullingPixelSize(-1.0f);

    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void OsgWidget::setInteractionOptions(const InteractionOptions& options)
{
    interactionOptions_ = options;
    if (auto* manipulator = dynamic_cast<PointCloudTrackballManipulator*>(viewer_->getCameraManipulator())) {
        manipulator->setWheelZoomFactor(wheelZoomFactorForSensitivity(interactionOptions_.wheelZoomSensitivityPercent));
    }
}

void OsgWidget::setSceneClickModeEnabled(bool enabled)
{
    sceneClickModeEnabled_ = enabled;
    sceneDragCaptureEnabled_ = false;
    leftButtonPressed_ = false;
    leftButtonDragDetected_ = false;
    leftButtonEventDispatched_ = false;
    rightButtonPressed_ = false;
    rightButtonDragDetected_ = false;
}

void OsgWidget::setRectangleSelectionEnabled(bool enabled)
{
    rectangleSelectionEnabled_ = enabled;
    selectionDragActive_ = false;
    leftButtonPressed_ = false;
    leftButtonDragDetected_ = false;
    leftButtonEventDispatched_ = false;
    rightButtonPressed_ = false;
    rightButtonDragDetected_ = false;
}

void OsgWidget::setSceneDragCaptureEnabled(bool enabled)
{
    sceneDragCaptureEnabled_ = enabled;
}

OsgWidget::~OsgWidget()
{
    if (viewer_.valid()) {
        viewer_->setDone(true);
    }
    if (isValid()) {
        makeCurrent();
        gaussianRenderer_.reset();
        doneCurrent();
    }
}

void OsgWidget::initializeGL()
{
    initializeOpenGLFunctions();

    graphicsWindow_ = new osgViewer::GraphicsWindowEmbedded(0, 0, width(), height());
    viewer_->getCamera()->setGraphicsContext(graphicsWindow_.get());

    updateViewport(width(), height());
    gaussianRenderer_ = std::make_unique<GaussianRenderer>();
    QString ignoredError;
    if (gaussianRenderer_->initialize(&ignoredError) && gaussianModel_ != nullptr) {
        gaussianRenderer_->setModel(gaussianModel_, &ignoredError);
    }
    initialized_ = true;
}

void OsgWidget::resizeGL(int w, int h)
{
    if (viewer_.valid()) {
        updateViewport(w, h);
    }
}

void OsgWidget::paintGL()
{
    if (viewer_.valid() && initialized_) {
        viewer_->frame();
        if (gaussianRenderer_ != nullptr && gaussianRenderer_->hasModel()) {
            const osg::Matrixd osgView = viewer_->getCamera()->getViewMatrix();
            const osg::Matrixd osgProjection = viewer_->getCamera()->getProjectionMatrix();
            QMatrix4x4 view;
            QMatrix4x4 projection;
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 4; ++column) {
                    view(row, column) = static_cast<float>(osgView(column, row));
                    projection(row, column) = static_cast<float>(osgProjection(column, row));
                }
            }
            gaussianRenderer_->render(
                view,
                projection,
                std::max(1, static_cast<int>(width() * devicePixelRatioF())),
                std::max(1, static_cast<int>(height() * devicePixelRatioF())));
            if (osg::GraphicsContext* graphicsContext = viewer_->getCamera()->getGraphicsContext()) {
                if (osg::State* state = graphicsContext->getState()) {
                    state->dirtyAllModes();
                    state->dirtyAllAttributes();
                    state->dirtyAllVertexArrays();
                }
            }
        }
        emit frameRendered();
        if (gaussianRenderer_ != nullptr && gaussianRenderer_->hasModel() && gaussianRenderer_->needsRedraw()) {
            update();
        }
    }
}

bool OsgWidget::setGaussianModel(std::shared_ptr<const GaussianModel> model, QString* errorMessage)
{
    gaussianModel_ = std::move(model);
    if (auto* manipulator = dynamic_cast<PointCloudTrackballManipulator*>(viewer_->getCameraManipulator())) {
        manipulator->setVerticalAxisFixed(false);
    }
    if (!isValid()) {
        update();
        return true;
    }
    makeCurrent();
    if (gaussianRenderer_ == nullptr) {
        gaussianRenderer_ = std::make_unique<GaussianRenderer>();
    }
    const bool success = gaussianRenderer_->initialize(errorMessage)
        && gaussianRenderer_->setModel(gaussianModel_, errorMessage);
    doneCurrent();
    update();
    return success;
}

void OsgWidget::clearGaussianModel()
{
    gaussianModel_.reset();
    gaussianInteractionActive_ = false;
    if (auto* manipulator = dynamic_cast<PointCloudTrackballManipulator*>(viewer_->getCameraManipulator())) {
        manipulator->setVerticalAxisFixed(true);
    }
    if (gaussianRenderer_ != nullptr && isValid()) {
        makeCurrent();
        gaussianRenderer_->clear();
        doneCurrent();
    }
    update();
}

bool OsgWidget::hasGaussianModel() const
{
    return gaussianModel_ != nullptr && !gaussianModel_->empty();
}

void OsgWidget::leaveEvent(QEvent* event)
{
    QOpenGLWidget::leaveEvent(event);
    emit sceneHoverEnded();
}

void OsgWidget::mousePressEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }

    const bool selectionDragRequested =
        rectangleSelectionEnabled_
        && event->button() == Qt::LeftButton
        && (event->modifiers() & Qt::AltModifier) == 0;
    if (selectionDragRequested) {
        selectionDragActive_ = true;
        selectionAnchor_ = event->localPos();
        emit selectionRectangleChanged(QRectF(selectionAnchor_, selectionAnchor_).normalized(), true);
        update();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (gaussianModel_ != nullptr && !gaussianModel_->empty()) {
            setGaussianOrbitCenterAt(event->localPos());
        }
        leftButtonPressed_ = true;
        leftButtonAnchor_ = event->localPos();
        leftButtonDragDetected_ = false;
        leftButtonEventDispatched_ = !sceneClickModeEnabled_;
        lastOrbitCursorPosition_ = event->localPos();
        lastOrbitEventPosition_ = event->localPos();
        if (!sceneClickModeEnabled_ && gaussianRenderer_ != nullptr && gaussianRenderer_->hasModel()) {
            gaussianInteractionActive_ = true;
            gaussianRenderer_->setInteractionActive(true);
        }
    } else if (event->button() == Qt::MiddleButton) {
        middleButtonPressed_ = true;
        middleButtonAnchor_ = event->localPos();
        lastPanCursorPosition_ = event->localPos();
        lastPanEventPosition_ = event->localPos();
    } else if (event->button() == Qt::RightButton) {
        rightButtonPressed_ = true;
        rightButtonDragDetected_ = false;
        rightButtonAnchor_ = event->localPos();
        lastPanCursorPosition_ = event->localPos();
        lastPanEventPosition_ = event->localPos();
    }

    if (sceneClickModeEnabled_ && event->button() == Qt::LeftButton) {
        emit scenePressed(event->localPos());
        return;
    }

    dispatchMouseButtonEvent(event->localPos(), event->button(), true);

    update();
}

void OsgWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }

    if (sceneClickModeEnabled_ && event->button() == Qt::LeftButton) {
        leftButtonPressed_ = false;
        leftButtonDragDetected_ = true;
        leftButtonEventDispatched_ = false;
        emit sceneDoubleClicked(event->localPos());
        update();
        return;
    }

    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void OsgWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }

    bool needsRedraw = false;

    if (rectangleSelectionEnabled_ && event->button() == Qt::LeftButton && selectionDragActive_) {
        const QRectF selectionRect(selectionAnchor_, event->localPos());
        selectionDragActive_ = false;
        emit selectionRectangleChanged(selectionRect.normalized(), false);
        if (selectionRect.normalized().width() >= 4.0 && selectionRect.normalized().height() >= 4.0) {
            emit selectionRectangleFinished(selectionRect.normalized());
        }
        needsRedraw = true;
    } else if (sceneClickModeEnabled_ && event->button() == Qt::LeftButton) {
        if (sceneDragCaptureEnabled_) {
            emit sceneDragReleased(event->localPos());
            sceneDragCaptureEnabled_ = false;
        } else if (leftButtonEventDispatched_) {
            dispatchMouseButtonEvent(event->localPos(), event->button(), false);
            needsRedraw = true;
        } else if (!leftButtonDragDetected_) {
            emit sceneClicked(event->localPos());
        }
    } else {
        dispatchMouseButtonEvent(event->localPos(), event->button(), false);
        needsRedraw = true;
        if (sceneClickModeEnabled_ && event->button() == Qt::RightButton && !rightButtonDragDetected_) {
            emit sceneSecondaryClicked(event->localPos());
        }
    }

    if (event->button() == Qt::LeftButton) {
        leftButtonPressed_ = false;
        leftButtonDragDetected_ = false;
        leftButtonEventDispatched_ = false;
        if (gaussianInteractionActive_ && gaussianRenderer_ != nullptr) {
            gaussianInteractionActive_ = false;
            gaussianRenderer_->setInteractionActive(false);
            needsRedraw = true;
        }
    } else if (event->button() == Qt::MiddleButton) {
        middleButtonPressed_ = false;
    } else if (event->button() == Qt::RightButton) {
        rightButtonPressed_ = false;
        rightButtonDragDetected_ = false;
    }

    if (needsRedraw) {
        update();
    }
}

void OsgWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }

    bool needsRedraw = false;

    if (rectangleSelectionEnabled_ && selectionDragActive_) {
        emit selectionRectangleChanged(QRectF(selectionAnchor_, event->localPos()).normalized(), true);
        update();
        return;
    }

    if (sceneClickModeEnabled_ && leftButtonPressed_) {
        const QPointF delta = event->localPos() - leftButtonAnchor_;
        if (std::hypot(delta.x(), delta.y()) > 4.0) {
            leftButtonDragDetected_ = true;
        }
    }
    if (sceneClickModeEnabled_ && rightButtonPressed_) {
        const QPointF delta = event->localPos() - rightButtonAnchor_;
        if (std::hypot(delta.x(), delta.y()) > 4.0) {
            rightButtonDragDetected_ = true;
        }
    }

    QPointF adjustedPosition = event->localPos();
    if ((event->buttons() & (Qt::RightButton | Qt::MiddleButton)) != 0
        && (rightButtonPressed_ || middleButtonPressed_)) {
        QPointF delta = event->localPos() - lastPanCursorPosition_;
        // OSG trackball pan already flips both screen deltas internally.
        // Qt->OSG Y-axis mapping compensates Y, so only X needs correction here.
        delta.setX(-delta.x());
        if (interactionOptions_.invertPanDrag) {
            delta = QPointF(-delta.x(), -delta.y());
        }
        adjustedPosition = lastPanEventPosition_ + delta;
        lastPanCursorPosition_ = event->localPos();
        lastPanEventPosition_ = adjustedPosition;
    } else if ((event->buttons() & Qt::LeftButton) != 0 && leftButtonPressed_) {
        QPointF delta = event->localPos() - lastOrbitCursorPosition_;
        // Orbit trackball applies mirrored screen-space deltas internally.
        // Qt->OSG Y-axis mapping already compensates Y, so only X needs correction.
        delta.setX(-delta.x());
        if (interactionOptions_.invertOrbitDrag) {
            delta = QPointF(-delta.x(), -delta.y());
        }
        adjustedPosition = lastOrbitEventPosition_ + delta;
        lastOrbitCursorPosition_ = event->localPos();
        lastOrbitEventPosition_ = adjustedPosition;
    }

    if (sceneClickModeEnabled_ && leftButtonPressed_ && sceneDragCaptureEnabled_) {
        emit sceneDragged(event->localPos());
        return;
    }

    if (sceneClickModeEnabled_ && leftButtonPressed_ && leftButtonDragDetected_ && !leftButtonEventDispatched_) {
        dispatchMouseButtonEvent(leftButtonAnchor_, Qt::LeftButton, true);
        leftButtonEventDispatched_ = true;
        if (gaussianRenderer_ != nullptr && gaussianRenderer_->hasModel()) {
            gaussianInteractionActive_ = true;
            gaussianRenderer_->setInteractionActive(true);
        }
    }

    if (!sceneClickModeEnabled_
        || (event->buttons() & Qt::LeftButton) == 0
        || !leftButtonPressed_
        || leftButtonEventDispatched_) {
        dispatchMouseMotion(adjustedPosition);
        needsRedraw = true;
    }

    if (event->buttons() == Qt::NoButton) {
        emit sceneHovered(event->localPos());
    }

    if (needsRedraw) {
        update();
    }
}

bool OsgWidget::setGaussianOrbitCenterAt(const QPointF& localPos)
{
    if (gaussianModel_ == nullptr || gaussianModel_->empty() || viewer_ == nullptr
        || viewer_->getCamera() == nullptr || viewer_->getCamera()->getViewport() == nullptr) {
        return false;
    }

    auto* manipulator = dynamic_cast<osgGA::TrackballManipulator*>(viewer_->getCameraManipulator());
    if (manipulator == nullptr) {
        return false;
    }

    osg::Camera* camera = viewer_->getCamera();
    const osg::Matrixd localToWindow =
        camera->getViewMatrix()
        * camera->getProjectionMatrix()
        * camera->getViewport()->computeWindowMatrix();
    const double devicePixelRatio = devicePixelRatioF();
    const double clickX = localPos.x() * devicePixelRatio;
    const double clickY = (static_cast<double>(height()) - localPos.y()) * devicePixelRatio;
    constexpr double kPickTolerancePixels = 24.0;
    const double tolerance = kPickTolerancePixels * devicePixelRatio;
    const double toleranceSquared = tolerance * tolerance;

    bool found = false;
    double bestDistanceSquared = toleranceSquared;
    double bestDepth = std::numeric_limits<double>::max();
    osg::Vec3d bestCenter;
    for (const GaussianGpuRecord& splat : gaussianModel_->splats) {
        const osg::Vec3d projected = osg::Vec3d(
            splat.positionAlpha[0],
            splat.positionAlpha[1],
            splat.positionAlpha[2])
            * localToWindow;
        if (!std::isfinite(projected.x()) || !std::isfinite(projected.y()) || !std::isfinite(projected.z())
            || projected.z() < 0.0 || projected.z() > 1.0) {
            continue;
        }

        const double dx = projected.x() - clickX;
        const double dy = projected.y() - clickY;
        const double distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > bestDistanceSquared) {
            continue;
        }
        if (!found
            || distanceSquared < bestDistanceSquared - 0.001
            || (std::abs(distanceSquared - bestDistanceSquared) <= 0.001 && projected.z() < bestDepth)) {
            found = true;
            bestDistanceSquared = distanceSquared;
            bestDepth = projected.z();
            bestCenter.set(splat.positionAlpha[0], splat.positionAlpha[1], splat.positionAlpha[2]);
        }
    }

    if (!found) {
        return false;
    }

    osg::Vec3d eye;
    osg::Vec3d currentCenter;
    osg::Vec3d up;
    manipulator->getTransformation(eye, currentCenter, up);
    osg::Vec3d forward = currentCenter - eye;
    if (forward.length2() <= 1e-12) {
        return false;
    }
    forward.normalize();

    const double pivotDistance = (bestCenter - eye) * forward;
    if (!std::isfinite(pivotDistance) || pivotDistance <= 0.01) {
        return false;
    }

    // Keep the eye and viewing direction unchanged. An arbitrary off-axis center would make
    // OrbitManipulator move the camera immediately; using the picked depth on the view axis
    // changes the orbit radius without causing a click-time scene jump.
    manipulator->setTransformation(eye, eye + forward * pivotDistance, up);
    return true;
}

void OsgWidget::dispatchMouseButtonEvent(const QPointF& localPos, Qt::MouseButton button, bool pressed)
{
    if (eventQueue() == nullptr) {
        return;
    }

    const int mappedButton = mapMouseButton(button);
    if (mappedButton == 0) {
        return;
    }

    const float devicePixelRatio = static_cast<float>(devicePixelRatioF());
    const float x = toDevicePixels(static_cast<float>(localPos.x()), devicePixelRatio);
    const float y = toDevicePixels(static_cast<float>(height()) - static_cast<float>(localPos.y()), devicePixelRatio);
    if (pressed) {
        eventQueue()->mouseButtonPress(x, y, mappedButton);
    } else {
        eventQueue()->mouseButtonRelease(x, y, mappedButton);
    }
}

void OsgWidget::dispatchMouseMotion(const QPointF& localPos)
{
    if (eventQueue() == nullptr) {
        return;
    }

    const float devicePixelRatio = static_cast<float>(devicePixelRatioF());
    const float x = toDevicePixels(static_cast<float>(localPos.x()), devicePixelRatio);
    const float y = toDevicePixels(static_cast<float>(height()) - static_cast<float>(localPos.y()), devicePixelRatio);
    eventQueue()->mouseMotion(x, y);
}

void OsgWidget::wheelEvent(QWheelEvent* event)
{
    if (event == nullptr) {
        return;
    }

    if (eventQueue() != nullptr && event->angleDelta().y() != 0) {
        const bool scrollUp = event->angleDelta().y() > 0;
        const auto scrollMotion =
            (scrollUp ^ interactionOptions_.invertWheelZoom)
                ? osgGA::GUIEventAdapter::SCROLL_UP
                : osgGA::GUIEventAdapter::SCROLL_DOWN;
        const int baseScrollStepCount = std::max(1, std::abs(event->angleDelta().y()) / 120);
        const int scrollStepCount =
            baseScrollStepCount * wheelZoomStepMultiplierForSensitivity(interactionOptions_.wheelZoomSensitivityPercent);
        for (int stepIndex = 0; stepIndex < scrollStepCount; ++stepIndex) {
            eventQueue()->mouseScroll(scrollMotion);
        }
    }

    update();
}

void OsgWidget::keyPressEvent(QKeyEvent* event)
{
    if (rectangleSelectionEnabled_ && event != nullptr && event->key() == Qt::Key_Escape) {
        selectionDragActive_ = false;
        emit selectionRectangleChanged(QRectF(), false);
        emit selectionEscapePressed();
        update();
        return;
    }

    if (sceneClickModeEnabled_ && event != nullptr && event->key() == Qt::Key_Escape) {
        emit sceneEscapePressed();
        update();
        return;
    }

    if (eventQueue() != nullptr) {
        eventQueue()->keyPress(static_cast<osgGA::GUIEventAdapter::KeySymbol>(event->key()));
    }

    update();
}

void OsgWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (eventQueue() != nullptr) {
        eventQueue()->keyRelease(static_cast<osgGA::GUIEventAdapter::KeySymbol>(event->key()));
    }

    update();
}

osgGA::EventQueue* OsgWidget::eventQueue() const
{
    return graphicsWindow_.valid() ? graphicsWindow_->getEventQueue() : nullptr;
}

void OsgWidget::updateViewport(int width, int height)
{
    if (!viewer_.valid() || !graphicsWindow_.valid()) {
        return;
    }

    const float devicePixelRatio = static_cast<float>(devicePixelRatioF());
    const int framebufferWidth = std::max(1, static_cast<int>(toDevicePixels(static_cast<float>(width), devicePixelRatio)));
    const int framebufferHeight = std::max(1, static_cast<int>(toDevicePixels(static_cast<float>(height), devicePixelRatio)));

    graphicsWindow_->resized(0, 0, framebufferWidth, framebufferHeight);
    graphicsWindow_->getEventQueue()->windowResize(0, 0, framebufferWidth, framebufferHeight);

    viewer_->getCamera()->setViewport(new osg::Viewport(0, 0, framebufferWidth, framebufferHeight));
    viewer_->getCamera()->setProjectionMatrixAsPerspective(
        30.0,
        static_cast<double>(framebufferWidth) / static_cast<double>(framebufferHeight),
        0.01,
        1000000.0);
}

int OsgWidget::mapMouseButton(Qt::MouseButton button) const
{
    if (button == Qt::LeftButton) {
        return 1;
    }
    if (button == Qt::MiddleButton || button == Qt::RightButton) {
        return 2;
    }
    return 0;
}

float OsgWidget::toDevicePixels(float value, float devicePixelRatio)
{
    return value * devicePixelRatio;
}
