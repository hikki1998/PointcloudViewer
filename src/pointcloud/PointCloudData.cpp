#include "pointcloud/PointCloudData.h"

#include <algorithm>

PointCloudData::PointCloudData()
{
    clear();
}

void PointCloudData::clear()
{
    points_.clear();
    minBounds_.x = minBounds_.y = minBounds_.z = std::numeric_limits<float>::max();
    maxBounds_.x = maxBounds_.y = maxBounds_.z = std::numeric_limits<float>::lowest();
    hasColor_ = false;
    hasIntensity_ = false;
    hasClassification_ = false;
    hasReturnInfo_ = false;
    hasGpsTime_ = false;
}

void PointCloudData::reserve(std::size_t count)
{
    points_.reserve(count);
}

void PointCloudData::appendPointFast(const PointRecord& point)
{
    points_.push_back(point);
}

void PointCloudData::finalizeImport(
    const PointRecord& minBounds,
    const PointRecord& maxBounds,
    bool hasColor,
    bool hasIntensity,
    bool hasClassification,
    bool hasReturnInfo,
    bool hasGpsTime)
{
    minBounds_ = minBounds;
    maxBounds_ = maxBounds;
    hasColor_ = hasColor;
    hasIntensity_ = hasIntensity;
    hasClassification_ = hasClassification;
    hasReturnInfo_ = hasReturnInfo;
    hasGpsTime_ = hasGpsTime;
}

void PointCloudData::addPoint(
    float x,
    float y,
    float z,
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b,
    std::uint8_t a,
    std::uint16_t intensity,
    std::uint8_t classification,
    std::uint8_t returnNumber,
    std::uint8_t numberOfReturns,
    double gpsTime,
    bool hasIntensity,
    bool hasClassification,
    bool hasReturnInfo,
    bool hasGpsTime)
{
    PointRecord point;
    point.x = x;
    point.y = y;
    point.z = z;
    point.r = r;
    point.g = g;
    point.b = b;
    point.a = a;
    point.intensity = intensity;
    point.classification = classification;
    point.returnNumber = returnNumber;
    point.numberOfReturns = numberOfReturns;
    point.gpsTime = gpsTime;
    point.hasIntensity = hasIntensity;
    point.hasClassification = hasClassification;
    point.hasReturnInfo = hasReturnInfo;
    point.hasGpsTime = hasGpsTime;

    points_.push_back(point);
    hasColor_ = hasColor_ || r != 255 || g != 255 || b != 255;
    hasIntensity_ = hasIntensity_ || hasIntensity;
    hasClassification_ = hasClassification_ || hasClassification;
    hasReturnInfo_ = hasReturnInfo_ || hasReturnInfo;
    hasGpsTime_ = hasGpsTime_ || hasGpsTime;
    updateBounds(point);
}

void PointCloudData::append(const PointCloudData& other)
{
    if (other.empty()) {
        return;
    }

    const bool wasEmpty = points_.empty();
    points_.reserve(points_.size() + other.points_.size());
    points_.insert(points_.end(), other.points_.begin(), other.points_.end());

    hasColor_ = hasColor_ || other.hasColor_;
    hasIntensity_ = hasIntensity_ || other.hasIntensity_;
    hasClassification_ = hasClassification_ || other.hasClassification_;
    hasReturnInfo_ = hasReturnInfo_ || other.hasReturnInfo_;
    hasGpsTime_ = hasGpsTime_ || other.hasGpsTime_;

    if (wasEmpty) {
        minBounds_ = other.minBounds_;
        maxBounds_ = other.maxBounds_;
    } else {
        minBounds_.x = std::min(minBounds_.x, other.minBounds_.x);
        minBounds_.y = std::min(minBounds_.y, other.minBounds_.y);
        minBounds_.z = std::min(minBounds_.z, other.minBounds_.z);
        maxBounds_.x = std::max(maxBounds_.x, other.maxBounds_.x);
        maxBounds_.y = std::max(maxBounds_.y, other.maxBounds_.y);
        maxBounds_.z = std::max(maxBounds_.z, other.maxBounds_.z);
    }
}

bool PointCloudData::empty() const
{
    return points_.empty();
}

std::size_t PointCloudData::size() const
{
    return points_.size();
}

const std::vector<PointRecord>& PointCloudData::points() const
{
    return points_;
}

std::vector<PointRecord>& PointCloudData::mutablePoints()
{
    return points_;
}

bool PointCloudData::hasColor() const
{
    return hasColor_;
}

bool PointCloudData::hasIntensity() const
{
    return hasIntensity_;
}

bool PointCloudData::hasClassification() const
{
    return hasClassification_;
}

bool PointCloudData::hasReturnInfo() const
{
    return hasReturnInfo_;
}

bool PointCloudData::hasGpsTime() const
{
    return hasGpsTime_;
}

const PointRecord& PointCloudData::minBounds() const
{
    return minBounds_;
}

const PointRecord& PointCloudData::maxBounds() const
{
    return maxBounds_;
}

void PointCloudData::updateBounds(const PointRecord& point)
{
    minBounds_.x = std::min(minBounds_.x, point.x);
    minBounds_.y = std::min(minBounds_.y, point.y);
    minBounds_.z = std::min(minBounds_.z, point.z);

    maxBounds_.x = std::max(maxBounds_.x, point.x);
    maxBounds_.y = std::max(maxBounds_.y, point.y);
    maxBounds_.z = std::max(maxBounds_.z, point.z);
}
