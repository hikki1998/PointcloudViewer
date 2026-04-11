#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace lasviewer::gui
{
enum class RibbonGlyph
{
    Open,
    Clear,
    Exit,
    Fit,
    Top,
    Front,
    Right,
    Axes,
    Bounds,
    DarkBackground,
    LightBackground,
    Rgb,
    Elevation,
    SingleColor,
    Classification,
    ThemeColorful,
    ThemeWhite,
    ThemeDarkGray,
    Log,
    Measure,
    Tower,
    TowerAdd,
    TowerInsert,
    TowerMove,
    TowerAdjust,
    TowerFocus,
    TowerRemove,
    Language
};

enum class WindowControlGlyph
{
    Minimize,
    Maximize,
    Restore,
    Close
};

QIcon createRibbonIcon(RibbonGlyph glyph);
QIcon createResourceIconOrFallback(const QString& resourcePath, RibbonGlyph fallbackGlyph);
QIcon createWindowControlIcon(WindowControlGlyph glyph, const QColor& color);
}
