#include "pointcloud/LasReader.h"

#include <QDir>
#include <QFileInfo>

#include "pointcloud/PointCloudData.h"

#ifdef LAS_VIEWER_HAS_LASLIB
#include <algorithm>
#include <limits>
#include <memory>
#include <QStringList>

#include "lasreader.hpp"
#endif

#ifdef LAS_VIEWER_HAS_LASLIB
namespace
{
QString extractProjectionInfo(const LASheader& header)
{
    if (header.vlr_geo_ogc_wkt != nullptr && header.vlr_geo_ogc_wkt[0] != '\0') {
        return QString::fromUtf8(header.vlr_geo_ogc_wkt).trimmed();
    }

    if (header.vlr_geo_ascii_params != nullptr && header.vlr_geo_ascii_params[0] != '\0') {
        QString asciiProjection = QString::fromLocal8Bit(header.vlr_geo_ascii_params).trimmed();
        asciiProjection.replace(QLatin1Char('|'), QLatin1Char('\n'));
        return asciiProjection.trimmed();
    }

    if (header.vlr_geo_keys != nullptr
        && header.vlr_geo_key_entries != nullptr
        && header.vlr_geo_keys->number_of_keys > 0) {
        QStringList keyParts;
        const int keyCount = header.vlr_geo_keys->number_of_keys;
        for (int keyIndex = 0; keyIndex < keyCount; ++keyIndex) {
            const LASvlr_key_entry& key = header.vlr_geo_key_entries[keyIndex];
            switch (key.key_id) {
            case 1024:
                keyParts.append(QStringLiteral("ModelType=%1").arg(key.value_offset));
                break;
            case 2048:
                keyParts.append(QStringLiteral("GeographicType=%1").arg(key.value_offset));
                break;
            case 3072:
                keyParts.append(QStringLiteral("ProjectedType=%1").arg(key.value_offset));
                break;
            case 4096:
                keyParts.append(QStringLiteral("VerticalType=%1").arg(key.value_offset));
                break;
            default:
                break;
            }
        }
        if (!keyParts.isEmpty()) {
            return keyParts.join(QStringLiteral(", "));
        }
    }

    return QStringLiteral("Unknown");
}
}
#endif

bool LasReader::read(
    const QString& filePath,
    PointCloudData* output,
    QString* errorMessage,
    LasFileMetadata* metadata,
    ProgressCallback progressCallback) const
{
    if (output == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Internal error: output buffer is null.";
        }
        return false;
    }

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("File not found: %1").arg(filePath);
        }
        return false;
    }

    if (!fileInfo.isFile()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Path is not a file: %1").arg(filePath);
        }
        return false;
    }

    output->clear();
    const QByteArray nativePath = QDir::toNativeSeparators(filePath).toLocal8Bit();

#ifndef LAS_VIEWER_HAS_LASLIB
    Q_UNUSED(nativePath);
    Q_UNUSED(metadata);
    if (errorMessage != nullptr) {
        *errorMessage = "LASlib support is not enabled in this build.";
    }
    return false;
