#include "gui/MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLinearGradient>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPushButton>
#include <QDropEvent>
#include <QFile>
#include <QPalette>
#include <QPlainTextEdit>
#include <QGuiApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QSlider>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QToolButton>
#include <QToolBar>
#include <QTranslator>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <QMouseEvent>

#include <algorithm>
#include <cmath>
#include <set>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "QtnRibbonBar.h"
#include "QtnRibbonGroup.h"
#include "QtnRibbonPage.h"
#include "QtnRibbonQuickAccessBar.h"

#include "domain/ClearanceAnalysis.h"
#include "domain/ClearanceReportExporter.h"
#include "domain/InspectionData.h"
#include "domain/InspectionReportExporter.h"
#include "domain/ProfileMarkerProjection.h"
#include "gui/PointCloudViewer.h"
#include "gui/ProfilePlotWidget.h"
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
constexpr int kProjectTreeItemTypeRole = Qt::UserRole;
constexpr int kProjectTreeFilePathRole = Qt::UserRole + 1;

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
    Tower,
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

QString datasetPathSummary(const QStringList& filePaths)
{
    if (filePaths.isEmpty()) {
        return QString();
    }

    QStringList lines;
    const int visibleCount = std::min(4, filePaths.size());
    for (int index = 0; index < visibleCount; ++index) {
        lines.append(filePaths.at(index));
    }
    if (filePaths.size() > visibleCount) {
        lines.append(
            QCoreApplication::translate("MainWindow", "... and %1 more")
                .arg(QLocale().toString(filePaths.size() - visibleCount)));
    }
    return lines.join(QLatin1Char('\n'));
}

QString projectRelativePathFor(const QString& projectFilePath, const QString& targetFilePath)
{
    const QFileInfo targetInfo(targetFilePath);
    if (!targetInfo.exists()) {
        return targetFilePath;
    }

    const QDir projectDir = QFileInfo(projectFilePath).absoluteDir();
    return projectDir.relativeFilePath(targetInfo.absoluteFilePath());
}

QString resolveProjectPath(const QString& projectFilePath, const QString& storedPath)
{
    if (storedPath.isEmpty()) {
        return QString();
    }

    const QFileInfo storedInfo(storedPath);
    if (storedInfo.isAbsolute()) {
        return storedPath;
    }

    return QFileInfo(QFileInfo(projectFilePath).absoluteDir(), storedPath).absoluteFilePath();
}

QJsonObject colorToJson(const QColor& color)
{
    return QJsonObject {
        { QStringLiteral("r"), color.red() },
        { QStringLiteral("g"), color.green() },
        { QStringLiteral("b"), color.blue() },
        { QStringLiteral("a"), color.alpha() }
    };
}

