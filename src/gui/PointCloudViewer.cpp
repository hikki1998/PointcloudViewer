#include "gui/PointCloudViewer.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include <osg/Array>
#include <osg/Camera>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/LineWidth>
#include <osg/Matrix>
#include <osg/Point>
#include <osg/StateSet>
#include <osg/Vec4>
#include <osg/Viewport>
#include <osgGA/CameraManipulator>
#include <osgGA/TrackballManipulator>

#include "osg/OsgPointCloudNode.h"
#include "pointcloud/LasReader.h"
#include "pointcloud/PointCloudData.h"

namespace
{
constexpr int kMeasurementOverlayRenderBin = 100;
constexpr int kAxisIndicatorSize = 112;
constexpr float kHoverPickTolerancePixels = 12.0f;

class AxisIndicatorOverlay final : public QWidget
{
public:
    explicit AxisIndicatorOverlay(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setFixedSize(kAxisIndicatorSize, kAxisIndicatorSize);
    }

    void setAxisDirections(const std::array<QPointF, 3>& axisDirections)
    {
        axisDirections_ = axisDirections;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        painter.setBrush(QColor(15, 23, 42, 170));
        painter.setPen(QPen(QColor(148, 163, 184, 160), 1.0));
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 14.0, 14.0);

        const QPointF center(width() * 0.5, height() * 0.5);
        painter.setBrush(QColor(248, 250, 252));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(center, 3.5, 3.5);

        const struct AxisStyle {
            QColor color;
            QString label;
        } kAxisStyles[3] = {
            { QColor(239, 68, 68), QStringLiteral("X+") },
            { QColor(34, 197, 94), QStringLiteral("Y+") },
            { QColor(59, 130, 246), QStringLiteral("Z+") }
        };

        painter.setFont(QFont(QStringLiteral("Segoe UI"), 9, QFont::DemiBold));
        for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
            const QPointF endPoint = center + axisDirections_[axisIndex];
            painter.setPen(QPen(kAxisStyles[axisIndex].color, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawLine(center, endPoint);

            painter.setBrush(kAxisStyles[axisIndex].color);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(endPoint, 3.8, 3.8);

            const QPointF labelAnchor = endPoint + QPointF(endPoint.x() >= center.x() ? 8.0 : -24.0, endPoint.y() >= center.y() ? 16.0 : -8.0);
            painter.setPen(kAxisStyles[axisIndex].color.lighter(150));
            painter.drawText(QRectF(labelAnchor.x(), labelAnchor.y() - 10.0, 28.0, 20.0), Qt::AlignLeft | Qt::AlignVCenter, kAxisStyles[axisIndex].label);
        }
    }

private:
    std::array<QPointF, 3> axisDirections_ = {
        QPointF(28.0, 0.0),
        QPointF(0.0, -28.0),
        QPointF(20.0, -20.0)
    };
};

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

QString colorModeLabel(PointCloudColorMode colorMode)
{
    switch (colorMode) {
    case PointCloudColorMode::Elevation:
        return QCoreApplication::translate("PointCloudViewer", "Elevation Ramp");
    case PointCloudColorMode::SingleColor:
        return QCoreApplication::translate("PointCloudViewer", "Single Color");
    case PointCloudColorMode::Rgb:
    default:
        return QCoreApplication::translate("PointCloudViewer", "RGB");
    }
}

QString formatPointCount(std::size_t pointCount)
{
    return QLocale().toString(static_cast<qlonglong>(pointCount));
}

float clampUnit(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

int percentFromUnit(float value)
{
    return static_cast<int>(std::lround(clampUnit(value) * 100.0f));
}

QString formatCoordinate(float value)
{
    return QLocale().toString(static_cast<double>(value), 'f', 2);
}

QString formatTriplet(float x, float y, float z)
{
    return QStringLiteral("%1, %2, %3")
        .arg(formatCoordinate(x))
        .arg(formatCoordinate(y))
        .arg(formatCoordinate(z));
}

osg::Vec4 measurementColorPrimary()
{
    return osg::Vec4(1.0f, 0.78f, 0.20f, 1.0f);
}

osg::Vec4 measurementColorSecondary()
{
    return osg::Vec4(0.20f, 0.83f, 0.96f, 1.0f);
}

void applyMeasurementForegroundState(osg::StateSet* stateSet)
{
    if (stateSet == nullptr) {
        return;
    }

    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
    stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
    stateSet->setAttributeAndModes(new osg::Depth(osg::Depth::ALWAYS, 0.0, 1.0, false), osg::StateAttribute::ON);
    stateSet->setRenderBinDetails(kMeasurementOverlayRenderBin, "RenderBin");
}

osg::ref_ptr<osg::Geode> buildMeasurementMarkersGeode(const MeasurementResult& measurementResult)
{
    if (!measurementResult.hasStartPoint) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

    vertices->push_back(osg::Vec3(
        measurementResult.startPoint.x,
        measurementResult.startPoint.y,
        measurementResult.startPoint.z));
    colors->push_back(measurementColorPrimary());

    if (measurementResult.hasEndPoint) {
        vertices->push_back(osg::Vec3(
            measurementResult.endPoint.x,
            measurementResult.endPoint.y,
            measurementResult.endPoint.z));
        colors->push_back(measurementColorSecondary());
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());

    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::Point(10.0f), osg::StateAttribute::ON);
    applyMeasurementForegroundState(stateSet);

    return geode;
}

osg::ref_ptr<osg::Geode> buildMeasurementLineGeode(const MeasurementResult& measurementResult)
{
    if (!measurementResult.isComplete()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

    vertices->push_back(osg::Vec3(
        measurementResult.startPoint.x,
        measurementResult.startPoint.y,
        measurementResult.startPoint.z));
    vertices->push_back(osg::Vec3(
        measurementResult.endPoint.x,
        measurementResult.endPoint.y,
        measurementResult.endPoint.z));

    colors->push_back(measurementColorPrimary());
    colors->push_back(measurementColorSecondary());

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());

    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::LineWidth(3.0f), osg::StateAttribute::ON);
    applyMeasurementForegroundState(stateSet);

    return geode;
}

osg::ref_ptr<osg::Geode> buildTowerMarkersGeode(const QList<TowerMarker>& towerMarkers)
{
    if (towerMarkers.isEmpty()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

    for (const TowerMarker& towerMarker : towerMarkers) {
        vertices->push_back(osg::Vec3(towerMarker.point.x, towerMarker.point.y, towerMarker.point.z));
        colors->push_back(osg::Vec4(0.99f, 0.43f, 0.12f, 1.0f));
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());

    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::Point(12.0f), osg::StateAttribute::ON);
    applyMeasurementForegroundState(stateSet);

    return geode;
}
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
    manipulator->setWheelZoomFactor(0.45);
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
}

void OsgWidget::setSceneClickModeEnabled(bool enabled)
{
    sceneClickModeEnabled_ = enabled;
    leftButtonPressed_ = false;
    leftButtonDragDetected_ = false;
    leftButtonEventDispatched_ = false;
}

OsgWidget::~OsgWidget()
{
    if (viewer_.valid()) {
        viewer_->setDone(true);
    }
}

