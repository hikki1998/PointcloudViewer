#include "gaussian/GaussianPlyReader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <thread>
#include <vector>

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>

namespace
{
constexpr float kSh0 = 0.28209479177387814f;

struct PlyHeader
{
    qint64 dataOffset = 0;
    qsizetype vertexCount = 0;
    int rowSize = 0;
    QHash<QString, int> propertyOffsets;
    QVector3D fileOffset;
};

struct GaussianPropertyOffsets
{
    int x = 0;
    int y = 0;
    int z = 0;
    int fDc0 = 0;
    int fDc1 = 0;
    int fDc2 = 0;
    int opacity = 0;
    int scale0 = 0;
    int scale1 = 0;
    int scale2 = 0;
    int rot0 = 0;
    int rot1 = 0;
    int rot2 = 0;
    int rot3 = 0;
};

struct Bounds
{
    QVector3D min = QVector3D(
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max());
    QVector3D max = QVector3D(
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest());
};

float readFloat(const char* row, int offset)
{
    float value = 0.0f;
    std::memcpy(&value, row + offset, sizeof(value));
    return value;
}

bool parseHeader(QFile& file, PlyHeader* header, QString* errorMessage)
{
    if (header == nullptr) {
        return false;
    }

    const QByteArray magic = file.readLine();
    const QByteArray format = file.readLine();
    if (magic.trimmed() != "ply" || format.trimmed() != "format binary_little_endian 1.0") {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Only binary little-endian Gaussian PLY files are supported.");
        }
        return false;
    }

    bool readingVertexProperties = false;
    while (!file.atEnd()) {
        const QByteArray rawLine = file.readLine();
        const QString line = QString::fromLatin1(rawLine).trimmed();
        if (line == QStringLiteral("end_header")) {
            header->dataOffset = file.pos();
            break;
        }
        if (line.startsWith(QStringLiteral("comment Offset:"), Qt::CaseInsensitive)) {
            const QStringList values = line.mid(line.indexOf(QLatin1Char(':')) + 1).simplified().split(QLatin1Char(' '));
            if (values.size() >= 3) {
                header->fileOffset = QVector3D(values[0].toFloat(), values[1].toFloat(), values[2].toFloat());
            }
            continue;
        }
        if (line.startsWith(QStringLiteral("element "))) {
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            readingVertexProperties = parts.size() == 3 && parts[1] == QStringLiteral("vertex");
            if (readingVertexProperties) {
                bool ok = false;
                const qulonglong count = parts[2].toULongLong(&ok);
                if (!ok || count > static_cast<qulonglong>(std::numeric_limits<qsizetype>::max())) {
                    if (errorMessage != nullptr) {
                        *errorMessage = QStringLiteral("Invalid Gaussian vertex count.");
                    }
                    return false;
                }
                header->vertexCount = static_cast<qsizetype>(count);
            }
            continue;
        }
        if (readingVertexProperties && line.startsWith(QStringLiteral("property "))) {
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() != 3 || (parts[1] != QStringLiteral("float") && parts[1] != QStringLiteral("float32"))) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Gaussian PLY vertex properties must be float32 scalars.");
                }
                return false;
            }
            header->propertyOffsets.insert(parts[2], header->rowSize);
            header->rowSize += static_cast<int>(sizeof(float));
        }
    }

    static const std::array<const char*, 14> requiredProperties = {
        "x", "y", "z", "f_dc_0", "f_dc_1", "f_dc_2", "opacity",
        "scale_0", "scale_1", "scale_2", "rot_0", "rot_1", "rot_2", "rot_3"
    };
    QStringList missingProperties;
    for (const char* property : requiredProperties) {
        if (!header->propertyOffsets.contains(QString::fromLatin1(property))) {
            missingProperties.append(QString::fromLatin1(property));
        }
    }
    if (header->dataOffset <= 0 || header->vertexCount <= 0 || !missingProperties.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = missingProperties.isEmpty()
                ? QStringLiteral("Gaussian PLY header is incomplete.")
                : QStringLiteral("Not a supported Gaussian PLY. Missing: %1").arg(missingProperties.join(QStringLiteral(", ")));
        }
        return false;
    }
    return true;
}

float sigmoid(float value)
{
    return 1.0f / (1.0f + std::exp(-value));
}