QColor colorFromJson(const QJsonObject& object, const QColor& fallback)
{
    if (object.isEmpty()) {
        return fallback;
    }

    return QColor(
        object.value(QStringLiteral("r")).toInt(fallback.red()),
        object.value(QStringLiteral("g")).toInt(fallback.green()),
        object.value(QStringLiteral("b")).toInt(fallback.blue()),
        object.value(QStringLiteral("a")).toInt(fallback.alpha()));
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
    case RibbonGlyph::Tower:
        painter.setPen(QPen(kRibbonGlyphColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.top() + 4.0), QPointF(r.center().x(), r.bottom() - 2.0));
        painter.drawLine(QPointF(r.center().x() - 7.0, r.top() + 10.0), QPointF(r.center().x() + 7.0, r.top() + 10.0));
        painter.drawLine(QPointF(r.center().x() - 5.0, r.top() + 17.0), QPointF(r.center().x() + 5.0, r.top() + 17.0));
        painter.setBrush(QColor(249, 115, 22));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(r.center().x() - 5.0, r.bottom() - 9.0, 10.0, 10.0));
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
    createProjectDock();
    createInspectorPanel();
    createProfileDock();
    createLogDock();
    createStatusBar();
    setDockNestingEnabled(true);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    if (projectDock_ != nullptr && inspectorDock_ != nullptr) {
        resizeDocks({ projectDock_, inspectorDock_ }, { 320, 380 }, Qt::Horizontal);
    }
    loadInteractionSettings();
    loadMeasurementSettings();
    loadVisualizationSettings();
    createConnections();
    applyLanguage(currentLanguage_);
    loadThemeSettings();
    loadWindowSettings();

    syncUiFromViewer();
    updateNavigationHelpText();
    showUserMessage(LogLevel::Info, tr("Ready. Open, add, or drag LAS/LAZ files to begin."), 4000);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    persistVisualizationSettings();
    persistInteractionSettings();
    persistMeasurementSettings();
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

    QStringList filePaths;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }

        const QString filePath = url.toLocalFile();
        if (!isSupportedPointCloudFile(filePath)) {
            continue;
        }

        filePaths.append(filePath);
    }

    if (!filePaths.isEmpty()) {
        const bool hasExistingPointCloud = viewer_ != nullptr && viewer_->hasPointCloud();
        const bool handled = hasExistingPointCloud
            ? appendPointCloudFiles(filePaths)
            : loadPointCloudFiles(filePaths);
        if (handled) {
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
    openAction_->setToolTip(tr("Open one or more LAS or LAZ datasets"));
    addPointCloudAction_ = new QAction(createRibbonIcon(RibbonGlyph::Open), tr("Add LAS Files"), this);
    addPointCloudAction_->setToolTip(tr("Add one or more LAS or LAZ datasets to the current project"));
    removeDatasetAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Remove Selected Dataset"), this);
    removeDatasetAction_->setToolTip(tr("Remove the selected LAS or LAZ dataset from the project"));
    locateDatasetAction_ = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Open Folder"), this);
    locateDatasetAction_->setToolTip(tr("Open the folder that contains the selected dataset"));
    copyDatasetPathAction_ = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("Copy Path"), this);
    copyDatasetPathAction_->setToolTip(tr("Copy the full path of the selected dataset"));
    expandProjectTreeAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowDown), tr("Expand All"), this);
    expandProjectTreeAction_->setToolTip(tr("Expand the project explorer tree"));
    collapseProjectTreeAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("Collapse All"), this);
    collapseProjectTreeAction_->setToolTip(tr("Collapse the project explorer tree"));

    openProjectAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Open Project"), this);
    saveProjectAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Project"), this);
    saveProjectAsAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Project As"), this);

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
    exportClearanceCsvAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Clearance CSV"), this);
    showProfileDockAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Profile View"), this);
    showProfileDockAction_->setCheckable(true);
    showProfileDockAction_->setChecked(true);

    startTowerEditAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Start Editing"), this);
    finishTowerEditAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Finish Editing"), this);
    addTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Click To Add Tower"), this);
    insertTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Insert Before Current"), this);
    moveTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Move Current Tower"), this);
    focusTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Focus Current Tower"), this);
    removeTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Remove Current Tower"), this);
    clearTowersAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Tower Markers"), this);
    cancelTowerToolAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Cancel Tower Tool"), this);
    showTowerXAction_ = new QAction(tr("Show X"), this);
    showTowerYAction_ = new QAction(tr("Show Y"), this);
    showTowerZAction_ = new QAction(tr("Show Z"), this);
    showTowerXAction_->setCheckable(true);
    showTowerYAction_->setCheckable(true);
    showTowerZAction_->setCheckable(true);
    startIssueMarkAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Mark Issue"), this);
    startIssueMarkAction_->setToolTip(tr("Click a point in the view to add an inspection issue"));
    cancelIssueToolAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Cancel Issue Tool"), this);
    focusIssueAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Focus Current Issue"), this);
    removeIssueAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Remove Current Issue"), this);
    clearIssuesAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Issues"), this);
    exportIssuesCsvAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Issues CSV"), this);
    exportInspectionReportAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Inspection Report"), this);

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
    ribbonBar_->quickAccessBar()->addAction(saveProjectAction_);
    ribbonBar_->quickAccessBar()->addAction(fitSceneAction_);
    ribbonBar_->quickAccessBar()->addAction(showAxesAction_);
    ribbonBar_->quickAccessBar()->addAction(measureAction_);

    homePage_ = ribbonBar_->addPage(tr("Home"));
    datasetRibbonGroup_ = homePage_->addGroup(tr("Dataset"));
    datasetRibbonGroup_->addAction(openAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(addPointCloudAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(openProjectAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(saveProjectAction_, Qt::ToolButtonTextUnderIcon);
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
    measureRibbonGroup_->addAction(exportClearanceCsvAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(showProfileDockAction_, Qt::ToolButtonTextUnderIcon);

    workspaceRibbonGroup_ = homePage_->addGroup(tr("Workspace"));
    workspaceRibbonGroup_->addAction(saveProjectAsAction_, Qt::ToolButtonTextUnderIcon);
    workspaceRibbonGroup_->addAction(startIssueMarkAction_, Qt::ToolButtonTextUnderIcon);
    workspaceRibbonGroup_->addAction(exportInspectionReportAction_, Qt::ToolButtonTextUnderIcon);
    workspaceRibbonGroup_->addAction(showLogAction_, Qt::ToolButtonTextUnderIcon);

    towerPage_ = ribbonBar_->addPage(tr("Tower"));
    towerRibbonGroup_ = towerPage_->addGroup(tr("Tower Editing"));
    towerRibbonGroup_->addAction(startTowerEditAction_, Qt::ToolButtonTextUnderIcon);
    towerRibbonGroup_->addAction(finishTowerEditAction_, Qt::ToolButtonTextUnderIcon);

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

void MainWindow::createProjectDock()
{
    projectDock_ = new QDockWidget(tr("Project Explorer"), this);
    projectDock_->setObjectName(QStringLiteral("projectExplorerDock"));
    projectDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    projectDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    projectDock_->setMinimumWidth(280);

    auto* projectDockContents = new QWidget(projectDock_);
    projectDockContents->setObjectName(QStringLiteral("projectExplorerSurface"));
    auto* projectDockLayout = new QVBoxLayout(projectDockContents);
    projectDockLayout->setContentsMargins(12, 12, 12, 12);
    projectDockLayout->setSpacing(10);

    projectSearchEdit_ = new QLineEdit(projectDockContents);
    projectSearchEdit_->setObjectName(QStringLiteral("projectExplorerSearch"));
    projectSearchEdit_->setClearButtonEnabled(true);

    projectToolBar_ = new QToolBar(projectDockContents);
    projectToolBar_->setObjectName(QStringLiteral("projectExplorerToolBar"));
    projectToolBar_->setIconSize(QSize(16, 16));
    projectToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    projectToolBar_->setMovable(false);
    projectToolBar_->setFloatable(false);
    projectToolBar_->addAction(openAction_);
    projectToolBar_->addAction(addPointCloudAction_);
    projectToolBar_->addAction(removeDatasetAction_);
    projectToolBar_->addSeparator();
    projectToolBar_->addAction(locateDatasetAction_);
    projectToolBar_->addAction(copyDatasetPathAction_);
    projectToolBar_->addSeparator();
    projectToolBar_->addAction(expandProjectTreeAction_);
    projectToolBar_->addAction(collapseProjectTreeAction_);

    projectTreeWidget_ = new QTreeWidget(projectDockContents);
    projectTreeWidget_->setObjectName(QStringLiteral("projectExplorerTree"));
    projectTreeWidget_->setColumnCount(1);
    projectTreeWidget_->setHeaderHidden(true);
    projectTreeWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    projectTreeWidget_->setAlternatingRowColors(false);
    projectTreeWidget_->setRootIsDecorated(true);
    projectTreeWidget_->setUniformRowHeights(true);
    projectTreeWidget_->setAnimated(true);
    projectTreeWidget_->setIndentation(18);
    projectTreeWidget_->setFrameShape(QFrame::NoFrame);
    projectTreeWidget_->setContextMenuPolicy(Qt::CustomContextMenu);

    projectDockLayout->addWidget(projectSearchEdit_);
    projectDockLayout->addWidget(projectToolBar_);
    projectDockLayout->addWidget(projectTreeWidget_, 1);

    projectDockContents->setStyleSheet(QStringLiteral(
        "QWidget#projectExplorerSurface {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f8fafc, stop:1 #eef4fb);"
        "border: none;"
        "}"
        "QLineEdit#projectExplorerSearch {"
        "background: rgba(255, 255, 255, 0.92);"
        "border: 1px solid #d6dee9;"
        "border-radius: 10px;"
        "padding: 8px 12px;"
        "color: #0f172a;"
        "selection-background-color: #bfdbfe;"
        "}"
        "QLineEdit#projectExplorerSearch:focus {"
        "border-color: #60a5fa;"
        "}"
        "QToolBar#projectExplorerToolBar {"
        "background: rgba(255, 255, 255, 0.7);"
        "border: 1px solid #d8e0ea;"
        "border-radius: 10px;"
        "padding: 4px;"
        "spacing: 4px;"
        "}"
        "QToolBar#projectExplorerToolBar QToolButton {"
        "background: transparent;"
        "border: none;"
        "border-radius: 8px;"
        "padding: 6px 10px;"
        "color: #1e293b;"
        "}"
        "QToolBar#projectExplorerToolBar QToolButton:hover {"
        "background: rgba(219, 234, 254, 0.95);"
        "}"
        "QToolBar#projectExplorerToolBar QToolButton:pressed,"
        "QToolBar#projectExplorerToolBar QToolButton:checked {"
        "background: #2563eb;"
        "color: #eff6ff;"
        "}"
        "QTreeWidget#projectExplorerTree {"
        "background: rgba(255, 255, 255, 0.84);"
        "border: 1px solid #d8e0ea;"
        "border-radius: 12px;"
        "padding: 8px 6px;"
        "color: #0f172a;"
        "outline: none;"
        "}"
        "QTreeWidget#projectExplorerTree::item {"
        "min-height: 28px;"
        "padding: 4px 8px;"
        "border-radius: 8px;"
        "}"
        "QTreeWidget#projectExplorerTree::item:hover {"
        "background: rgba(219, 234, 254, 0.85);"
        "}"
        "QTreeWidget#projectExplorerTree::item:selected {"
        "background: #2563eb;"
        "color: #eff6ff;"
        "}"));

    projectDock_->setWidget(projectDockContents);
    addDockWidget(Qt::LeftDockWidgetArea, projectDock_);
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
    auto towerTab = createTabPage(QStringLiteral("sceneInspectorTowerPage"));
    auto issueTab = createTabPage(QStringLiteral("sceneInspectorIssuePage"));
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

    auto* towerToolbarHost = new QWidget(towerTab.first);
    auto* towerToolbarHostLayout = new QHBoxLayout(towerToolbarHost);
    towerToolbarHostLayout->setContentsMargins(0, 0, 0, 0);
    towerToolbarHostLayout->setSpacing(8);

    towerToolBar_ = new QToolBar(towerToolbarHost);
    towerToolBar_->setIconSize(QSize(16, 16));
    towerToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    towerToolBar_->setMovable(false);
    towerToolBar_->setFloatable(false);
    towerToolBar_->addAction(addTowerAction_);
    towerToolBar_->addAction(insertTowerAction_);
    towerToolBar_->addAction(moveTowerAction_);
    towerToolBar_->addSeparator();
    towerToolBar_->addAction(focusTowerAction_);
    towerToolBar_->addAction(removeTowerAction_);
    towerToolBar_->addAction(clearTowersAction_);
    towerToolBar_->addAction(cancelTowerToolAction_);

    towerActionsMenu_ = new QMenu(towerToolbarHost);
    towerActionsMenu_->addAction(addTowerAction_);
    towerActionsMenu_->addAction(insertTowerAction_);
    towerActionsMenu_->addAction(moveTowerAction_);
    towerActionsMenu_->addSeparator();
    towerActionsMenu_->addAction(focusTowerAction_);
    towerActionsMenu_->addAction(removeTowerAction_);
    towerActionsMenu_->addAction(clearTowersAction_);
    towerActionsMenu_->addAction(cancelTowerToolAction_);
    towerActionsMenu_->addSeparator();
    towerActionsMenu_->addAction(showTowerXAction_);
    towerActionsMenu_->addAction(showTowerYAction_);
    towerActionsMenu_->addAction(showTowerZAction_);

    towerMenuButton_ = new QToolButton(towerToolbarHost);
    towerMenuButton_->setPopupMode(QToolButton::InstantPopup);
    towerMenuButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    towerMenuButton_->setMenu(towerActionsMenu_);

    towerToolbarHostLayout->addWidget(towerToolBar_, 1);
    towerToolbarHostLayout->addWidget(towerMenuButton_, 0);

    auto* towerPanel = new QWidget(towerTab.first);
    auto* towerLayout = new QVBoxLayout(towerPanel);
    towerLayout->setContentsMargins(0, 0, 0, 0);
    towerLayout->setSpacing(8);

    towerCountValueLabel_ = new QLabel(towerPanel);
    towerCountValueLabel_->setWordWrap(true);
    towerToolStatusLabel_ = new QLabel(towerPanel);
    towerToolStatusLabel_->setWordWrap(true);

    towerTableWidget_ = new QTableWidget(towerPanel);
    towerTableWidget_->setColumnCount(5);
    towerTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    towerTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    towerTableWidget_->setAlternatingRowColors(true);
    towerTableWidget_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    towerTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Name"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z") });
    towerTableWidget_->verticalHeader()->setVisible(false);
    towerTableWidget_->horizontalHeader()->setStretchLastSection(false);
    towerTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    towerTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    towerTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    towerTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    towerTableWidget_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    towerTableWidget_->setColumnHidden(2, true);
    towerTableWidget_->setColumnHidden(3, true);
    towerTableWidget_->setColumnHidden(4, true);

    towerLayout->addWidget(towerCountValueLabel_);
    towerLayout->addWidget(towerToolStatusLabel_);
    towerLayout->addWidget(towerTableWidget_, 1);

    towerDetailsGroupBox_ = new QGroupBox(tr("Selected Tower Details"), towerPanel);
    towerDetailsLayout_ = new QFormLayout(towerDetailsGroupBox_);
    towerDetailsLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    towerDetailsLayout_->setFormAlignment(Qt::AlignTop);
    towerCodeEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerLineNameEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerVoltageLevelEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerStructureTypeEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerInspectionDateEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerStatusEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerNotesEdit_ = new QPlainTextEdit(towerDetailsGroupBox_);
    towerNotesEdit_->setMaximumHeight(96);
    towerDetailsLayout_->addRow(tr("Code"), towerCodeEdit_);
    towerDetailsLayout_->addRow(tr("Line"), towerLineNameEdit_);
    towerDetailsLayout_->addRow(tr("Voltage"), towerVoltageLevelEdit_);
    towerDetailsLayout_->addRow(tr("Tower Type"), towerStructureTypeEdit_);
    towerDetailsLayout_->addRow(tr("Inspection Date"), towerInspectionDateEdit_);
    towerDetailsLayout_->addRow(tr("Tower Status"), towerStatusEdit_);
    towerDetailsLayout_->addRow(tr("Notes"), towerNotesEdit_);
    towerLayout->addWidget(towerDetailsGroupBox_);

    auto* issueToolbarHost = new QWidget(issueTab.first);
    auto* issueToolbarHostLayout = new QHBoxLayout(issueToolbarHost);
    issueToolbarHostLayout->setContentsMargins(0, 0, 0, 0);
    issueToolbarHostLayout->setSpacing(8);

    issueToolBar_ = new QToolBar(issueToolbarHost);
    issueToolBar_->setIconSize(QSize(16, 16));
    issueToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    issueToolBar_->setMovable(false);
    issueToolBar_->setFloatable(false);
    issueToolBar_->addAction(startIssueMarkAction_);
    issueToolBar_->addAction(cancelIssueToolAction_);
    issueToolBar_->addSeparator();
    issueToolBar_->addAction(focusIssueAction_);
    issueToolBar_->addAction(removeIssueAction_);
    issueToolBar_->addAction(clearIssuesAction_);
    issueToolBar_->addSeparator();
    issueToolBar_->addAction(exportIssuesCsvAction_);
    issueToolBar_->addAction(exportInspectionReportAction_);

    issueActionsMenu_ = new QMenu(issueToolbarHost);
    issueActionsMenu_->addAction(startIssueMarkAction_);
    issueActionsMenu_->addAction(cancelIssueToolAction_);
    issueActionsMenu_->addSeparator();
    issueActionsMenu_->addAction(focusIssueAction_);
    issueActionsMenu_->addAction(removeIssueAction_);
    issueActionsMenu_->addAction(clearIssuesAction_);
    issueActionsMenu_->addSeparator();
    issueActionsMenu_->addAction(exportIssuesCsvAction_);
    issueActionsMenu_->addAction(exportInspectionReportAction_);

    issueMenuButton_ = new QToolButton(issueToolbarHost);
    issueMenuButton_->setPopupMode(QToolButton::InstantPopup);
    issueMenuButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    issueMenuButton_->setMenu(issueActionsMenu_);

    issueToolbarHostLayout->addWidget(issueToolBar_, 1);
    issueToolbarHostLayout->addWidget(issueMenuButton_, 0);

    auto* issuePanel = new QWidget(issueTab.first);
    auto* issueLayout = new QVBoxLayout(issuePanel);
    issueLayout->setContentsMargins(0, 0, 0, 0);
    issueLayout->setSpacing(8);

    issueCountValueLabel_ = new QLabel(issuePanel);
    issueCountValueLabel_->setWordWrap(true);
    issueToolStatusLabel_ = new QLabel(issuePanel);
    issueToolStatusLabel_->setWordWrap(true);

    issueTableWidget_ = new QTableWidget(issuePanel);
    issueTableWidget_->setColumnCount(6);
    issueTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    issueTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    issueTableWidget_->setAlternatingRowColors(true);
    issueTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    issueTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Title"), tr("Severity"), tr("Status"), tr("Tower"), tr("Category") });
    issueTableWidget_->verticalHeader()->setVisible(false);
    issueTableWidget_->horizontalHeader()->setStretchLastSection(false);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    issueDetailsGroupBox_ = new QGroupBox(tr("Selected Issue Details"), issuePanel);
    issueDetailsLayout_ = new QFormLayout(issueDetailsGroupBox_);
    issueDetailsLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    issueDetailsLayout_->setFormAlignment(Qt::AlignTop);
    issueTitleEdit_ = new QLineEdit(issueDetailsGroupBox_);
    issueCategoryComboBox_ = new QComboBox(issueDetailsGroupBox_);
    issueCategoryComboBox_->setEditable(true);
    issueCategoryComboBox_->addItems({ tr("Vegetation"), tr("Insulator"), tr("Tower Body"), tr("Channel Risk"), tr("Other") });
    issueSeverityComboBox_ = new QComboBox(issueDetailsGroupBox_);
    issueStatusComboBox_ = new QComboBox(issueDetailsGroupBox_);
    issueRelatedTowerComboBox_ = new QComboBox(issueDetailsGroupBox_);
    issueImagePathEdit_ = new QLineEdit(issueDetailsGroupBox_);
    issueDescriptionEdit_ = new QPlainTextEdit(issueDetailsGroupBox_);
    issueDescriptionEdit_->setMaximumHeight(110);
    issueLocationValueLabel_ = new QLabel(issueDetailsGroupBox_);
    issueLocationValueLabel_->setWordWrap(true);
    issueCreatedAtValueLabel_ = new QLabel(issueDetailsGroupBox_);
    issueCreatedAtValueLabel_->setWordWrap(true);
    issueSeverityComboBox_->addItems({
        issueSeverityDisplayName(IssueSeverity::Info),
        issueSeverityDisplayName(IssueSeverity::Minor),
        issueSeverityDisplayName(IssueSeverity::Major),
        issueSeverityDisplayName(IssueSeverity::Critical)
    });
    issueStatusComboBox_->addItems({
        issueStatusDisplayName(IssueStatus::Open),
        issueStatusDisplayName(IssueStatus::Monitoring),
        issueStatusDisplayName(IssueStatus::Resolved)
    });
    issueDetailsLayout_->addRow(tr("Title"), issueTitleEdit_);
    issueDetailsLayout_->addRow(tr("Category"), issueCategoryComboBox_);
    issueDetailsLayout_->addRow(tr("Severity"), issueSeverityComboBox_);
    issueDetailsLayout_->addRow(tr("Issue Status"), issueStatusComboBox_);
    issueDetailsLayout_->addRow(tr("Related Tower"), issueRelatedTowerComboBox_);
    issueDetailsLayout_->addRow(tr("Image Path"), issueImagePathEdit_);
    issueDetailsLayout_->addRow(tr("Location"), issueLocationValueLabel_);
    issueDetailsLayout_->addRow(tr("Created At"), issueCreatedAtValueLabel_);
    issueDetailsLayout_->addRow(tr("Description"), issueDescriptionEdit_);

    issueLayout->addWidget(issueCountValueLabel_);
    issueLayout->addWidget(issueToolStatusLabel_);
    issueLayout->addWidget(issueTableWidget_, 1);
    issueLayout->addWidget(issueDetailsGroupBox_);

    renderingGroupBox_ = new QGroupBox(tr("Rendering Controls"), renderingTab.first);
    renderingLayout_ = new QFormLayout(renderingGroupBox_);
    renderingLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    renderingLayout_->setFormAlignment(Qt::AlignTop);

    pointSizeControl_ = createSliderControl(pointSizeSlider_, pointSizeValueLabel_, 1, 20, 1);
    pointOpacityControl_ = createSliderControl(pointOpacitySlider_, pointOpacityValueLabel_, 10, 100, 5);
    depthCueControl_ = createSliderControl(depthCueSlider_, depthCueValueLabel_, 0, 100, 5);
    edlStrengthControl_ = createSliderControl(edlStrengthSlider_, edlStrengthValueLabel_, 0, 100, 5);

    colorModeComboBox_ = new QComboBox(renderingGroupBox_);
    colorModeComboBox_->addItem(tr("RGB"));
    colorModeComboBox_->addItem(tr("Elevation Ramp"));
    colorModeComboBox_->addItem(tr("Single Color"));

    pointColorButton_ = new QPushButton(tr("Pick Color"), renderingGroupBox_);
    backgroundColorButton_ = new QPushButton(tr("Pick Background"), renderingGroupBox_);

    roundSplatsCheckBox_ = new QCheckBox(tr("Round splats (survey style)"), renderingGroupBox_);
    axesCheckBox_ = new QCheckBox(tr("Show XYZ axes"), renderingGroupBox_);
    boundingBoxCheckBox_ = new QCheckBox(tr("Show bounding box"), renderingGroupBox_);

    renderingLayout_->addRow(tr("Point Size"), pointSizeControl_);
    renderingLayout_->addRow(tr("Point Opacity"), pointOpacityControl_);
    renderingLayout_->addRow(tr("Depth Cue"), depthCueControl_);
    renderingLayout_->addRow(tr("EDL-style Shading"), edlStrengthControl_);
    renderingLayout_->addRow(tr("Color Mode"), colorModeComboBox_);
    renderingLayout_->addRow(tr("Single Color"), pointColorButton_);
    renderingLayout_->addRow(tr("Background"), backgroundColorButton_);
    renderingLayout_->addRow(QString(), roundSplatsCheckBox_);
    renderingLayout_->addRow(QString(), axesCheckBox_);
    renderingLayout_->addRow(QString(), boundingBoxCheckBox_);

    auto* measurementToolbarHost = new QWidget(measurementTab.first);
    auto* measurementToolbarHostLayout = new QHBoxLayout(measurementToolbarHost);
    measurementToolbarHostLayout->setContentsMargins(0, 0, 0, 0);
    measurementToolbarHostLayout->setSpacing(8);

    measurementToolBar_ = new QToolBar(measurementToolbarHost);
    measurementToolBar_->setIconSize(QSize(16, 16));
    measurementToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    measurementToolBar_->setMovable(false);
    measurementToolBar_->setFloatable(false);
    measurementToolBar_->addAction(measureAction_);
    measurementToolBar_->addAction(clearMeasurementAction_);
    measurementToolBar_->addSeparator();
    measurementToolBar_->addAction(exportClearanceCsvAction_);
    measurementToolBar_->addAction(showProfileDockAction_);
    measurementToolbarHostLayout->addWidget(measurementToolBar_, 1);

    measurementGroupBox_ = new QGroupBox(tr("Measurement"), measurementTab.first);
    measurementLayout_ = new QFormLayout(measurementGroupBox_);
    measurementLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    measurementLayout_->setFormAlignment(Qt::AlignTop);

    measurementToggleButton_ = new QPushButton(tr("Start Measurement"), measurementGroupBox_);
    measurementClearButton_ = new QPushButton(tr("Clear Measurement"), measurementGroupBox_);
    measurementStartValueLabel_ = new QLabel(measurementGroupBox_);
    measurementEndValueLabel_ = new QLabel(measurementGroupBox_);
    measurementDistanceValueLabel_ = new QLabel(measurementGroupBox_);
    measurementHorizontalDistanceValueLabel_ = new QLabel(measurementGroupBox_);
    measurementDeltaZValueLabel_ = new QLabel(measurementGroupBox_);
    measurementSegmentsValueLabel_ = new QLabel(measurementGroupBox_);

    const QList<QLabel*> measurementLabels = {
        measurementStartValueLabel_,
        measurementEndValueLabel_,
        measurementDistanceValueLabel_,
        measurementHorizontalDistanceValueLabel_,
        measurementDeltaZValueLabel_,
        measurementSegmentsValueLabel_
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
    measurementLayout_->addRow(tr("Horizontal Distance"), measurementHorizontalDistanceValueLabel_);
    measurementLayout_->addRow(tr("Height Delta"), measurementDeltaZValueLabel_);
    measurementLayout_->addRow(tr("Path Segments"), measurementSegmentsValueLabel_);

    clearanceGroupBox_ = new QGroupBox(tr("Clearance Analysis"), measurementTab.first);
    clearanceLayout_ = new QFormLayout(clearanceGroupBox_);
    clearanceLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    clearanceLayout_->setFormAlignment(Qt::AlignTop);

    clearanceThresholdSpinBox_ = new QDoubleSpinBox(clearanceGroupBox_);
    clearanceThresholdSpinBox_->setRange(0.0, 1000000.0);
    clearanceThresholdSpinBox_->setDecimals(2);
    clearanceThresholdSpinBox_->setSingleStep(0.5);
    clearanceThresholdSpinBox_->setKeyboardTracking(false);

    clearanceShortestValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceWarningCountValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceStatusValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceStatusValueLabel_->setWordWrap(true);

    for (QLabel* label : { clearanceShortestValueLabel_, clearanceWarningCountValueLabel_, clearanceStatusValueLabel_ }) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    clearanceLayout_->addRow(tr("Warning Threshold"), clearanceThresholdSpinBox_);
    clearanceLayout_->addRow(tr("Shortest Segment"), clearanceShortestValueLabel_);
    clearanceLayout_->addRow(tr("Warning Segments"), clearanceWarningCountValueLabel_);
    clearanceLayout_->addRow(tr("Status"), clearanceStatusValueLabel_);

    clearanceSegmentsGroupBox_ = new QGroupBox(tr("Path Segment Details"), measurementTab.first);
    auto* clearanceSegmentsLayout = new QVBoxLayout(clearanceSegmentsGroupBox_);
    clearanceSegmentsLayout->setContentsMargins(12, 12, 12, 12);
    clearanceSegmentsLayout->setSpacing(8);

    clearanceSegmentsSummaryLabel_ = new QLabel(clearanceSegmentsGroupBox_);
    clearanceSegmentsSummaryLabel_->setWordWrap(true);
    clearanceSegmentsSummaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    clearanceSegmentsTableWidget_ = new QTableWidget(clearanceSegmentsGroupBox_);
    clearanceSegmentsTableWidget_->setColumnCount(8);
    clearanceSegmentsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    clearanceSegmentsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    clearanceSegmentsTableWidget_->setAlternatingRowColors(true);
    clearanceSegmentsTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    clearanceSegmentsTableWidget_->setHorizontalHeaderLabels({
        tr("Segment"),
        tr("From"),
        tr("To"),
        tr("Chainage"),
        tr("Horizontal"),
        tr("3D"),
        tr("dZ"),
        tr("Status")
    });
    clearanceSegmentsTableWidget_->verticalHeader()->setVisible(false);
    clearanceSegmentsTableWidget_->horizontalHeader()->setStretchLastSection(false);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->setMinimumHeight(220);

    clearanceSegmentsLayout->addWidget(clearanceSegmentsSummaryLabel_);
    clearanceSegmentsLayout->addWidget(clearanceSegmentsTableWidget_, 1);

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
    towerTab.second->addWidget(towerToolbarHost);
    towerTab.second->addWidget(towerPanel, 1);
    issueTab.second->addWidget(issueToolbarHost);
    issueTab.second->addWidget(issuePanel, 1);
    renderingTab.second->addWidget(renderingGroupBox_);
    renderingTab.second->addStretch(1);
    measurementTab.second->addWidget(measurementToolbarHost);
    measurementTab.second->addWidget(measurementGroupBox_);
    measurementTab.second->addWidget(clearanceGroupBox_);
    measurementTab.second->addWidget(clearanceSegmentsGroupBox_);
    measurementTab.second->addStretch(1);
    navigationTab.second->addWidget(navigationGroupBox_);
    navigationTab.second->addStretch(1);

    inspectorTabWidget_->addTab(overviewTab.first, QString());
    inspectorTabWidget_->addTab(towerTab.first, QString());
    inspectorTabWidget_->addTab(issueTab.first, QString());
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
        "QToolBar {"
        "background: transparent;"
        "border: none;"
        "spacing: 6px;"
        "padding: 0;"
        "}"
        "QToolButton {"
        "background-color: #ffffff;"
        "border: 1px solid #d6dde8;"
        "border-radius: 6px;"
        "padding: 6px 10px;"
        "color: #1f2937;"
        "font-weight: 600;"
        "}"
        "QToolButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QToolButton:pressed {"
        "background-color: #dbeafe;"
        "}"
        "QToolButton::menu-indicator {"
        "subcontrol-origin: padding;"
        "subcontrol-position: right center;"
        "right: 8px;"
        "}"
        "QLabel {"
        "color: #1f2937;"
        "}"
        "QTreeWidget {"
        "background-color: #ffffff;"
        "border: 1px solid #d6dde8;"
        "border-radius: 8px;"
        "alternate-background-color: #f8fbff;"
        "padding: 4px;"
        "}"
        "QTreeWidget::item {"
        "padding: 4px 6px;"
        "}"
        "QTreeWidget::item:selected {"
        "background-color: #dbeafe;"
        "color: #0f172a;"
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
        "QPushButton, QComboBox, QSpinBox, QDoubleSpinBox {"
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
        "QSpinBox, QDoubleSpinBox {"
        "padding-right: 20px;"
        "}"
        "QComboBox::drop-down {"
        "subcontrol-origin: padding;"
        "subcontrol-position: top right;"
        "width: 24px;"
        "border: none;"
        "}"
        "QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
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
        "QPushButton:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover {"
        "border-color: #94a3b8;"
        "}"
        "QCheckBox {"
        "color: #1f2937;"
        "}");

    inspectorDock_->setWidget(inspectorTabWidget_);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);
}

