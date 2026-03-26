#include "gui/MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QLocale>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPushButton>
#include <QDropEvent>
#include <QPalette>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <QMouseEvent>

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
    ThemeDarkGray
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
        return QStringLiteral("Elevation ramp");
    case PointCloudColorMode::SingleColor:
        return QStringLiteral("Single color");
    case PointCloudColorMode::Rgb:
    default:
        return QStringLiteral("RGB");
    }
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
    }

    return QIcon(pixmap);
}
}

MainWindow::MainWindow(QWidget* parent)
    : Qtitan::RibbonMainWindow(parent)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowTitle(QStringLiteral("LAS Point Cloud Viewer"));
    resize(1520, 920);
    setAcceptDrops(true);

    viewer_ = new PointCloudViewer(this);
    setCentralWidget(viewer_);

    createActions();
    createRibbon();
    createInspectorPanel();
    createStatusBar();
    createConnections();

    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        updateWindowChromePalette(ribbonStyle->getTheme());
    }

    syncUiFromViewer();

    statusBar()->showMessage(QStringLiteral("Ready. Open or drag a LAS/LAZ file to begin."), 4000);
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

    statusBar()->showMessage(tr("Only LAS and LAZ files can be dropped here."), 4000);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ribbonBar_ && event != nullptr) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QWidget* child = ribbonBar_->childAt(mouseEvent->pos());
                const bool clickedBackground = child == nullptr || child == ribbonBar_;
                if (clickedBackground) {
                    if (QWindow* win = windowHandle()) {
                        win->startSystemMove();
                        return true;
                    }
                }
            }
        }
    }

    return Qtitan::RibbonMainWindow::eventFilter(watched, event);
}

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
}

void MainWindow::createRibbon()
{
    ribbonBar_ = new Qtitan::RibbonBar(this);
    // Avoid Qtitan's DWM-integrated frame path, which paints a black caption area on this setup.
    ribbonBar_->setFrameThemeEnabled(false);
    ribbonBar_->setTitleBarVisible(false);
    ribbonBar_->showQuickAccess(true);
    ribbonBar_->installEventFilter(this);
    setRibbonBar(ribbonBar_);

    ribbonBar_->quickAccessBar()->addAction(openAction_);
    ribbonBar_->quickAccessBar()->addAction(fitSceneAction_);
    ribbonBar_->quickAccessBar()->addAction(showAxesAction_);

    Qtitan::RibbonPage* homePage = ribbonBar_->addPage(tr("Home"));
    Qtitan::RibbonGroup* dataGroup = homePage->addGroup(tr("Dataset"));
    dataGroup->addAction(openAction_, Qt::ToolButtonTextUnderIcon);
    dataGroup->addAction(clearAction_, Qt::ToolButtonTextUnderIcon);
    dataGroup->addAction(exitAction_, Qt::ToolButtonTextUnderIcon);

    Qtitan::RibbonGroup* viewGroup = homePage->addGroup(tr("Camera"));
    viewGroup->addAction(fitSceneAction_, Qt::ToolButtonTextUnderIcon);
    viewGroup->addAction(topViewAction_, Qt::ToolButtonTextUnderIcon);
    viewGroup->addAction(frontViewAction_, Qt::ToolButtonTextUnderIcon);
    viewGroup->addAction(rightViewAction_, Qt::ToolButtonTextUnderIcon);

    Qtitan::RibbonGroup* sceneGroup = homePage->addGroup(tr("Scene Guides"));
    sceneGroup->addAction(showAxesAction_, Qt::ToolButtonTextUnderIcon);
    sceneGroup->addAction(showBoundingBoxAction_, Qt::ToolButtonTextUnderIcon);
    sceneGroup->addAction(darkBackgroundAction_, Qt::ToolButtonTextUnderIcon);
    sceneGroup->addAction(lightBackgroundAction_, Qt::ToolButtonTextUnderIcon);

    Qtitan::RibbonPage* appearancePage = ribbonBar_->addPage(tr("Appearance"));
    Qtitan::RibbonGroup* colorGroup = appearancePage->addGroup(tr("Point Colors"));
    colorGroup->addAction(rgbColorAction_, Qt::ToolButtonTextUnderIcon);
    colorGroup->addAction(elevationColorAction_, Qt::ToolButtonTextUnderIcon);
    colorGroup->addAction(singleColorAction_, Qt::ToolButtonTextUnderIcon);

    Qtitan::RibbonGroup* themeGroup = appearancePage->addGroup(tr("Office Theme"));
    themeGroup->addAction(themeColorfulAction_, Qt::ToolButtonTextUnderIcon);
    themeGroup->addAction(themeWhiteAction_, Qt::ToolButtonTextUnderIcon);
    themeGroup->addAction(themeDarkGrayAction_, Qt::ToolButtonTextUnderIcon);
}

