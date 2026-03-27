#pragma once

#include <QColor>

enum class PointCloudColorMode
{
    Rgb = 0,
    Elevation,
    SingleColor
};

enum class PointCloudViewPreset
{
    Isometric = 0,
    Top,
    Front,
    Right
};

struct PointCloudVisualizationOptions
{
    float pointSize = 3.0f;
    float pointOpacity = 1.0f;
    PointCloudColorMode colorMode = PointCloudColorMode::Rgb;
    QColor singleColor = QColor(53, 142, 255);
    QColor backgroundColor = QColor(241, 244, 249);
    float depthCueStrength = 0.0f;
    float edlStrength = 0.0f;
    bool useRoundSplats = true;
    bool showAxes = true;
    bool showBoundingBox = true;
};
