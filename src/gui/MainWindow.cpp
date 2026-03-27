#include "gui/MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QLibraryInfo>
#include <QLocale>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPushButton>
#include <QDropEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QTranslator>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <QMouseEvent>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "QtnRibbonBar.h"
#include "QtnRibbonGroup.h"
#include "QtnRibbonPage.h"
#include "QtnRibbonQuickAccessBar.h"

#include "gui/PointCloudViewer.h"
#include "osg/PointCloudVisualization.h"
#include "pointcloud/PointCloudData.h"

namespace
{
const QColor kDarkBackground(20, 28, 38);
const QColor kLightBackground(241, 244, 249);
const QColor kWindowChromeLight(243, 246, 251);
const QColor kWindowChromeDark(51, 65, 85);
const QColor kRibbonGlyphColor(28, 64, 111);
const QColor kRibbonAccentColor(59, 130, 246);
constexpr int kWindowResizeBorder = 8;

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
    ThemeColorful,
    ThemeWhite,
    ThemeDarkGray,
    Log,
    Measure,
    Language
};

enum class WindowControlGlyph
{
    Minimize,
    Maximize,
    Restore,
    Close
};

QString formatCoordinate(float value)
{
    return QLocale().toString(static_cast<double>(value), 'f', 2);
}

QString formatTriplet(float x, float y, float z)
{
    return QStringLiteral("%1, %2, %3")
        .arg(formatCoordinate(x))
        .arg(formatCoordinate(y))
        .arg(formatCoordinate(z));
}

QString colorModeName(PointCloudColorMode colorMode)
{
    switch (colorMode) {
    case PointCloudColorMode::Elevation:
        return QCoreApplication::translate("MainWindow", "Elevation ramp");
    case PointCloudColorMode::SingleColor:
        return QCoreApplication::translate("MainWindow", "Single color");
    case PointCloudColorMode::Rgb:
    default:
        return QCoreApplication::translate("MainWindow", "RGB");
    }
}

QString measurementPointText(const MeasurementResult& measurementResult, bool useStartPoint)
{
    const bool hasPoint = useStartPoint ? measurementResult.hasStartPoint : measurementResult.hasEndPoint;
    if (!hasPoint) {
        return QCoreApplication::translate("MainWindow", "Not set");
    }

    const PointRecord& point = useStartPoint ? measurementResult.startPoint : measurementResult.endPoint;
    return formatTriplet(point.x, point.y, point.z);
}

QString languageCodeFor(MainWindow::UiLanguage language)
{
    switch (language) {
    case MainWindow::UiLanguage::Chinese:
        return QStringLiteral("zh_CN");
    case MainWindow::UiLanguage::English:
    default:
        return QStringLiteral("en");
    }
}

MainWindow::UiLanguage defaultLanguageFromLocale()
{
    const QString localeName = QLocale::system().name().toLower();
    return localeName.startsWith(QStringLiteral("zh"))
        ? MainWindow::UiLanguage::Chinese
        : MainWindow::UiLanguage::English;
}

bool isSupportedPointCloudFile(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    const QString suffix = fileInfo.suffix().toLower();
    return suffix == QStringLiteral("las") || suffix == QStringLiteral("laz");
}

