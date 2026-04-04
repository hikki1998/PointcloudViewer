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
    colors.insert(0, QColor(148, 163, 184));  // Created / Unclassified
    colors.insert(1, QColor(100, 116, 139));  // Unclassified Point
    colors.insert(2, QColor(143, 68, 67));    // Ground Point
    colors.insert(3, QColor(172, 219, 74));   // Low Vegetation Point
    colors.insert(4, QColor(92, 224, 0));     // Medium Tree Point
    colors.insert(5, QColor(34, 139, 34));    // High Vegetation Point
    colors.insert(6, QColor(160, 120, 90));   // Building Point
    colors.insert(7, QColor(136, 132, 96));   // Low Point
    colors.insert(8, QColor(140, 16, 255));   // Model Key Point
    colors.insert(9, QColor(150, 150, 150));  // Temporary Structure
    colors.insert(10, QColor(101, 120, 173)); // Bridge
    colors.insert(11, QColor(96, 96, 96));    // Railway
    colors.insert(12, QColor(120, 120, 120)); // Highway
    colors.insert(13, QColor(64, 156, 255));  // Non-navigable River
    colors.insert(14, QColor(28, 126, 214));  // Lake
    colors.insert(15, QColor(171, 71, 188));  // Substation
    colors.insert(16, QColor(255, 128, 0));   // Conductor
    colors.insert(17, QColor(96, 96, 96));    // Tower
    colors.insert(18, QColor(255, 150, 204)); // Crossing Above
    colors.insert(19, QColor(255, 124, 124)); // Crossing Below
    colors.insert(20, QColor(255, 142, 10));  // Ground Wire
    colors.insert(21, QColor(188, 188, 188)); // Other
    colors.insert(22, QColor(77, 182, 172));  // Boat / Vehicle
    colors.insert(23, QColor(0, 150, 136));   // Other Line
    colors.insert(24, QColor(121, 85, 72));   // Under-Line Structure
    colors.insert(25, QColor(3, 169, 244));   // Navigable River
    colors.insert(26, QColor(255, 193, 7));   // Railway Catenary / Contact Wire
    colors.insert(27, QColor(255, 194, 52));  // Insulator
    colors.insert(28, QColor(245, 192, 192)); // Jumper Wire
    colors.insert(29, QColor(86, 86, 86));    // Tower Body
    colors.insert(30, QColor(176, 176, 176)); // Reserved30
    colors.insert(31, QColor(233, 30, 99));   // Sag Zone
    return colors;
}

inline QMap<int, bool> defaultPointClassificationVisibility()
{
    QMap<int, bool> visibility;
    visibility.insert(-1, true); // Other / Unknown
    for (int classification = 0; classification <= 31; ++classification) {
        visibility.insert(classification, true);
    }
    return visibility;
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
    QMap<int, bool> classificationVisibility = defaultPointClassificationVisibility();
    QColor classificationFallbackColor = defaultPointClassificationFallbackColor();
    QColor backgroundColor = QColor(241, 244, 249);
    float depthCueStrength = 0.0f;
    float edlStrength = 0.0f;
    bool useRoundSplats = true;
    bool showAxes = true;
    bool showBoundingBox = true;
};