void OsgWidget::initializeGL()
{
    initializeOpenGLFunctions();

    graphicsWindow_ = new osgViewer::GraphicsWindowEmbedded(0, 0, width(), height());
    viewer_->getCamera()->setGraphicsContext(graphicsWindow_.get());

    updateViewport(width(), height());
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
        emit frameRendered();
    }
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

    if (event->button() == Qt::LeftButton) {
        leftButtonPressed_ = true;
        leftButtonAnchor_ = event->localPos();
        leftButtonDragDetected_ = false;
        leftButtonEventDispatched_ = !sceneClickModeEnabled_;
        lastOrbitCursorPosition_ = event->localPos();
        lastOrbitEventPosition_ = event->localPos();
    } else if (event->button() == Qt::MiddleButton) {
        middleButtonPressed_ = true;
        middleButtonAnchor_ = event->localPos();
        lastPanCursorPosition_ = event->localPos();
        lastPanEventPosition_ = event->localPos();
    } else if (event->button() == Qt::RightButton) {
        rightButtonPressed_ = true;
        rightButtonAnchor_ = event->localPos();
        lastPanCursorPosition_ = event->localPos();
        lastPanEventPosition_ = event->localPos();
    }

    if (sceneClickModeEnabled_ && event->button() == Qt::LeftButton) {
        update();
        return;
    }

    dispatchMouseButtonEvent(event->localPos(), event->button(), true);

    update();
}

void OsgWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }

    if (sceneClickModeEnabled_ && event->button() == Qt::LeftButton) {
        if (leftButtonEventDispatched_) {
            dispatchMouseButtonEvent(event->localPos(), event->button(), false);
        } else if (!leftButtonDragDetected_) {
            emit sceneClicked(event->localPos());
        }
    } else {
        dispatchMouseButtonEvent(event->localPos(), event->button(), false);
    }

    if (event->button() == Qt::LeftButton) {
        leftButtonPressed_ = false;
        leftButtonDragDetected_ = false;
        leftButtonEventDispatched_ = false;
    } else if (event->button() == Qt::MiddleButton) {
        middleButtonPressed_ = false;
    } else if (event->button() == Qt::RightButton) {
        rightButtonPressed_ = false;
    }

    update();
}

void OsgWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }

    if (sceneClickModeEnabled_ && leftButtonPressed_) {
        const QPointF delta = event->localPos() - leftButtonAnchor_;
        if (std::hypot(delta.x(), delta.y()) > 4.0) {
            leftButtonDragDetected_ = true;
        }
    }

    QPointF adjustedPosition = event->localPos();
    if ((event->buttons() & (Qt::RightButton | Qt::MiddleButton)) != 0
        && (rightButtonPressed_ || middleButtonPressed_)) {
        QPointF delta = event->localPos() - lastPanCursorPosition_;
        if (interactionOptions_.invertPanDrag) {
            delta = QPointF(-delta.x(), -delta.y());
        }
        adjustedPosition = lastPanEventPosition_ + delta;
        lastPanCursorPosition_ = event->localPos();
        lastPanEventPosition_ = adjustedPosition;
    } else if ((event->buttons() & Qt::LeftButton) != 0 && leftButtonPressed_) {
        QPointF delta = event->localPos() - lastOrbitCursorPosition_;
        if (interactionOptions_.invertOrbitDrag) {
            delta = QPointF(-delta.x(), -delta.y());
        }
        adjustedPosition = lastOrbitEventPosition_ + delta;
        lastOrbitCursorPosition_ = event->localPos();
        lastOrbitEventPosition_ = adjustedPosition;
    }

    if (sceneClickModeEnabled_ && leftButtonPressed_ && leftButtonDragDetected_ && !leftButtonEventDispatched_) {
        dispatchMouseButtonEvent(leftButtonAnchor_, Qt::LeftButton, true);
        leftButtonEventDispatched_ = true;
    }

    if (!sceneClickModeEnabled_
        || (event->buttons() & Qt::LeftButton) == 0
        || !leftButtonPressed_
        || leftButtonEventDispatched_) {
        dispatchMouseMotion(adjustedPosition);
    }

    if (event->buttons() == Qt::NoButton) {
        emit sceneHovered(event->localPos());
    }

    update();
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
        const int scrollStepCount = std::max(1, std::abs(event->angleDelta().y()) / 120) * 2;
        for (int stepIndex = 0; stepIndex < scrollStepCount; ++stepIndex) {
            eventQueue()->mouseScroll(scrollMotion);
        }
    }

    update();
}

void OsgWidget::keyPressEvent(QKeyEvent* event)
{
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

PointCloudViewer::PointCloudViewer(QWidget* parent)
    : QWidget(parent)
{
    layout_ = new QGridLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);

    osgWidget_ = new OsgWidget(this);
    createStatusPanel();
    createMeasurementOverlayWidgets();
    axisIndicatorOverlay_ = new AxisIndicatorOverlay(osgWidget_);
    axisIndicatorOverlay_->show();
    axisIndicatorOverlay_->raise();

    layout_->addWidget(osgWidget_, 0, 0);
    layout_->addWidget(statusPanel_, 1, 0);
    layout_->setRowStretch(0, 1);

    rootGroup_ = new osg::Group();
    if (osgViewer::Viewer* viewer = osgWidget_->getViewer()) {
        viewer->setSceneData(rootGroup_.get());
    }

    setStyleSheet(
        "QFrame#viewerStatusPanel {"
        "background-color: rgba(14, 20, 28, 220);"
        "border-top: 1px solid rgba(255, 255, 255, 28);"
        "}"
        "QLabel#viewerTitleLabel {"
        "color: #f3f6fb;"
        "font-size: 14px;"
        "font-weight: 600;"
        "}"
        "QLabel#viewerDetailLabel {"
        "color: #aeb8c7;"
        "font-size: 11px;"
        "}"
        "QLabel#viewerCursorLabel {"
        "color: #dbe4ef;"
        "font-size: 11px;"
        "}");

    connect(osgWidget_, &OsgWidget::sceneClicked, this, &PointCloudViewer::handleSceneClick);
    connect(osgWidget_, &OsgWidget::sceneHovered, this, &PointCloudViewer::handleSceneHover);
    connect(osgWidget_, &OsgWidget::sceneHoverEnded, this, &PointCloudViewer::clearHoveredPoint);
    connect(osgWidget_, &OsgWidget::frameRendered, this, [this]() {
        updateMeasurementOverlayWidgets();
        updateTowerOverlayWidgets();
        updateAxisIndicator();
    });

    applyClearColor();
    positionAxisIndicator();
    retranslateUi();
}

PointCloudViewer::~PointCloudViewer()
{
    if (osgWidget_ != nullptr) {
        if (osgViewer::Viewer* viewer = osgWidget_->getViewer()) {
            viewer->setDone(true);
        }
    }
}