QIcon createRibbonIcon(RibbonGlyph glyph)
{
    constexpr int iconSize = 48;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF canvas(2.0, 2.0, iconSize - 4.0, iconSize - 4.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(248, 250, 252));
    painter.drawRoundedRect(canvas, 12.0, 12.0);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(203, 213, 225), 1.2));
    painter.drawRoundedRect(canvas, 12.0, 12.0);

    QPen glyphPen(kRibbonGlyphColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(glyphPen);

    const QRectF r(11.0, 11.0, 26.0, 26.0);
    switch (glyph) {
    case RibbonGlyph::Open:
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 8.0, 20.0, 12.0), 3.0, 3.0);
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 8.0), QPointF(r.left() + 12.0, r.top() + 4.5));
        painter.drawLine(QPointF(r.left() + 12.0, r.top() + 4.5), QPointF(r.left() + 18.0, r.top() + 4.5));
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.top() + 10.0), QPointF(r.center().x(), r.bottom() - 2.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom() - 2.0), QPointF(r.center().x() - 5.0, r.bottom() - 7.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom() - 2.0), QPointF(r.center().x() + 5.0, r.bottom() - 7.0));
        break;
    case RibbonGlyph::Clear:
        painter.drawRoundedRect(QRectF(r.left() + 3.0, r.top() + 10.0, 18.0, 12.0), 3.0, 3.0);
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 10.0), QPointF(r.left() + 12.0, r.top() + 5.0));
        painter.drawLine(QPointF(r.left() + 12.0, r.top() + 5.0), QPointF(r.left() + 18.0, r.top() + 5.0));
        painter.setPen(QPen(QColor(220, 38, 38), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 21.0, r.top() + 9.0), QPointF(r.right(), r.bottom() - 1.0));
        painter.drawLine(QPointF(r.right(), r.top() + 9.0), QPointF(r.left() + 21.0, r.bottom() - 1.0));
        break;
    case RibbonGlyph::Exit:
        painter.drawRoundedRect(QRectF(r.left() + 4.0, r.top() + 4.0, 14.0, 18.0), 3.0, 3.0);
        painter.setPen(QPen(QColor(220, 38, 38), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 20.0, r.center().y()), QPointF(r.right(), r.center().y()));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 5.0, r.center().y() - 5.0));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 5.0, r.center().y() + 5.0));
        break;
    case RibbonGlyph::Fit:
        painter.drawRect(QRectF(r.left() + 4.0, r.top() + 4.0, 18.0, 18.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left(), r.top() + 8.0), QPointF(r.left() + 6.0, r.top() + 8.0));
        painter.drawLine(QPointF(r.left() + 8.0, r.top()), QPointF(r.left() + 8.0, r.top() + 6.0));
        painter.drawLine(QPointF(r.right() - 6.0, r.top()), QPointF(r.right() - 6.0, r.top() + 6.0));
        painter.drawLine(QPointF(r.right() - 8.0, r.top() + 8.0), QPointF(r.right(), r.top() + 8.0));
        painter.drawLine(QPointF(r.left(), r.bottom() - 8.0), QPointF(r.left() + 6.0, r.bottom() - 8.0));
        painter.drawLine(QPointF(r.left() + 8.0, r.bottom() - 6.0), QPointF(r.left() + 8.0, r.bottom()));
        painter.drawLine(QPointF(r.right() - 6.0, r.bottom() - 6.0), QPointF(r.right() - 6.0, r.bottom()));
        painter.drawLine(QPointF(r.right() - 8.0, r.bottom() - 8.0), QPointF(r.right(), r.bottom() - 8.0));
        break;
    case RibbonGlyph::Top:
        painter.drawEllipse(QRectF(r.left() + 6.0, r.top() + 3.0, 14.0, 6.0));
        painter.drawLine(QPointF(r.left() + 6.0, r.top() + 6.0), QPointF(r.left() + 6.0, r.bottom() - 2.0));
        painter.drawLine(QPointF(r.left() + 20.0, r.top() + 6.0), QPointF(r.left() + 20.0, r.bottom() - 2.0));
        painter.drawArc(QRectF(r.left() + 6.0, r.bottom() - 8.0, 14.0, 6.0), 180 * 16, 180 * 16);
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x(), r.top() + 10.0));
        painter.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x() - 4.0, r.top() + 4.0));
        painter.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x() + 4.0, r.top() + 4.0));
        break;
    case RibbonGlyph::Front:
        painter.drawRect(QRectF(r.left() + 4.0, r.top() + 5.0, 18.0, 16.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.bottom()), QPointF(r.center().x(), r.top() + 15.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom()), QPointF(r.center().x() - 4.0, r.bottom() - 4.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom()), QPointF(r.center().x() + 4.0, r.bottom() - 4.0));
        break;
    case RibbonGlyph::Right:
        painter.drawRect(QRectF(r.left() + 5.0, r.top() + 5.0, 8.0, 16.0));
        painter.drawRect(QRectF(r.left() + 13.0, r.top() + 8.0, 8.0, 13.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.left() + 17.0, r.center().y()));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 4.0, r.center().y() - 4.0));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 4.0, r.center().y() + 4.0));
        break;
    case RibbonGlyph::Axes:
        painter.setPen(QPen(QColor(220, 38, 38), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 6.0, r.bottom() - 6.0), QPointF(r.right() - 2.0, r.bottom() - 6.0));
        painter.setPen(QPen(QColor(22, 163, 74), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 6.0, r.bottom() - 6.0), QPointF(r.left() + 6.0, r.top() + 2.0));
        painter.setPen(QPen(QColor(37, 99, 235), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 6.0, r.bottom() - 6.0), QPointF(r.right() - 6.0, r.top() + 6.0));
        break;
    case RibbonGlyph::Bounds:
        painter.drawRect(QRectF(r.left() + 5.0, r.top() + 8.0, 14.0, 14.0));
        painter.drawLine(QPointF(r.left() + 11.0, r.top() + 4.0), QPointF(r.right() - 1.0, r.top() + 10.0));
        painter.drawLine(QPointF(r.right() - 1.0, r.top() + 10.0), QPointF(r.right() - 1.0, r.bottom() - 4.0));
        painter.drawLine(QPointF(r.left() + 19.0, r.top() + 8.0), QPointF(r.right() - 1.0, r.top() + 14.0));
        painter.drawLine(QPointF(r.left() + 19.0, r.bottom() - 2.0), QPointF(r.right() - 1.0, r.bottom() - 8.0));
        break;
    case RibbonGlyph::DarkBackground:
        painter.setBrush(kDarkBackground);
        painter.setPen(QPen(QColor(51, 65, 85), 1.2));
        painter.drawRoundedRect(QRectF(r.left() + 1.0, r.top() + 5.0, 24.0, 16.0), 6.0, 6.0);
        painter.setPen(QPen(QColor(248, 250, 252), 1.8));
        painter.setBrush(QColor(248, 250, 252));
        painter.drawEllipse(QRectF(r.left() + 6.0, r.top() + 9.0, 5.0, 5.0));
        painter.drawEllipse(QRectF(r.left() + 14.0, r.top() + 12.0, 4.0, 4.0));
        break;
    case RibbonGlyph::LightBackground:
        painter.setBrush(kLightBackground);
        painter.setPen(QPen(QColor(148, 163, 184), 1.2));
        painter.drawRoundedRect(QRectF(r.left() + 1.0, r.top() + 5.0, 24.0, 16.0), 6.0, 6.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(251, 191, 36));
        painter.drawEllipse(QRectF(r.left() + 17.0, r.top() + 7.0, 7.0, 7.0));
        painter.setBrush(QColor(148, 163, 184));
        painter.drawEllipse(QRectF(r.left() + 7.0, r.top() + 12.0, 5.0, 5.0));
        break;
    case RibbonGlyph::Rgb:
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(239, 68, 68));
        painter.drawEllipse(QRectF(r.left() + 3.0, r.top() + 9.0, 8.0, 8.0));
        painter.setBrush(QColor(34, 197, 94));
        painter.drawEllipse(QRectF(r.left() + 11.0, r.top() + 9.0, 8.0, 8.0));
        painter.setBrush(QColor(59, 130, 246));
        painter.drawEllipse(QRectF(r.left() + 7.0, r.top() + 16.0, 8.0, 8.0));
        break;
    case RibbonGlyph::Elevation: {
        painter.setPen(QPen(QColor(148, 163, 184), 2.0));
        painter.drawLine(QPointF(r.left() + 2.0, r.bottom() - 3.0), QPointF(r.right() - 2.0, r.bottom() - 3.0));
        QLinearGradient gradient(QPointF(r.left(), r.bottom()), QPointF(r.right(), r.top()));
        gradient.setColorAt(0.0, QColor(37, 99, 235));
        gradient.setColorAt(0.5, QColor(16, 185, 129));
        gradient.setColorAt(1.0, QColor(249, 115, 22));
        painter.setPen(QPen(QBrush(gradient), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(QPolygonF({
            QPointF(r.left() + 2.0, r.bottom() - 6.0),
            QPointF(r.left() + 8.0, r.top() + 14.0),
            QPointF(r.left() + 15.0, r.top() + 8.0),
            QPointF(r.right() - 1.0, r.top() + 2.0)
        }));
        break;
    }
    case RibbonGlyph::SingleColor:
        painter.setPen(QPen(kRibbonGlyphColor, 2.2));
        painter.drawLine(QPointF(r.left() + 5.0, r.bottom() - 4.0), QPointF(r.right() - 6.0, r.top() + 5.0));
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 5.0), QPointF(r.right() - 3.0, r.bottom() - 4.0));
        painter.setBrush(QColor(53, 142, 255));
        painter.setPen(QPen(QColor(37, 99, 235), 1.2));
        painter.drawEllipse(QRectF(r.left() + 9.0, r.top() + 9.0, 8.0, 8.0));
        break;
    case RibbonGlyph::ThemeColorful:
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(59, 130, 246));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 5.0, 22.0, 5.0), 2.5, 2.5);
        painter.setBrush(QColor(244, 114, 182));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 13.0, 15.0, 5.0), 2.5, 2.5);
        painter.setBrush(QColor(16, 185, 129));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 21.0, 19.0, 5.0), 2.5, 2.5);
        break;
    case RibbonGlyph::ThemeWhite:
        painter.setPen(QPen(QColor(148, 163, 184), 1.8));
        painter.setBrush(QColor(255, 255, 255));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 4.0, 22.0, 18.0), 5.0, 5.0);
        painter.setPen(QPen(QColor(59, 130, 246), 2.5));
        painter.drawLine(QPointF(r.left() + 4.0, r.top() + 9.0), QPointF(r.right() - 4.0, r.top() + 9.0));
        break;
    case RibbonGlyph::ThemeDarkGray:
        painter.setPen(QPen(QColor(71, 85, 105), 1.8));
        painter.setBrush(QColor(51, 65, 85));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 4.0, 22.0, 18.0), 5.0, 5.0);
        painter.setPen(QPen(QColor(148, 163, 184), 2.5));
        painter.drawLine(QPointF(r.left() + 4.0, r.top() + 9.0), QPointF(r.right() - 4.0, r.top() + 9.0));
        break;
    case RibbonGlyph::Log:
        painter.drawRoundedRect(QRectF(r.left() + 3.0, r.top() + 4.0, 20.0, 18.0), 4.0, 4.0);
        painter.drawLine(QPointF(r.left() + 7.0, r.top() + 9.0), QPointF(r.right() - 3.0, r.top() + 9.0));
        painter.drawLine(QPointF(r.left() + 7.0, r.top() + 14.0), QPointF(r.right() - 6.0, r.top() + 14.0));
        painter.drawLine(QPointF(r.left() + 7.0, r.top() + 19.0), QPointF(r.right() - 9.0, r.top() + 19.0));
        painter.setBrush(kRibbonAccentColor);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 7.0, 2.8, 2.8));
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 12.0, 2.8, 2.8));
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 17.0, 2.8, 2.8));
        break;
    case RibbonGlyph::Measure:
        painter.drawEllipse(QRectF(r.left() + 3.0, r.top() + 7.0, 6.0, 6.0));
        painter.drawEllipse(QRectF(r.right() - 9.0, r.bottom() - 9.0, 6.0, 6.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 12.0), QPointF(r.right() - 6.0, r.bottom() - 6.0));
        break;
    case RibbonGlyph::Language:
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 4.0, 18.0, 18.0));
        painter.drawLine(QPointF(r.center().x(), r.top() + 4.0), QPointF(r.center().x(), r.bottom() + 4.0));
        painter.drawLine(QPointF(r.left() + 4.0, r.center().y()), QPointF(r.right() + 4.0, r.center().y()));
        painter.drawArc(QRectF(r.left() + 7.0, r.top() + 4.0, 12.0, 18.0), 90 * 16, 180 * 16);
        painter.drawArc(QRectF(r.left() + 7.0, r.top() + 4.0, 12.0, 18.0), 270 * 16, 180 * 16);
        break;
    }

    return QIcon(pixmap);
}

QIcon createWindowControlIcon(WindowControlGlyph glyph, const QColor& color)
{
    constexpr int iconSize = 12;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPen pen(color, glyph == WindowControlGlyph::Close ? 1.8 : 1.4, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    painter.setPen(pen);

    switch (glyph) {
    case WindowControlGlyph::Minimize:
        painter.drawLine(QPointF(2.0, 8.0), QPointF(10.0, 8.0));
        break;
    case WindowControlGlyph::Maximize:
        painter.drawRect(QRectF(2.0, 2.0, 8.0, 8.0));
        break;
    case WindowControlGlyph::Restore:
        painter.drawRect(QRectF(4.0, 2.0, 6.0, 6.0));
        painter.drawLine(QPointF(4.0, 4.0), QPointF(2.0, 4.0));
        painter.drawLine(QPointF(2.0, 4.0), QPointF(2.0, 10.0));
        painter.drawLine(QPointF(2.0, 10.0), QPointF(8.0, 10.0));
        break;
    case WindowControlGlyph::Close:
        painter.drawLine(QPointF(2.5, 2.5), QPointF(9.5, 9.5));
        painter.drawLine(QPointF(9.5, 2.5), QPointF(2.5, 9.5));
        break;
    }

    return QIcon(pixmap);
}
}

