#pragma once

#include <osg/ref_ptr>

#include "osg/PointCloudVisualization.h"

namespace osg
{
class Group;
class Node;
}

class PointCloudData;

class OsgPointCloudNode
{
public:
    static osg::ref_ptr<osg::Group> build(
        const PointCloudData& pointCloudData,
        const PointCloudVisualizationOptions& visualizationOptions);
};
