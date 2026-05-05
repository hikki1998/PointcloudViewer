#include "pointcloud/LasWriter.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
#pragma pack(push, 1)
struct LasHeader12
{
    char fileSignature[4];
    std::uint16_t fileSourceId;
    std::uint16_t globalEncoding;
    std::uint32_t projectIdGuid1;
    std::uint16_t projectIdGuid2;
    std::uint16_t projectIdGuid3;
    char projectIdGuid4[8];
    std::uint8_t versionMajor;
    std::uint8_t versionMinor;
    char systemIdentifier[32];
    char generatingSoftware[32];
    std::uint16_t fileCreationDay;
    std::uint16_t fileCreationYear;
    std::uint16_t headerSize;
    std::uint32_t offsetToPointData;
    std::uint32_t numberOfVlrs;
    std::uint8_t pointDataFormat;
    std::uint16_t pointDataRecordLength;
    std::uint32_t numberOfPointRecords;
    std::uint32_t numberOfPointsByReturn[5];
    double xScaleFactor;
    double yScaleFactor;
    double zScaleFactor;
    double xOffset;
    double yOffset;
    double zOffset;
    double maxX;
    double minX;
    double maxY;
    double minY;
    double maxZ;
    double minZ;
};
#pragma pack(pop)

static_assert(sizeof(LasHeader12) == 227, "LAS 1.2 header must be 227 bytes");

template<typename T>
T clampToInt(double value)
{
    constexpr double kMin = static_cast<double>(std::numeric_limits<T>::min());
    constexpr double kMax = static_cast<double>(std::numeric_limits<T>::max());
    return static_cast<T>(std::clamp(value, kMin, kMax));
}

void writePointFormat2(QDataStream& stream, const PointRecord& point,
                       double xScale, double yScale, double zScale,
                       double xOff, double yOff, double zOff)
{
    const std::int32_t xi = clampToInt<std::int32_t>(
        std::floor((point.x - xOff) / xScale + 0.5));
    const std::int32_t yi = clampToInt<std::int32_t>(
        std::floor((point.y - yOff) / yScale + 0.5));
    const std::int32_t zi = clampToInt<std::int32_t>(
        std::floor((point.z - zOff) / zScale + 0.5));
    stream << xi << yi << zi;

    stream << static_cast<std::uint16_t>(point.intensity);

    std::uint8_t returnByte = 0;
    returnByte |= (point.returnNumber & 0x07);
    returnByte |= ((point.numberOfReturns & 0x07) << 3);
    stream << returnByte;

    std::uint8_t classFlags = 0;
    if (point.hasClassification) {
        classFlags |= (point.classification & 0x1F);
    }
    stream << classFlags;

    std::int8_t scanAngleRank = 0;
    stream << scanAngleRank;

    std::uint8_t userData = 0;
    stream << userData;

    std::uint16_t pointSourceId = 0;
    stream << pointSourceId;

    stream << static_cast<std::uint16_t>(std::min<int>(65535, point.r * 256));
    stream << static_cast<std::uint16_t>(std::min<int>(65535, point.g * 256));
    stream << static_cast<std::uint16_t>(std::min<int>(65535, point.b * 256));
}
}

bool LasWriter::write(
    const QString& filePath,
    const PointCloudData& data,
    QString* errorMessage) const
{
    if (data.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No point cloud data to write.");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to create file: %1").arg(filePath);
        }
        return false;
    }

    const double margin = 0.001;
    const double rangeX = data.maxBounds().x - data.minBounds().x + margin;
    const double rangeY = data.maxBounds().y - data.minBounds().y + margin;
    const double rangeZ = data.maxBounds().z - data.minBounds().z + margin;
    const double xScale = std::max(rangeX * 1e-9, 0.001);
    const double yScale = std::max(rangeY * 1e-9, 0.001);
    const double zScale = std::max(rangeZ * 1e-9, 0.001);

    LasHeader12 header {};
    header.fileSignature[0] = 'L';
    header.fileSignature[1] = 'A';
    header.fileSignature[2] = 'S';
    header.fileSignature[3] = 'F';
    header.versionMajor = 1;
    header.versionMinor = 2;
    header.headerSize = sizeof(LasHeader12);
    header.offsetToPointData = sizeof(LasHeader12);
    header.numberOfVlrs = 0;
    header.pointDataFormat = 2;
    header.pointDataRecordLength = 26;
    header.numberOfPointRecords = static_cast<std::uint32_t>(data.size());
    header.xScaleFactor = xScale;
    header.yScaleFactor = yScale;
    header.zScaleFactor = zScale;
    header.xOffset = data.minBounds().x;
    header.yOffset = data.minBounds().y;
    header.zOffset = data.minBounds().z;
    header.minX = data.minBounds().x;
    header.minY = data.minBounds().y;
    header.minZ = data.minBounds().z;
    header.maxX = data.maxBounds().x;
    header.maxY = data.maxBounds().y;
    header.maxZ = data.maxBounds().z;
    std::memset(header.systemIdentifier, 0, sizeof(header.systemIdentifier));
    std::memcpy(header.systemIdentifier, "LASPointCloudViewer", 19);
    std::memset(header.generatingSoftware, 0, sizeof(header.generatingSoftware));
    std::memcpy(header.generatingSoftware, "LASViewer", 9);

    file.write(reinterpret_cast<const char*>(&header), sizeof(LasHeader12));

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    const std::vector<PointRecord>& points = data.points();
    for (const PointRecord& point : points) {
        writePointFormat2(
            stream, point,
            xScale, yScale, zScale,
            data.minBounds().x, data.minBounds().y, data.minBounds().z);
    }

    file.close();

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Successfully wrote %1 points").arg(points.size());
    }

    return true;
}