bool PointCloudViewer::loadPointCloud(const QString& filePath, QString* errorMessage)
{
    LasReader reader;
    PointCloudData loadedPointCloud;
    QString localError;

    if (!reader.read(filePath, &loadedPointCloud, &localError)) {
        updateMessage(tr("Open failed"), localError);
        if (errorMessage != nullptr) {
            *errorMessage = localError;
        }
        return false;
    }

    if (loadedPointCloud.empty()) {
        localError = tr("Point cloud file is empty.");
        updateMessage(tr("Open failed"), localError);
        if (errorMessage != nullptr) {
            *errorMessage = localError;
        }
        return false;
    }

    currentPointCloud_ = std::move(loadedPointCloud);
    currentFilePath_ = filePath;
    hoveredPointValid_ = false;
    lastHoverQueryPosition_ = QPointF();
    lastHoverQueryTime_ = {};
    towerMarkers_.clear();
    selectedTowerIndex_ = -1;
    towerEditMode_ = TowerEditMode::None;
    towerEditTargetIndex_ = -1;
    updateSceneClickCapture();
    resetMeasurementState(false);

    if (!currentPointCloud_.hasColor() && visualizationOptions_.colorMode == PointCloudColorMode::Rgb) {
        visualizationOptions_.colorMode = PointCloudColorMode::Elevation;
    }

    rebuildScene();
    applyViewPreset(PointCloudViewPreset::Isometric);
    updateFooter();

    if (errorMessage != nullptr) {
        *errorMessage = tr("Loaded point cloud with %1 points.")
            .arg(formatPointCount(currentPointCloud_.size()));
    }

    emit pointCloudLoaded();
    emit visualizationOptionsChanged();
    emit measurementChanged();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerEditModeChanged();
    emit towerMarkersChanged();
    return true;
}

void PointCloudViewer::clearPointCloud()
{
    currentPointCloud_.clear();
    currentFilePath_.clear();
    hoveredPointValid_ = false;
    lastHoverQueryPosition_ = QPointF();
    lastHoverQueryTime_ = {};
    towerMarkers_.clear();
    selectedTowerIndex_ = -1;
    towerEditMode_ = TowerEditMode::None;
    towerEditTargetIndex_ = -1;
    updateSceneClickCapture();
    resetMeasurementState(false);

    if (rootGroup_.valid()) {
        rootGroup_->removeChildren(0, rootGroup_->getNumChildren());
    }

    pointCloudNode_ = nullptr;
    towerMarkersNode_ = nullptr;
    measurementOverlayNode_ = nullptr;
    updateTowerOverlayWidgets();
    updateMessage(
        tr("Scene cleared"),
        tr("Open a LAS or LAZ file to continue."));

    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }

    emit pointCloudCleared();
    emit measurementChanged();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerEditModeChanged();
    emit towerMarkersChanged();
}

bool PointCloudViewer::hasPointCloud() const
{
    return !currentPointCloud_.empty();
}

QString PointCloudViewer::currentFilePath() const
{
    return currentFilePath_;
}

const PointCloudData* PointCloudViewer::pointCloudData() const
{
    return hasPointCloud() ? &currentPointCloud_ : nullptr;
}

const PointCloudVisualizationOptions& PointCloudViewer::visualizationOptions() const
{
    return visualizationOptions_;
}

const InteractionOptions& PointCloudViewer::interactionOptions() const
{
    return interactionOptions_;
}

bool PointCloudViewer::measurementEnabled() const
{
    return measurementEnabled_;
}

const MeasurementResult& PointCloudViewer::measurementResult() const
{
    return measurementResult_;
}

bool PointCloudViewer::hasHoveredPoint() const
{
    return hoveredPointValid_;
}

PointRecord PointCloudViewer::hoveredPoint() const
{
    return hoveredPoint_;
}

const QList<TowerMarker>& PointCloudViewer::towerMarkers() const
{
    return towerMarkers_;
}

int PointCloudViewer::selectedTowerIndex() const
{
    return selectedTowerIndex_;
}

TowerEditMode PointCloudViewer::towerEditMode() const
{
    return towerEditMode_;
}