void MainWindow::createProfileDock()
{
    profileDock_ = new QDockWidget(tr("Span Profile"), this);
    profileDock_->setObjectName(QStringLiteral("spanProfileDock"));
    profileDock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    profileDock_->setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    profileDock_->setMinimumHeight(220);

    auto* profileSurface = new QWidget(profileDock_);
    profileSurface->setObjectName(QStringLiteral("spanProfileSurface"));
    auto* profileLayout = new QVBoxLayout(profileSurface);
    profileLayout->setContentsMargins(12, 12, 12, 12);
    profileLayout->setSpacing(8);

    auto* titleLabel = new QLabel(tr("Measured corridor profile"), profileSurface);
    titleLabel->setObjectName(QStringLiteral("spanProfileTitleLabel"));
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel#spanProfileTitleLabel {"
        "font-size: 14px;"
        "font-weight: 600;"
        "color: #0f172a;"
        "}"));

    auto* subtitleLabel = new QLabel(
        tr("The profile updates from the current measurement path, highlights clearance segments below the threshold, and overlays nearby towers and issues."),
        profileSurface);
    subtitleLabel->setObjectName(QStringLiteral("spanProfileSubtitleLabel"));
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setStyleSheet(QStringLiteral("color: #64748b;"));

    profilePlotWidget_ = new ProfilePlotWidget(profileSurface);
    profilePlotWidget_->setObjectName(QStringLiteral("spanProfilePlotWidget"));
    profilePlotWidget_->setStyleSheet(QStringLiteral(
        "ProfilePlotWidget#spanProfilePlotWidget {"
        "background: transparent;"
        "}"));

    profileLayout->addWidget(titleLabel);
    profileLayout->addWidget(subtitleLabel);
    profileLayout->addWidget(profilePlotWidget_, 1);

    profileSurface->setStyleSheet(QStringLiteral(
        "QWidget#spanProfileSurface {"
        "background-color: #eef4fb;"
        "border-top: 1px solid #d7e2f0;"
        "}"));

    profileDock_->setWidget(profileSurface);
    addDockWidget(Qt::BottomDockWidgetArea, profileDock_);
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

QWidget* MainWindow::createSliderControl(QSlider*& slider, QLabel*& valueLabel, int minimum, int maximum, int step)
{
    auto* container = new QWidget(renderingGroupBox_);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(minimum, maximum);
    slider->setSingleStep(step);
    slider->setPageStep(std::max(step, (maximum - minimum) / 10));
    slider->setTracking(true);
    slider->setTickInterval(step);

    valueLabel = new QLabel(container);
    valueLabel->setMinimumWidth(56);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "color: #475569;"
        "font-weight: 600;"
        "}"));

    layout->addWidget(slider, 1);
    layout->addWidget(valueLabel, 0);
    return container;
}

void MainWindow::updateSliderValueLabel(QSlider* slider, QLabel* valueLabel, const QString& formatText) const
{
    if (slider == nullptr || valueLabel == nullptr) {
        return;
    }

    valueLabel->setText(formatText.arg(QLocale().toString(slider->value())));
}

void MainWindow::updateVisualizationTooltips()
{
    const auto applyTooltip = [](QWidget* primary, QWidget* secondary, QWidget* tertiary, const QString& tooltip) {
        if (primary != nullptr) {
            primary->setToolTip(tooltip);
        }
        if (secondary != nullptr) {
            secondary->setToolTip(tooltip);
        }
        if (tertiary != nullptr) {
            tertiary->setToolTip(tooltip);
        }
    };

    if (pointSizeSlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>Point Size</b><br/>"
            "Controls the screen size of each rendered point.");
        applyTooltip(pointSizeSlider_, pointSizeValueLabel_, pointSizeControl_, tooltip);
    }
    if (pointOpacitySlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>Point Opacity</b><br/>"
            "Controls how solid each point appears.<br/>"
            "Lower values reveal deeper layers; higher values make the cloud denser and stronger.");
        applyTooltip(pointOpacitySlider_, pointOpacityValueLabel_, pointOpacityControl_, tooltip);
    }
    if (depthCueSlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>Depth Cue</b><br/>"
            "Adds distance-based fading for better front/back separation.<br/>"
            "Higher values make distant points fade more strongly.");
        applyTooltip(depthCueSlider_, depthCueValueLabel_, depthCueControl_, tooltip);
    }
    if (edlStrengthSlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>EDL-style Shading</b><br/>"
            "Enhances point edges and local depth contrast, similar to survey software display enhancement.<br/>"
            "Higher values produce stronger contour darkening and a more layered look.");
        applyTooltip(edlStrengthSlider_, edlStrengthValueLabel_, edlStrengthControl_, tooltip);
    }
    if (roundSplatsCheckBox_ != nullptr) {
        roundSplatsCheckBox_->setToolTip(tr(
            "<b>Round splats</b><br/>"
            "Draw points as circular splats instead of square pixels for a more natural survey-style point cloud look."));
    }
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("LAS Point Cloud Viewer"));

    openAction_->setText(tr("Open"));
    openAction_->setToolTip(tr("Open one or more LAS or LAZ datasets"));
    addPointCloudAction_->setText(tr("Add LAS Files"));
    addPointCloudAction_->setToolTip(tr("Add one or more LAS or LAZ datasets to the current project"));
    removeDatasetAction_->setText(tr("Remove Selected Dataset"));
    removeDatasetAction_->setToolTip(tr("Remove the selected LAS or LAZ dataset from the project"));
    locateDatasetAction_->setText(tr("Open Folder"));
    locateDatasetAction_->setToolTip(tr("Open the folder that contains the selected dataset"));
    copyDatasetPathAction_->setText(tr("Copy Path"));
    copyDatasetPathAction_->setToolTip(tr("Copy the full path of the selected dataset"));
    expandProjectTreeAction_->setText(tr("Expand All"));
    expandProjectTreeAction_->setToolTip(tr("Expand the project explorer tree"));
    collapseProjectTreeAction_->setText(tr("Collapse All"));
    collapseProjectTreeAction_->setToolTip(tr("Collapse the project explorer tree"));
    openProjectAction_->setText(tr("Open Project"));
    saveProjectAction_->setText(tr("Save Project"));
    saveProjectAsAction_->setText(tr("Save Project As"));
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
    exportClearanceCsvAction_->setText(tr("Export Clearance CSV"));
    showProfileDockAction_->setText(tr("Profile View"));
    showProfileDockAction_->setToolTip(tr("Show or hide the span profile dock"));
    startTowerEditAction_->setText(tr("Start Editing"));
    finishTowerEditAction_->setText(tr("Finish Editing"));
    addTowerAction_->setText(tr("Click To Add Tower"));
    insertTowerAction_->setText(tr("Insert Before Current"));
    moveTowerAction_->setText(tr("Move Current Tower"));
    focusTowerAction_->setText(tr("Focus Current Tower"));
    removeTowerAction_->setText(tr("Remove Current Tower"));
    clearTowersAction_->setText(tr("Clear Tower Markers"));
    cancelTowerToolAction_->setText(tr("Cancel Tower Tool"));
    showTowerXAction_->setText(tr("Show X"));
    showTowerYAction_->setText(tr("Show Y"));
    showTowerZAction_->setText(tr("Show Z"));
    startIssueMarkAction_->setText(tr("Mark Issue"));
    startIssueMarkAction_->setToolTip(tr("Click a point in the view to add an inspection issue"));
    cancelIssueToolAction_->setText(tr("Cancel Issue Tool"));
    focusIssueAction_->setText(tr("Focus Current Issue"));
    removeIssueAction_->setText(tr("Remove Current Issue"));
    clearIssuesAction_->setText(tr("Clear Issues"));
    exportIssuesCsvAction_->setText(tr("Export Issues CSV"));
    exportInspectionReportAction_->setText(tr("Export Inspection Report"));
    showLogAction_->setText(tr("Log"));
    showLogAction_->setToolTip(tr("Show or hide the log panel"));
    languageEnglishAction_->setText(QStringLiteral("English"));
    languageChineseAction_->setText(QStringLiteral("\u4e2d\u6587"));

    if (homePage_ != nullptr) {
        homePage_->setTitle(tr("Home"));
    }
    if (towerPage_ != nullptr) {
        towerPage_->setTitle(tr("Tower"));
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
    if (towerRibbonGroup_ != nullptr) {
        towerRibbonGroup_->setTitle(tr("Tower Editing"));
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

    if (projectDock_ != nullptr) {
        projectDock_->setWindowTitle(tr("Project Explorer"));
    }
    if (inspectorDock_ != nullptr) {
        inspectorDock_->setWindowTitle(tr("Scene Inspector"));
    }
    if (profileDock_ != nullptr) {
        profileDock_->setWindowTitle(tr("Span Profile"));
    }
    if (logDock_ != nullptr) {
        logDock_->setWindowTitle(tr("Application Log"));
    }
    if (inspectorTabWidget_ != nullptr) {
        inspectorTabWidget_->setTabText(0, tr("Overview"));
        inspectorTabWidget_->setTabText(1, tr("Tower"));
        inspectorTabWidget_->setTabText(2, tr("Issues"));
        inspectorTabWidget_->setTabText(3, tr("Rendering"));
        inspectorTabWidget_->setTabText(4, tr("Measurement"));
        inspectorTabWidget_->setTabText(5, tr("Navigation"));
    }
    if (datasetGroupBox_ != nullptr) {
        datasetGroupBox_->setTitle(tr("Dataset Summary"));
    }
    if (projectSearchEdit_ != nullptr) {
        projectSearchEdit_->setPlaceholderText(tr("Filter datasets or folders"));
    }
    if (towerTableWidget_ != nullptr) {
        towerTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Name"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z") });
    }
    if (issueTableWidget_ != nullptr) {
        issueTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Title"), tr("Severity"), tr("Status"), tr("Tower"), tr("Category") });
    }
    if (towerMenuButton_ != nullptr) {
        towerMenuButton_->setText(tr("Menu"));
    }
    if (issueMenuButton_ != nullptr) {
        issueMenuButton_->setText(tr("Menu"));
    }
    if (renderingGroupBox_ != nullptr) {
        renderingGroupBox_->setTitle(tr("Rendering Controls"));
    }
    if (measurementGroupBox_ != nullptr) {
        measurementGroupBox_->setTitle(tr("Measurement"));
    }
    if (clearanceGroupBox_ != nullptr) {
        clearanceGroupBox_->setTitle(tr("Clearance Analysis"));
    }
    if (clearanceSegmentsGroupBox_ != nullptr) {
        clearanceSegmentsGroupBox_->setTitle(tr("Path Segment Details"));
    }
    if (navigationGroupBox_ != nullptr) {
        navigationGroupBox_->setTitle(tr("Navigation"));
    }
    if (towerDetailsGroupBox_ != nullptr) {
        towerDetailsGroupBox_->setTitle(tr("Selected Tower Details"));
    }
    if (issueDetailsGroupBox_ != nullptr) {
        issueDetailsGroupBox_->setTitle(tr("Selected Issue Details"));
    }
    if (projectSearchEdit_ != nullptr) {
        projectSearchEdit_->setPlaceholderText(tr("Filter datasets or folders"));
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
    setFieldLabel(towerDetailsLayout_, towerCodeEdit_, tr("Code"));
    setFieldLabel(towerDetailsLayout_, towerLineNameEdit_, tr("Line"));
    setFieldLabel(towerDetailsLayout_, towerVoltageLevelEdit_, tr("Voltage"));
    setFieldLabel(towerDetailsLayout_, towerStructureTypeEdit_, tr("Tower Type"));
    setFieldLabel(towerDetailsLayout_, towerInspectionDateEdit_, tr("Inspection Date"));
    setFieldLabel(towerDetailsLayout_, towerStatusEdit_, tr("Tower Status"));
    setFieldLabel(towerDetailsLayout_, towerNotesEdit_, tr("Notes"));
    setFieldLabel(issueDetailsLayout_, issueTitleEdit_, tr("Title"));
    setFieldLabel(issueDetailsLayout_, issueCategoryComboBox_, tr("Category"));
    setFieldLabel(issueDetailsLayout_, issueSeverityComboBox_, tr("Severity"));
    setFieldLabel(issueDetailsLayout_, issueStatusComboBox_, tr("Issue Status"));
    setFieldLabel(issueDetailsLayout_, issueRelatedTowerComboBox_, tr("Related Tower"));
    setFieldLabel(issueDetailsLayout_, issueImagePathEdit_, tr("Image Path"));
    setFieldLabel(issueDetailsLayout_, issueLocationValueLabel_, tr("Location"));
    setFieldLabel(issueDetailsLayout_, issueCreatedAtValueLabel_, tr("Created At"));
    setFieldLabel(issueDetailsLayout_, issueDescriptionEdit_, tr("Description"));
    if (issueCategoryComboBox_ != nullptr && issueCategoryComboBox_->count() >= 5) {
        issueCategoryComboBox_->setItemText(0, tr("Vegetation"));
        issueCategoryComboBox_->setItemText(1, tr("Insulator"));
        issueCategoryComboBox_->setItemText(2, tr("Tower Body"));
        issueCategoryComboBox_->setItemText(3, tr("Channel Risk"));
        issueCategoryComboBox_->setItemText(4, tr("Other"));
    }
    if (issueSeverityComboBox_ != nullptr && issueSeverityComboBox_->count() >= 4) {
        issueSeverityComboBox_->setItemText(0, issueSeverityDisplayName(IssueSeverity::Info));
        issueSeverityComboBox_->setItemText(1, issueSeverityDisplayName(IssueSeverity::Minor));
        issueSeverityComboBox_->setItemText(2, issueSeverityDisplayName(IssueSeverity::Major));
        issueSeverityComboBox_->setItemText(3, issueSeverityDisplayName(IssueSeverity::Critical));
    }
    if (issueStatusComboBox_ != nullptr && issueStatusComboBox_->count() >= 3) {
        issueStatusComboBox_->setItemText(0, issueStatusDisplayName(IssueStatus::Open));
        issueStatusComboBox_->setItemText(1, issueStatusDisplayName(IssueStatus::Monitoring));
        issueStatusComboBox_->setItemText(2, issueStatusDisplayName(IssueStatus::Resolved));
    }

    setFieldLabel(renderingLayout_, pointSizeControl_, tr("Point Size"));
    setFieldLabel(renderingLayout_, pointOpacityControl_, tr("Point Opacity"));
    setFieldLabel(renderingLayout_, depthCueControl_, tr("Depth Cue"));
    setFieldLabel(renderingLayout_, edlStrengthControl_, tr("EDL-style Shading"));
    setFieldLabel(renderingLayout_, colorModeComboBox_, tr("Color Mode"));
    setFieldLabel(renderingLayout_, pointColorButton_, tr("Single Color"));
    setFieldLabel(renderingLayout_, backgroundColorButton_, tr("Background"));
    colorModeComboBox_->setItemText(0, tr("RGB"));
    colorModeComboBox_->setItemText(1, tr("Elevation Ramp"));
    colorModeComboBox_->setItemText(2, tr("Single Color"));
    roundSplatsCheckBox_->setText(tr("Round splats (survey style)"));
    axesCheckBox_->setText(tr("Show XYZ axes"));
    boundingBoxCheckBox_->setText(tr("Show bounding box"));
    updateSliderValueLabel(pointSizeSlider_, pointSizeValueLabel_, tr("%1 px"));
    updateSliderValueLabel(pointOpacitySlider_, pointOpacityValueLabel_, tr("%1%"));
    updateSliderValueLabel(depthCueSlider_, depthCueValueLabel_, tr("%1%"));
    updateSliderValueLabel(edlStrengthSlider_, edlStrengthValueLabel_, tr("%1%"));
    updateVisualizationTooltips();

    setFieldLabel(measurementLayout_, measurementStartValueLabel_, tr("Start Point"));
    setFieldLabel(measurementLayout_, measurementEndValueLabel_, tr("End Point"));
    setFieldLabel(measurementLayout_, measurementDistanceValueLabel_, tr("3D Distance"));
    setFieldLabel(measurementLayout_, measurementHorizontalDistanceValueLabel_, tr("Horizontal Distance"));
    setFieldLabel(measurementLayout_, measurementDeltaZValueLabel_, tr("Height Delta"));
    setFieldLabel(measurementLayout_, measurementSegmentsValueLabel_, tr("Path Segments"));
    setFieldLabel(clearanceLayout_, clearanceThresholdSpinBox_, tr("Warning Threshold"));
    setFieldLabel(clearanceLayout_, clearanceShortestValueLabel_, tr("Shortest Segment"));
    setFieldLabel(clearanceLayout_, clearanceWarningCountValueLabel_, tr("Warning Segments"));
    setFieldLabel(clearanceLayout_, clearanceStatusValueLabel_, tr("Status"));
    if (clearanceSegmentsSummaryLabel_ != nullptr) {
        clearanceSegmentsSummaryLabel_->setText(tr("Add at least two measured points to list corridor segments and export clearance details."));
    }
    if (clearanceSegmentsTableWidget_ != nullptr) {
        clearanceSegmentsTableWidget_->setHorizontalHeaderLabels({
            tr("Segment"),
            tr("From"),
            tr("To"),
            tr("Chainage"),
            tr("Horizontal"),
            tr("3D"),
            tr("dZ"),
            tr("Status")
        });
    }
    measurementToggleButton_->setText(
        viewer_ != nullptr && viewer_->measurementEnabled() ? tr("Stop Measurement") : tr("Start Measurement"));
    measurementClearButton_->setText(tr("Clear Measurement"));
    if (clearanceThresholdSpinBox_ != nullptr) {
        clearanceThresholdSpinBox_->setSuffix(tr(" m"));
        clearanceThresholdSpinBox_->setSpecialValueText(tr("Disabled"));
    }

    invertOrbitCheckBox_->setText(tr("Invert orbit drag"));
    invertPanCheckBox_->setText(tr("Invert pan drag"));
    invertWheelCheckBox_->setText(tr("Invert wheel zoom"));

    if (profileDock_ != nullptr) {
        if (auto* titleLabel = profileDock_->findChild<QLabel*>(QStringLiteral("spanProfileTitleLabel"))) {
            titleLabel->setText(tr("Measured corridor profile"));
        }
        if (auto* subtitleLabel = profileDock_->findChild<QLabel*>(QStringLiteral("spanProfileSubtitleLabel"))) {
            subtitleLabel->setText(tr("The profile updates from the current measurement path, highlights clearance segments below the threshold, and overlays nearby towers and issues."));
        }
    }

    if (viewer_ != nullptr) {
        setColorButtonAppearance(pointColorButton_, viewer_->visualizationOptions().singleColor, tr("Pick Color"));
        setColorButtonAppearance(backgroundColorButton_, viewer_->visualizationOptions().backgroundColor, tr("Pick Background"));
    }
    updateWindowControlButtons();
    rebuildProjectTree();
    updateDatasetPanel();
    updateNavigationHelpText();
    updateMeasurementPanel();
    updateTowerPanel();
    updateActionState();
    if (viewer_ != nullptr) {
        viewer_->update();
    }
}

