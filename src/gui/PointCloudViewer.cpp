#include "gui/PointCloudViewer.h"

#include "gui/WelcomeWorkspaceWidget.h"
#include "gui/PointCloudViewerOverlays.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointer>
#include <QRubberBand>
#include <QResizeEvent>
#include <QTimer>
#include <QtMath>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include <osg/Array>
#include <osg/BlendFunc>
#include <osg/Camera>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/LineStipple>
#include <osg/LineWidth>
#include <osg/Matrix>
#include <osg/MatrixTransform>
#include <osg/Point>
#include <osg/Shape>
#include <osg/ShapeDrawable>
#include <osg/State>
#include <osg/StateSet>
#include <osg/Vec4>
#include <osg/Viewport>
#include <osgGA/CameraManipulator>
#include <osgGA/TrackballManipulator>

#include "gaussian/GaussianPlyReader.h"
#include "gaussian/GaussianRenderer.h"
#include "osg/OsgPointCloudNode.h"
#include "domain/DataManager.h"
#include "pointcloud/ClipFilter.h"
#include "pointcloud/LasReader.h"
#include "pointcloud/PointCloudData.h"

namespace
{
constexpr int kMeasurementOverlayRenderBin = 100;
constexpr int kAxisIndicatorSize = 112;
constexpr float kHoverPickTolerancePixels = 12.0f;
constexpr int kRoutePreviewOverlayWidth = 320;
constexpr int kRoutePreviewOverlayHeight = 292;
constexpr int kRoutePreviewRenderWidth = 300;
constexpr int kRoutePreviewRenderHeight = 156;
constexpr int kMinWheelZoomSensitivityPercent = 50;
constexpr int kMaxWheelZoomSensitivityPercent = 200;
constexpr double kDefaultWheelZoomFactor = 0.45;
constexpr int kRouteRoamTimerIntervalMs = 33;
constexpr std::size_t kInteractionPreviewThresholdPoints = 500000;
constexpr std::size_t kInteractionPreviewTargetPoints = 180000;
constexpr int kInteractionLodIdleMilliseconds = 150;
constexpr double kRouteRoamMinSpeedMetersPerSecond = 0.1;
constexpr double kRouteRoamMaxSpeedMetersPerSecond = 80.0;
constexpr double kRouteRoamDwellSeconds = 0.8;
constexpr double kRouteRoamThirdPersonDistanceMeters = 10.0;
constexpr double kRouteRoamThirdPersonHeightMeters = 3.0;
constexpr double kRouteRoamFirstPersonLookAheadMeters = 18.0;
constexpr double kRouteRoamThirdPersonLookAheadMeters = 8.0;
constexpr double kRoutePreviewDefaultFocalLengthRatio = 1.0;
constexpr double kRoutePreviewBaseEquivalentFocalLengthMm = 24.0;
constexpr double kRoutePreviewFullFrameSensorHeightMm = 24.0;
constexpr double kRoutePreviewMinVerticalFovDeg = 8.0;
constexpr double kRoutePreviewMaxVerticalFovDeg = 120.0;

double routeSegmentLength(const PointRecord& startPoint, const PointRecord& endPoint)
{
    const double dx = static_cast<double>(endPoint.x - startPoint.x);
    const double dy = static_cast<double>(endPoint.y - startPoint.y);
    const double dz = static_cast<double>(endPoint.z - startPoint.z);
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double clampRouteRoamSpeed(double speedMetersPerSecond)
{
    return std::clamp(speedMetersPerSecond, kRouteRoamMinSpeedMetersPerSecond, kRouteRoamMaxSpeedMetersPerSecond);
}

int clampWheelZoomSensitivityPercent(int percent)
{
    return std::clamp(percent, kMinWheelZoomSensitivityPercent, kMaxWheelZoomSensitivityPercent);
}

double wheelZoomSensitivityScale(int percent)
{
    return static_cast<double>(clampWheelZoomSensitivityPercent(percent)) / 100.0;
}

std::size_t interactionPreviewThresholdPoints()
{
    bool ok = false;
    const int overrideValue = qEnvironmentVariableIntValue("LAS_VIEWER_INTERACTION_LOD_THRESHOLD_POINTS", &ok);
    return ok && overrideValue > 0
        ? static_cast<std::size_t>(overrideValue)
        : kInteractionPreviewThresholdPoints;
}

double wheelZoomFactorForSensitivity(int percent)
{
    return kDefaultWheelZoomFactor * wheelZoomSensitivityScale(percent);
}

int wheelZoomStepMultiplierForSensitivity(int percent)
{
    const double scale = wheelZoomSensitivityScale(percent);
    return std::clamp(static_cast<int>(std::lround(scale * 2.0)), 1, 4);
}

double normalizedRoutePreviewFocalLengthRatio(double ratio)
{
    if (!std::isfinite(ratio) || ratio <= 0.0) {
        return kRoutePreviewDefaultFocalLengthRatio;
    }

    return std::clamp(ratio, 0.1, 64.0);
}

double routePreviewVerticalFovRadians(double focalLengthRatio)
{
    const double normalizedRatio = normalizedRoutePreviewFocalLengthRatio(focalLengthRatio);
    const double equivalentFocalLengthMm = kRoutePreviewBaseEquivalentFocalLengthMm * normalizedRatio;
    const double unclampedRadians = 2.0 * std::atan(
        kRoutePreviewFullFrameSensorHeightMm / (2.0 * equivalentFocalLengthMm));
    return std::clamp(
        unclampedRadians,
        qDegreesToRadians(kRoutePreviewMinVerticalFovDeg),
        qDegreesToRadians(kRoutePreviewMaxVerticalFovDeg));
}

osg::Vec4 qColorToVec4(const QColor& color, float alphaScale = 1.0f)
{
    const float alpha = std::clamp(alphaScale, 0.0f, 1.0f) * static_cast<float>(color.alphaF());
    return osg::Vec4(
        static_cast<float>(color.redF()),
        static_cast<float>(color.greenF()),
        static_cast<float>(color.blueF()),
        alpha);
}

osg::Vec3 toOverlayLocalVec3(const PointRecord& point, const osg::Vec3d& sceneOrigin)
{
    return osg::Vec3(
        static_cast<float>(point.x - sceneOrigin.x()),
        static_cast<float>(point.y - sceneOrigin.y()),
        static_cast<float>(point.z - sceneOrigin.z()));
}

osg::Vec3 toOverlayLocalVec3(const osg::Vec3d& point, const osg::Vec3d& sceneOrigin)
{
    return osg::Vec3(
        static_cast<float>(point.x() - sceneOrigin.x()),
        static_cast<float>(point.y() - sceneOrigin.y()),
        static_cast<float>(point.z() - sceneOrigin.z()));
}

osg::ref_ptr<osg::Node> wrapOverlayNodeWithSceneOrigin(osg::Node* node, const osg::Vec3d& sceneOrigin)
{
    if (node == nullptr) {
        return nullptr;
    }

    osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform();
    transform->setMatrix(osg::Matrixd::translate(sceneOrigin));
    transform->addChild(node);
    return transform;
}

class RouteCameraPreviewOverlay final : public QWidget
{
public:
    explicit RouteCameraPreviewOverlay(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setFixedSize(kRoutePreviewOverlayWidth, kRoutePreviewOverlayHeight);
    }

    void setPreviewState(
        bool hasPreview,
        const QImage& previewImage,
        const QString& title,
        const QString& subtitle,
        const QString& footer,
        const QString& targetStatus,
        const QString& alignmentHint,
        bool hasTarget,
        bool targetVisible,
        const QPointF& targetNormalizedPoint,
        const QColor& statusColor,
        bool captureFlashActive)
    {
        hasPreview_ = hasPreview;
        previewImage_ = previewImage;
        title_ = title;
        subtitle_ = subtitle;
        footer_ = footer;
        targetStatus_ = targetStatus;
        alignmentHint_ = alignmentHint;
        hasTarget_ = hasTarget;
        targetVisible_ = targetVisible;
        targetNormalizedPoint_ = targetNormalizedPoint;
        statusColor_ = statusColor;
        captureFlashActive_ = captureFlashActive;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF cardRect = rect().adjusted(1, 1, -1, -1);
        painter.setBrush(QColor(255, 255, 255, 238));
        painter.setPen(QPen(QColor(203, 213, 225, 232), 1.2));
        painter.drawRoundedRect(cardRect, 18.0, 18.0);

        const QRectF titleRect(16.0, 14.0, width() - 32.0, 22.0);
        QFont titleFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold);
        painter.setFont(titleFont);
        painter.setPen(QColor(15, 23, 42));
        painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title_);

        const QRectF subtitleRect(16.0, 38.0, width() - 32.0, 18.0);
        QFont subtitleFont(QStringLiteral("Segoe UI"), 9);
        painter.setFont(subtitleFont);
        painter.setPen(QColor(71, 85, 105));
        painter.drawText(subtitleRect, Qt::AlignLeft | Qt::AlignVCenter, subtitle_);

        const QRectF previewRect(16.0, 64.0, width() - 32.0, 156.0);
        painter.setBrush(QColor(9, 16, 27, 244));
        painter.setPen(QPen(QColor(30, 41, 59, 220), 1.0));
        painter.drawRoundedRect(previewRect, 14.0, 14.0);

        if (hasPreview_ && !previewImage_.isNull()) {
            painter.save();
            painter.setClipRect(previewRect.adjusted(1, 1, -1, -1));
            painter.drawImage(previewRect, previewImage_);

            painter.setPen(QPen(QColor(226, 232, 240, 186), 1.0));
            const QPointF center = previewRect.center();
            painter.drawLine(QPointF(center.x() - 18.0, center.y()), QPointF(center.x() + 18.0, center.y()));
            painter.drawLine(QPointF(center.x(), center.y() - 18.0), QPointF(center.x(), center.y() + 18.0));

            if (hasTarget_) {
                if (targetVisible_) {
                    const QPointF targetPoint(
                        previewRect.left() + targetNormalizedPoint_.x() * previewRect.width(),
                        previewRect.top() + targetNormalizedPoint_.y() * previewRect.height());
                    painter.setPen(QPen(QColor(251, 146, 60), 2.0));
                    painter.setBrush(QColor(255, 237, 213, 140));
                    painter.drawEllipse(targetPoint, 6.5, 6.5);
                    painter.drawLine(QPointF(targetPoint.x() - 10.0, targetPoint.y()), QPointF(targetPoint.x() + 10.0, targetPoint.y()));
                    painter.drawLine(QPointF(targetPoint.x(), targetPoint.y() - 10.0), QPointF(targetPoint.x(), targetPoint.y() + 10.0));
                } else {
                    const QRectF badgeRect(previewRect.right() - 116.0, previewRect.top() + 10.0, 104.0, 26.0);
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(statusColor_.isValid() ? statusColor_ : QColor(127, 29, 29, 232));
                    painter.drawRoundedRect(badgeRect, 13.0, 13.0);
                    painter.setPen(QColor(255, 237, 213));
                    painter.drawText(badgeRect, Qt::AlignCenter, targetStatus_);
                }
            }
            painter.restore();
        } else {
            painter.setPen(QColor(203, 213, 225));
            painter.drawText(
                previewRect,
                Qt::AlignCenter,
                targetStatus_.isEmpty()
                    ? QCoreApplication::translate("PointCloudViewer", "Camera preview unavailable")
                    : targetStatus_);
        }

        const QRectF footerRect(16.0, 228.0, width() - 32.0, 18.0);
        painter.setPen(QColor(71, 85, 105));
        painter.drawText(footerRect, Qt::AlignLeft | Qt::AlignVCenter, footer_);

        const QRectF targetRect(16.0, 248.0, width() - 32.0, 16.0);
        painter.setPen(statusColor_.isValid() ? statusColor_ : QColor(30, 64, 175));
        painter.drawText(targetRect, Qt::AlignLeft | Qt::AlignVCenter, targetStatus_);

        const QRectF alignmentRect(16.0, 266.0, width() - 32.0, 16.0);
        painter.setPen(QColor(100, 116, 139));
        painter.drawText(alignmentRect, Qt::AlignLeft | Qt::AlignVCenter, alignmentHint_);

        if (captureFlashActive_) {
            painter.setPen(QPen(QColor(255, 255, 255, 232), 2.2));
            painter.setBrush(QColor(255, 255, 255, 32));
            painter.drawRoundedRect(previewRect.adjusted(1.5, 1.5, -1.5, -1.5), 12.0, 12.0);
        }
    }

private:
    bool hasPreview_ = false;
    QImage previewImage_;
    QString title_;
    QString subtitle_;
    QString footer_;
    QString targetStatus_;
    QString alignmentHint_;
    bool hasTarget_ = false;
    bool targetVisible_ = false;
    QPointF targetNormalizedPoint_;
    QColor statusColor_;
    bool captureFlashActive_ = false;
};

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



