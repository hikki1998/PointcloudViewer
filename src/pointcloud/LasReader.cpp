#include "pointcloud/LasReader.h"

#include <QDir>
#include <QFileInfo>

#include "pointcloud/PointCloudData.h"

#if defined(LAS_VIEWER_HAS_LASLIB) || defined(LAS_VIEWER_HAS_LASZIP_API)
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <QStringList>
#endif

#ifdef LAS_VIEWER_HAS_LASLIB
#include "lasreader.hpp"
#endif

#ifdef LAS_VIEWER_HAS_LASZIP_API
#include <laszip_api.h>
#endif

#if defined(LAS_VIEWER_HAS_LASLIB) || defined(LAS_VIEWER_HAS_LASZIP_API)
namespace
{
bool pointFormatHasColor(int pointFormat)
{
    switch (pointFormat) {
    case 2:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
        return true;
    default:
        return false;
    }
}

bool pointFormatHasGpsTime(int pointFormat)
{
    switch (pointFormat) {
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
        return true;
    default:
        return false;
    }
}

std::uint8_t colorChannelToByte(std::uint16_t value)
{
    return static_cast<std::uint8_t>(std::min<std::uint16_t>(value / 256, 255));
}

#ifdef LAS_VIEWER_HAS_LASLIB
QString extractProjectionInfo(const LASheader& header);

void populateMetadataFromHeader(const LASheader& header, std::size_t pointCount, LasFileMetadata* metadata)
{
    if (metadata == nullptr) {
        return;
    }

    metadata->pointCount = pointCount;
    metadata->minBounds.x = header.min_x;
    metadata->minBounds.y = header.min_y;
    metadata->minBounds.z = header.min_z;
    metadata->maxBounds.x = header.max_x;
    metadata->maxBounds.y = header.max_y;
    metadata->maxBounds.z = header.max_z;
    metadata->hasColor = pointFormatHasColor(header.point_data_format);
    metadata->hasIntensity = true;
    metadata->hasClassification = true;
    metadata->hasReturnInfo = true;
    metadata->hasGpsTime = pointFormatHasGpsTime(header.point_data_format);
    metadata->projectionText = extractProjectionInfo(header);
}

PointRecord readPointRecord(const LASreader& reader)
{
    PointRecord point;
    point.x = reader.get_x();
    point.y = reader.get_y();
    point.z = reader.get_z();
    point.a = 255;
    point.intensity = reader.point.get_intensity();
    point.classification = reader.point.get_classification();
    point.returnNumber = reader.point.get_return_number();
    point.numberOfReturns = reader.point.get_number_of_returns();
    point.gpsTime = reader.point.have_gps_time ? reader.point.get_gps_time() : 0.0;
    point.hasIntensity = true;
    point.hasClassification = true;
    point.hasReturnInfo = true;
    point.hasGpsTime = reader.point.have_gps_time;

    if (reader.point.have_rgb) {
        point.r = colorChannelToByte(reader.point.get_R());
        point.g = colorChannelToByte(reader.point.get_G());
        point.b = colorChannelToByte(reader.point.get_B());
    }

    return point;
}

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
#endif

#ifdef LAS_VIEWER_HAS_LASZIP_API
struct LaszipReaderDeleter
{
    void operator()(void* pointer) const
    {
        if (pointer != nullptr) {
            laszip_close_reader(pointer);
            laszip_destroy(pointer);
        }
    }
};

using LaszipReaderPtr = std::unique_ptr<void, LaszipReaderDeleter>;

QString laszipError(laszip_POINTER reader, const QString& fallback)
{
    if (reader == nullptr) {
        return fallback;
    }

    laszip_CHAR* error = nullptr;
    if (laszip_get_error(reader, &error) == 0 && error != nullptr && error[0] != '\0') {
        return QString::fromLocal8Bit(error);
    }
    return fallback;
}

LaszipReaderPtr openLaszipReader(const QString& filePath, QString* errorMessage)
{
    laszip_POINTER rawReader = nullptr;
    if (laszip_create(&rawReader) != 0 || rawReader == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to initialize LASzip reader.");
        }
        return LaszipReaderPtr(nullptr);
    }

    LaszipReaderPtr reader(rawReader);
    const QByteArray nativePath = QDir::toNativeSeparators(filePath).toLocal8Bit();
    laszip_BOOL isCompressed = 0;
    if (laszip_open_reader(reader.get(), nativePath.constData(), &isCompressed) != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = laszipError(
                reader.get(),
                QStringLiteral("Failed to open LAS/LAZ file: %1").arg(filePath));
        }
        return LaszipReaderPtr(nullptr);
    }

    return reader;
}

