#pragma once

#include <cstddef>
#include <vector>

#include <QVector3D>

struct GaussianGpuRecord
{
    float positionAlpha[4] = {};
    float covarianceA[4] = {};
    float covarianceB[4] = {};
    float color[4] = {};
};

static_assert(sizeof(GaussianGpuRecord) == sizeof(float) * 16, "Gaussian GPU record layout changed");

struct GaussianModel
{
    std::vector<GaussianGpuRecord> splats;
    QVector3D minBounds;
    QVector3D maxBounds;
    QVector3D fileOffset;

    [[nodiscard]] bool empty() const { return splats.empty(); }
    [[nodiscard]] std::size_t size() const { return splats.size(); }
};
