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
    std::uint16_t intensity = 0;
    std::uint8_t classification = 0;
    std::uint8_t returnNumber = 0;
    std::uint8_t numberOfReturns = 0;
    double gpsTime = 0.0;
    bool hasIntensity = false;
    bool hasClassification = false;
    bool hasReturnInfo = false;
    bool hasGpsTime = false;
};

class PointCloudData
{
public:
    PointCloudData();

    void clear();
    void reserve(std::size_t count);
    void appendPointFast(const PointRecord& point);
    void finalizeImport(
        const PointRecord& minBounds,
        const PointRecord& maxBounds,
        bool hasColor,
        bool hasIntensity,
        bool hasClassification,
        bool hasReturnInfo,
        bool hasGpsTime);
    void addPoint(
        float x,
        float y,
        float z,
        std::uint8_t r = 255,
        std::uint8_t g = 255,
        std::uint8_t b = 255,
        std::uint8_t a = 255,
        std::uint16_t intensity = 0,
        std::uint8_t classification = 0,
        std::uint8_t returnNumber = 0,
        std::uint8_t numberOfReturns = 0,
        double gpsTime = 0.0,
        bool hasIntensity = false,
        bool hasClassification = false,
        bool hasReturnInfo = false,
        bool hasGpsTime = false);
    void append(const PointCloudData& other);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] const std::vector<PointRecord>& points() const;
    [[nodiscard]] bool hasColor() const;
    [[nodiscard]] bool hasIntensity() const;
    [[nodiscard]] bool hasClassification() const;
    [[nodiscard]] bool hasReturnInfo() const;
    [[nodiscard]] bool hasGpsTime() const;

    [[nodiscard]] const PointRecord& minBounds() const;
    [[nodiscard]] const PointRecord& maxBounds() const;

private:
    void updateBounds(const PointRecord& point);

    std::vector<PointRecord> points_;
    PointRecord minBounds_;
    PointRecord maxBounds_;
    bool hasColor_ = false;
    bool hasIntensity_ = false;
    bool hasClassification_ = false;
    bool hasReturnInfo_ = false;
    bool hasGpsTime_ = false;
};
