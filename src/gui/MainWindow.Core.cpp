#include "gui/MainWindow.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QScreen>
#include <QTabBar>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <algorithm>

#include "QtnRibbonBar.h"
#include "QtnRibbonQuickAccessBar.h"
#include "QtnRibbonSystemPopupBar.h"
#include "gui/MainWindowInternal.h"
#include "gui/PointCloudViewer.h"
#include "gui/ProfileClassificationDock.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/RouteDetailsDock.h"
#include "gui/SceneInspectorDock.h"

using namespace mainwindow_internal;

#ifdef Q_OS_WIN
namespace
{
constexpr int kDefaultCustomCaptionHeight = 48;

struct TopLevelWindowSearchContext
{
    DWORD processId = 0;
    QString expectedTitle;
    HWND exactMatchWindow = nullptr;
    HWND window = nullptr;
    LONG bestArea = -1;
};

BOOL CALLBACK findLargestVisibleQtWindowCallback(HWND hwnd, LPARAM lParam)
{
    auto* context = reinterpret_cast<TopLevelWindowSearchContext*>(lParam);
    if (context == nullptr) {
        return FALSE;
    }

    DWORD windowProcessId = 0;
    GetWindowThreadProcessId(hwnd, &windowProcessId);
    if (windowProcessId != context->processId || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr) {
        return TRUE;
    }

    wchar_t className[256] = {};
    const int classNameLength = GetClassNameW(hwnd, className, static_cast<int>(sizeof(className) / sizeof(className[0])));
    if (classNameLength <= 0 || !QString::fromWCharArray(className, classNameLength).startsWith(QStringLiteral("Qt"), Qt::CaseInsensitive)) {
        return TRUE;
    }

    wchar_t windowTitle[512] = {};
    const int windowTitleLength = GetWindowTextW(hwnd, windowTitle, static_cast<int>(sizeof(windowTitle) / sizeof(windowTitle[0])));
    const QString title = QString::fromWCharArray(windowTitle, windowTitleLength);
    if (!context->expectedTitle.isEmpty() && title == context->expectedTitle) {
        context->exactMatchWindow = hwnd;
        return FALSE;
    }

    RECT windowRect {};
    if (!GetWindowRect(hwnd, &windowRect)) {
        return TRUE;
    }

    const LONG width = windowRect.right - windowRect.left;
    const LONG height = windowRect.bottom - windowRect.top;
    const LONG area = width * height;
    if (area <= context->bestArea) {
        return TRUE;
    }

    context->bestArea = area;
    context->window = hwnd;
    return TRUE;
}

HWND resolveVisibleTopLevelQtWindow(const QString& expectedTitle)
{
    TopLevelWindowSearchContext context;
    context.processId = GetCurrentProcessId();
    context.expectedTitle = expectedTitle;
    EnumWindows(findLargestVisibleQtWindowCallback, reinterpret_cast<LPARAM>(&context));
    return context.exactMatchWindow != nullptr ? context.exactMatchWindow : context.window;
}

void applyMaximizedWorkArea(HWND hwnd, MINMAXINFO* minMaxInfo)
{
    if (hwnd == nullptr || minMaxInfo == nullptr) {
        return;
    }

    MONITORINFO monitorInfo {};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitorInfo)) {
        return;
    }

    const RECT& monitorRect = monitorInfo.rcMonitor;
    const RECT& workRect = monitorInfo.rcWork;
    minMaxInfo->ptMaxPosition.x = workRect.left - monitorRect.left;
    minMaxInfo->ptMaxPosition.y = workRect.top - monitorRect.top;
    minMaxInfo->ptMaxSize.x = workRect.right - workRect.left;
    minMaxInfo->ptMaxSize.y = workRect.bottom - workRect.top;
    minMaxInfo->ptMaxTrackSize = minMaxInfo->ptMaxSize;
}

void applyCustomWindowStyle(HWND hwnd)
{
    if (hwnd == nullptr) {
        return;
    }

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    const LONG_PTR desiredStyle =
        (style & ~static_cast<LONG_PTR>(WS_POPUP))
        | static_cast<LONG_PTR>(
            WS_OVERLAPPED
            | WS_CAPTION
            | WS_SYSMENU
            | WS_THICKFRAME
            | WS_MINIMIZEBOX
            | WS_MAXIMIZEBOX);
    if (style == desiredStyle) {
        return;
    }

    SetWindowLongPtr(hwnd, GWL_STYLE, desiredStyle);
    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
}

void beginNativeSystemMove(HWND hwnd)
{
    if (hwnd == nullptr) {
        return;
    }

    ReleaseCapture();
    SendMessageW(hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}

QPoint pointFromNativeLParam(LPARAM lParam)
{
    return QPoint(
        static_cast<int>(static_cast<short>(LOWORD(lParam))),
        static_cast<int>(static_cast<short>(HIWORD(lParam))));
}

}
#endif