void MainWindow::createInspectorPanel()
{
    inspectorDock_ = new QDockWidget(tr("Scene Inspector"), this);
    inspectorDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    inspectorDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    auto* panel = new QWidget(inspectorDock_);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(14, 14, 14, 14);
    panelLayout->setSpacing(12);

    auto* datasetGroupBox = new QGroupBox(tr("Dataset Summary"), panel);
    auto* datasetLayout = new QFormLayout(datasetGroupBox);
    datasetLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    datasetLayout->setFormAlignment(Qt::AlignTop);

    datasetNameValueLabel_ = new QLabel(datasetGroupBox);
    datasetPathValueLabel_ = new QLabel(datasetGroupBox);
    datasetPointsValueLabel_ = new QLabel(datasetGroupBox);
    datasetBoundsValueLabel_ = new QLabel(datasetGroupBox);
    datasetExtentValueLabel_ = new QLabel(datasetGroupBox);
    datasetColorValueLabel_ = new QLabel(datasetGroupBox);

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

    datasetLayout->addRow(tr("Name"), datasetNameValueLabel_);
    datasetLayout->addRow(tr("Path"), datasetPathValueLabel_);
    datasetLayout->addRow(tr("Points"), datasetPointsValueLabel_);
    datasetLayout->addRow(tr("Bounds"), datasetBoundsValueLabel_);
    datasetLayout->addRow(tr("Extent"), datasetExtentValueLabel_);
    datasetLayout->addRow(tr("Color Source"), datasetColorValueLabel_);

    auto* renderingGroupBox = new QGroupBox(tr("Rendering Controls"), panel);
    auto* renderingLayout = new QFormLayout(renderingGroupBox);
    renderingLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    renderingLayout->setFormAlignment(Qt::AlignTop);

    pointSizeSpinBox_ = new QSpinBox(renderingGroupBox);
    pointSizeSpinBox_->setRange(1, 12);
    pointSizeSpinBox_->setSuffix(tr(" px"));

    colorModeComboBox_ = new QComboBox(renderingGroupBox);
    colorModeComboBox_->addItem(tr("RGB"));
    colorModeComboBox_->addItem(tr("Elevation Ramp"));
    colorModeComboBox_->addItem(tr("Single Color"));

    pointColorButton_ = new QPushButton(tr("Pick Color"), renderingGroupBox);
    backgroundColorButton_ = new QPushButton(tr("Pick Background"), renderingGroupBox);

    axesCheckBox_ = new QCheckBox(tr("Show XYZ axes"), renderingGroupBox);
    boundingBoxCheckBox_ = new QCheckBox(tr("Show bounding box"), renderingGroupBox);

    renderingLayout->addRow(tr("Point Size"), pointSizeSpinBox_);
    renderingLayout->addRow(tr("Color Mode"), colorModeComboBox_);
    renderingLayout->addRow(tr("Single Color"), pointColorButton_);
    renderingLayout->addRow(tr("Background"), backgroundColorButton_);
    renderingLayout->addRow(QString(), axesCheckBox_);
    renderingLayout->addRow(QString(), boundingBoxCheckBox_);

    auto* tipsGroupBox = new QGroupBox(tr("Navigation"), panel);
    auto* tipsLayout = new QVBoxLayout(tipsGroupBox);
    auto* tipsLabel = new QLabel(
        tr("Left drag orbits, right drag pans, and the mouse wheel zooms. Use the ribbon camera presets to quickly inspect the dataset from top, front, or right."),
        tipsGroupBox);
    tipsLabel->setWordWrap(true);
    tipsLayout->addWidget(tipsLabel);

    panelLayout->addWidget(datasetGroupBox);
    panelLayout->addWidget(renderingGroupBox);
    panelLayout->addWidget(tipsGroupBox);
    panelLayout->addStretch(1);

    panel->setStyleSheet(
        "QWidget {"
        "background-color: #f6f8fb;"
        "color: #1f2937;"
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
        "padding: 6px 10px;"
        "color: #111827;"
        "}"
        "QPushButton:hover, QComboBox:hover, QSpinBox:hover {"
        "border-color: #94a3b8;"
        "}"
        "QCheckBox {"
        "color: #1f2937;"
        "}");

    inspectorDock_->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);
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

    connect(viewer_, &PointCloudViewer::pointCloudLoaded, this, [this]() {
        syncUiFromViewer();
        statusBar()->showMessage(tr("Point cloud loaded."), 3000);
    });
    connect(viewer_, &PointCloudViewer::pointCloudCleared, this, [this]() {
        syncUiFromViewer();
        statusBar()->showMessage(tr("Scene cleared."), 3000);
    });
    connect(viewer_, &PointCloudViewer::visualizationOptionsChanged, this, [this]() { syncUiFromViewer(); });
}