void PointCloudViewer::setPointSize(int pointSize)
{
    const float clampedPointSize = std::clamp(static_cast<float>(pointSize), 1.0f, 12.0f);
    if (qFuzzyCompare(visualizationOptions_.pointSize, clampedPointSize)) {
        return;
    }

    visualizationOptions_.pointSize = clampedPointSize;
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setPointOpacity(int opacityPercent)
{
    const float clampedOpacity = clampUnit(static_cast<float>(opacityPercent) / 100.0f);
    if (qFuzzyCompare(visualizationOptions_.pointOpacity, clampedOpacity)) {
        return;
    }

    visualizationOptions_.pointOpacity = clampedOpacity;
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setColorMode(int colorModeIndex)
{
    switch (colorModeIndex) {
    case 1:
        setColorMode(PointCloudColorMode::Elevation);
        break;
    case 2:
        setColorMode(PointCloudColorMode::SingleColor);
        break;
    case 0:
    default:
        setColorMode(PointCloudColorMode::Rgb);
        break;
    }
}

void PointCloudViewer::setColorMode(PointCloudColorMode colorMode)
{
    if (visualizationOptions_.colorMode == colorMode) {
        return;
    }

    visualizationOptions_.colorMode = colorMode;
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setSingleColor(const QColor& color)
{
    if (!color.isValid() || visualizationOptions_.singleColor == color) {
        return;
    }

    visualizationOptions_.singleColor = color;
    if (visualizationOptions_.colorMode == PointCloudColorMode::SingleColor) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setBackgroundColor(const QColor& color)
{
    if (!color.isValid() || visualizationOptions_.backgroundColor == color) {
        return;
    }

    visualizationOptions_.backgroundColor = color;
    applyClearColor();
    if (hasPointCloud()) {
        rebuildScene();
    }
    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setDepthCueStrength(int strengthPercent)
{
    const float clampedStrength = clampUnit(static_cast<float>(strengthPercent) / 100.0f);
    if (qFuzzyCompare(visualizationOptions_.depthCueStrength, clampedStrength)) {
        return;
    }

    visualizationOptions_.depthCueStrength = clampedStrength;
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setEdlStrength(int strengthPercent)
{
    const float clampedStrength = clampUnit(static_cast<float>(strengthPercent) / 100.0f);
    if (qFuzzyCompare(visualizationOptions_.edlStrength, clampedStrength)) {
        return;
    }

    visualizationOptions_.edlStrength = clampedStrength;
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setUseRoundSplats(bool enabled)
{
    if (visualizationOptions_.useRoundSplats == enabled) {
        return;
    }

    visualizationOptions_.useRoundSplats = enabled;
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setShowAxes(bool showAxes)
{
    if (visualizationOptions_.showAxes == showAxes) {
        return;
    }

    visualizationOptions_.showAxes = showAxes;
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setShowBoundingBox(bool showBoundingBox)
{
    if (visualizationOptions_.showBoundingBox == showBoundingBox) {
        return;
    }

    visualizationOptions_.showBoundingBox = showBoundingBox;
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::resetView()
{
    setViewPreset(PointCloudViewPreset::Isometric);
}

void PointCloudViewer::setViewPreset(PointCloudViewPreset viewPreset)
{
    applyViewPreset(viewPreset);
}

void PointCloudViewer::setInteractionOptions(const InteractionOptions& options)
{
    if (interactionOptions_.invertOrbitDrag == options.invertOrbitDrag
        && interactionOptions_.invertPanDrag == options.invertPanDrag
        && interactionOptions_.invertWheelZoom == options.invertWheelZoom) {
        return;
    }

    interactionOptions_ = options;
    if (osgWidget_ != nullptr) {
        osgWidget_->setInteractionOptions(interactionOptions_);
    }
    emit interactionOptionsChanged();
}

void PointCloudViewer::setInvertOrbitDrag(bool invert)
{
    InteractionOptions options = interactionOptions_;
    options.invertOrbitDrag = invert;
    setInteractionOptions(options);
}

void PointCloudViewer::setInvertPanDrag(bool invert)
{
    InteractionOptions options = interactionOptions_;
    options.invertPanDrag = invert;
    setInteractionOptions(options);
}

void PointCloudViewer::setInvertWheelZoom(bool invert)
{
    InteractionOptions options = interactionOptions_;
    options.invertWheelZoom = invert;
    setInteractionOptions(options);
}

void PointCloudViewer::setMeasurementEnabled(bool enabled)
{
    if (enabled && !hasPointCloud()) {
        emit measurementMessage(tr("Load a point cloud before starting measurement."), true);
        return;
    }

    if (measurementEnabled_ == enabled) {
        return;
    }

    if (enabled && towerEditMode_ != TowerEditMode::None) {
        towerEditMode_ = TowerEditMode::None;
        towerEditTargetIndex_ = -1;
        emit towerEditModeChanged();
    }

    measurementEnabled_ = enabled;
    updateSceneClickCapture();

    if (!measurementEnabled_) {
        resetMeasurementState();
        emit measurementMessage(tr("Measurement mode disabled."), false);
    } else {
        resetMeasurementState();
        emit measurementMessage(tr("Measurement mode enabled. Click the first point."), false);
    }

    updateFooter();
    emit measurementModeChanged();
}

void PointCloudViewer::clearMeasurement()
{
    const bool hadMeasurement = measurementResult_.hasStartPoint || measurementResult_.hasEndPoint;
    resetMeasurementState();
    if (hadMeasurement) {
        emit measurementMessage(tr("Measurement cleared."), false);
    }
    updateFooter();
}

void PointCloudViewer::updateSceneClickCapture()
{
    if (osgWidget_ == nullptr) {
        return;
    }

    const bool sceneClickEnabled =
        measurementEnabled_
        || towerEditMode_ != TowerEditMode::None
        || !towerMarkers_.isEmpty();
    osgWidget_->setSceneClickModeEnabled(sceneClickEnabled);
}

bool PointCloudViewer::addTowerMarker(const QString& name, const PointRecord& point)
{
    return insertTowerMarker(towerMarkers_.size(), name, point);
}

bool PointCloudViewer::insertTowerMarker(int index, const QString& name, const PointRecord& point)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty() || index < 0 || index > towerMarkers_.size()) {
        return false;
    }

    TowerMarker towerMarker;
    towerMarker.name = trimmedName;
    towerMarker.point = point;
    towerMarkers_.insert(index, towerMarker);
    selectedTowerIndex_ = index;
    updateSceneClickCapture();
    refreshTowerMarkersOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerMarkersChanged();
    return true;
}

bool PointCloudViewer::addTowerMarkerFromHoveredPoint(const QString& name, QString* errorMessage)
{
    if (!hoveredPointValid_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Hover a point before adding a tower marker.");
        }
        return false;
    }

    if (!addTowerMarker(name, hoveredPoint_)) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Tower marker name cannot be empty.");
        }
        return false;
    }

    return true;
}

void PointCloudViewer::setTowerMarkers(const QList<TowerMarker>& towerMarkers)
{
    towerMarkers_ = towerMarkers;
    selectedTowerIndex_ = towerMarkers_.isEmpty() ? -1 : std::clamp(selectedTowerIndex_, 0, towerMarkers_.size() - 1);
    updateSceneClickCapture();
    refreshTowerMarkersOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerMarkersChanged();
}

bool PointCloudViewer::setTowerMarkerName(int index, const QString& name)
{
    if (index < 0 || index >= towerMarkers_.size()) {
        return false;
    }

    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return false;
    }

    if (towerMarkers_[index].name == trimmedName) {
        return true;
    }

    towerMarkers_[index].name = trimmedName;
    refreshTowerMarkersOverlay();
    emit towerMarkersChanged();
    return true;
}

void PointCloudViewer::setSelectedTowerIndex(int index)
{
    const int normalizedIndex = (index >= 0 && index < towerMarkers_.size()) ? index : -1;
    if (selectedTowerIndex_ == normalizedIndex) {
        return;
    }

    selectedTowerIndex_ = normalizedIndex;
    refreshTowerMarkersOverlay();
    emit selectedTowerChanged(selectedTowerIndex_);
}

bool PointCloudViewer::removeTowerMarker(int index)
{
    if (index < 0 || index >= towerMarkers_.size()) {
        return false;
    }

    towerMarkers_.removeAt(index);
    if (towerMarkers_.isEmpty()) {
        selectedTowerIndex_ = -1;
    } else {
        selectedTowerIndex_ = std::clamp(index, 0, towerMarkers_.size() - 1);
    }
    updateSceneClickCapture();
    refreshTowerMarkersOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerMarkersChanged();
    return true;
}

bool PointCloudViewer::moveTowerMarker(int index, const PointRecord& point)
{
    if (index < 0 || index >= towerMarkers_.size()) {
        return false;
    }

    towerMarkers_[index].point = point;
    selectedTowerIndex_ = index;
    refreshTowerMarkersOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerMarkersChanged();
    return true;
}

void PointCloudViewer::clearTowerMarkers()
{
    if (towerMarkers_.isEmpty()) {
        return;
    }

    towerMarkers_.clear();
    selectedTowerIndex_ = -1;
    cancelTowerEditMode();
    updateSceneClickCapture();
    refreshTowerMarkersOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerMarkersChanged();
}

void PointCloudViewer::beginTowerAddMode()
{
    towerEditMode_ = TowerEditMode::AddAfterLast;
    towerEditTargetIndex_ = -1;
    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    } else {
        updateSceneClickCapture();
    }
    emit towerEditModeChanged();
}

void PointCloudViewer::beginTowerInsertMode(int beforeIndex)
{
    if (beforeIndex < 0 || beforeIndex >= towerMarkers_.size()) {
        return;
    }

    towerEditMode_ = TowerEditMode::InsertBeforeSelected;
    towerEditTargetIndex_ = beforeIndex;
    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    } else {
        updateSceneClickCapture();
    }
    emit towerEditModeChanged();
}

void PointCloudViewer::beginTowerMoveMode(int towerIndex)
{
    if (towerIndex < 0 || towerIndex >= towerMarkers_.size()) {
        return;
    }

    towerEditMode_ = TowerEditMode::MoveSelected;
    towerEditTargetIndex_ = towerIndex;
    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    } else {
        updateSceneClickCapture();
    }
    emit towerEditModeChanged();
}

void PointCloudViewer::cancelTowerEditMode()
{
    if (towerEditMode_ == TowerEditMode::None) {
        return;
    }

    towerEditMode_ = TowerEditMode::None;
    towerEditTargetIndex_ = -1;
    updateSceneClickCapture();
    emit towerEditModeChanged();
}

bool PointCloudViewer::focusOnPoint(const PointRecord& point, double distanceScale)
{
    if (!hasPointCloud() || osgWidget_ == nullptr) {
        return false;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr) {
        return false;
    }

    auto* manipulator = dynamic_cast<osgGA::TrackballManipulator*>(viewer->getCameraManipulator());
    if (manipulator == nullptr) {
        return false;
    }

    osg::Vec3d eye;
    osg::Vec3d center;
    osg::Vec3d up;
    manipulator->getTransformation(eye, center, up);

    osg::Vec3d offset = eye - center;
    const double minFocusDistance = std::max({
        static_cast<double>(currentPointCloud_.maxBounds().x - currentPointCloud_.minBounds().x),
        static_cast<double>(currentPointCloud_.maxBounds().y - currentPointCloud_.minBounds().y),
        static_cast<double>(currentPointCloud_.maxBounds().z - currentPointCloud_.minBounds().z),
        2.0
    }) * std::max(0.05, distanceScale);

    if (offset.length2() < 1e-8) {
        offset = osg::Vec3d(minFocusDistance, -minFocusDistance, minFocusDistance * 0.6);
    } else {
        offset.normalize();
        offset *= std::max(minFocusDistance, (eye - center).length() * std::max(0.15, distanceScale));
    }

    const osg::Vec3d target(point.x, point.y, point.z);
    manipulator->setHomePosition(target + offset, target, up.length2() < 1e-8 ? osg::Vec3d(0.0, 0.0, 1.0) : up);
    manipulator->home(0.0);
    osgWidget_->update();
    return true;
}

void PointCloudViewer::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (event != nullptr && event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

void PointCloudViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionAxisIndicator();
}

void PointCloudViewer::createStatusPanel()
{
    statusPanel_ = new QFrame(this);
    statusPanel_->setObjectName(QStringLiteral("viewerStatusPanel"));

    auto* statusLayout = new QVBoxLayout(statusPanel_);
    statusLayout->setContentsMargins(16, 10, 16, 12);
    statusLayout->setSpacing(2);

    titleLabel_ = new QLabel(statusPanel_);
    titleLabel_->setObjectName(QStringLiteral("viewerTitleLabel"));

    detailLabel_ = new QLabel(statusPanel_);
    detailLabel_->setObjectName(QStringLiteral("viewerDetailLabel"));
    detailLabel_->setWordWrap(true);

    cursorLabel_ = new QLabel(statusPanel_);
    cursorLabel_->setObjectName(QStringLiteral("viewerCursorLabel"));
    cursorLabel_->setWordWrap(true);
    cursorLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    statusLayout->addWidget(titleLabel_);
    statusLayout->addWidget(detailLabel_);
    statusLayout->addWidget(cursorLabel_);
}

void PointCloudViewer::createMeasurementOverlayWidgets()
{
    auto configureOverlayLabel = [this](QLabel*& label, const QString& objectName, const QString& extraStyle = QString()) {
        label = new QLabel(osgWidget_);
        label->setObjectName(objectName);
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        label->setStyleSheet(QStringLiteral(
            "QLabel {"
            "background-color: rgba(15, 23, 42, 220);"
            "color: #f8fafc;"
            "border: 1px solid rgba(148, 163, 184, 180);"
            "border-radius: 8px;"
            "padding: 4px 8px;"
            "font-size: 12px;"
            "font-weight: 600;"
            "}")
            + extraStyle);
        label->hide();
        label->raise();
    };

    configureOverlayLabel(measurementStartOverlayLabel_, QStringLiteral("measurementStartOverlayLabel"));
    configureOverlayLabel(measurementEndOverlayLabel_, QStringLiteral("measurementEndOverlayLabel"));
    configureOverlayLabel(
        measurementSummaryOverlayLabel_,
        QStringLiteral("measurementSummaryOverlayLabel"),
        QStringLiteral(
            "QLabel {"
            "font-size: 13px;"
            "padding: 6px 10px;"
            "}"));
}

void PointCloudViewer::rebuildScene()
{
    if (!rootGroup_.valid()) {
        return;
    }

    rootGroup_->removeChildren(0, rootGroup_->getNumChildren());
    pointCloudNode_ = nullptr;
    towerMarkersNode_ = nullptr;
    measurementOverlayNode_ = nullptr;

    if (!hasPointCloud()) {
        if (osgWidget_ != nullptr) {
            osgWidget_->update();
        }
        updateTowerOverlayWidgets();
        updateMeasurementOverlayWidgets();
        return;
    }

    pointCloudNode_ = OsgPointCloudNode::build(currentPointCloud_, visualizationOptions_);
    if (pointCloudNode_.valid()) {
        rootGroup_->addChild(pointCloudNode_.get());
    }

    refreshTowerMarkersOverlay();
    refreshMeasurementOverlay();
}

void PointCloudViewer::updateFooter()
{
    if (!hasPointCloud()) {
        updateMessage(
            tr("Ready for point cloud inspection"),
            tr("Open a LAS or LAZ file. Left drag orbits, middle or right drag pans, and the mouse wheel zooms."));
        if (cursorLabel_ != nullptr) {
            cursorLabel_->setText(tr("Cursor Point: N/A"));
        }
        return;
    }

    const QFileInfo fileInfo(currentFilePath_);
    const QString title = fileInfo.fileName().isEmpty() ? currentFilePath_ : fileInfo.fileName();
    QString detail = tr("%1 points | %2 | %3 px | Axes %4 | Bounds %5")
        .arg(formatPointCount(currentPointCloud_.size()))
        .arg(colorModeLabel(visualizationOptions_.colorMode))
        .arg(QLocale().toString(static_cast<int>(visualizationOptions_.pointSize)))
        .arg(visualizationOptions_.showAxes ? tr("on") : tr("off"))
        .arg(visualizationOptions_.showBoundingBox ? tr("on") : tr("off"));
    detail += tr(" | Towers %1").arg(QLocale().toString(towerMarkers_.size()));

    if (measurementEnabled_) {
        if (measurementResult_.isComplete()) {
            detail += tr(" | Measure %1 | ΔZ %2")
                .arg(formatCoordinate(measurementResult_.distance3d))
                .arg(formatCoordinate(measurementResult_.deltaZ));
        } else if (measurementResult_.hasStartPoint) {
            detail += tr(" | Measurement: pick the second point");
        } else {
            detail += tr(" | Measurement: pick the first point");
        }
    }

    detail += tr(" | Opacity %1% | Depth Cue %2 | EDL-style %3")
        .arg(QLocale().toString(percentFromUnit(visualizationOptions_.pointOpacity)))
        .arg(QLocale().toString(percentFromUnit(visualizationOptions_.depthCueStrength)))
        .arg(QLocale().toString(percentFromUnit(visualizationOptions_.edlStrength)));

    if (visualizationOptions_.useRoundSplats) {
        detail += tr(" | Round splats");
    }

    updateMessage(title, detail);

    if (cursorLabel_ != nullptr) {
        cursorLabel_->setText(
            hoveredPointValid_
                ? tr("Cursor Point: %1").arg(formatTriplet(hoveredPoint_.x, hoveredPoint_.y, hoveredPoint_.z))
                : tr("Cursor Point: N/A"));
    }
}

void PointCloudViewer::updateMessage(const QString& title, const QString& detail)
{
    titleLabel_->setText(title);
    detailLabel_->setText(detail);
}

void PointCloudViewer::applyClearColor()
{
    if (osgWidget_ == nullptr) {
        return;
    }

    if (osgViewer::Viewer* viewer = osgWidget_->getViewer()) {
        viewer->getCamera()->setClearColor(osg::Vec4(
            visualizationOptions_.backgroundColor.redF(),
            visualizationOptions_.backgroundColor.greenF(),
            visualizationOptions_.backgroundColor.blueF(),
            1.0f));
    }

    osgWidget_->update();
}

void PointCloudViewer::applyViewPreset(PointCloudViewPreset viewPreset)
{
    if (!hasPointCloud() || osgWidget_ == nullptr) {
        return;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr) {
        return;
    }

    osgGA::CameraManipulator* manipulator = viewer->getCameraManipulator();
    if (manipulator == nullptr) {
        return;
    }

    const PointRecord& minBounds = currentPointCloud_.minBounds();
    const PointRecord& maxBounds = currentPointCloud_.maxBounds();
    const osg::Vec3d center(
        (minBounds.x + maxBounds.x) * 0.5,
        (minBounds.y + maxBounds.y) * 0.5,
        (minBounds.z + maxBounds.z) * 0.5);

    const double spanX = static_cast<double>(maxBounds.x - minBounds.x);
    const double spanY = static_cast<double>(maxBounds.y - minBounds.y);
    const double spanZ = static_cast<double>(maxBounds.z - minBounds.z);
    const double maxSpan = std::max({spanX, spanY, spanZ, 1.0});
    const double distance = maxSpan * 1.45;

    osg::Vec3d eye;
    osg::Vec3d up(0.0, 0.0, 1.0);

    switch (viewPreset) {
    case PointCloudViewPreset::Top:
        eye = center + osg::Vec3d(0.0, 0.0, distance);
        up = osg::Vec3d(0.0, 1.0, 0.0);
        break;
    case PointCloudViewPreset::Front:
        eye = center + osg::Vec3d(0.0, -distance, 0.0);
        break;
    case PointCloudViewPreset::Right:
        eye = center + osg::Vec3d(distance, 0.0, 0.0);
        break;
    case PointCloudViewPreset::Isometric:
    default:
        eye = center + osg::Vec3d(distance, -distance, distance * 0.6);
        break;
    }

    manipulator->setHomePosition(eye, center, up);
    manipulator->home(0.0);
    osgWidget_->update();
}

void PointCloudViewer::handleSceneClick(const QPointF& localPos)
{
    if (towerEditMode_ == TowerEditMode::None && !measurementEnabled_ && towerMarkers_.isEmpty()) {
        return;
    }

    if (towerEditMode_ == TowerEditMode::None && !measurementEnabled_) {
        setSelectedTowerIndex(pickTowerMarkerAtScreenPosition(localPos));
        return;
    }

    PointRecord pickedPoint;
    if (!pickPointAtScreenPosition(localPos, &pickedPoint)) {
        emit measurementMessage(tr("No point was found near the clicked position."), true);
        return;
    }

    if (towerEditMode_ != TowerEditMode::None) {
        const TowerEditMode requestedMode = towerEditMode_;
        const int targetIndex = towerEditTargetIndex_;
        if (towerEditMode_ != TowerEditMode::AddAfterLast) {
            towerEditMode_ = TowerEditMode::None;
            towerEditTargetIndex_ = -1;
            updateSceneClickCapture();
            emit towerEditModeChanged();
        }
        emit towerEditRequested(pickedPoint, static_cast<int>(requestedMode), targetIndex);
        return;
    }

    if (!measurementResult_.hasStartPoint || measurementResult_.isComplete()) {
        measurementResult_ = MeasurementResult();
        measurementResult_.hasStartPoint = true;
        measurementResult_.startPoint = pickedPoint;
        emit measurementMessage(tr("First point selected. Click the second point."), false);
    } else {
        measurementResult_.hasEndPoint = true;
        measurementResult_.endPoint = pickedPoint;

        const double dx = static_cast<double>(measurementResult_.endPoint.x - measurementResult_.startPoint.x);
        const double dy = static_cast<double>(measurementResult_.endPoint.y - measurementResult_.startPoint.y);
        const double dz = static_cast<double>(measurementResult_.endPoint.z - measurementResult_.startPoint.z);

        measurementResult_.distance3d = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
        measurementResult_.deltaZ = measurementResult_.endPoint.z - measurementResult_.startPoint.z;

        emit measurementMessage(
            tr("Measured distance %1 with height delta %2.")
                .arg(formatCoordinate(measurementResult_.distance3d))
                .arg(formatCoordinate(measurementResult_.deltaZ)),
            false);
    }

    refreshMeasurementOverlay();
    updateFooter();
    emit measurementChanged();
}

void PointCloudViewer::handleSceneHover(const QPointF& localPos)
{
    if (!hasPointCloud()) {
        clearHoveredPoint();
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const bool timeReady =
        lastHoverQueryTime_.time_since_epoch().count() == 0
        || std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHoverQueryTime_).count() >= 24;
    const bool movedEnough =
        std::hypot(localPos.x() - lastHoverQueryPosition_.x(), localPos.y() - lastHoverQueryPosition_.y()) >= 2.0;

    if (!timeReady && !movedEnough) {
        return;
    }

    lastHoverQueryTime_ = now;
    lastHoverQueryPosition_ = localPos;

    PointRecord pickedPoint;
    if (pickPointAtScreenPosition(localPos, &pickedPoint, kHoverPickTolerancePixels)) {
        updateHoveredPoint(&pickedPoint);
    } else {
        clearHoveredPoint();
    }
}

void PointCloudViewer::clearHoveredPoint()
{
    lastHoverQueryTime_ = {};
    if (!hoveredPointValid_) {
        return;
    }

    hoveredPointValid_ = false;
    updateFooter();
}

void PointCloudViewer::updateHoveredPoint(const PointRecord* hoveredPoint)
{
    if (hoveredPoint == nullptr) {
        clearHoveredPoint();
        return;
    }

    hoveredPoint_ = *hoveredPoint;
    hoveredPointValid_ = true;
    updateFooter();
}

bool PointCloudViewer::pickPointAtScreenPosition(const QPointF& localPos, PointRecord* pickedPoint, float tolerancePixels) const
{
    if (pickedPoint == nullptr || !hasPointCloud() || osgWidget_ == nullptr) {
        return false;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCamera() == nullptr || viewer->getCamera()->getViewport() == nullptr) {
        return false;
    }

    osg::Camera* camera = viewer->getCamera();
    const osg::Matrixd worldToWindow =
        camera->getViewMatrix() *
        camera->getProjectionMatrix() *
        camera->getViewport()->computeWindowMatrix();

    const double devicePixelRatio = osgWidget_->devicePixelRatioF();
    const double clickX = localPos.x() * devicePixelRatio;
    const double clickY = (static_cast<double>(osgWidget_->height()) - localPos.y()) * devicePixelRatio;
    const double tolerance = static_cast<double>(tolerancePixels) * devicePixelRatio;
    const double toleranceSquared = tolerance * tolerance;

    bool found = false;
    double bestDistanceSquared = toleranceSquared;
    double bestDepth = std::numeric_limits<double>::max();

    for (const PointRecord& point : currentPointCloud_.points()) {
        const osg::Vec3d projected = osg::Vec3d(point.x, point.y, point.z) * worldToWindow;
        if (projected.z() < 0.0 || projected.z() > 1.0) {
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
            *pickedPoint = point;
        }
    }

    return found;
}

int PointCloudViewer::pickTowerMarkerAtScreenPosition(const QPointF& localPos, float tolerancePixels) const
{
    if (towerMarkers_.isEmpty() || osgWidget_ == nullptr) {
        return -1;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCamera() == nullptr || viewer->getCamera()->getViewport() == nullptr) {
        return -1;
    }

    osg::Camera* camera = viewer->getCamera();
    const osg::Matrixd worldToWindow =
        camera->getViewMatrix() *
        camera->getProjectionMatrix() *
        camera->getViewport()->computeWindowMatrix();

    const double devicePixelRatio = osgWidget_->devicePixelRatioF();
    const double clickX = localPos.x() * devicePixelRatio;
    const double clickY = (static_cast<double>(osgWidget_->height()) - localPos.y()) * devicePixelRatio;
    const double tolerance = static_cast<double>(tolerancePixels) * devicePixelRatio;
    const double toleranceSquared = tolerance * tolerance;

    int bestIndex = -1;
    double bestDistanceSquared = toleranceSquared;
    double bestDepth = std::numeric_limits<double>::max();

    for (int towerIndex = 0; towerIndex < towerMarkers_.size(); ++towerIndex) {
        const TowerMarker& towerMarker = towerMarkers_.at(towerIndex);
        const osg::Vec3d projected = osg::Vec3d(towerMarker.point.x, towerMarker.point.y, towerMarker.point.z) * worldToWindow;
        if (projected.z() < 0.0 || projected.z() > 1.0) {
            continue;
        }

        const double dx = projected.x() - clickX;
        const double dy = projected.y() - clickY;
        const double distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > bestDistanceSquared) {
            continue;
        }

        if (bestIndex < 0
            || distanceSquared < bestDistanceSquared - 0.001
            || (std::abs(distanceSquared - bestDistanceSquared) <= 0.001 && projected.z() < bestDepth)) {
            bestIndex = towerIndex;
            bestDistanceSquared = distanceSquared;
            bestDepth = projected.z();
        }
    }

    return bestIndex;
}