MainWindow::MainWindow(QTranslator* appTranslator, QTranslator* qtTranslator, QWidget* parent)
    : Qtitan::RibbonMainWindow(parent)
    , appTranslator_(appTranslator)
    , qtTranslator_(qtTranslator)
{
    setWindowFlag(Qt::WindowMinimizeButtonHint, true);
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    setWindowFlag(Qt::WindowCloseButtonHint, true);
    resize(1520, 920);
    setMinimumSize(960, 640);
    setAcceptDrops(true);

    loadLanguageSettings();

    viewer_ = new PointCloudViewer(this);
    setCentralWidget(viewer_);
    viewer_->setInspectionRouteEditingEnabled(routeEditingEnabled_);

    createActions();
    createRibbon();
    createViewQuickToolBar();
    createProjectDock();
    createInspectorPanel();
    createRouteDetailsDock();
    createProfileClassificationDock();
    createProfileDock();
    createLogDock();
    createStatusBar();
    setDockNestingEnabled(true);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    applyDefaultDockWidths(this, projectDock_, inspectorDock_);
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
    persistWindowSettings(true);
    Qtitan::RibbonMainWindow::closeEvent(event);
    if (event != nullptr && !event->isAccepted()) {
        closingWindow_ = false;
    }
}

bool MainWindow::event(QEvent* event)
{
    if (event != nullptr && event->type() == QEvent::Close) {
        closingWindow_ = true;
    }

    return Qtitan::RibbonMainWindow::event(event);
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
        const bool hasExistingPointCloud = viewer_ != nullptr && !viewer_->currentFilePaths().isEmpty();
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
#ifdef Q_OS_WIN
    QWidget* watchedWidget = qobject_cast<QWidget*>(watched);
    if (event != nullptr
        && watchedWidget != nullptr
        && ribbonBar_ != nullptr
        && (watchedWidget == ribbonBar_ || ribbonBar_->isAncestorOf(watchedWidget))
        && !isFullScreen()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton
                && isDraggableRibbonArea(ribbonBar_->mapFromGlobal(mouseEvent->globalPos()))) {
                pendingRibbonWindowMove_ = true;
                pendingRibbonWindowMoveGlobalPos_ = mouseEvent->globalPos();
            } else {
                pendingRibbonWindowMove_ = false;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (pendingRibbonWindowMove_
                && (mouseEvent->buttons() & Qt::LeftButton)
                && (mouseEvent->globalPos() - pendingRibbonWindowMoveGlobalPos_).manhattanLength() >= QApplication::startDragDistance()) {
                pendingRibbonWindowMove_ = false;

                bool startedSystemMove = false;
                if (QWindow* win = windowHandle()) {
                    startedSystemMove = win->startSystemMove();
                }
                if (!startedSystemMove) {
                    HWND hwnd = resolveVisibleTopLevelQtWindow(windowTitle());
                    if (hwnd == nullptr) {
                        hwnd = reinterpret_cast<HWND>(winId());
                    }
                    beginNativeSystemMove(hwnd);
                    startedSystemMove = true;
                }
                if (startedSystemMove) {
                    event->accept();
                    return true;
                }
            }
            break;
        }
        case QEvent::MouseButtonDblClick: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            pendingRibbonWindowMove_ = false;
            if (mouseEvent->button() == Qt::LeftButton
                && isDraggableRibbonArea(ribbonBar_->mapFromGlobal(mouseEvent->globalPos()))) {
                toggleMaximizedWindow();
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
        case QEvent::Leave:
            pendingRibbonWindowMove_ = false;
            break;
        default:
            break;
        }
    }