void MainWindow::createConnections()
{
    connect(openAction_, &QAction::triggered, this, [this]() { openPointCloud(); });
    connect(addPointCloudAction_, &QAction::triggered, this, [this]() { addPointCloudFiles(); });
    connect(removeDatasetAction_, &QAction::triggered, this, [this]() { removeSelectedDataset(); });
    connect(locateDatasetAction_, &QAction::triggered, this, [this]() {
        const QString datasetPath = selectedDatasetPath();
        if (datasetPath.isEmpty()) {
            return;
        }

        const QString folderPath = QFileInfo(datasetPath).absolutePath();
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath))) {
            showUserMessage(LogLevel::Warning, tr("Unable to open the dataset folder."), 3000);
        }
    });
    connect(copyDatasetPathAction_, &QAction::triggered, this, [this]() {
        const QString datasetPath = selectedDatasetPath();
        if (datasetPath.isEmpty()) {
            return;
        }

        if (QGuiApplication::clipboard() != nullptr) {
            QGuiApplication::clipboard()->setText(datasetPath);
            showUserMessage(LogLevel::Info, tr("Dataset path copied."), 2000);
        }
    });
    connect(expandProjectTreeAction_, &QAction::triggered, this, [this]() {
        if (projectTreeWidget_ != nullptr) {
            projectTreeWidget_->expandAll();
        }
    });
    connect(collapseProjectTreeAction_, &QAction::triggered, this, [this]() {
        if (projectTreeWidget_ != nullptr) {
            projectTreeWidget_->collapseAll();
            if (projectTreeWidget_->topLevelItemCount() > 0) {
                projectTreeWidget_->topLevelItem(0)->setExpanded(true);
            }
        }
    });
    connect(openProjectAction_, &QAction::triggered, this, [this]() { openProject(); });
    connect(saveProjectAction_, &QAction::triggered, this, [this]() { saveProject(); });
    connect(saveProjectAsAction_, &QAction::triggered, this, [this]() { saveProjectAs(); });
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
    connect(exportClearanceCsvAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        const ClearanceAnalysisResult analysisResult = analyzeClearancePath(
            viewer_->measurementResult().points,
            static_cast<float>(clearanceWarningThresholdMeters_));
        if (!analysisResult.isValid()) {
            showUserMessage(LogLevel::Warning, tr("Add at least two measured points before exporting clearance details."), 3000);
            return;
        }

        const QString filePath = QFileDialog::getSaveFileName(
            this,
            tr("Export Clearance CSV"),
            QStringLiteral("clearance_segments.csv"),
            tr("CSV Files (*.csv)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        if (!ClearanceReportExporter::exportSegmentsCsv(filePath, analysisResult, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        showUserMessage(LogLevel::Info, tr("Clearance CSV exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    });
    connect(showProfileDockAction_, &QAction::toggled, this, [this](bool visible) {
        if (profileDock_ != nullptr && profileDock_->isVisible() != visible) {
            profileDock_->setVisible(visible);
        }
    });

    connect(pointSizeSlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setPointSize);
    connect(pointOpacitySlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setPointOpacity);
    connect(depthCueSlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setDepthCueStrength);
    connect(edlStrengthSlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setEdlStrength);
    connect(pointSizeSlider_, &QSlider::valueChanged, this, [this](int) {
        updateSliderValueLabel(pointSizeSlider_, pointSizeValueLabel_, tr("%1 px"));
    });
    connect(pointOpacitySlider_, &QSlider::valueChanged, this, [this](int) {
        updateSliderValueLabel(pointOpacitySlider_, pointOpacityValueLabel_, tr("%1%"));
    });
    connect(depthCueSlider_, &QSlider::valueChanged, this, [this](int) {
        updateSliderValueLabel(depthCueSlider_, depthCueValueLabel_, tr("%1%"));
    });
    connect(edlStrengthSlider_, &QSlider::valueChanged, this, [this](int) {
        updateSliderValueLabel(edlStrengthSlider_, edlStrengthValueLabel_, tr("%1%"));
    });
    connect(
        colorModeComboBox_,
        qOverload<int>(&QComboBox::currentIndexChanged),
        viewer_,
        static_cast<void (PointCloudViewer::*)(int)>(&PointCloudViewer::setColorMode));
    connect(pointColorButton_, &QPushButton::clicked, this, [this]() { choosePointColor(); });
    connect(backgroundColorButton_, &QPushButton::clicked, this, [this]() { chooseBackgroundColor(); });
    connect(roundSplatsCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setUseRoundSplats);
    connect(axesCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setShowAxes);
    connect(boundingBoxCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setShowBoundingBox);
    connect(invertOrbitCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertOrbitDrag);
    connect(invertPanCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertPanDrag);
    connect(invertWheelCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertWheelZoom);
    connect(measurementToggleButton_, &QPushButton::clicked, this, [this]() {
        viewer_->setMeasurementEnabled(!viewer_->measurementEnabled());
    });
    connect(measurementClearButton_, &QPushButton::clicked, viewer_, &PointCloudViewer::clearMeasurement);
    connect(clearanceThresholdSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        clearanceWarningThresholdMeters_ = value;
        persistMeasurementSettings();
        updateMeasurementPanel();
    });
    connect(clearanceSegmentsTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
        if (profilePlotWidget_ != nullptr) {
            profilePlotWidget_->setSelectedSegmentIndex(currentRow);
        }
    });

    const auto beginAddTower = [this]() {
        if (!towerEditingEnabled_) {
            showUserMessage(LogLevel::Warning, tr("Start tower editing before using tower tools."), 3000);
            return;
        }
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before adding tower markers."), 3000);
            return;
        }
        viewer_->beginTowerAddMode();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower add mode enabled. Click points continuously to add tower markers, or cancel the tool when finished."), 4500);
    };
    const auto beginInsertTower = [this]() {
        if (!towerEditingEnabled_) {
            showUserMessage(LogLevel::Warning, tr("Start tower editing before using tower tools."), 3000);
            return;
        }
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before inserting tower markers."), 3000);
            return;
        }

        const int currentRow = viewer_ != nullptr ? viewer_->selectedTowerIndex() : -1;
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            showUserMessage(LogLevel::Warning, tr("Select the current tower before inserting a new one."), 3000);
            return;
        }

        viewer_->setSelectedTowerIndex(currentRow);
        viewer_->beginTowerInsertMode(currentRow);
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Click a point in the view to insert a tower marker before the current one."), 4000);
    };
    const auto beginMoveTower = [this]() {
        if (!towerEditingEnabled_) {
            showUserMessage(LogLevel::Warning, tr("Start tower editing before using tower tools."), 3000);
            return;
        }
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before moving tower markers."), 3000);
            return;
        }

        const int currentRow = viewer_ != nullptr ? viewer_->selectedTowerIndex() : -1;
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            showUserMessage(LogLevel::Warning, tr("Select the current tower before moving it."), 3000);
            return;
        }

        viewer_->setSelectedTowerIndex(currentRow);
        viewer_->beginTowerMoveMode(currentRow);
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Click a point in the view to move the current tower marker."), 4000);
    };
    const auto focusSelectedTower = [this]() {
        if (viewer_ == nullptr || towerTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = towerTableWidget_->currentRow();
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            return;
        }

        viewer_->focusOnPoint(viewer_->towerMarkers().at(currentRow).point);
    };
    const auto removeSelectedTower = [this]() {
        if (!towerEditingEnabled_) {
            showUserMessage(LogLevel::Warning, tr("Start tower editing before removing tower markers."), 3000);
            return;
        }
        if (viewer_ == nullptr || towerTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = towerTableWidget_->currentRow();
        if (!viewer_->removeTowerMarker(currentRow)) {
            return;
        }

        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower marker removed."), 2500);
    };
    const auto clearAllTowers = [this]() {
        if (!towerEditingEnabled_) {
            showUserMessage(LogLevel::Warning, tr("Start tower editing before clearing tower markers."), 3000);
            return;
        }
        if (viewer_ == nullptr || viewer_->towerMarkers().isEmpty()) {
            return;
        }

        viewer_->clearTowerMarkers();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower markers cleared."), 2500);
    };
    const auto cancelTowerTool = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        viewer_->cancelTowerEditMode();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower tool cancelled."), 2500);
    };
    const auto startTowerEditing = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before editing tower markers."), 3000);
            return;
        }

        setTowerEditingEnabled(true);
        if (inspectorTabWidget_ != nullptr) {
            inspectorTabWidget_->setCurrentIndex(1);
        }
        showUserMessage(LogLevel::Info, tr("Tower editing started. Use the tools in the right dock to add, insert, move, rename, or remove tower markers."), 4500);
    };
    const auto finishTowerEditing = [this]() {
        setTowerEditingEnabled(false);
        showUserMessage(LogLevel::Info, tr("Tower editing finished."), 2500);
    };

    connect(startTowerEditAction_, &QAction::triggered, this, startTowerEditing);
    connect(finishTowerEditAction_, &QAction::triggered, this, finishTowerEditing);
    connect(addTowerAction_, &QAction::triggered, this, beginAddTower);
    connect(insertTowerAction_, &QAction::triggered, this, beginInsertTower);
    connect(moveTowerAction_, &QAction::triggered, this, beginMoveTower);
    connect(focusTowerAction_, &QAction::triggered, this, focusSelectedTower);
    connect(removeTowerAction_, &QAction::triggered, this, removeSelectedTower);
    connect(clearTowersAction_, &QAction::triggered, this, clearAllTowers);
    connect(cancelTowerToolAction_, &QAction::triggered, this, cancelTowerTool);
    connect(showTowerXAction_, &QAction::toggled, this, [this](bool checked) {
        if (towerTableWidget_ != nullptr) {
            towerTableWidget_->setColumnHidden(2, !checked);
        }
    });
    connect(showTowerYAction_, &QAction::toggled, this, [this](bool checked) {
        if (towerTableWidget_ != nullptr) {
            towerTableWidget_->setColumnHidden(3, !checked);
        }
    });
    connect(showTowerZAction_, &QAction::toggled, this, [this](bool checked) {
        if (towerTableWidget_ != nullptr) {
            towerTableWidget_->setColumnHidden(4, !checked);
        }
    });
    connect(towerTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
        if (viewer_ != nullptr) {
            viewer_->setSelectedTowerIndex(currentRow);
        }
        updateActionState();
        updateTowerPanel();
    });
    connect(towerTableWidget_, &QTableWidget::cellChanged, this, [this](int row, int column) {
        if (viewer_ == nullptr || towerTableWidget_ == nullptr || column != 1) {
            return;
        }

        if (!towerEditingEnabled_) {
            updateTowerPanel();
            return;
        }

        QTableWidgetItem* item = towerTableWidget_->item(row, column);
        if (item == nullptr) {
            return;
        }

        if (!viewer_->setTowerMarkerName(row, item->text())) {
            showUserMessage(LogLevel::Warning, tr("Tower marker name cannot be empty."), 3000);
            updateTowerPanel();
            return;
        }

        updateTowerPanel();
    });
    const auto commitTowerDetails = [this]() {
        if (updatingTowerDetails_ || viewer_ == nullptr) {
            return;
        }

        const int selectedTowerIndex = viewer_->selectedTowerIndex();
        if (selectedTowerIndex < 0 || selectedTowerIndex >= viewer_->towerMarkers().size()) {
            return;
        }

        TowerRecord towerRecord = viewer_->towerMarkers().at(selectedTowerIndex);
        towerRecord.code = towerCodeEdit_->text().trimmed();
        towerRecord.lineName = towerLineNameEdit_->text().trimmed();
        towerRecord.voltageLevel = towerVoltageLevelEdit_->text().trimmed();
        towerRecord.structureType = towerStructureTypeEdit_->text().trimmed();
        towerRecord.inspectionDate = towerInspectionDateEdit_->text().trimmed();
        towerRecord.status = towerStatusEdit_->text().trimmed();
        towerRecord.notes = towerNotesEdit_->toPlainText().trimmed();
        if (viewer_->setTowerRecord(selectedTowerIndex, towerRecord)) {
            updateTowerPanel();
        }
    };
    connect(towerCodeEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerLineNameEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerVoltageLevelEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerStructureTypeEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerInspectionDateEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerStatusEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerNotesEdit_, &QPlainTextEdit::textChanged, this, commitTowerDetails);

    const auto beginIssueMarking = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before marking issues."), 3000);
            return;
        }

        viewer_->beginIssueAddMode();
        if (inspectorTabWidget_ != nullptr) {
            inspectorTabWidget_->setCurrentIndex(2);
        }
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Issue marking enabled. Click a point in the view to add an issue, or right-click to cancel."), 4500);
    };
    const auto cancelIssueTool = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        viewer_->cancelIssueEditMode();
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Issue tool cancelled."), 2500);
    };
    const auto focusSelectedIssue = [this]() {
        if (viewer_ == nullptr || issueTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = issueTableWidget_->currentRow();
        if (currentRow < 0 || currentRow >= viewer_->inspectionIssues().size()) {
            return;
        }

        viewer_->focusOnPoint(viewer_->inspectionIssues().at(currentRow).point, 0.2);
    };
    const auto removeSelectedIssue = [this]() {
        if (viewer_ == nullptr || issueTableWidget_ == nullptr) {
            return;
        }

        if (viewer_->removeInspectionIssue(issueTableWidget_->currentRow())) {
            updateIssuePanel();
            showUserMessage(LogLevel::Info, tr("Inspection issue removed."), 2500);
        }
    };
    const auto clearAllIssues = [this]() {
        if (viewer_ == nullptr || viewer_->inspectionIssues().isEmpty()) {
            return;
        }

        viewer_->clearInspectionIssues();
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Inspection issues cleared."), 2500);
    };
    const auto exportIssuesCsv = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        const QString filePath = QFileDialog::getSaveFileName(
            this,
            tr("Export Issues CSV"),
            QStringLiteral("inspection_issues.csv"),
            tr("CSV Files (*.csv)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        if (!InspectionReportExporter::exportIssuesCsv(filePath, viewer_->inspectionIssues(), &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }
        showUserMessage(LogLevel::Info, tr("Issue CSV exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    };
    const auto exportInspectionReport = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            return;
        }

        const QString filePath = QFileDialog::getSaveFileName(
            this,
            tr("Export Inspection Report"),
            QStringLiteral("inspection_report.html"),
            tr("HTML Files (*.html)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        const QString projectName = currentProjectFilePath_.isEmpty()
            ? tr("Current Project")
            : QFileInfo(currentProjectFilePath_).completeBaseName();
        if (!InspectionReportExporter::exportProjectHtml(
                filePath,
                projectName,
                viewer_->currentFilePaths(),
                viewer_->towerMarkers(),
                viewer_->inspectionIssues(),
                &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        showUserMessage(LogLevel::Info, tr("Inspection report exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    };
    connect(startIssueMarkAction_, &QAction::triggered, this, beginIssueMarking);
    connect(cancelIssueToolAction_, &QAction::triggered, this, cancelIssueTool);
    connect(focusIssueAction_, &QAction::triggered, this, focusSelectedIssue);
    connect(removeIssueAction_, &QAction::triggered, this, removeSelectedIssue);
    connect(clearIssuesAction_, &QAction::triggered, this, clearAllIssues);
    connect(exportIssuesCsvAction_, &QAction::triggered, this, exportIssuesCsv);
    connect(exportInspectionReportAction_, &QAction::triggered, this, exportInspectionReport);
    connect(issueTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
        if (viewer_ != nullptr) {
            viewer_->setSelectedIssueIndex(currentRow);
        }
        updateActionState();
        updateIssuePanel();
    });
    const auto commitIssueDetails = [this]() {
        if (updatingIssueDetails_ || viewer_ == nullptr) {
            return;
        }

        const int selectedIssueIndex = viewer_->selectedIssueIndex();
        if (selectedIssueIndex < 0 || selectedIssueIndex >= viewer_->inspectionIssues().size()) {
            return;
        }

        InspectionIssue issue = viewer_->inspectionIssues().at(selectedIssueIndex);
        issue.title = issueTitleEdit_->text().trimmed();
        issue.category = issueCategoryComboBox_->currentText().trimmed();
        issue.severity = static_cast<IssueSeverity>(issueSeverityComboBox_->currentIndex());
        issue.status = static_cast<IssueStatus>(issueStatusComboBox_->currentIndex());
        issue.relatedTowerIndex = issueRelatedTowerComboBox_->currentData().toInt();
        issue.relatedTowerName = issueRelatedTowerComboBox_->currentText().trimmed();
        issue.imagePath = issueImagePathEdit_->text().trimmed();
        issue.description = issueDescriptionEdit_->toPlainText().trimmed();
        if (viewer_->updateInspectionIssue(selectedIssueIndex, issue)) {
            updateIssuePanel();
        }
    };
    connect(issueTitleEdit_, &QLineEdit::editingFinished, this, commitIssueDetails);
    connect(issueCategoryComboBox_, &QComboBox::editTextChanged, this, [commitIssueDetails](const QString&) { commitIssueDetails(); });
    connect(issueSeverityComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [commitIssueDetails](int) { commitIssueDetails(); });
    connect(issueStatusComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [commitIssueDetails](int) { commitIssueDetails(); });
    connect(issueRelatedTowerComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [commitIssueDetails](int) { commitIssueDetails(); });
    connect(issueImagePathEdit_, &QLineEdit::editingFinished, this, commitIssueDetails);
    connect(issueDescriptionEdit_, &QPlainTextEdit::textChanged, this, commitIssueDetails);
    connect(projectSearchEdit_, &QLineEdit::textChanged, this, [this](const QString&) {
        refreshProjectTreeFilter();
    });
    connect(projectTreeWidget_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem*, QTreeWidgetItem*) {
        updateActionState();
    });
    connect(projectTreeWidget_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (item == nullptr || item->data(0, kProjectTreeItemTypeRole).toString() != QStringLiteral("dataset")) {
            return;
        }

        locateDatasetAction_->trigger();
    });
    connect(projectTreeWidget_, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (projectTreeWidget_ == nullptr) {
            return;
        }

        if (QTreeWidgetItem* item = projectTreeWidget_->itemAt(pos)) {
            projectTreeWidget_->setCurrentItem(item);
        }

        QMenu menu(projectTreeWidget_);
        menu.addAction(openAction_);
        menu.addAction(addPointCloudAction_);
        menu.addAction(removeDatasetAction_);
        menu.addSeparator();
        menu.addAction(locateDatasetAction_);
        menu.addAction(copyDatasetPathAction_);
        menu.addSeparator();
        menu.addAction(expandProjectTreeAction_);
        menu.addAction(collapseProjectTreeAction_);
        menu.exec(projectTreeWidget_->viewport()->mapToGlobal(pos));
    });
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
    connect(profileDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (showProfileDockAction_ != nullptr && showProfileDockAction_->isChecked() != visible) {
            showProfileDockAction_->setChecked(visible);
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
        setTowerEditingEnabled(false);
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
    connect(viewer_, &PointCloudViewer::towerMarkersChanged, this, [this]() {
        syncUiFromViewer();
        updateMeasurementPanel();
        updateTowerPanel();
    });
    connect(viewer_, &PointCloudViewer::selectedTowerChanged, this, [this](int index) {
        if (towerTableWidget_ != nullptr && towerTableWidget_->currentRow() != index) {
            const QSignalBlocker blocker(towerTableWidget_);
            if (index >= 0) {
                towerTableWidget_->setCurrentCell(index, 1);
            } else {
                towerTableWidget_->clearSelection();
                towerTableWidget_->setCurrentItem(nullptr);
            }
        }
        updateActionState();
        updateMeasurementPanel();
        updateTowerPanel();
    });
    connect(viewer_, &PointCloudViewer::towerEditModeChanged, this, [this]() {
        updateTowerPanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::towerEditRequested, this, [this](const PointRecord& point, int modeValue, int targetIndex) {
        const TowerEditMode mode = static_cast<TowerEditMode>(modeValue);
        if (viewer_ == nullptr) {
            return;
        }

        if (mode == TowerEditMode::MoveSelected) {
            if (viewer_->moveTowerMarker(targetIndex, point)) {
                updateTowerPanel();
                showUserMessage(LogLevel::Info, tr("Tower marker moved."), 2500);
            }
            return;
        }

        const QString towerName = nextDefaultTowerName();

        const bool inserted = mode == TowerEditMode::InsertBeforeSelected
            ? viewer_->insertTowerMarker(targetIndex, towerName, point)
            : viewer_->addTowerMarker(towerName, point);
        if (!inserted) {
            showUserMessage(LogLevel::Warning, tr("Tower marker name cannot be empty."), 3000);
            return;
        }

        updateTowerPanel();
        showUserMessage(
            LogLevel::Info,
            mode == TowerEditMode::AddAfterLast
                ? tr("Tower marker added. Continue clicking points to add more, or cancel the tool when finished.")
                : tr("Tower marker added."),
            mode == TowerEditMode::AddAfterLast ? 3500 : 2500);
    });
    connect(viewer_, &PointCloudViewer::inspectionIssuesChanged, this, [this]() {
        syncUiFromViewer();
        updateMeasurementPanel();
        updateIssuePanel();
    });
    connect(viewer_, &PointCloudViewer::selectedIssueChanged, this, [this](int index) {
        if (issueTableWidget_ != nullptr && issueTableWidget_->currentRow() != index) {
            const QSignalBlocker blocker(issueTableWidget_);
            if (index >= 0) {
                issueTableWidget_->setCurrentCell(index, 1);
            } else {
                issueTableWidget_->clearSelection();
                issueTableWidget_->setCurrentItem(nullptr);
            }
        }
        updateActionState();
        updateMeasurementPanel();
        updateIssuePanel();
    });
    connect(viewer_, &PointCloudViewer::issueEditModeChanged, this, [this]() {
        updateIssuePanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::issueEditRequested, this, [this](const PointRecord& point) {
        if (viewer_ == nullptr) {
            return;
        }

        InspectionIssue issue;
        issue.id = issueDefaultId();
        issue.title = nextDefaultIssueTitle();
        issue.category = tr("Other");
        issue.severity = IssueSeverity::Major;
        issue.status = IssueStatus::Open;
        issue.point = point;
        issue.relatedTowerIndex = viewer_->selectedTowerIndex();
        if (issue.relatedTowerIndex >= 0 && issue.relatedTowerIndex < viewer_->towerMarkers().size()) {
            issue.relatedTowerName = viewer_->towerMarkers().at(issue.relatedTowerIndex).name;
        }
        issue.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
        if (viewer_->addInspectionIssue(issue)) {
            if (inspectorTabWidget_ != nullptr) {
                inspectorTabWidget_->setCurrentIndex(2);
            }
            updateIssuePanel();
            showUserMessage(LogLevel::Info, tr("Inspection issue added. Continue clicking points to add more, or right-click to cancel."), 3500);
        }
    });
    connect(viewer_, &PointCloudViewer::measurementMessage, this, [this](const QString& message, bool error) {
        showUserMessage(error ? LogLevel::Error : LogLevel::Info, message, error ? 4000 : 3000);
    });
}

void MainWindow::openProject()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open Project"),
        QString(),
        tr("LiDAR Power Projects (*.json *.lpproj);;JSON Files (*.json);;All Files (*.*)"));

    if (filePath.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Open project cancelled."), 2000);
        return;
    }

    loadProjectFile(filePath);
}

void MainWindow::saveProject()
{
    if (currentProjectFilePath_.isEmpty()) {
        saveProjectAs();
        return;
    }

    saveProjectFile(currentProjectFilePath_);
}

void MainWindow::saveProjectAs()
{
    const QString initialPath = currentProjectFilePath_.isEmpty()
        ? QStringLiteral("project.lpproj")
        : currentProjectFilePath_;
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Project"),
        initialPath,
        tr("LiDAR Power Projects (*.lpproj);;JSON Files (*.json)"));

    if (filePath.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Save project cancelled."), 2000);
        return;
    }

    QString normalizedPath = filePath;
    if (QFileInfo(normalizedPath).suffix().isEmpty()) {
        normalizedPath += QStringLiteral(".lpproj");
    }

    saveProjectFile(normalizedPath);
}

bool MainWindow::loadProjectFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        showUserMessage(LogLevel::Error, tr("Failed to open project file."), 5000);
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) {
        showUserMessage(LogLevel::Error, tr("Project file format is invalid."), 5000);
        return false;
    }

    const QJsonObject projectObject = document.object();
    QStringList pointCloudFilePaths;
    const QJsonArray pointCloudFilesArray = projectObject.value(QStringLiteral("pointCloudFilePaths")).toArray();
    for (const QJsonValue& pathValue : pointCloudFilesArray) {
        const QString resolvedPath = resolveProjectPath(filePath, pathValue.toString());
        if (!resolvedPath.isEmpty()) {
            pointCloudFilePaths.append(resolvedPath);
        }
    }
    if (pointCloudFilePaths.isEmpty()) {
        const QString legacyPointCloudPath = resolveProjectPath(
            filePath,
            projectObject.value(QStringLiteral("pointCloudFilePath")).toString());
        if (!legacyPointCloudPath.isEmpty()) {
            pointCloudFilePaths.append(legacyPointCloudPath);
        }
    }
    if (pointCloudFilePaths.isEmpty()) {
        showUserMessage(LogLevel::Error, tr("Project file does not contain any point cloud paths."), 5000);
        return false;
    }

    QString errorMessage;
    if (!viewer_->loadPointCloudFiles(pointCloudFilePaths, &errorMessage)) {
        syncUiFromViewer();
        showUserMessage(LogLevel::Error, errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage, 6000);
        return false;
    }

    const QJsonObject visualizationObject = projectObject.value(QStringLiteral("visualization")).toObject();
    const PointCloudVisualizationOptions defaults = viewer_->visualizationOptions();
    viewer_->setPointSize(visualizationObject.value(QStringLiteral("pointSize")).toInt(static_cast<int>(defaults.pointSize)));
    viewer_->setPointOpacity(visualizationObject.value(QStringLiteral("pointOpacity")).toInt(static_cast<int>(defaults.pointOpacity * 100.0f)));
    viewer_->setDepthCueStrength(visualizationObject.value(QStringLiteral("depthCueStrength")).toInt(static_cast<int>(defaults.depthCueStrength * 100.0f)));
    viewer_->setEdlStrength(visualizationObject.value(QStringLiteral("edlStrength")).toInt(static_cast<int>(defaults.edlStrength * 100.0f)));
    viewer_->setColorMode(visualizationObject.value(QStringLiteral("colorMode")).toInt(static_cast<int>(defaults.colorMode)));
    viewer_->setSingleColor(colorFromJson(visualizationObject.value(QStringLiteral("singleColor")).toObject(), defaults.singleColor));
    viewer_->setBackgroundColor(colorFromJson(visualizationObject.value(QStringLiteral("backgroundColor")).toObject(), defaults.backgroundColor));
    viewer_->setUseRoundSplats(visualizationObject.value(QStringLiteral("useRoundSplats")).toBool(defaults.useRoundSplats));
    viewer_->setShowAxes(visualizationObject.value(QStringLiteral("showAxes")).toBool(defaults.showAxes));
    viewer_->setShowBoundingBox(visualizationObject.value(QStringLiteral("showBoundingBox")).toBool(defaults.showBoundingBox));

    InteractionOptions interactionOptions = viewer_->interactionOptions();
    const QJsonObject interactionObject = projectObject.value(QStringLiteral("interaction")).toObject();
    interactionOptions.invertOrbitDrag = interactionObject.value(QStringLiteral("invertOrbitDrag")).toBool(interactionOptions.invertOrbitDrag);
    interactionOptions.invertPanDrag = interactionObject.value(QStringLiteral("invertPanDrag")).toBool(interactionOptions.invertPanDrag);
    interactionOptions.invertWheelZoom = interactionObject.value(QStringLiteral("invertWheelZoom")).toBool(interactionOptions.invertWheelZoom);
    viewer_->setInteractionOptions(interactionOptions);

    const QJsonObject measurementObject = projectObject.value(QStringLiteral("measurement")).toObject();
    clearanceWarningThresholdMeters_ = measurementObject.value(QStringLiteral("clearanceThresholdMeters")).toDouble(clearanceWarningThresholdMeters_);
    if (clearanceThresholdSpinBox_ != nullptr) {
        const QSignalBlocker blocker(clearanceThresholdSpinBox_);
        clearanceThresholdSpinBox_->setValue(clearanceWarningThresholdMeters_);
    }

    QList<TowerMarker> towerMarkers;
    const QJsonArray towersArray = projectObject.value(QStringLiteral("towerMarkers")).toArray();
    for (const QJsonValue& towerValue : towersArray) {
        const TowerRecord towerRecord = towerRecordFromJson(towerValue.toObject());
        if (towerRecord.name.isEmpty()) {
            continue;
        }
        towerMarkers.append(towerRecord);
    }
    viewer_->setTowerMarkers(towerMarkers);

    QList<InspectionIssue> inspectionIssues;
    const QJsonArray issuesArray = projectObject.value(QStringLiteral("inspectionIssues")).toArray();
    for (const QJsonValue& issueValue : issuesArray) {
        const InspectionIssue issue = inspectionIssueFromJson(issueValue.toObject());
        if (issue.title.trimmed().isEmpty()) {
            continue;
        }
        inspectionIssues.append(issue);
    }
    viewer_->setInspectionIssues(inspectionIssues);

    currentProjectFilePath_ = filePath;
    setTowerEditingEnabled(false);
    const QString languageCode = projectObject.value(QStringLiteral("language")).toString();
    if (languageCode == QStringLiteral("zh_CN")) {
        applyLanguage(UiLanguage::Chinese);
    } else if (languageCode == QStringLiteral("en")) {
        applyLanguage(UiLanguage::English);
    }
    syncUiFromViewer();
    updateTowerPanel();
    showUserMessage(LogLevel::Info, tr("Project loaded: %1").arg(QFileInfo(filePath).fileName()), 4000);
    return true;
}

bool MainWindow::saveProjectFile(const QString& filePath)
{
    if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
        showUserMessage(LogLevel::Warning, tr("Load a point cloud before saving a project."), 4000);
        return false;
    }

    QJsonObject visualizationObject {
        { QStringLiteral("pointSize"), static_cast<int>(std::lround(viewer_->visualizationOptions().pointSize)) },
        { QStringLiteral("pointOpacity"), static_cast<int>(std::lround(viewer_->visualizationOptions().pointOpacity * 100.0f)) },
        { QStringLiteral("depthCueStrength"), static_cast<int>(std::lround(viewer_->visualizationOptions().depthCueStrength * 100.0f)) },
        { QStringLiteral("edlStrength"), static_cast<int>(std::lround(viewer_->visualizationOptions().edlStrength * 100.0f)) },
        { QStringLiteral("colorMode"), static_cast<int>(viewer_->visualizationOptions().colorMode) },
        { QStringLiteral("singleColor"), colorToJson(viewer_->visualizationOptions().singleColor) },
        { QStringLiteral("backgroundColor"), colorToJson(viewer_->visualizationOptions().backgroundColor) },
        { QStringLiteral("useRoundSplats"), viewer_->visualizationOptions().useRoundSplats },
        { QStringLiteral("showAxes"), viewer_->visualizationOptions().showAxes },
        { QStringLiteral("showBoundingBox"), viewer_->visualizationOptions().showBoundingBox }
    };

    QJsonObject interactionObject {
        { QStringLiteral("invertOrbitDrag"), viewer_->interactionOptions().invertOrbitDrag },
        { QStringLiteral("invertPanDrag"), viewer_->interactionOptions().invertPanDrag },
        { QStringLiteral("invertWheelZoom"), viewer_->interactionOptions().invertWheelZoom }
    };

    QJsonObject measurementObject {
        { QStringLiteral("clearanceThresholdMeters"), clearanceWarningThresholdMeters_ }
    };

    QJsonArray towersArray;
    for (const TowerMarker& towerMarker : viewer_->towerMarkers()) {
        towersArray.append(towerRecordToJson(towerMarker));
    }

    QJsonArray inspectionIssuesArray;
    for (const InspectionIssue& issue : viewer_->inspectionIssues()) {
        inspectionIssuesArray.append(inspectionIssueToJson(issue));
    }

    QJsonArray pointCloudFilesArray;
    for (const QString& pointCloudFilePath : viewer_->currentFilePaths()) {
        pointCloudFilesArray.append(projectRelativePathFor(filePath, pointCloudFilePath));
    }

    QJsonObject projectObject {
        { QStringLiteral("version"), 3 },
        { QStringLiteral("pointCloudFilePaths"), pointCloudFilesArray },
        { QStringLiteral("pointCloudFilePath"), viewer_->currentFilePath().isEmpty() ? QString() : projectRelativePathFor(filePath, viewer_->currentFilePath()) },
        { QStringLiteral("language"), languageCodeFor(currentLanguage_) },
        { QStringLiteral("visualization"), visualizationObject },
        { QStringLiteral("interaction"), interactionObject },
        { QStringLiteral("measurement"), measurementObject },
        { QStringLiteral("towerMarkers"), towersArray },
        { QStringLiteral("inspectionIssues"), inspectionIssuesArray }
    };

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        showUserMessage(LogLevel::Error, tr("Failed to save project file."), 5000);
        return false;
    }

    file.write(QJsonDocument(projectObject).toJson(QJsonDocument::Indented));
    file.close();
    currentProjectFilePath_ = filePath;
    rebuildProjectTree();
    showUserMessage(LogLevel::Info, tr("Project saved: %1").arg(QFileInfo(filePath).fileName()), 3000);
    return true;
}

void MainWindow::openPointCloud()
{
    const QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        tr("Open LAS Point Clouds"),
        QString(),
        tr("LAS Files (*.las *.laz);;All Files (*.*)"));

    if (filePaths.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Open cancelled."), 2000);
        return;
    }

    loadPointCloudFiles(filePaths);
}

void MainWindow::addPointCloudFiles()
{
    const QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        tr("Add LAS Point Clouds"),
        QString(),
        tr("LAS Files (*.las *.laz);;All Files (*.*)"));

    if (filePaths.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Add datasets cancelled."), 2000);
        return;
    }

    appendPointCloudFiles(filePaths);
}