MainWindow::MainWindow(QTranslator* appTranslator, QTranslator* qtTranslator, QWidget* parent)
    : Qtitan::RibbonMainWindow(parent)
    , appTranslator_(appTranslator)
    , qtTranslator_(qtTranslator)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::WindowMinimizeButtonHint, true);
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    setWindowFlag(Qt::WindowCloseButtonHint, true);
    resize(1520, 920);
    setMinimumSize(960, 640);
    setAcceptDrops(true);

    loadLanguageSettings();

    viewer_ = new PointCloudViewer(this);
    setCentralWidget(viewer_);

    createActions();
    createRibbon();
    createInspectorPanel();
    createLogDock();
    createStatusBar();
    setDockNestingEnabled(true);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    loadInteractionSettings();
    loadVisualizationSettings();
    createConnections();
    applyLanguage(currentLanguage_);
    loadThemeSettings();
    loadWindowSettings();

    syncUiFromViewer();
    updateNavigationHelpText();
    showUserMessage(LogLevel::Info, tr("Ready. Open or drag a LAS/LAZ file to begin."), 4000);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    persistVisualizationSettings();
    persistInteractionSettings();
    persistLanguageSettings();
    persistThemeSettings();
    persistWindowSettings();
    Qtitan::RibbonMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event == nullptr || event->mimeData() == nullptr || !event->mimeData()->hasUrls()) {
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile() && isSupportedPointCloudFile(url.toLocalFile())) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (event == nullptr || event->mimeData() == nullptr || !event->mimeData()->hasUrls()) {
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }

        const QString filePath = url.toLocalFile();
        if (!isSupportedPointCloudFile(filePath)) {
            continue;
        }

        if (loadPointCloudFile(filePath)) {
            event->acceptProposedAction();
        }
        return;
    }

    showUserMessage(LogLevel::Warning, tr("Only LAS and LAZ files can be dropped here."), 4000);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ribbonBar_ && event != nullptr) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && isDraggableRibbonArea(mouseEvent->pos())) {
                toggleMaximizedWindow();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && !isMaximized() && isDraggableRibbonArea(mouseEvent->pos())) {
                if (QWindow* win = windowHandle()) {
                    win->startSystemMove();
                    return true;
                }
            }
        }
    }

    return Qtitan::RibbonMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent* event)
{
    Qtitan::RibbonMainWindow::changeEvent(event);

    if (event == nullptr) {
        return;
    }

    if (event->type() == QEvent::WindowStateChange) {
        updateWindowControlButtons();
    } else if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, long* result)
{
    Q_UNUSED(eventType);

    if (message != nullptr && result != nullptr) {
        MSG* nativeMessage = static_cast<MSG*>(message);
        if (nativeMessage->message == WM_NCHITTEST && !isFullScreen()) {
            const int globalX = static_cast<short>(LOWORD(nativeMessage->lParam));
            const int globalY = static_cast<short>(HIWORD(nativeMessage->lParam));
            const QPoint localPosition = mapFromGlobal(QPoint(globalX, globalY));
            if (rect().contains(localPosition)) {
                if (!isMaximized() && !isWindowControlWidget(childAt(localPosition))) {
                    const bool onLeft = localPosition.x() >= 0 && localPosition.x() < kWindowResizeBorder;
                    const bool onRight = localPosition.x() < width() && localPosition.x() >= width() - kWindowResizeBorder;
                    const bool onTop = localPosition.y() >= 0 && localPosition.y() < kWindowResizeBorder;
                    const bool onBottom = localPosition.y() < height() && localPosition.y() >= height() - kWindowResizeBorder;

                    if (onTop && onLeft) {
                        *result = HTTOPLEFT;
                        return true;
                    }
                    if (onTop && onRight) {
                        *result = HTTOPRIGHT;
                        return true;
                    }
                    if (onBottom && onLeft) {
                        *result = HTBOTTOMLEFT;
                        return true;
                    }
                    if (onBottom && onRight) {
                        *result = HTBOTTOMRIGHT;
                        return true;
                    }
                    if (onLeft) {
                        *result = HTLEFT;
                        return true;
                    }
                    if (onRight) {
                        *result = HTRIGHT;
                        return true;
                    }
                    if (onTop) {
                        *result = HTTOP;
                        return true;
                    }
                    if (onBottom) {
                        *result = HTBOTTOM;
                        return true;
                    }
                }

                if (ribbonBar_ != nullptr && ribbonBar_->geometry().contains(localPosition)) {
                    const QPoint ribbonPosition = ribbonBar_->mapFrom(this, localPosition);
                    if (isDraggableRibbonArea(ribbonPosition)) {
                        *result = HTCAPTION;
                        return true;
                    }
                }
            }
        }
    }

    return Qtitan::RibbonMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::createActions()
{
    openAction_ = new QAction(createRibbonIcon(RibbonGlyph::Open), tr("Open"), this);
    openAction_->setShortcut(QKeySequence::Open);
    openAction_->setToolTip(tr("Open a LAS or LAZ dataset"));

    clearAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear"), this);
    clearAction_->setToolTip(tr("Clear the current scene"));

    exitAction_ = new QAction(createRibbonIcon(RibbonGlyph::Exit), tr("Exit"), this);
    exitAction_->setShortcut(QKeySequence::Quit);

    fitSceneAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Fit Scene"), this);
    fitSceneAction_->setToolTip(tr("Reset to a fitted isometric view"));

    topViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Top), tr("Top"), this);
    frontViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Front), tr("Front"), this);
    rightViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Right), tr("Right"), this);

    showAxesAction_ = new QAction(createRibbonIcon(RibbonGlyph::Axes), tr("Axes"), this);
    showAxesAction_->setCheckable(true);

    showBoundingBoxAction_ = new QAction(createRibbonIcon(RibbonGlyph::Bounds), tr("Bounds"), this);
    showBoundingBoxAction_->setCheckable(true);

    darkBackgroundAction_ = new QAction(createRibbonIcon(RibbonGlyph::DarkBackground), tr("Dark"), this);
    lightBackgroundAction_ = new QAction(createRibbonIcon(RibbonGlyph::LightBackground), tr("Light"), this);

    colorModeActionGroup_ = new QActionGroup(this);
    colorModeActionGroup_->setExclusive(true);

    rgbColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::Rgb), tr("RGB"), this);
    rgbColorAction_->setCheckable(true);
    elevationColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::Elevation), tr("Elevation"), this);
    elevationColorAction_->setCheckable(true);
    singleColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::SingleColor), tr("Single"), this);
    singleColorAction_->setCheckable(true);

    colorModeActionGroup_->addAction(rgbColorAction_);
    colorModeActionGroup_->addAction(elevationColorAction_);
    colorModeActionGroup_->addAction(singleColorAction_);

    themeActionGroup_ = new QActionGroup(this);
    themeActionGroup_->setExclusive(true);

    themeColorfulAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeColorful), tr("Colorful"), this);
    themeColorfulAction_->setCheckable(true);
    themeWhiteAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeWhite), tr("White"), this);
    themeWhiteAction_->setCheckable(true);
    themeDarkGrayAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeDarkGray), tr("Dark Gray"), this);
    themeDarkGrayAction_->setCheckable(true);

    themeActionGroup_->addAction(themeColorfulAction_);
    themeActionGroup_->addAction(themeWhiteAction_);
    themeActionGroup_->addAction(themeDarkGrayAction_);

    measureAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Measure"), this);
    measureAction_->setCheckable(true);

    clearMeasurementAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Measure"), this);

    showLogAction_ = new QAction(createRibbonIcon(RibbonGlyph::Log), tr("Log"), this);
    showLogAction_->setCheckable(true);
    showLogAction_->setChecked(false);
    showLogAction_->setToolTip(tr("Show or hide the log panel"));

    languageActionGroup_ = new QActionGroup(this);
    languageActionGroup_->setExclusive(true);

    languageEnglishAction_ = new QAction(createRibbonIcon(RibbonGlyph::Language), QStringLiteral("English"), this);
    languageEnglishAction_->setCheckable(true);
    languageChineseAction_ = new QAction(createRibbonIcon(RibbonGlyph::Language), QStringLiteral("\u4e2d\u6587"), this);
    languageChineseAction_->setCheckable(true);

    languageActionGroup_->addAction(languageEnglishAction_);
    languageActionGroup_->addAction(languageChineseAction_);
}

