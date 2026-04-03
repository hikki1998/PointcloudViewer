#pragma once

#include <QColor>
#include <QMap>

enum class PointCloudColorMode
{
    Rgb = 0,
    Elevation,
    SingleColor,
    Classification
};

inline QMap<int, QColor> defaultPointClassificationColors()
{
    QMap<int, QColor> colors;
    colors.insert(0, QColor(148, 163, 184));  // Unclassified
    colors.insert(1, QColor(100, 116, 139));  // Unassigned
    colors.insert(2, QColor(146, 108, 74));   // Ground
    colors.insert(3, QColor(190, 242, 100));  // Low Vegetation
    colors.insert(4, QColor(74, 222, 128));   // Medium Vegetation
    colors.insert(5, QColor(22, 163, 74));    // High Vegetation
    colors.insert(6, QColor(251, 146, 60));   // Building
    colors.insert(7, QColor(239, 68, 68));    // Low Point / Noise
    colors.insert(9, QColor(14, 165, 233));   // Water
    colors.insert(13, QColor(168, 85, 247));  // Wire / Conductor
    return colors;
}

inline QColor defaultPointClassificationFallbackColor()
{
    return QColor(203, 213, 225);
}

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
    QMap<int, QColor> classificationColors = defaultPointClassificationColors();
    QColor classificationFallbackColor = defaultPointClassificationFallbackColor();
    QColor backgroundColor = QColor(241, 244, 249);
    float depthCueStrength = 0.0f;
    float edlStrength = 0.0f;
    bool useRoundSplats = true;
    bool showAxes = true;
    bool showBoundingBox = true;
};