std::size_t pointCountFromHeader(const laszip_header_struct& header)
{
    if (header.extended_number_of_point_records > 0) {
        const std::uint64_t maxSizeT = static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
        return header.extended_number_of_point_records <= maxSizeT
            ? static_cast<std::size_t>(header.extended_number_of_point_records)
            : 0;
    }
    return static_cast<std::size_t>(header.number_of_point_records);
}

QString boundedString(const char* data, std::size_t size)
{
    if (data == nullptr || size == 0) {
        return QString();
    }

    std::size_t textSize = 0;
    while (textSize < size && data[textSize] != '\0') {
        ++textSize;
    }
    return QString::fromLocal8Bit(data, static_cast<int>(textSize)).trimmed();
}

std::uint16_t readLittleEndianU16(const laszip_U8* data)
{
    return static_cast<std::uint16_t>(data[0])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8);
}

QString extractGeoKeySummary(const laszip_vlr_struct& vlr)
{
    if (vlr.data == nullptr || vlr.record_length_after_header < 8) {
        return QString();
    }

    const laszip_U8* data = vlr.data;
    const std::uint16_t keyCount = readLittleEndianU16(data + 6);
    const std::size_t requiredSize = 8 + static_cast<std::size_t>(keyCount) * 8;
    if (requiredSize > vlr.record_length_after_header) {
        return QString();
    }

    QStringList keyParts;
    for (std::uint16_t index = 0; index < keyCount; ++index) {
        const laszip_U8* keyData = data + 8 + static_cast<std::size_t>(index) * 8;
        const std::uint16_t keyId = readLittleEndianU16(keyData);
        const std::uint16_t valueOffset = readLittleEndianU16(keyData + 6);
        switch (keyId) {
        case 1024:
            keyParts.append(QStringLiteral("ModelType=%1").arg(valueOffset));
            break;
        case 2048:
            keyParts.append(QStringLiteral("GeographicType=%1").arg(valueOffset));
            break;
        case 3072:
            keyParts.append(QStringLiteral("ProjectedType=%1").arg(valueOffset));
            break;
        case 4096:
            keyParts.append(QStringLiteral("VerticalType=%1").arg(valueOffset));
            break;
        default:
            break;
        }
    }

    return keyParts.join(QStringLiteral(", "));
}

QString extractProjectionInfo(const laszip_header_struct& header)
{
    QString geoKeySummary;
    for (laszip_U32 index = 0; index < header.number_of_variable_length_records; ++index) {
        const laszip_vlr_struct& vlr = header.vlrs[index];
        const QString userId = boundedString(vlr.user_id, sizeof(vlr.user_id));
        if (userId != QLatin1String("LASF_Projection")) {
            continue;
        }

        if (vlr.data != nullptr && (vlr.record_id == 2111 || vlr.record_id == 2112)) {
            const QString wkt = boundedString(
                reinterpret_cast<const char*>(vlr.data),
                vlr.record_length_after_header);
            if (!wkt.isEmpty()) {
                return wkt;
            }
        }

        if (vlr.data != nullptr && vlr.record_id == 34737) {
            QString asciiProjection = boundedString(
                reinterpret_cast<const char*>(vlr.data),
                vlr.record_length_after_header);
            asciiProjection.replace(QLatin1Char('|'), QLatin1Char('\n'));
            if (!asciiProjection.trimmed().isEmpty()) {
                return asciiProjection.trimmed();
            }
        }

        if (vlr.record_id == 34735 && geoKeySummary.isEmpty()) {
            geoKeySummary = extractGeoKeySummary(vlr);
        }
    }

    return geoKeySummary.isEmpty() ? QStringLiteral("Unknown") : geoKeySummary;
}

void populateMetadataFromHeader(const laszip_header_struct& header, LasFileMetadata* metadata)
{
    if (metadata == nullptr) {
        return;
    }

    metadata->pointCount = pointCountFromHeader(header);
    metadata->minBounds.x = header.min_x;
    metadata->minBounds.y = header.min_y;
    metadata->minBounds.z = header.min_z;
    metadata->maxBounds.x = header.max_x;
    metadata->maxBounds.y = header.max_y;
    metadata->maxBounds.z = header.max_z;
    metadata->hasColor = pointFormatHasColor(header.point_data_format);
    metadata->hasIntensity = true;
    metadata->hasClassification = true;
    metadata->hasReturnInfo = true;
    metadata->hasGpsTime = pointFormatHasGpsTime(header.point_data_format);
    metadata->projectionText = extractProjectionInfo(header);
}