#else
    LASreadOpener opener;
    opener.set_file_name(nativePath.constData());

    std::unique_ptr<LASreader> reader(opener.open());
    if (!reader) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to open LAS/LAZ file: %1").arg(filePath);
        }
        return false;
    }

    const auto maxSizeT = static_cast<I64>(std::numeric_limits<std::size_t>::max());
    if (reader->npoints > 0 && reader->npoints <= maxSizeT) {
        output->reserve(static_cast<std::size_t>(reader->npoints));
    }

    const std::size_t totalPoints = (reader->npoints > 0 && reader->npoints <= maxSizeT)
        ? static_cast<std::size_t>(reader->npoints)
        : 0;
    const std::size_t progressStep = totalPoints > 0
        ? std::max<std::size_t>(8192, totalPoints / 200)
        : static_cast<std::size_t>(65536);

    PointRecord minBounds;
    PointRecord maxBounds;
    minBounds.x = minBounds.y = minBounds.z = std::numeric_limits<float>::max();
    maxBounds.x = maxBounds.y = maxBounds.z = std::numeric_limits<float>::lowest();
    bool hasColor = false;
    bool hasIntensity = false;
    bool hasClassification = false;
    bool hasReturnInfo = false;
    bool hasGpsTime = false;
    bool firstPoint = true;
    std::size_t loadedCount = 0;
    while (reader->read_point()) {
        PointRecord point;
        point.x = static_cast<float>(reader->get_x());
        point.y = static_cast<float>(reader->get_y());
        point.z = static_cast<float>(reader->get_z());
        point.a = 255;
        point.intensity = reader->point.get_intensity();
        point.classification = reader->point.get_classification();
        point.returnNumber = reader->point.get_return_number();
        point.numberOfReturns = reader->point.get_number_of_returns();
        point.gpsTime = reader->point.have_gps_time ? reader->point.get_gps_time() : 0.0;
        point.hasIntensity = true;
        point.hasClassification = true;
        point.hasReturnInfo = true;
        point.hasGpsTime = reader->point.have_gps_time;

        if (reader->point.have_rgb) {
            point.r = static_cast<std::uint8_t>(std::min<U16>(reader->point.get_R() / 256, 255));
            point.g = static_cast<std::uint8_t>(std::min<U16>(reader->point.get_G() / 256, 255));
            point.b = static_cast<std::uint8_t>(std::min<U16>(reader->point.get_B() / 256, 255));
        }

        output->appendPointFast(point);

        if (firstPoint) {
            minBounds = point;
            maxBounds = point;
            firstPoint = false;
        } else {
            minBounds.x = std::min(minBounds.x, point.x);
            minBounds.y = std::min(minBounds.y, point.y);
            minBounds.z = std::min(minBounds.z, point.z);
            maxBounds.x = std::max(maxBounds.x, point.x);
            maxBounds.y = std::max(maxBounds.y, point.y);
            maxBounds.z = std::max(maxBounds.z, point.z);
        }

        hasColor = hasColor || point.r != 255 || point.g != 255 || point.b != 255;
        hasIntensity = hasIntensity || point.hasIntensity;
        hasClassification = hasClassification || point.hasClassification;
        hasReturnInfo = hasReturnInfo || point.hasReturnInfo;
        hasGpsTime = hasGpsTime || point.hasGpsTime;

        ++loadedCount;
        if (progressCallback && (loadedCount == totalPoints || loadedCount % progressStep == 0)) {
            progressCallback({ loadedCount, totalPoints });
        }
    }

    const QString projectionText = extractProjectionInfo(reader->header);
    reader->close();

    if (loadedCount == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No points were read from file: %1").arg(filePath);
        }
        return false;
    }

    output->finalizeImport(
        minBounds,
        maxBounds,
        hasColor,
        hasIntensity,
        hasClassification,
        hasReturnInfo,
        hasGpsTime);

    if (metadata != nullptr) {
        metadata->pointCount = loadedCount;
        metadata->minBounds = minBounds;
        metadata->maxBounds = maxBounds;
        metadata->hasColor = hasColor;
        metadata->hasIntensity = hasIntensity;
        metadata->hasClassification = hasClassification;
        metadata->hasReturnInfo = hasReturnInfo;
        metadata->hasGpsTime = hasGpsTime;
        metadata->projectionText = projectionText;
    }

    if (progressCallback && loadedCount > 0) {
        progressCallback({ loadedCount, totalPoints > 0 ? totalPoints : loadedCount });
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Successfully loaded %1 points").arg(loadedCount);
    }

    return true;
#endif
}

bool LasReader::readPreview(
    const QString& filePath,
    PointCloudData* output,
    std::size_t targetPreviewPoints,
    std::size_t maxScanPoints,
    QString* errorMessage) const
{
    if (output == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Internal error: preview output buffer is null.";
        }
        return false;
    }

    output->clear();

#ifndef LAS_VIEWER_HAS_LASLIB
    if (errorMessage != nullptr) {
        *errorMessage = "LASlib support is not enabled in this build.";
    }
    return false;
