#include "gui/PointCloudViewer.h"

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
#include <utility>

#include <osg/Camera>
#include <osg/Group>
#include <osg/Vec4>
#include <osg/Viewport>
#include <osgGA/CameraManipulator>
#include <osgGA/TrackballManipulator>

#include "osg/OsgPointCloudNode.h"
#include "pointcloud/LasReader.h"
#include "pointcloud/PointCloudData.h"

namespace
{
QString colorModeLabel(PointCloudColorMode colorMode)
{
    switch (colorMode) {
    case PointCloudColorMode::Elevation:
        return QStringLiteral("Elevation Ramp");
    case PointCloudColorMode::SingleColor:
        return QStringLiteral("Single Color");
    case PointCloudColorMode::Rgb:
    default:
        return QStringLiteral("RGB");
    }
}

QString formatPointCount(std::size_t pointCount)
{
    return QLocale().toString(static_cast<qlonglong>(pointCount));
}
}

OsgWidget::OsgWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    viewer_ = new osgViewer::Viewer();
    viewer_->setThreadingModel(osgViewer::Viewer::SingleThreaded);
    viewer_->setReleaseContextAtEndOfFrameHint(false);
    viewer_->setKeyEventSetsDone(0);
    viewer_->setCameraManipulator(new osgGA::TrackballManipulator());

    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void OsgWidget::setInteractionOptions(const InteractionOptions& options)
{
    interactionOptions_ = options;
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
    } else if (event->button() == Qt::MiddleButton) {
        middleButtonPressed_ = true;
        middleButtonAnchor_ = event->localPos();
    } else if (event->button() == Qt::RightButton) {
        rightButtonPressed_ = true;
        rightButtonAnchor_ = event->localPos();
    }

    if (eventQueue() != nullptr) {
        const float devicePixelRatio = static_cast<float>(devicePixelRatioF());
        const float x = toDevicePixels(static_cast<float>(event->localPos().x()), devicePixelRatio);
        const float y = toDevicePixels(static_cast<float>(height()) - static_cast<float>(event->localPos().y()), devicePixelRatio);

        int button = 0;
        if (event->button() & Qt::LeftButton) {
            button = 1;
        }
        if (event->button() & Qt::MiddleButton) {
            button = 2;
        }
        if (event->button() & Qt::RightButton) {
            button = 3;
        }

        eventQueue()->mouseButtonPress(x, y, button);
    }

    update();
}

void OsgWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }

    if (eventQueue() != nullptr) {
        const float devicePixelRatio = static_cast<float>(devicePixelRatioF());
        const float x = toDevicePixels(static_cast<float>(event->localPos().x()), devicePixelRatio);
        const float y = toDevicePixels(static_cast<float>(height()) - static_cast<float>(event->localPos().y()), devicePixelRatio);

        int button = 0;
        if (event->button() & Qt::LeftButton) {
            button = 1;
        }
        if (event->button() & Qt::MiddleButton) {
            button = 2;
        }
        if (event->button() & Qt::RightButton) {
            button = 3;
        }

        eventQueue()->mouseButtonRelease(x, y, button);
    }

    if (event->button() == Qt::LeftButton) {
        leftButtonPressed_ = false;
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

    if (eventQueue() != nullptr) {
        QPointF adjustedPosition = event->localPos();
        if ((event->buttons() & (Qt::RightButton | Qt::MiddleButton)) != 0
            && interactionOptions_.invertPanDrag) {
            const QPointF anchor = rightButtonPressed_ ? rightButtonAnchor_ : middleButtonAnchor_;
            adjustedPosition = reflectPosition(adjustedPosition, anchor);
        } else if ((event->buttons() & Qt::LeftButton) != 0
            && interactionOptions_.invertOrbitDrag
            && leftButtonPressed_) {
            adjustedPosition = reflectPosition(adjustedPosition, leftButtonAnchor_);
        }

        const float devicePixelRatio = static_cast<float>(devicePixelRatioF());
        const float x = toDevicePixels(static_cast<float>(adjustedPosition.x()), devicePixelRatio);
        const float y = toDevicePixels(static_cast<float>(height()) - static_cast<float>(adjustedPosition.y()), devicePixelRatio);
        eventQueue()->mouseMotion(x, y);
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
        eventQueue()->mouseScroll(
            (scrollUp ^ interactionOptions_.invertWheelZoom)
                ? osgGA::GUIEventAdapter::SCROLL_UP
                : osgGA::GUIEventAdapter::SCROLL_DOWN);
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

float OsgWidget::toDevicePixels(float value, float devicePixelRatio)
{
    return value * devicePixelRatio;
}

QPointF OsgWidget::reflectPosition(const QPointF& position, const QPointF& anchor)
{
    return QPointF(anchor.x() - (position.x() - anchor.x()), anchor.y() - (position.y() - anchor.y()));
}

PointCloudViewer::PointCloudViewer(QWidget* parent)
    : QWidget(parent)
{
    layout_ = new QGridLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);

    osgWidget_ = new OsgWidget(this);
    createStatusPanel();

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

    applyClearColor();
    updateMessage(
        QStringLiteral("Ready for point cloud inspection"),
        QStringLiteral("Open a LAS or LAZ file. Mouse: orbit, pan, zoom. Use the navigation options to invert orbit, pan, or wheel behavior."));
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
        updateMessage(QStringLiteral("Open failed"), localError);
        if (errorMessage != nullptr) {
            *errorMessage = localError;
        }
        return false;
    }

    if (loadedPointCloud.empty()) {
        localError = QStringLiteral("Point cloud file is empty.");
        updateMessage(QStringLiteral("Open failed"), localError);
        if (errorMessage != nullptr) {
            *errorMessage = localError;
        }
        return false;
    }

    currentPointCloud_ = std::move(loadedPointCloud);
    currentFilePath_ = filePath;

    if (!currentPointCloud_.hasColor() && visualizationOptions_.colorMode == PointCloudColorMode::Rgb) {
        visualizationOptions_.colorMode = PointCloudColorMode::Elevation;
    }

    rebuildScene();
    applyViewPreset(PointCloudViewPreset::Isometric);
    updateFooter();

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Loaded point cloud with %1 points.")
            .arg(formatPointCount(currentPointCloud_.size()));
    }

    emit pointCloudLoaded();
    emit visualizationOptionsChanged();
    return true;
}

void PointCloudViewer::clearPointCloud()
{
    currentPointCloud_.clear();
    currentFilePath_.clear();

    if (rootGroup_.valid()) {
        rootGroup_->removeChildren(0, rootGroup_->getNumChildren());
    }

    pointCloudNode_ = nullptr;
    updateMessage(
        QStringLiteral("Scene cleared"),
        QStringLiteral("Open a LAS or LAZ file to continue."));

    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }

    emit pointCloudCleared();
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

void PointCloudViewer::rebuildScene()
{
    if (!rootGroup_.valid()) {
        return;
    }

    rootGroup_->removeChildren(0, rootGroup_->getNumChildren());
    pointCloudNode_ = nullptr;

    if (!hasPointCloud()) {
        if (osgWidget_ != nullptr) {
            osgWidget_->update();
        }
        return;
    }

    pointCloudNode_ = OsgPointCloudNode::build(currentPointCloud_, visualizationOptions_);
    if (pointCloudNode_.valid()) {
        rootGroup_->addChild(pointCloudNode_.get());
    }

    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::updateFooter()
{
    if (!hasPointCloud()) {
        updateMessage(
            QStringLiteral("Ready for point cloud inspection"),
            QStringLiteral("Open a LAS or LAZ file. Mouse: orbit, pan, zoom. Use the navigation options to invert orbit, pan, or wheel behavior."));
        return;
    }

    const QFileInfo fileInfo(currentFilePath_);
    const QString title = fileInfo.fileName().isEmpty() ? currentFilePath_ : fileInfo.fileName();
    const QString detail = QStringLiteral(
        "%1 points | %2 | %3 px | Axes %4 | Bounds %5")
        .arg(formatPointCount(currentPointCloud_.size()))
        .arg(colorModeLabel(visualizationOptions_.colorMode))
        .arg(QLocale().toString(static_cast<int>(visualizationOptions_.pointSize)))
        .arg(visualizationOptions_.showAxes ? QStringLiteral("on") : QStringLiteral("off"))
        .arg(visualizationOptions_.showBoundingBox ? QStringLiteral("on") : QStringLiteral("off"));

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
    const double distance = maxSpan * 2.4;

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