QString colorModeLabel(PointCloudColorMode colorMode)
{
    switch (colorMode) {
    case PointCloudColorMode::Elevation:
        return QCoreApplication::translate("PointCloudViewer", "Elevation Ramp");
    case PointCloudColorMode::SingleColor:
        return QCoreApplication::translate("PointCloudViewer", "Single Color");
    case PointCloudColorMode::Classification:
        return QCoreApplication::translate("PointCloudViewer", "Classification");
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

PointRecord minPointRecord()
{
    PointRecord point;
    point.x = point.y = point.z = std::numeric_limits<double>::max();
    return point;
}

PointRecord maxPointRecord()
{
    PointRecord point;
    point.x = point.y = point.z = std::numeric_limits<double>::lowest();
    return point;
}

int percentFromUnit(float value)
{
    return static_cast<int>(std::lround(clampUnit(value) * 100.0f));
}

bool routeLabelModeUsesSequence(RouteLabelDisplayMode mode)
{
    return mode == RouteLabelDisplayMode::Sequence
        || mode == RouteLabelDisplayMode::CompactSequence;
}

bool routeLabelModeHidden(RouteLabelDisplayMode mode)
{
    return mode == RouteLabelDisplayMode::Hidden;
}

bool routeLabelModeCompact(RouteLabelDisplayMode mode)
{
    return mode == RouteLabelDisplayMode::CompactName
        || mode == RouteLabelDisplayMode::CompactSequence;
}

QColor routePreviewBlendColor(const QColor& first, const QColor& second, float factor)
{
    const float clampedFactor = std::clamp(factor, 0.0f, 1.0f);
    const float inverseFactor = 1.0f - clampedFactor;
    return QColor(
        static_cast<int>(std::lround(first.red() * inverseFactor + second.red() * clampedFactor)),
        static_cast<int>(std::lround(first.green() * inverseFactor + second.green() * clampedFactor)),
        static_cast<int>(std::lround(first.blue() * inverseFactor + second.blue() * clampedFactor)));
}

QColor routePreviewColorForElevation(float normalizedHeight)
{
    const QColor lowColor(40, 110, 230);
    const QColor midColor(56, 201, 166);
    const QColor highColor(244, 146, 66);

    if (normalizedHeight <= 0.5f) {
        return routePreviewBlendColor(lowColor, midColor, normalizedHeight * 2.0f);
    }

    return routePreviewBlendColor(midColor, highColor, (normalizedHeight - 0.5f) * 2.0f);
}

int routePreviewEffectiveClassification(
    const PointRecord& point,
    const PointCloudVisualizationOptions& visualizationOptions)
{
    int classification = point.hasClassification ? static_cast<int>(point.classification) : -1;
    if (visualizationOptions.classificationEditStore != nullptr && point.sourceDatasetId >= 0) {
        const auto datasetPathIt = visualizationOptions.classificationDatasetPathsById.constFind(point.sourceDatasetId);
        if (datasetPathIt != visualizationOptions.classificationDatasetPathsById.constEnd()) {
            classification = visualizationOptions.classificationEditStore->effectiveClassification(
                datasetPathIt.value(),
                point.sourcePointIndex,
                classification);
        }
    }

    return classification;
}

bool routePreviewPointVisible(
    const PointRecord& point,
    const PointCloudVisualizationOptions& visualizationOptions)
{
    const bool fallbackVisible = visualizationOptions.classificationVisibility.value(-1, true);
    if (!point.hasClassification) {
        return fallbackVisible;
    }

    return visualizationOptions.classificationVisibility.value(
        routePreviewEffectiveClassification(point, visualizationOptions),
        fallbackVisible);
}

QColor routePreviewPointColor(
    const PointRecord& point,
    const PointCloudVisualizationOptions& visualizationOptions,
    double minZ,
    double heightSpan)
{
    switch (visualizationOptions.colorMode) {
    case PointCloudColorMode::Elevation:
    {
        const double normalizedHeight = heightSpan > 0.0 ? (point.z - minZ) / heightSpan : 0.5;
        QColor color = routePreviewColorForElevation(static_cast<float>(std::clamp(normalizedHeight, 0.0, 1.0)));
        color.setAlpha(255);
        return color;
    }
    case PointCloudColorMode::SingleColor:
    {
        QColor color = visualizationOptions.singleColor;
        color.setAlpha(255);
        return color;
    }
    case PointCloudColorMode::Classification:
    {
        if (!point.hasClassification) {
            QColor color = visualizationOptions.classificationFallbackColor;
            color.setAlpha(255);
            return color;
        }
        const auto colorIt = visualizationOptions.classificationColors.constFind(
            routePreviewEffectiveClassification(point, visualizationOptions));
        QColor color = colorIt != visualizationOptions.classificationColors.constEnd()
            ? colorIt.value()
            : visualizationOptions.classificationFallbackColor;
        color.setAlpha(255);
        return color;
    }
    case PointCloudColorMode::Rgb:
    default:
        return QColor(point.r, point.g, point.b, point.a);
    }
}

QRgb blendRoutePreviewPixel(QRgb backgroundPixel, const QColor& pointColor, float alpha)
{
    const float clampedAlpha = clampUnit(alpha);
    if (clampedAlpha <= 0.0f) {
        return backgroundPixel;
    }
    if (clampedAlpha >= 1.0f) {
        return qRgba(pointColor.red(), pointColor.green(), pointColor.blue(), 255);
    }

    const float inverseAlpha = 1.0f - clampedAlpha;
    const int red = static_cast<int>(std::lround(
        qRed(backgroundPixel) * inverseAlpha + pointColor.red() * clampedAlpha));
    const int green = static_cast<int>(std::lround(
        qGreen(backgroundPixel) * inverseAlpha + pointColor.green() * clampedAlpha));
    const int blue = static_cast<int>(std::lround(
        qBlue(backgroundPixel) * inverseAlpha + pointColor.blue() * clampedAlpha));
    return qRgba(red, green, blue, 255);
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

osg::ref_ptr<osg::Geode> buildInspectionRoutePointsGeode(
    const QList<PointRecord>& waypoints,
    int selectedIndex,
    const osg::Vec4& waypointColor,
    const osg::Vec3d& sceneOrigin)
{
    if (waypoints.isEmpty()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    for (int index = 0; index < waypoints.size(); ++index) {
        const PointRecord& waypoint = waypoints.at(index);
        vertices->push_back(toOverlayLocalVec3(waypoint, sceneOrigin));
        if (index == selectedIndex) {
            colors->push_back(osg::Vec4(0.99f, 0.92f, 0.23f, 1.0f));
        } else {
            colors->push_back(waypointColor);
        }
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
    stateSet->setAttributeAndModes(new osg::Point(11.0f), osg::StateAttribute::ON);
    applyMeasurementForegroundState(stateSet);
    return geode;
}

osg::ref_ptr<osg::Geode> buildInspectionRoutePartPointsGeode(
    const QList<PointRecord>& partPoints,
    const QList<int>& partPointIndices,
    const QSet<int>& secondaryHighlightPartIndices,
    int primaryHighlightPartIndex,
    const osg::Vec4& partPointColor,
    const osg::Vec3d& sceneOrigin)
{
    if (partPoints.isEmpty()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    for (int index = 0; index < partPoints.size(); ++index) {
        const PointRecord& partPoint = partPoints.at(index);
        const int partIndex = index < partPointIndices.size() ? partPointIndices.at(index) : -1;

        vertices->push_back(toOverlayLocalVec3(partPoint, sceneOrigin));
        if (partIndex > 0 && partIndex == primaryHighlightPartIndex) {
            colors->push_back(osg::Vec4(0.98f, 0.50f, 0.11f, 1.0f));
        } else if (partIndex > 0 && secondaryHighlightPartIndices.contains(partIndex)) {
            colors->push_back(osg::Vec4(0.99f, 0.75f, 0.25f, 0.96f));
        } else {
            colors->push_back(partPointColor);
        }
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
    stateSet->setAttributeAndModes(new osg::Point(9.0f), osg::StateAttribute::ON);
    applyMeasurementForegroundState(stateSet);
    return geode;
}

osg::ref_ptr<osg::Geode> buildInspectionRouteLineGeode(
    const QList<PointRecord>& waypoints,
    const osg::Vec4& lineColor,
    const osg::Vec3d& sceneOrigin)
{
    if (waypoints.size() < 2) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    for (const PointRecord& waypoint : waypoints) {
        vertices->push_back(toOverlayLocalVec3(waypoint, sceneOrigin));
    }
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(lineColor);

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_OVERALL);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());
    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::LineWidth(2.6f), osg::StateAttribute::ON);
    applyMeasurementForegroundState(stateSet);
    return geode;
}

osg::ref_ptr<osg::Node> buildInspectionRouteRoamTrackerNode(
    const osg::Vec3d& position,
    const osg::Vec4& color,
    const osg::Vec3d& sceneOrigin)
{
    osg::ref_ptr<osg::Sphere> sphere = new osg::Sphere(osg::Vec3(0.0f, 0.0f, 0.0f), 1.2f);
    osg::ref_ptr<osg::ShapeDrawable> sphereDrawable = new osg::ShapeDrawable(sphere.get());
    sphereDrawable->setColor(color);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(sphereDrawable.get());

    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    applyMeasurementForegroundState(stateSet);

    osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform();
    transform->setMatrix(osg::Matrixd::translate(position - sceneOrigin));
    transform->addChild(geode.get());
    return transform;
}

osg::ref_ptr<osg::Geode> buildInspectionRouteWaypointPartLinksGeode(
    const QList<PointRecord>& waypoints,
    const QList<QList<PointRecord>>& waypointTargetPoints,
    int selectedWaypointIndex,
    int selectedTargetIndex,
    const osg::Vec3d& sceneOrigin)
{
    if (waypoints.isEmpty()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

    for (int index = 0; index < waypoints.size(); ++index) {
        if (index >= waypointTargetPoints.size()) {
            continue;
        }

        const QList<PointRecord>& targets = waypointTargetPoints.at(index);
        if (targets.isEmpty()) {
            continue;
        }

        const PointRecord& waypoint = waypoints.at(index);
        osg::Vec3 startPoint = toOverlayLocalVec3(waypoint, sceneOrigin);
        for (int targetIndex = 0; targetIndex < targets.size(); ++targetIndex) {
            const PointRecord& targetPoint = targets.at(targetIndex);
            osg::Vec3 endPoint = toOverlayLocalVec3(targetPoint, sceneOrigin);
            const osg::Vec3 direction = endPoint - startPoint;
            const float distance = direction.length();
            if (distance <= 0.001f) {
                continue;
            }

            const bool waypointSelected = index == selectedWaypointIndex;
            const bool targetSelected = waypointSelected && targetIndex == selectedTargetIndex;
            const osg::Vec4 linkColor = targetSelected
                ? osg::Vec4(0.95f, 0.27f, 0.63f, 0.94f)
                : waypointSelected
                    ? osg::Vec4(0.86f, 0.62f, 0.97f, 0.86f)
                    : osg::Vec4(0.72f, 0.25f, 0.86f, 0.64f);

            vertices->push_back(startPoint);
            vertices->push_back(endPoint);
            colors->push_back(linkColor);
            colors->push_back(linkColor);
        }
    }

    if (vertices->empty()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());

    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateSet->setMode(GL_LINE_STIPPLE, osg::StateAttribute::ON);
    stateSet->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), osg::StateAttribute::ON);
    stateSet->setAttributeAndModes(new osg::LineStipple(1, 0xAAAA), osg::StateAttribute::ON);
    stateSet->setAttributeAndModes(new osg::LineWidth(1.5f), osg::StateAttribute::ON);
    applyMeasurementForegroundState(stateSet);
    return geode;
}

osg::ref_ptr<osg::Geode> buildInspectionRouteFrustumGeode(
    const QList<PointRecord>& waypoints,
    const QList<QList<PointRecord>>& waypointTargetPoints,
    int selectedWaypointIndex,
    int selectedTargetIndex,
    const osg::Vec3d& sceneOrigin)
{
    if (waypoints.isEmpty()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> edgeVertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> edgeColors = new osg::Vec4Array();
    osg::ref_ptr<osg::Vec3Array> faceVertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> faceColors = new osg::Vec4Array();

    for (int index = 0; index < waypoints.size(); ++index) {
        if (index >= waypointTargetPoints.size()) {
            continue;
        }

        const QList<PointRecord>& targets = waypointTargetPoints.at(index);
        if (targets.isEmpty()) {
            continue;
        }

        const PointRecord& waypoint = waypoints.at(index);
        osg::Vec3 apex = toOverlayLocalVec3(waypoint, sceneOrigin);
        for (int targetIndex = 0; targetIndex < targets.size(); ++targetIndex) {
            const PointRecord& targetPoint = targets.at(targetIndex);
            osg::Vec3 targetLocal = toOverlayLocalVec3(targetPoint, sceneOrigin);
            osg::Vec3 direction = targetLocal - apex;
            const float distance = direction.length();
            if (distance <= 0.001f) {
                continue;
            }

            direction /= distance;
            osg::Vec3 upReference = std::abs(direction.z()) >= 0.92f
                ? osg::Vec3(0.0f, 1.0f, 0.0f)
                : osg::Vec3(0.0f, 0.0f, 1.0f);
            osg::Vec3 right = direction ^ upReference;
            if (right.length2() <= 0.00001f) {
                continue;
            }
            right.normalize();
            osg::Vec3 up = right ^ direction;
            up.normalize();

            const float frustumLength = std::clamp(distance * 0.22f, 1.4f, 7.0f);
            const float halfWidth = std::clamp(frustumLength * 0.18f, 0.35f, 1.25f);
            const float halfHeight = std::clamp(frustumLength * 0.14f, 0.28f, 1.0f);
            const osg::Vec3 baseCenter = apex + direction * frustumLength;
            const std::array<osg::Vec3, 4> corners = {
                baseCenter + right * halfWidth + up * halfHeight,
                baseCenter - right * halfWidth + up * halfHeight,
                baseCenter - right * halfWidth - up * halfHeight,
                baseCenter + right * halfWidth - up * halfHeight
            };

            const bool waypointSelected = index == selectedWaypointIndex;
            const bool targetSelected = waypointSelected && targetIndex == selectedTargetIndex;
            const osg::Vec4 edgeColor = targetSelected
                ? osg::Vec4(0.99f, 0.87f, 0.27f, 0.95f)
                : waypointSelected
                    ? osg::Vec4(0.99f, 0.76f, 0.32f, 0.90f)
                    : osg::Vec4(0.98f, 0.58f, 0.17f, 0.78f);
            const osg::Vec4 faceColor = targetSelected
                ? osg::Vec4(0.99f, 0.87f, 0.27f, 0.18f)
                : waypointSelected
                    ? osg::Vec4(0.99f, 0.76f, 0.32f, 0.14f)
                    : osg::Vec4(0.98f, 0.58f, 0.17f, 0.10f);

            for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
                const osg::Vec3& currentCorner = corners.at(cornerIndex);
                const osg::Vec3& nextCorner = corners.at((cornerIndex + 1) % 4);
                edgeVertices->push_back(apex);
                edgeVertices->push_back(currentCorner);
                edgeVertices->push_back(currentCorner);
                edgeVertices->push_back(nextCorner);
                edgeColors->push_back(edgeColor);
                edgeColors->push_back(edgeColor);
                edgeColors->push_back(edgeColor);
                edgeColors->push_back(edgeColor);

                faceVertices->push_back(apex);
                faceVertices->push_back(currentCorner);
                faceVertices->push_back(nextCorner);
                faceColors->push_back(faceColor);
                faceColors->push_back(faceColor);
                faceColors->push_back(faceColor);
            }
        }
    }

    if (edgeVertices->size() == 0u) {
        return nullptr;
    }

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();

    if (faceVertices->size() > 0u) {
        osg::ref_ptr<osg::Geometry> faceGeometry = new osg::Geometry();
        faceGeometry->setUseDisplayList(false);
        faceGeometry->setUseVertexBufferObjects(true);
        faceGeometry->setVertexArray(faceVertices.get());
        faceGeometry->setColorArray(faceColors.get(), osg::Array::BIND_PER_VERTEX);
        faceGeometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(faceVertices->size())));
        geode->addDrawable(faceGeometry.get());
    }

    osg::ref_ptr<osg::Geometry> edgeGeometry = new osg::Geometry();
    edgeGeometry->setUseDisplayList(false);
    edgeGeometry->setUseVertexBufferObjects(true);
    edgeGeometry->setVertexArray(edgeVertices.get());
    edgeGeometry->setColorArray(edgeColors.get(), osg::Array::BIND_PER_VERTEX);
    edgeGeometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, static_cast<GLsizei>(edgeVertices->size())));
    geode->addDrawable(edgeGeometry.get());

    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateSet->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), osg::StateAttribute::ON);
    stateSet->setAttributeAndModes(new osg::LineWidth(1.6f), osg::StateAttribute::ON);
    applyMeasurementForegroundState(stateSet);
    return geode;
}
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
    createWelcomeOverlay();
    createRouteCameraPreviewOverlay();
    axisIndicatorOverlay_ = new AxisIndicatorOverlay(osgWidget_);
    axisIndicatorOverlay_->show();
    axisIndicatorOverlay_->raise();
    selectionRubberBand_ = new QRubberBand(QRubberBand::Rectangle, osgWidget_);
    selectionRubberBand_->hide();
    profileClassificationPolygonOverlay_ = new PolygonSelectionOverlay(osgWidget_);
    profileClassificationPolygonOverlay_->hide();
    clipPolygonOverlay_ = new PolygonSelectionOverlay(osgWidget_);
    clipPolygonOverlay_->hide();

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

    connect(osgWidget_, &OsgWidget::scenePressed, this, &PointCloudViewer::handleScenePress);
    connect(osgWidget_, &OsgWidget::sceneClicked, this, &PointCloudViewer::handleSceneClick);
    connect(osgWidget_, &OsgWidget::sceneDoubleClicked, this, &PointCloudViewer::handleSceneDoubleClick);
    connect(osgWidget_, &OsgWidget::sceneDragged, this, &PointCloudViewer::handleSceneDrag);
    connect(osgWidget_, &OsgWidget::sceneDragReleased, this, &PointCloudViewer::handleSceneDragRelease);
    connect(osgWidget_, &OsgWidget::sceneEscapePressed, this, &PointCloudViewer::handleSceneEscapePressed);
    connect(osgWidget_, &OsgWidget::sceneSecondaryClicked, this, &PointCloudViewer::handleSceneSecondaryClick);
    connect(osgWidget_, &OsgWidget::sceneHovered, this, &PointCloudViewer::handleSceneHover);
    connect(osgWidget_, &OsgWidget::sceneHoverEnded, this, &PointCloudViewer::clearHoveredPoint);
    connect(osgWidget_, &OsgWidget::selectionRectangleChanged, this, &PointCloudViewer::handleSelectionRectangleChanged);
    connect(osgWidget_, &OsgWidget::selectionRectangleFinished, this, &PointCloudViewer::handleSelectionRectangleFinished);
    connect(osgWidget_, &OsgWidget::selectionEscapePressed, this, &PointCloudViewer::handleSelectionEscapePressed);
    connect(osgWidget_, &OsgWidget::frameRendered, this, &PointCloudViewer::scheduleOverlayWidgetRefresh);
    connect(osgWidget_, &OsgWidget::frameRendered, this, &PointCloudViewer::sceneFrameRendered);
    connect(osgWidget_, &OsgWidget::frameRendered, this, &PointCloudViewer::handleFrameRendered);

    refineIdleTimer_ = new QTimer(this);
    refineIdleTimer_->setSingleShot(true);
    refineIdleTimer_->setInterval(kInteractionLodIdleMilliseconds);
    connect(refineIdleTimer_, &QTimer::timeout, this, [this]() {
        if (osgWidget_ != nullptr && osgWidget_->cameraInteractionActive()) {
            refineIdleTimer_->start();
            return;
        }
        setInteractionLodActive(false);
    });

    classificationTaskStatusTimer_ = new QTimer(this);
    classificationTaskStatusTimer_->setInterval(250);
    connect(classificationTaskStatusTimer_, &QTimer::timeout, this, [this]() {
        if (!profileClassificationTaskActive_) {
            classificationTaskStatusTimer_->stop();
            return;
        }
        updateFooter();
    });

    routeRoamTimer_ = new QTimer(this);
    routeRoamTimer_->setInterval(kRouteRoamTimerIntervalMs);
    connect(routeRoamTimer_, &QTimer::timeout, this, &PointCloudViewer::updateInspectionRouteRoam);

    applyClearColor();
    updateWelcomeOverlayVisibility();
    positionAxisIndicator();
    positionRouteCameraPreviewOverlay();
    retranslateUi();
}