void covarianceFromScaleRotation(
    const std::array<float, 3>& scale,
    const std::array<float, 4>& rotation,
    GaussianGpuRecord* output)
{
    float w = rotation[0];
    float x = rotation[1];
    float y = rotation[2];
    float z = rotation[3];
    const float length = std::sqrt(w * w + x * x + y * y + z * z);
    if (length > 1e-8f) {
        w /= length;
        x /= length;
        y /= length;
        z /= length;
    } else {
        w = 1.0f;
        x = y = z = 0.0f;
    }

    const float r00 = 1.0f - 2.0f * (y * y + z * z);
    const float r01 = 2.0f * (x * y - w * z);
    const float r02 = 2.0f * (x * z + w * y);
    const float r10 = 2.0f * (x * y + w * z);
    const float r11 = 1.0f - 2.0f * (x * x + z * z);
    const float r12 = 2.0f * (y * z - w * x);
    const float r20 = 2.0f * (x * z - w * y);
    const float r21 = 2.0f * (y * z + w * x);
    const float r22 = 1.0f - 2.0f * (x * x + y * y);

    const float sx2 = scale[0] * scale[0];
    const float sy2 = scale[1] * scale[1];
    const float sz2 = scale[2] * scale[2];
    output->covarianceA[0] = r00 * r00 * sx2 + r01 * r01 * sy2 + r02 * r02 * sz2;
    output->covarianceA[1] = r00 * r10 * sx2 + r01 * r11 * sy2 + r02 * r12 * sz2;
    output->covarianceA[2] = r00 * r20 * sx2 + r01 * r21 * sy2 + r02 * r22 * sz2;
    output->covarianceB[0] = r10 * r10 * sx2 + r11 * r11 * sy2 + r12 * r12 * sz2;
    output->covarianceB[1] = r10 * r20 * sx2 + r11 * r21 * sy2 + r12 * r22 * sz2;
    output->covarianceB[2] = r20 * r20 * sx2 + r21 * r21 * sy2 + r22 * r22 * sz2;
}

void convertRange(
    const uchar* mapped,
    qsizetype begin,
    qsizetype end,
    int rowSize,
    const GaussianPropertyOffsets& offsets,
    GaussianGpuRecord* output,
    Bounds* bounds)
{
    for (qsizetype index = begin; index < end; ++index) {
        const char* row = reinterpret_cast<const char*>(mapped) + static_cast<qint64>(index) * rowSize;
        GaussianGpuRecord& splat = output[index];
        const float x = readFloat(row, offsets.x);
        const float y = readFloat(row, offsets.y);
        const float z = readFloat(row, offsets.z);
        bounds->min.setX(std::min(bounds->min.x(), x));
        bounds->min.setY(std::min(bounds->min.y(), y));
        bounds->min.setZ(std::min(bounds->min.z(), z));
        bounds->max.setX(std::max(bounds->max.x(), x));
        bounds->max.setY(std::max(bounds->max.y(), y));
        bounds->max.setZ(std::max(bounds->max.z(), z));
        splat.positionAlpha[0] = x;
        splat.positionAlpha[1] = y;
        splat.positionAlpha[2] = z;
        splat.positionAlpha[3] = sigmoid(readFloat(row, offsets.opacity));

        const std::array<float, 3> scale = {
            std::exp(readFloat(row, offsets.scale0)),
            std::exp(readFloat(row, offsets.scale1)),
            std::exp(readFloat(row, offsets.scale2))
        };
        const std::array<float, 4> rotation = {
            readFloat(row, offsets.rot0), readFloat(row, offsets.rot1),
            readFloat(row, offsets.rot2), readFloat(row, offsets.rot3)
        };
        covarianceFromScaleRotation(scale, rotation, &splat);
        splat.color[0] = std::clamp(0.5f + kSh0 * readFloat(row, offsets.fDc0), 0.0f, 1.0f);
        splat.color[1] = std::clamp(0.5f + kSh0 * readFloat(row, offsets.fDc1), 0.0f, 1.0f);
        splat.color[2] = std::clamp(0.5f + kSh0 * readFloat(row, offsets.fDc2), 0.0f, 1.0f);
        splat.color[3] = 1.0f;
    }
}

void recenterRange(GaussianGpuRecord* splats, qsizetype begin, qsizetype end, const QVector3D& center)
{
    for (qsizetype index = begin; index < end; ++index) {
        splats[index].positionAlpha[0] -= center.x();
        splats[index].positionAlpha[1] -= center.y();
        splats[index].positionAlpha[2] -= center.z();
    }
}
}