PointRecord readPointRecord(
    laszip_POINTER reader,
    const laszip_point_struct& lasPoint,
    const laszip_header_struct& header)
{
    PointRecord point;
    laszip_F64 coordinates[3] = { 0.0, 0.0, 0.0 };
    if (laszip_get_coordinates(reader, coordinates) == 0) {
        point.x = coordinates[0];
        point.y = coordinates[1];
        point.z = coordinates[2];
    } else {
        point.x = static_cast<double>(lasPoint.X) * header.x_scale_factor + header.x_offset;
        point.y = static_cast<double>(lasPoint.Y) * header.y_scale_factor + header.y_offset;
        point.z = static_cast<double>(lasPoint.Z) * header.z_scale_factor + header.z_offset;
    }

    const bool extendedPointFormat = header.point_data_format >= 6;
    point.a = 255;
    point.intensity = lasPoint.intensity;
    point.classification = extendedPointFormat
        ? lasPoint.extended_classification
        : lasPoint.classification;
    point.returnNumber = extendedPointFormat
        ? lasPoint.extended_return_number
        : lasPoint.return_number;
    point.numberOfReturns = extendedPointFormat
        ? lasPoint.extended_number_of_returns
        : lasPoint.number_of_returns;
    point.gpsTime = pointFormatHasGpsTime(header.point_data_format) ? lasPoint.gps_time : 0.0;
    point.hasIntensity = true;
    point.hasClassification = true;
    point.hasReturnInfo = true;
    point.hasGpsTime = pointFormatHasGpsTime(header.point_data_format);

    if (pointFormatHasColor(header.point_data_format)) {
        point.r = colorChannelToByte(lasPoint.rgb[0]);
        point.g = colorChannelToByte(lasPoint.rgb[1]);
        point.b = colorChannelToByte(lasPoint.rgb[2]);
    }

    return point;
}
#endif
}
#endif

bool LasReader::readMetadata(
    const QString& filePath,
    LasFileMetadata* metadata,
    QString* errorMessage) const
{
    if (metadata == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Internal error: metadata output buffer is null.";
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

#if defined(LAS_VIEWER_HAS_LASLIB)
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
    populateMetadataFromHeader(reader->header, totalPoints, metadata);
    reader->close();

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Metadata loaded for %1 points").arg(totalPoints);
    }
    return true;
#elif defined(LAS_VIEWER_HAS_LASZIP_API)
    QString openError;
    LaszipReaderPtr reader = openLaszipReader(filePath, &openError);
    if (!reader) {
        if (errorMessage != nullptr) {
            *errorMessage = openError;
        }
        return false;
    }

    laszip_header_struct* header = nullptr;
    if (laszip_get_header_pointer(reader.get(), &header) != 0 || header == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = laszipError(reader.get(), QStringLiteral("Failed to read LAS/LAZ header."));
        }
        return false;
    }

    populateMetadataFromHeader(*header, metadata);
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Metadata loaded for %1 points").arg(metadata->pointCount);
    }
    return true;
#else
    if (errorMessage != nullptr) {
        *errorMessage = "LAS/LAZ support is not enabled in this build.";
    }
    return false;
#endif
}

bool LasReader::read(
    const QString& filePath,
    PointCloudData* output,
    QString* errorMessage,
    LasFileMetadata* metadata,
    ProgressCallback progressCallback,
    CancellationCallback cancellationCallback) const
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

#if !defined(LAS_VIEWER_HAS_LASLIB) && !defined(LAS_VIEWER_HAS_LASZIP_API)
    Q_UNUSED(metadata);
    if (errorMessage != nullptr) {
        *errorMessage = "LAS/LAZ support is not enabled in this build.";
    }
    return false;
#else
    PointRecord minBounds;
    PointRecord maxBounds;
    minBounds.x = minBounds.y = minBounds.z = std::numeric_limits<double>::max();
    maxBounds.x = maxBounds.y = maxBounds.z = std::numeric_limits<double>::lowest();
    bool hasColor = false;
    bool hasIntensity = false;
    bool hasClassification = false;
    bool hasReturnInfo = false;
    bool hasGpsTime = false;
    bool firstPoint = true;
    std::size_t loadedCount = 0;
    const auto pointCallback = [&output, &minBounds, &maxBounds, &hasColor, &hasIntensity, &hasClassification, &hasReturnInfo, &hasGpsTime, &firstPoint, &loadedCount](const PointRecord& point) {
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
    };

    QString pointReadMessage;
    if (!readPoints(filePath, pointCallback, &pointReadMessage, metadata, progressCallback, cancellationCallback)) {
        if (errorMessage != nullptr) {
            *errorMessage = pointReadMessage;
        }
        output->clear();
        return false;
    }

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
    }

    if (progressCallback && loadedCount > 0) {
        progressCallback({ loadedCount, metadata != nullptr && metadata->pointCount > 0 ? metadata->pointCount : loadedCount });
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Successfully loaded %1 points").arg(loadedCount);
    }

    return true;