void MainWindow::openPointCloud()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open LAS Point Cloud"),
        QString(),
        tr("LAS Files (*.las *.laz);;All Files (*.*)"));

    if (filePath.isEmpty()) {
        statusBar()->showMessage(tr("Open cancelled."), 2000);
        return;
    }

    loadPointCloudFile(filePath);
}

bool MainWindow::loadPointCloudFile(const QString& filePath)
{
    QString errorMessage;
    if (viewer_->loadPointCloud(filePath, &errorMessage)) {
        const QFileInfo fileInfo(filePath);
        statusBar()->showMessage(tr("Loaded %1").arg(fileInfo.fileName()), 4000);
        return true;
    }

    syncUiFromViewer();
    statusBar()->showMessage(errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage, 6000);
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
        syncUiFromViewer();
        statusBar()->showMessage(tr("Theme updated."), 2500);
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

    update();
}

void MainWindow::syncUiFromViewer()
{
    const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();

    {
        const QSignalBlocker pointSizeBlocker(pointSizeSpinBox_);
        const QSignalBlocker colorModeBlocker(colorModeComboBox_);
        const QSignalBlocker axesBlocker(axesCheckBox_);
        const QSignalBlocker boundsBlocker(boundingBoxCheckBox_);
        const QSignalBlocker axesActionBlocker(showAxesAction_);
        const QSignalBlocker boundsActionBlocker(showBoundingBoxAction_);
        const QSignalBlocker rgbActionBlocker(rgbColorAction_);
        const QSignalBlocker elevationActionBlocker(elevationColorAction_);
        const QSignalBlocker singleActionBlocker(singleColorAction_);
        const QSignalBlocker colorfulThemeBlocker(themeColorfulAction_);
        const QSignalBlocker whiteThemeBlocker(themeWhiteAction_);
        const QSignalBlocker darkThemeBlocker(themeDarkGrayAction_);

        pointSizeSpinBox_->setValue(static_cast<int>(options.pointSize));
        colorModeComboBox_->setCurrentIndex(static_cast<int>(options.colorMode));
        axesCheckBox_->setChecked(options.showAxes);
        boundingBoxCheckBox_->setChecked(options.showBoundingBox);

        showAxesAction_->setChecked(options.showAxes);
        showBoundingBoxAction_->setChecked(options.showBoundingBox);
        rgbColorAction_->setChecked(options.colorMode == PointCloudColorMode::Rgb);
        elevationColorAction_->setChecked(options.colorMode == PointCloudColorMode::Elevation);
        singleColorAction_->setChecked(options.colorMode == PointCloudColorMode::SingleColor);

        if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
            themeColorfulAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016Colorful);
            themeWhiteAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016White);
            themeDarkGrayAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016DarkGray);
        }
    }

    setColorButtonAppearance(pointColorButton_, options.singleColor, tr("Pick Color"));
    setColorButtonAppearance(backgroundColorButton_, options.backgroundColor, tr("Pick Background"));
    updateDatasetPanel();
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