#endif

    return Qtitan::RibbonMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent* event)
{
    Qtitan::RibbonMainWindow::changeEvent(event);

    if (event == nullptr) {
        return;
    }

    if (event->type() == QEvent::WindowStateChange) {
#ifdef Q_OS_WIN
        normalizeNativeWindowStyle();
#endif
        updateWindowControlButtons();
    } else if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

void MainWindow::showEvent(QShowEvent* event)
{
    Qtitan::RibbonMainWindow::showEvent(event);
    scheduleDockPanelSizing();
    QTimer::singleShot(120, this, [this]() { scheduleDockPanelSizing(); });
    QTimer::singleShot(400, this, [this]() { scheduleDockPanelSizing(); });

    if (!dockScreenTrackingConnected_) {
        if (QWindow* win = windowHandle()) {
            connect(
                win,
                &QWindow::screenChanged,
                this,
                [this](QScreen*) { scheduleDockPanelSizing(); });
            dockScreenTrackingConnected_ = true;
        }
    }
#ifdef Q_OS_WIN
    normalizeNativeWindowStyle();
    QTimer::singleShot(120, this, [this]() { normalizeNativeWindowStyle(); });
    QTimer::singleShot(400, this, [this]() { normalizeNativeWindowStyle(); });
    QTimer::singleShot(1200, this, [this]() { normalizeNativeWindowStyle(); });
#endif
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, long* result)
{
    Q_UNUSED(eventType);

    if (message != nullptr && result != nullptr) {
        MSG* nativeMessage = static_cast<MSG*>(message);
        if (nativeMessage->message == WM_NCCALCSIZE && !isFullScreen()) {
            *result = 0;
            return true;
        }

        if (nativeMessage->message == WM_GETMINMAXINFO) {
            applyMaximizedWorkArea(nativeMessage->hwnd, reinterpret_cast<MINMAXINFO*>(nativeMessage->lParam));
            *result = 0;
            return true;
        }

        if (nativeMessage->message == WM_NCHITTEST && !isFullScreen()) {
            const QPoint globalPosition = pointFromNativeLParam(nativeMessage->lParam);
            const QPoint localPosition = mapFromGlobal(globalPosition);
            if (rect().contains(localPosition)) {
                QWidget* const hoveredWidget = childAt(localPosition);
                if (!isMaximized() && !isWindowControlWidget(hoveredWidget)) {
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

                if (ribbonBar_ != nullptr
                    && ribbonBar_->geometry().contains(localPosition)
                    && isDraggableRibbonArea(ribbonBar_->mapFromGlobal(globalPosition))) {
                    *result = HTCAPTION;
                    return true;
                }

            }
        }
    }

    return Qtitan::RibbonMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::normalizeNativeWindowStyle()
{
    HWND hwnd = resolveVisibleTopLevelQtWindow(windowTitle());
    if (hwnd == nullptr) {
        if (QWindow* win = windowHandle()) {
            hwnd = reinterpret_cast<HWND>(win->winId());
        }
    }
    if (hwnd == nullptr) {
        hwnd = reinterpret_cast<HWND>(winId());
    }
    applyCustomWindowStyle(hwnd);
}
#endif

void MainWindow::scheduleDockPanelSizing()
{
    if (dockPanelSizingPending_) {
        return;
    }

    dockPanelSizingPending_ = true;
    QTimer::singleShot(0, this, [this]() { normalizeDockPanelSizing(); });
}

void MainWindow::normalizeDockPanelSizing()
{
    dockPanelSizingPending_ = false;

    if (isMinimized()) {
        return;
    }

    const int projectMinimumWidth = adaptiveDockWidth(this, 0.14, 220, 280);
    const int projectMaximumWidth = adaptiveDockWidth(this, 0.18, projectMinimumWidth, 320);
    const int inspectorMinimumWidth = adaptiveDockWidth(this, 0.14, 240, 300);
    const int routeDetailsMinimumWidth = adaptiveDockWidth(this, 0.14, 240, 300);
    const int profileClassificationMinimumWidth = adaptiveDockWidth(this, 0.16, 240, 300);
    const int profileClassificationMaximumWidth = adaptiveDockWidth(this, 0.19, profileClassificationMinimumWidth, 340);

    if (projectDock_ != nullptr) {
        projectDock_->setMinimumWidth(projectMinimumWidth);
    }
    if (inspectorDock_ != nullptr) {
        inspectorDock_->setMinimumWidth(inspectorMinimumWidth);
    }
    if (routeDetailsDock_ != nullptr) {
        routeDetailsDock_->setMinimumWidth(routeDetailsMinimumWidth);
    }
    if (profileClassificationDock_ != nullptr) {
        profileClassificationDock_->setMinimumWidth(profileClassificationMinimumWidth);
    }

    auto shrinkDockWidth = [this](QDockWidget* dock, int maximumWidth) {
        if (dock == nullptr || dock->isFloating() || !dock->isVisible()) {
            return;
        }

        const int currentWidth = dock->width();
        if (currentWidth > maximumWidth) {
            resizeDocks({ dock }, { maximumWidth }, Qt::Horizontal);
        }
    };

    shrinkDockWidth(projectDock_, projectMaximumWidth);
    shrinkDockWidth(profileClassificationDock_, profileClassificationMaximumWidth);
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

    const int ribbonHeight = ribbonBar_->height();
    if (ribbonHeight <= 0) {
        return false;
    }

    int captionHeight = ribbonBar_->titleBarHeight();
    if (captionHeight <= 0) {
        captionHeight = std::min(kDefaultCustomCaptionHeight, ribbonHeight);
    } else {
        captionHeight = std::min(captionHeight, ribbonHeight);
    }
    if (position.y() < 0 || position.y() >= captionHeight) {
        return false;
    }

    auto containsRibbonChild = [this, &position](const QWidget* widget) {
        if (widget == nullptr || !widget->isVisible()) {
            return false;
        }
        const QRect widgetRect(ribbonBar_->mapFromGlobal(widget->mapToGlobal(QPoint(0, 0))), widget->size());
        return widgetRect.contains(position);
    };

    if (containsRibbonChild(windowControlsWidget_)) {
        return false;
    }
    if (containsRibbonChild(ribbonBar_->getSystemButton())) {
        return false;
    }
    if (containsRibbonChild(ribbonBar_->quickAccessBar())) {
        return false;
    }

    QWidget* child = ribbonBar_->childAt(position);
    return !isWindowControlWidget(child) && !isInteractiveRibbonWidget(child);
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
            || qobject_cast<const QMenu*>(current) != nullptr
            || className.contains(QStringLiteral("RibbonTab"), Qt::CaseInsensitive)
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
