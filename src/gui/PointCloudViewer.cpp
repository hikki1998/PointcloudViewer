#include "gui/PointCloudViewer.h"

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
#include <osg/StateSet>
#include <osg/Vec4>
#include <osg/Viewport>
#include <osgGA/CameraManipulator>
#include <osgGA/TrackballManipulator>

#include "osg/OsgPointCloudNode.h"
#include "domain/DataManager.h"
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

class PolygonSelectionOverlay final : public QWidget
{
public:
    explicit PolygonSelectionOverlay(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        hide();
    }

    void setPolygonState(const QPolygonF& polygon, bool hasPreviewPoint, const QPointF& previewPoint)
    {
        polygon_ = polygon;
        hasPreviewPoint_ = hasPreviewPoint;
        previewPoint_ = previewPoint;

        if (polygon_.isEmpty()) {
            hide();
            return;
        }

        show();
        raise();
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        if (polygon_.isEmpty()) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (polygon_.size() >= 3) {
            QPainterPath fillPath;
            fillPath.addPolygon(polygon_);
            painter.fillPath(fillPath, QColor(56, 189, 248, 36));
        }

        QPen polygonPen(QColor(56, 189, 248, 225), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(polygonPen);
        for (int index = 1; index < polygon_.size(); ++index) {
            painter.drawLine(polygon_.at(index - 1), polygon_.at(index));
        }

        if (polygon_.size() >= 3) {
            painter.drawLine(polygon_.constLast(), polygon_.constFirst());
        }

        if (hasPreviewPoint_ && !polygon_.isEmpty()) {
            QPen previewPen(QColor(14, 165, 233, 210), 1.5, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(previewPen);
            painter.drawLine(polygon_.constLast(), previewPoint_);
        }

        painter.setPen(QPen(QColor(255, 255, 255, 210), 1.0));
        painter.setBrush(QColor(14, 165, 233, 220));
        for (int index = 0; index < polygon_.size(); ++index) {
            painter.drawEllipse(polygon_.at(index), 4.0, 4.0);
        }
    }

private:
    QPolygonF polygon_;
    QPointF previewPoint_;
    bool hasPreviewPoint_ = false;
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

osg::Vec4 measurementColorPrimary()
{
    return osg::Vec4(1.0f, 0.78f, 0.20f, 1.0f);
}

osg::Vec4 measurementColorSecondary()
{
    return osg::Vec4(0.20f, 0.83f, 0.96f, 1.0f);
}

osg::Vec4 measurementColorIntermediate()
{
    return osg::Vec4(0.96f, 0.93f, 0.55f, 1.0f);
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

osg::ref_ptr<osg::Geode> buildMeasurementMarkersGeode(
    const MeasurementResult& measurementResult,
    const osg::Vec3d& sceneOrigin)
{
    if (measurementResult.points.isEmpty()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

    for (int pointIndex = 0; pointIndex < measurementResult.points.size(); ++pointIndex) {
        const PointRecord& point = measurementResult.points.at(pointIndex);
        vertices->push_back(toOverlayLocalVec3(point, sceneOrigin));

        if (pointIndex == 0) {
            colors->push_back(measurementColorPrimary());
        } else if (pointIndex == measurementResult.points.size() - 1) {
            colors->push_back(measurementColorSecondary());
        } else {
            colors->push_back(measurementColorIntermediate());
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
    stateSet->setAttributeAndModes(new osg::Point(10.0f), osg::StateAttribute::ON);
    applyMeasurementForegroundState(stateSet);

    return geode;
}

osg::ref_ptr<osg::Geode> buildMeasurementLineGeode(
    const MeasurementResult& measurementResult,
    const osg::Vec3d& sceneOrigin)
{
    if (measurementResult.points.size() < 2) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

    for (int pointIndex = 0; pointIndex < measurementResult.points.size(); ++pointIndex) {
        const PointRecord& point = measurementResult.points.at(pointIndex);
        vertices->push_back(toOverlayLocalVec3(point, sceneOrigin));

        if (pointIndex == 0) {
            colors->push_back(measurementColorPrimary());
        } else if (pointIndex == measurementResult.points.size() - 1) {
            colors->push_back(measurementColorSecondary());
        } else {
            colors->push_back(measurementColorIntermediate());
        }
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());

    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::LineWidth(3.0f), osg::StateAttribute::ON);
    applyMeasurementForegroundState(stateSet);

    return geode;
}

osg::ref_ptr<osg::Geode> buildTowerMarkersGeode(
    const QList<TowerRecord>& towerMarkers,
    const osg::Vec3d& sceneOrigin)
{
    if (towerMarkers.isEmpty()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

    for (const TowerMarker& towerMarker : towerMarkers) {
        vertices->push_back(toOverlayLocalVec3(towerMarker.point, sceneOrigin));
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

osg::ref_ptr<osg::Geode> buildInspectionIssuesGeode(
    const QList<InspectionIssue>& issues,
    const osg::Vec3d& sceneOrigin)
{
    if (issues.isEmpty()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

    for (const InspectionIssue& issue : issues) {
        vertices->push_back(toOverlayLocalVec3(issue.point, sceneOrigin));

        osg::Vec4 color(0.93f, 0.27f, 0.27f, 1.0f);
        switch (issue.severity) {
        case IssueSeverity::Info:
            color = osg::Vec4(0.29f, 0.60f, 0.94f, 1.0f);
            break;
        case IssueSeverity::Minor:
            color = osg::Vec4(0.98f, 0.76f, 0.24f, 1.0f);
            break;
        case IssueSeverity::Major:
            color = osg::Vec4(0.96f, 0.49f, 0.20f, 1.0f);
            break;
        case IssueSeverity::Critical:
        default:
            color = osg::Vec4(0.86f, 0.16f, 0.16f, 1.0f);
            break;
        }
        colors->push_back(color);
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
    stateSet->setAttributeAndModes(new osg::Point(13.0f), osg::StateAttribute::ON);
    applyMeasurementForegroundState(stateSet);
    return geode;
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
        rightButtonDragDetected_ = false;
        rightButtonAnchor_ = event->localPos();
        lastPanCursorPosition_ = event->localPos();
        lastPanEventPosition_ = event->localPos();
    }

    if (sceneClickModeEnabled_ && event->button() == Qt::LeftButton) {
        emit scenePressed(event->localPos());
        update();
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

    if (rectangleSelectionEnabled_ && event->button() == Qt::LeftButton && selectionDragActive_) {
        const QRectF selectionRect(selectionAnchor_, event->localPos());
        selectionDragActive_ = false;
        emit selectionRectangleChanged(selectionRect.normalized(), false);
        if (selectionRect.normalized().width() >= 4.0 && selectionRect.normalized().height() >= 4.0) {
            emit selectionRectangleFinished(selectionRect.normalized());
        }
        update();
        return;
    }

    if (sceneClickModeEnabled_ && event->button() == Qt::LeftButton) {
        if (sceneDragCaptureEnabled_) {
            emit sceneDragReleased(event->localPos());
            sceneDragCaptureEnabled_ = false;
        } else if (leftButtonEventDispatched_) {
            dispatchMouseButtonEvent(event->localPos(), event->button(), false);
        } else if (!leftButtonDragDetected_) {
            emit sceneClicked(event->localPos());
        }
    } else {
        dispatchMouseButtonEvent(event->localPos(), event->button(), false);
        if (sceneClickModeEnabled_ && event->button() == Qt::RightButton && !rightButtonDragDetected_) {
            emit sceneSecondaryClicked(event->localPos());
        }
    }

    if (event->button() == Qt::LeftButton) {
        leftButtonPressed_ = false;
        leftButtonDragDetected_ = false;
        leftButtonEventDispatched_ = false;
    } else if (event->button() == Qt::MiddleButton) {
        middleButtonPressed_ = false;
    } else if (event->button() == Qt::RightButton) {
        rightButtonPressed_ = false;
        rightButtonDragDetected_ = false;
    }

    update();
}

void OsgWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event == nullptr) {
        return;
    }

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
        if (interactionOptions_.invertOrbitDrag) {
            delta = QPointF(-delta.x(), -delta.y());
        }
        adjustedPosition = lastOrbitEventPosition_ + delta;
        lastOrbitCursorPosition_ = event->localPos();
        lastOrbitEventPosition_ = adjustedPosition;
    }

    if (sceneClickModeEnabled_ && leftButtonPressed_ && sceneDragCaptureEnabled_) {
        emit sceneDragged(event->localPos());
        update();
        return;
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
    connect(osgWidget_, &OsgWidget::frameRendered, this, [this]() {
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
    if (classificationTaskThread_.joinable()) {
        classificationTaskThread_.join();
    }

    if (osgWidget_ != nullptr) {
        if (osgViewer::Viewer* viewer = osgWidget_->getViewer()) {
            viewer->setDone(true);
        }
    }
}

bool PointCloudViewer::loadPointCloud(const QString& filePath, QString* errorMessage)
{
    return loadPointCloudFiles(QStringList { filePath }, errorMessage);
}

void PointCloudViewer::showTransientPreviewPointCloud(const QString& filePath, const PointCloudData& pointCloudPreview, const QString& detailMessage)
{
    if (pointCloudPreview.empty()) {
        return;
    }

    currentPointCloud_ = std::make_shared<PointCloudData>(pointCloudPreview);
    currentFilePath_ = QFileInfo(filePath).absoluteFilePath();
    hoveredPointValid_ = false;
    lastHoverQueryPosition_ = QPointF();
    lastHoverQueryTime_ = {};
    if (currentPointCloud_ != nullptr && !currentPointCloud_->hasColor() && visualizationOptions_.colorMode == PointCloudColorMode::Rgb) {
        visualizationOptions_.colorMode = PointCloudColorMode::Elevation;
    }

    updateMessage(tr("Preview Ready"), detailMessage);
    rebuildScene();
    updateFooter();
    updateWelcomeOverlayVisibility();
}

bool PointCloudViewer::loadPointCloudFiles(const QStringList& filePaths, QString* errorMessage)
{
    LasReader reader;
    QString localError;
    QStringList normalizedFilePaths;
    QList<LoadedPointCloudDataset> loadedDatasets;
    QList<PointCloudDatasetInfo> datasetInfos;
    nextDatasetId_ = 1;

    for (const QString& filePath : filePaths) {
        const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
        if (!absolutePath.isEmpty() && !normalizedFilePaths.contains(absolutePath, Qt::CaseInsensitive)) {
            normalizedFilePaths.append(absolutePath);
        }
    }

    if (normalizedFilePaths.isEmpty()) {
        localError = tr("No point cloud files were specified.");
        updateMessage(tr("Open failed"), localError);
        if (errorMessage != nullptr) {
            *errorMessage = localError;
        }
        return false;
    }

    const QString loadTitle = normalizedFilePaths.size() == 1
        ? tr("Loading %1").arg(QFileInfo(normalizedFilePaths.constFirst()).fileName())
        : tr("Loading %1 datasets").arg(QLocale().toString(normalizedFilePaths.size()));
    setLoadingState(true, loadTitle, tr("Preparing point cloud import..."), 0);
    emit pointCloudLoadingStarted(loadTitle);
    emit pointCloudLoadingProgress(tr("Preparing point cloud import..."), 0, 1000);

    for (int fileIndex = 0; fileIndex < normalizedFilePaths.size(); ++fileIndex) {
        const QString& filePath = normalizedFilePaths.at(fileIndex);
        PointCloudData datasetPointCloud;
        LasFileMetadata metadata;
        QElapsedTimer progressThrottle;
        progressThrottle.start();
        const auto progressCallback = [this, &normalizedFilePaths, fileIndex, &filePath, &progressThrottle](const LasReadProgress& progress) {
            const double fileFraction = progress.totalPoints > 0
                ? std::clamp(static_cast<double>(progress.pointsRead) / static_cast<double>(progress.totalPoints), 0.0, 1.0)
                : 0.0;
            const bool finishedFile = progress.totalPoints > 0 && progress.pointsRead >= progress.totalPoints;
            if (!finishedFile && progressThrottle.isValid() && progressThrottle.elapsed() < 40) {
                return;
            }

            progressThrottle.restart();
            const double overallFraction = normalizedFilePaths.isEmpty()
                ? 0.0
                : (static_cast<double>(fileIndex) + fileFraction) / static_cast<double>(normalizedFilePaths.size());
            const int overallValue = std::clamp(static_cast<int>(std::lround(overallFraction * 1000.0)), 0, 1000);
            const int percent = std::clamp(static_cast<int>(std::lround(overallFraction * 100.0)), 0, 100);
            const QString detail = progress.totalPoints > 0
                ? tr("Reading %1 (%2/%3 points, %4%)")
                      .arg(QFileInfo(filePath).fileName())
                      .arg(formatPointCount(progress.pointsRead))
                      .arg(formatPointCount(progress.totalPoints))
                      .arg(QLocale().toString(percent))
                : tr("Reading %1 (%2 points)")
                      .arg(QFileInfo(filePath).fileName())
                      .arg(formatPointCount(progress.pointsRead));
            setLoadingState(true, pointCloudLoadingTitle_, detail, percent);
            emit pointCloudLoadingProgress(detail, overallValue, 1000);
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        };
        if (!reader.read(filePath, &datasetPointCloud, &localError, &metadata, progressCallback)) {
            setLoadingState(false, QString(), QString(), -1);
            emit pointCloudLoadingFinished();
            updateMessage(tr("Open failed"), localError);
            if (errorMessage != nullptr) {
                *errorMessage = localError;
            }
            return false;
        }

        if (datasetPointCloud.empty()) {
            localError = tr("Point cloud file is empty: %1").arg(QFileInfo(filePath).fileName());
            updateMessage(tr("Open failed"), localError);
            if (errorMessage != nullptr) {
                *errorMessage = localError;
            }
            return false;
        }
        PointCloudDatasetInfo datasetInfo;
        datasetInfo.datasetId = nextDatasetId_++;
        datasetInfo.filePath = filePath;
        datasetInfo.pointCount = metadata.pointCount;
        datasetInfo.minBounds = metadata.minBounds;
        datasetInfo.maxBounds = metadata.maxBounds;
        datasetInfo.hasColor = metadata.hasColor;
        datasetInfo.hasIntensity = metadata.hasIntensity;
        datasetInfo.hasClassification = metadata.hasClassification;
        datasetInfo.hasReturnInfo = metadata.hasReturnInfo;
        datasetInfo.hasGpsTime = metadata.hasGpsTime;
        datasetInfo.visible = true;
        datasetInfo.projectionText = metadata.projectionText;
        datasetInfos.append(datasetInfo);

        LoadedPointCloudDataset loadedDataset;
        loadedDataset.info = datasetInfo;
        loadedDataset.pointCloud = std::make_shared<PointCloudData>(std::move(datasetPointCloud));
        std::vector<PointRecord>& sourcePoints = loadedDataset.pointCloud->mutablePoints();
        for (std::size_t pointIndex = 0; pointIndex < sourcePoints.size(); ++pointIndex) {
            sourcePoints[pointIndex].sourceDatasetId = datasetInfo.datasetId;
            sourcePoints[pointIndex].sourcePointIndex = static_cast<std::uint32_t>(pointIndex);
        }
        loadedDatasets.append(std::move(loadedDataset));
    }

    loadedPointCloudDatasets_ = std::move(loadedDatasets);
    DataManager::instance().setPointCloudDatasets(datasetInfos);
    currentFilePaths_ = normalizedFilePaths;
    syncCurrentFilePath();
    hoveredPointValid_ = false;
    lastHoverQueryPosition_ = QPointF();
    lastHoverQueryTime_ = {};
    towerMarkers_.clear();
    selectedTowerIndex_ = -1;
    towerEditMode_ = TowerEditMode::None;
    towerEditTargetIndex_ = -1;
    inspectionIssues_.clear();
    hiddenInspectionIssueIndices_.clear();
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    selectedIssueIndex_ = -1;
    issueEditMode_ = IssueEditMode::None;
    inspectionRouteWaypoints_.clear();
    inspectionRouteLabels_.clear();
    inspectionRoutePartPoints_.clear();
    inspectionRouteWaypointTargetPoints_.clear();
    inspectionRouteWaypointHasTargetPoints_.clear();
    inspectionRouteWaypointAircraftYawDegs_.clear();
    inspectionRouteWaypointGimbalPitchDegs_.clear();
    inspectionRouteWaypointCameraYawDegs_.clear();
    inspectionRouteWaypointCameraPitchDegs_.clear();
    inspectionRouteWaypointTargetLabels_.clear();
    inspectionRouteVisible_ = true;
    routeWaypointDragActive_ = false;
    routeWaypointDragIndex_ = -1;
    routeWaypointDragPreviewValid_ = false;
    DataManager::instance().clearTrajectory();
    selectedInspectionRouteWaypointIndex_ = -1;
    classificationEditStore_.clear();
    classificationUndoStack_.clear();
    classificationRedoStack_.clear();
    updateSceneClickCapture();
    syncVisualizationClassificationState();
    resetMeasurementState(false);

    if (currentPointCloud_ != nullptr && !currentPointCloud_->hasColor() && visualizationOptions_.colorMode == PointCloudColorMode::Rgb) {
        visualizationOptions_.colorMode = PointCloudColorMode::Elevation;
    }

    rebuildMergedPointCloud();
    rebuildScene();
    applyViewPreset(PointCloudViewPreset::Isometric);
    updateFooter();
    updateWelcomeOverlayVisibility();
    setLoadingState(false, QString(), QString(), -1);
    emit pointCloudLoadingFinished();

    if (errorMessage != nullptr) {
        *errorMessage = normalizedFilePaths.size() == 1
            ? tr("Loaded point cloud with %1 points.")
                  .arg(formatPointCount(currentPointCloud_ != nullptr ? currentPointCloud_->size() : 0))
            : tr("Loaded %1 datasets with %2 points.")
                  .arg(QLocale().toString(normalizedFilePaths.size()))
                  .arg(formatPointCount(currentPointCloud_ != nullptr ? currentPointCloud_->size() : 0));
    }

    emit pointCloudLoaded();
    emit visualizationOptionsChanged();
    emit measurementChanged();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerEditModeChanged();
    emit towerMarkersChanged();
    emit selectedIssueChanged(selectedIssueIndex_);
    emit issueEditModeChanged();
    emit inspectionIssuesChanged();
    return true;
}

bool PointCloudViewer::appendPointCloudFiles(const QStringList& filePaths, QString* errorMessage)
{
    if (!hasLoadedPointClouds()) {
        return loadPointCloudFiles(filePaths, errorMessage);
    }

    LasReader reader;
    QString localError;
    QStringList newFilePaths;
    QList<LoadedPointCloudDataset> newDatasets;
    QList<PointCloudDatasetInfo> newDatasetInfos;

    for (const QString& filePath : filePaths) {
        const QString absolutePath = QFileInfo(filePath).absoluteFilePath();
        if (absolutePath.isEmpty()) {
            continue;
        }
        if (currentFilePaths_.contains(absolutePath, Qt::CaseInsensitive)
            || newFilePaths.contains(absolutePath, Qt::CaseInsensitive)) {
            continue;
        }
        newFilePaths.append(absolutePath);
    }

    if (newFilePaths.isEmpty()) {
        localError = tr("All selected datasets are already loaded.");
        if (errorMessage != nullptr) {
            *errorMessage = localError;
        }
        return true;
    }

    const QString loadTitle = newFilePaths.size() == 1
        ? tr("Adding %1").arg(QFileInfo(newFilePaths.constFirst()).fileName())
        : tr("Adding %1 datasets").arg(QLocale().toString(newFilePaths.size()));
    setLoadingState(true, loadTitle, tr("Preparing point cloud import..."), 0);
    emit pointCloudLoadingStarted(loadTitle);
    emit pointCloudLoadingProgress(tr("Preparing point cloud import..."), 0, 1000);

    for (int fileIndex = 0; fileIndex < newFilePaths.size(); ++fileIndex) {
        const QString& filePath = newFilePaths.at(fileIndex);
        PointCloudData datasetPointCloud;
        LasFileMetadata metadata;
        QElapsedTimer progressThrottle;
        progressThrottle.start();
        const auto progressCallback = [this, &newFilePaths, fileIndex, &filePath, &progressThrottle](const LasReadProgress& progress) {
            const double fileFraction = progress.totalPoints > 0
                ? std::clamp(static_cast<double>(progress.pointsRead) / static_cast<double>(progress.totalPoints), 0.0, 1.0)
                : 0.0;
            const bool finishedFile = progress.totalPoints > 0 && progress.pointsRead >= progress.totalPoints;
            if (!finishedFile && progressThrottle.isValid() && progressThrottle.elapsed() < 40) {
                return;
            }

            progressThrottle.restart();
            const double overallFraction = newFilePaths.isEmpty()
                ? 0.0
                : (static_cast<double>(fileIndex) + fileFraction) / static_cast<double>(newFilePaths.size());
            const int overallValue = std::clamp(static_cast<int>(std::lround(overallFraction * 1000.0)), 0, 1000);
            const int percent = std::clamp(static_cast<int>(std::lround(overallFraction * 100.0)), 0, 100);
            const QString detail = progress.totalPoints > 0
                ? tr("Reading %1 (%2/%3 points, %4%)")
                      .arg(QFileInfo(filePath).fileName())
                      .arg(formatPointCount(progress.pointsRead))
                      .arg(formatPointCount(progress.totalPoints))
                      .arg(QLocale().toString(percent))
                : tr("Reading %1 (%2 points)")
                      .arg(QFileInfo(filePath).fileName())
                      .arg(formatPointCount(progress.pointsRead));
            setLoadingState(true, pointCloudLoadingTitle_, detail, percent);
            emit pointCloudLoadingProgress(detail, overallValue, 1000);
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        };
        if (!reader.read(filePath, &datasetPointCloud, &localError, &metadata, progressCallback)) {
            setLoadingState(false, QString(), QString(), -1);
            emit pointCloudLoadingFinished();
            updateMessage(tr("Add failed"), localError);
            if (errorMessage != nullptr) {
                *errorMessage = localError;
            }
            return false;
        }

        if (datasetPointCloud.empty()) {
            localError = tr("Point cloud file is empty: %1").arg(QFileInfo(filePath).fileName());
            updateMessage(tr("Add failed"), localError);
            if (errorMessage != nullptr) {
                *errorMessage = localError;
            }
            return false;
        }

        PointCloudDatasetInfo datasetInfo;
        datasetInfo.datasetId = nextDatasetId_++;
        datasetInfo.filePath = filePath;
        datasetInfo.pointCount = metadata.pointCount;
        datasetInfo.minBounds = metadata.minBounds;
        datasetInfo.maxBounds = metadata.maxBounds;
        datasetInfo.hasColor = metadata.hasColor;
        datasetInfo.hasIntensity = metadata.hasIntensity;
        datasetInfo.hasClassification = metadata.hasClassification;
        datasetInfo.hasReturnInfo = metadata.hasReturnInfo;
        datasetInfo.hasGpsTime = metadata.hasGpsTime;
        datasetInfo.visible = true;
        datasetInfo.projectionText = metadata.projectionText;
        newDatasetInfos.append(datasetInfo);

        LoadedPointCloudDataset loadedDataset;
        loadedDataset.info = datasetInfo;
        loadedDataset.pointCloud = std::make_shared<PointCloudData>(std::move(datasetPointCloud));
        std::vector<PointRecord>& sourcePoints = loadedDataset.pointCloud->mutablePoints();
        for (std::size_t pointIndex = 0; pointIndex < sourcePoints.size(); ++pointIndex) {
            sourcePoints[pointIndex].sourceDatasetId = datasetInfo.datasetId;
            sourcePoints[pointIndex].sourcePointIndex = static_cast<std::uint32_t>(pointIndex);
        }
        newDatasets.append(std::move(loadedDataset));
    }

    for (LoadedPointCloudDataset& dataset : newDatasets) {
        loadedPointCloudDatasets_.append(std::move(dataset));
    }
    QList<PointCloudDatasetInfo> combinedDatasets = DataManager::instance().pointCloudDatasets();
    combinedDatasets.append(newDatasetInfos);
    DataManager::instance().setPointCloudDatasets(combinedDatasets);
    currentFilePaths_.append(newFilePaths);
    syncCurrentFilePath();
    hoveredPointValid_ = false;
    lastHoverQueryPosition_ = QPointF();
    lastHoverQueryTime_ = {};

    syncVisualizationClassificationState();
    rebuildMergedPointCloud();
    rebuildScene();
    updateFooter();
    updateWelcomeOverlayVisibility();
    setLoadingState(false, QString(), QString(), -1);
    emit pointCloudLoadingFinished();

    if (errorMessage != nullptr) {
        *errorMessage = newFilePaths.size() == 1
            ? tr("Added %1. Total datasets: %2, total points: %3.")
                  .arg(QFileInfo(newFilePaths.constFirst()).fileName())
                  .arg(QLocale().toString(currentFilePaths_.size()))
                  .arg(formatPointCount(currentPointCloud_ != nullptr ? currentPointCloud_->size() : 0))
            : tr("Added %1 datasets. Total datasets: %2, total points: %3.")
                  .arg(QLocale().toString(newFilePaths.size()))
                  .arg(QLocale().toString(currentFilePaths_.size()))
                  .arg(formatPointCount(currentPointCloud_ != nullptr ? currentPointCloud_->size() : 0));
    }

    emit pointCloudLoaded();
    emit visualizationOptionsChanged();
    return true;
}

void PointCloudViewer::clearPointCloud()
{
    routeRoamStopInternal(true);
    currentPointCloud_.reset();
    currentFilePath_.clear();
    currentFilePaths_.clear();
    loadedPointCloudDatasets_.clear();
    DataManager::instance().clear();
    hoveredPointValid_ = false;
    lastHoverQueryPosition_ = QPointF();
    lastHoverQueryTime_ = {};
    towerMarkers_.clear();
    selectedTowerIndex_ = -1;
    towerEditMode_ = TowerEditMode::None;
    towerEditTargetIndex_ = -1;
    inspectionIssues_.clear();
    hiddenInspectionIssueIndices_.clear();
    selectedIssueIndex_ = -1;
    issueEditMode_ = IssueEditMode::None;
    inspectionRouteWaypoints_.clear();
    inspectionRouteLabels_.clear();
    inspectionRoutePartPoints_.clear();
    inspectionRouteWaypointTargetPoints_.clear();
    inspectionRouteWaypointHasTargetPoints_.clear();
    inspectionRouteWaypointAircraftYawDegs_.clear();
    inspectionRouteWaypointGimbalPitchDegs_.clear();
    inspectionRouteWaypointCameraYawDegs_.clear();
    inspectionRouteWaypointCameraPitchDegs_.clear();
    inspectionRouteWaypointTargetLabels_.clear();
    inspectionRouteVisible_ = true;
    routeWaypointDragActive_ = false;
    routeWaypointDragIndex_ = -1;
    routeWaypointDragPreviewValid_ = false;
    selectedInspectionRouteWaypointIndex_ = -1;
    profileClassificationModeEnabled_ = false;
    profileClassificationSelectionMode_ = ProfileClassificationSelectionMode::Rectangle;
    profileClassificationTaskActive_ = false;
    profileClassificationSelectionActive_ = false;
    profileClassificationSelectionRect_ = QRectF();
    classificationTaskStartTime_ = {};
    classificationTaskScannedPoints_.store(0);
    lastClassificationTaskScannedPoints_ = 0;
    lastClassificationTaskElapsedMilliseconds_ = 0;
    if (classificationTaskStatusTimer_ != nullptr) {
        classificationTaskStatusTimer_->stop();
    }
    classificationEditStore_.clear();
    classificationUndoStack_.clear();
    classificationRedoStack_.clear();
    nextDatasetId_ = 1;
    clearSelectionRubberBand();
    clearProfileClassificationPolygonSelection();
    updateSceneClickCapture();
    syncVisualizationClassificationState();
    resetMeasurementState(false);

    if (rootGroup_.valid()) {
        rootGroup_->removeChildren(0, rootGroup_->getNumChildren());
    }

    pointCloudNode_ = nullptr;
    towerMarkersNode_ = nullptr;
    inspectionIssuesNode_ = nullptr;
    inspectionRouteNode_ = nullptr;
    measurementOverlayNode_ = nullptr;
    updateTowerOverlayWidgets();
    updateInspectionIssueOverlayWidgets();
    updateInspectionRouteOverlayWidgets();
    updateMessage(
        tr("Scene cleared"),
        tr("Open one or more LAS or LAZ files to continue."));
    updateWelcomeOverlayVisibility();

    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }

    emit pointCloudCleared();
    emit measurementChanged();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerEditModeChanged();
    emit towerMarkersChanged();
    emit selectedIssueChanged(selectedIssueIndex_);
    emit issueEditModeChanged();
    emit inspectionIssuesChanged();
    emit selectedInspectionRouteWaypointChanged(selectedInspectionRouteWaypointIndex_);
    emit inspectionRouteChanged();
}

bool PointCloudViewer::hasPointCloud() const
{
    return currentPointCloud_ != nullptr && !currentPointCloud_->empty();
}

bool PointCloudViewer::hasLoadedPointClouds() const
{
    return !currentFilePaths_.isEmpty();
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
    return hasPointCloud() ? currentPointCloud_.get() : nullptr;
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

bool PointCloudViewer::measurementEnabled() const
{
    return measurementEnabled_;
}

bool PointCloudViewer::profileClassificationModeEnabled() const
{
    return profileClassificationModeEnabled_;
}

bool PointCloudViewer::profileClassificationTaskActive() const
{
    return profileClassificationTaskActive_;
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

const QList<TowerRecord>& PointCloudViewer::towerMarkers() const
{
    return towerMarkers_;
}

const QList<InspectionIssue>& PointCloudViewer::inspectionIssues() const
{
    return inspectionIssues_;
}

const QList<PointRecord>& PointCloudViewer::inspectionRouteWaypoints() const
{
    return inspectionRouteWaypoints_;
}

QColor PointCloudViewer::inspectionRouteWaypointColor() const
{
    return inspectionRouteWaypointColor_;
}

QColor PointCloudViewer::inspectionRoutePartPointColor() const
{
    return inspectionRoutePartPointColor_;
}

QColor PointCloudViewer::inspectionRouteTrajectoryColor() const
{
    return inspectionRouteTrajectoryColor_;
}

int PointCloudViewer::selectedTowerIndex() const
{
    return selectedTowerIndex_;
}

int PointCloudViewer::selectedIssueIndex() const
{
    return selectedIssueIndex_;
}

int PointCloudViewer::selectedInspectionRouteWaypointIndex() const
{
    return selectedInspectionRouteWaypointIndex_;
}

TowerEditMode PointCloudViewer::towerEditMode() const
{
    return towerEditMode_;
}

int PointCloudViewer::towerEditTargetIndex() const
{
    return towerEditTargetIndex_;
}

IssueEditMode PointCloudViewer::issueEditMode() const
{
    return issueEditMode_;
}

const QSet<int>& PointCloudViewer::profileClassificationSourceClasses() const
{
    return profileClassificationSourceClasses_;
}

int PointCloudViewer::profileClassificationTargetClass() const
{
    return profileClassificationTargetClass_;
}

ProfileClassificationSelectionMode PointCloudViewer::profileClassificationSelectionMode() const
{
    return profileClassificationSelectionMode_;
}

bool PointCloudViewer::canUndoClassificationEdits() const
{
    return !classificationUndoStack_.isEmpty();
}

bool PointCloudViewer::canRedoClassificationEdits() const
{
    return !classificationRedoStack_.isEmpty();
}

int PointCloudViewer::classificationEditedPointCount() const
{
    return classificationEditStore_.editedPointCount();
}

const ClassificationEditStore& PointCloudViewer::classificationEditStore() const
{
    return classificationEditStore_;
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

void PointCloudViewer::setClassificationColor(int classification, const QColor& color)
{
    if (classification > 255 || !color.isValid()) {
        return;
    }

    if (classification < 0) {
        setClassificationFallbackColor(color);
        return;
    }

    const auto colorIt = visualizationOptions_.classificationColors.constFind(classification);
    if (colorIt != visualizationOptions_.classificationColors.constEnd() && colorIt.value() == color) {
        return;
    }

    visualizationOptions_.classificationColors.insert(classification, color);
    if (visualizationOptions_.colorMode == PointCloudColorMode::Classification && hasPointCloud()) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClassificationVisible(int classification, bool visible)
{
    if (classification < -1 || classification > 255) {
        return;
    }

    const bool currentVisible = visualizationOptions_.classificationVisibility.value(
        classification,
        visualizationOptions_.classificationVisibility.value(-1, true));
    if (currentVisible == visible) {
        return;
    }

    visualizationOptions_.classificationVisibility.insert(classification, visible);
    if (hasPointCloud()) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClassificationColorMap(const QMap<int, QColor>& colorMap)
{
    if (visualizationOptions_.classificationColors == colorMap) {
        return;
    }

    visualizationOptions_.classificationColors = colorMap;
    if (visualizationOptions_.colorMode == PointCloudColorMode::Classification && hasPointCloud()) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClassificationVisibilityMap(const QMap<int, bool>& visibilityMap)
{
    if (visualizationOptions_.classificationVisibility == visibilityMap) {
        return;
    }

    visualizationOptions_.classificationVisibility = visibilityMap;
    if (!visualizationOptions_.classificationVisibility.contains(-1)) {
        visualizationOptions_.classificationVisibility.insert(-1, true);
    }
    if (hasPointCloud()) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClassificationFallbackColor(const QColor& color)
{
    if (!color.isValid() || visualizationOptions_.classificationFallbackColor == color) {
        return;
    }

    visualizationOptions_.classificationFallbackColor = color;
    if (visualizationOptions_.colorMode == PointCloudColorMode::Classification && hasPointCloud()) {
        rebuildScene();
    }

    updateFooter();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::resetClassificationColors()
{
    visualizationOptions_.classificationColors = defaultPointClassificationColors();
    visualizationOptions_.classificationVisibility = defaultPointClassificationVisibility();
    visualizationOptions_.classificationFallbackColor = defaultPointClassificationFallbackColor();

    if (hasPointCloud()) {
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

void PointCloudViewer::setProfileClassificationModeEnabled(bool enabled)
{
    if (enabled && (!hasPointCloud() || pointCloudLoadingActive_ || tiledPointCloudModeActive_)) {
        emit measurementMessage(tr("Wait until the current point cloud is fully ready before starting profile classification."), true);
        return;
    }

    if (profileClassificationModeEnabled_ == enabled) {
        return;
    }

    if (enabled) {
        if (measurementEnabled_) {
            setMeasurementEnabled(false);
        }
        if (towerEditMode_ != TowerEditMode::None) {
            cancelTowerEditMode();
        }
        if (issueEditMode_ != IssueEditMode::None) {
            cancelIssueEditMode();
        }
    } else {
        clearSelectionRubberBand();
        clearProfileClassificationPolygonSelection();
    }

    profileClassificationModeEnabled_ = enabled;
    profileClassificationSelectionActive_ = false;
    profileClassificationSelectionRect_ = QRectF();
    if (enabled) {
        clearProfileClassificationPolygonSelection();
    }
    updateSceneClickCapture();
    updateProfileClassificationPolygonOverlay();
    updateFooter();
    emit profileClassificationModeChanged(profileClassificationModeEnabled_);
    emit profileClassificationStateChanged();
    emit measurementMessage(
        !profileClassificationModeEnabled_
            ? tr("Profile classification mode disabled.")
            : (profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
                ? tr("Profile classification mode enabled (polygon). Left-click to add vertices, double-click to apply, right-click to undo one vertex, drag to adjust view, and press Esc to clear or exit.")
                : tr("Profile classification mode enabled (rectangle). Drag a rectangle to classify source classes, hold Alt and drag left mouse to adjust view, right-click to exit, and press Esc to cancel.")),
        false);
}

void PointCloudViewer::setProfileClassificationSelectionMode(ProfileClassificationSelectionMode mode)
{
    if (profileClassificationSelectionMode_ == mode || profileClassificationTaskActive_) {
        return;
    }

    profileClassificationSelectionMode_ = mode;
    clearSelectionRubberBand();
    clearProfileClassificationPolygonSelection();
    updateSceneClickCapture();
    updateProfileClassificationPolygonOverlay();
    updateFooter();
    emit profileClassificationStateChanged();

    if (!profileClassificationModeEnabled_) {
        return;
    }

    emit measurementMessage(
        profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
            ? tr("Switched to polygon selection. Left-click to add vertices, double-click to apply, and right-click to undo one vertex.")
            : tr("Switched to rectangle selection. Drag a rectangle to apply profile classification."),
        false);
}

void PointCloudViewer::setProfileClassificationSourceClasses(const QSet<int>& classifications)
{
    if (profileClassificationSourceClasses_ == classifications) {
        return;
    }

    profileClassificationSourceClasses_ = classifications;
    emit profileClassificationStateChanged();
}

void PointCloudViewer::setProfileClassificationTargetClass(int classification)
{
    if (profileClassificationTargetClass_ == classification) {
        return;
    }

    profileClassificationTargetClass_ = classification;
    emit profileClassificationStateChanged();
}

void PointCloudViewer::undoClassificationEdit()
{
    if (classificationUndoStack_.isEmpty() || profileClassificationTaskActive_) {
        return;
    }

    const ClassificationEditBatch batch = classificationUndoStack_.takeLast();
    classificationEditStore_.revertBatch(batch);
    classificationRedoStack_.append(batch);
    syncVisualizationClassificationState();
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
    emit classificationEditsChanged();
    emit profileClassificationStateChanged();
    emit measurementMessage(
        tr("Reverted %1 profile classification point(s).")
            .arg(QLocale().toString(batch.changedCount)),
        false);
}

void PointCloudViewer::redoClassificationEdit()
{
    if (classificationRedoStack_.isEmpty() || profileClassificationTaskActive_) {
        return;
    }

    const ClassificationEditBatch batch = classificationRedoStack_.takeLast();
    classificationEditStore_.applyBatch(batch);
    classificationUndoStack_.append(batch);
    syncVisualizationClassificationState();
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
    emit classificationEditsChanged();
    emit profileClassificationStateChanged();
    emit measurementMessage(
        tr("Reapplied %1 profile classification point(s).")
            .arg(QLocale().toString(batch.changedCount)),
        false);
}

void PointCloudViewer::clearClassificationEdits()
{
    if (classificationEditStore_.isEmpty() && classificationUndoStack_.isEmpty() && classificationRedoStack_.isEmpty()) {
        return;
    }

    classificationEditStore_.clear();
    classificationUndoStack_.clear();
    classificationRedoStack_.clear();
    syncVisualizationClassificationState();
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
    emit classificationEditsChanged();
    emit profileClassificationStateChanged();
    emit measurementMessage(tr("Cleared all project profile classification edits."), false);
}

void PointCloudViewer::setClassificationEditStore(const ClassificationEditStore& store)
{
    classificationEditStore_ = store;
    classificationUndoStack_.clear();
    classificationRedoStack_.clear();
    syncVisualizationClassificationState();
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
    emit classificationEditsChanged();
    emit profileClassificationStateChanged();
}

void PointCloudViewer::commitClassificationEditsToPointCloudData()
{
    if (classificationEditStore_.isEmpty()) {
        return;
    }

    const ClassificationEditStore::StoreMap editsByDataset = classificationEditStore_.editsByDataset();
    bool changedAnyPoint = false;
    QList<PointCloudDatasetInfo> datasetInfos = DataManager::instance().pointCloudDatasets();

    for (LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (dataset.pointCloud == nullptr) {
            continue;
        }

        auto editsIt = editsByDataset.constFind(dataset.info.filePath);
        if (editsIt == editsByDataset.constEnd()) {
            for (auto probeIt = editsByDataset.constBegin(); probeIt != editsByDataset.constEnd(); ++probeIt) {
                if (probeIt.key().compare(dataset.info.filePath, Qt::CaseInsensitive) == 0) {
                    editsIt = probeIt;
                    break;
                }
            }
        }
        if (editsIt == editsByDataset.constEnd()) {
            continue;
        }

        std::vector<PointRecord>& points = dataset.pointCloud->mutablePoints();
        bool datasetHasClassification = dataset.info.hasClassification;
        for (auto pointIt = editsIt->constBegin(); pointIt != editsIt->constEnd(); ++pointIt) {
            const std::size_t pointIndex = static_cast<std::size_t>(pointIt.key());
            if (pointIndex >= points.size()) {
                continue;
            }

            PointRecord& point = points[pointIndex];
            point.classification = static_cast<std::uint8_t>(std::clamp(pointIt.value(), 0, 255));
            point.hasClassification = true;
            datasetHasClassification = true;
            changedAnyPoint = true;
        }

        dataset.info.hasClassification = datasetHasClassification;
        for (PointCloudDatasetInfo& info : datasetInfos) {
            if (info.filePath.compare(dataset.info.filePath, Qt::CaseInsensitive) == 0) {
                info.hasClassification = datasetHasClassification;
                break;
            }
        }
    }

    if (changedAnyPoint) {
        DataManager::instance().setPointCloudDatasets(datasetInfos);
    }

    classificationEditStore_.clear();
    classificationUndoStack_.clear();
    classificationRedoStack_.clear();
    syncVisualizationClassificationState();
    rebuildMergedPointCloud();
    rebuildScene();
    updateFooter();
    emit visualizationOptionsChanged();
    emit classificationEditsChanged();
    emit profileClassificationStateChanged();
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

    if (enabled && profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }

    measurementEnabled_ = enabled;
    updateSceneClickCapture();

    if (!measurementEnabled_) {
        resetMeasurementState();
        emit measurementMessage(tr("Measurement mode disabled."), false);
    } else {
        resetMeasurementState();
        emit measurementMessage(tr("Measurement mode enabled. Click points to measure, and right-click to undo the last point."), false);
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

    const bool rectangleSelectionEnabled =
        profileClassificationModeEnabled_
        && profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Rectangle;
    const bool polygonSelectionEnabled =
        profileClassificationModeEnabled_
        && profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon;

    osgWidget_->setRectangleSelectionEnabled(rectangleSelectionEnabled);
    const bool sceneClickEnabled =
        polygonSelectionEnabled
        || (!profileClassificationModeEnabled_
            && (measurementEnabled_
            || towerEditMode_ != TowerEditMode::None
            || issueEditMode_ != IssueEditMode::None
            || !towerMarkers_.isEmpty()
            || !inspectionIssues_.isEmpty()
            || (inspectionRouteVisible_ && !inspectionRouteWaypoints_.isEmpty())));
    osgWidget_->setSceneClickModeEnabled(sceneClickEnabled);
    updateProfileClassificationPolygonOverlay();
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

    TowerRecord towerMarker;
    towerMarker.index = index;
    towerMarker.name = trimmedName;
    towerMarker.point = point;
    towerMarkers_.insert(index, towerMarker);
    normalizeTowerMarkerIndices();
    selectedTowerIndex_ = index;
    if (towerEditMode_ == TowerEditMode::InsertBeforeSelected) {
        towerEditTargetIndex_ = selectedTowerIndex_;
    }
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

void PointCloudViewer::setTowerMarkers(const QList<TowerRecord>& towerMarkers)
{
    towerMarkers_ = towerMarkers;
    normalizeTowerMarkerIndices();
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

bool PointCloudViewer::setTowerRecord(int index, const TowerRecord& towerRecord)
{
    if (index < 0 || index >= towerMarkers_.size() || towerRecord.name.trimmed().isEmpty()) {
        return false;
    }

    towerMarkers_[index] = towerRecord;
    towerMarkers_[index].index = index;
    towerMarkers_[index].name = towerRecord.name.trimmed();
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
    if ((towerEditMode_ == TowerEditMode::InsertBeforeSelected || towerEditMode_ == TowerEditMode::MoveSelected)
        && selectedTowerIndex_ >= 0) {
        towerEditTargetIndex_ = selectedTowerIndex_;
    }
    selectedIssueIndex_ = -1;
    refreshTowerMarkersOverlay();
    refreshInspectionIssuesOverlay();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit selectedIssueChanged(selectedIssueIndex_);
}

bool PointCloudViewer::removeTowerMarker(int index)
{
    if (index < 0 || index >= towerMarkers_.size()) {
        return false;
    }

    towerMarkers_.removeAt(index);
    normalizeTowerMarkerIndices();
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
    towerMarkers_[index].index = index;
    selectedTowerIndex_ = index;
    if (towerEditMode_ == TowerEditMode::MoveSelected) {
        towerEditTargetIndex_ = selectedTowerIndex_;
    }
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

void PointCloudViewer::normalizeTowerMarkerIndices()
{
    for (int index = 0; index < towerMarkers_.size(); ++index) {
        towerMarkers_[index].index = index;
    }
}

void PointCloudViewer::beginTowerAddMode()
{
    if (profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }
    towerEditMode_ = TowerEditMode::AddAfterLast;
    towerEditTargetIndex_ = -1;
    towerAddModeStartCount_ = towerMarkers_.size();
    cancelIssueEditMode();
    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    } else {
        updateSceneClickCapture();
    }
    emit towerEditModeChanged();
}

void PointCloudViewer::beginTowerInsertMode(int beforeIndex)
{
    if (profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }
    if (beforeIndex < 0 || beforeIndex >= towerMarkers_.size()) {
        return;
    }

    selectedTowerIndex_ = beforeIndex;
    selectedIssueIndex_ = -1;
    towerEditMode_ = TowerEditMode::InsertBeforeSelected;
    towerEditTargetIndex_ = beforeIndex;
    towerAddModeStartCount_ = towerMarkers_.size();
    cancelIssueEditMode();
    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    } else {
        updateSceneClickCapture();
    }
    refreshTowerMarkersOverlay();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerEditModeChanged();
}

void PointCloudViewer::beginTowerMoveMode(int towerIndex)
{
    if (profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }
    if (towerIndex < 0 || towerIndex >= towerMarkers_.size()) {
        return;
    }

    selectedTowerIndex_ = towerIndex;
    selectedIssueIndex_ = -1;
    towerEditMode_ = TowerEditMode::MoveSelected;
    towerEditTargetIndex_ = towerIndex;
    towerAddModeStartCount_ = towerMarkers_.size();
    cancelIssueEditMode();
    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    } else {
        updateSceneClickCapture();
    }
    refreshTowerMarkersOverlay();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerEditModeChanged();
}

void PointCloudViewer::cancelTowerEditMode()
{
    if (towerEditMode_ == TowerEditMode::None) {
        return;
    }

    towerEditMode_ = TowerEditMode::None;
    towerEditTargetIndex_ = -1;
    towerAddModeStartCount_ = towerMarkers_.size();
    updateSceneClickCapture();
    emit towerEditModeChanged();
}

void PointCloudViewer::setInspectionIssues(const QList<InspectionIssue>& issues)
{
    inspectionIssues_ = issues;
    hiddenInspectionIssueIndices_.clear();
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    selectedIssueIndex_ = inspectionIssues_.isEmpty() ? -1 : std::clamp(selectedIssueIndex_, 0, inspectionIssues_.size() - 1);
    updateSceneClickCapture();
    refreshInspectionIssuesOverlay();
    updateFooter();
    emit selectedIssueChanged(selectedIssueIndex_);
    emit inspectionIssuesChanged();
}

bool PointCloudViewer::addInspectionIssue(const InspectionIssue& issue)
{
    if (issue.title.trimmed().isEmpty()) {
        return false;
    }

    InspectionIssue normalizedIssue = issue;
    normalizedIssue.id = normalizedIssue.id.trimmed().isEmpty() ? issueDefaultId() : normalizedIssue.id.trimmed();
    normalizedIssue.title = normalizedIssue.title.trimmed();
    if (normalizedIssue.createdAt.trimmed().isEmpty()) {
        normalizedIssue.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    }
    inspectionIssues_.append(normalizedIssue);
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    selectedIssueIndex_ = inspectionIssues_.size() - 1;
    selectedTowerIndex_ = -1;
    updateSceneClickCapture();
    refreshInspectionIssuesOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit selectedIssueChanged(selectedIssueIndex_);
    emit inspectionIssuesChanged();
    return true;
}

bool PointCloudViewer::updateInspectionIssue(int index, const InspectionIssue& issue)
{
    if (index < 0 || index >= inspectionIssues_.size() || issue.title.trimmed().isEmpty()) {
        return false;
    }

    InspectionIssue normalizedIssue = issue;
    normalizedIssue.id = normalizedIssue.id.trimmed().isEmpty() ? issueDefaultId() : normalizedIssue.id.trimmed();
    normalizedIssue.title = normalizedIssue.title.trimmed();
    if (normalizedIssue.createdAt.trimmed().isEmpty()) {
        normalizedIssue.createdAt = inspectionIssues_.at(index).createdAt;
    }
    inspectionIssues_[index] = normalizedIssue;
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    refreshInspectionIssuesOverlay();
    emit inspectionIssuesChanged();
    return true;
}

bool PointCloudViewer::removeInspectionIssue(int index)
{
    if (index < 0 || index >= inspectionIssues_.size()) {
        return false;
    }

    inspectionIssues_.removeAt(index);
    QSet<int> remappedHiddenIndices;
    for (int hiddenIndex : hiddenInspectionIssueIndices_) {
        if (hiddenIndex == index) {
            continue;
        }
        remappedHiddenIndices.insert(hiddenIndex > index ? hiddenIndex - 1 : hiddenIndex);
    }
    hiddenInspectionIssueIndices_ = std::move(remappedHiddenIndices);
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    selectedIssueIndex_ = inspectionIssues_.isEmpty() ? -1 : std::clamp(index, 0, inspectionIssues_.size() - 1);
    updateSceneClickCapture();
    refreshInspectionIssuesOverlay();
    updateFooter();
    emit selectedIssueChanged(selectedIssueIndex_);
    emit inspectionIssuesChanged();
    return true;
}

void PointCloudViewer::clearInspectionIssues()
{
    if (inspectionIssues_.isEmpty()) {
        return;
    }

    inspectionIssues_.clear();
    hiddenInspectionIssueIndices_.clear();
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    selectedIssueIndex_ = -1;
    cancelIssueEditMode();
    updateSceneClickCapture();
    refreshInspectionIssuesOverlay();
    updateFooter();
    emit selectedIssueChanged(selectedIssueIndex_);
    emit inspectionIssuesChanged();
}

void PointCloudViewer::setSelectedIssueIndex(int index)
{
    const int normalizedIndex = (index >= 0 && index < inspectionIssues_.size()) ? index : -1;
    if (selectedIssueIndex_ == normalizedIndex) {
        return;
    }

    selectedIssueIndex_ = normalizedIndex;
    if (selectedIssueIndex_ >= 0) {
        selectedTowerIndex_ = -1;
        emit selectedTowerChanged(selectedTowerIndex_);
    }
    refreshInspectionIssuesOverlay();
    refreshTowerMarkersOverlay();
    emit selectedIssueChanged(selectedIssueIndex_);
}

void PointCloudViewer::setInspectionRouteDisplayData(const InspectionRouteDisplayData& displayData)
{
    routeRoamStopInternal(false);
    inspectionRouteWaypoints_ = displayData.waypoints;
    inspectionRouteLabels_ = displayData.labels;
    inspectionRoutePartPoints_ = displayData.partPoints;
    inspectionRoutePartLabels_ = displayData.partLabels;
    inspectionRoutePartPointIndices_ = displayData.partPointIndices;
    inspectionRouteWaypointTargetPoints_ = displayData.waypointTargetPoints;
    inspectionRouteWaypointHasTargetPoints_ = displayData.waypointHasTargetPoints;
    inspectionRouteWaypointAircraftYawDegs_ = displayData.waypointAircraftYawDegs;
    inspectionRouteWaypointGimbalPitchDegs_ = displayData.waypointGimbalPitchDegs;
    inspectionRouteWaypointCameraYawDegs_ = displayData.waypointCameraYawDegs;
    inspectionRouteWaypointCameraPitchDegs_ = displayData.waypointCameraPitchDegs;
    inspectionRouteWaypointFocalLengthRatios_ = displayData.waypointFocalLengthRatios;
    inspectionRouteWaypointTargetLabels_ = displayData.waypointTargetLabels;
    inspectionRouteWaypointAllTargetPoints_ = displayData.waypointAllTargetPoints;
    inspectionRouteWaypointAllTargetPartIndices_ = displayData.waypointAllTargetPartIndices;
    inspectionRouteWaypointAllCameraYawDegs_ = displayData.waypointAllCameraYawDegs;
    inspectionRouteWaypointAllCameraPitchDegs_ = displayData.waypointAllCameraPitchDegs;
    inspectionRouteWaypointAllFocalLengthRatios_ = displayData.waypointAllFocalLengthRatios;
    inspectionRouteWaypointAllTargetLabels_ = displayData.waypointAllTargetLabels;
    inspectionRouteVisible_ = true;
    routeWaypointDragActive_ = false;
    routeWaypointDragIndex_ = -1;
    routeWaypointDragPreviewValid_ = false;
    DataManager::instance().setTrajectory(
        DataManager::instance().trajectoryItem().name.trimmed().isEmpty()
            ? tr("Inspection Route")
            : DataManager::instance().trajectoryItem().name,
        inspectionRouteWaypoints_,
        inspectionRouteVisible_);
    if (inspectionRouteLabels_.size() < inspectionRouteWaypoints_.size()) {
        for (int index = inspectionRouteLabels_.size(); index < inspectionRouteWaypoints_.size(); ++index) {
            inspectionRouteLabels_.append(QString::number(index + 1));
        }
    } else if (inspectionRouteLabels_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteLabels_.erase(
            inspectionRouteLabels_.begin() + inspectionRouteWaypoints_.size(),
            inspectionRouteLabels_.end());
    }

    while (inspectionRoutePartLabels_.size() < inspectionRoutePartPoints_.size()) {
        inspectionRoutePartLabels_.append(QString());
    }
    while (inspectionRoutePartLabels_.size() > inspectionRoutePartPoints_.size()) {
        inspectionRoutePartLabels_.removeLast();
    }

    while (inspectionRoutePartPointIndices_.size() < inspectionRoutePartPoints_.size()) {
        inspectionRoutePartPointIndices_.append(-1);
    }
    while (inspectionRoutePartPointIndices_.size() > inspectionRoutePartPoints_.size()) {
        inspectionRoutePartPointIndices_.removeLast();
    }

    if (inspectionRouteWaypointTargetPoints_.size() < inspectionRouteWaypoints_.size()) {
        while (inspectionRouteWaypointTargetPoints_.size() < inspectionRouteWaypoints_.size()) {
            inspectionRouteWaypointTargetPoints_.append(PointRecord());
        }
    } else if (inspectionRouteWaypointTargetPoints_.size() > inspectionRouteWaypoints_.size()) {
        while (inspectionRouteWaypointTargetPoints_.size() > inspectionRouteWaypoints_.size()) {
            inspectionRouteWaypointTargetPoints_.removeLast();
        }
    }

    if (inspectionRouteWaypointHasTargetPoints_.size() < inspectionRouteWaypoints_.size()) {
        while (inspectionRouteWaypointHasTargetPoints_.size() < inspectionRouteWaypoints_.size()) {
            inspectionRouteWaypointHasTargetPoints_.append(false);
        }
    } else if (inspectionRouteWaypointHasTargetPoints_.size() > inspectionRouteWaypoints_.size()) {
        while (inspectionRouteWaypointHasTargetPoints_.size() > inspectionRouteWaypoints_.size()) {
            inspectionRouteWaypointHasTargetPoints_.removeLast();
        }
    }

    while (inspectionRouteWaypointAircraftYawDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAircraftYawDegs_.append(0.0);
    }
    while (inspectionRouteWaypointAircraftYawDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAircraftYawDegs_.removeLast();
    }

    while (inspectionRouteWaypointGimbalPitchDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointGimbalPitchDegs_.append(0.0);
    }
    while (inspectionRouteWaypointGimbalPitchDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointGimbalPitchDegs_.removeLast();
    }

    while (inspectionRouteWaypointCameraYawDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointCameraYawDegs_.append(0.0);
    }
    while (inspectionRouteWaypointCameraYawDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointCameraYawDegs_.removeLast();
    }

    while (inspectionRouteWaypointCameraPitchDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointCameraPitchDegs_.append(0.0);
    }
    while (inspectionRouteWaypointCameraPitchDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointCameraPitchDegs_.removeLast();
    }

    while (inspectionRouteWaypointFocalLengthRatios_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointFocalLengthRatios_.append(kRoutePreviewDefaultFocalLengthRatio);
    }
    while (inspectionRouteWaypointFocalLengthRatios_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointFocalLengthRatios_.removeLast();
    }

    while (inspectionRouteWaypointTargetLabels_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointTargetLabels_.append(QString());
    }
    while (inspectionRouteWaypointTargetLabels_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointTargetLabels_.removeLast();
    }

    while (inspectionRouteWaypointAllTargetPoints_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetPoints_.append(QList<PointRecord>());
    }
    while (inspectionRouteWaypointAllTargetPoints_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetPoints_.removeLast();
    }

    while (inspectionRouteWaypointAllTargetPartIndices_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetPartIndices_.append(QList<int>());
    }
    while (inspectionRouteWaypointAllTargetPartIndices_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetPartIndices_.removeLast();
    }

    while (inspectionRouteWaypointAllCameraYawDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllCameraYawDegs_.append(QList<double>());
    }
    while (inspectionRouteWaypointAllCameraYawDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllCameraYawDegs_.removeLast();
    }

    while (inspectionRouteWaypointAllCameraPitchDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllCameraPitchDegs_.append(QList<double>());
    }
    while (inspectionRouteWaypointAllCameraPitchDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllCameraPitchDegs_.removeLast();
    }

    while (inspectionRouteWaypointAllFocalLengthRatios_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllFocalLengthRatios_.append(QList<double>());
    }
    while (inspectionRouteWaypointAllFocalLengthRatios_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllFocalLengthRatios_.removeLast();
    }

    while (inspectionRouteWaypointAllTargetLabels_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetLabels_.append(QStringList());
    }
    while (inspectionRouteWaypointAllTargetLabels_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetLabels_.removeLast();
    }

    for (int waypointIndex = 0; waypointIndex < inspectionRouteWaypoints_.size(); ++waypointIndex) {
        QList<PointRecord>& allTargetPoints = inspectionRouteWaypointAllTargetPoints_[waypointIndex];
        QList<int>& allTargetPartIndices = inspectionRouteWaypointAllTargetPartIndices_[waypointIndex];
        QList<double>& allCameraYawDegs = inspectionRouteWaypointAllCameraYawDegs_[waypointIndex];
        QList<double>& allCameraPitchDegs = inspectionRouteWaypointAllCameraPitchDegs_[waypointIndex];
        QList<double>& allFocalLengthRatios = inspectionRouteWaypointAllFocalLengthRatios_[waypointIndex];
        QStringList& allTargetLabels = inspectionRouteWaypointAllTargetLabels_[waypointIndex];

        const bool hasLegacyTarget =
            waypointIndex < inspectionRouteWaypointHasTargetPoints_.size()
            && inspectionRouteWaypointHasTargetPoints_.at(waypointIndex)
            && waypointIndex < inspectionRouteWaypointTargetPoints_.size();
        if (allTargetPoints.isEmpty() && hasLegacyTarget) {
            allTargetPoints.append(inspectionRouteWaypointTargetPoints_.at(waypointIndex));
        }

        while (allTargetPartIndices.size() < allTargetPoints.size()) {
            allTargetPartIndices.append(-1);
        }
        while (allTargetPartIndices.size() > allTargetPoints.size()) {
            allTargetPartIndices.removeLast();
        }

        const double legacyCameraYaw = waypointIndex < inspectionRouteWaypointCameraYawDegs_.size()
            ? inspectionRouteWaypointCameraYawDegs_.at(waypointIndex)
            : 0.0;
        const double legacyCameraPitch = waypointIndex < inspectionRouteWaypointCameraPitchDegs_.size()
            ? inspectionRouteWaypointCameraPitchDegs_.at(waypointIndex)
            : 0.0;
        const double legacyFocalLengthRatio = waypointIndex < inspectionRouteWaypointFocalLengthRatios_.size()
            ? normalizedRoutePreviewFocalLengthRatio(inspectionRouteWaypointFocalLengthRatios_.at(waypointIndex))
            : kRoutePreviewDefaultFocalLengthRatio;
        while (allCameraYawDegs.size() < allTargetPoints.size()) {
            allCameraYawDegs.append(legacyCameraYaw);
        }
        while (allCameraYawDegs.size() > allTargetPoints.size()) {
            allCameraYawDegs.removeLast();
        }

        while (allCameraPitchDegs.size() < allTargetPoints.size()) {
            allCameraPitchDegs.append(legacyCameraPitch);
        }
        while (allCameraPitchDegs.size() > allTargetPoints.size()) {
            allCameraPitchDegs.removeLast();
        }

        while (allFocalLengthRatios.size() < allTargetPoints.size()) {
            allFocalLengthRatios.append(legacyFocalLengthRatio);
        }
        while (allFocalLengthRatios.size() > allTargetPoints.size()) {
            allFocalLengthRatios.removeLast();
        }
        for (double& focalLengthRatio : allFocalLengthRatios) {
            focalLengthRatio = normalizedRoutePreviewFocalLengthRatio(focalLengthRatio);
        }

        while (allTargetLabels.size() < allTargetPoints.size()) {
            allTargetLabels.append(tr("Target %1").arg(QLocale().toString(allTargetLabels.size() + 1)));
        }
        while (allTargetLabels.size() > allTargetPoints.size()) {
            allTargetLabels.removeLast();
        }

        if (!allTargetPoints.isEmpty()) {
            inspectionRouteWaypointHasTargetPoints_[waypointIndex] = true;
            inspectionRouteWaypointTargetPoints_[waypointIndex] = allTargetPoints.constFirst();
            inspectionRouteWaypointCameraYawDegs_[waypointIndex] = allCameraYawDegs.isEmpty() ? 0.0 : allCameraYawDegs.constFirst();
            inspectionRouteWaypointCameraPitchDegs_[waypointIndex] = allCameraPitchDegs.isEmpty() ? 0.0 : allCameraPitchDegs.constFirst();
            inspectionRouteWaypointFocalLengthRatios_[waypointIndex] =
                allFocalLengthRatios.isEmpty() ? legacyFocalLengthRatio : allFocalLengthRatios.constFirst();
            if (inspectionRouteWaypointTargetLabels_.at(waypointIndex).trimmed().isEmpty()) {
                inspectionRouteWaypointTargetLabels_[waypointIndex] = allTargetLabels.isEmpty() ? QString() : allTargetLabels.constFirst();
            }
        } else {
            inspectionRouteWaypointHasTargetPoints_[waypointIndex] = false;
            inspectionRouteWaypointTargetPoints_[waypointIndex] = PointRecord();
            inspectionRouteWaypointCameraYawDegs_[waypointIndex] = 0.0;
            inspectionRouteWaypointCameraPitchDegs_[waypointIndex] = 0.0;
            inspectionRouteWaypointFocalLengthRatios_[waypointIndex] = legacyFocalLengthRatio;
        }
    }

    selectedInspectionRouteWaypointIndex_ =
        inspectionRouteWaypoints_.isEmpty()
            ? -1
            : std::clamp(selectedInspectionRouteWaypointIndex_, 0, inspectionRouteWaypoints_.size() - 1);
    selectedInspectionRouteWaypointTargetIndex_ =
        normalizeInspectionRouteWaypointTargetIndex(selectedInspectionRouteWaypointIndex_, selectedInspectionRouteWaypointTargetIndex_);

    refreshInspectionRouteOverlay();
    updateFooter();
    emit selectedInspectionRouteWaypointChanged(selectedInspectionRouteWaypointIndex_);
    emit inspectionRouteChanged();
}

void PointCloudViewer::setInspectionRouteWaypoints(const QList<PointRecord>& waypoints, const QStringList& labels)
{
    InspectionRouteDisplayData displayData;
    displayData.waypoints = waypoints;
    displayData.labels = labels;
    setInspectionRouteDisplayData(displayData);
}

RouteLabelDisplayMode PointCloudViewer::inspectionRouteWaypointLabelDisplayMode() const
{
    return routeWaypointLabelDisplayMode_;
}

RouteLabelDisplayMode PointCloudViewer::inspectionRoutePartLabelDisplayMode() const
{
    return routePartLabelDisplayMode_;
}

void PointCloudViewer::setInspectionRouteWaypointLabelDisplayMode(RouteLabelDisplayMode mode)
{
    if (routeWaypointLabelDisplayMode_ == mode) {
        return;
    }

    routeWaypointLabelDisplayMode_ = mode;
    updateInspectionRouteOverlayWidgets();
    updateRouteCameraPreviewOverlay();
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::setInspectionRoutePartLabelDisplayMode(RouteLabelDisplayMode mode)
{
    if (routePartLabelDisplayMode_ == mode) {
        return;
    }

    routePartLabelDisplayMode_ = mode;
    updateInspectionRouteOverlayWidgets();
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::setInspectionRouteWaypointColor(const QColor& color)
{
    if (!color.isValid() || inspectionRouteWaypointColor_ == color) {
        return;
    }

    inspectionRouteWaypointColor_ = color;
    refreshInspectionRouteOverlay();
}

void PointCloudViewer::setInspectionRoutePartPointColor(const QColor& color)
{
    if (!color.isValid() || inspectionRoutePartPointColor_ == color) {
        return;
    }

    inspectionRoutePartPointColor_ = color;
    refreshInspectionRouteOverlay();
}

void PointCloudViewer::setInspectionRouteTrajectoryColor(const QColor& color)
{
    if (!color.isValid() || inspectionRouteTrajectoryColor_ == color) {
        return;
    }

    inspectionRouteTrajectoryColor_ = color;
    refreshInspectionRouteOverlay();
}

void PointCloudViewer::clearInspectionRouteWaypoints()
{
    if (inspectionRouteWaypoints_.isEmpty()) {
        return;
    }

    routeRoamStopInternal(false);
    inspectionRouteWaypoints_.clear();
    inspectionRouteLabels_.clear();
    inspectionRoutePartPoints_.clear();
    inspectionRoutePartLabels_.clear();
    inspectionRoutePartPointIndices_.clear();
    inspectionRouteWaypointTargetPoints_.clear();
    inspectionRouteWaypointHasTargetPoints_.clear();
    inspectionRouteWaypointAircraftYawDegs_.clear();
    inspectionRouteWaypointGimbalPitchDegs_.clear();
    inspectionRouteWaypointCameraYawDegs_.clear();
    inspectionRouteWaypointCameraPitchDegs_.clear();
    inspectionRouteWaypointFocalLengthRatios_.clear();
    inspectionRouteWaypointTargetLabels_.clear();
    inspectionRouteWaypointAllTargetPoints_.clear();
    inspectionRouteWaypointAllTargetPartIndices_.clear();
    inspectionRouteWaypointAllCameraYawDegs_.clear();
    inspectionRouteWaypointAllCameraPitchDegs_.clear();
    inspectionRouteWaypointAllFocalLengthRatios_.clear();
    inspectionRouteWaypointAllTargetLabels_.clear();
    inspectionRouteVisible_ = true;
    routeWaypointDragActive_ = false;
    routeWaypointDragIndex_ = -1;
    routeWaypointDragPreviewValid_ = false;
    DataManager::instance().clearTrajectory();
    selectedInspectionRouteWaypointIndex_ = -1;
    selectedInspectionRouteWaypointTargetIndex_ = -1;
    refreshInspectionRouteOverlay();
    updateFooter();
    emit selectedInspectionRouteWaypointChanged(selectedInspectionRouteWaypointIndex_);
    emit inspectionRouteChanged();
}

void PointCloudViewer::setSelectedInspectionRouteWaypointIndex(int index)
{
    const int normalizedIndex =
        (index >= 0 && index < inspectionRouteWaypoints_.size()) ? index : -1;
    const int normalizedTargetIndex = normalizeInspectionRouteWaypointTargetIndex(
        normalizedIndex,
        -1);

    if (selectedInspectionRouteWaypointIndex_ == normalizedIndex
        && selectedInspectionRouteWaypointTargetIndex_ == normalizedTargetIndex) {
        return;
    }

    selectedInspectionRouteWaypointIndex_ = normalizedIndex;
    selectedInspectionRouteWaypointTargetIndex_ = normalizedTargetIndex;
    if (inspectionRouteRoamActive()) {
        updateInspectionRouteOverlayWidgets();
        updateRouteCameraPreviewOverlay();
        if (osgWidget_ != nullptr) {
            osgWidget_->update();
        }
    } else {
        refreshInspectionRouteOverlay();
    }
    updateFooter();
    emit selectedInspectionRouteWaypointChanged(selectedInspectionRouteWaypointIndex_);
}

int PointCloudViewer::selectedInspectionRouteWaypointTargetIndex() const
{
    return selectedInspectionRouteWaypointTargetIndex_;
}

void PointCloudViewer::setSelectedInspectionRouteWaypointTargetIndex(int index)
{
    const int normalizedTargetIndex = normalizeInspectionRouteWaypointTargetIndex(
        selectedInspectionRouteWaypointIndex_,
        index);
    if (selectedInspectionRouteWaypointTargetIndex_ == normalizedTargetIndex) {
        return;
    }

    selectedInspectionRouteWaypointTargetIndex_ = normalizedTargetIndex;
    if (inspectionRouteRoamActive()) {
        updateInspectionRouteOverlayWidgets();
        updateRouteCameraPreviewOverlay();
        if (osgWidget_ != nullptr) {
            osgWidget_->update();
        }
    } else {
        refreshInspectionRouteOverlay();
    }
    updateFooter();
}

bool PointCloudViewer::inspectionRouteEditingEnabled() const
{
    return inspectionRouteEditingEnabled_;
}

void PointCloudViewer::setInspectionRouteEditingEnabled(bool enabled)
{
    if (inspectionRouteEditingEnabled_ == enabled) {
        return;
    }

    inspectionRouteEditingEnabled_ = enabled;
    if (inspectionRouteEditingEnabled_) {
        return;
    }

    if (osgWidget_ != nullptr) {
        osgWidget_->setSceneDragCaptureEnabled(false);
        osgWidget_->unsetCursor();
    }

    if (routeWaypointDragActive_ || routeWaypointDragPreviewValid_) {
        routeWaypointDragActive_ = false;
        routeWaypointDragIndex_ = -1;
        routeWaypointDragPreviewValid_ = false;
        refreshInspectionRouteOverlay();
    }
}

void PointCloudViewer::beginIssueAddMode()
{
    if (profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }
    issueEditMode_ = IssueEditMode::Add;
    cancelTowerEditMode();
    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    } else {
        updateSceneClickCapture();
    }
    emit issueEditModeChanged();
}

void PointCloudViewer::cancelIssueEditMode()
{
    if (issueEditMode_ == IssueEditMode::None) {
        return;
    }

    issueEditMode_ = IssueEditMode::None;
    updateSceneClickCapture();
    emit issueEditModeChanged();
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
    if (hasPointCloud()) {
        datasetExtent = std::max({
            static_cast<double>(currentPointCloud_->maxBounds().x - currentPointCloud_->minBounds().x),
            static_cast<double>(currentPointCloud_->maxBounds().y - currentPointCloud_->minBounds().y),
            static_cast<double>(currentPointCloud_->maxBounds().z - currentPointCloud_->minBounds().z),
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

bool PointCloudViewer::isInspectionIssueVisible(int index) const
{
    return index >= 0
        && index < inspectionIssues_.size()
        && !hiddenInspectionIssueIndices_.contains(index);
}

void PointCloudViewer::setInspectionIssueVisible(int index, bool visible)
{
    if (index < 0 || index >= inspectionIssues_.size()) {
        return;
    }

    const bool currentlyVisible = !hiddenInspectionIssueIndices_.contains(index);
    if (currentlyVisible == visible) {
        return;
    }

    if (visible) {
        hiddenInspectionIssueIndices_.remove(index);
    } else {
        hiddenInspectionIssueIndices_.insert(index);
    }

    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    refreshInspectionIssuesOverlay();
    updateFooter();
}

bool PointCloudViewer::inspectionRouteVisible() const
{
    return inspectionRouteVisible_;
}

bool PointCloudViewer::inspectionRouteRoamActive() const
{
    return inspectionRouteRoamPlaybackState_ != InspectionRouteRoamPlaybackState::Stopped;
}

bool PointCloudViewer::inspectionRouteRoamPlaying() const
{
    return inspectionRouteRoamPlaybackState_ == InspectionRouteRoamPlaybackState::Playing;
}

bool PointCloudViewer::inspectionRouteRoamPaused() const
{
    return inspectionRouteRoamPlaybackState_ == InspectionRouteRoamPlaybackState::Paused;
}

double PointCloudViewer::inspectionRouteRoamSpeedMetersPerSecond() const
{
    return inspectionRouteRoamSpeedMetersPerSecond_;
}

RouteRoamViewMode PointCloudViewer::inspectionRouteRoamViewMode() const
{
    return inspectionRouteRoamViewMode_;
}

void PointCloudViewer::setInspectionRouteVisible(bool visible)
{
    if (inspectionRouteVisible_ == visible) {
        return;
    }

    inspectionRouteVisible_ = visible;
    if (!inspectionRouteVisible_) {
        routeRoamStopInternal(true);
    }
    DataManager::instance().setTrajectory(
        DataManager::instance().trajectoryItem().name.trimmed().isEmpty()
            ? tr("Inspection Route")
            : DataManager::instance().trajectoryItem().name,
        inspectionRouteWaypoints_,
        inspectionRouteVisible_);
    refreshInspectionRouteOverlay();
    updateFooter();
    emit inspectionRouteChanged();
}

void PointCloudViewer::setInspectionRouteRoamSpeedMetersPerSecond(double speedMetersPerSecond)
{
    const double clampedSpeed = clampRouteRoamSpeed(speedMetersPerSecond);
    if (qFuzzyCompare(inspectionRouteRoamSpeedMetersPerSecond_, clampedSpeed)) {
        return;
    }

    inspectionRouteRoamSpeedMetersPerSecond_ = clampedSpeed;
    updateFooter();
    emit inspectionRouteRoamStateChanged();
}

void PointCloudViewer::setInspectionRouteRoamViewMode(RouteRoamViewMode mode)
{
    if (inspectionRouteRoamViewMode_ == mode) {
        return;
    }

    inspectionRouteRoamViewMode_ = mode;
    inspectionRouteRoamThirdPersonFollowInitialized_ = false;
    if (inspectionRouteRoamActive()) {
        updateInspectionRouteRoam();
        refreshInspectionRouteOverlay();
    }
    updateFooter();
    emit inspectionRouteRoamStateChanged();
}

void PointCloudViewer::startInspectionRouteRoam(int startWaypointIndex)
{
    if (!hasPointCloud() || inspectionRouteWaypoints_.isEmpty() || !inspectionRouteVisible_) {
        return;
    }

    if (!inspectionRouteRoamActive()) {
        routeRoamCaptureManualView();
    }
    inspectionRouteRoamLastCaptureWaypointIndex_ = -1;
    inspectionRouteRoamCaptureCount_ = 0;
    inspectionRouteRoamCaptureFlashRemainingSeconds_ = 0.0;
    inspectionRouteRoamCurrentPositionValid_ = false;
    inspectionRouteRoamThirdPersonFollowInitialized_ = false;

    if (inspectionRouteWaypoints_.size() == 1) {
        inspectionRouteRoamCurrentSegmentIndex_ = 0;
        inspectionRouteRoamSegmentProgressMeters_ = 0.0;
        inspectionRouteRoamDwelling_ = true;
        inspectionRouteRoamDwellRemainingSeconds_ = kRouteRoamDwellSeconds;
        inspectionRouteRoamPlaybackState_ = InspectionRouteRoamPlaybackState::Playing;
        refreshInspectionRouteOverlay();
        routeRoamUpdateSelectionState(0);
        osg::Vec3d position;
        osg::Vec3d forward;
        osg::Vec3d up;
        if (routeRoamComputeWaypointPose(0, inspectionRouteWaypoints_, &position, &forward, &up)) {
            routeRoamApplyPose(position, forward, up);
        }
        inspectionRouteRoamLastUpdateTime_ = std::chrono::steady_clock::now();
        if (routeRoamTimer_ != nullptr && !routeRoamTimer_->isActive()) {
            routeRoamTimer_->start();
        }
        updateFooter();
        emit inspectionRouteRoamStateChanged();
        return;
    }

    const int startIndex = std::clamp(startWaypointIndex, 0, inspectionRouteWaypoints_.size() - 1);
    if (startIndex >= inspectionRouteWaypoints_.size() - 1) {
        inspectionRouteRoamCurrentSegmentIndex_ = inspectionRouteWaypoints_.size() - 2;
        inspectionRouteRoamSegmentProgressMeters_ = routeSegmentLength(
            inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_),
            inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_ + 1));
    } else {
        inspectionRouteRoamCurrentSegmentIndex_ = startIndex;
        inspectionRouteRoamSegmentProgressMeters_ = 0.0;
    }

    inspectionRouteRoamDwelling_ = true;
    inspectionRouteRoamDwellRemainingSeconds_ = kRouteRoamDwellSeconds;
    inspectionRouteRoamPlaybackState_ = InspectionRouteRoamPlaybackState::Playing;
    inspectionRouteRoamLastUpdateTime_ = std::chrono::steady_clock::now();

    refreshInspectionRouteOverlay();
    routeRoamUpdateSelectionState(startIndex);
    osg::Vec3d position;
    osg::Vec3d forward;
    osg::Vec3d up;
    if (routeRoamComputeWaypointPose(startIndex, inspectionRouteWaypoints_, &position, &forward, &up)) {
        routeRoamApplyPose(position, forward, up);
    }

    if (routeRoamTimer_ != nullptr && !routeRoamTimer_->isActive()) {
        routeRoamTimer_->start();
    }
    updateFooter();
    emit inspectionRouteRoamStateChanged();
}

void PointCloudViewer::pauseInspectionRouteRoam()
{
    if (inspectionRouteRoamPlaybackState_ != InspectionRouteRoamPlaybackState::Playing) {
        return;
    }

    inspectionRouteRoamPlaybackState_ = InspectionRouteRoamPlaybackState::Paused;
    if (routeRoamTimer_ != nullptr) {
        routeRoamTimer_->stop();
    }
    updateFooter();
    emit inspectionRouteRoamStateChanged();
}

void PointCloudViewer::resumeInspectionRouteRoam()
{
    if (inspectionRouteRoamPlaybackState_ != InspectionRouteRoamPlaybackState::Paused) {
        return;
    }

    inspectionRouteRoamPlaybackState_ = InspectionRouteRoamPlaybackState::Playing;
    inspectionRouteRoamLastUpdateTime_ = std::chrono::steady_clock::now();
    if (routeRoamTimer_ != nullptr && !routeRoamTimer_->isActive()) {
        routeRoamTimer_->start();
    }
    updateFooter();
    emit inspectionRouteRoamStateChanged();
}

void PointCloudViewer::stopInspectionRouteRoam(bool restoreManualView)
{
    routeRoamStopInternal(restoreManualView);
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

void PointCloudViewer::createWelcomeOverlay()
{
    welcomeOverlay_ = new QFrame(osgWidget_);
    welcomeOverlay_->setObjectName(QStringLiteral("viewerWelcomeOverlay"));
    welcomeOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    welcomeOverlay_->setStyleSheet(QStringLiteral(
        "QFrame#viewerWelcomeOverlay {"
        "background-color: #0a1118;"
        "}"));

    auto* overlayLayout = new QVBoxLayout(welcomeOverlay_);
    overlayLayout->setContentsMargins(24, 24, 24, 24);
    overlayLayout->setSpacing(0);
    overlayLayout->addStretch();

    welcomeImageLabel_ = new QLabel(welcomeOverlay_);
    welcomeImageLabel_->setAlignment(Qt::AlignCenter);
    welcomeImageLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    overlayLayout->addWidget(welcomeImageLabel_, 0, Qt::AlignCenter);

    welcomeStatusLabel_ = new QLabel(welcomeOverlay_);
    welcomeStatusLabel_->setAlignment(Qt::AlignCenter);
    welcomeStatusLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    welcomeStatusLabel_->setStyleSheet(QStringLiteral(
        "QLabel {"
        "color: rgba(226, 232, 240, 0.96);"
        "font-size: 13px;"
        "font-weight: 600;"
        "padding-top: 14px;"
        "}"));
    welcomeStatusLabel_->hide();
    overlayLayout->addWidget(welcomeStatusLabel_, 0, Qt::AlignHCenter);

    overlayLayout->addStretch();
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
    if (welcomeOverlay_ == nullptr || osgWidget_ == nullptr || welcomeImageLabel_ == nullptr) {
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

    const QPixmap splashPixmap(QStringLiteral(":/assets/icon/Splash.png"));
    if (!splashPixmap.isNull()) {
        const QSize availableSize(
            std::max(320, static_cast<int>(std::lround(welcomeOverlay_->width() * 0.60))),
            std::max(180, static_cast<int>(std::lround(welcomeOverlay_->height() * 0.60))));
        welcomeImageLabel_->setPixmap(splashPixmap.scaled(availableSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        welcomeImageLabel_->clear();
    }

    if (welcomeStatusLabel_ != nullptr) {
        if (pointCloudLoadingActive_ && !pointCloudLoadingDetail_.trimmed().isEmpty()) {
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

void PointCloudViewer::rebuildScene()
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

    pointCloudNode_ = OsgPointCloudNode::build(*currentPointCloud_, visualizationOptions_);
    if (pointCloudNode_.valid()) {
        rootGroup_->addChild(pointCloudNode_.get());
    }

    refreshTowerMarkersOverlay();
    refreshInspectionIssuesOverlay();
    refreshInspectionRouteOverlay();
    refreshMeasurementOverlay();
}

void PointCloudViewer::rebuildMergedPointCloud()
{
    currentPointCloud_.reset();
    std::shared_ptr<PointCloudData> singleVisibleDataset;
    int visibleDatasetCount = 0;
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (!dataset.info.visible || !dataset.pointCloud) {
            continue;
        }
        ++visibleDatasetCount;
        if (visibleDatasetCount == 1) {
            singleVisibleDataset = dataset.pointCloud;
        } else {
            if (visibleDatasetCount == 2) {
                currentPointCloud_ = std::make_shared<PointCloudData>();
                currentPointCloud_->append(*singleVisibleDataset);
            }
            currentPointCloud_->append(*dataset.pointCloud);
        }
    }

    if (visibleDatasetCount == 1) {
        currentPointCloud_ = std::move(singleVisibleDataset);
    }
    syncCurrentFilePath();
}

void PointCloudViewer::updateSceneOriginFromCurrentPointCloud()
{
    sceneOriginValid_ = false;
    sceneOriginWorld_.set(0.0, 0.0, 0.0);

    if (currentPointCloud_ == nullptr || currentPointCloud_->empty()) {
        return;
    }

    const PointRecord& minBounds = currentPointCloud_->minBounds();
    const PointRecord& maxBounds = currentPointCloud_->maxBounds();
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
        .arg(formatPointCount(currentPointCloud_ != nullptr ? currentPointCloud_->size() : 0))
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

    const PointRecord& minBounds = currentPointCloud_->minBounds();
    const PointRecord& maxBounds = currentPointCloud_->maxBounds();
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

    if (profileClassificationModeEnabled_
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

void PointCloudViewer::syncVisualizationClassificationState()
{
    visualizationOptions_.classificationEditStore =
        std::make_shared<ClassificationEditStore>(classificationEditStore_);
    visualizationOptions_.classificationDatasetPathsById.clear();
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        visualizationOptions_.classificationDatasetPathsById.insert(
            dataset.info.datasetId,
            dataset.info.filePath);
    }
}

void PointCloudViewer::clearSelectionRubberBand()
{
    profileClassificationSelectionActive_ = false;
    profileClassificationSelectionRect_ = QRectF();
    if (selectionRubberBand_ != nullptr) {
        selectionRubberBand_->hide();
    }
}

void PointCloudViewer::clearProfileClassificationPolygonSelection()
{
    profileClassificationPolygonPoints_.clear();
    profileClassificationPolygonPreviewPoint_ = QPointF();
    profileClassificationPolygonPreviewActive_ = false;
    updateProfileClassificationPolygonOverlay();
}

void PointCloudViewer::updateProfileClassificationPolygonOverlay()
{
    if (profileClassificationPolygonOverlay_ == nullptr || osgWidget_ == nullptr) {
        return;
    }

    profileClassificationPolygonOverlay_->setGeometry(osgWidget_->rect());
    auto* overlay = static_cast<PolygonSelectionOverlay*>(profileClassificationPolygonOverlay_);

    const bool polygonVisible =
        profileClassificationModeEnabled_
        && profileClassificationSelectionMode_ == ProfileClassificationSelectionMode::Polygon
        && !profileClassificationTaskActive_
        && !profileClassificationPolygonPoints_.isEmpty();

    if (!polygonVisible) {
        overlay->setPolygonState(QPolygonF(), false, QPointF());
        return;
    }

    overlay->setPolygonState(
        profileClassificationPolygonPoints_,
        profileClassificationPolygonPreviewActive_,
        profileClassificationPolygonPreviewPoint_);
}

void PointCloudViewer::tryFinishProfileClassificationPolygonSelection()
{
    if (!profileClassificationModeEnabled_
        || profileClassificationSelectionMode_ != ProfileClassificationSelectionMode::Polygon
        || profileClassificationTaskActive_) {
        return;
    }

    if (profileClassificationPolygonPoints_.size() < 3) {
        emit measurementMessage(tr("Add at least three polygon vertices before applying profile classification."), true);
        return;
    }

    const QPolygonF polygon = profileClassificationPolygonPoints_;
    clearProfileClassificationPolygonSelection();
    beginProfileClassificationSelection(polygon.boundingRect(), polygon);
}

void PointCloudViewer::handleSelectionRectangleChanged(const QRectF& localRect, bool active)
{
    if (profileClassificationSelectionMode_ != ProfileClassificationSelectionMode::Rectangle) {
        return;
    }

    profileClassificationSelectionActive_ = active;
    profileClassificationSelectionRect_ = active ? localRect.normalized() : QRectF();
    if (selectionRubberBand_ == nullptr) {
        return;
    }

    if (!active || localRect.isNull()) {
        selectionRubberBand_->hide();
        return;
    }

    selectionRubberBand_->setGeometry(localRect.normalized().toRect());
    selectionRubberBand_->show();
    selectionRubberBand_->raise();
}

void PointCloudViewer::handleSelectionRectangleFinished(const QRectF& localRect)
{
    if (profileClassificationSelectionMode_ != ProfileClassificationSelectionMode::Rectangle) {
        return;
    }

    clearSelectionRubberBand();
    beginProfileClassificationSelection(localRect.normalized(), QPolygonF());
}

void PointCloudViewer::handleSelectionEscapePressed()
{
    if (!profileClassificationModeEnabled_
        || profileClassificationSelectionMode_ != ProfileClassificationSelectionMode::Rectangle) {
        return;
    }

    clearSelectionRubberBand();
    setProfileClassificationModeEnabled(false);
}

void PointCloudViewer::beginProfileClassificationSelection(const QRectF& viewportRect, const QPolygonF& viewportPolygon)
{
    if (!profileClassificationModeEnabled_ || profileClassificationTaskActive_) {
        return;
    }
    if (profileClassificationSourceClasses_.isEmpty()) {
        emit measurementMessage(tr("Choose at least one source classification before profile classification."), true);
        return;
    }
    if (!hasPointCloud() || osgWidget_ == nullptr) {
        return;
    }

    if (classificationTaskThread_.joinable()) {
        classificationTaskThread_.join();
    }

    struct DatasetSnapshot
    {
        QString datasetPath;
        std::shared_ptr<PointCloudData> pointCloud;
    };

    QList<DatasetSnapshot> datasets;
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (!dataset.info.visible || dataset.pointCloud == nullptr) {
            continue;
        }
        datasets.append({ dataset.info.filePath, dataset.pointCloud });
    }

    if (datasets.isEmpty()) {
        emit measurementMessage(tr("No visible datasets are available for profile classification."), true);
        return;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCamera() == nullptr || viewer->getCamera()->getViewport() == nullptr) {
        return;
    }

    const osg::Matrixd worldToWindow =
        viewer->getCamera()->getViewMatrix()
        * viewer->getCamera()->getProjectionMatrix()
        * viewer->getCamera()->getViewport()->computeWindowMatrix();
    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    const osg::Matrixd localToWindow = osg::Matrixd::translate(sceneOrigin) * worldToWindow;
    const QSet<int> sourceClasses = profileClassificationSourceClasses_;
    const int targetClassification = profileClassificationTargetClass_;
    const bool usePolygon = viewportPolygon.size() >= 3;
    const QPolygonF selectionPolygon = usePolygon ? viewportPolygon : QPolygonF();
    const QRectF selectionRect = usePolygon ? selectionPolygon.boundingRect() : viewportRect.normalized();
    if (selectionRect.width() < 2.0 || selectionRect.height() < 2.0) {
        emit measurementMessage(tr("Selection region is too small for profile classification."), true);
        return;
    }
    const std::shared_ptr<const ClassificationEditStore> editSnapshot =
        std::make_shared<ClassificationEditStore>(classificationEditStore_);
    const qreal devicePixelRatio = osgWidget_->devicePixelRatioF();
    const int widgetHeight = osgWidget_->height();
    const std::uint64_t token = ++classificationTaskToken_;
    const auto taskStartTime = std::chrono::steady_clock::now();

    profileClassificationTaskActive_ = true;
    classificationTaskStartTime_ = taskStartTime;
    classificationTaskScannedPoints_.store(0);
    updateFooter();
    if (classificationTaskStatusTimer_ != nullptr) {
        classificationTaskStatusTimer_->start();
    }
    emit profileClassificationStateChanged();
    emit measurementMessage(
        usePolygon
            ? tr("Applying profile classification polygon selection...")
            : tr("Applying profile classification selection..."),
        false);

    classificationTaskThread_ = std::thread([this, token, datasets, sourceClasses, targetClassification, selectionRect, selectionPolygon, usePolygon, localToWindow, sceneOrigin, editSnapshot, devicePixelRatio, widgetHeight, taskStartTime]() {
        ClassificationEditBatch batch;
        batch.targetClassification = targetClassification;
        std::uint64_t scannedPointCount = 0;

        const double minX = selectionRect.left();
        const double maxX = selectionRect.right();
        const double minY = selectionRect.top();
        const double maxY = selectionRect.bottom();

        for (const DatasetSnapshot& dataset : datasets) {
            if (dataset.pointCloud == nullptr) {
                continue;
            }

            const std::vector<PointRecord>& points = dataset.pointCloud->points();
            for (const PointRecord& point : points) {
                ++scannedPointCount;
                if ((scannedPointCount & 0x1FFFu) == 0u) {
                    classificationTaskScannedPoints_.store(scannedPointCount);
                }
                const osg::Vec3d projected = osg::Vec3d(
                    point.x - sceneOrigin.x(),
                    point.y - sceneOrigin.y(),
                    point.z - sceneOrigin.z())
                    * localToWindow;
                if (projected.z() < 0.0 || projected.z() > 1.0) {
                    continue;
                }

                const QPointF viewportPoint(
                    projected.x() / devicePixelRatio,
                    static_cast<double>(widgetHeight) - projected.y() / devicePixelRatio);
                if (viewportPoint.x() < minX || viewportPoint.x() > maxX || viewportPoint.y() < minY || viewportPoint.y() > maxY) {
                    continue;
                }
                if (usePolygon && !selectionPolygon.containsPoint(viewportPoint, Qt::WindingFill)) {
                    continue;
                }

                const int rawClassification = point.hasClassification ? static_cast<int>(point.classification) : -1;
                const int effectiveClassification = editSnapshot != nullptr
                    ? editSnapshot->effectiveClassification(dataset.datasetPath, point.sourcePointIndex, rawClassification)
                    : rawClassification;
                if (!sourceClasses.contains(effectiveClassification)) {
                    continue;
                }

                ++batch.hitCount;
                if (effectiveClassification == targetClassification) {
                    continue;
                }

                ClassificationEditBatchItem item;
                item.datasetPath = dataset.datasetPath;
                item.pointIndex = point.sourcePointIndex;
                item.previousEffectiveClassification = effectiveClassification;
                item.targetClassification = targetClassification;
                item.hadPreviousOverride = editSnapshot != nullptr
                    && editSnapshot->tryGetOverride(dataset.datasetPath, point.sourcePointIndex, &item.previousOverrideClassification);
                batch.items.append(item);
            }
        }

        batch.changedCount = batch.items.size();
        classificationTaskScannedPoints_.store(scannedPointCount);
        const auto elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - taskStartTime);
        QMetaObject::invokeMethod(
            this,
            [this, token, batch, scannedPointCount, elapsedMilliseconds]() {
                finalizeProfileClassificationTask(
                    token,
                    batch,
                    scannedPointCount,
                    static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsedMilliseconds.count())));
            },
            Qt::QueuedConnection);
    });
}

void PointCloudViewer::finalizeProfileClassificationTask(
    std::uint64_t token,
    const ClassificationEditBatch& batch,
    std::uint64_t scannedPointCount,
    std::uint64_t elapsedMilliseconds)
{
    if (token != classificationTaskToken_) {
        return;
    }

    if (classificationTaskThread_.joinable()) {
        classificationTaskThread_.join();
    }

    profileClassificationTaskActive_ = false;
    lastClassificationTaskScannedPoints_ = scannedPointCount;
    lastClassificationTaskElapsedMilliseconds_ = elapsedMilliseconds;
    if (classificationTaskStatusTimer_ != nullptr) {
        classificationTaskStatusTimer_->stop();
    }
    if (!batch.isEmpty()) {
        classificationEditStore_.applyBatch(batch);
        classificationUndoStack_.append(batch);
        classificationRedoStack_.clear();
        syncVisualizationClassificationState();
        rebuildScene();
        emit visualizationOptionsChanged();
        emit classificationEditsChanged();
    }

    updateFooter();
    emit profileClassificationStateChanged();
    const QString completionStats = QStringLiteral(" (%1, %2ms)")
        .arg(QLocale().toString(static_cast<qlonglong>(scannedPointCount)))
        .arg(QLocale().toString(static_cast<qlonglong>(elapsedMilliseconds)));
    emit measurementMessage(
        tr("Profile classification completed. %1 point(s) hit, %2 point(s) changed to class %3.")
            .arg(QLocale().toString(batch.hitCount))
            .arg(QLocale().toString(batch.changedCount))
            .arg(QLocale().toString(batch.targetClassification))
            + completionStats,
        false);
}

int PointCloudViewer::effectiveClassificationForPoint(const QString& datasetPath, const PointRecord& point) const
{
    const int rawClassification = point.hasClassification ? static_cast<int>(point.classification) : -1;
    return classificationEditStore_.effectiveClassification(datasetPath, point.sourcePointIndex, rawClassification);
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

    for (const PointRecord& point : currentPointCloud_->points()) {
        const osg::Vec3d projected = osg::Vec3d(
            point.x - sceneOrigin.x(),
            point.y - sceneOrigin.y(),
            point.z - sceneOrigin.z())
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

    for (int towerIndex = 0; towerIndex < towerMarkers_.size(); ++towerIndex) {
        const TowerMarker& towerMarker = towerMarkers_.at(towerIndex);
        const osg::Vec3d projected = osg::Vec3d(
            towerMarker.point.x - sceneOrigin.x(),
            towerMarker.point.y - sceneOrigin.y(),
            towerMarker.point.z - sceneOrigin.z())
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
            bestIndex = towerIndex;
            bestDistanceSquared = distanceSquared;
            bestDepth = projected.z();
        }
    }

    return bestIndex;
}

int PointCloudViewer::pickInspectionIssueAtScreenPosition(const QPointF& localPos, float tolerancePixels) const
{
    if (inspectionIssues_.isEmpty() || osgWidget_ == nullptr) {
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

    for (int issueIndex = 0; issueIndex < inspectionIssues_.size(); ++issueIndex) {
        if (hiddenInspectionIssueIndices_.contains(issueIndex)) {
            continue;
        }
        const InspectionIssue& issue = inspectionIssues_.at(issueIndex);
        const osg::Vec3d projected = osg::Vec3d(
            issue.point.x - sceneOrigin.x(),
            issue.point.y - sceneOrigin.y(),
            issue.point.z - sceneOrigin.z())
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
            bestIndex = issueIndex;
            bestDistanceSquared = distanceSquared;
            bestDepth = projected.z();
        }
    }

    return bestIndex;
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

osg::ref_ptr<osg::Node> PointCloudViewer::buildMeasurementOverlay() const
{
    if (!measurementResult_.hasStartPoint) {
        return nullptr;
    }

    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    osg::ref_ptr<osg::Group> overlay = new osg::Group();

    osg::ref_ptr<osg::Geode> markersGeode = buildMeasurementMarkersGeode(measurementResult_, sceneOrigin);
    if (markersGeode.valid()) {
        overlay->addChild(markersGeode.get());
    }

    osg::ref_ptr<osg::Geode> lineGeode = buildMeasurementLineGeode(measurementResult_, sceneOrigin);
    if (lineGeode.valid()) {
        overlay->addChild(lineGeode.get());
    }

    return overlay->getNumChildren() > 0
        ? wrapOverlayNodeWithSceneOrigin(overlay.release(), sceneOrigin)
        : nullptr;
}

osg::ref_ptr<osg::Node> PointCloudViewer::buildTowerMarkersOverlay() const
{
    if (towerMarkers_.isEmpty()) {
        return nullptr;
    }

    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    osg::ref_ptr<osg::Geode> markersGeode = buildTowerMarkersGeode(towerMarkers_, sceneOrigin);
    return wrapOverlayNodeWithSceneOrigin(markersGeode.release(), sceneOrigin);
}

osg::ref_ptr<osg::Node> PointCloudViewer::buildInspectionIssuesOverlay() const
{
    if (inspectionIssues_.isEmpty()) {
        return nullptr;
    }

    QList<InspectionIssue> visibleIssues;
    visibleIssues.reserve(inspectionIssues_.size());
    for (int issueIndex = 0; issueIndex < inspectionIssues_.size(); ++issueIndex) {
        if (hiddenInspectionIssueIndices_.contains(issueIndex)) {
            continue;
        }
        visibleIssues.append(inspectionIssues_.at(issueIndex));
    }

    if (visibleIssues.isEmpty()) {
        return nullptr;
    }

    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    osg::ref_ptr<osg::Geode> markersGeode = buildInspectionIssuesGeode(visibleIssues, sceneOrigin);
    return wrapOverlayNodeWithSceneOrigin(markersGeode.release(), sceneOrigin);
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

    if (osgWidget_ == nullptr || measurementResult_.points.isEmpty()) {
        hideAllLabels();
        return;
    }

    bool startVisible = false;
    const QPointF startPoint = projectPointToViewport(measurementResult_.startPoint, &startVisible);
    if (measurementStartOverlayLabel_ != nullptr && startVisible) {
        measurementStartOverlayLabel_->setText(QStringLiteral("1"));
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
        measurementEndOverlayLabel_->setText(QLocale().toString(measurementResult_.pointCount()));
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
        tr("%1 pts | 3D %2 | Height %3")
            .arg(QLocale().toString(measurementResult_.pointCount()))
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

void PointCloudViewer::updateInspectionIssueOverlayWidgets()
{
    if (osgWidget_ == nullptr) {
        return;
    }

    while (issueOverlayLabels_.size() < inspectionIssues_.size()) {
        auto* label = new QLabel(osgWidget_);
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        label->hide();
        issueOverlayLabels_.append(label);
    }

    for (int issueIndex = 0; issueIndex < issueOverlayLabels_.size(); ++issueIndex) {
        QLabel* label = issueOverlayLabels_.at(issueIndex);
        if (label == nullptr) {
            continue;
        }

        if (issueIndex >= inspectionIssues_.size() || hiddenInspectionIssueIndices_.contains(issueIndex)) {
            label->hide();
            continue;
        }

        bool pointVisible = false;
        const InspectionIssue& issue = inspectionIssues_.at(issueIndex);
        const QPointF anchor = projectPointToViewport(issue.point, &pointVisible);
        if (!pointVisible) {
            label->hide();
            continue;
        }

        QString backgroundColor = QStringLiteral("rgba(153, 27, 27, 220)");
        QString borderColor = QStringLiteral("rgba(252, 165, 165, 180)");
        switch (issue.severity) {
        case IssueSeverity::Info:
            backgroundColor = QStringLiteral("rgba(30, 64, 175, 220)");
            borderColor = QStringLiteral("rgba(147, 197, 253, 190)");
            break;
        case IssueSeverity::Minor:
            backgroundColor = QStringLiteral("rgba(161, 98, 7, 220)");
            borderColor = QStringLiteral("rgba(253, 230, 138, 190)");
            break;
        case IssueSeverity::Major:
            backgroundColor = QStringLiteral("rgba(194, 65, 12, 220)");
            borderColor = QStringLiteral("rgba(253, 186, 116, 190)");
            break;
        case IssueSeverity::Critical:
        default:
            break;
        }

        const bool isSelected = issueIndex == selectedIssueIndex_;
        label->setText(issue.title);
        label->setStyleSheet(QStringLiteral(
            "QLabel {"
            "background-color: %1;"
            "color: #fff7ed;"
            "border: 1px solid %2;"
            "border-radius: 8px;"
            "padding: 4px 8px;"
            "font-size: 12px;"
            "font-weight: 600;"
            "}").arg(
                isSelected ? QStringLiteral("rgba(126, 34, 206, 230)") : backgroundColor,
                isSelected ? QStringLiteral("rgba(233, 213, 255, 220)") : borderColor));
        positionOverlayLabel(label, anchor, QPoint(14, 16));
    }
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
    const double minZ = currentPointCloud_->minBounds().z;
    const double heightSpan = std::max(0.0, currentPointCloud_->maxBounds().z - minZ);
    const std::vector<PointRecord>& points = currentPointCloud_->points();
    const std::size_t pointStride = std::max<std::size_t>(1u, points.size() / 140000u);

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

QString PointCloudViewer::inspectionRouteWaypointLabelText(int index) const
{
    if (index < 0) {
        return QString();
    }

    if (routeLabelModeHidden(routeWaypointLabelDisplayMode_)) {
        return QString();
    }

    if (routeLabelModeUsesSequence(routeWaypointLabelDisplayMode_)) {
        return QLocale().toString(index + 1);
    }

    if (index < inspectionRouteLabels_.size()) {
        const QString label = inspectionRouteLabels_.at(index).trimmed();
        if (!label.isEmpty()) {
            return label;
        }
    }

    return QLocale().toString(index + 1);
}

QString PointCloudViewer::inspectionRoutePartLabelText(int index) const
{
    if (index < 0) {
        return QString();
    }

    if (routeLabelModeHidden(routePartLabelDisplayMode_)) {
        return QString();
    }

    if (routeLabelModeUsesSequence(routePartLabelDisplayMode_)) {
        return QLocale().toString(index + 1);
    }

    if (index < inspectionRoutePartLabels_.size()) {
        const QString label = inspectionRoutePartLabels_.at(index).trimmed();
        if (!label.isEmpty()) {
            return label;
        }
    }

    return QLocale().toString(index + 1);
}

int PointCloudViewer::normalizeInspectionRouteWaypointTargetIndex(int waypointIndex, int targetIndex) const
{
    if (waypointIndex < 0 || waypointIndex >= inspectionRouteWaypoints_.size()) {
        return -1;
    }

    if (waypointIndex < inspectionRouteWaypointAllTargetPoints_.size()) {
        const QList<PointRecord>& targets = inspectionRouteWaypointAllTargetPoints_.at(waypointIndex);
        if (!targets.isEmpty()) {
            return (targetIndex >= 0 && targetIndex < targets.size()) ? targetIndex : 0;
        }
    }

    const bool hasLegacyTarget =
        waypointIndex < inspectionRouteWaypointHasTargetPoints_.size()
        && inspectionRouteWaypointHasTargetPoints_.at(waypointIndex)
        && waypointIndex < inspectionRouteWaypointTargetPoints_.size();
    return hasLegacyTarget ? 0 : -1;
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

void PointCloudViewer::refreshInspectionIssuesOverlay()
{
    if (rootGroup_.valid() && inspectionIssuesNode_.valid()) {
        rootGroup_->removeChild(inspectionIssuesNode_.get());
        inspectionIssuesNode_ = nullptr;
    }

    if (rootGroup_.valid() && !inspectionIssues_.isEmpty()) {
        inspectionIssuesNode_ = buildInspectionIssuesOverlay();
        if (inspectionIssuesNode_.valid()) {
            rootGroup_->addChild(inspectionIssuesNode_.get());
        }
    }

    updateInspectionIssueOverlayWidgets();
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
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

void PointCloudViewer::updateInspectionRouteRoam()
{
    if (inspectionRouteRoamPlaybackState_ != InspectionRouteRoamPlaybackState::Playing) {
        return;
    }
    if (!hasPointCloud() || !inspectionRouteVisible_ || inspectionRouteWaypoints_.isEmpty()) {
        routeRoamStopInternal(false);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (inspectionRouteRoamLastUpdateTime_.time_since_epoch().count() == 0) {
        inspectionRouteRoamLastUpdateTime_ = now;
        return;
    }

    double remainingSeconds = std::chrono::duration<double>(now - inspectionRouteRoamLastUpdateTime_).count();
    inspectionRouteRoamLastUpdateTime_ = now;
    remainingSeconds = std::clamp(remainingSeconds, 0.0, 0.25);
    if (inspectionRouteRoamCaptureFlashRemainingSeconds_ > 0.0) {
        const double previousFlashSeconds = inspectionRouteRoamCaptureFlashRemainingSeconds_;
        inspectionRouteRoamCaptureFlashRemainingSeconds_ =
            std::max(0.0, inspectionRouteRoamCaptureFlashRemainingSeconds_ - remainingSeconds);
        if (previousFlashSeconds > 0.0 && inspectionRouteRoamCaptureFlashRemainingSeconds_ <= 0.0) {
            updateRouteCameraPreviewOverlay();
        }
    }

    const int waypointCount = inspectionRouteWaypoints_.size();
    if (waypointCount == 1) {
        osg::Vec3d position;
        osg::Vec3d forward;
        osg::Vec3d up;
        if (routeRoamComputeWaypointPose(0, inspectionRouteWaypoints_, &position, &forward, &up)) {
            routeRoamApplyPose(position, forward, up);
        }
        routeRoamUpdateSelectionState(0);
        if (inspectionRouteRoamDwelling_) {
            inspectionRouteRoamDwellRemainingSeconds_ = std::max(0.0, inspectionRouteRoamDwellRemainingSeconds_ - remainingSeconds);
            if (inspectionRouteRoamDwellRemainingSeconds_ <= 0.0) {
                routeRoamStopInternal(false);
            }
        }
        return;
    }

    while (remainingSeconds > 0.0) {
        const int segmentIndex = std::clamp(inspectionRouteRoamCurrentSegmentIndex_, 0, waypointCount - 2);
        const PointRecord& startPoint = inspectionRouteWaypoints_.at(segmentIndex);
        const PointRecord& endPoint = inspectionRouteWaypoints_.at(segmentIndex + 1);
        const double segmentLength = routeSegmentLength(startPoint, endPoint);

        if (inspectionRouteRoamDwelling_) {
            const double consumed = std::min(remainingSeconds, inspectionRouteRoamDwellRemainingSeconds_);
            inspectionRouteRoamDwellRemainingSeconds_ -= consumed;
            remainingSeconds -= consumed;
            if (inspectionRouteRoamDwellRemainingSeconds_ <= 0.0) {
                inspectionRouteRoamDwelling_ = false;
            } else {
                break;
            }
            continue;
        }

        if (segmentLength <= 0.0001) {
            inspectionRouteRoamCurrentSegmentIndex_ = segmentIndex + 1;
            inspectionRouteRoamSegmentProgressMeters_ = 0.0;
            routeRoamUpdateSelectionState(inspectionRouteRoamCurrentSegmentIndex_);
            if (inspectionRouteRoamCurrentSegmentIndex_ >= waypointCount - 1) {
                routeRoamStopInternal(false);
                return;
            }
            inspectionRouteRoamDwelling_ = true;
            inspectionRouteRoamDwellRemainingSeconds_ = kRouteRoamDwellSeconds;
            continue;
        }

        const double clampedSpeed = clampRouteRoamSpeed(inspectionRouteRoamSpeedMetersPerSecond_);
        const double remainOnSegment = std::max(0.0, segmentLength - inspectionRouteRoamSegmentProgressMeters_);
        const double travelDistance = clampedSpeed * remainingSeconds;
        if (travelDistance < remainOnSegment) {
            inspectionRouteRoamSegmentProgressMeters_ += travelDistance;
            remainingSeconds = 0.0;
            break;
        }

        const double timeUsed = remainOnSegment / clampedSpeed;
        remainingSeconds = std::max(0.0, remainingSeconds - timeUsed);
        inspectionRouteRoamCurrentSegmentIndex_ = segmentIndex + 1;
        inspectionRouteRoamSegmentProgressMeters_ = 0.0;
        routeRoamUpdateSelectionState(inspectionRouteRoamCurrentSegmentIndex_);
        if (inspectionRouteRoamCurrentSegmentIndex_ >= waypointCount - 1) {
            routeRoamStopInternal(false);
            return;
        }
        inspectionRouteRoamDwelling_ = true;
        inspectionRouteRoamDwellRemainingSeconds_ = kRouteRoamDwellSeconds;
    }

    int poseWaypointIndex = std::clamp(inspectionRouteRoamCurrentSegmentIndex_, 0, waypointCount - 1);
    const PointRecord& poseWaypoint = inspectionRouteWaypoints_.at(poseWaypointIndex);
    osg::Vec3d interpolatedPosition(
        static_cast<double>(poseWaypoint.x),
        static_cast<double>(poseWaypoint.y),
        static_cast<double>(poseWaypoint.z));
    double orientationBlendFactor = 0.0;
    if (inspectionRouteRoamCurrentSegmentIndex_ < waypointCount - 1) {
        const PointRecord& segmentStart = inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_);
        const PointRecord& segmentEnd = inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_ + 1);
        const double segmentLength = routeSegmentLength(segmentStart, segmentEnd);
        if (segmentLength > 0.0001) {
            const double t = std::clamp(inspectionRouteRoamSegmentProgressMeters_ / segmentLength, 0.0, 1.0);
            orientationBlendFactor = t;
            interpolatedPosition.set(
                static_cast<double>(segmentStart.x)
                    + (static_cast<double>(segmentEnd.x) - static_cast<double>(segmentStart.x)) * t,
                static_cast<double>(segmentStart.y)
                    + (static_cast<double>(segmentEnd.y) - static_cast<double>(segmentStart.y)) * t,
                static_cast<double>(segmentStart.z)
                    + (static_cast<double>(segmentEnd.z) - static_cast<double>(segmentStart.z)) * t);
        }
    }

    osg::Vec3d waypointPosition;
    osg::Vec3d forward;
    osg::Vec3d up;
    if (!routeRoamComputeWaypointPose(poseWaypointIndex, inspectionRouteWaypoints_, &waypointPosition, &forward, &up)) {
        return;
    }
    if (inspectionRouteRoamCurrentSegmentIndex_ < waypointCount - 1 && orientationBlendFactor > 0.0001) {
        if (inspectionRouteRoamViewMode_ == RouteRoamViewMode::ThirdPerson) {
            osg::Vec3d nextWaypointPosition;
            osg::Vec3d nextForward;
            osg::Vec3d nextUp;
            if (routeRoamComputeWaypointPose(
                    inspectionRouteRoamCurrentSegmentIndex_ + 1,
                    inspectionRouteWaypoints_,
                    &nextWaypointPosition,
                    &nextForward,
                    &nextUp)) {
                const double blend = std::clamp(orientationBlendFactor, 0.0, 1.0);
                osg::Vec3d blendedForward = forward * (1.0 - blend) + nextForward * blend;
                if (blendedForward.length2() > 0.000001) {
                    blendedForward.normalize();
                    forward = blendedForward;
                }

                osg::Vec3d blendedUp = up * (1.0 - blend) + nextUp * blend;
                if (blendedUp.length2() > 0.000001) {
                    blendedUp.normalize();
                    up = blendedUp;
                } else {
                    up = osg::Vec3d(0.0, 0.0, 1.0);
                }

                osg::Vec3d right = forward ^ up;
                if (right.length2() > 0.000001) {
                    right.normalize();
                    up = right ^ forward;
                    up.normalize();
                }
            }
        }
    }

    if (inspectionRouteRoamViewMode_ == RouteRoamViewMode::FirstPerson
        && !inspectionRouteRoamDwelling_
        && inspectionRouteRoamCurrentSegmentIndex_ < waypointCount - 1) {
        const PointRecord& segmentStart = inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_);
        const PointRecord& segmentEnd = inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_ + 1);
        osg::Vec3d segmentForward(
            static_cast<double>(segmentEnd.x - segmentStart.x),
            static_cast<double>(segmentEnd.y - segmentStart.y),
            static_cast<double>(segmentEnd.z - segmentStart.z));
        if (segmentForward.length2() > 0.000001) {
            segmentForward.normalize();
            forward = segmentForward;

            osg::Vec3d worldUp(0.0, 0.0, 1.0);
            osg::Vec3d right = forward ^ worldUp;
            if (right.length2() <= 0.000001) {
                worldUp = osg::Vec3d(0.0, 1.0, 0.0);
                right = forward ^ worldUp;
            }
            right.normalize();
            up = right ^ forward;
            up.normalize();
        }
    }

    routeRoamApplyPose(interpolatedPosition, forward, up);
}

void PointCloudViewer::routeRoamUpdateSelectionState(int waypointIndex)
{
    const int normalizedTargetIndex = normalizeInspectionRouteWaypointTargetIndex(waypointIndex, 0);
    setSelectedInspectionRouteWaypointIndex(waypointIndex);
    setSelectedInspectionRouteWaypointTargetIndex(normalizedTargetIndex);
    if (inspectionRouteRoamPlaybackState_ == InspectionRouteRoamPlaybackState::Playing) {
        routeRoamTriggerPhotoCapture(waypointIndex, normalizedTargetIndex);
    }
}

void PointCloudViewer::routeRoamTriggerPhotoCapture(int waypointIndex, int targetIndex)
{
    if (waypointIndex < 0
        || waypointIndex >= inspectionRouteWaypoints_.size()
        || waypointIndex == inspectionRouteRoamLastCaptureWaypointIndex_) {
        return;
    }

    inspectionRouteRoamLastCaptureWaypointIndex_ = waypointIndex;
    ++inspectionRouteRoamCaptureCount_;
    inspectionRouteRoamCaptureFlashRemainingSeconds_ = 0.35;

    QString targetLabel;
    if (waypointIndex < inspectionRouteWaypointAllTargetLabels_.size()
        && targetIndex >= 0
        && targetIndex < inspectionRouteWaypointAllTargetLabels_.at(waypointIndex).size()) {
        targetLabel = inspectionRouteWaypointAllTargetLabels_.at(waypointIndex).at(targetIndex).trimmed();
    }
    if (targetLabel.isEmpty() && targetIndex >= 0) {
        targetLabel = tr("Target %1").arg(QLocale().toString(targetIndex + 1));
    }
    if (targetLabel.isEmpty()) {
        targetLabel = tr("Unlinked");
    }

    updateRouteCameraPreviewOverlay();
    updateFooter();
    emit inspectionRouteRoamPhotoCaptured(
        waypointIndex,
        targetIndex,
        targetLabel,
        inspectionRouteRoamCaptureCount_);
}

bool PointCloudViewer::routeRoamComputeWaypointPose(
    int waypointIndex,
    const QList<PointRecord>& waypoints,
    osg::Vec3d* position,
    osg::Vec3d* forward,
    osg::Vec3d* up) const
{
    if (position == nullptr || forward == nullptr || up == nullptr) {
        return false;
    }
    if (waypointIndex < 0 || waypointIndex >= waypoints.size()) {
        return false;
    }

    const PointRecord& waypoint = waypoints.at(waypointIndex);
    *position = osg::Vec3d(
        static_cast<double>(waypoint.x),
        static_cast<double>(waypoint.y),
        static_cast<double>(waypoint.z));

    const double aircraftYawDeg = waypointIndex < inspectionRouteWaypointAircraftYawDegs_.size()
        ? inspectionRouteWaypointAircraftYawDegs_.at(waypointIndex)
        : 0.0;
    const double gimbalPitchDeg = waypointIndex < inspectionRouteWaypointGimbalPitchDegs_.size()
        ? inspectionRouteWaypointGimbalPitchDegs_.at(waypointIndex)
        : 0.0;
    const int targetIndex = normalizeInspectionRouteWaypointTargetIndex(waypointIndex, 0);
    const QList<double> cameraYawCandidates = waypointIndex < inspectionRouteWaypointAllCameraYawDegs_.size()
        ? inspectionRouteWaypointAllCameraYawDegs_.at(waypointIndex)
        : QList<double>();
    const QList<double> cameraPitchCandidates = waypointIndex < inspectionRouteWaypointAllCameraPitchDegs_.size()
        ? inspectionRouteWaypointAllCameraPitchDegs_.at(waypointIndex)
        : QList<double>();
    const double cameraYawDeg =
        (targetIndex >= 0 && targetIndex < cameraYawCandidates.size())
            ? cameraYawCandidates.at(targetIndex)
            : (waypointIndex < inspectionRouteWaypointCameraYawDegs_.size()
                ? inspectionRouteWaypointCameraYawDegs_.at(waypointIndex)
                : 0.0);
    const double cameraPitchDeg =
        (targetIndex >= 0 && targetIndex < cameraPitchCandidates.size())
            ? cameraPitchCandidates.at(targetIndex)
            : (waypointIndex < inspectionRouteWaypointCameraPitchDegs_.size()
                ? inspectionRouteWaypointCameraPitchDegs_.at(waypointIndex)
                : 0.0);

    const double yawRadians = qDegreesToRadians(aircraftYawDeg + cameraYawDeg);
    const double pitchRadians = qDegreesToRadians(gimbalPitchDeg + cameraPitchDeg);
    osg::Vec3d routeForward(
        std::sin(yawRadians) * std::cos(pitchRadians),
        std::cos(yawRadians) * std::cos(pitchRadians),
        std::sin(pitchRadians));

    if (routeForward.length2() <= 0.00001) {
        const bool hasNextWaypoint = waypointIndex + 1 < waypoints.size();
        const bool hasPrevWaypoint = waypointIndex - 1 >= 0;
        if (hasNextWaypoint) {
            const PointRecord& nextPoint = waypoints.at(waypointIndex + 1);
            routeForward = osg::Vec3d(
                static_cast<double>(nextPoint.x - waypoint.x),
                static_cast<double>(nextPoint.y - waypoint.y),
                static_cast<double>(nextPoint.z - waypoint.z));
        } else if (hasPrevWaypoint) {
            const PointRecord& prevPoint = waypoints.at(waypointIndex - 1);
            routeForward = osg::Vec3d(
                static_cast<double>(waypoint.x - prevPoint.x),
                static_cast<double>(waypoint.y - prevPoint.y),
                static_cast<double>(waypoint.z - prevPoint.z));
        }
        if (routeForward.length2() <= 0.00001) {
            routeForward = osg::Vec3d(0.0, 1.0, 0.0);
        }
    }
    routeForward.normalize();

    osg::Vec3d worldUp(0.0, 0.0, 1.0);
    osg::Vec3d right = routeForward ^ worldUp;
    if (right.length2() <= 0.00001) {
        worldUp = osg::Vec3d(0.0, 1.0, 0.0);
        right = routeForward ^ worldUp;
    }
    right.normalize();
    osg::Vec3d routeUp = right ^ routeForward;
    routeUp.normalize();

    *forward = routeForward;
    *up = routeUp;
    return true;
}

bool PointCloudViewer::routeRoamApplyPose(
    const osg::Vec3d& position,
    const osg::Vec3d& forward,
    const osg::Vec3d& up)
{
    if (osgWidget_ == nullptr) {
        return false;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCameraManipulator() == nullptr) {
        return false;
    }

    osgGA::CameraManipulator* manipulator = viewer->getCameraManipulator();

    const bool positionChanged = !inspectionRouteRoamCurrentPositionValid_
        || (position - inspectionRouteRoamCurrentPosition_).length2() > 0.000001;
    inspectionRouteRoamCurrentPosition_ = position;
    inspectionRouteRoamCurrentPositionValid_ = true;

    if (inspectionRouteRoamViewMode_ == RouteRoamViewMode::FirstPerson) {
        inspectionRouteRoamThirdPersonFollowInitialized_ = false;
        const osg::Vec3d eye = position;
        const osg::Vec3d center = position + forward * kRouteRoamFirstPersonLookAheadMeters;
        if (auto* trackball = dynamic_cast<osgGA::TrackballManipulator*>(manipulator)) {
            trackball->setTransformation(eye, center, up);
        } else {
            manipulator->setHomePosition(eye, center, up, false);
            manipulator->home(0.0);
        }
    } else {
        if (auto* trackball = dynamic_cast<osgGA::TrackballManipulator*>(manipulator)) {
            if (!inspectionRouteRoamThirdPersonFollowInitialized_) {
                const osg::Vec3d eye = position - forward * kRouteRoamThirdPersonDistanceMeters + up * kRouteRoamThirdPersonHeightMeters;
                const osg::Vec3d center = position + forward * kRouteRoamThirdPersonLookAheadMeters;
                trackball->setTransformation(eye, center, up);
                inspectionRouteRoamThirdPersonFollowInitialized_ = true;
                inspectionRouteRoamLastFollowPosition_ = position;
            } else {
                osg::Vec3d currentEye;
                osg::Vec3d currentCenter;
                osg::Vec3d currentUp;
                trackball->getTransformation(currentEye, currentCenter, currentUp);
                const osg::Vec3d followDelta = position - inspectionRouteRoamLastFollowPosition_;
                if (followDelta.length2() > 0.000001) {
                    trackball->setTransformation(currentEye + followDelta, currentCenter + followDelta, currentUp);
                }
                inspectionRouteRoamLastFollowPosition_ = position;
            }
        } else {
            const osg::Vec3d eye = position - forward * kRouteRoamThirdPersonDistanceMeters + up * kRouteRoamThirdPersonHeightMeters;
            const osg::Vec3d center = position + forward * kRouteRoamThirdPersonLookAheadMeters;
            manipulator->setHomePosition(eye, center, up, false);
            manipulator->home(0.0);
        }
    }

    if (positionChanged && inspectionRouteRoamPlaybackState_ != InspectionRouteRoamPlaybackState::Stopped) {
        refreshInspectionRouteOverlay();
    }

    osgWidget_->update();
    return true;
}

void PointCloudViewer::routeRoamCaptureManualView()
{
    if (inspectionRouteRoamManualViewCaptured_ || osgWidget_ == nullptr) {
        return;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCameraManipulator() == nullptr) {
        return;
    }

    osgGA::CameraManipulator* manipulator = viewer->getCameraManipulator();
    osg::Vec3d eye;
    osg::Vec3d center;
    osg::Vec3d up;
    if (auto* trackball = dynamic_cast<osgGA::TrackballManipulator*>(manipulator)) {
        trackball->getTransformation(eye, center, up);
    } else if (viewer->getCamera() != nullptr) {
        viewer->getCamera()->getViewMatrixAsLookAt(eye, center, up);
    } else {
        return;
    }

    inspectionRouteRoamSavedEye_ = eye;
    inspectionRouteRoamSavedCenter_ = center;
    inspectionRouteRoamSavedUp_ = up;
    inspectionRouteRoamSavedManipulator_ = manipulator;
    inspectionRouteRoamManualViewCaptured_ = true;
}

void PointCloudViewer::routeRoamRestoreManualView()
{
    if (!inspectionRouteRoamManualViewCaptured_ || osgWidget_ == nullptr) {
        return;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCameraManipulator() == nullptr) {
        inspectionRouteRoamManualViewCaptured_ = false;
        inspectionRouteRoamSavedManipulator_ = nullptr;
        return;
    }

    osgGA::CameraManipulator* manipulator = viewer->getCameraManipulator();
    if (auto* trackball = dynamic_cast<osgGA::TrackballManipulator*>(manipulator)) {
        trackball->setTransformation(
            inspectionRouteRoamSavedEye_,
            inspectionRouteRoamSavedCenter_,
            inspectionRouteRoamSavedUp_);
    } else {
        manipulator->setHomePosition(
            inspectionRouteRoamSavedEye_,
            inspectionRouteRoamSavedCenter_,
            inspectionRouteRoamSavedUp_,
            false);
        manipulator->home(0.0);
    }

    inspectionRouteRoamManualViewCaptured_ = false;
    inspectionRouteRoamSavedManipulator_ = nullptr;
    osgWidget_->update();
}

void PointCloudViewer::routeRoamStopInternal(bool restoreManualView)
{
    const bool wasActive = inspectionRouteRoamActive();
    if (routeRoamTimer_ != nullptr) {
        routeRoamTimer_->stop();
    }

    inspectionRouteRoamPlaybackState_ = InspectionRouteRoamPlaybackState::Stopped;
    inspectionRouteRoamCurrentSegmentIndex_ = 0;
    inspectionRouteRoamSegmentProgressMeters_ = 0.0;
    inspectionRouteRoamDwelling_ = false;
    inspectionRouteRoamDwellRemainingSeconds_ = 0.0;
    inspectionRouteRoamLastCaptureWaypointIndex_ = -1;
    inspectionRouteRoamCaptureCount_ = 0;
    inspectionRouteRoamCaptureFlashRemainingSeconds_ = 0.0;
    inspectionRouteRoamLastUpdateTime_ = {};
    inspectionRouteRoamCurrentPositionValid_ = false;
    inspectionRouteRoamThirdPersonFollowInitialized_ = false;

    if (restoreManualView) {
        routeRoamRestoreManualView();
    }

    if (wasActive) {
        refreshInspectionRouteOverlay();
        updateFooter();
        emit inspectionRouteRoamStateChanged();
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

void PointCloudViewer::recalculateMeasurementResult()
{
    measurementResult_.hasStartPoint = !measurementResult_.points.isEmpty();
    measurementResult_.hasEndPoint = measurementResult_.points.size() >= 2;
    measurementResult_.distance3d = 0.0f;
    measurementResult_.deltaZ = 0.0f;

    if (!measurementResult_.hasStartPoint) {
        measurementResult_.startPoint = PointRecord();
        measurementResult_.endPoint = PointRecord();
        return;
    }

    measurementResult_.startPoint = measurementResult_.points.constFirst();
    measurementResult_.endPoint = measurementResult_.points.constLast();

    if (!measurementResult_.hasEndPoint) {
        return;
    }

    for (int pointIndex = 1; pointIndex < measurementResult_.points.size(); ++pointIndex) {
        const PointRecord& previousPoint = measurementResult_.points.at(pointIndex - 1);
        const PointRecord& currentPoint = measurementResult_.points.at(pointIndex);
        const double dx = static_cast<double>(currentPoint.x - previousPoint.x);
        const double dy = static_cast<double>(currentPoint.y - previousPoint.y);
        const double dz = static_cast<double>(currentPoint.z - previousPoint.z);
        measurementResult_.distance3d += static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
    }

    measurementResult_.deltaZ = measurementResult_.endPoint.z - measurementResult_.startPoint.z;
}

bool PointCloudViewer::undoLastMeasurementPoint()
{
    if (measurementResult_.points.isEmpty()) {
        return false;
    }

    measurementResult_.points.removeLast();
    recalculateMeasurementResult();
    refreshMeasurementOverlay();
    updateFooter();
    emit measurementChanged();
    return true;
}

void PointCloudViewer::resetMeasurementState(bool notifyChange)
{
    const bool hadMeasurement = !measurementResult_.points.isEmpty();
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
    updateInspectionRouteOverlayWidgets();
    updateRouteCameraPreviewOverlay();
    updateAxisIndicator();
}
