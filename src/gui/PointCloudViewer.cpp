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
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
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
    manipulator->setTrackballSize(1.1);
    manipulator->setWheelZoomFactor(0.45);
    manipulator->setMinimumDistance(0.0005, true);
    viewer_->setCameraManipulator(manipulator);

    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void OsgWidget::setInteractionOptions(const InteractionOptions& options)
{
    interactionOptions_ = options;
}

void OsgWidget::setMeasurementModeEnabled(bool enabled)
{
    measurementModeEnabled_ = enabled;
    leftButtonPressed_ = false;
    leftButtonDragDetected_ = false;
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

void OsgWidget::mousePressEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }

    if (event->button() == Qt::LeftButton) {
        leftButtonPressed_ = true;
        leftButtonAnchor_ = event->localPos();
        leftButtonDragDetected_ = false;
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

    if (measurementModeEnabled_ && event->button() == Qt::LeftButton) {
        update();
        return;
    }

    if (eventQueue() != nullptr) {
        const float devicePixelRatio = static_cast<float>(devicePixelRatioF());
        const float x = toDevicePixels(static_cast<float>(event->localPos().x()), devicePixelRatio);
        const float y = toDevicePixels(static_cast<float>(height()) - static_cast<float>(event->localPos().y()), devicePixelRatio);

        const int button = mapMouseButton(event->button());
        if (button != 0) {
            eventQueue()->mouseButtonPress(x, y, button);
        }
    }

    update();
}

void OsgWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }

    if (measurementModeEnabled_ && event->button() == Qt::LeftButton) {
        if (!leftButtonDragDetected_) {
            emit sceneClicked(event->localPos());
        }
    } else if (eventQueue() != nullptr) {
        const float devicePixelRatio = static_cast<float>(devicePixelRatioF());
        const float x = toDevicePixels(static_cast<float>(event->localPos().x()), devicePixelRatio);
        const float y = toDevicePixels(static_cast<float>(height()) - static_cast<float>(event->localPos().y()), devicePixelRatio);

        const int button = mapMouseButton(event->button());
        if (button != 0) {
            eventQueue()->mouseButtonRelease(x, y, button);
        }
    }

    if (event->button() == Qt::LeftButton) {
        leftButtonPressed_ = false;
        leftButtonDragDetected_ = false;
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

    if (measurementModeEnabled_ && leftButtonPressed_) {
        const QPointF delta = event->localPos() - leftButtonAnchor_;
        if (std::hypot(delta.x(), delta.y()) > 4.0) {
            leftButtonDragDetected_ = true;
        }
    }

    if (eventQueue() != nullptr) {
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
        } else if (!measurementModeEnabled_
            && (event->buttons() & Qt::LeftButton) != 0
            && leftButtonPressed_) {
            QPointF delta = event->localPos() - lastOrbitCursorPosition_;
            if (interactionOptions_.invertOrbitDrag) {
                delta = QPointF(-delta.x(), -delta.y());
            }
            adjustedPosition = lastOrbitEventPosition_ + delta;
            lastOrbitCursorPosition_ = event->localPos();
            lastOrbitEventPosition_ = adjustedPosition;
        }

        if (!(measurementModeEnabled_ && (event->buttons() & Qt::LeftButton) != 0)) {
            const float devicePixelRatio = static_cast<float>(devicePixelRatioF());
            const float x = toDevicePixels(static_cast<float>(adjustedPosition.x()), devicePixelRatio);
            const float y = toDevicePixels(static_cast<float>(height()) - static_cast<float>(adjustedPosition.y()), devicePixelRatio);
            eventQueue()->mouseMotion(x, y);
        }
    }

    update();
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
        1.0,
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
        "}");

    connect(osgWidget_, &OsgWidget::sceneClicked, this, &PointCloudViewer::handleSceneClick);
    connect(osgWidget_, &OsgWidget::frameRendered, this, &PointCloudViewer::updateMeasurementOverlayWidgets);

    applyClearColor();
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
    return true;
}

void PointCloudViewer::clearPointCloud()
{
    currentPointCloud_.clear();
    currentFilePath_.clear();
    resetMeasurementState(false);

    if (rootGroup_.valid()) {
        rootGroup_->removeChildren(0, rootGroup_->getNumChildren());
    }

    pointCloudNode_ = nullptr;
    measurementOverlayNode_ = nullptr;
    updateMessage(
        tr("Scene cleared"),
        tr("Open a LAS or LAZ file to continue."));

    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }

    emit pointCloudCleared();
    emit measurementChanged();
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

    measurementEnabled_ = enabled;
    if (osgWidget_ != nullptr) {
        osgWidget_->setMeasurementModeEnabled(measurementEnabled_);
    }

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

void PointCloudViewer::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    if (event != nullptr && event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
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

    statusLayout->addWidget(titleLabel_);
    statusLayout->addWidget(detailLabel_);
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
    measurementOverlayNode_ = nullptr;

    if (!hasPointCloud()) {
        if (osgWidget_ != nullptr) {
            osgWidget_->update();
        }
        updateMeasurementOverlayWidgets();
        return;
    }

    pointCloudNode_ = OsgPointCloudNode::build(currentPointCloud_, visualizationOptions_);
    if (pointCloudNode_.valid()) {
        rootGroup_->addChild(pointCloudNode_.get());
    }

    measurementOverlayNode_ = buildMeasurementOverlay();
    if (measurementOverlayNode_.valid()) {
        rootGroup_->addChild(measurementOverlayNode_.get());
    }

    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }

    updateMeasurementOverlayWidgets();
}

void PointCloudViewer::updateFooter()
{
    if (!hasPointCloud()) {
        updateMessage(
            tr("Ready for point cloud inspection"),
            tr("Open a LAS or LAZ file. Left drag orbits, middle or right drag pans, and the mouse wheel zooms."));
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

    updateMessage(title, detail);
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
    if (!measurementEnabled_) {
        return;
    }

    PointRecord pickedPoint;
    if (!pickPointAtScreenPosition(localPos, &pickedPoint)) {
        emit measurementMessage(tr("No point was found near the clicked position."), false);
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

    rebuildScene();
    updateFooter();
    emit measurementChanged();
}

bool PointCloudViewer::pickPointAtScreenPosition(const QPointF& localPos, PointRecord* pickedPoint) const
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
    const double tolerance = 14.0 * devicePixelRatio;
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
    measurementOverlayNode_ = nullptr;

    if (hasPointCloud()) {
        rebuildScene();
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
}