bool MainWindow::loadPointCloudFile(const QString& filePath)
{
    return loadPointCloudFiles(QStringList { filePath });
}

bool MainWindow::loadPointCloudFiles(const QStringList& filePaths)
{
    if (viewer_ == nullptr) {
        return false;
    }

    QString errorMessage;
    if (viewer_->loadPointCloudFiles(filePaths, &errorMessage)) {
        currentProjectFilePath_.clear();
        setTowerEditingEnabled(false);
        const QString successMessage = filePaths.size() == 1
            ? tr("Loaded %1. %2").arg(QFileInfo(filePaths.constFirst()).fileName(), errorMessage)
            : tr("Loaded %1 datasets. %2")
                  .arg(QLocale().toString(filePaths.size()))
                  .arg(errorMessage);
        showUserMessage(LogLevel::Info, successMessage, 4500);
        return true;
    }

    syncUiFromViewer();
    showUserMessage(
        LogLevel::Error,
        errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
        6000);
    return false;
}

bool MainWindow::appendPointCloudFiles(const QStringList& filePaths)
{
    if (viewer_ == nullptr) {
        return false;
    }
    QString errorMessage;
    if (!viewer_->appendPointCloudFiles(filePaths, &errorMessage)) {
        syncUiFromViewer();
        showUserMessage(
            LogLevel::Error,
            errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
            6000);
        return false;
    }

    currentProjectFilePath_.clear();
    setTowerEditingEnabled(false);
    syncUiFromViewer();
    showUserMessage(
        LogLevel::Info,
        errorMessage.isEmpty() ? tr("Datasets added.") : errorMessage,
        4500);
    return true;
}