void MainWindow::createRibbon()
{
    ribbonBar_ = new Qtitan::RibbonBar(this);
    // Avoid Qtitan's DWM-integrated frame path, which paints a black caption area on this setup.
    ribbonBar_->setFrameThemeEnabled(false);
    ribbonBar_->setTitleBarVisible(false);
    ribbonBar_->showQuickAccess(true);
    ribbonBar_->setStyleSheet(QStringLiteral(
        "QAbstractButton {"
        "color: #1f2937;"
        "}"
        "QAbstractButton:checked, QAbstractButton:pressed {"
        "color: #f8fafc;"
        "}"));
    ribbonBar_->installEventFilter(this);
    setRibbonBar(ribbonBar_);
    createWindowControls();

    ribbonBar_->quickAccessBar()->addAction(openAction_);
    ribbonBar_->quickAccessBar()->addAction(fitSceneAction_);
    ribbonBar_->quickAccessBar()->addAction(showAxesAction_);
    ribbonBar_->quickAccessBar()->addAction(measureAction_);

    homePage_ = ribbonBar_->addPage(tr("Home"));
    datasetRibbonGroup_ = homePage_->addGroup(tr("Dataset"));
    datasetRibbonGroup_->addAction(openAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(clearAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(exitAction_, Qt::ToolButtonTextUnderIcon);

    cameraRibbonGroup_ = homePage_->addGroup(tr("Camera"));
    cameraRibbonGroup_->addAction(fitSceneAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(topViewAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(frontViewAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(rightViewAction_, Qt::ToolButtonTextUnderIcon);

    sceneRibbonGroup_ = homePage_->addGroup(tr("Scene Guides"));
    sceneRibbonGroup_->addAction(showAxesAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(showBoundingBoxAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(darkBackgroundAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(lightBackgroundAction_, Qt::ToolButtonTextUnderIcon);

    measureRibbonGroup_ = homePage_->addGroup(tr("Measure"));
    measureRibbonGroup_->addAction(measureAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(clearMeasurementAction_, Qt::ToolButtonTextUnderIcon);

    workspaceRibbonGroup_ = homePage_->addGroup(tr("Workspace"));
    workspaceRibbonGroup_->addAction(showLogAction_, Qt::ToolButtonTextUnderIcon);

    appearancePage_ = ribbonBar_->addPage(tr("Appearance"));
    colorRibbonGroup_ = appearancePage_->addGroup(tr("Point Colors"));
    colorRibbonGroup_->addAction(rgbColorAction_, Qt::ToolButtonTextUnderIcon);
    colorRibbonGroup_->addAction(elevationColorAction_, Qt::ToolButtonTextUnderIcon);
    colorRibbonGroup_->addAction(singleColorAction_, Qt::ToolButtonTextUnderIcon);

    themeRibbonGroup_ = appearancePage_->addGroup(tr("Office Theme"));
    themeRibbonGroup_->addAction(themeColorfulAction_, Qt::ToolButtonTextUnderIcon);
    themeRibbonGroup_->addAction(themeWhiteAction_, Qt::ToolButtonTextUnderIcon);
    themeRibbonGroup_->addAction(themeDarkGrayAction_, Qt::ToolButtonTextUnderIcon);

    languageRibbonGroup_ = appearancePage_->addGroup(tr("Language"));
    languageRibbonGroup_->addAction(languageEnglishAction_, Qt::ToolButtonTextUnderIcon);
    languageRibbonGroup_->addAction(languageChineseAction_, Qt::ToolButtonTextUnderIcon);
}

void MainWindow::createWindowControls()
{
    if (ribbonBar_ == nullptr) {
        return;
    }

    windowControlsWidget_ = new QWidget(ribbonBar_);
    auto* controlsLayout = new QHBoxLayout(windowControlsWidget_);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(0);

    minimizeButton_ = new QToolButton(windowControlsWidget_);
    maximizeButton_ = new QToolButton(windowControlsWidget_);
    closeButton_ = new QToolButton(windowControlsWidget_);

    const QList<QToolButton*> buttons = { minimizeButton_, maximizeButton_, closeButton_ };
    for (QToolButton* button : buttons) {
        button->setAutoRaise(true);
        button->setCursor(Qt::ArrowCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIconSize(QSize(12, 12));
        button->setFixedSize(34, 26);
        controlsLayout->addWidget(button);
    }

    minimizeButton_->setObjectName(QStringLiteral("windowMinimizeButton"));
    maximizeButton_->setObjectName(QStringLiteral("windowMaximizeButton"));
    closeButton_->setObjectName(QStringLiteral("windowCloseButton"));

    connect(minimizeButton_, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(maximizeButton_, &QToolButton::clicked, this, [this]() { toggleMaximizedWindow(); });
    connect(closeButton_, &QToolButton::clicked, this, &QWidget::close);

    ribbonBar_->setCornerWidget(windowControlsWidget_, Qt::TopRightCorner);
    updateWindowControlButtons();
}

void MainWindow::createInspectorPanel()
{
    inspectorDock_ = new QDockWidget(tr("Scene Inspector"), this);
    inspectorDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    inspectorDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    inspectorTabWidget_ = new QTabWidget(inspectorDock_);
    inspectorTabWidget_->setObjectName(QStringLiteral("sceneInspectorTabs"));
    inspectorTabWidget_->setDocumentMode(true);
    inspectorTabWidget_->setMovable(false);

    auto createTabPage = [this](const QString& objectName) {
        auto* scrollArea = new QScrollArea(inspectorTabWidget_);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        auto* page = new QWidget(scrollArea);
        page->setObjectName(objectName);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(14, 14, 14, 14);
        pageLayout->setSpacing(12);
        pageLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
        scrollArea->setWidget(page);
        return qMakePair(scrollArea, pageLayout);
    };

    auto overviewTab = createTabPage(QStringLiteral("sceneInspectorOverviewPage"));
    auto renderingTab = createTabPage(QStringLiteral("sceneInspectorRenderingPage"));
    auto measurementTab = createTabPage(QStringLiteral("sceneInspectorMeasurementPage"));
    auto navigationTab = createTabPage(QStringLiteral("sceneInspectorNavigationPage"));

    datasetGroupBox_ = new QGroupBox(tr("Dataset Summary"), overviewTab.first);
    datasetLayout_ = new QFormLayout(datasetGroupBox_);
    datasetLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    datasetLayout_->setFormAlignment(Qt::AlignTop);

    datasetNameValueLabel_ = new QLabel(datasetGroupBox_);
    datasetPathValueLabel_ = new QLabel(datasetGroupBox_);
    datasetPointsValueLabel_ = new QLabel(datasetGroupBox_);
    datasetBoundsValueLabel_ = new QLabel(datasetGroupBox_);
    datasetExtentValueLabel_ = new QLabel(datasetGroupBox_);
    datasetColorValueLabel_ = new QLabel(datasetGroupBox_);

    const QList<QLabel*> datasetLabels = {
        datasetNameValueLabel_,
        datasetPathValueLabel_,
        datasetPointsValueLabel_,
        datasetBoundsValueLabel_,
        datasetExtentValueLabel_,
        datasetColorValueLabel_
    };
    for (QLabel* label : datasetLabels) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    datasetLayout_->addRow(tr("Name"), datasetNameValueLabel_);
    datasetLayout_->addRow(tr("Path"), datasetPathValueLabel_);
    datasetLayout_->addRow(tr("Points"), datasetPointsValueLabel_);
    datasetLayout_->addRow(tr("Bounds"), datasetBoundsValueLabel_);
    datasetLayout_->addRow(tr("Extent"), datasetExtentValueLabel_);
    datasetLayout_->addRow(tr("Color Source"), datasetColorValueLabel_);

    renderingGroupBox_ = new QGroupBox(tr("Rendering Controls"), renderingTab.first);
    renderingLayout_ = new QFormLayout(renderingGroupBox_);
    renderingLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    renderingLayout_->setFormAlignment(Qt::AlignTop);

    pointSizeSpinBox_ = new QSpinBox(renderingGroupBox_);
    pointSizeSpinBox_->setRange(1, 12);
    pointSizeSpinBox_->setSuffix(tr(" px"));

    colorModeComboBox_ = new QComboBox(renderingGroupBox_);
    colorModeComboBox_->addItem(tr("RGB"));
    colorModeComboBox_->addItem(tr("Elevation Ramp"));
    colorModeComboBox_->addItem(tr("Single Color"));

    pointColorButton_ = new QPushButton(tr("Pick Color"), renderingGroupBox_);
    backgroundColorButton_ = new QPushButton(tr("Pick Background"), renderingGroupBox_);

    axesCheckBox_ = new QCheckBox(tr("Show XYZ axes"), renderingGroupBox_);
    boundingBoxCheckBox_ = new QCheckBox(tr("Show bounding box"), renderingGroupBox_);

    renderingLayout_->addRow(tr("Point Size"), pointSizeSpinBox_);
    renderingLayout_->addRow(tr("Color Mode"), colorModeComboBox_);
    renderingLayout_->addRow(tr("Single Color"), pointColorButton_);
    renderingLayout_->addRow(tr("Background"), backgroundColorButton_);
    renderingLayout_->addRow(QString(), axesCheckBox_);
    renderingLayout_->addRow(QString(), boundingBoxCheckBox_);

    measurementGroupBox_ = new QGroupBox(tr("Measurement"), measurementTab.first);
    measurementLayout_ = new QFormLayout(measurementGroupBox_);
    measurementLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    measurementLayout_->setFormAlignment(Qt::AlignTop);

    measurementToggleButton_ = new QPushButton(tr("Start Measurement"), measurementGroupBox_);
    measurementClearButton_ = new QPushButton(tr("Clear Measurement"), measurementGroupBox_);
    measurementStartValueLabel_ = new QLabel(measurementGroupBox_);
    measurementEndValueLabel_ = new QLabel(measurementGroupBox_);
    measurementDistanceValueLabel_ = new QLabel(measurementGroupBox_);
    measurementDeltaZValueLabel_ = new QLabel(measurementGroupBox_);

    const QList<QLabel*> measurementLabels = {
        measurementStartValueLabel_,
        measurementEndValueLabel_,
        measurementDistanceValueLabel_,
        measurementDeltaZValueLabel_
    };
    for (QLabel* label : measurementLabels) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    measurementLayout_->addRow(QString(), measurementToggleButton_);
    measurementLayout_->addRow(QString(), measurementClearButton_);
    measurementLayout_->addRow(tr("Start Point"), measurementStartValueLabel_);
    measurementLayout_->addRow(tr("End Point"), measurementEndValueLabel_);
    measurementLayout_->addRow(tr("3D Distance"), measurementDistanceValueLabel_);
    measurementLayout_->addRow(tr("Height Delta"), measurementDeltaZValueLabel_);

    navigationGroupBox_ = new QGroupBox(tr("Navigation"), navigationTab.first);
    auto* tipsLayout = new QVBoxLayout(navigationGroupBox_);
    navigationTipsLabel_ = new QLabel(navigationGroupBox_);
    navigationTipsLabel_->setWordWrap(true);
    tipsLayout->addWidget(navigationTipsLabel_);

    auto* navigationToggleContainer = new QWidget(navigationGroupBox_);
    navigationToggleLayout_ = new QFormLayout(navigationToggleContainer);
    navigationToggleLayout_->setContentsMargins(0, 0, 0, 0);
    navigationToggleLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    navigationToggleLayout_->setFormAlignment(Qt::AlignTop);

    invertOrbitCheckBox_ = new QCheckBox(tr("Invert orbit drag"), navigationToggleContainer);
    invertPanCheckBox_ = new QCheckBox(tr("Invert pan drag"), navigationToggleContainer);
    invertWheelCheckBox_ = new QCheckBox(tr("Invert wheel zoom"), navigationToggleContainer);
    navigationToggleLayout_->addRow(QString(), invertOrbitCheckBox_);
    navigationToggleLayout_->addRow(QString(), invertPanCheckBox_);
    navigationToggleLayout_->addRow(QString(), invertWheelCheckBox_);
    tipsLayout->addWidget(navigationToggleContainer);

    overviewTab.second->addWidget(datasetGroupBox_);
    overviewTab.second->addStretch(1);
    renderingTab.second->addWidget(renderingGroupBox_);
    renderingTab.second->addStretch(1);
    measurementTab.second->addWidget(measurementGroupBox_);
    measurementTab.second->addStretch(1);
    navigationTab.second->addWidget(navigationGroupBox_);
    navigationTab.second->addStretch(1);

    inspectorTabWidget_->addTab(overviewTab.first, QString());
    inspectorTabWidget_->addTab(renderingTab.first, QString());
    inspectorTabWidget_->addTab(measurementTab.first, QString());
    inspectorTabWidget_->addTab(navigationTab.first, QString());

    inspectorTabWidget_->setStyleSheet(
        "QWidget {"
        "background-color: #f6f8fb;"
        "color: #1f2937;"
        "}"
        "QTabWidget::pane {"
        "border: 1px solid #d6dde8;"
        "border-radius: 10px;"
        "top: -1px;"
        "}"
        "QTabBar::tab {"
        "background-color: #eef2f7;"
        "border: 1px solid #d6dde8;"
        "border-bottom: none;"
        "border-top-left-radius: 8px;"
        "border-top-right-radius: 8px;"
        "padding: 8px 14px;"
        "margin-right: 4px;"
        "color: #475569;"
        "font-weight: 600;"
        "}"
        "QTabBar::tab:selected {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "background-color: #e2e8f0;"
        "}"
        "QLabel {"
        "color: #1f2937;"
        "}"
        "QGroupBox {"
        "font-weight: 600;"
        "background-color: #ffffff;"
        "border: 1px solid #d6dde8;"
        "border-radius: 8px;"
        "margin-top: 8px;"
        "padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 8px;"
        "padding: 0 4px;"
        "color: #334155;"
        "background-color: #f6f8fb;"
        "}"
        "QPushButton, QComboBox, QSpinBox {"
        "background-color: #ffffff;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "min-height: 32px;"
        "padding: 4px 10px;"
        "color: #111827;"
        "}"
        "QComboBox {"
        "padding-right: 30px;"
        "}"
        "QSpinBox {"
        "padding-right: 20px;"
        "}"
        "QComboBox::drop-down {"
        "subcontrol-origin: padding;"
        "subcontrol-position: top right;"
        "width: 24px;"
        "border: none;"
        "}"
        "QSpinBox::up-button, QSpinBox::down-button {"
        "width: 18px;"
        "border: none;"
        "}"
        "QComboBox QAbstractItemView {"
        "padding: 4px 0;"
        "selection-background-color: #dbeafe;"
        "selection-color: #111827;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "min-height: 24px;"
        "padding: 4px 10px;"
        "}"
        "QPushButton:hover, QComboBox:hover, QSpinBox:hover {"
        "border-color: #94a3b8;"
        "}"
        "QCheckBox {"
        "color: #1f2937;"
        "}");

    inspectorDock_->setWidget(inspectorTabWidget_);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);
}

void MainWindow::createLogDock()
{
    logDock_ = new QDockWidget(tr("Application Log"), this);
    logDock_->setObjectName(QStringLiteral("applicationLogDock"));
    logDock_->setAllowedAreas(
        Qt::BottomDockWidgetArea
        | Qt::TopDockWidgetArea);
    logDock_->setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    logDock_->setMinimumHeight(140);
    logDock_->setMaximumHeight(260);

    logTextEdit_ = new QPlainTextEdit(logDock_);
    logTextEdit_->setReadOnly(true);
    logTextEdit_->setMaximumBlockCount(500);
    logTextEdit_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    logTextEdit_->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "background-color: #0f172a;"
        "color: #dbe4f0;"
        "border: none;"
        "font-family: Consolas, 'Courier New', monospace;"
        "font-size: 11px;"
        "}"));

    logDock_->setWidget(logTextEdit_);
    addDockWidget(Qt::BottomDockWidgetArea, logDock_);
    logDock_->hide();
}

void MainWindow::createStatusBar()
{
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet(QStringLiteral(
        "QStatusBar {"
        "background-color: #eef2f7;"
        "color: #334155;"
        "border-top: 1px solid #d6dde8;"
        "}"));
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("LAS Point Cloud Viewer"));

    openAction_->setText(tr("Open"));
    openAction_->setToolTip(tr("Open a LAS or LAZ dataset"));
    clearAction_->setText(tr("Clear"));
    clearAction_->setToolTip(tr("Clear the current scene"));
    exitAction_->setText(tr("Exit"));
    fitSceneAction_->setText(tr("Fit Scene"));
    fitSceneAction_->setToolTip(tr("Reset to a fitted isometric view"));
    topViewAction_->setText(tr("Top"));
    frontViewAction_->setText(tr("Front"));
    rightViewAction_->setText(tr("Right"));
    showAxesAction_->setText(tr("Axes"));
    showBoundingBoxAction_->setText(tr("Bounds"));
    darkBackgroundAction_->setText(tr("Dark"));
    lightBackgroundAction_->setText(tr("Light"));
    rgbColorAction_->setText(tr("RGB"));
    elevationColorAction_->setText(tr("Elevation"));
    singleColorAction_->setText(tr("Single"));
    themeColorfulAction_->setText(tr("Colorful"));
    themeWhiteAction_->setText(tr("White"));
    themeDarkGrayAction_->setText(tr("Dark Gray"));
    measureAction_->setText(tr("Measure"));
    clearMeasurementAction_->setText(tr("Clear Measure"));
    showLogAction_->setText(tr("Log"));
    showLogAction_->setToolTip(tr("Show or hide the log panel"));
    languageEnglishAction_->setText(QStringLiteral("English"));
    languageChineseAction_->setText(QStringLiteral("\u4e2d\u6587"));

    if (homePage_ != nullptr) {
        homePage_->setTitle(tr("Home"));
    }
    if (appearancePage_ != nullptr) {
        appearancePage_->setTitle(tr("Appearance"));
    }
    if (datasetRibbonGroup_ != nullptr) {
        datasetRibbonGroup_->setTitle(tr("Dataset"));
    }
    if (cameraRibbonGroup_ != nullptr) {
        cameraRibbonGroup_->setTitle(tr("Camera"));
    }
    if (sceneRibbonGroup_ != nullptr) {
        sceneRibbonGroup_->setTitle(tr("Scene Guides"));
    }
    if (measureRibbonGroup_ != nullptr) {
        measureRibbonGroup_->setTitle(tr("Measure"));
    }
    if (workspaceRibbonGroup_ != nullptr) {
        workspaceRibbonGroup_->setTitle(tr("Workspace"));
    }
    if (colorRibbonGroup_ != nullptr) {
        colorRibbonGroup_->setTitle(tr("Point Colors"));
    }
    if (themeRibbonGroup_ != nullptr) {
        themeRibbonGroup_->setTitle(tr("Office Theme"));
    }
    if (languageRibbonGroup_ != nullptr) {
        languageRibbonGroup_->setTitle(tr("Language"));
    }

    if (inspectorDock_ != nullptr) {
        inspectorDock_->setWindowTitle(tr("Scene Inspector"));
    }
    if (logDock_ != nullptr) {
        logDock_->setWindowTitle(tr("Application Log"));
    }
    if (inspectorTabWidget_ != nullptr) {
        inspectorTabWidget_->setTabText(0, tr("Overview"));
        inspectorTabWidget_->setTabText(1, tr("Rendering"));
        inspectorTabWidget_->setTabText(2, tr("Measurement"));
        inspectorTabWidget_->setTabText(3, tr("Navigation"));
    }
    if (datasetGroupBox_ != nullptr) {
        datasetGroupBox_->setTitle(tr("Dataset Summary"));
    }
    if (renderingGroupBox_ != nullptr) {
        renderingGroupBox_->setTitle(tr("Rendering Controls"));
    }
    if (measurementGroupBox_ != nullptr) {
        measurementGroupBox_->setTitle(tr("Measurement"));
    }
    if (navigationGroupBox_ != nullptr) {
        navigationGroupBox_->setTitle(tr("Navigation"));
    }

    auto setFieldLabel = [](QFormLayout* layout, QWidget* field, const QString& text) {
        if (layout == nullptr || field == nullptr) {
            return;
        }
        if (auto* label = qobject_cast<QLabel*>(layout->labelForField(field))) {
            label->setText(text);
        }
    };

    setFieldLabel(datasetLayout_, datasetNameValueLabel_, tr("Name"));
    setFieldLabel(datasetLayout_, datasetPathValueLabel_, tr("Path"));
    setFieldLabel(datasetLayout_, datasetPointsValueLabel_, tr("Points"));
    setFieldLabel(datasetLayout_, datasetBoundsValueLabel_, tr("Bounds"));
    setFieldLabel(datasetLayout_, datasetExtentValueLabel_, tr("Extent"));
    setFieldLabel(datasetLayout_, datasetColorValueLabel_, tr("Color Source"));

    setFieldLabel(renderingLayout_, pointSizeSpinBox_, tr("Point Size"));
    setFieldLabel(renderingLayout_, colorModeComboBox_, tr("Color Mode"));
    setFieldLabel(renderingLayout_, pointColorButton_, tr("Single Color"));
    setFieldLabel(renderingLayout_, backgroundColorButton_, tr("Background"));
    pointSizeSpinBox_->setSuffix(tr(" px"));
    colorModeComboBox_->setItemText(0, tr("RGB"));
    colorModeComboBox_->setItemText(1, tr("Elevation Ramp"));
    colorModeComboBox_->setItemText(2, tr("Single Color"));
    axesCheckBox_->setText(tr("Show XYZ axes"));
    boundingBoxCheckBox_->setText(tr("Show bounding box"));

    setFieldLabel(measurementLayout_, measurementStartValueLabel_, tr("Start Point"));
    setFieldLabel(measurementLayout_, measurementEndValueLabel_, tr("End Point"));
    setFieldLabel(measurementLayout_, measurementDistanceValueLabel_, tr("3D Distance"));
    setFieldLabel(measurementLayout_, measurementDeltaZValueLabel_, tr("Height Delta"));

    invertOrbitCheckBox_->setText(tr("Invert orbit drag"));
    invertPanCheckBox_->setText(tr("Invert pan drag"));
    invertWheelCheckBox_->setText(tr("Invert wheel zoom"));

    if (viewer_ != nullptr) {
        setColorButtonAppearance(pointColorButton_, viewer_->visualizationOptions().singleColor, tr("Pick Color"));
        setColorButtonAppearance(backgroundColorButton_, viewer_->visualizationOptions().backgroundColor, tr("Pick Background"));
    }
    updateWindowControlButtons();
    updateDatasetPanel();
    updateNavigationHelpText();
    updateMeasurementPanel();
    updateActionState();
    if (viewer_ != nullptr) {
        viewer_->update();
    }
}

void MainWindow::createConnections()
{
    connect(openAction_, &QAction::triggered, this, [this]() { openPointCloud(); });
    connect(clearAction_, &QAction::triggered, this, [this]() { clearPointCloud(); });
    connect(exitAction_, &QAction::triggered, this, &QWidget::close);

    connect(fitSceneAction_, &QAction::triggered, viewer_, &PointCloudViewer::resetView);
    connect(topViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Top); });
    connect(frontViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Front); });
    connect(rightViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Right); });

    connect(showAxesAction_, &QAction::toggled, viewer_, &PointCloudViewer::setShowAxes);
    connect(showBoundingBoxAction_, &QAction::toggled, viewer_, &PointCloudViewer::setShowBoundingBox);
    connect(darkBackgroundAction_, &QAction::triggered, this, [this]() { viewer_->setBackgroundColor(kDarkBackground); });
    connect(lightBackgroundAction_, &QAction::triggered, this, [this]() { viewer_->setBackgroundColor(kLightBackground); });

    connect(rgbColorAction_, &QAction::triggered, this, [this]() { viewer_->setColorMode(PointCloudColorMode::Rgb); });
    connect(elevationColorAction_, &QAction::triggered, this, [this]() { viewer_->setColorMode(PointCloudColorMode::Elevation); });
    connect(singleColorAction_, &QAction::triggered, this, [this]() { viewer_->setColorMode(PointCloudColorMode::SingleColor); });

    connect(themeColorfulAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016Colorful); });
    connect(themeWhiteAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016White); });
    connect(themeDarkGrayAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016DarkGray); });
    connect(measureAction_, &QAction::toggled, viewer_, &PointCloudViewer::setMeasurementEnabled);
    connect(clearMeasurementAction_, &QAction::triggered, viewer_, &PointCloudViewer::clearMeasurement);

    connect(pointSizeSpinBox_, qOverload<int>(&QSpinBox::valueChanged), viewer_, &PointCloudViewer::setPointSize);
    connect(
        colorModeComboBox_,
        qOverload<int>(&QComboBox::currentIndexChanged),
        viewer_,
        static_cast<void (PointCloudViewer::*)(int)>(&PointCloudViewer::setColorMode));
    connect(pointColorButton_, &QPushButton::clicked, this, [this]() { choosePointColor(); });
    connect(backgroundColorButton_, &QPushButton::clicked, this, [this]() { chooseBackgroundColor(); });
    connect(axesCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setShowAxes);
    connect(boundingBoxCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setShowBoundingBox);
    connect(invertOrbitCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertOrbitDrag);
    connect(invertPanCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertPanDrag);
    connect(invertWheelCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertWheelZoom);
    connect(measurementToggleButton_, &QPushButton::clicked, this, [this]() {
        viewer_->setMeasurementEnabled(!viewer_->measurementEnabled());
    });
    connect(measurementClearButton_, &QPushButton::clicked, viewer_, &PointCloudViewer::clearMeasurement);
    connect(languageEnglishAction_, &QAction::triggered, this, [this]() { applyLanguage(UiLanguage::English); });
    connect(languageChineseAction_, &QAction::triggered, this, [this]() { applyLanguage(UiLanguage::Chinese); });

    connect(showLogAction_, &QAction::toggled, this, [this](bool visible) {
        if (logDock_ != nullptr) {
            if (visible) {
                logDock_->show();
                resizeDocks({logDock_}, {190}, Qt::Vertical);
            } else {
                logDock_->hide();
            }
            persistWindowSettings();
        }
    });
    connect(logDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (showLogAction_ != nullptr && showLogAction_->isChecked() != visible) {
            showLogAction_->setChecked(visible);
        }
        persistWindowSettings();
    });
    if (inspectorTabWidget_ != nullptr) {
        connect(inspectorTabWidget_, &QTabWidget::currentChanged, this, [this](int) {
            persistWindowSettings();
        });
    }

    connect(viewer_, &PointCloudViewer::pointCloudLoaded, this, [this]() {
        syncUiFromViewer();
    });
    connect(viewer_, &PointCloudViewer::pointCloudCleared, this, [this]() {
        syncUiFromViewer();
        showUserMessage(LogLevel::Info, tr("Scene cleared."), 3000);
    });
    connect(viewer_, &PointCloudViewer::visualizationOptionsChanged, this, [this]() { syncUiFromViewer(); });
    connect(viewer_, &PointCloudViewer::visualizationOptionsChanged, this, [this]() { persistVisualizationSettings(); });
    connect(viewer_, &PointCloudViewer::interactionOptionsChanged, this, [this]() {
        persistInteractionSettings();
        syncUiFromViewer();
        updateNavigationHelpText();
        showUserMessage(LogLevel::Info, tr("Navigation preferences updated."), 2500);
    });
    connect(viewer_, &PointCloudViewer::measurementChanged, this, [this]() {
        syncUiFromViewer();
        updateMeasurementPanel();
    });
    connect(viewer_, &PointCloudViewer::measurementModeChanged, this, [this]() {
        syncUiFromViewer();
        updateMeasurementPanel();
    });
    connect(viewer_, &PointCloudViewer::measurementMessage, this, [this](const QString& message, bool error) {
        showUserMessage(error ? LogLevel::Error : LogLevel::Info, message, error ? 4000 : 3000);
    });
}