#else
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("File not found: %1").arg(filePath);
        }
        return false;
    }

    const QByteArray nativePath = QDir::toNativeSeparators(filePath).toLocal8Bit();
    LASreadOpener opener;
    opener.set_file_name(nativePath.constData());

    std::unique_ptr<LASreader> reader(opener.open());
    if (!reader) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to open LAS/LAZ file: %1").arg(filePath);
        }
        return false;
    }

    const auto maxSizeT = static_cast<I64>(std::numeric_limits<std::size_t>::max());
    const std::size_t totalPoints = (reader->npoints > 0 && reader->npoints <= maxSizeT)
        ? static_cast<std::size_t>(reader->npoints)
        : 0;
    const std::size_t scanLimit = totalPoints > 0
        ? std::min(totalPoints, std::max(targetPreviewPoints, maxScanPoints))
        : std::max(targetPreviewPoints, maxScanPoints);
    const std::size_t stride = targetPreviewPoints > 0
        ? std::max<std::size_t>(1, scanLimit / std::max<std::size_t>(1, targetPreviewPoints))
        : 1;

    output->reserve(targetPreviewPoints);
    PointRecord minBounds;
    PointRecord maxBounds;
    minBounds.x = minBounds.y = minBounds.z = std::numeric_limits<float>::max();
    maxBounds.x = maxBounds.y = maxBounds.z = std::numeric_limits<float>::lowest();
    bool hasColor = false;
    bool hasIntensity = false;
    bool hasClassification = false;
    bool hasReturnInfo = false;
    bool hasGpsTime = false;
    bool firstPoint = true;

    std::size_t scannedCount = 0;
    std::size_t sampledCount = 0;
    while (scannedCount < scanLimit && reader->read_point()) {
        if ((scannedCount % stride) == 0) {
            PointRecord point;
            point.x = static_cast<float>(reader->get_x());
            point.y = static_cast<float>(reader->get_y());
            point.z = static_cast<float>(reader->get_z());
            point.a = 255;
            point.intensity = reader->point.get_intensity();
            point.classification = reader->point.get_classification();
            point.returnNumber = reader->point.get_return_number();
            point.numberOfReturns = reader->point.get_number_of_returns();
            point.gpsTime = reader->point.have_gps_time ? reader->point.get_gps_time() : 0.0;
            point.hasIntensity = true;
            point.hasClassification = true;
            point.hasReturnInfo = true;
            point.hasGpsTime = reader->point.have_gps_time;

            if (reader->point.have_rgb) {
                point.r = static_cast<std::uint8_t>(std::min<U16>(reader->point.get_R() / 256, 255));
                point.g = static_cast<std::uint8_t>(std::min<U16>(reader->point.get_G() / 256, 255));
                point.b = static_cast<std::uint8_t>(std::min<U16>(reader->point.get_B() / 256, 255));
            }

            output->appendPointFast(point);
            if (firstPoint) {
                minBounds = point;
                maxBounds = point;
                firstPoint = false;
            } else {
                minBounds.x = std::min(minBounds.x, point.x);
                minBounds.y = std::min(minBounds.y, point.y);
                minBounds.z = std::min(minBounds.z, point.z);
                maxBounds.x = std::max(maxBounds.x, point.x);
                maxBounds.y = std::max(maxBounds.y, point.y);
                maxBounds.z = std::max(maxBounds.z, point.z);
            }

            hasColor = hasColor || point.r != 255 || point.g != 255 || point.b != 255;
            hasIntensity = hasIntensity || point.hasIntensity;
            hasClassification = hasClassification || point.hasClassification;
            hasReturnInfo = hasReturnInfo || point.hasReturnInfo;
            hasGpsTime = hasGpsTime || point.hasGpsTime;
            ++sampledCount;
        }

        ++scannedCount;
    }

    reader->close();

    if (sampledCount == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No preview points were read from file: %1").arg(filePath);
        }
        return false;
    }

    output->finalizeImport(
        minBounds,
        maxBounds,
        hasColor,
        hasIntensity,
        hasClassification,
        hasReturnInfo,
        hasGpsTime);

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Preview loaded with %1 sampled points").arg(sampledCount);
    }

    return true;
#endif
}