PointCloudViewer::~PointCloudViewer()
{
    cancelAsyncPointCloudLoad();
    if (gaussianLoadThread_.joinable()) {
        gaussianLoadThread_.join();
    }
    if (classificationTaskThread_.joinable()) {
        classificationTaskThread_.join();
    }

    if (osgWidget_ != nullptr) {
        if (osgViewer::Viewer* viewer = osgWidget_->getViewer()) {
            viewer->setDone(true);
        }
    }
}

bool PointCloudViewer::hasPointCloud() const
{
    return visiblePointCount() > 0;
}

bool PointCloudViewer::hasGaussianModel() const
{
    return currentGaussianModel_ != nullptr && !currentGaussianModel_->empty();
}

bool PointCloudViewer::hasRenderableScene() const
{
    return hasPointCloud() || hasGaussianModel();
}

bool PointCloudViewer::hasLoadedPointClouds() const
{
    return !currentFilePaths_.isEmpty();
}

bool PointCloudViewer::isPointCloudLoadingInProgress() const
{
    return pointCloudLoadingActive_;
}

bool PointCloudViewer::canCancelPointCloudLoading() const
{
    return pointCloudLoadingActive_ && pointCloudLoadingCancellable_;
}

QString PointCloudViewer::currentFilePath() const
{
    return currentFilePath_;
}

QStringList PointCloudViewer::currentFilePaths() const
{
    return currentFilePaths_;
}

const PointCloudData* PointCloudViewer::pointCloudData() const
{
    return hasFullResolutionPointCloud() ? ensureFullResolutionPointCloudCache() : nullptr;
}

bool PointCloudViewer::hasFullResolutionPointCloud() const
{
    return hasPointCloud() && !tiledPointCloudModeActive_;
}

const PointCloudData* PointCloudViewer::fullResolutionPointCloudData(QString* errorMessage)
{
    return ensureFullResolutionPointCloudCache(errorMessage);
}

std::size_t PointCloudViewer::visiblePointCount() const
{
    if (loadedPointCloudDatasets_.isEmpty()) {
        return currentPointCloud_ != nullptr ? currentPointCloud_->size() : 0;
    }

    std::size_t count = 0;
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (dataset.info.visible && dataset.pointCloud != nullptr) {
            count += dataset.pointCloud->size();
        }
    }
    return count;
}

bool PointCloudViewer::visiblePointCloudBounds(PointRecord* minBounds, PointRecord* maxBounds) const
{
    if (minBounds == nullptr || maxBounds == nullptr) {
        return false;
    }

    if (loadedPointCloudDatasets_.isEmpty()) {
        if (currentPointCloud_ == nullptr || currentPointCloud_->empty()) {
            return false;
        }
        *minBounds = currentPointCloud_->minBounds();
        *maxBounds = currentPointCloud_->maxBounds();
        return true;
    }

    bool found = false;
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (!dataset.info.visible || dataset.pointCloud == nullptr || dataset.pointCloud->empty()) {
            continue;
        }
        if (!found) {
            *minBounds = dataset.pointCloud->minBounds();
            *maxBounds = dataset.pointCloud->maxBounds();
            found = true;
            continue;
        }
        const PointRecord& datasetMin = dataset.pointCloud->minBounds();
        const PointRecord& datasetMax = dataset.pointCloud->maxBounds();
        minBounds->x = std::min(minBounds->x, datasetMin.x);
        minBounds->y = std::min(minBounds->y, datasetMin.y);
        minBounds->z = std::min(minBounds->z, datasetMin.z);
        maxBounds->x = std::max(maxBounds->x, datasetMax.x);
        maxBounds->y = std::max(maxBounds->y, datasetMax.y);
        maxBounds->z = std::max(maxBounds->z, datasetMax.z);
    }
    return found;
}

const PointCloudData* PointCloudViewer::ensureFullResolutionPointCloudCache(QString* errorMessage) const
{
    if (!hasFullResolutionPointCloud()) {
        if (errorMessage != nullptr) {
            *errorMessage = tiledPointCloudModeActive_
                ? tr("Full-resolution point cloud data is still loading.")
                : tr("No visible point cloud data is available.");
        }
        return nullptr;
    }
    if (mergedPointCloudCache_ != nullptr) {
        return mergedPointCloudCache_.get();
    }
    if (loadedPointCloudDatasets_.isEmpty()) {
        return currentPointCloud_.get();
    }

    std::shared_ptr<PointCloudData> singleDataset;
    int visibleDatasetCount = 0;
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (!dataset.info.visible || dataset.pointCloud == nullptr || dataset.pointCloud->empty()) {
            continue;
        }
        ++visibleDatasetCount;
        if (visibleDatasetCount == 1) {
            singleDataset = dataset.pointCloud;
        } else {
            if (visibleDatasetCount == 2) {
                mergedPointCloudCache_ = std::make_shared<PointCloudData>();
                mergedPointCloudCache_->append(*singleDataset);
            }
            mergedPointCloudCache_->append(*dataset.pointCloud);
        }
    }
    if (visibleDatasetCount == 1) {
        mergedPointCloudCache_ = singleDataset;
    }
    return mergedPointCloudCache_.get();
}

const QList<PointCloudDatasetInfo>& PointCloudViewer::pointCloudDatasets() const
{
    return DataManager::instance().pointCloudDatasets();
}

const PointCloudVisualizationOptions& PointCloudViewer::visualizationOptions() const
{
    return visualizationOptions_;
}

const InteractionOptions& PointCloudViewer::interactionOptions() const
{
    return interactionOptions_;
}

bool PointCloudViewer::hasHoveredPoint() const
{
    return hoveredPointValid_;
}

PointRecord PointCloudViewer::hoveredPoint() const
{
    return hoveredPoint_;
}

