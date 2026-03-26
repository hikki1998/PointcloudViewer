#include "pointcloud/LasReader.h"

#include <QDir>
#include <QFileInfo>

#include "pointcloud/PointCloudData.h"

#ifdef LAS_VIEWER_HAS_LASLIB
#include <algorithm>
#include <limits>
#include <memory>

#include "lasreader.hpp"
#endif

bool LasReader::read(const QString& filePath, PointCloudData* output, QString* errorMessage) const
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

    std::size_t loadedCount = 0;
    while (reader->read_point()) {
        reader->compute_coordinates();

        std::uint8_t r = 255;
        std::uint8_t g = 255;
        std::uint8_t b = 255;

        if (reader->point.have_rgb) {
            r = static_cast<std::uint8_t>(std::min<U16>(reader->point.get_R() / 256, 255));
            g = static_cast<std::uint8_t>(std::min<U16>(reader->point.get_G() / 256, 255));
            b = static_cast<std::uint8_t>(std::min<U16>(reader->point.get_B() / 256, 255));
        }

        output->addPoint(
            static_cast<float>(reader->point.coordinates[0]),
            static_cast<float>(reader->point.coordinates[1]),
            static_cast<float>(reader->point.coordinates[2]),
            r,
            g,
            b,
            255);

        ++loadedCount;
    }

    reader->close();

    if (loadedCount == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No points were read from file: %1").arg(filePath);
        }
        return false;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Successfully loaded %1 points").arg(loadedCount);
    }

    return true;
#endif
}
