#pragma once

#include <memory>

#include <QHash>
#include <QList>

#include <osg/ref_ptr>

#include "osg/PointCloudVisualization.h"
#include "pointcloud/PointCloudTileStore.h"

namespace osg
{
class Group;
class Geode;
class Node;
}

class PointCloudData;

class OsgPointCloudNode
{
public:
    OsgPointCloudNode();

    static osg::ref_ptr<osg::Group> build(
        const PointCloudData& pointCloudData,
        const PointCloudVisualizationOptions& visualizationOptions);

    osg::Group* root() const;
    void clear();
    void setVisualizationOptions(const PointCloudVisualizationOptions& visualizationOptions);
    void setSceneBounds(const PointRecord& minBounds, const PointRecord& maxBounds, bool hasData);
    void setPreviewTiles(const PointCloudTileSet& tileSet);
    void setFullTileData(const PointCloudTileId& tileId, const std::shared_ptr<PointCloudData>& pointCloud);
    bool setTilePromoted(const PointCloudTileId& tileId, bool promoted);
    QList<PointCloudTileId> tileIds() const;

private:
    struct TileNodeEntry
    {
        PointCloudTileId id;
        PointRecord minBounds;
        PointRecord maxBounds;
        std::shared_ptr<PointCloudData> previewPointCloud;
        std::shared_ptr<PointCloudData> fullPointCloud;
        osg::ref_ptr<osg::Group> group;
        osg::ref_ptr<osg::Node> previewNode;
        osg::ref_ptr<osg::Node> fullNode;
        bool promoted = false;
    };

    void rebuildAllTiles();
    void rebuildAuxiliaryNodes();
    void updateTileNode(TileNodeEntry& tileEntry);
    osg::ref_ptr<osg::Node> buildTileNode(const std::shared_ptr<PointCloudData>& pointCloud) const;

    PointCloudVisualizationOptions visualizationOptions_;
    PointRecord sceneMinBounds_;
    PointRecord sceneMaxBounds_;
    bool hasSceneBounds_ = false;
    osg::ref_ptr<osg::Group> root_;
    osg::ref_ptr<osg::Group> tileGroup_;
    osg::ref_ptr<osg::Node> boundsNode_;
    osg::ref_ptr<osg::Node> axesNode_;
    QHash<PointCloudTileId, TileNodeEntry> tileEntries_;
};
