#include "pointcloud/PointCloudTileStore.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
float safeAxisSpan(float minValue, float maxValue)
{
    return std::max(maxValue - minValue, 1e-5f);
}

std::uint16_t clampTileCoordinate(float normalizedValue, std::uint32_t divisionsPerAxis)
{
    if (divisionsPerAxis <= 1) {
        return 0;
    }

    const float clamped = std::clamp(normalizedValue, 0.0f, 0.999999f);
    const std::uint32_t coordinate = static_cast<std::uint32_t>(std::floor(clamped * static_cast<float>(divisionsPerAxis)));
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(coordinate, divisionsPerAxis - 1));
}
}

std::uint64_t PointCloudTileId::packed() const
{
    return (static_cast<std::uint64_t>(depth) << 48)
        | (static_cast<std::uint64_t>(x) << 32)
        | (static_cast<std::uint64_t>(y) << 16)
        | static_cast<std::uint64_t>(z);
}

uint qHash(const PointCloudTileId& tileId, uint seed) noexcept
{
    return qHash(static_cast<qulonglong>(tileId.packed()), seed);
}

PointCloudTileAccumulator::PointCloudTileAccumulator(
    const PointRecord& minBounds,
    const PointRecord& maxBounds,
    std::size_t totalPointCountHint,
    std::size_t targetLeafPointCount)
    : minBounds_(minBounds)
    , maxBounds_(maxBounds)
    , totalPointCountHint_(totalPointCountHint)
    , targetLeafPointCount_(std::max<std::size_t>(1, targetLeafPointCount))
    , depth_(computeDepth(totalPointCountHint, targetLeafPointCount_))
    , divisionsPerAxis_(static_cast<std::uint32_t>(1u << depth_))
    , spanX_(safeAxisSpan(minBounds.x, maxBounds.x))
    , spanY_(safeAxisSpan(minBounds.y, maxBounds.y))
    , spanZ_(safeAxisSpan(minBounds.z, maxBounds.z))
{
}

void PointCloudTileAccumulator::addPoint(const PointRecord& point)
{
    TileAccumulatorState& tile = ensureTile(tileIdForPoint(point));
    tile.pointCloud->appendPointFast(point);
    tile.hasColor = tile.hasColor || point.r != 255 || point.g != 255 || point.b != 255;
    tile.hasIntensity = tile.hasIntensity || point.hasIntensity;
    tile.hasClassification = tile.hasClassification || point.hasClassification;
    tile.hasReturnInfo = tile.hasReturnInfo || point.hasReturnInfo;
    tile.hasGpsTime = tile.hasGpsTime || point.hasGpsTime;

    if (!tile.initialized) {
        tile.minBounds = point;
        tile.maxBounds = point;
        tile.initialized = true;
    } else {
        tile.minBounds.x = std::min(tile.minBounds.x, point.x);
        tile.minBounds.y = std::min(tile.minBounds.y, point.y);
        tile.minBounds.z = std::min(tile.minBounds.z, point.z);
        tile.maxBounds.x = std::max(tile.maxBounds.x, point.x);
        tile.maxBounds.y = std::max(tile.maxBounds.y, point.y);
        tile.maxBounds.z = std::max(tile.maxBounds.z, point.z);
    }

    ++tile.pointCount;
}

PointCloudTileSet PointCloudTileAccumulator::finalize()
{
    PointCloudTileSet tileSet;
    tileSet.minBounds = minBounds_;
    tileSet.maxBounds = maxBounds_;
    tileSet.sourcePointCount = totalPointCountHint_;
    tileSet.depth = depth_;
    tileSet.tiles.reserve(tiles_.size());

    for (TileAccumulatorState& tile : tiles_) {
        if (!tile.initialized || tile.pointCloud == nullptr || tile.pointCount == 0) {
            continue;
        }

        tile.pointCloud->finalizeImport(
            tile.minBounds,
            tile.maxBounds,
            tile.hasColor,
            tile.hasIntensity,
            tile.hasClassification,
            tile.hasReturnInfo,
            tile.hasGpsTime);

        PointCloudTileData tileData;
        tileData.id = tile.id;
        tileData.pointCloud = tile.pointCloud;
        tileData.minBounds = tile.minBounds;
        tileData.maxBounds = tile.maxBounds;
        tileData.pointCount = tile.pointCount;
        tileSet.tiles.append(std::move(tileData));
    }

    return tileSet;
}

PointCloudTileId PointCloudTileAccumulator::tileIdForPoint(const PointRecord& point) const
{
    PointCloudTileId tileId;
    tileId.depth = static_cast<std::uint16_t>(depth_);
    tileId.x = clampTileCoordinate((point.x - minBounds_.x) / spanX_, divisionsPerAxis_);
    tileId.y = clampTileCoordinate((point.y - minBounds_.y) / spanY_, divisionsPerAxis_);
    tileId.z = clampTileCoordinate((point.z - minBounds_.z) / spanZ_, divisionsPerAxis_);
    return tileId;
}

PointCloudTileAccumulator::TileAccumulatorState& PointCloudTileAccumulator::ensureTile(const PointCloudTileId& tileId)
{
    const auto existing = tileIndexById_.constFind(tileId);
    if (existing != tileIndexById_.constEnd()) {
        return tiles_[existing.value()];
    }

    TileAccumulatorState tile;
    tile.id = tileId;
    const int newIndex = tiles_.size();
    tiles_.append(std::move(tile));
    tileIndexById_.insert(tileId, newIndex);
    return tiles_.last();
}

int PointCloudTileAccumulator::computeDepth(std::size_t totalPointCountHint, std::size_t targetLeafPointCount)
{
    if (targetLeafPointCount == 0 || totalPointCountHint <= targetLeafPointCount) {
        return 0;
    }

    std::size_t desiredLeafCount =
        (totalPointCountHint + targetLeafPointCount - 1) / targetLeafPointCount;
    desiredLeafCount = std::max<std::size_t>(1, desiredLeafCount);

    int depth = 0;
    std::size_t availableLeafCount = 1;
    while (availableLeafCount < desiredLeafCount && depth < 10) {
        ++depth;
        availableLeafCount *= 8;
    }

    return depth;
}

PointCloudTileSet buildPointCloudTileSet(
    const PointCloudData& pointCloud,
    const PointRecord& minBounds,
    const PointRecord& maxBounds,
    std::size_t totalPointCountHint,
    std::size_t targetLeafPointCount)
{
    PointCloudTileAccumulator accumulator(minBounds, maxBounds, totalPointCountHint, targetLeafPointCount);
    for (const PointRecord& point : pointCloud.points()) {
        accumulator.addPoint(point);
    }
    return accumulator.finalize();
}