void PointCloudViewer::setVisualizationOptions(const PointCloudVisualizationOptions& options)
{
    const bool geometryChanged = visualizationOptions_.colorMode != options.colorMode
        || visualizationOptions_.singleColor != options.singleColor
        || visualizationOptions_.classificationColors != options.classificationColors
        || visualizationOptions_.classificationVisibility != options.classificationVisibility
        || visualizationOptions_.classificationFallbackColor != options.classificationFallbackColor;
    const bool auxiliaryChanged = visualizationOptions_.showAxes != options.showAxes
        || visualizationOptions_.showBoundingBox != options.showBoundingBox;
    const bool backgroundChanged = visualizationOptions_.backgroundColor != options.backgroundColor;

    visualizationOptions_ = options;
    if (!visualizationOptions_.classificationVisibility.contains(-1)) {
        visualizationOptions_.classificationVisibility.insert(-1, true);
    }
    syncVisualizationClassificationState();
    if (backgroundChanged) {
        applyClearColor();
    }
    if (hasPointCloud()) {
        if (geometryChanged || auxiliaryChanged) {
            rebuildScene();
        } else {
            OsgPointCloudNode::updateRenderingState(pointCloudNode_.get(), visualizationOptions_);
            osgWidget_->update();
        }
    }
    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setPointSize(int pointSize)
{
    const float clampedPointSize = std::clamp(static_cast<float>(pointSize), 1.0f, 12.0f);
    if (qFuzzyCompare(visualizationOptions_.pointSize, clampedPointSize)) {
        return;
    }

    visualizationOptions_.pointSize = clampedPointSize;
    OsgPointCloudNode::updateRenderingState(pointCloudNode_.get(), visualizationOptions_);
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
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
    OsgPointCloudNode::updateRenderingState(pointCloudNode_.get(), visualizationOptions_);
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
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
    case 3:
        setColorMode(PointCloudColorMode::Classification);
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
    OsgPointCloudNode::updateRenderingState(pointCloudNode_.get(), visualizationOptions_);
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
    OsgPointCloudNode::updateRenderingState(pointCloudNode_.get(), visualizationOptions_);
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
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
    OsgPointCloudNode::updateRenderingState(pointCloudNode_.get(), visualizationOptions_);
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setUseRoundSplats(bool enabled)
{
    if (visualizationOptions_.useRoundSplats == enabled) {
        return;
    }

    visualizationOptions_.useRoundSplats = enabled;
    OsgPointCloudNode::updateRenderingState(pointCloudNode_.get(), visualizationOptions_);
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
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
        && interactionOptions_.invertWheelZoom == options.invertWheelZoom
        && interactionOptions_.wheelZoomSensitivityPercent == clampWheelZoomSensitivityPercent(options.wheelZoomSensitivityPercent)) {
        return;
    }

    interactionOptions_ = options;
    interactionOptions_.wheelZoomSensitivityPercent =
        clampWheelZoomSensitivityPercent(interactionOptions_.wheelZoomSensitivityPercent);
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

void PointCloudViewer::setWheelZoomSensitivityPercent(int percent)
{
    InteractionOptions options = interactionOptions_;
    options.wheelZoomSensitivityPercent = percent;
    setInteractionOptions(options);
}

void PointCloudViewer::updateSceneClickCapture()
{
    if (osgWidget_ == nullptr) {
        return;
    }

    const bool rectangleSelectionEnabled =
        profileClassificationModeEnabled_
        && profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Rectangle;
    const bool polygonSelectionEnabled =
        profileClassificationModeEnabled_
        && profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon;
    const bool clipSceneClickEnabled = clipEditMode_ != ClipRegion::None;

    osgWidget_->setRectangleSelectionEnabled(rectangleSelectionEnabled);
    const bool sceneClickEnabled =
        clipSceneClickEnabled
        || polygonSelectionEnabled
        || (!profileClassificationModeEnabled_
            && (measurementEnabled_
            || towerEditMode_ != TowerEditMode::None
            || issueEditMode_ != IssueEditMode::None
            || !towerMarkers_.isEmpty()
            || !inspectionIssues_.isEmpty()
            || (inspectionRouteVisible_ && !inspectionRouteWaypoints_.isEmpty())));
    osgWidget_->setSceneClickModeEnabled(sceneClickEnabled);
    updateProfileClassificationPolygonOverlay();
    updateClipPolygonOverlay();
}

bool PointCloudViewer::focusOnPoint(const PointRecord& point, double distanceScale)
{
    if (!hasLoadedPointClouds() || osgWidget_ == nullptr) {
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
    double datasetExtent = 2.0;
    PointRecord visibleMinBounds;
    PointRecord visibleMaxBounds;
    if (visiblePointCloudBounds(&visibleMinBounds, &visibleMaxBounds)) {
        datasetExtent = std::max({
            static_cast<double>(visibleMaxBounds.x - visibleMinBounds.x),
            static_cast<double>(visibleMaxBounds.y - visibleMinBounds.y),
            static_cast<double>(visibleMaxBounds.z - visibleMinBounds.z),
            2.0
        });
    } else {
        for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
            datasetExtent = std::max(datasetExtent, std::max({
                static_cast<double>(dataset.info.maxBounds.x - dataset.info.minBounds.x),
                static_cast<double>(dataset.info.maxBounds.y - dataset.info.minBounds.y),
                static_cast<double>(dataset.info.maxBounds.z - dataset.info.minBounds.z),
                2.0
            }));
        }
    }
    const double minFocusDistance = datasetExtent * std::max(0.05, distanceScale);

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

bool PointCloudViewer::focusOnBounds(const PointRecord& minBounds, const PointRecord& maxBounds, double distanceScale)
{
    PointRecord centerPoint;
    centerPoint.x = (minBounds.x + maxBounds.x) * 0.5f;
    centerPoint.y = (minBounds.y + maxBounds.y) * 0.5f;
    centerPoint.z = (minBounds.z + maxBounds.z) * 0.5f;

    if (!hasLoadedPointClouds() || osgWidget_ == nullptr) {
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
    const double maxExtent = std::max({
        static_cast<double>(maxBounds.x - minBounds.x),
        static_cast<double>(maxBounds.y - minBounds.y),
        static_cast<double>(maxBounds.z - minBounds.z),
        2.0
    });
    const double focusDistance = std::max(2.0, maxExtent * std::max(0.18, distanceScale));
    if (offset.length2() < 1e-8) {
        offset = osg::Vec3d(focusDistance, -focusDistance, focusDistance * 0.6);
    } else {
        offset.normalize();
        offset *= std::max(focusDistance, (eye - center).length() * std::max(0.15, distanceScale));
    }

    const osg::Vec3d target(centerPoint.x, centerPoint.y, centerPoint.z);
    manipulator->setHomePosition(target + offset, target, up.length2() < 1e-8 ? osg::Vec3d(0.0, 0.0, 1.0) : up);
    manipulator->home(0.0);
    osgWidget_->update();
    return true;
}

bool PointCloudViewer::removePointCloudDataset(const QString& filePath)
{
    for (int datasetIndex = 0; datasetIndex < loadedPointCloudDatasets_.size(); ++datasetIndex) {
        if (loadedPointCloudDatasets_.at(datasetIndex).info.filePath.compare(filePath, Qt::CaseInsensitive) != 0) {
            continue;
        }

        loadedPointCloudDatasets_.removeAt(datasetIndex);
        invalidateMergedPointCloudCache();
        for (int pathIndex = currentFilePaths_.size() - 1; pathIndex >= 0; --pathIndex) {
            if (currentFilePaths_.at(pathIndex).compare(filePath, Qt::CaseInsensitive) == 0) {
                currentFilePaths_.removeAt(pathIndex);
            }
        }
        QList<PointCloudDatasetInfo> datasetInfos = DataManager::instance().pointCloudDatasets();
        for (int infoIndex = datasetInfos.size() - 1; infoIndex >= 0; --infoIndex) {
            if (datasetInfos.at(infoIndex).filePath.compare(filePath, Qt::CaseInsensitive) == 0) {
                datasetInfos.removeAt(infoIndex);
            }
        }
        DataManager::instance().setPointCloudDatasets(datasetInfos);
        rebuildMergedPointCloud();
        rebuildScene();
        updateFooter();
        updateWelcomeOverlayVisibility();
        emit pointCloudLoaded();
        return true;
    }
    return false;
}

bool PointCloudViewer::setPointCloudDatasetVisible(const QString& filePath, bool visible)
{
    if (filePath.trimmed().isEmpty()) {
        return false;
    }

    bool changed = false;
    for (int datasetIndex = 0; datasetIndex < loadedPointCloudDatasets_.size(); ++datasetIndex) {
        LoadedPointCloudDataset& dataset = loadedPointCloudDatasets_[datasetIndex];
        if (dataset.info.filePath.compare(filePath, Qt::CaseInsensitive) != 0) {
            continue;
        }

        if (dataset.info.visible == visible) {
            return true;
        }

        dataset.info.visible = visible;
        DataManager::instance().setPointCloudDatasetVisible(filePath, visible);
        changed = true;
        break;
    }

    if (!changed) {
        return false;
    }

    rebuildMergedPointCloud();
    hoveredPointValid_ = false;
    lastHoverQueryPosition_ = QPointF();
    lastHoverQueryTime_ = {};
    rebuildScene();
    updateFooter();
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
    positionRouteCameraPreviewOverlay();
    updateProfileClassificationPolygonOverlay();
    updateWelcomeOverlayVisibility();
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

void PointCloudViewer::createWelcomeOverlay()
{
    welcomeOverlay_ = new QFrame(osgWidget_);
    welcomeOverlay_->setObjectName(QStringLiteral("viewerWelcomeOverlay"));
    welcomeOverlay_->setStyleSheet(QStringLiteral(
        "QFrame#viewerWelcomeOverlay {"
        "background-color: #f4f7fb;"
        "}"));

    auto* overlayLayout = new QVBoxLayout(welcomeOverlay_);
    overlayLayout->setContentsMargins(0, 0, 0, 0);
    overlayLayout->setSpacing(0);

    welcomeWorkspace_ = new WelcomeWorkspaceWidget(welcomeOverlay_);
    overlayLayout->addWidget(welcomeWorkspace_, 1);

    welcomeStatusLabel_ = new QLabel(welcomeOverlay_);
    welcomeStatusLabel_->setAlignment(Qt::AlignCenter);
    welcomeStatusLabel_->setStyleSheet(QStringLiteral(
        "QLabel {"
        "background-color: #f4f7fb;"
        "color: #334155;"
        "font-size: 14px;"
        "font-weight: 600;"
        "padding: 24px;"
        "}"));
    welcomeStatusLabel_->hide();
    overlayLayout->addWidget(welcomeStatusLabel_, 1);

    connect(welcomeWorkspace_, &WelcomeWorkspaceWidget::openProjectRequested,
        this, &PointCloudViewer::welcomeOpenProjectRequested);
    connect(welcomeWorkspace_, &WelcomeWorkspaceWidget::openDataRequested,
        this, &PointCloudViewer::welcomeOpenDataRequested);
    connect(welcomeWorkspace_, &WelcomeWorkspaceWidget::addDataRequested,
        this, &PointCloudViewer::welcomeAddDataRequested);
    connect(welcomeWorkspace_, &WelcomeWorkspaceWidget::recentProjectRequested,
        this, &PointCloudViewer::welcomeRecentProjectRequested);
    connect(welcomeWorkspace_, &WelcomeWorkspaceWidget::recentDataRequested,
        this, &PointCloudViewer::welcomeRecentDataRequested);

    welcomeOverlay_->hide();
}

void PointCloudViewer::createRouteCameraPreviewOverlay()
{
    routeCameraPreviewOverlay_ = new RouteCameraPreviewOverlay(osgWidget_);
    routeCameraPreviewOverlay_->hide();
}

void PointCloudViewer::setLoadingState(bool active, const QString& title, const QString& detail, int progressPercent)
{
    pointCloudLoadingActive_ = active;
    if (!active) {
        pointCloudLoadingCancellable_ = false;
    }
    pointCloudLoadingTitle_ = title;
    pointCloudLoadingDetail_ = detail;
    pointCloudLoadingProgressPercent_ = progressPercent;

    if (active) {
        updateMessage(title, detail);
    } else {
        pointCloudLoadingTitle_.clear();
        pointCloudLoadingDetail_.clear();
        pointCloudLoadingProgressPercent_ = -1;
    }

    updateWelcomeOverlayVisibility();
}

void PointCloudViewer::updateWelcomeOverlayVisibility()
{
    if (welcomeOverlay_ == nullptr || osgWidget_ == nullptr || welcomeWorkspace_ == nullptr) {
        return;
    }

    welcomeOverlay_->setGeometry(osgWidget_->rect());

    if (hasLoadedPointClouds() || (pointCloudLoadingActive_ && hasPointCloud())) {
        welcomeOverlay_->hide();
        if (axisIndicatorOverlay_ != nullptr && visualizationOptions_.showAxes) {
            axisIndicatorOverlay_->raise();
        }
        return;
    }

    const bool showLoading = pointCloudLoadingActive_ && !pointCloudLoadingDetail_.trimmed().isEmpty();
    welcomeWorkspace_->setVisible(!showLoading);
    if (welcomeStatusLabel_ != nullptr) {
        if (showLoading) {
            const QString loadingText = pointCloudLoadingProgressPercent_ >= 0
                ? tr("%1\n%2")
                      .arg(pointCloudLoadingTitle_.trimmed().isEmpty() ? tr("Loading point cloud") : pointCloudLoadingTitle_)
                      .arg(pointCloudLoadingDetail_)
                : pointCloudLoadingDetail_;
            welcomeStatusLabel_->setText(loadingText);
            welcomeStatusLabel_->show();
        } else {
            welcomeStatusLabel_->hide();
            welcomeStatusLabel_->clear();
        }
    }

    welcomeOverlay_->show();
    welcomeOverlay_->raise();
}

void PointCloudViewer::rebuildScene(bool reusePreparedDatasetNodes)
{
    if (!rootGroup_.valid()) {
        return;
    }

    updateSceneOriginFromCurrentPointCloud();

    rootGroup_->removeChildren(0, rootGroup_->getNumChildren());
    pointCloudNode_ = nullptr;
    towerMarkersNode_ = nullptr;
    inspectionIssuesNode_ = nullptr;
    inspectionRouteNode_ = nullptr;
    measurementOverlayNode_ = nullptr;
    clipOverlayNode_ = nullptr;

    if (!hasPointCloud()) {
        if (osgWidget_ != nullptr) {
            osgWidget_->update();
        }
        updateTowerOverlayWidgets();
        updateInspectionIssueOverlayWidgets();
        updateInspectionRouteOverlayWidgets();
        updateRouteCameraPreviewOverlay();
        updateMeasurementOverlayWidgets();
        return;
    }

    PointRecord sceneMinBounds;
    PointRecord sceneMaxBounds;
    const bool hasSceneBounds = visiblePointCloudBounds(&sceneMinBounds, &sceneMaxBounds);
    PointCloudVisualizationOptions datasetVisualizationOptions = visualizationOptions_;
    datasetVisualizationOptions.showAxes = false;
    datasetVisualizationOptions.showBoundingBox = false;
    datasetVisualizationOptions.sharedElevationRangeValid = hasSceneBounds;
    datasetVisualizationOptions.sharedElevationMin = hasSceneBounds ? sceneMinBounds.z : 0.0;
    datasetVisualizationOptions.sharedElevationMax = hasSceneBounds ? sceneMaxBounds.z : 0.0;

    osg::ref_ptr<osg::Group> pointCloudGroup = new osg::Group();
    if (loadedPointCloudDatasets_.isEmpty() && currentPointCloud_ != nullptr && !currentPointCloud_->empty()) {
        osg::ref_ptr<osg::Node> previewNode = OsgPointCloudNode::build(*currentPointCloud_, datasetVisualizationOptions);
        if (previewNode.valid()) {
            pointCloudGroup->addChild(previewNode.get());
        }
    }
    for (LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (!dataset.info.visible || dataset.pointCloud == nullptr || dataset.pointCloud->empty()) {
            dataset.sceneNode = nullptr;
            continue;
        }
        if (!reusePreparedDatasetNodes || !dataset.fullSceneNode.valid()) {
            dataset.fullSceneNode = OsgPointCloudNode::build(*dataset.pointCloud, datasetVisualizationOptions);
        }
        if (!reusePreparedDatasetNodes || (dataset.interactionPreview != nullptr && !dataset.previewSceneNode.valid())) {
            dataset.previewSceneNode = dataset.interactionPreview != nullptr
                ? OsgPointCloudNode::build(*dataset.interactionPreview, datasetVisualizationOptions)
                : nullptr;
        }
        dataset.sceneNode = new osg::Group();
        if (dataset.fullSceneNode.valid()) {
            dataset.fullSceneNode->setNodeMask(cameraMoving_ && dataset.previewSceneNode.valid() ? 0u : ~0u);
            dataset.sceneNode->addChild(dataset.fullSceneNode.get());
        }
        if (dataset.previewSceneNode.valid()) {
            dataset.previewSceneNode->setNodeMask(cameraMoving_ ? ~0u : 0u);
            dataset.sceneNode->addChild(dataset.previewSceneNode.get());
        }
        if (dataset.sceneNode->getNumChildren() > 0) {
            pointCloudGroup->addChild(dataset.sceneNode.get());
        }
    }
    pointCloudNode_ = pointCloudGroup;
    if (pointCloudGroup->getNumChildren() > 0) {
        rootGroup_->addChild(pointCloudGroup.get());
    }
    if (hasSceneBounds) {
        osg::ref_ptr<osg::Group> auxiliaryNodes = OsgPointCloudNode::buildAuxiliaryNodes(
            sceneMinBounds,
            sceneMaxBounds,
            visualizationOptions_);
        if (auxiliaryNodes.valid() && auxiliaryNodes->getNumChildren() > 0) {
            rootGroup_->addChild(auxiliaryNodes.get());
        }
    }

    refreshTowerMarkersOverlay();
    refreshInspectionIssuesOverlay();
    refreshInspectionRouteOverlay();
    refreshMeasurementOverlay();
    refreshClipOverlay();
}

void PointCloudViewer::rebuildMergedPointCloud()
{
    invalidateMergedPointCloudCache();
    currentPointCloud_.reset();
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (!dataset.info.visible || dataset.pointCloud == nullptr || dataset.pointCloud->empty()) {
            continue;
        }
        currentPointCloud_ = dataset.pointCloud;
        break;
    }
    syncCurrentFilePath();
}

void PointCloudViewer::invalidateMergedPointCloudCache()
{
    mergedPointCloudCache_.reset();
}

void PointCloudViewer::updateSceneOriginFromCurrentPointCloud()
{
    sceneOriginValid_ = false;
    sceneOriginWorld_.set(0.0, 0.0, 0.0);

    PointRecord minBounds;
    PointRecord maxBounds;
    if (!visiblePointCloudBounds(&minBounds, &maxBounds)) {
        return;
    }
    sceneOriginWorld_.set(
        (minBounds.x + maxBounds.x) * 0.5,
        (minBounds.y + maxBounds.y) * 0.5,
        (minBounds.z + maxBounds.z) * 0.5);
    sceneOriginValid_ = true;
}

osg::Vec3d PointCloudViewer::overlaySceneOrigin() const
{
    return sceneOriginValid_ ? sceneOriginWorld_ : osg::Vec3d(0.0, 0.0, 0.0);
}

void PointCloudViewer::updateFooter()
{
    if (!hasLoadedPointClouds()) {
        updateMessage(
            tr("Ready for point cloud inspection"),
            tr("Open one or more LAS or LAZ files. Left drag orbits, middle or right drag pans, and the mouse wheel zooms."));
        if (cursorLabel_ != nullptr) {
            cursorLabel_->setText(tr("Cursor Point: N/A"));
        }
        return;
    }

    if (!hasPointCloud()) {
        if (hasGaussianModel()) {
            const QFileInfo fileInfo(currentFilePath_);
            updateMessage(
                fileInfo.fileName().isEmpty() ? currentFilePath_ : fileInfo.fileName(),
                tr("%1 Gaussian splats | GPU rasterization | Local origin offset %2")
                    .arg(formatPointCount(currentGaussianModel_->size()))
                    .arg(formatTriplet(
                        currentGaussianModel_->fileOffset.x(),
                        currentGaussianModel_->fileOffset.y(),
                        currentGaussianModel_->fileOffset.z())));
            if (cursorLabel_ != nullptr) {
                cursorLabel_->setText(tr("Cursor Point: N/A"));
            }
            return;
        }
        updateMessage(
            tr("All point cloud datasets are hidden"),
            tr("Enable one or more datasets in the project explorer to continue browsing, measuring, or editing."));
        if (cursorLabel_ != nullptr) {
            cursorLabel_->setText(tr("Cursor Point: N/A"));
        }
        return;
    }

    const QFileInfo fileInfo(currentFilePath_);
    const QString title = currentFilePaths_.size() > 1
        ? tr("%1 datasets loaded").arg(QLocale().toString(currentFilePaths_.size()))
        : (fileInfo.fileName().isEmpty() ? currentFilePath_ : fileInfo.fileName());
    QString detail = tr("%1 points | Datasets %2 | %3 | %4 px | Axes %5 | Bounds %6")
        .arg(formatPointCount(visiblePointCount()))
        .arg(QLocale().toString(currentFilePaths_.size()))
        .arg(colorModeLabel(visualizationOptions_.colorMode))
        .arg(QLocale().toString(static_cast<int>(visualizationOptions_.pointSize)))
        .arg(visualizationOptions_.showAxes ? tr("on") : tr("off"))
        .arg(visualizationOptions_.showBoundingBox ? tr("on") : tr("off"));
    detail += tr(" | Towers %1").arg(QLocale().toString(towerMarkers_.size()));
    detail += tr(" | Issues %1").arg(QLocale().toString(inspectionIssues_.size() - hiddenInspectionIssueIndices_.size()));
    detail += tr(" | Route WPs %1").arg(QLocale().toString(inspectionRouteVisible_ ? inspectionRouteWaypoints_.size() : 0));
    if (inspectionRouteRoamActive()) {
        const QString roamState = inspectionRouteRoamPlaying() ? tr("playing") : tr("paused");
        detail += tr(" | Roam %1 @ %2 m/s")
            .arg(roamState)
            .arg(QLocale().toString(inspectionRouteRoamSpeedMetersPerSecond_, 'f', 1));
        detail += tr(" | Photos %1")
            .arg(QLocale().toString(inspectionRouteRoamCaptureCount_));
    }

    if (tiledPointCloudModeActive_) {
        detail += tr(" | Preview mode: full resolution is still loading");
    }

    if (measurementEnabled_) {
        if (measurementResult_.isComplete()) {
            detail += tr(" | Measure %1 over %2 pts | ΔZ %3")
                .arg(formatCoordinate(measurementResult_.distance3d))
                .arg(QLocale().toString(measurementResult_.pointCount()))
                .arg(formatCoordinate(measurementResult_.deltaZ));
        } else if (measurementResult_.hasStartPoint) {
            detail += tr(" | Measurement: pick the next point, right-click to undo");
        } else {
            detail += tr(" | Measurement: pick the first point");
        }
    }

    if (profileClassificationModeEnabled_) {
        const QString modeText =
            profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
                ? tr("polygon")
                : tr("rectangle");
        if (profileClassificationTaskActive_) {
            detail += tr(" | Profile classify (%1): processing").arg(modeText);
            const std::uint64_t scannedPointCount = classificationTaskScannedPoints_.load();
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = classificationTaskStartTime_.time_since_epoch().count() == 0
                ? std::chrono::milliseconds(0)
                : std::chrono::duration_cast<std::chrono::milliseconds>(now - classificationTaskStartTime_);
            detail += QStringLiteral(" (%1, %2ms)")
                .arg(QLocale().toString(static_cast<qlonglong>(scannedPointCount)))
                .arg(QLocale().toString(static_cast<qlonglong>(elapsed.count())));
        } else {
            detail += tr(" | Profile classify (%1): source %2 -> target %3 | edits %4")
                .arg(modeText)
                .arg(QLocale().toString(profileClassificationSourceClasses_.size()))
                .arg(QLocale().toString(profileClassificationTargetClass_))
                .arg(QLocale().toString(classificationEditStore_.editedPointCount()));
            if (profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
                && !profileClassificationPolygonPoints_.isEmpty()) {
                detail += tr(" | vertices %1")
                    .arg(QLocale().toString(profileClassificationPolygonPoints_.size()));
            }
            if (lastClassificationTaskElapsedMilliseconds_ > 0) {
                detail += QStringLiteral(" (%1, %2ms)")
                    .arg(QLocale().toString(static_cast<qlonglong>(lastClassificationTaskScannedPoints_)))
                    .arg(QLocale().toString(static_cast<qlonglong>(lastClassificationTaskElapsedMilliseconds_)));
            }
        }
    }

    if (issueEditMode_ == IssueEditMode::Add) {
        detail += tr(" | Issue marking: click a point to add an issue, right-click to cancel");
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
    if (!hasRenderableScene() || osgWidget_ == nullptr) {
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

    PointRecord gaussianMinBounds;
    PointRecord gaussianMaxBounds;
    if (hasGaussianModel()) {
        gaussianMinBounds.x = currentGaussianModel_->minBounds.x();
        gaussianMinBounds.y = currentGaussianModel_->minBounds.y();
        gaussianMinBounds.z = currentGaussianModel_->minBounds.z();
        gaussianMaxBounds.x = currentGaussianModel_->maxBounds.x();
        gaussianMaxBounds.y = currentGaussianModel_->maxBounds.y();
        gaussianMaxBounds.z = currentGaussianModel_->maxBounds.z();
    }
    PointRecord pointCloudMinBounds;
    PointRecord pointCloudMaxBounds;
    const bool hasVisibleBounds = visiblePointCloudBounds(&pointCloudMinBounds, &pointCloudMaxBounds);
    const PointRecord& minBounds = hasVisibleBounds ? pointCloudMinBounds : gaussianMinBounds;
    const PointRecord& maxBounds = hasVisibleBounds ? pointCloudMaxBounds : gaussianMaxBounds;
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
    if (clipEditMode_ == ClipRegion::ScreenPolygonPrism) {
        if (!clipLockedCamera_.valid) {
            clipLockedCamera_.valid = captureClipCameraSnapshot(
                &clipLockedCamera_.eye,
                &clipLockedCamera_.forward,
                &clipLockedCamera_.right,
                &clipLockedCamera_.up,
                &clipLockedCamera_.windowToWorld);
            if (!clipLockedCamera_.valid) {
                emit measurementMessage(tr("Unable to capture the current camera for polygon clipping."), true);
                return;
            }
        }

        if (clipPolygonPoints_.isEmpty()
            || std::hypot(
                localPos.x() - clipPolygonPoints_.constLast().x(),
                localPos.y() - clipPolygonPoints_.constLast().y()) >= 2.0) {
            clipPolygonPoints_.append(localPos);
        }
        clipPolygonPreviewPoint_ = localPos;
        clipPolygonPreviewActive_ = true;
        updateClipPolygonOverlay();
        emit measurementMessage(
            clipPolygonPoints_.size() >= 3
                ? tr("Clip polygon vertex %1 added. Double-click to apply, right-click to undo.")
                    .arg(QLocale().toString(clipPolygonPoints_.size()))
                : tr("Clip polygon vertex %1 added. Add at least %2 vertices to apply.")
                    .arg(QLocale().toString(clipPolygonPoints_.size()))
                    .arg(QLocale().toString(3)),
            false);
        return;
    }

    if (clipEditMode_ == ClipRegion::Box) {
        PointRecord pickedPoint;
        if (!pickPointAtScreenPosition(localPos, &pickedPoint)) {
            emit measurementMessage(tr("No point was found near the clicked position."), true);
            return;
        }

        if (!clipBoxFirstPointValid_) {
            clipBoxFirstPoint_ = pickedPoint;
            clipBoxFirstPointValid_ = true;
            if (clipBoxAlignment_ == ClipRegion::ViewAligned) {
                clipLockedCamera_.valid = captureClipCameraSnapshot(
                    &clipLockedCamera_.eye,
                    &clipLockedCamera_.forward,
                    &clipLockedCamera_.right,
                    &clipLockedCamera_.up,
                    &clipLockedCamera_.windowToWorld);
                if (!clipLockedCamera_.valid) {
                    clipBoxFirstPointValid_ = false;
                    emit measurementMessage(tr("Unable to capture the current camera for the view-aligned box clip."), true);
                    return;
                }
            }
            emit measurementMessage(
                tr("Box first corner selected. Hover another point to preview the box, then left-click to confirm."),
                false);
            return;
        }

        ClipRegion clipRegion;
        if (!buildBoxClipRegion(clipBoxFirstPoint_, pickedPoint, clipBoxAlignment_, &clipRegion)) {
            emit measurementMessage(tr("Unable to build the clip box from the selected points."), true);
            return;
        }

        setClipRegion(clipRegion);
        resetClipEditingState();
        updateSceneClickCapture();
        updateClipPolygonOverlay();
        emit measurementMessage(
            clipBoxAlignment_ == ClipRegion::ViewAligned
                ? tr("View-aligned box clip applied.")
                : tr("World-aligned box clip applied."),
            false);
        return;
    }

    if (profileClassificationModeEnabled_) {
        if (profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
            && !profileClassificationTaskActive_) {
            profileClassificationPolygonPoints_.append(localPos);
            profileClassificationPolygonPreviewPoint_ = localPos;
            profileClassificationPolygonPreviewActive_ = true;
            updateProfileClassificationPolygonOverlay();
            emit profileClassificationStateChanged();
            emit measurementMessage(
                profileClassificationPolygonPoints_.size() >= 3
                    ? tr("Polygon vertex %1 added. Double-click to apply, right-click to undo one vertex.")
                        .arg(QLocale().toString(profileClassificationPolygonPoints_.size()))
                    : tr("Polygon vertex %1 added. Add at least %2 vertices to apply.")
                        .arg(QLocale().toString(profileClassificationPolygonPoints_.size()))
                        .arg(QLocale().toString(3)),
                false);
        }
        return;
    }

    if (towerEditMode_ == TowerEditMode::None
        && issueEditMode_ == IssueEditMode::None
        && !measurementEnabled_
        && towerMarkers_.isEmpty()
        && inspectionIssues_.isEmpty()
        && inspectionRouteWaypoints_.isEmpty()) {
        return;
    }

    if (towerEditMode_ == TowerEditMode::None && issueEditMode_ == IssueEditMode::None && !measurementEnabled_) {
        const int pickedIssueIndex = pickInspectionIssueAtScreenPosition(localPos);
        if (pickedIssueIndex >= 0) {
            setSelectedIssueIndex(pickedIssueIndex);
            return;
        }

        const int pickedTowerIndex = pickTowerMarkerAtScreenPosition(localPos);
        if (pickedTowerIndex >= 0) {
            setSelectedTowerIndex(pickedTowerIndex);
            return;
        }

        const int pickedRouteWaypointIndex = pickInspectionRouteWaypointAtScreenPosition(localPos);
        if (pickedRouteWaypointIndex >= 0) {
            setSelectedInspectionRouteWaypointIndex(pickedRouteWaypointIndex);
            return;
        }

        setSelectedTowerIndex(-1);
        setSelectedInspectionRouteWaypointIndex(-1);
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
        emit towerEditRequested(pickedPoint, static_cast<int>(requestedMode), targetIndex);
        return;
    }

    if (issueEditMode_ == IssueEditMode::Add) {
        emit issueEditRequested(pickedPoint);
        return;
    }

    measurementResult_.points.append(pickedPoint);
    recalculateMeasurementResult();

    if (measurementResult_.pointCount() == 1) {
        emit measurementMessage(tr("First point selected. Click the next point to continue measuring."), false);
    } else {
        emit measurementMessage(
            tr("Measured %1 segment(s), total distance %2, height delta %3. Right-click to undo the last point.")
                .arg(QLocale().toString(measurementResult_.pointCount() - 1))
                .arg(formatCoordinate(measurementResult_.distance3d))
                .arg(formatCoordinate(measurementResult_.deltaZ)),
            false);
    }

    refreshMeasurementOverlay();
    updateFooter();
    emit measurementChanged();
}

void PointCloudViewer::handleScenePress(const QPointF& localPos)
{
    if (osgWidget_ == nullptr) {
        return;
    }

    osgWidget_->setSceneDragCaptureEnabled(false);
    osgWidget_->unsetCursor();
    routeWaypointDragActive_ = false;
    routeWaypointDragIndex_ = -1;
    routeWaypointDragPreviewValid_ = false;

    if (clipEditMode_ != ClipRegion::None
        || profileClassificationModeEnabled_
        || measurementEnabled_
        || towerEditMode_ != TowerEditMode::None
        || issueEditMode_ != IssueEditMode::None
        || !inspectionRouteEditingEnabled_
        || !inspectionRouteVisible_
        || inspectionRouteWaypoints_.isEmpty()) {
        return;
    }

    const int pickedRouteWaypointIndex = pickInspectionRouteWaypointAtScreenPosition(localPos);
    if (pickedRouteWaypointIndex < 0) {
        return;
    }

    routeWaypointDragIndex_ = pickedRouteWaypointIndex;
    routeWaypointDragAnchor_ = localPos;
    routeWaypointDragPreviewPoint_ = inspectionRouteWaypoints_.at(pickedRouteWaypointIndex);
    routeWaypointDragPreviewValid_ = true;
    setSelectedInspectionRouteWaypointIndex(pickedRouteWaypointIndex);
    osgWidget_->setSceneDragCaptureEnabled(true);
    osgWidget_->setCursor(Qt::OpenHandCursor);
}

void PointCloudViewer::handleSceneDoubleClick(const QPointF& localPos)
{
    if (clipEditMode_ == ClipRegion::ScreenPolygonPrism) {
        if (clipPolygonPoints_.isEmpty()
            || std::hypot(
                localPos.x() - clipPolygonPoints_.constLast().x(),
                localPos.y() - clipPolygonPoints_.constLast().y()) >= 2.0) {
            clipPolygonPoints_.append(localPos);
        }

        if (clipPolygonPoints_.size() < 3) {
            emit measurementMessage(tr("Add at least three polygon vertices before applying the clip."), true);
            return;
        }

        ClipRegion clipRegion;
        if (!clipLockedCamera_.valid
            || !buildScreenPolygonClipRegion(
                clipPolygonPoints_,
                clipLockedCamera_.eye,
                clipLockedCamera_.forward,
                clipLockedCamera_.windowToWorld,
                &clipRegion)) {
            emit measurementMessage(tr("Unable to build the clip volume from the drawn polygon."), true);
            return;
        }

        setClipRegion(clipRegion);
        resetClipEditingState();
        updateSceneClickCapture();
        updateClipPolygonOverlay();
        emit measurementMessage(tr("Polygon clip applied."), false);
        return;
    }

    if (profileClassificationModeEnabled_) {
        if (profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
            && !profileClassificationTaskActive_) {
            if (profileClassificationPolygonPoints_.size() < 3) {
                emit measurementMessage(tr("Add at least three polygon vertices before applying profile classification."), true);
            } else {
                tryFinishProfileClassificationPolygonSelection();
            }
        }
        return;
    }

    if (measurementEnabled_
        || towerEditMode_ != TowerEditMode::None
        || issueEditMode_ != IssueEditMode::None
        || !inspectionRouteEditingEnabled_) {
        return;
    }

    const int pickedRouteWaypointIndex = pickInspectionRouteWaypointAtScreenPosition(localPos);
    if (pickedRouteWaypointIndex < 0) {
        return;
    }

    setSelectedInspectionRouteWaypointIndex(pickedRouteWaypointIndex);
    emit inspectionRouteWaypointDoubleClicked(pickedRouteWaypointIndex);
}

void PointCloudViewer::handleSceneDrag(const QPointF& localPos)
{
    if (routeWaypointDragIndex_ < 0 || routeWaypointDragIndex_ >= inspectionRouteWaypoints_.size()) {
        return;
    }

    const QPointF dragDelta = localPos - routeWaypointDragAnchor_;
    if (!routeWaypointDragActive_ && std::hypot(dragDelta.x(), dragDelta.y()) < 4.0) {
        return;
    }

    PointRecord pickedPoint;
    if (!pickPointAtScreenPosition(localPos, &pickedPoint, 18.0f)) {
        return;
    }

    routeWaypointDragActive_ = true;
    routeWaypointDragPreviewPoint_ = pickedPoint;
    routeWaypointDragPreviewValid_ = true;
    if (osgWidget_ != nullptr) {
        osgWidget_->setCursor(Qt::ClosedHandCursor);
    }
    refreshInspectionRouteOverlay();
}

void PointCloudViewer::handleSceneDragRelease(const QPointF& localPos)
{
    Q_UNUSED(localPos);

    if (osgWidget_ != nullptr) {
        osgWidget_->setSceneDragCaptureEnabled(false);
        osgWidget_->unsetCursor();
    }

    const int draggedWaypointIndex = routeWaypointDragIndex_;
    const bool shouldCommit =
        routeWaypointDragActive_
        && routeWaypointDragPreviewValid_
        && draggedWaypointIndex >= 0
        && draggedWaypointIndex < inspectionRouteWaypoints_.size();
    const PointRecord draggedPoint = routeWaypointDragPreviewPoint_;

    routeWaypointDragActive_ = false;
    routeWaypointDragIndex_ = -1;
    routeWaypointDragPreviewValid_ = false;
    refreshInspectionRouteOverlay();

    if (shouldCommit) {
        emit inspectionRouteWaypointDragFinished(draggedWaypointIndex, draggedPoint);
    }
}

void PointCloudViewer::handleSceneEscapePressed()
{
    if (osgWidget_ != nullptr) {
        osgWidget_->setSceneDragCaptureEnabled(false);
        osgWidget_->unsetCursor();
    }

    if (clipEditMode_ != ClipRegion::None) {
        const bool hadPendingClipState =
            !clipPolygonPoints_.isEmpty()
            || clipBoxFirstPointValid_
            || clipPreviewActive_;
        resetClipEditingState();
        clearClipPreview();
        updateSceneClickCapture();
        updateClipPolygonOverlay();
        refreshClipOverlay();
        if (hadPendingClipState) {
            emit measurementMessage(tr("Clip editing cancelled."), false);
        }
        return;
    }

    if (profileClassificationModeEnabled_
        && profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
        && !profileClassificationTaskActive_) {
        if (!profileClassificationPolygonPoints_.isEmpty()) {
            clearProfileClassificationPolygonSelection();
            emit profileClassificationStateChanged();
            emit measurementMessage(tr("Polygon selection cleared."), false);
        } else {
            setProfileClassificationModeEnabled(false);
        }
        return;
    }

    if (routeWaypointDragActive_ || routeWaypointDragPreviewValid_) {
        routeWaypointDragActive_ = false;
        routeWaypointDragIndex_ = -1;
        routeWaypointDragPreviewValid_ = false;
        refreshInspectionRouteOverlay();
        emit measurementMessage(tr("Route waypoint move cancelled."), false);
    }
}

void PointCloudViewer::handleSceneSecondaryClick(const QPointF& localPos)
{
    Q_UNUSED(localPos);

    if (clipEditMode_ == ClipRegion::ScreenPolygonPrism) {
        if (!clipPolygonPoints_.isEmpty()) {
            clipPolygonPoints_.removeLast();
            if (clipPolygonPoints_.isEmpty()) {
                clipPolygonPreviewPoint_ = QPointF();
                clipPolygonPreviewActive_ = false;
                clipLockedCamera_ = ClipCameraSnapshot();
            }
            updateClipPolygonOverlay();
            emit measurementMessage(
                clipPolygonPoints_.isEmpty()
                    ? tr("Clip polygon vertex undone. Selection is now empty.")
                    : tr("Clip polygon vertex undone. %1 vertex/vertices remain.")
                        .arg(QLocale().toString(clipPolygonPoints_.size())),
                false);
        } else {
            emit measurementMessage(tr("No clip polygon vertex to undo."), true);
        }
        return;
    }

    if (clipEditMode_ == ClipRegion::Box) {
        if (clipBoxFirstPointValid_) {
            clipBoxFirstPointValid_ = false;
            clipLockedCamera_ = ClipCameraSnapshot();
            clearClipPreview();
            refreshClipOverlay();
            emit measurementMessage(tr("Box first corner cleared. Click the first corner point to start again."), false);
        } else {
            emit measurementMessage(tr("No clip box corner to undo."), true);
        }
        return;
    }

    if (profileClassificationModeEnabled_) {
        if (profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon) {
            if (profileClassificationTaskActive_) {
                return;
            }
            if (!profileClassificationPolygonPoints_.isEmpty()) {
                profileClassificationPolygonPoints_.removeLast();
                if (profileClassificationPolygonPoints_.isEmpty()) {
                    profileClassificationPolygonPreviewPoint_ = QPointF();
                    profileClassificationPolygonPreviewActive_ = false;
                } else {
                    profileClassificationPolygonPreviewPoint_ = profileClassificationPolygonPoints_.constLast();
                    profileClassificationPolygonPreviewActive_ = true;
                }
                updateProfileClassificationPolygonOverlay();
                emit profileClassificationStateChanged();
                emit measurementMessage(
                    profileClassificationPolygonPoints_.isEmpty()
                        ? tr("Polygon vertex undone. Selection is now empty.")
                        : tr("Polygon vertex undone. %1 vertex/vertices remain.")
                            .arg(QLocale().toString(profileClassificationPolygonPoints_.size())),
                    false);
            } else {
                emit measurementMessage(
                    tr("No polygon vertex to undo. Left-click to add vertices, double-click to apply."),
                    true);
            }
            return;
        }

        if (profileClassificationSelectionActive_) {
            clearSelectionRubberBand();
            emit measurementMessage(tr("Profile classification selection cancelled."), false);
        } else {
            setProfileClassificationModeEnabled(false);
        }
        return;
    }

    if (towerEditMode_ == TowerEditMode::AddAfterLast) {
        if (towerMarkers_.size() > towerAddModeStartCount_) {
            if (removeTowerMarker(towerMarkers_.size() - 1)) {
                emit measurementMessage(tr("Tower marker removed."), false);
            }
        } else {
            cancelTowerEditMode();
            emit measurementMessage(tr("Tower tool cancelled."), false);
        }
        return;
    }

    if (towerEditMode_ != TowerEditMode::None) {
        cancelTowerEditMode();
        emit measurementMessage(tr("Tower tool cancelled."), false);
        return;
    }

    if (issueEditMode_ == IssueEditMode::Add) {
        cancelIssueEditMode();
        emit measurementMessage(tr("Issue marking cancelled."), false);
        return;
    }

    if (!measurementEnabled_) {
        return;
    }

    if (!undoLastMeasurementPoint()) {
        emit measurementMessage(tr("No measurement point to undo."), true);
        return;
    }

    if (measurementResult_.pointCount() >= 2) {
        emit measurementMessage(
            tr("Measurement point removed. %1 segment(s) remain, total distance %2.")
                .arg(QLocale().toString(measurementResult_.pointCount() - 1))
                .arg(formatCoordinate(measurementResult_.distance3d)),
            false);
    } else if (measurementResult_.hasStartPoint) {
        emit measurementMessage(tr("Measurement point removed. Click the next point to continue measuring."), false);
    } else {
        emit measurementMessage(tr("Measurement points cleared."), false);
    }
}

void PointCloudViewer::handleSceneHover(const QPointF& localPos)
{
    if (clipEditMode_ == ClipRegion::ScreenPolygonPrism) {
        if (!clipPolygonPoints_.isEmpty()) {
            clipPolygonPreviewPoint_ = localPos;
            clipPolygonPreviewActive_ = true;
            updateClipPolygonOverlay();
        }
        return;
    }

    if (clipEditMode_ == ClipRegion::Box) {
        PointRecord pickedPoint;
        if (pickPointAtScreenPosition(localPos, &pickedPoint, kHoverPickTolerancePixels)) {
            updateHoveredPoint(&pickedPoint);
            if (clipBoxFirstPointValid_) {
                ClipRegion previewRegion;
                if (buildBoxClipRegion(clipBoxFirstPoint_, pickedPoint, clipBoxAlignment_, &previewRegion)) {
                    clipPreviewRegion_ = previewRegion;
                    clipPreviewActive_ = true;
                    refreshClipOverlay();
                }
            }
        } else {
            clearHoveredPoint();
            if (clipPreviewActive_) {
                clearClipPreview();
                refreshClipOverlay();
            }
        }
        return;
    }

    if (profileClassificationModeEnabled_) {
        if (profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
            && !profileClassificationTaskActive_
            && !profileClassificationPolygonPoints_.isEmpty()) {
            profileClassificationPolygonPreviewPoint_ = localPos;
            profileClassificationPolygonPreviewActive_ = true;
            updateProfileClassificationPolygonOverlay();
        }
        return;
    }

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
    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    const osg::Matrixd localToWindow = osg::Matrixd::translate(sceneOrigin) * worldToWindow;

    const double devicePixelRatio = osgWidget_->devicePixelRatioF();
    const double clickX = localPos.x() * devicePixelRatio;
    const double clickY = (static_cast<double>(osgWidget_->height()) - localPos.y()) * devicePixelRatio;
    const double tolerance = static_cast<double>(tolerancePixels) * devicePixelRatio;
    const double toleranceSquared = tolerance * tolerance;

    bool found = false;
    double bestDistanceSquared = toleranceSquared;
    double bestDepth = std::numeric_limits<double>::max();
    const auto testPoint = [&](const PointRecord& point) {
        const osg::Vec3d projected = osg::Vec3d(
            point.x - sceneOrigin.x(),
            point.y - sceneOrigin.y(),
            point.z - sceneOrigin.z())
            * localToWindow;
        if (projected.z() < 0.0 || projected.z() > 1.0) {
            return;
        }

        const double dx = projected.x() - clickX;
        const double dy = projected.y() - clickY;
        const double distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > bestDistanceSquared) {
            return;
        }

        if (!found
            || distanceSquared < bestDistanceSquared - 0.001
            || (std::abs(distanceSquared - bestDistanceSquared) <= 0.001 && projected.z() < bestDepth)) {
            found = true;
            bestDistanceSquared = distanceSquared;
            bestDepth = projected.z();
            *pickedPoint = point;
        }
    };

    if (!loadedPointCloudDatasets_.isEmpty()) {
        for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
            if (!dataset.info.visible || dataset.pointCloud == nullptr
                || !datasetBoundsNearScreenPosition(dataset, localToWindow, clickX, clickY, tolerance)) {
                continue;
            }
            QVector<std::uint32_t> candidateIndices;
            collectPickCandidateIndices(dataset, localToWindow, clickX, clickY, tolerance, &candidateIndices);
            const std::vector<PointRecord>& points = dataset.pointCloud->points();
            if (candidateIndices.isEmpty()) {
                for (const PointRecord& point : points) {
                    testPoint(point);
                }
            } else {
                for (std::uint32_t pointIndex : candidateIndices) {
                    if (pointIndex < points.size()) {
                        testPoint(points[pointIndex]);
                    }
                }
            }
        }
    } else {
        for (const PointRecord& point : currentPointCloud_->points()) {
            testPoint(point);
        }
    }
    return found;
}

void PointCloudViewer::buildDatasetInteractionPreview(LoadedPointCloudDataset* dataset)
{
    if (dataset == nullptr || dataset->pointCloud == nullptr
        || dataset->pointCloud->size() < interactionPreviewThresholdPoints()) {
        return;
    }

    const std::vector<PointRecord>& points = dataset->pointCloud->points();
    const std::size_t stride = std::max<std::size_t>(1, points.size() / kInteractionPreviewTargetPoints);
    auto preview = std::make_shared<PointCloudData>();
    preview->reserve(std::min(kInteractionPreviewTargetPoints, points.size()));
    for (std::size_t pointIndex = 0; pointIndex < points.size(); pointIndex += stride) {
        preview->appendPointFast(points[pointIndex]);
    }
    preview->finalizeImport(
        dataset->pointCloud->minBounds(),
        dataset->pointCloud->maxBounds(),
        dataset->pointCloud->hasColor(),
        dataset->pointCloud->hasIntensity(),
        dataset->pointCloud->hasClassification(),
        dataset->pointCloud->hasReturnInfo(),
        dataset->pointCloud->hasGpsTime());
    dataset->interactionPreview = std::move(preview);
}

void PointCloudViewer::setInteractionLodActive(bool active)
{
    if (cameraMoving_ == active) {
        return;
    }
    cameraMoving_ = active;
    for (LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (!dataset.sceneNode.valid() || !dataset.previewSceneNode.valid() || !dataset.fullSceneNode.valid()) {
            continue;
        }
        dataset.fullSceneNode->setNodeMask(active ? 0u : ~0u);
        dataset.previewSceneNode->setNodeMask(active ? ~0u : 0u);
    }
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::handleFrameRendered()
{
    if (osgWidget_ == nullptr || loadedPointCloudDatasets_.isEmpty()) {
        return;
    }
    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCamera() == nullptr) {
        return;
    }

    const osg::Matrixd viewMatrix = viewer->getCamera()->getViewMatrix();
    if (!frameCameraStateValid_) {
        lastCameraViewMatrix_ = viewMatrix;
        frameCameraStateValid_ = true;
        return;
    }
    bool changed = false;
    for (int row = 0; row < 4 && !changed; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (std::abs(viewMatrix(row, column) - lastCameraViewMatrix_(row, column)) > 1e-7) {
                changed = true;
                break;
            }
        }
    }
    if (!changed) {
        return;
    }

    lastCameraViewMatrix_ = viewMatrix;
    setInteractionLodActive(true);
    if (refineIdleTimer_ != nullptr) {
        refineIdleTimer_->start();
    }
}

void PointCloudViewer::buildDatasetSpatialIndex(LoadedPointCloudDataset* dataset)
{
    if (dataset == nullptr || dataset->pointCloud == nullptr || dataset->pointCloud->empty()) {
        return;
    }

    const std::size_t pointCount = dataset->pointCloud->size();
    const int gridSize = std::clamp(
        static_cast<int>(std::ceil(std::sqrt(static_cast<double>(pointCount) / 4096.0))),
        4,
        64);
    dataset->spatialGridSize = gridSize;
    dataset->spatialGridPointIndices.clear();
    dataset->spatialGridPointIndices.resize(gridSize * gridSize);

    const PointRecord& min = dataset->pointCloud->minBounds();
    const PointRecord& max = dataset->pointCloud->maxBounds();
    const double spanX = std::max(1e-9, max.x - min.x);
    const double spanY = std::max(1e-9, max.y - min.y);
    const std::vector<PointRecord>& points = dataset->pointCloud->points();
    for (std::size_t pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
        const PointRecord& point = points[pointIndex];
        const int x = std::clamp(static_cast<int>((point.x - min.x) / spanX * gridSize), 0, gridSize - 1);
        const int y = std::clamp(static_cast<int>((point.y - min.y) / spanY * gridSize), 0, gridSize - 1);
        dataset->spatialGridPointIndices[y * gridSize + x].append(static_cast<std::uint32_t>(pointIndex));
    }
}

void PointCloudViewer::collectPickCandidateIndices(
    const LoadedPointCloudDataset& dataset,
    const osg::Matrixd& localToWindow,
    double clickX,
    double clickY,
    double tolerance,
    QVector<std::uint32_t>* candidateIndices) const
{
    if (candidateIndices == nullptr || dataset.spatialGridSize <= 0 || dataset.spatialGridPointIndices.isEmpty()) {
        return;
    }

    const PointRecord& min = dataset.info.minBounds;
    const PointRecord& max = dataset.info.maxBounds;
    const double spanX = std::max(1e-9, max.x - min.x);
    const double spanY = std::max(1e-9, max.y - min.y);
    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    const int gridSize = dataset.spatialGridSize;
    for (int y = 0; y < gridSize; ++y) {
        for (int x = 0; x < gridSize; ++x) {
            const double x0 = min.x + spanX * static_cast<double>(x) / gridSize;
            const double x1 = min.x + spanX * static_cast<double>(x + 1) / gridSize;
            const double y0 = min.y + spanY * static_cast<double>(y) / gridSize;
            const double y1 = min.y + spanY * static_cast<double>(y + 1) / gridSize;
            double screenMinX = std::numeric_limits<double>::max();
            double screenMinY = std::numeric_limits<double>::max();
            double screenMaxX = std::numeric_limits<double>::lowest();
            double screenMaxY = std::numeric_limits<double>::lowest();
            bool visible = false;
            const osg::Vec3d corners[] = {
                { x0, y0, min.z }, { x1, y0, min.z }, { x0, y1, min.z }, { x1, y1, min.z },
                { x0, y0, max.z }, { x1, y0, max.z }, { x0, y1, max.z }, { x1, y1, max.z }
            };
            for (const osg::Vec3d& corner : corners) {
                const osg::Vec3d projected = (corner - sceneOrigin) * localToWindow;
                if (projected.z() < 0.0 || projected.z() > 1.0) {
                    continue;
                }
                visible = true;
                screenMinX = std::min(screenMinX, projected.x());
                screenMinY = std::min(screenMinY, projected.y());
                screenMaxX = std::max(screenMaxX, projected.x());
                screenMaxY = std::max(screenMaxY, projected.y());
            }
            if (visible
                && clickX >= screenMinX - tolerance && clickX <= screenMaxX + tolerance
                && clickY >= screenMinY - tolerance && clickY <= screenMaxY + tolerance) {
                candidateIndices->append(dataset.spatialGridPointIndices.at(y * gridSize + x));
            }
        }
    }
}

bool PointCloudViewer::datasetBoundsNearScreenPosition(
    const LoadedPointCloudDataset& dataset,
    const osg::Matrixd& localToWindow,
    double clickX,
    double clickY,
    double tolerance) const
{
    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    const PointRecord& min = dataset.info.minBounds;
    const PointRecord& max = dataset.info.maxBounds;
    const osg::Vec3d corners[] = {
        { min.x, min.y, min.z }, { max.x, min.y, min.z },
        { min.x, max.y, min.z }, { max.x, max.y, min.z },
        { min.x, min.y, max.z }, { max.x, min.y, max.z },
        { min.x, max.y, max.z }, { max.x, max.y, max.z }
    };
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    bool anyVisible = false;
    for (const osg::Vec3d& corner : corners) {
        const osg::Vec3d projected = (corner - sceneOrigin) * localToWindow;
        if (projected.z() < 0.0 || projected.z() > 1.0) {
            continue;
        }
        anyVisible = true;
        minX = std::min(minX, projected.x());
        minY = std::min(minY, projected.y());
        maxX = std::max(maxX, projected.x());
        maxY = std::max(maxY, projected.y());
    }
    return !anyVisible
        || (clickX >= minX - tolerance && clickX <= maxX + tolerance
            && clickY >= minY - tolerance && clickY <= maxY + tolerance);
}

int PointCloudViewer::pickInspectionRouteWaypointAtScreenPosition(const QPointF& localPos, float tolerancePixels) const
{
    if (!inspectionRouteVisible_ || inspectionRouteWaypoints_.isEmpty() || osgWidget_ == nullptr) {
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
    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    const osg::Matrixd localToWindow = osg::Matrixd::translate(sceneOrigin) * worldToWindow;

    const double devicePixelRatio = osgWidget_->devicePixelRatioF();
    const double clickX = localPos.x() * devicePixelRatio;
    const double clickY = (static_cast<double>(osgWidget_->height()) - localPos.y()) * devicePixelRatio;
    const double tolerance = static_cast<double>(tolerancePixels) * devicePixelRatio;
    const double toleranceSquared = tolerance * tolerance;

    int bestIndex = -1;
    double bestDistanceSquared = toleranceSquared;
    double bestDepth = std::numeric_limits<double>::max();

    for (int waypointIndex = 0; waypointIndex < inspectionRouteWaypoints_.size(); ++waypointIndex) {
        const PointRecord& waypoint = inspectionRouteWaypoints_.at(waypointIndex);
        const osg::Vec3d projected = osg::Vec3d(
            waypoint.x - sceneOrigin.x(),
            waypoint.y - sceneOrigin.y(),
            waypoint.z - sceneOrigin.z())
            * localToWindow;
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
            bestIndex = waypointIndex;
            bestDistanceSquared = distanceSquared;
            bestDepth = projected.z();
        }
    }

    return bestIndex;
}

osg::ref_ptr<osg::Node> PointCloudViewer::buildInspectionRouteOverlay() const
{
    if (inspectionRouteWaypoints_.isEmpty() || !inspectionRouteVisible_) {
        return nullptr;
    }

    QList<PointRecord> overlayWaypoints = inspectionRouteWaypoints_;
    if (routeWaypointDragActive_
        && routeWaypointDragPreviewValid_
        && routeWaypointDragIndex_ >= 0
        && routeWaypointDragIndex_ < overlayWaypoints.size()) {
        overlayWaypoints[routeWaypointDragIndex_] = routeWaypointDragPreviewPoint_;
    }

    const bool showSelectionOverlays = !inspectionRouteRoamActive();
    const int effectiveSelectedWaypointIndex = showSelectionOverlays ? selectedInspectionRouteWaypointIndex_ : -1;
    const int effectiveSelectedTargetIndex = showSelectionOverlays ? selectedInspectionRouteWaypointTargetIndex_ : -1;

    QSet<int> secondaryHighlightPartIndices;
    int primaryHighlightPartIndex = -1;
    if (effectiveSelectedWaypointIndex >= 0
        && effectiveSelectedWaypointIndex < inspectionRouteWaypointAllTargetPartIndices_.size()) {
        const QList<int>& selectedWaypointTargetPartIndices =
            inspectionRouteWaypointAllTargetPartIndices_.at(effectiveSelectedWaypointIndex);
        for (int partIndex : selectedWaypointTargetPartIndices) {
            if (partIndex > 0) {
                secondaryHighlightPartIndices.insert(partIndex);
            }
        }

        if (!selectedWaypointTargetPartIndices.isEmpty()) {
            const int normalizedTargetIndex = normalizeInspectionRouteWaypointTargetIndex(
                effectiveSelectedWaypointIndex,
                effectiveSelectedTargetIndex);
            if (normalizedTargetIndex >= 0 && normalizedTargetIndex < selectedWaypointTargetPartIndices.size()) {
                primaryHighlightPartIndex = selectedWaypointTargetPartIndices.at(normalizedTargetIndex);
            }
        }
    }

    const osg::Vec4 waypointColor = qColorToVec4(inspectionRouteWaypointColor_);
    const osg::Vec4 partPointColor = qColorToVec4(inspectionRoutePartPointColor_);
    const osg::Vec4 trajectoryColor = qColorToVec4(inspectionRouteTrajectoryColor_);
    const osg::Vec3d sceneOrigin = overlaySceneOrigin();

    osg::ref_ptr<osg::Group> overlay = new osg::Group();
    osg::ref_ptr<osg::Geode> routeLineGeode = buildInspectionRouteLineGeode(
        overlayWaypoints,
        trajectoryColor,
        sceneOrigin);
    if (routeLineGeode.valid()) {
        overlay->addChild(routeLineGeode.get());
    }

    if (showSelectionOverlays) {
        osg::ref_ptr<osg::Geode> routeWaypointPartLinksGeode = buildInspectionRouteWaypointPartLinksGeode(
            overlayWaypoints,
            inspectionRouteWaypointAllTargetPoints_,
            effectiveSelectedWaypointIndex,
            effectiveSelectedTargetIndex,
            sceneOrigin);
        if (routeWaypointPartLinksGeode.valid()) {
            overlay->addChild(routeWaypointPartLinksGeode.get());
        }

        osg::ref_ptr<osg::Geode> routeFrustumGeode = buildInspectionRouteFrustumGeode(
            overlayWaypoints,
            inspectionRouteWaypointAllTargetPoints_,
            effectiveSelectedWaypointIndex,
            effectiveSelectedTargetIndex,
            sceneOrigin);
        if (routeFrustumGeode.valid()) {
            overlay->addChild(routeFrustumGeode.get());
        }
    }

    osg::ref_ptr<osg::Geode> routePartPointsGeode = buildInspectionRoutePartPointsGeode(
        inspectionRoutePartPoints_,
        inspectionRoutePartPointIndices_,
        secondaryHighlightPartIndices,
        primaryHighlightPartIndex,
        partPointColor,
        sceneOrigin);
    if (routePartPointsGeode.valid()) {
        overlay->addChild(routePartPointsGeode.get());
    }

    osg::ref_ptr<osg::Geode> routePointsGeode = buildInspectionRoutePointsGeode(
        overlayWaypoints,
        effectiveSelectedWaypointIndex,
        waypointColor,
        sceneOrigin);
    if (routePointsGeode.valid()) {
        overlay->addChild(routePointsGeode.get());
    }

    if (inspectionRouteRoamActive()
        && inspectionRouteRoamViewMode_ == RouteRoamViewMode::ThirdPerson
        && inspectionRouteRoamCurrentPositionValid_) {
        osg::ref_ptr<osg::Node> routeRoamTrackerNode = buildInspectionRouteRoamTrackerNode(
            inspectionRouteRoamCurrentPosition_,
            osg::Vec4(0.96f, 0.18f, 0.86f, 0.98f),
            sceneOrigin);
        if (routeRoamTrackerNode.valid()) {
            overlay->addChild(routeRoamTrackerNode.get());
        }
    }

    return overlay->getNumChildren() > 0
        ? wrapOverlayNodeWithSceneOrigin(overlay.release(), sceneOrigin)
        : nullptr;
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
    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    const osg::Matrixd localToWindow = osg::Matrixd::translate(sceneOrigin) * worldToWindow;

    const osg::Vec3d projected = osg::Vec3d(
        point.x - sceneOrigin.x(),
        point.y - sceneOrigin.y(),
        point.z - sceneOrigin.z())
        * localToWindow;
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

void PointCloudViewer::updateInspectionRouteOverlayWidgets()
{
    if (osgWidget_ == nullptr) {
        return;
    }

    if (!inspectionRouteVisible_) {
        for (QLabel* label : inspectionRouteOverlayLabels_) {
            if (label != nullptr) {
                label->hide();
            }
        }
        for (QLabel* label : inspectionRoutePartOverlayLabels_) {
            if (label != nullptr) {
                label->hide();
            }
        }
        return;
    }

    QList<PointRecord> overlayWaypoints = inspectionRouteWaypoints_;
    if (routeWaypointDragActive_
        && routeWaypointDragPreviewValid_
        && routeWaypointDragIndex_ >= 0
        && routeWaypointDragIndex_ < overlayWaypoints.size()) {
        overlayWaypoints[routeWaypointDragIndex_] = routeWaypointDragPreviewPoint_;
    }

    QSet<int> secondaryHighlightPartIndices;
    int primaryHighlightPartIndex = -1;
    if (selectedInspectionRouteWaypointIndex_ >= 0
        && selectedInspectionRouteWaypointIndex_ < inspectionRouteWaypointAllTargetPartIndices_.size()) {
        const QList<int>& selectedWaypointTargetPartIndices =
            inspectionRouteWaypointAllTargetPartIndices_.at(selectedInspectionRouteWaypointIndex_);
        for (int partIndex : selectedWaypointTargetPartIndices) {
            if (partIndex > 0) {
                secondaryHighlightPartIndices.insert(partIndex);
            }
        }

        if (!selectedWaypointTargetPartIndices.isEmpty()) {
            const int normalizedTargetIndex = normalizeInspectionRouteWaypointTargetIndex(
                selectedInspectionRouteWaypointIndex_,
                selectedInspectionRouteWaypointTargetIndex_);
            if (normalizedTargetIndex >= 0 && normalizedTargetIndex < selectedWaypointTargetPartIndices.size()) {
                primaryHighlightPartIndex = selectedWaypointTargetPartIndices.at(normalizedTargetIndex);
            }
        }
    }

    const RouteLabelDisplayMode waypointLabelMode = routeWaypointLabelDisplayMode_;
    const RouteLabelDisplayMode partLabelMode = routePartLabelDisplayMode_;
    const bool waypointLabelsHidden = routeLabelModeHidden(waypointLabelMode);
    const bool partLabelsHidden = routeLabelModeHidden(partLabelMode);
    const bool compactWaypointLabels = routeLabelModeCompact(waypointLabelMode);
    const bool compactPartLabels = routeLabelModeCompact(partLabelMode);

    while (inspectionRouteOverlayLabels_.size() < inspectionRouteWaypoints_.size()) {
        auto* label = new QLabel(osgWidget_);
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        label->hide();
        inspectionRouteOverlayLabels_.append(label);
    }

    for (int waypointIndex = 0; waypointIndex < inspectionRouteOverlayLabels_.size(); ++waypointIndex) {
        QLabel* label = inspectionRouteOverlayLabels_.at(waypointIndex);
        if (label == nullptr) {
            continue;
        }

        if (waypointIndex >= inspectionRouteWaypoints_.size()) {
            label->hide();
            continue;
        }
        if (waypointLabelsHidden) {
            label->hide();
            continue;
        }

        bool pointVisible = false;
        const PointRecord& waypoint = overlayWaypoints.at(waypointIndex);
        const QPointF anchor = projectPointToViewport(waypoint, &pointVisible);
        if (!pointVisible) {
            label->hide();
            continue;
        }

        const bool isSelected = waypointIndex == selectedInspectionRouteWaypointIndex_;
        const QString labelText = inspectionRouteWaypointLabelText(waypointIndex);
        if (labelText.isEmpty()) {
            label->hide();
            continue;
        }
        label->setText(labelText);
        if (compactWaypointLabels) {
            label->setStyleSheet(QStringLiteral(
                "QLabel {"
                "background-color: %1;"
                "color: %2;"
                "border: 1px solid %3;"
                "border-radius: 6px;"
                "padding: 2px 6px;"
                "font-size: 10px;"
                "font-weight: 600;"
                "}").arg(
                    isSelected ? QStringLiteral("rgba(37, 99, 235, 214)") : QStringLiteral("rgba(15, 23, 42, 158)"),
                    QStringLiteral("#f8fafc"),
                    isSelected ? QStringLiteral("rgba(191, 219, 254, 220)") : QStringLiteral("rgba(148, 163, 184, 170)")));
            positionOverlayLabel(label, anchor, QPoint(10, -12));
        } else {
            label->setStyleSheet(QStringLiteral(
                "QLabel {"
                "background-color: %1;"
                "color: #0f172a;"
                "border: 1px solid %2;"
                "border-radius: 8px;"
                "padding: 3px 8px;"
                "font-size: 11px;"
                "font-weight: 700;"
                "}").arg(
                    isSelected ? QStringLiteral("rgba(254, 240, 138, 236)") : QStringLiteral("rgba(224, 242, 254, 232)"),
                    isSelected ? QStringLiteral("rgba(245, 158, 11, 220)") : QStringLiteral("rgba(56, 189, 248, 180)")));
            positionOverlayLabel(label, anchor, QPoint(14, -16));
        }
    }

    while (inspectionRoutePartOverlayLabels_.size() < inspectionRoutePartPoints_.size()) {
        auto* label = new QLabel(osgWidget_);
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        label->hide();
        inspectionRoutePartOverlayLabels_.append(label);
    }

    for (int partIndex = 0; partIndex < inspectionRoutePartOverlayLabels_.size(); ++partIndex) {
        QLabel* label = inspectionRoutePartOverlayLabels_.at(partIndex);
        if (label == nullptr) {
            continue;
        }

        if (partIndex >= inspectionRoutePartPoints_.size()) {
            label->hide();
            continue;
        }
        if (partLabelsHidden) {
            label->hide();
            continue;
        }

        bool pointVisible = false;
        const PointRecord& partPoint = inspectionRoutePartPoints_.at(partIndex);
        const QPointF anchor = projectPointToViewport(partPoint, &pointVisible);
        if (!pointVisible) {
            label->hide();
            continue;
        }

        const int currentPartIndex =
            partIndex < inspectionRoutePartPointIndices_.size() ? inspectionRoutePartPointIndices_.at(partIndex) : -1;
        const bool isPrimaryTarget = currentPartIndex > 0 && currentPartIndex == primaryHighlightPartIndex;
        const bool isSecondaryTarget =
            !isPrimaryTarget && currentPartIndex > 0 && secondaryHighlightPartIndices.contains(currentPartIndex);
        const QString labelText = inspectionRoutePartLabelText(partIndex);
        if (labelText.isEmpty()) {
            label->hide();
            continue;
        }
        label->setText(labelText);
        if (compactPartLabels) {
            label->setStyleSheet(QStringLiteral(
                "QLabel {"
                "background-color: %1;"
                "color: #fef3c7;"
                "border: 1px solid %2;"
                "border-radius: 6px;"
                "padding: 2px 6px;"
                "font-size: 10px;"
                "font-weight: 600;"
                "}").arg(
                    isPrimaryTarget
                        ? QStringLiteral("rgba(180, 83, 9, 220)")
                        : (isSecondaryTarget
                            ? QStringLiteral("rgba(120, 53, 15, 196)")
                            : QStringLiteral("rgba(68, 64, 60, 150)")),
                    isPrimaryTarget
                        ? QStringLiteral("rgba(253, 186, 116, 230)")
                        : (isSecondaryTarget
                            ? QStringLiteral("rgba(251, 146, 60, 210)")
                            : QStringLiteral("rgba(216, 180, 147, 168)"))));
            positionOverlayLabel(label, anchor, QPoint(10, 12));
        } else {
            label->setStyleSheet(QStringLiteral(
                "QLabel {"
                "background-color: %1;"
                "color: %2;"
                "border: 1px solid %3;"
                "border-radius: 8px;"
                "padding: 3px 8px;"
                "font-size: 11px;"
                "font-weight: 700;"
                "}").arg(
                    isPrimaryTarget
                        ? QStringLiteral("rgba(255, 153, 102, 245)")
                        : (isSecondaryTarget
                            ? QStringLiteral("rgba(255, 224, 178, 240)")
                            : QStringLiteral("rgba(255, 237, 213, 236)")),
                    QStringLiteral("#7c2d12"),
                    isPrimaryTarget
                        ? QStringLiteral("rgba(234, 88, 12, 230)")
                        : (isSecondaryTarget
                            ? QStringLiteral("rgba(249, 115, 22, 220)")
                            : QStringLiteral("rgba(251, 146, 60, 200)"))));
            positionOverlayLabel(label, anchor, QPoint(14, 16));
        }
    }
}

void PointCloudViewer::updateRouteCameraPreviewOverlay()
{
    auto* previewOverlay = static_cast<RouteCameraPreviewOverlay*>(routeCameraPreviewOverlay_);
    if (previewOverlay == nullptr || osgWidget_ == nullptr) {
        return;
    }

    positionRouteCameraPreviewOverlay();

    if (!inspectionRouteVisible_
        || !hasPointCloud()
        || selectedInspectionRouteWaypointIndex_ < 0
        || selectedInspectionRouteWaypointIndex_ >= inspectionRouteWaypoints_.size()) {
        previewOverlay->hide();
        return;
    }

    QList<PointRecord> previewWaypoints = inspectionRouteWaypoints_;
    if (routeWaypointDragActive_
        && routeWaypointDragPreviewValid_
        && routeWaypointDragIndex_ >= 0
        && routeWaypointDragIndex_ < previewWaypoints.size()) {
        previewWaypoints[routeWaypointDragIndex_] = routeWaypointDragPreviewPoint_;
    }

    const int waypointIndex = selectedInspectionRouteWaypointIndex_;
    const PointRecord& cameraPoint = previewWaypoints.at(waypointIndex);

    QList<PointRecord> waypointTargets;
    QList<double> waypointCameraYawCandidates;
    QList<double> waypointCameraPitchCandidates;
    QList<double> waypointFocalLengthCandidates;
    QStringList waypointTargetLabels;
    if (waypointIndex < inspectionRouteWaypointAllTargetPoints_.size()) {
        waypointTargets = inspectionRouteWaypointAllTargetPoints_.at(waypointIndex);
    }
    if (waypointIndex < inspectionRouteWaypointAllCameraYawDegs_.size()) {
        waypointCameraYawCandidates = inspectionRouteWaypointAllCameraYawDegs_.at(waypointIndex);
    }
    if (waypointIndex < inspectionRouteWaypointAllCameraPitchDegs_.size()) {
        waypointCameraPitchCandidates = inspectionRouteWaypointAllCameraPitchDegs_.at(waypointIndex);
    }
    if (waypointIndex < inspectionRouteWaypointAllFocalLengthRatios_.size()) {
        waypointFocalLengthCandidates = inspectionRouteWaypointAllFocalLengthRatios_.at(waypointIndex);
    }
    if (waypointIndex < inspectionRouteWaypointAllTargetLabels_.size()) {
        waypointTargetLabels = inspectionRouteWaypointAllTargetLabels_.at(waypointIndex);
    }

    if (waypointTargets.isEmpty()) {
        const bool hasLegacyTarget =
            waypointIndex < inspectionRouteWaypointHasTargetPoints_.size()
            && inspectionRouteWaypointHasTargetPoints_.at(waypointIndex)
            && waypointIndex < inspectionRouteWaypointTargetPoints_.size();
        if (hasLegacyTarget) {
            waypointTargets.append(inspectionRouteWaypointTargetPoints_.at(waypointIndex));
            if (waypointIndex < inspectionRouteWaypointCameraYawDegs_.size()) {
                waypointCameraYawCandidates.append(inspectionRouteWaypointCameraYawDegs_.at(waypointIndex));
            }
            if (waypointIndex < inspectionRouteWaypointCameraPitchDegs_.size()) {
                waypointCameraPitchCandidates.append(inspectionRouteWaypointCameraPitchDegs_.at(waypointIndex));
            }
            if (waypointIndex < inspectionRouteWaypointFocalLengthRatios_.size()) {
                waypointFocalLengthCandidates.append(inspectionRouteWaypointFocalLengthRatios_.at(waypointIndex));
            }
            const QString legacyTargetLabel =
                waypointIndex < inspectionRouteWaypointTargetLabels_.size()
                    ? inspectionRouteWaypointTargetLabels_.at(waypointIndex)
                    : QString();
            waypointTargetLabels.append(legacyTargetLabel);
        }
    }

    const bool hasTarget = !waypointTargets.isEmpty();
    const int selectedTargetIndex = hasTarget
        ? normalizeInspectionRouteWaypointTargetIndex(waypointIndex, selectedInspectionRouteWaypointTargetIndex_)
        : -1;
    const PointRecord targetPoint = (hasTarget && selectedTargetIndex >= 0 && selectedTargetIndex < waypointTargets.size())
        ? waypointTargets.at(selectedTargetIndex)
        : PointRecord();
    const double aircraftYawDeg = waypointIndex < inspectionRouteWaypointAircraftYawDegs_.size()
        ? inspectionRouteWaypointAircraftYawDegs_.at(waypointIndex)
        : 0.0;
    const double gimbalPitchDeg = waypointIndex < inspectionRouteWaypointGimbalPitchDegs_.size()
        ? inspectionRouteWaypointGimbalPitchDegs_.at(waypointIndex)
        : 0.0;
    const double cameraYawDeg =
        (hasTarget && selectedTargetIndex >= 0 && selectedTargetIndex < waypointCameraYawCandidates.size())
            ? waypointCameraYawCandidates.at(selectedTargetIndex)
            : (waypointIndex < inspectionRouteWaypointCameraYawDegs_.size()
                ? inspectionRouteWaypointCameraYawDegs_.at(waypointIndex)
                : 0.0);
    const double cameraPitchDeg =
        (hasTarget && selectedTargetIndex >= 0 && selectedTargetIndex < waypointCameraPitchCandidates.size())
            ? waypointCameraPitchCandidates.at(selectedTargetIndex)
            : (waypointIndex < inspectionRouteWaypointCameraPitchDegs_.size()
                ? inspectionRouteWaypointCameraPitchDegs_.at(waypointIndex)
                : 0.0);
    const double focalLengthRatio =
        (hasTarget && selectedTargetIndex >= 0 && selectedTargetIndex < waypointFocalLengthCandidates.size())
            ? normalizedRoutePreviewFocalLengthRatio(waypointFocalLengthCandidates.at(selectedTargetIndex))
            : (waypointIndex < inspectionRouteWaypointFocalLengthRatios_.size()
                ? normalizedRoutePreviewFocalLengthRatio(inspectionRouteWaypointFocalLengthRatios_.at(waypointIndex))
                : kRoutePreviewDefaultFocalLengthRatio);

    const double yawRadians = qDegreesToRadians(aircraftYawDeg + cameraYawDeg);
    const double pitchRadians = qDegreesToRadians(gimbalPitchDeg + cameraPitchDeg);
    osg::Vec3d forward(
        std::sin(yawRadians) * std::cos(pitchRadians),
        std::cos(yawRadians) * std::cos(pitchRadians),
        std::sin(pitchRadians));
    if (forward.length2() <= 0.00001) {
        forward = osg::Vec3d(0.0, 1.0, 0.0);
    }
    forward.normalize();

    osg::Vec3d worldUp(0.0, 0.0, 1.0);
    osg::Vec3d right = forward ^ worldUp;
    if (right.length2() <= 0.00001) {
        worldUp = osg::Vec3d(0.0, 1.0, 0.0);
        right = forward ^ worldUp;
    }
    right.normalize();
    osg::Vec3d up = right ^ forward;
    up.normalize();

    QImage previewImage(kRoutePreviewRenderWidth, kRoutePreviewRenderHeight, QImage::Format_ARGB32_Premultiplied);
    previewImage.fill(visualizationOptions_.backgroundColor.rgba());
    std::vector<float> depthBuffer(static_cast<std::size_t>(previewImage.width() * previewImage.height()), std::numeric_limits<float>::max());

    const double aspectRatio = static_cast<double>(previewImage.width()) / static_cast<double>(previewImage.height());
    const double verticalFovRadians = routePreviewVerticalFovRadians(focalLengthRatio);
    const double tanHalfFov = std::tan(verticalFovRadians * 0.5);
    const double nearPlane = 0.35;
    PointRecord visibleMinBounds;
    PointRecord visibleMaxBounds;
    visiblePointCloudBounds(&visibleMinBounds, &visibleMaxBounds);
    const double minZ = visibleMinBounds.z;
    const double heightSpan = std::max(0.0, visibleMaxBounds.z - minZ);
    const std::size_t totalVisiblePoints = visiblePointCount();
    const std::size_t pointStride = std::max<std::size_t>(1u, totalVisiblePoints / 140000u);

    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (!dataset.info.visible || dataset.pointCloud == nullptr) {
            continue;
        }
        const std::vector<PointRecord>& points = dataset.pointCloud->points();
        for (std::size_t pointIndex = 0; pointIndex < points.size(); pointIndex += pointStride) {
            const PointRecord& point = points[pointIndex];
            if (!routePreviewPointVisible(point, visualizationOptions_)) {
                continue;
            }

            const osg::Vec3d relativePoint(
                static_cast<double>(point.x) - static_cast<double>(cameraPoint.x),
                static_cast<double>(point.y) - static_cast<double>(cameraPoint.y),
                static_cast<double>(point.z) - static_cast<double>(cameraPoint.z));
            const double zCamera = relativePoint * forward;
            if (zCamera <= nearPlane) {
                continue;
            }

            const double xCamera = relativePoint * right;
            const double yCamera = relativePoint * up;
            const double normalizedX = xCamera / (zCamera * tanHalfFov * aspectRatio);
            const double normalizedY = yCamera / (zCamera * tanHalfFov);
            if (std::abs(normalizedX) > 1.05 || std::abs(normalizedY) > 1.05) {
                continue;
            }

            const int pixelX = std::clamp(
                static_cast<int>(std::lround((normalizedX * 0.5 + 0.5) * static_cast<double>(previewImage.width() - 1))),
                0,
                previewImage.width() - 1);
            const int pixelY = std::clamp(
                static_cast<int>(std::lround((0.5 - normalizedY * 0.5) * static_cast<double>(previewImage.height() - 1))),
                0,
                previewImage.height() - 1);
            const std::size_t depthIndex = static_cast<std::size_t>(pixelY * previewImage.width() + pixelX);
            if (zCamera >= depthBuffer[depthIndex]) {
                continue;
            }

            depthBuffer[depthIndex] = static_cast<float>(zCamera);
            const QColor pointColor = routePreviewPointColor(point, visualizationOptions_, minZ, heightSpan);
            const float pointAlpha = clampUnit(
                static_cast<float>(pointColor.alphaF()) * clampUnit(visualizationOptions_.pointOpacity));
            if (pointAlpha <= 0.01f) {
                continue;
            }

            QRgb* scanLine = reinterpret_cast<QRgb*>(previewImage.scanLine(pixelY));
            scanLine[pixelX] = blendRoutePreviewPixel(scanLine[pixelX], pointColor, pointAlpha);
            if (pixelX + 1 < previewImage.width()) {
                scanLine[pixelX + 1] = blendRoutePreviewPixel(scanLine[pixelX + 1], pointColor, pointAlpha);
            }
            if (pixelY + 1 < previewImage.height()) {
                QRgb* nextScanLine = reinterpret_cast<QRgb*>(previewImage.scanLine(pixelY + 1));
                nextScanLine[pixelX] = blendRoutePreviewPixel(nextScanLine[pixelX], pointColor, pointAlpha);
            }
        }
    }

    bool targetVisible = false;
    QPointF targetNormalizedPoint(0.5, 0.5);
    QString alignmentHint = tr("Link a part point to enable aiming guidance.");
    QColor statusColor(30, 64, 175);
    if (hasTarget) {
        const osg::Vec3d relativeTarget(
            static_cast<double>(targetPoint.x) - static_cast<double>(cameraPoint.x),
            static_cast<double>(targetPoint.y) - static_cast<double>(cameraPoint.y),
            static_cast<double>(targetPoint.z) - static_cast<double>(cameraPoint.z));
        const double zCamera = relativeTarget * forward;
        if (zCamera > nearPlane) {
            const double xCamera = relativeTarget * right;
            const double yCamera = relativeTarget * up;
            const double normalizedX = xCamera / (zCamera * tanHalfFov * aspectRatio);
            const double normalizedY = yCamera / (zCamera * tanHalfFov);
            targetNormalizedPoint = QPointF(
                normalizedX * 0.5 + 0.5,
                0.5 - normalizedY * 0.5);
            targetVisible =
                targetNormalizedPoint.x() >= 0.0
                && targetNormalizedPoint.x() <= 1.0
                && targetNormalizedPoint.y() >= 0.0
                && targetNormalizedPoint.y() <= 1.0;
        }

        if (targetVisible) {
            const double deltaXNormalized = targetNormalizedPoint.x() - 0.5;
            const double deltaYNormalized = 0.5 - targetNormalizedPoint.y();
            const double deltaXPixels = deltaXNormalized * static_cast<double>(previewImage.width());
            const double deltaYPixels = deltaYNormalized * static_cast<double>(previewImage.height());
            const double radialErrorPixels = std::hypot(deltaXPixels, deltaYPixels);

            auto directionText = [](double deltaX, double deltaY) {
                if (std::abs(deltaX) <= 6.0 && std::abs(deltaY) <= 6.0) {
                    return QCoreApplication::translate("PointCloudViewer", "Target nearly centered");
                }

                QStringList parts;
                if (deltaX < -2.0) {
                    parts.append(QCoreApplication::translate("PointCloudViewer", "move left"));
                } else if (deltaX > 2.0) {
                    parts.append(QCoreApplication::translate("PointCloudViewer", "move right"));
                }
                if (deltaY < -2.0) {
                    parts.append(QCoreApplication::translate("PointCloudViewer", "move down"));
                } else if (deltaY > 2.0) {
                    parts.append(QCoreApplication::translate("PointCloudViewer", "move up"));
                }
                return parts.join(QCoreApplication::translate("PointCloudViewer", " + "));
            };

            alignmentHint = tr("Offset %1 px | %2")
                .arg(QLocale().toString(radialErrorPixels, 'f', 1))
                .arg(directionText(deltaXPixels, deltaYPixels));

            if (radialErrorPixels <= 10.0) {
                statusColor = QColor(22, 163, 74);
            } else if (radialErrorPixels <= 28.0) {
                statusColor = QColor(202, 138, 4);
            } else {
                statusColor = QColor(220, 38, 38);
            }
        } else {
            alignmentHint = tr("Drag waypoint or adjust yaw/pitch until the target returns to frame.");
            statusColor = QColor(185, 28, 28);
        }
    }

    QString labelText = inspectionRouteWaypointLabelText(waypointIndex);
    if (labelText.isEmpty()) {
        labelText = QLocale().toString(waypointIndex + 1);
    }
    const QString targetLabel = hasTarget
        ? ((selectedTargetIndex >= 0
            && selectedTargetIndex < waypointTargetLabels.size()
            && !waypointTargetLabels.at(selectedTargetIndex).trimmed().isEmpty())
                ? waypointTargetLabels.at(selectedTargetIndex).trimmed()
                : tr("Target %1").arg(QLocale().toString(selectedTargetIndex + 1)))
        : (waypointIndex < inspectionRouteWaypointTargetLabels_.size()
            && !inspectionRouteWaypointTargetLabels_.at(waypointIndex).trimmed().isEmpty()
                ? inspectionRouteWaypointTargetLabels_.at(waypointIndex)
                : tr("Unlinked"));
    const QString title =
        routeWaypointDragActive_ && waypointIndex == routeWaypointDragIndex_
            ? tr("Route Camera Preview | Dragging %1").arg(labelText)
            : tr("Route Camera Preview | %1").arg(labelText);
    const QString subtitle = hasTarget
        ? tr("Target %1/%2: %3")
              .arg(QLocale().toString(selectedTargetIndex + 1))
              .arg(QLocale().toString(waypointTargets.size()))
              .arg(targetLabel)
        : tr("Target: %1").arg(targetLabel);
    const QString footer = hasTarget
        ? tr("Yaw %1 | Pitch %2 | Cam %3 / %4 | Target %5/%6")
              .arg(QLocale().toString(aircraftYawDeg, 'f', 1))
              .arg(QLocale().toString(gimbalPitchDeg, 'f', 1))
              .arg(QLocale().toString(cameraYawDeg, 'f', 1))
              .arg(QLocale().toString(cameraPitchDeg, 'f', 1))
              .arg(QLocale().toString(selectedTargetIndex + 1))
              .arg(QLocale().toString(waypointTargets.size()))
        : tr("Yaw %1 | Pitch %2 | Cam %3 / %4")
              .arg(QLocale().toString(aircraftYawDeg, 'f', 1))
              .arg(QLocale().toString(gimbalPitchDeg, 'f', 1))
              .arg(QLocale().toString(cameraYawDeg, 'f', 1))
              .arg(QLocale().toString(cameraPitchDeg, 'f', 1));
    const QString targetStatus = !hasTarget
        ? tr("No linked part point")
        : (targetVisible
            ? tr("Target %1 in frame").arg(QLocale().toString(selectedTargetIndex + 1))
            : tr("Target %1 off-screen").arg(QLocale().toString(selectedTargetIndex + 1)));
    const bool captureFlashActive = inspectionRouteRoamCaptureFlashRemainingSeconds_ > 0.0;
    const QString captureAwareTargetStatus = captureFlashActive
        ? (hasTarget
            ? tr("Captured: %1").arg(targetLabel)
            : tr("Captured waypoint snapshot"))
        : targetStatus;

    previewOverlay->setPreviewState(
        true,
        previewImage,
        title,
        subtitle,
        footer,
        captureAwareTargetStatus,
        alignmentHint,
        hasTarget,
        targetVisible,
        targetNormalizedPoint,
        captureFlashActive ? QColor(22, 163, 74) : statusColor,
        captureFlashActive);
    previewOverlay->show();
    previewOverlay->raise();
}

void PointCloudViewer::positionRouteCameraPreviewOverlay()
{
    if (routeCameraPreviewOverlay_ == nullptr || osgWidget_ == nullptr) {
        return;
    }

    routeCameraPreviewOverlay_->move(
        std::max(12, osgWidget_->width() - routeCameraPreviewOverlay_->width() - 16),
        std::max(12, osgWidget_->height() - routeCameraPreviewOverlay_->height() - 16));
    routeCameraPreviewOverlay_->raise();
}

void PointCloudViewer::refreshInspectionRouteOverlay()
{
    osg::ref_ptr<osg::Node> rebuiltOverlayNode;
    if (rootGroup_.valid() && !inspectionRouteWaypoints_.isEmpty()) {
        rebuiltOverlayNode = buildInspectionRouteOverlay();
    }

    if (rootGroup_.valid()) {
        if (rebuiltOverlayNode.valid()) {
            rootGroup_->addChild(rebuiltOverlayNode.get());
        }
        if (inspectionRouteNode_.valid()) {
            rootGroup_->removeChild(inspectionRouteNode_.get());
        }
        inspectionRouteNode_ = rebuiltOverlayNode;
    }

    updateInspectionRouteOverlayWidgets();
    updateRouteCameraPreviewOverlay();
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::scheduleOverlayWidgetRefresh()
{
    if (overlayWidgetRefreshPending_) {
        return;
    }

    overlayWidgetRefreshPending_ = true;
    QMetaObject::invokeMethod(
        this,
        [this]() { flushOverlayWidgetRefresh(); },
        Qt::QueuedConnection);
}

void PointCloudViewer::flushOverlayWidgetRefresh()
{
    overlayWidgetRefreshPending_ = false;
    updateMeasurementOverlayWidgets();
    updateTowerOverlayWidgets();
    updateInspectionIssueOverlayWidgets();
    updateInspectionRouteOverlayWidgets();
    if (selectionRubberBand_ != nullptr && selectionRubberBand_->isVisible()) {
        selectionRubberBand_->raise();
    }
    if (profileClassificationPolygonOverlay_ != nullptr && profileClassificationPolygonOverlay_->isVisible()) {
        profileClassificationPolygonOverlay_->raise();
    }
    updateAxisIndicator();
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

void PointCloudViewer::syncCurrentFilePath()
{
    currentFilePath_.clear();
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (dataset.info.visible) {
            currentFilePath_ = dataset.info.filePath;
            return;
        }
    }

    if (!currentFilePaths_.isEmpty()) {
        currentFilePath_ = currentFilePaths_.constFirst();
    }
}

WelcomeWorkspaceWidget* PointCloudViewer::welcomeWorkspace() const
{
    return welcomeWorkspace_;
}

QImage PointCloudViewer::captureSceneThumbnail() const
{
    return osgWidget_ != nullptr ? osgWidget_->grabFramebuffer() : QImage();
}

void PointCloudViewer::requestSceneFrame()
{
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::retranslateUi()
{
    if (welcomeWorkspace_ != nullptr) {
        welcomeWorkspace_->retranslateUi();
    }
    updateFooter();
    updateMeasurementOverlayWidgets();
    updateInspectionRouteOverlayWidgets();
    updateRouteCameraPreviewOverlay();
    updateAxisIndicator();
}