void MainWindow::clearPointCloud()
{
    currentProjectFilePath_.clear();
    setTowerEditingEnabled(false);
    viewer_->clearPointCloud();
}

void MainWindow::removeSelectedDataset()
{
    if (viewer_ == nullptr || projectTreeWidget_ == nullptr) {
        return;
    }

    const QString datasetPath = selectedDatasetPath();
    if (datasetPath.isEmpty()) {
        showUserMessage(LogLevel::Warning, tr("Select a dataset in the project tree before removing it."), 3000);
        return;
    }
    QStringList remainingFilePaths = viewer_->currentFilePaths();
    remainingFilePaths.removeAll(datasetPath);

    if (remainingFilePaths.isEmpty()) {
        clearPointCloud();
        showUserMessage(LogLevel::Info, tr("Dataset removed. The project is now empty."), 3000);
        return;
    }

    const QList<TowerMarker> towerMarkers = viewer_->towerMarkers();
    const QList<InspectionIssue> inspectionIssues = viewer_->inspectionIssues();
    const int selectedTowerIndex = viewer_->selectedTowerIndex();
    const int selectedIssueIndex = viewer_->selectedIssueIndex();
    QString errorMessage;
    if (viewer_->loadPointCloudFiles(remainingFilePaths, &errorMessage)) {
        currentProjectFilePath_.clear();
        setTowerEditingEnabled(false);
        viewer_->setTowerMarkers(towerMarkers);
        viewer_->setInspectionIssues(inspectionIssues);
        viewer_->setSelectedTowerIndex(selectedTowerIndex);
        viewer_->setSelectedIssueIndex(selectedIssueIndex);
        syncUiFromViewer();
        showUserMessage(LogLevel::Info, tr("Dataset removed from the project."), 3000);
    } else {
        syncUiFromViewer();
        showUserMessage(
            LogLevel::Error,
            errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
            6000);
    }
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
        const QSignalBlocker pointSizeBlocker(pointSizeSlider_);
        const QSignalBlocker pointOpacityBlocker(pointOpacitySlider_);
        const QSignalBlocker depthCueBlocker(depthCueSlider_);
        const QSignalBlocker edlStrengthBlocker(edlStrengthSlider_);
        const QSignalBlocker colorModeBlocker(colorModeComboBox_);
        const QSignalBlocker roundSplatsBlocker(roundSplatsCheckBox_);
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

        pointSizeSlider_->setValue(static_cast<int>(options.pointSize));
        pointOpacitySlider_->setValue(static_cast<int>(std::lround(options.pointOpacity * 100.0f)));
        depthCueSlider_->setValue(static_cast<int>(std::lround(options.depthCueStrength * 100.0f)));
        edlStrengthSlider_->setValue(static_cast<int>(std::lround(options.edlStrength * 100.0f)));
        colorModeComboBox_->setCurrentIndex(static_cast<int>(options.colorMode));
        roundSplatsCheckBox_->setChecked(options.useRoundSplats);
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

    updateSliderValueLabel(pointSizeSlider_, pointSizeValueLabel_, tr("%1 px"));
    updateSliderValueLabel(pointOpacitySlider_, pointOpacityValueLabel_, tr("%1%"));
    updateSliderValueLabel(depthCueSlider_, depthCueValueLabel_, tr("%1%"));
    updateSliderValueLabel(edlStrengthSlider_, edlStrengthValueLabel_, tr("%1%"));

    setColorButtonAppearance(pointColorButton_, options.singleColor, tr("Pick Color"));
    setColorButtonAppearance(backgroundColorButton_, options.backgroundColor, tr("Pick Background"));
    updateNavigationHelpText();
    rebuildProjectTree();
    updateDatasetPanel();
    updateMeasurementPanel();
    updateTowerPanel();
    updateIssuePanel();
    updateActionState();
}