bool GaussianPlyReader::read(const QString& filePath, GaussianModel* model, QString* errorMessage) const
{
    if (model == nullptr) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to open Gaussian PLY: %1").arg(QFileInfo(filePath).fileName());
        }
        return false;
    }

    PlyHeader header;
    if (!parseHeader(file, &header, errorMessage)) {
        return false;
    }

    const qint64 expectedBytes = static_cast<qint64>(header.vertexCount) * header.rowSize;
    if (file.size() - header.dataOffset < expectedBytes) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Gaussian PLY data is truncated.");
        }
        return false;
    }

    GaussianModel loaded;
    loaded.fileOffset = header.fileOffset;
    loaded.splats.resize(static_cast<std::size_t>(header.vertexCount));

    uchar* mapped = file.map(header.dataOffset, expectedBytes);
    if (mapped == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unable to map Gaussian PLY data into memory.");
        }
        return false;
    }

    const auto offset = [&header](const char* name) {
        return header.propertyOffsets.value(QString::fromLatin1(name));
    };
    const GaussianPropertyOffsets offsets = {
        offset("x"), offset("y"), offset("z"),
        offset("f_dc_0"), offset("f_dc_1"), offset("f_dc_2"), offset("opacity"),
        offset("scale_0"), offset("scale_1"), offset("scale_2"),
        offset("rot_0"), offset("rot_1"), offset("rot_2"), offset("rot_3")
    };

    const unsigned int hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
    const qsizetype minimumRowsPerThread = 250000;
    const unsigned int workerCount = std::max(
        1u,
        std::min(
            hardwareThreads,
            static_cast<unsigned int>((header.vertexCount + minimumRowsPerThread - 1) / minimumRowsPerThread)));
    std::vector<Bounds> threadBounds(workerCount);
    std::vector<std::thread> workers;
    workers.reserve(workerCount > 0 ? workerCount - 1 : 0);
    const qsizetype rowsPerWorker = (header.vertexCount + workerCount - 1) / workerCount;
    for (unsigned int workerIndex = 1; workerIndex < workerCount; ++workerIndex) {
        const qsizetype begin = static_cast<qsizetype>(workerIndex) * rowsPerWorker;
        const qsizetype end = std::min(header.vertexCount, begin + rowsPerWorker);
        workers.emplace_back(
            convertRange,
            mapped,
            begin,
            end,
            header.rowSize,
            std::cref(offsets),
            loaded.splats.data(),
            &threadBounds[workerIndex]);
    }
    convertRange(
        mapped,
        0,
        std::min(header.vertexCount, rowsPerWorker),
        header.rowSize,
        offsets,
        loaded.splats.data(),
        &threadBounds[0]);
    for (std::thread& worker : workers) {
        worker.join();
    }
    file.unmap(mapped);

    QVector3D minBounds = threadBounds[0].min;
    QVector3D maxBounds = threadBounds[0].max;
    for (unsigned int workerIndex = 1; workerIndex < workerCount; ++workerIndex) {
        minBounds.setX(std::min(minBounds.x(), threadBounds[workerIndex].min.x()));
        minBounds.setY(std::min(minBounds.y(), threadBounds[workerIndex].min.y()));
        minBounds.setZ(std::min(minBounds.z(), threadBounds[workerIndex].min.z()));
        maxBounds.setX(std::max(maxBounds.x(), threadBounds[workerIndex].max.x()));
        maxBounds.setY(std::max(maxBounds.y(), threadBounds[workerIndex].max.y()));
        maxBounds.setZ(std::max(maxBounds.z(), threadBounds[workerIndex].max.z()));
    }

    const QVector3D center = (minBounds + maxBounds) * 0.5f;
    workers.clear();
    for (unsigned int workerIndex = 1; workerIndex < workerCount; ++workerIndex) {
        const qsizetype begin = static_cast<qsizetype>(workerIndex) * rowsPerWorker;
        const qsizetype end = std::min(header.vertexCount, begin + rowsPerWorker);
        workers.emplace_back(recenterRange, loaded.splats.data(), begin, end, std::cref(center));
    }
    recenterRange(
        loaded.splats.data(),
        0,
        std::min(header.vertexCount, rowsPerWorker),
        center);
    for (std::thread& worker : workers) {
        worker.join();
    }
    loaded.minBounds = minBounds - center;
    loaded.maxBounds = maxBounds - center;
    loaded.fileOffset += center;
    *model = std::move(loaded);
    return true;
}