void MainWindow::openPointCloud()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open LAS Point Cloud"),
        QString(),
        tr("LAS Files (*.las *.laz);;All Files (*.*)"));

    if (filePath.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Open cancelled."), 2000);
        return;
    }

    loadPointCloudFile(filePath);
}

bool MainWindow::loadPointCloudFile(const QString& filePath)
{
    QString errorMessage;
    if (viewer_->loadPointCloud(filePath, &errorMessage)) {
        const QFileInfo fileInfo(filePath);
        showUserMessage(
            LogLevel::Info,
            tr("Loaded %1. %2").arg(fileInfo.fileName(), errorMessage),
            4000);
        return true;
    }

    syncUiFromViewer();
    showUserMessage(
        LogLevel::Error,
        errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
        6000);
    return false;
}

void MainWindow::clearPointCloud()
{
    viewer_->clearPointCloud();
}

void MainWindow::choosePointColor()
{
    const QColor initialColor = viewer_->visualizationOptions().singleColor;
    const QColor chosenColor = QColorDialog::getColor(initialColor, this, tr("Choose Single Point Color"));
    if (chosenColor.isValid()) {
        viewer_->setSingleColor(chosenColor);
        if (viewer_->visualizationOptions().colorMode != PointCloudColorMode::SingleColor) {
            viewer_->setColorMode(PointCloudColorMode::SingleColor);
        }
    }
}

