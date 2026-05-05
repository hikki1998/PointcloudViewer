#include "pointcloud/ClipFilter.h"

#include <algorithm>

namespace
{
constexpr double kClipPlaneEpsilon = 1e-6;
}

bool ClipFilter::pointInsidePlanes(const osg::Vec3d& point, const ClipRegion& clip)
{
    for (const ClipPlane& plane : clip.planes) {
        if (plane.normal.length2() <= 0.0) {
            continue;
        }

        const double signedDistance = plane.normal * point + plane.distance;
        if (signedDistance < -kClipPlaneEpsilon) {
            return false;
        }
    }

    return true;
}

bool ClipFilter::pointInsideClip(const PointRecord& point, const ClipRegion& clip)
{
    if (!clip.isActive()) {
        return true;
    }

    return pointInsidePlanes(osg::Vec3d(point.x, point.y, point.z), clip);
}

std::shared_ptr<PointCloudData> ClipFilter::apply(
    const PointCloudData& source,
    const ClipRegion& clipRegion)
{
    if (source.empty() || !clipRegion.isActive()) {
        return std::make_shared<PointCloudData>(source);
    }

    auto result = std::make_shared<PointCloudData>();
    result->reserve(source.size());
    PointRecord minBounds;
    PointRecord maxBounds;
    bool hasAnyPoint = false;

    const std::vector<PointRecord>& sourcePoints = source.points();
    for (const PointRecord& point : sourcePoints) {
        const bool inside = pointInsideClip(point, clipRegion);
        if (clipRegion.keepInside ? inside : !inside) {
            result->appendPointFast(point);
            if (!hasAnyPoint) {
                minBounds = point;
                maxBounds = point;
                hasAnyPoint = true;
            } else {
                minBounds.x = std::min(minBounds.x, point.x);
                minBounds.y = std::min(minBounds.y, point.y);
                minBounds.z = std::min(minBounds.z, point.z);
                maxBounds.x = std::max(maxBounds.x, point.x);
                maxBounds.y = std::max(maxBounds.y, point.y);
                maxBounds.z = std::max(maxBounds.z, point.z);
            }
        }
    }

    if (hasAnyPoint) {
        result->finalizeImport(
            minBounds,
            maxBounds,
            source.hasColor(),
            source.hasIntensity(),
            source.hasClassification(),
            source.hasReturnInfo(),
            source.hasGpsTime());
    }

    return result;
}