void MainWindow::updateDatasetPanel()
{
    const PointCloudData* pointCloudData = viewer_->pointCloudData();
    if (pointCloudData == nullptr) {
        datasetNameValueLabel_->setText(tr("No dataset loaded"));
        datasetPathValueLabel_->setText(tr("Open, add, or drag LAS/LAZ files into the window."));
        datasetPointsValueLabel_->setText(QStringLiteral("0"));
        datasetBoundsValueLabel_->setText(tr("N/A"));
        datasetExtentValueLabel_->setText(tr("N/A"));
        datasetColorValueLabel_->setText(colorModeName(viewer_->visualizationOptions().colorMode));
        return;
    }

    const QStringList filePaths = viewer_->currentFilePaths();
    const PointRecord& minBounds = pointCloudData->minBounds();
    const PointRecord& maxBounds = pointCloudData->maxBounds();

    datasetNameValueLabel_->setText(
        filePaths.size() == 1
            ? QFileInfo(filePaths.constFirst()).fileName()
            : tr("%1 datasets").arg(QLocale().toString(filePaths.size())));
    datasetPathValueLabel_->setText(datasetPathSummary(filePaths));
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
    const bool hasTowerMarkers = viewer_ != nullptr && !viewer_->towerMarkers().isEmpty();
    const bool hasTowerSelection = viewer_ != nullptr && viewer_->selectedTowerIndex() >= 0;
    const bool towerToolActive = viewer_ != nullptr && viewer_->towerEditMode() != TowerEditMode::None;
    const bool hasIssues = viewer_ != nullptr && !viewer_->inspectionIssues().isEmpty();
    const bool hasIssueSelection = viewer_ != nullptr && viewer_->selectedIssueIndex() >= 0;
    const bool issueToolActive = viewer_ != nullptr && viewer_->issueEditMode() != IssueEditMode::None;
    const bool hasDatasetSelection = !selectedDatasetPath().isEmpty();
    openProjectAction_->setEnabled(true);
    saveProjectAction_->setEnabled(hasPointCloud);
    saveProjectAsAction_->setEnabled(hasPointCloud);
    addPointCloudAction_->setEnabled(true);
    removeDatasetAction_->setEnabled(hasPointCloud && hasDatasetSelection);
    locateDatasetAction_->setEnabled(hasDatasetSelection);
    copyDatasetPathAction_->setEnabled(hasDatasetSelection);
    expandProjectTreeAction_->setEnabled(projectTreeWidget_ != nullptr && projectTreeWidget_->topLevelItemCount() > 0);
    collapseProjectTreeAction_->setEnabled(projectTreeWidget_ != nullptr && projectTreeWidget_->topLevelItemCount() > 0);
    clearAction_->setEnabled(hasPointCloud);
    fitSceneAction_->setEnabled(hasPointCloud);
    topViewAction_->setEnabled(hasPointCloud);
    frontViewAction_->setEnabled(hasPointCloud);
    rightViewAction_->setEnabled(hasPointCloud);
    measureAction_->setEnabled(hasPointCloud);
    clearMeasurementAction_->setEnabled(hasPointCloud && viewer_->measurementResult().hasStartPoint);
    exportClearanceCsvAction_->setEnabled(hasPointCloud && viewer_->measurementResult().isComplete());
    showProfileDockAction_->setEnabled(true);
    showProfileDockAction_->setChecked(profileDock_ != nullptr && profileDock_->isVisible());
    measurementToggleButton_->setEnabled(hasPointCloud);
    measurementClearButton_->setEnabled(hasPointCloud && viewer_->measurementResult().hasStartPoint);
    clearanceThresholdSpinBox_->setEnabled(hasPointCloud);
    if (clearanceSegmentsTableWidget_ != nullptr) {
        clearanceSegmentsTableWidget_->setEnabled(hasPointCloud && viewer_->measurementResult().isComplete());
    }
    startTowerEditAction_->setEnabled(hasPointCloud && !towerEditingEnabled_);
    finishTowerEditAction_->setEnabled(towerEditingEnabled_);
    addTowerAction_->setEnabled(hasPointCloud && towerEditingEnabled_ && !towerToolActive);
    insertTowerAction_->setEnabled(hasTowerSelection && towerEditingEnabled_ && !towerToolActive);
    moveTowerAction_->setEnabled(hasTowerSelection && towerEditingEnabled_ && !towerToolActive);
    focusTowerAction_->setEnabled(hasTowerSelection);
    removeTowerAction_->setEnabled(hasTowerSelection && towerEditingEnabled_ && !towerToolActive);
    clearTowersAction_->setEnabled(hasTowerMarkers && towerEditingEnabled_ && !towerToolActive);
    cancelTowerToolAction_->setEnabled(towerEditingEnabled_ && towerToolActive);
    startIssueMarkAction_->setEnabled(hasPointCloud && !issueToolActive);
    cancelIssueToolAction_->setEnabled(issueToolActive);
    focusIssueAction_->setEnabled(hasIssueSelection);
    removeIssueAction_->setEnabled(hasIssueSelection);
    clearIssuesAction_->setEnabled(hasIssues);
    exportIssuesCsvAction_->setEnabled(hasIssues);
    exportInspectionReportAction_->setEnabled(hasPointCloud && (hasTowerMarkers || hasIssues));
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

void MainWindow::loadMeasurementSettings()
{
    QSettings settings;
    clearanceWarningThresholdMeters_ = settings.value(
        QStringLiteral("measurement/clearanceThresholdMeters"),
        clearanceWarningThresholdMeters_).toDouble();

    if (clearanceThresholdSpinBox_ != nullptr) {
        const QSignalBlocker blocker(clearanceThresholdSpinBox_);
        clearanceThresholdSpinBox_->setValue(clearanceWarningThresholdMeters_);
    }
}

void MainWindow::persistMeasurementSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("measurement/clearanceThresholdMeters"), clearanceWarningThresholdMeters_);
}

void MainWindow::loadVisualizationSettings()
{
    if (viewer_ == nullptr) {
        return;
    }

    QSettings settings;
    const PointCloudVisualizationOptions defaults = viewer_->visualizationOptions();
    viewer_->setPointSize(settings.value(QStringLiteral("visualization/pointSize"), defaults.pointSize).toInt());
    viewer_->setPointOpacity(settings.value(QStringLiteral("visualization/pointOpacity"), defaults.pointOpacity * 100.0f).toInt());
    viewer_->setDepthCueStrength(settings.value(QStringLiteral("visualization/depthCueStrength"), defaults.depthCueStrength * 100.0f).toInt());
    viewer_->setEdlStrength(settings.value(QStringLiteral("visualization/edlStrength"), defaults.edlStrength * 100.0f).toInt());
    viewer_->setColorMode(settings.value(QStringLiteral("visualization/colorMode"), static_cast<int>(defaults.colorMode)).toInt());
    viewer_->setSingleColor(settings.value(QStringLiteral("visualization/singleColor"), defaults.singleColor).value<QColor>());
    viewer_->setBackgroundColor(settings.value(QStringLiteral("visualization/backgroundColor"), defaults.backgroundColor).value<QColor>());
    viewer_->setUseRoundSplats(settings.value(QStringLiteral("visualization/useRoundSplats"), defaults.useRoundSplats).toBool());
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
    settings.setValue(QStringLiteral("visualization/pointOpacity"), options.pointOpacity * 100.0f);
    settings.setValue(QStringLiteral("visualization/depthCueStrength"), options.depthCueStrength * 100.0f);
    settings.setValue(QStringLiteral("visualization/edlStrength"), options.edlStrength * 100.0f);
    settings.setValue(QStringLiteral("visualization/colorMode"), static_cast<int>(options.colorMode));
    settings.setValue(QStringLiteral("visualization/singleColor"), options.singleColor);
    settings.setValue(QStringLiteral("visualization/backgroundColor"), options.backgroundColor);
    settings.setValue(QStringLiteral("visualization/useRoundSplats"), options.useRoundSplats);
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
    const bool showProfile = settings.value(QStringLiteral("window/showProfile"), true).toBool();
    if (inspectorTabWidget_ != nullptr) {
        inspectorTabWidget_->setCurrentIndex(settings.value(QStringLiteral("window/inspectorTab"), 0).toInt());
    }
    if (showLogAction_ != nullptr) {
        showLogAction_->setChecked(showLog);
    }
    if (showProfileDockAction_ != nullptr) {
        showProfileDockAction_->setChecked(showProfile);
    }
    if (logDock_ != nullptr) {
        logDock_->setVisible(showLog);
    }
    if (profileDock_ != nullptr) {
        profileDock_->setVisible(showProfile);
    }
}

void MainWindow::persistWindowSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/maximized"), isMaximized());
    settings.setValue(QStringLiteral("window/showLog"), logDock_ != nullptr && logDock_->isVisible());
    settings.setValue(QStringLiteral("window/showProfile"), profileDock_ != nullptr && profileDock_->isVisible());
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
    const ClearanceAnalysisResult clearanceAnalysis = analyzeClearancePath(
        measurementResult.points,
        static_cast<float>(clearanceWarningThresholdMeters_));
    measurementToggleButton_->setText(
        viewer_->measurementEnabled() ? tr("Stop Measurement") : tr("Start Measurement"));
    measurementClearButton_->setText(tr("Clear Measurement"));

    measurementStartValueLabel_->setText(measurementPointText(measurementResult, true));
    measurementEndValueLabel_->setText(measurementPointText(measurementResult, false));
    measurementDistanceValueLabel_->setText(
        measurementResult.isComplete() ? formatCoordinate(measurementResult.distance3d) : tr("N/A"));
    measurementHorizontalDistanceValueLabel_->setText(
        clearanceAnalysis.isValid() ? formatCoordinate(clearanceAnalysis.totalHorizontalDistance) : tr("N/A"));
    measurementDeltaZValueLabel_->setText(
        measurementResult.isComplete() ? formatCoordinate(measurementResult.deltaZ) : tr("N/A"));
    measurementSegmentsValueLabel_->setText(
        measurementResult.isComplete()
            ? QLocale().toString(measurementResult.pointCount() - 1)
            : QStringLiteral("0"));

    QString clearanceStatusText = tr("Add at least two measured points to analyze corridor clearance.");
    QString clearanceStatusStyle = QStringLiteral("color: #475569;");
    if (!clearanceAnalysis.isValid()) {
        clearanceShortestValueLabel_->setText(tr("N/A"));
        clearanceWarningCountValueLabel_->setText(QStringLiteral("0"));
    } else {
        clearanceShortestValueLabel_->setText(formatCoordinate(clearanceAnalysis.minimumSegmentDistance));
        clearanceWarningCountValueLabel_->setText(QLocale().toString(clearanceAnalysis.warningCount));

        if (!clearanceAnalysis.thresholdEnabled()) {
            clearanceStatusText = tr("Clearance threshold is disabled. Set a value above 0 m to enable warnings.");
            clearanceStatusStyle = QStringLiteral("color: #475569;");
        } else if (clearanceAnalysis.hasWarnings()) {
            clearanceStatusText = tr("%1 segment(s) are below the clearance threshold of %2 m.")
                .arg(QLocale().toString(clearanceAnalysis.warningCount))
                .arg(formatCoordinate(static_cast<float>(clearanceWarningThresholdMeters_)));
            clearanceStatusStyle = QStringLiteral("color: #b91c1c; font-weight: 600;");
        } else {
            clearanceStatusText = tr("All measured segments satisfy the clearance threshold of %1 m.")
                .arg(formatCoordinate(static_cast<float>(clearanceWarningThresholdMeters_)));
            clearanceStatusStyle = QStringLiteral("color: #15803d; font-weight: 600;");
        }
    }

    clearanceStatusValueLabel_->setText(clearanceStatusText);
    clearanceStatusValueLabel_->setStyleSheet(clearanceStatusStyle);
    updateClearanceSegmentsTable(clearanceAnalysis);
    if (profilePlotWidget_ != nullptr) {
        profilePlotWidget_->setAnalysisResult(clearanceAnalysis);
        profilePlotWidget_->setProfileMarkers(projectProfileMarkers(
            clearanceAnalysis,
            viewer_->towerMarkers(),
            viewer_->selectedTowerIndex(),
            viewer_->inspectionIssues(),
            viewer_->selectedIssueIndex()));
        profilePlotWidget_->setSelectedSegmentIndex(
            clearanceSegmentsTableWidget_ != nullptr ? clearanceSegmentsTableWidget_->currentRow() : -1);
    }
}