#endif
}

bool LasReader::readPoints(
    const QString& filePath,
    PointCallback pointCallback,
    QString* errorMessage,
    LasFileMetadata* metadata,
    ProgressCallback progressCallback,
    CancellationCallback cancellationCallback) const
{
    if (!pointCallback) {
        if (errorMessage != nullptr) {
            *errorMessage = "Internal error: point callback is null.";
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

#if defined(LAS_VIEWER_HAS_LASLIB)
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
    populateMetadataFromHeader(reader->header, totalPoints, metadata);

    const std::size_t progressStep = totalPoints > 0
        ? std::max<std::size_t>(8192, totalPoints / 200)
        : static_cast<std::size_t>(65536);

    std::size_t loadedCount = 0;
    while (reader->read_point()) {
        if (cancellationCallback && cancellationCallback()) {
            reader->close();
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Point cloud loading was cancelled.");
            }
            return false;
        }

        pointCallback(readPointRecord(*reader));
        ++loadedCount;
        if (progressCallback && (loadedCount == totalPoints || loadedCount % progressStep == 0)) {
            progressCallback({ loadedCount, totalPoints });
        }
    }

    reader->close();

    if (metadata != nullptr) {
        metadata->pointCount = loadedCount > 0 ? loadedCount : metadata->pointCount;
    }
    if (progressCallback && loadedCount > 0) {
        progressCallback({ loadedCount, totalPoints > 0 ? totalPoints : loadedCount });
    }
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Successfully streamed %1 points").arg(loadedCount);
    }
    return loadedCount > 0;
#elif defined(LAS_VIEWER_HAS_LASZIP_API)
    QString openError;
    LaszipReaderPtr reader = openLaszipReader(filePath, &openError);
    if (!reader) {
        if (errorMessage != nullptr) {
            *errorMessage = openError;
        }
        return false;
    }

    laszip_header_struct* header = nullptr;
    laszip_point_struct* point = nullptr;
    if (laszip_get_header_pointer(reader.get(), &header) != 0 || header == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = laszipError(reader.get(), QStringLiteral("Failed to read LAS/LAZ header."));
        }
        return false;
    }
    if (laszip_get_point_pointer(reader.get(), &point) != 0 || point == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = laszipError(reader.get(), QStringLiteral("Failed to access LAS/LAZ point buffer."));
        }
        return false;
    }

    populateMetadataFromHeader(*header, metadata);
    const std::size_t totalPoints = pointCountFromHeader(*header);
    const std::size_t progressStep = totalPoints > 0
        ? std::max<std::size_t>(8192, totalPoints / 200)
        : static_cast<std::size_t>(65536);

    std::size_t loadedCount = 0;
    while (totalPoints == 0 || loadedCount < totalPoints) {
        if (cancellationCallback && cancellationCallback()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Point cloud loading was cancelled.");
            }
            return false;
        }

        if (laszip_read_point(reader.get()) != 0) {
            if (totalPoints == 0) {
                break;
            }
            if (errorMessage != nullptr) {
                *errorMessage = laszipError(reader.get(), QStringLiteral("Failed while reading LAS/LAZ point data."));
            }
            return false;
        }

        pointCallback(readPointRecord(reader.get(), *point, *header));
        ++loadedCount;
        if (progressCallback && (loadedCount == totalPoints || loadedCount % progressStep == 0)) {
            progressCallback({ loadedCount, totalPoints });
        }
    }

    if (metadata != nullptr) {
        metadata->pointCount = loadedCount > 0 ? loadedCount : metadata->pointCount;
    }
    if (progressCallback && loadedCount > 0) {
        progressCallback({ loadedCount, totalPoints > 0 ? totalPoints : loadedCount });
    }
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Successfully streamed %1 points").arg(loadedCount);
    }
    return loadedCount > 0;
#else
    if (errorMessage != nullptr) {
        *errorMessage = "LAS/LAZ support is not enabled in this build.";
    }
    return false;
#endif
}