void MainWindow::chooseBackgroundColor()
{
    const QColor initialColor = viewer_->visualizationOptions().backgroundColor;
    const QColor chosenColor = QColorDialog::getColor(initialColor, this, tr("Choose Background Color"));
    if (chosenColor.isValid()) {
        viewer_->setBackgroundColor(chosenColor);
    }
}

void MainWindow::applyOfficeTheme(Qtitan::RibbonStyle::Theme theme)
{
    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        ribbonStyle->setTheme(theme);
        updateWindowChromePalette(theme);
        persistThemeSettings();
        syncUiFromViewer();
        showUserMessage(LogLevel::Info, tr("Theme updated."), 2500);
    }
}

void MainWindow::updateWindowChromePalette(Qtitan::RibbonStyle::Theme theme)
{
    const bool useDarkChrome = theme == Qtitan::RibbonStyle::Office2016DarkGray;
    const QColor frameColor = useDarkChrome ? kWindowChromeDark : kWindowChromeLight;
    const QColor textColor = useDarkChrome ? QColor(241, 245, 249) : QColor(31, 41, 55);

    QPalette windowPalette = palette();
    windowPalette.setColor(QPalette::Window, frameColor);
    windowPalette.setColor(QPalette::WindowText, textColor);
    setPalette(windowPalette);
    setBackgroundRole(QPalette::Window);

    if (ribbonBar_ != nullptr) {
        QPalette ribbonPalette = ribbonBar_->palette();
        ribbonPalette.setColor(QPalette::Window, frameColor);
        ribbonPalette.setColor(QPalette::WindowText, textColor);
        ribbonBar_->setPalette(ribbonPalette);
        ribbonBar_->setBackgroundRole(QPalette::Window);
        ribbonBar_->setAutoFillBackground(true);

        const QList<Qtitan::RibbonPage*> pages = ribbonBar_->pages();
        for (Qtitan::RibbonPage* page : pages) {
            if (page == nullptr) {
                continue;
            }

            QPalette pagePalette = page->palette();
            pagePalette.setColor(QPalette::Window, frameColor);
            pagePalette.setColor(QPalette::WindowText, textColor);
            page->setPalette(pagePalette);
            page->setBackgroundRole(QPalette::Window);
            page->setAutoFillBackground(true);
            page->update();
        }

        ribbonBar_->update();
    }

    updateWindowControlAppearance(theme);
    updateWindowControlButtons();
    update();
}

