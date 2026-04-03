#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include <QHash>
#include <QList>

#include "pointcloud/PointCloudData.h"

struct PointCloudTileId
{
    std::uint16_t depth = 0;
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t z = 0;

    [[nodiscard]] std::uint64_t packed() const;

    bool operator==(const PointCloudTileId& other) const
    {
        return depth == other.depth
            && x == other.x
            && y == other.y
            && z == other.z;
    }
};

uint qHash(const PointCloudTileId& tileId, uint seed = 0) noexcept;

struct PointCloudTileData
{
    PointCloudTileId id;
    std::shared_ptr<PointCloudData> pointCloud;
    PointRecord minBounds;
    PointRecord maxBounds;
    std::size_t pointCount = 0;
};

struct PointCloudTileSet
{
    PointRecord minBounds;
    PointRecord maxBounds;
    std::size_t sourcePointCount = 0;
    int depth = 0;
    QList<PointCloudTileData> tiles;

    [[nodiscard]] bool empty() const
    {
        return tiles.isEmpty();
    }
};

class PointCloudTileAccumulator
{
public:
    PointCloudTileAccumulator(
        const PointRecord& minBounds,
        const PointRecord& maxBounds,
        std::size_t totalPointCountHint,
        std::size_t targetLeafPointCount = 120000);

    void addPoint(const PointRecord& point);
    [[nodiscard]] PointCloudTileSet finalize();
    [[nodiscard]] int depth() const { return depth_; }

private:
    struct TileAccumulatorState
    {
        PointCloudTileId id;
        std::shared_ptr<PointCloudData> pointCloud = std::make_shared<PointCloudData>();
        PointRecord minBounds;
        PointRecord maxBounds;
        bool hasColor = false;
        bool hasIntensity = false;
        bool hasClassification = false;
        bool hasReturnInfo = false;
        bool hasGpsTime = false;
        std::size_t pointCount = 0;
        bool initialized = false;
    };

    [[nodiscard]] PointCloudTileId tileIdForPoint(const PointRecord& point) const;
    TileAccumulatorState& ensureTile(const PointCloudTileId& tileId);
    static int computeDepth(std::size_t totalPointCountHint, std::size_t targetLeafPointCount);

    PointRecord minBounds_;
    PointRecord maxBounds_;
    std::size_t totalPointCountHint_ = 0;
    std::size_t targetLeafPointCount_ = 120000;
    int depth_ = 0;
    std::uint32_t divisionsPerAxis_ = 1;
    float spanX_ = 1.0f;
    float spanY_ = 1.0f;
    float spanZ_ = 1.0f;
    QHash<PointCloudTileId, int> tileIndexById_;
    QList<TileAccumulatorState> tiles_;
};

PointCloudTileSet buildPointCloudTileSet(
    const PointCloudData& pointCloud,
    const PointRecord& minBounds,
    const PointRecord& maxBounds,
    std::size_t totalPointCountHint,
    std::size_t targetLeafPointCount = 120000);