void MainWindow::updateClearanceSegmentsTable(const ClearanceAnalysisResult& clearanceAnalysis)
{
    if (clearanceSegmentsSummaryLabel_ == nullptr || clearanceSegmentsTableWidget_ == nullptr) {
        return;
    }

    const int previousRow = clearanceSegmentsTableWidget_->currentRow();
    const QSignalBlocker blocker(clearanceSegmentsTableWidget_);
    clearanceSegmentsTableWidget_->setRowCount(0);

    if (!clearanceAnalysis.isValid()) {
        clearanceSegmentsSummaryLabel_->setText(
            tr("Add at least two measured points to list corridor segments and export clearance details."));
        clearanceSegmentsTableWidget_->clearSelection();
        return;
    }

    if (!clearanceAnalysis.thresholdEnabled()) {
        clearanceSegmentsSummaryLabel_->setText(
            tr("Listed %1 path segment(s). Set a warning threshold above 0 m to flag low-clearance spans.")
                .arg(QLocale().toString(clearanceAnalysis.segments.size())));
    } else if (clearanceAnalysis.hasWarnings()) {
        clearanceSegmentsSummaryLabel_->setText(
            tr("%1 segment(s) are below %2 m. Select a row to highlight it in the profile or export the full list.")
                .arg(QLocale().toString(clearanceAnalysis.warningCount))
                .arg(formatCoordinate(clearanceAnalysis.threshold)));
    } else {
        clearanceSegmentsSummaryLabel_->setText(
            tr("All %1 segment(s) satisfy the current clearance threshold of %2 m.")
                .arg(QLocale().toString(clearanceAnalysis.segments.size()))
                .arg(formatCoordinate(clearanceAnalysis.threshold)));
    }

    int preferredRow = previousRow;
    if (preferredRow < 0 || preferredRow >= clearanceAnalysis.segments.size()) {
        preferredRow = 0;
        for (int segmentIndex = 0; segmentIndex < clearanceAnalysis.segments.size(); ++segmentIndex) {
            if (clearanceAnalysis.segments.at(segmentIndex).belowThreshold) {
                preferredRow = segmentIndex;
                break;
            }
        }
    }

    for (int segmentIndex = 0; segmentIndex < clearanceAnalysis.segments.size(); ++segmentIndex) {
        const ClearanceSegment& segment = clearanceAnalysis.segments.at(segmentIndex);
        clearanceSegmentsTableWidget_->insertRow(segmentIndex);

        auto createReadOnlyItem = [](const QString& text, const QColor& color = QColor(), Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags((item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            item->setTextAlignment(alignment);
            if (color.isValid()) {
                item->setForeground(color);
            }
            return item;
        };

        const QColor statusColor = segment.belowThreshold ? QColor(185, 28, 28) : QColor(22, 101, 52);
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            0,
            createReadOnlyItem(QLocale().toString(segmentIndex + 1), QColor(), Qt::AlignCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            1,
            createReadOnlyItem(QLocale().toString(segment.startPointIndex + 1), QColor(), Qt::AlignCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            2,
            createReadOnlyItem(QLocale().toString(segment.endPointIndex + 1), QColor(), Qt::AlignCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            3,
            createReadOnlyItem(
                tr("%1 - %2 m")
                    .arg(formatCoordinate(segment.chainageStart))
                    .arg(formatCoordinate(segment.chainageEnd))));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            4,
            createReadOnlyItem(formatCoordinate(segment.horizontalDistance), QColor(), Qt::AlignRight | Qt::AlignVCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            5,
            createReadOnlyItem(formatCoordinate(segment.distance3d), QColor(), Qt::AlignRight | Qt::AlignVCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            6,
            createReadOnlyItem(formatCoordinate(segment.deltaZ), QColor(), Qt::AlignRight | Qt::AlignVCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            7,
            createReadOnlyItem(segment.belowThreshold ? tr("Warning") : tr("OK"), statusColor, Qt::AlignCenter));
    }

    if (clearanceSegmentsTableWidget_->rowCount() > 0) {
        const int normalizedRow = std::max(0, std::min(preferredRow, clearanceSegmentsTableWidget_->rowCount() - 1));
        clearanceSegmentsTableWidget_->setCurrentCell(normalizedRow, 0);
    } else {
        clearanceSegmentsTableWidget_->clearSelection();
    }
}

void MainWindow::rebuildProjectTree()
{
    if (projectTreeWidget_ == nullptr || viewer_ == nullptr) {
        return;
    }

    const QString previousDatasetPath = selectedDatasetPath();
    const QSignalBlocker blocker(projectTreeWidget_);
    projectTreeWidget_->clear();

    auto* projectRoot = new QTreeWidgetItem(projectTreeWidget_, QStringList {
        currentProjectFilePath_.isEmpty()
            ? tr("Current Project")
            : QFileInfo(currentProjectFilePath_).fileName()
    });
    projectRoot->setData(0, kProjectTreeItemTypeRole, QStringLiteral("project"));
    projectRoot->setIcon(0, style()->standardIcon(QStyle::SP_DriveHDIcon));

    auto* datasetsRoot = new QTreeWidgetItem(projectRoot, QStringList {
        tr("Datasets (%1)").arg(QLocale().toString(viewer_->currentFilePaths().size()))
    });
    datasetsRoot->setData(0, kProjectTreeItemTypeRole, QStringLiteral("datasets"));
    datasetsRoot->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));

    QHash<QString, QTreeWidgetItem*> directoryItems;
    directoryItems.insert(QString(), datasetsRoot);
    QTreeWidgetItem* selectedItem = nullptr;

    for (const QString& filePath : viewer_->currentFilePaths()) {
        const QFileInfo fileInfo(filePath);
        const QString absoluteFilePath = fileInfo.absoluteFilePath();
        const QString directoryPath = QDir::fromNativeSeparators(fileInfo.absolutePath());
        const QStringList segments = directoryPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);

        QString cumulativePath;
        QTreeWidgetItem* parentItem = datasetsRoot;
        for (const QString& segment : segments) {
            cumulativePath = cumulativePath.isEmpty()
                ? segment
                : cumulativePath + QLatin1Char('/') + segment;

            QTreeWidgetItem* folderItem = directoryItems.value(cumulativePath, nullptr);
            if (folderItem == nullptr) {
                folderItem = new QTreeWidgetItem(parentItem, QStringList { segment });
                folderItem->setData(0, kProjectTreeItemTypeRole, QStringLiteral("folder"));
                folderItem->setToolTip(0, QDir::toNativeSeparators(cumulativePath));
                folderItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
                directoryItems.insert(cumulativePath, folderItem);
            }
            parentItem = folderItem;
        }

        auto* datasetItem = new QTreeWidgetItem(parentItem, QStringList { fileInfo.fileName() });
        datasetItem->setData(0, kProjectTreeItemTypeRole, QStringLiteral("dataset"));
        datasetItem->setData(0, kProjectTreeFilePathRole, absoluteFilePath);
        datasetItem->setToolTip(0, absoluteFilePath);
        datasetItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));

        if (!previousDatasetPath.isEmpty() && previousDatasetPath.compare(absoluteFilePath, Qt::CaseInsensitive) == 0) {
            selectedItem = datasetItem;
        }
    }

    projectRoot->setExpanded(true);
    datasetsRoot->setExpanded(true);
    projectTreeWidget_->expandToDepth(1);

    if (selectedItem == nullptr) {
        QTreeWidgetItemIterator it(projectTreeWidget_);
        while (*it != nullptr) {
            if ((*it)->data(0, kProjectTreeItemTypeRole).toString() == QStringLiteral("dataset")) {
                selectedItem = *it;
                break;
            }
            ++it;
        }
    }

    projectTreeWidget_->setCurrentItem(selectedItem != nullptr ? selectedItem : datasetsRoot);
    refreshProjectTreeFilter();
}

void MainWindow::refreshProjectTreeFilter()
{
    if (projectTreeWidget_ == nullptr) {
        return;
    }

    const QString filterText = projectSearchEdit_ != nullptr ? projectSearchEdit_->text().trimmed() : QString();
    const auto updateItemVisibility = [&filterText](auto&& self, QTreeWidgetItem* item) -> bool {
        if (item == nullptr) {
            return false;
        }

        bool hasVisibleChild = false;
        for (int childIndex = 0; childIndex < item->childCount(); ++childIndex) {
            hasVisibleChild = self(self, item->child(childIndex)) || hasVisibleChild;
        }

        const QString itemText = item->text(0) + QLatin1Char('\n') + item->toolTip(0);
        const bool matchesSelf =
            filterText.isEmpty()
            || itemText.contains(filterText, Qt::CaseInsensitive);
        const QString itemType = item->data(0, kProjectTreeItemTypeRole).toString();
        const bool forceVisible = itemType == QStringLiteral("project") || itemType == QStringLiteral("datasets");
        const bool visible = forceVisible || matchesSelf || hasVisibleChild;
        item->setHidden(!visible);
        if (!filterText.isEmpty() && visible && item->childCount() > 0) {
            item->setExpanded(true);
        }
        return visible;
    };

    for (int rootIndex = 0; rootIndex < projectTreeWidget_->topLevelItemCount(); ++rootIndex) {
        updateItemVisibility(updateItemVisibility, projectTreeWidget_->topLevelItem(rootIndex));
    }

    if (QTreeWidgetItem* currentItem = projectTreeWidget_->currentItem();
        currentItem != nullptr && currentItem->isHidden()) {
        projectTreeWidget_->setCurrentItem(nullptr);
    }

    updateActionState();
}

QString MainWindow::selectedDatasetPath() const
{
    if (projectTreeWidget_ == nullptr || projectTreeWidget_->currentItem() == nullptr) {
        return QString();
    }

    const QTreeWidgetItem* currentItem = projectTreeWidget_->currentItem();
    if (currentItem->data(0, kProjectTreeItemTypeRole).toString() != QStringLiteral("dataset")) {
        return QString();
    }

    return currentItem->data(0, kProjectTreeFilePathRole).toString();
}

void MainWindow::updateTowerPanel()
{
    if (viewer_ == nullptr || towerTableWidget_ == nullptr || towerCountValueLabel_ == nullptr || towerToolStatusLabel_ == nullptr) {
        return;
    }

    const QList<TowerMarker>& towerMarkers = viewer_->towerMarkers();
    towerCountValueLabel_->setText(
        towerMarkers.isEmpty()
            ? tr("No tower markers yet. Use the toolbar or menu above to add one from the point cloud.")
            : tr("%1 tower marker(s)").arg(QLocale().toString(towerMarkers.size())));

    switch (viewer_->towerEditMode()) {
    case TowerEditMode::AddAfterLast:
        towerToolStatusLabel_->setText(tr("Tower tool: click points in the view continuously to add tower markers. Cancel the tool when finished."));
        break;
    case TowerEditMode::InsertBeforeSelected:
        towerToolStatusLabel_->setText(
            tr("Tower tool: click a point in the view to insert a tower marker before current tower #%1.")
                .arg(QLocale().toString(viewer_->towerEditTargetIndex() + 1)));
        break;
    case TowerEditMode::MoveSelected:
        towerToolStatusLabel_->setText(
            tr("Tower tool: click a point in the view to move current tower #%1.")
                .arg(QLocale().toString(viewer_->towerEditTargetIndex() + 1)));
        break;
    case TowerEditMode::None:
    default:
        towerToolStatusLabel_->setText(
            towerEditingEnabled_
                ? tr("Tower editing active. Select the current tower, then use the toolbar above to insert, move, rename, focus, or remove it.")
                : tr("Tower editing is off. Use the Ribbon to start editing before changing tower markers."));
        break;
    }

    const int selectedRow = viewer_->selectedTowerIndex();
    const QSignalBlocker blocker(towerTableWidget_);
    towerTableWidget_->setEditTriggers(static_cast<QAbstractItemView::EditTriggers>(
        towerEditingEnabled_
            ? (QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed)
            : QAbstractItemView::NoEditTriggers));
    towerTableWidget_->setRowCount(0);
    for (int towerIndex = 0; towerIndex < towerMarkers.size(); ++towerIndex) {
        const TowerMarker& towerMarker = towerMarkers.at(towerIndex);
        towerTableWidget_->insertRow(towerIndex);

        auto* indexItem = new QTableWidgetItem(QLocale().toString(towerIndex + 1));
        indexItem->setFlags((indexItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        indexItem->setTextAlignment(Qt::AlignCenter);
        towerTableWidget_->setItem(towerIndex, 0, indexItem);

        auto* nameItem = new QTableWidgetItem(towerMarker.name);
        if (towerEditingEnabled_) {
            nameItem->setFlags(nameItem->flags() | Qt::ItemIsEditable);
        } else {
            nameItem->setFlags((nameItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        }
        towerTableWidget_->setItem(towerIndex, 1, nameItem);

        const QStringList coordinateTexts = {
            formatCoordinate(towerMarker.point.x),
            formatCoordinate(towerMarker.point.y),
            formatCoordinate(towerMarker.point.z)
        };
        for (int coordinateColumn = 0; coordinateColumn < coordinateTexts.size(); ++coordinateColumn) {
            auto* coordinateItem = new QTableWidgetItem(coordinateTexts.at(coordinateColumn));
            coordinateItem->setFlags((coordinateItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            coordinateItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            towerTableWidget_->setItem(towerIndex, coordinateColumn + 2, coordinateItem);
        }
    }

    if (towerTableWidget_->rowCount() > 0) {
        towerTableWidget_->setCurrentCell(std::clamp(selectedRow, 0, towerTableWidget_->rowCount() - 1), 1);
    } else {
        towerTableWidget_->clearSelection();
    }

    updateTowerDetailEditor();
}

QString MainWindow::nextDefaultTowerName() const
{
    if (viewer_ == nullptr) {
        return QString();
    }

    std::set<QString> existingNames;
    for (const TowerMarker& towerMarker : viewer_->towerMarkers()) {
        existingNames.insert(towerMarker.name.trimmed());
    }

    int towerIndex = 1;
    while (true) {
        const QString candidate = tr("Tower %1").arg(QLocale().toString(towerIndex));
        if (existingNames.find(candidate) == existingNames.end()) {
            return candidate;
        }
        ++towerIndex;
    }
}

QString MainWindow::nextDefaultIssueTitle() const
{
    if (viewer_ == nullptr) {
        return QString();
    }

    std::set<QString> existingTitles;
    for (const InspectionIssue& issue : viewer_->inspectionIssues()) {
        existingTitles.insert(issue.title.trimmed());
    }

    int issueIndex = 1;
    while (true) {
        const QString candidate = tr("Issue %1").arg(QLocale().toString(issueIndex));
        if (existingTitles.find(candidate) == existingTitles.end()) {
            return candidate;
        }
        ++issueIndex;
    }
}

void MainWindow::updateTowerDetailEditor()
{
    if (viewer_ == nullptr || towerCodeEdit_ == nullptr) {
        return;
    }

    updatingTowerDetails_ = true;
    const QList<TowerRecord>& towers = viewer_->towerMarkers();
    const int selectedTowerIndex = viewer_->selectedTowerIndex();
    const bool hasSelection = selectedTowerIndex >= 0 && selectedTowerIndex < towers.size();

    const TowerRecord towerRecord = hasSelection ? towers.at(selectedTowerIndex) : TowerRecord();
    towerCodeEdit_->setText(towerRecord.code);
    towerLineNameEdit_->setText(towerRecord.lineName);
    towerVoltageLevelEdit_->setText(towerRecord.voltageLevel);
    towerStructureTypeEdit_->setText(towerRecord.structureType);
    towerInspectionDateEdit_->setText(towerRecord.inspectionDate);
    towerStatusEdit_->setText(towerRecord.status);
    towerNotesEdit_->setPlainText(towerRecord.notes);

    const QList<QWidget*> editors = {
        towerCodeEdit_,
        towerLineNameEdit_,
        towerVoltageLevelEdit_,
        towerStructureTypeEdit_,
        towerInspectionDateEdit_,
        towerStatusEdit_,
        towerNotesEdit_
    };
    for (QWidget* editor : editors) {
        if (editor != nullptr) {
            editor->setEnabled(hasSelection && towerEditingEnabled_);
        }
    }
    updatingTowerDetails_ = false;
}

void MainWindow::updateIssuePanel()
{
    if (viewer_ == nullptr || issueTableWidget_ == nullptr || issueCountValueLabel_ == nullptr || issueToolStatusLabel_ == nullptr) {
        return;
    }

    const QList<InspectionIssue>& issues = viewer_->inspectionIssues();
    issueCountValueLabel_->setText(
        issues.isEmpty()
            ? tr("No inspection issues yet. Use the toolbar above to mark one from the point cloud.")
            : tr("%1 inspection issue(s)").arg(QLocale().toString(issues.size())));
    issueToolStatusLabel_->setText(
        viewer_->issueEditMode() == IssueEditMode::Add
            ? tr("Issue tool: click points in the view continuously to add inspection issues. Right-click to cancel the tool.")
            : tr("Select an issue to edit its business details, focus the scene, or export reports."));

    const int selectedRow = viewer_->selectedIssueIndex();
    const QSignalBlocker blocker(issueTableWidget_);
    issueTableWidget_->setRowCount(0);
    for (int issueIndex = 0; issueIndex < issues.size(); ++issueIndex) {
        const InspectionIssue& issue = issues.at(issueIndex);
        issueTableWidget_->insertRow(issueIndex);

        auto* indexItem = new QTableWidgetItem(QLocale().toString(issueIndex + 1));
        indexItem->setFlags((indexItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        indexItem->setTextAlignment(Qt::AlignCenter);
        issueTableWidget_->setItem(issueIndex, 0, indexItem);

        auto* titleItem = new QTableWidgetItem(issue.title);
        titleItem->setFlags((titleItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        issueTableWidget_->setItem(issueIndex, 1, titleItem);

        const QStringList rowValues = {
            issueSeverityDisplayName(issue.severity),
            issueStatusDisplayName(issue.status),
            issue.relatedTowerName,
            issue.category
        };
        for (int valueIndex = 0; valueIndex < rowValues.size(); ++valueIndex) {
            auto* valueItem = new QTableWidgetItem(rowValues.at(valueIndex));
            valueItem->setFlags((valueItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            issueTableWidget_->setItem(issueIndex, valueIndex + 2, valueItem);
        }
    }

    if (issueTableWidget_->rowCount() > 0 && selectedRow >= 0) {
        issueTableWidget_->setCurrentCell(std::clamp(selectedRow, 0, issueTableWidget_->rowCount() - 1), 1);
    } else {
        issueTableWidget_->clearSelection();
    }

    updateIssueDetailEditor();
}

void MainWindow::updateIssueDetailEditor()
{
    if (viewer_ == nullptr || issueTitleEdit_ == nullptr) {
        return;
    }

    updatingIssueDetails_ = true;
    const QList<InspectionIssue>& issues = viewer_->inspectionIssues();
    const QList<TowerRecord>& towers = viewer_->towerMarkers();
    const int selectedIssueIndex = viewer_->selectedIssueIndex();
    const bool hasSelection = selectedIssueIndex >= 0 && selectedIssueIndex < issues.size();
    const InspectionIssue issue = hasSelection ? issues.at(selectedIssueIndex) : InspectionIssue();

    issueTitleEdit_->setText(issue.title);
    issueCategoryComboBox_->setEditText(issue.category);
    issueSeverityComboBox_->setCurrentIndex(static_cast<int>(issue.severity));
    issueStatusComboBox_->setCurrentIndex(static_cast<int>(issue.status));
    issueImagePathEdit_->setText(issue.imagePath);
    issueDescriptionEdit_->setPlainText(issue.description);
    issueLocationValueLabel_->setText(
        hasSelection ? formatTriplet(issue.point.x, issue.point.y, issue.point.z) : tr("N/A"));
    issueCreatedAtValueLabel_->setText(hasSelection ? issue.createdAt : tr("N/A"));

    issueRelatedTowerComboBox_->clear();
    issueRelatedTowerComboBox_->addItem(tr("None"), -1);
    for (int towerIndex = 0; towerIndex < towers.size(); ++towerIndex) {
        issueRelatedTowerComboBox_->addItem(towers.at(towerIndex).name, towerIndex);
    }
    const int relatedTowerOption = issueRelatedTowerComboBox_->findData(issue.relatedTowerIndex);
    issueRelatedTowerComboBox_->setCurrentIndex(relatedTowerOption >= 0 ? relatedTowerOption : 0);

    const QList<QWidget*> editors = {
        issueTitleEdit_,
        issueCategoryComboBox_,
        issueSeverityComboBox_,
        issueStatusComboBox_,
        issueRelatedTowerComboBox_,
        issueImagePathEdit_,
        issueDescriptionEdit_
    };
    for (QWidget* editor : editors) {
        if (editor != nullptr) {
            editor->setEnabled(hasSelection);
        }
    }
    updatingIssueDetails_ = false;
}

void MainWindow::setTowerEditingEnabled(bool enabled)
{
    if (towerEditingEnabled_ == enabled) {
        return;
    }

    towerEditingEnabled_ = enabled;
    if (!towerEditingEnabled_ && viewer_ != nullptr) {
        viewer_->cancelTowerEditMode();
    }

    updateActionState();
    updateTowerPanel();
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