void MainWindow::updateWindowControlButtons()
{
    if (minimizeButton_ == nullptr || maximizeButton_ == nullptr || closeButton_ == nullptr) {
        return;
    }

    Qtitan::RibbonStyle::Theme theme = Qtitan::RibbonStyle::Office2016White;
    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        theme = ribbonStyle->getTheme();
    }

    const bool useDarkChrome = theme == Qtitan::RibbonStyle::Office2016DarkGray;
    const QColor iconColor = useDarkChrome ? QColor(241, 245, 249) : QColor(31, 41, 55);

    minimizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Minimize, iconColor));
    closeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Close, iconColor));

    if (isMaximized()) {
        maximizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Restore, iconColor));
        maximizeButton_->setToolTip(tr("Restore Down"));
    } else {
        maximizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Maximize, iconColor));
        maximizeButton_->setToolTip(tr("Maximize"));
    }

    minimizeButton_->setToolTip(tr("Minimize"));
    closeButton_->setToolTip(tr("Close"));
}

void MainWindow::updateWindowControlAppearance(Qtitan::RibbonStyle::Theme theme)
{
    if (windowControlsWidget_ == nullptr) {
        return;
    }

    const bool useDarkChrome = theme == Qtitan::RibbonStyle::Office2016DarkGray;
    const QString hoverColor = useDarkChrome ? QStringLiteral("rgba(255, 255, 255, 0.12)") : QStringLiteral("rgba(15, 23, 42, 0.08)");
    const QString pressedColor = useDarkChrome ? QStringLiteral("rgba(255, 255, 255, 0.18)") : QStringLiteral("rgba(15, 23, 42, 0.14)");
    windowControlsWidget_->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "border: none;"
        "background: transparent;"
        "padding: 0;"
        "}"
        "QToolButton:hover {"
        "background: %1;"
        "}"
        "QToolButton:pressed {"
        "background: %2;"
        "}"
        "QToolButton#windowCloseButton:hover {"
        "background: #e11d48;"
        "}"
        "QToolButton#windowCloseButton:pressed {"
        "background: #be123c;"
        "}").arg(hoverColor, pressedColor));
}

void MainWindow::toggleMaximizedWindow()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
    updateWindowControlButtons();
}

bool MainWindow::isDraggableRibbonArea(const QPoint& position) const
{
    if (ribbonBar_ == nullptr || !ribbonBar_->rect().contains(position)) {
        return false;
    }

    QWidget* child = ribbonBar_->childAt(position);
    return !isInteractiveRibbonWidget(child);
}

bool MainWindow::isInteractiveRibbonWidget(const QWidget* widget) const
{
    const QWidget* current = widget;
    while (current != nullptr) {
        if (current == ribbonBar_) {
            return false;
        }
        if (isWindowControlWidget(current)) {
            return true;
        }
        const QString className = QString::fromLatin1(current->metaObject()->className());
        if (qobject_cast<const QAbstractButton*>(current) != nullptr
            || qobject_cast<const QComboBox*>(current) != nullptr
            || qobject_cast<const QAbstractSpinBox*>(current) != nullptr
            || qobject_cast<const QTabBar*>(current) != nullptr
            || className.contains(QStringLiteral("RibbonTab"), Qt::CaseInsensitive)
            || className.contains(QStringLiteral("RibbonPage"), Qt::CaseInsensitive)
            || className.contains(QStringLiteral("QuickAccess"), Qt::CaseInsensitive)
            || className.contains(QStringLiteral("SystemButton"), Qt::CaseInsensitive)) {
            return true;
        }
        current = current->parentWidget();
    }

    return false;
}

bool MainWindow::isWindowControlWidget(const QWidget* widget) const
{
    const QWidget* current = widget;
    while (current != nullptr) {
        if (current == windowControlsWidget_) {
            return true;
        }
        current = current->parentWidget();
    }
    return false;
}

void MainWindow::syncUiFromViewer()
{
    const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();
    const InteractionOptions& interactionOptions = viewer_->interactionOptions();

    {
        const QSignalBlocker pointSizeBlocker(pointSizeSpinBox_);
        const QSignalBlocker colorModeBlocker(colorModeComboBox_);
        const QSignalBlocker axesBlocker(axesCheckBox_);
        const QSignalBlocker boundsBlocker(boundingBoxCheckBox_);
        const QSignalBlocker orbitBlocker(invertOrbitCheckBox_);
        const QSignalBlocker panBlocker(invertPanCheckBox_);
        const QSignalBlocker wheelBlocker(invertWheelCheckBox_);
        const QSignalBlocker axesActionBlocker(showAxesAction_);
        const QSignalBlocker boundsActionBlocker(showBoundingBoxAction_);
        const QSignalBlocker rgbActionBlocker(rgbColorAction_);
        const QSignalBlocker elevationActionBlocker(elevationColorAction_);
        const QSignalBlocker singleActionBlocker(singleColorAction_);
        const QSignalBlocker colorfulThemeBlocker(themeColorfulAction_);
        const QSignalBlocker whiteThemeBlocker(themeWhiteAction_);
        const QSignalBlocker darkThemeBlocker(themeDarkGrayAction_);
        const QSignalBlocker measurementActionBlocker(measureAction_);
        const QSignalBlocker englishLanguageBlocker(languageEnglishAction_);
        const QSignalBlocker chineseLanguageBlocker(languageChineseAction_);

        pointSizeSpinBox_->setValue(static_cast<int>(options.pointSize));
        colorModeComboBox_->setCurrentIndex(static_cast<int>(options.colorMode));
        axesCheckBox_->setChecked(options.showAxes);
        boundingBoxCheckBox_->setChecked(options.showBoundingBox);
        invertOrbitCheckBox_->setChecked(interactionOptions.invertOrbitDrag);
        invertPanCheckBox_->setChecked(interactionOptions.invertPanDrag);
        invertWheelCheckBox_->setChecked(interactionOptions.invertWheelZoom);

        showAxesAction_->setChecked(options.showAxes);
        showBoundingBoxAction_->setChecked(options.showBoundingBox);
        rgbColorAction_->setChecked(options.colorMode == PointCloudColorMode::Rgb);
        elevationColorAction_->setChecked(options.colorMode == PointCloudColorMode::Elevation);
        singleColorAction_->setChecked(options.colorMode == PointCloudColorMode::SingleColor);
        measureAction_->setChecked(viewer_->measurementEnabled());
        languageEnglishAction_->setChecked(currentLanguage_ == UiLanguage::English);
        languageChineseAction_->setChecked(currentLanguage_ == UiLanguage::Chinese);

        if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
            themeColorfulAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016Colorful);
            themeWhiteAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016White);
            themeDarkGrayAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016DarkGray);
        }
    }

    setColorButtonAppearance(pointColorButton_, options.singleColor, tr("Pick Color"));
    setColorButtonAppearance(backgroundColorButton_, options.backgroundColor, tr("Pick Background"));
    updateNavigationHelpText();
    updateDatasetPanel();
    updateMeasurementPanel();
    updateActionState();
}

void MainWindow::updateDatasetPanel()
{
    const PointCloudData* pointCloudData = viewer_->pointCloudData();
    if (pointCloudData == nullptr) {
        datasetNameValueLabel_->setText(tr("No dataset loaded"));
        datasetPathValueLabel_->setText(tr("Open or drag a LAS/LAZ file into the window."));
        datasetPointsValueLabel_->setText(QStringLiteral("0"));
        datasetBoundsValueLabel_->setText(tr("N/A"));
        datasetExtentValueLabel_->setText(tr("N/A"));
        datasetColorValueLabel_->setText(colorModeName(viewer_->visualizationOptions().colorMode));
        return;
    }

    const QFileInfo fileInfo(viewer_->currentFilePath());
    const PointRecord& minBounds = pointCloudData->minBounds();
    const PointRecord& maxBounds = pointCloudData->maxBounds();

    datasetNameValueLabel_->setText(fileInfo.fileName());
    datasetPathValueLabel_->setText(viewer_->currentFilePath());
    datasetPointsValueLabel_->setText(QLocale().toString(static_cast<qlonglong>(pointCloudData->size())));
    datasetBoundsValueLabel_->setText(
        tr("Min (%1)\nMax (%2)")
            .arg(formatTriplet(minBounds.x, minBounds.y, minBounds.z))
            .arg(formatTriplet(maxBounds.x, maxBounds.y, maxBounds.z)));
    datasetExtentValueLabel_->setText(formatTriplet(
        maxBounds.x - minBounds.x,
        maxBounds.y - minBounds.y,
        maxBounds.z - minBounds.z));
    datasetColorValueLabel_->setText(
        tr("%1 | Native RGB: %2")
            .arg(colorModeName(viewer_->visualizationOptions().colorMode))
            .arg(pointCloudData->hasColor() ? tr("yes") : tr("no")));
}

