#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

struct PointRecord
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

class PointCloudData
{
public:
    PointCloudData();

    void clear();
    void reserve(std::size_t count);
    void addPoint(float x, float y, float z, std::uint8_t r = 255, std::uint8_t g = 255, std::uint8_t b = 255, std::uint8_t a = 255);
    void append(const PointCloudData& other);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] const std::vector<PointRecord>& points() const;
    [[nodiscard]] bool hasColor() const;

    [[nodiscard]] const PointRecord& minBounds() const;
    [[nodiscard]] const PointRecord& maxBounds() const;

private:
    void updateBounds(const PointRecord& point);

    std::vector<PointRecord> points_;
    PointRecord minBounds_;
    PointRecord maxBounds_;
    bool hasColor_ = false;
};