osg::ref_ptr<osg::Node> PointCloudViewer::buildMeasurementOverlay() const
{
    if (!measurementResult_.hasStartPoint) {
        return nullptr;
    }

    osg::ref_ptr<osg::Group> overlay = new osg::Group();

    osg::ref_ptr<osg::Geode> markersGeode = buildMeasurementMarkersGeode(measurementResult_);
    if (markersGeode.valid()) {
        overlay->addChild(markersGeode.get());
    }

    osg::ref_ptr<osg::Geode> lineGeode = buildMeasurementLineGeode(measurementResult_);
    if (lineGeode.valid()) {
        overlay->addChild(lineGeode.get());
    }

    return overlay->getNumChildren() > 0 ? overlay.release() : nullptr;
}

osg::ref_ptr<osg::Node> PointCloudViewer::buildTowerMarkersOverlay() const
{
    if (towerMarkers_.isEmpty()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Geode> markersGeode = buildTowerMarkersGeode(towerMarkers_);
    return markersGeode.release();
}

QPointF PointCloudViewer::projectPointToViewport(const PointRecord& point, bool* visible) const
{
    if (visible != nullptr) {
        *visible = false;
    }

    if (osgWidget_ == nullptr || !hasPointCloud()) {
        return QPointF();
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCamera() == nullptr || viewer->getCamera()->getViewport() == nullptr) {
        return QPointF();
    }

    osg::Camera* camera = viewer->getCamera();
    const osg::Matrixd worldToWindow =
        camera->getViewMatrix() *
        camera->getProjectionMatrix() *
        camera->getViewport()->computeWindowMatrix();

    const osg::Vec3d projected = osg::Vec3d(point.x, point.y, point.z) * worldToWindow;
    if (projected.z() < 0.0 || projected.z() > 1.0) {
        return QPointF();
    }

    const qreal devicePixelRatio = osgWidget_->devicePixelRatioF();
    const QPointF viewportPoint(
        projected.x() / devicePixelRatio,
        static_cast<qreal>(osgWidget_->height()) - projected.y() / devicePixelRatio);

    if (viewportPoint.x() < 0.0
        || viewportPoint.x() > static_cast<qreal>(osgWidget_->width())
        || viewportPoint.y() < 0.0
        || viewportPoint.y() > static_cast<qreal>(osgWidget_->height())) {
        return QPointF();
    }

    if (visible != nullptr) {
        *visible = true;
    }
    return viewportPoint;
}

void PointCloudViewer::updateMeasurementOverlayWidgets()
{
    auto hideAllLabels = [this]() {
        if (measurementStartOverlayLabel_ != nullptr) {
            measurementStartOverlayLabel_->hide();
        }
        if (measurementEndOverlayLabel_ != nullptr) {
            measurementEndOverlayLabel_->hide();
        }
        if (measurementSummaryOverlayLabel_ != nullptr) {
            measurementSummaryOverlayLabel_->hide();
        }
    };

    if (osgWidget_ == nullptr || !measurementResult_.hasStartPoint) {
        hideAllLabels();
        return;
    }

    bool startVisible = false;
    const QPointF startPoint = projectPointToViewport(measurementResult_.startPoint, &startVisible);
    if (measurementStartOverlayLabel_ != nullptr && startVisible) {
        measurementStartOverlayLabel_->setText(tr("A"));
        positionOverlayLabel(measurementStartOverlayLabel_, startPoint, QPoint(14, -18));
    } else if (measurementStartOverlayLabel_ != nullptr) {
        measurementStartOverlayLabel_->hide();
    }

    if (!measurementResult_.hasEndPoint) {
        if (measurementEndOverlayLabel_ != nullptr) {
            measurementEndOverlayLabel_->hide();
        }
        if (measurementSummaryOverlayLabel_ != nullptr) {
            measurementSummaryOverlayLabel_->hide();
        }
        return;
    }

    bool endVisible = false;
    const QPointF endPoint = projectPointToViewport(measurementResult_.endPoint, &endVisible);
    if (measurementEndOverlayLabel_ != nullptr && endVisible) {
        measurementEndOverlayLabel_->setText(tr("B"));
        positionOverlayLabel(measurementEndOverlayLabel_, endPoint, QPoint(14, -18));
    } else if (measurementEndOverlayLabel_ != nullptr) {
        measurementEndOverlayLabel_->hide();
    }

    if (measurementSummaryOverlayLabel_ == nullptr || (!startVisible && !endVisible)) {
        if (measurementSummaryOverlayLabel_ != nullptr) {
            measurementSummaryOverlayLabel_->hide();
        }
        return;
    }

    measurementSummaryOverlayLabel_->setText(
        tr("3D %1 | Height %2")
            .arg(formatCoordinate(measurementResult_.distance3d))
            .arg(formatCoordinate(measurementResult_.deltaZ)));

    const QPointF summaryAnchor = startVisible && endVisible
        ? QPointF((startPoint.x() + endPoint.x()) * 0.5, (startPoint.y() + endPoint.y()) * 0.5)
        : (startVisible ? startPoint : endPoint);
    positionOverlayLabel(measurementSummaryOverlayLabel_, summaryAnchor, QPoint(0, -34));
}

void PointCloudViewer::updateTowerOverlayWidgets()
{
    if (osgWidget_ == nullptr) {
        return;
    }

    while (towerOverlayLabels_.size() < towerMarkers_.size()) {
        auto* label = new QLabel(osgWidget_);
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        label->setStyleSheet(QStringLiteral(
            "QLabel {"
            "background-color: rgba(124, 45, 18, 220);"
            "color: #fff7ed;"
            "border: 1px solid rgba(251, 146, 60, 180);"
            "border-radius: 8px;"
            "padding: 4px 8px;"
            "font-size: 12px;"
            "font-weight: 600;"
            "}"));
        label->hide();
        towerOverlayLabels_.append(label);
    }

    for (int towerIndex = 0; towerIndex < towerOverlayLabels_.size(); ++towerIndex) {
        QLabel* label = towerOverlayLabels_.at(towerIndex);
        if (label == nullptr) {
            continue;
        }

        if (towerIndex >= towerMarkers_.size()) {
            label->hide();
            continue;
        }

        bool pointVisible = false;
        const TowerMarker& towerMarker = towerMarkers_.at(towerIndex);
        const QPointF anchor = projectPointToViewport(towerMarker.point, &pointVisible);
        if (!pointVisible) {
            label->hide();
            continue;
        }

        label->setText(towerMarker.name);
        const bool isSelected = towerIndex == selectedTowerIndex_;
        label->setStyleSheet(QStringLiteral(
            "QLabel {"
            "background-color: %1;"
            "color: %2;"
            "border: 1px solid %3;"
            "border-radius: 8px;"
            "padding: 4px 8px;"
            "font-size: 12px;"
            "font-weight: 600;"
            "}").arg(
                isSelected ? QStringLiteral("rgba(180, 83, 9, 235)") : QStringLiteral("rgba(124, 45, 18, 220)"),
                QStringLiteral("#fff7ed"),
                isSelected ? QStringLiteral("rgba(253, 224, 71, 220)") : QStringLiteral("rgba(251, 146, 60, 180)")));
        positionOverlayLabel(label, anchor, QPoint(14, -18));
    }
}

void PointCloudViewer::refreshMeasurementOverlay()
{
    if (rootGroup_.valid() && measurementOverlayNode_.valid()) {
        rootGroup_->removeChild(measurementOverlayNode_.get());
        measurementOverlayNode_ = nullptr;
    }

    if (rootGroup_.valid() && measurementResult_.hasStartPoint) {
        measurementOverlayNode_ = buildMeasurementOverlay();
        if (measurementOverlayNode_.valid()) {
            rootGroup_->addChild(measurementOverlayNode_.get());
        }
    }

    updateMeasurementOverlayWidgets();
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::refreshTowerMarkersOverlay()
{
    if (rootGroup_.valid() && towerMarkersNode_.valid()) {
        rootGroup_->removeChild(towerMarkersNode_.get());
        towerMarkersNode_ = nullptr;
    }

    if (rootGroup_.valid() && !towerMarkers_.isEmpty()) {
        towerMarkersNode_ = buildTowerMarkersOverlay();
        if (towerMarkersNode_.valid()) {
            rootGroup_->addChild(towerMarkersNode_.get());
        }
    }

    updateTowerOverlayWidgets();
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::updateAxisIndicator()
{
    if (axisIndicatorOverlay_ == nullptr || osgWidget_ == nullptr) {
        return;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCamera() == nullptr) {
        return;
    }

    const osg::Matrixd viewMatrix = viewer->getCamera()->getViewMatrix();
    const osg::Vec3d xView = osg::Matrixd::transform3x3(osg::Vec3d(1.0, 0.0, 0.0), viewMatrix);
    const osg::Vec3d yView = osg::Matrixd::transform3x3(osg::Vec3d(0.0, 1.0, 0.0), viewMatrix);
    const osg::Vec3d zView = osg::Matrixd::transform3x3(osg::Vec3d(0.0, 0.0, 1.0), viewMatrix);

    const auto toOverlayDirection = [](const osg::Vec3d& viewVector) {
        QPointF direction(viewVector.x(), -viewVector.y());
        const qreal length = std::hypot(direction.x(), direction.y());
        if (length <= 0.0001) {
            return QPointF(0.0, 0.0);
        }
        const qreal scale = 32.0 / length;
        return QPointF(direction.x() * scale, direction.y() * scale);
    };

    auto* axisIndicatorOverlay = static_cast<AxisIndicatorOverlay*>(axisIndicatorOverlay_);
    axisIndicatorOverlay->setAxisDirections({
        toOverlayDirection(xView),
        toOverlayDirection(yView),
        toOverlayDirection(zView)
    });
    positionAxisIndicator();
}

void PointCloudViewer::positionAxisIndicator()
{
    if (axisIndicatorOverlay_ == nullptr || osgWidget_ == nullptr) {
        return;
    }

    axisIndicatorOverlay_->move(
        std::max(12, osgWidget_->width() - axisIndicatorOverlay_->width() - 14),
        14);
    axisIndicatorOverlay_->raise();
}

void PointCloudViewer::positionOverlayLabel(QLabel* label, const QPointF& anchor, const QPoint& offset) const
{
    if (label == nullptr || osgWidget_ == nullptr) {
        return;
    }

    label->adjustSize();
    const QSize labelSize = label->sizeHint();
    const int x = std::clamp(
        static_cast<int>(std::lround(anchor.x())) - labelSize.width() / 2 + offset.x(),
        0,
        std::max(0, osgWidget_->width() - labelSize.width()));
    const int y = std::clamp(
        static_cast<int>(std::lround(anchor.y())) - labelSize.height() / 2 + offset.y(),
        0,
        std::max(0, osgWidget_->height() - labelSize.height()));

    label->setGeometry(x, y, labelSize.width(), labelSize.height());
    label->show();
    label->raise();
}

void PointCloudViewer::resetMeasurementState(bool notifyChange)
{
    const bool hadMeasurement = measurementResult_.hasStartPoint || measurementResult_.hasEndPoint;
    measurementResult_ = MeasurementResult();

    if (hasPointCloud()) {
        refreshMeasurementOverlay();
        updateFooter();
    }

    if (notifyChange && hadMeasurement) {
        emit measurementChanged();
    } else if (notifyChange && measurementEnabled_) {
        emit measurementChanged();
    }
}

void PointCloudViewer::retranslateUi()
{
    updateFooter();
    updateMeasurementOverlayWidgets();
    updateAxisIndicator();
}