void MainWindow::updateActionState()
{
    const bool hasPointCloud = viewer_->hasPointCloud();
    clearAction_->setEnabled(hasPointCloud);
    fitSceneAction_->setEnabled(hasPointCloud);
    topViewAction_->setEnabled(hasPointCloud);
    frontViewAction_->setEnabled(hasPointCloud);
    rightViewAction_->setEnabled(hasPointCloud);
    measureAction_->setEnabled(hasPointCloud);
    clearMeasurementAction_->setEnabled(hasPointCloud && viewer_->measurementResult().hasStartPoint);
    measurementToggleButton_->setEnabled(hasPointCloud);
    measurementClearButton_->setEnabled(hasPointCloud && viewer_->measurementResult().hasStartPoint);
}

void MainWindow::setColorButtonAppearance(QPushButton* button, const QColor& color, const QString& fallbackText) const
{
    if (button == nullptr) {
        return;
    }

    const int luminance = (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
    const QString foreground = luminance >= 150 ? QStringLiteral("#111827") : QStringLiteral("#f8fafc");

    button->setText(fallbackText);
    button->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "background-color: %1;"
        "color: %2;"
        "border: 1px solid rgba(15, 23, 42, 0.14);"
        "border-radius: 4px;"
        "padding: 6px 10px;"
        "}").arg(color.name(), foreground));
}

void MainWindow::appendLog(LogLevel level, const QString& message)
{
    if (logTextEdit_ == nullptr || message.trimmed().isEmpty()) {
        return;
    }

    QString levelText;
    switch (level) {
    case LogLevel::Warning:
        levelText = QStringLiteral("WARN");
        break;
    case LogLevel::Error:
        levelText = QStringLiteral("ERROR");
        break;
    case LogLevel::Info:
    default:
        levelText = QStringLiteral("INFO");
        break;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    logTextEdit_->appendPlainText(
        QStringLiteral("[%1] %2 %3")
            .arg(timestamp)
            .arg(levelText.leftJustified(5, QLatin1Char(' ')))
            .arg(message));
    logTextEdit_->verticalScrollBar()->setValue(logTextEdit_->verticalScrollBar()->maximum());
}

void MainWindow::showUserMessage(LogLevel level, const QString& message, int timeoutMs)
{
    if (statusBar() != nullptr) {
        statusBar()->showMessage(message, timeoutMs);
    }
    appendLog(level, message);
}

void MainWindow::loadInteractionSettings()
{
    QSettings settings;
    InteractionOptions options;
    options.invertOrbitDrag = settings.value(QStringLiteral("interaction/invertOrbitDrag"), false).toBool();
    options.invertPanDrag = settings.value(QStringLiteral("interaction/invertPanDrag"), false).toBool();
    options.invertWheelZoom = settings.value(QStringLiteral("interaction/invertWheelZoom"), false).toBool();
    viewer_->setInteractionOptions(options);
}

void MainWindow::persistInteractionSettings() const
{
    QSettings settings;
    const InteractionOptions& options = viewer_->interactionOptions();
    settings.setValue(QStringLiteral("interaction/invertOrbitDrag"), options.invertOrbitDrag);
    settings.setValue(QStringLiteral("interaction/invertPanDrag"), options.invertPanDrag);
    settings.setValue(QStringLiteral("interaction/invertWheelZoom"), options.invertWheelZoom);
}

void MainWindow::loadVisualizationSettings()
{
    if (viewer_ == nullptr) {
        return;
    }

    QSettings settings;
    const PointCloudVisualizationOptions defaults = viewer_->visualizationOptions();
    viewer_->setPointSize(settings.value(QStringLiteral("visualization/pointSize"), defaults.pointSize).toInt());
    viewer_->setColorMode(settings.value(QStringLiteral("visualization/colorMode"), static_cast<int>(defaults.colorMode)).toInt());
    viewer_->setSingleColor(settings.value(QStringLiteral("visualization/singleColor"), defaults.singleColor).value<QColor>());
    viewer_->setBackgroundColor(settings.value(QStringLiteral("visualization/backgroundColor"), defaults.backgroundColor).value<QColor>());
    viewer_->setShowAxes(settings.value(QStringLiteral("visualization/showAxes"), defaults.showAxes).toBool());
    viewer_->setShowBoundingBox(settings.value(QStringLiteral("visualization/showBoundingBox"), defaults.showBoundingBox).toBool());
}

void MainWindow::persistVisualizationSettings() const
{
    if (viewer_ == nullptr) {
        return;
    }

    QSettings settings;
    const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();
    settings.setValue(QStringLiteral("visualization/pointSize"), options.pointSize);
    settings.setValue(QStringLiteral("visualization/colorMode"), static_cast<int>(options.colorMode));
    settings.setValue(QStringLiteral("visualization/singleColor"), options.singleColor);
    settings.setValue(QStringLiteral("visualization/backgroundColor"), options.backgroundColor);
    settings.setValue(QStringLiteral("visualization/showAxes"), options.showAxes);
    settings.setValue(QStringLiteral("visualization/showBoundingBox"), options.showBoundingBox);
}

void MainWindow::loadLanguageSettings()
{
    QSettings settings;
    const QString storedLanguage = settings.value(QStringLiteral("ui/language")).toString();
    if (storedLanguage == QStringLiteral("zh_CN")) {
        currentLanguage_ = UiLanguage::Chinese;
    } else if (storedLanguage == QStringLiteral("en")) {
        currentLanguage_ = UiLanguage::English;
    } else {
        currentLanguage_ = defaultLanguageFromLocale();
    }
}

void MainWindow::persistLanguageSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/language"), languageCodeFor(currentLanguage_));
}

void MainWindow::loadWindowSettings()
{
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    if (settings.value(QStringLiteral("window/maximized"), false).toBool()) {
        showMaximized();
    }

    const bool showLog = settings.value(QStringLiteral("window/showLog"), false).toBool();
    if (inspectorTabWidget_ != nullptr) {
        inspectorTabWidget_->setCurrentIndex(settings.value(QStringLiteral("window/inspectorTab"), 0).toInt());
    }
    if (showLogAction_ != nullptr) {
        showLogAction_->setChecked(showLog);
    }
    if (logDock_ != nullptr) {
        logDock_->setVisible(showLog);
    }
}

void MainWindow::persistWindowSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/maximized"), isMaximized());
    settings.setValue(QStringLiteral("window/showLog"), logDock_ != nullptr && logDock_->isVisible());
    settings.setValue(
        QStringLiteral("window/inspectorTab"),
        inspectorTabWidget_ != nullptr ? inspectorTabWidget_->currentIndex() : 0);
}

void MainWindow::loadThemeSettings()
{
    const int storedTheme = QSettings().value(
        QStringLiteral("ui/theme"),
        static_cast<int>(Qtitan::RibbonStyle::Office2016Colorful)).toInt();

    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        const auto theme = static_cast<Qtitan::RibbonStyle::Theme>(storedTheme);
        ribbonStyle->setTheme(theme);
        updateWindowChromePalette(theme);
    }
}

void MainWindow::persistThemeSettings() const
{
    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        QSettings().setValue(QStringLiteral("ui/theme"), static_cast<int>(ribbonStyle->getTheme()));
    }
}

void MainWindow::updateNavigationHelpText()
{
    if (navigationTipsLabel_ == nullptr || viewer_ == nullptr) {
        return;
    }

    const InteractionOptions& options = viewer_->interactionOptions();
    navigationTipsLabel_->setText(tr(
        "Left drag orbits (%1), middle or right drag pans (%2), and the mouse wheel zooms (%3). "
        "Use the toggles below to match your preferred interaction direction.")
        .arg(options.invertOrbitDrag ? tr("inverted") : tr("normal"))
        .arg(options.invertPanDrag ? tr("inverted") : tr("normal"))
        .arg(options.invertWheelZoom ? tr("inverted") : tr("normal")));
}

void MainWindow::updateMeasurementPanel()
{
    if (viewer_ == nullptr || measurementToggleButton_ == nullptr || measurementClearButton_ == nullptr) {
        return;
    }

    const MeasurementResult& measurementResult = viewer_->measurementResult();
    measurementToggleButton_->setText(
        viewer_->measurementEnabled() ? tr("Stop Measurement") : tr("Start Measurement"));
    measurementClearButton_->setText(tr("Clear Measurement"));

    measurementStartValueLabel_->setText(measurementPointText(measurementResult, true));
    measurementEndValueLabel_->setText(measurementPointText(measurementResult, false));
    measurementDistanceValueLabel_->setText(
        measurementResult.isComplete() ? formatCoordinate(measurementResult.distance3d) : tr("N/A"));
    measurementDeltaZValueLabel_->setText(
        measurementResult.isComplete() ? formatCoordinate(measurementResult.deltaZ) : tr("N/A"));
}

void MainWindow::applyLanguage(UiLanguage language)
{
    currentLanguage_ = language;

    if (appTranslator_ != nullptr) {
        qApp->removeTranslator(appTranslator_);
        if (language == UiLanguage::Chinese) {
            const QString translationDir = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
            appTranslator_->load(QStringLiteral("lasviewer_zh_CN"), translationDir);
            qApp->installTranslator(appTranslator_);
        }
    }

    if (qtTranslator_ != nullptr) {
        qApp->removeTranslator(qtTranslator_);
        if (language == UiLanguage::Chinese) {
            const QString deployedTranslationDir = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
            const QString qtTranslationName = QStringLiteral("qt_zh_CN");
            if (qtTranslator_->load(qtTranslationName, deployedTranslationDir)
                || qtTranslator_->load(qtTranslationName, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
                qApp->installTranslator(qtTranslator_);
            }
        }
    }

    persistLanguageSettings();
    retranslateUi();
}