bool LasReader::readPreview(
    const QString& filePath,
    PointCloudData* output,
    std::size_t targetPreviewPoints,
    std::size_t maxScanPoints,
    QString* errorMessage,
    CancellationCallback cancellationCallback) const
{
    if (output == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Internal error: preview output buffer is null.";
        }
        return false;
    }

    output->clear();

#if !defined(LAS_VIEWER_HAS_LASLIB) && !defined(LAS_VIEWER_HAS_LASZIP_API)
    if (errorMessage != nullptr) {
        *errorMessage = "LAS/LAZ support is not enabled in this build.";
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

    const std::size_t targetPoints = targetPreviewPoints > 0 ? targetPreviewPoints : 1;
    output->reserve(targetPoints);
    PointRecord minBounds;
    PointRecord maxBounds;
    minBounds.x = minBounds.y = minBounds.z = std::numeric_limits<double>::max();
    maxBounds.x = maxBounds.y = maxBounds.z = std::numeric_limits<double>::lowest();
    bool hasColor = false;
    bool hasIntensity = false;
    bool hasClassification = false;
    bool hasReturnInfo = false;
    bool hasGpsTime = false;
    bool firstPoint = true;
    std::size_t sampledCount = 0;

#if defined(LAS_VIEWER_HAS_LASLIB)
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
        ? std::min(totalPoints, std::max(targetPoints, maxScanPoints))
        : std::max(targetPoints, maxScanPoints);
    const std::size_t stride = std::max<std::size_t>(1, scanLimit / targetPoints);

    std::size_t scannedCount = 0;
    while (scannedCount < scanLimit && reader->read_point()) {
        if (cancellationCallback && cancellationCallback()) {
            reader->close();
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Point cloud loading was cancelled.");
            }
            output->clear();
            return false;
        }

        if ((scannedCount % stride) == 0) {
            const PointRecord point = readPointRecord(*reader);
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
#else
    QString openError;
    LaszipReaderPtr reader = openLaszipReader(filePath, &openError);
    if (!reader) {
        if (errorMessage != nullptr) {
            *errorMessage = openError;
        }
        return false;
    }

    laszip_header_struct* header = nullptr;
    laszip_point_struct* point = nullptr;
    if (laszip_get_header_pointer(reader.get(), &header) != 0 || header == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = laszipError(reader.get(), QStringLiteral("Failed to read LAS/LAZ header."));
        }
        return false;
    }
    if (laszip_get_point_pointer(reader.get(), &point) != 0 || point == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = laszipError(reader.get(), QStringLiteral("Failed to access LAS/LAZ point buffer."));
        }
        return false;
    }

    const std::size_t totalPoints = pointCountFromHeader(*header);
    const std::size_t scanLimit = totalPoints > 0
        ? std::min(totalPoints, std::max(targetPoints, maxScanPoints))
        : std::max(targetPoints, maxScanPoints);
    const std::size_t stride = std::max<std::size_t>(1, scanLimit / targetPoints);

    std::size_t scannedCount = 0;
    while (scannedCount < scanLimit) {
        if (cancellationCallback && cancellationCallback()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Point cloud loading was cancelled.");
            }
            output->clear();
            return false;
        }

        if (laszip_read_point(reader.get()) != 0) {
            break;
        }

        if ((scannedCount % stride) == 0) {
            const PointRecord sampledPoint = readPointRecord(reader.get(), *point, *header);
            output->appendPointFast(sampledPoint);
            if (firstPoint) {
                minBounds = sampledPoint;
                maxBounds = sampledPoint;
                firstPoint = false;
            } else {
                minBounds.x = std::min(minBounds.x, sampledPoint.x);
                minBounds.y = std::min(minBounds.y, sampledPoint.y);
                minBounds.z = std::min(minBounds.z, sampledPoint.z);
                maxBounds.x = std::max(maxBounds.x, sampledPoint.x);
                maxBounds.y = std::max(maxBounds.y, sampledPoint.y);
                maxBounds.z = std::max(maxBounds.z, sampledPoint.z);
            }

            hasColor = hasColor || sampledPoint.r != 255 || sampledPoint.g != 255 || sampledPoint.b != 255;
            hasIntensity = hasIntensity || sampledPoint.hasIntensity;
            hasClassification = hasClassification || sampledPoint.hasClassification;
            hasReturnInfo = hasReturnInfo || sampledPoint.hasReturnInfo;
            hasGpsTime = hasGpsTime || sampledPoint.hasGpsTime;
            ++sampledCount;
        }

        ++scannedCount;
    }
#endif

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
