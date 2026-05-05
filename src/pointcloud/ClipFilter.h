#pragma once

#include "osg/PointCloudVisualization.h"
#include "pointcloud/PointCloudData.h"

#include <memory>

class ClipFilter
{
public:
    static std::shared_ptr<PointCloudData> apply(
        const PointCloudData& source,
        const ClipRegion& clipRegion);

    static bool pointInsidePlanes(const osg::Vec3d& point, const ClipRegion& clip);
    static bool pointInsideClip(const PointRecord& point, const ClipRegion& clip);
};
