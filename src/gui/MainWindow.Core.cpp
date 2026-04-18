#include "gui/MainWindow.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDockWidget>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QTabBar>
#include <QTimer>
#include <QUuid>
#include <QUrl>
#include <QWindow>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <algorithm>

#include "QtnRibbonBar.h"
#include "QtnRibbonQuickAccessBar.h"
#include "QtnRibbonSystemPopupBar.h"
#include "capture/ScreenRecorderFactory.h"
#include "gui/MainWindowInternal.h"
#include "gui/PointCloudViewer.h"
#include "gui/ProfileClassificationDock.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/RouteDetailsDock.h"
#include "gui/SceneInspectorDock.h"
#include "gui/support/UiHelpers.h"

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

namespace
{

QString makeUniqueOutputPath(const QString& candidatePath)
{
    if (!QFileInfo::exists(candidatePath)) {
        return candidatePath;
    }

    const QFileInfo candidateInfo(candidatePath);
    const QString extension = candidateInfo.suffix();
    const QString extensionPart = extension.isEmpty() ? QString() : QStringLiteral(".") + extension;
    const QString baseName = candidateInfo.completeBaseName().isEmpty()
        ? QStringLiteral("recording")
        : candidateInfo.completeBaseName();
    const QDir parentDir = candidateInfo.absoluteDir();

    int suffixIndex = 1;
    QString uniquePath;
    do {
        uniquePath = parentDir.filePath(
            QStringLiteral("%1_%2%3")
                .arg(baseName)
                .arg(suffixIndex)
                .arg(extensionPart));
        ++suffixIndex;
    } while (QFileInfo::exists(uniquePath));

    return uniquePath;
}

}

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
    screenRecorder_ = capture::createScreenRecorder();

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
    stopScreenRecording(false);
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

QString MainWindow::defaultCaptureSaveDirectory() const
{
    QString baseDirectory = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (baseDirectory.isEmpty()) {
        baseDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }
    if (baseDirectory.isEmpty()) {
        baseDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    if (baseDirectory.isEmpty()) {
        baseDirectory = QDir::homePath();
    }

    return QDir::toNativeSeparators(QDir(baseDirectory).filePath(QStringLiteral("LASViewerCaptures")));
}

QString MainWindow::createTemporaryRecordingOutputPath(const QString& preferredFileName) const
{
    QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempRoot.trimmed().isEmpty()) {
        tempRoot = QDir::tempPath();
    }

    QDir tempDir(QDir::fromNativeSeparators(tempRoot));
    if (!tempDir.mkpath(QStringLiteral("."))) {
        return QString();
    }

    if (!tempDir.cd(QStringLiteral("LASViewerRecordingTemp"))) {
        if (!tempDir.mkdir(QStringLiteral("LASViewerRecordingTemp"))) {
            return QString();
        }
        if (!tempDir.cd(QStringLiteral("LASViewerRecordingTemp"))) {
            return QString();
        }
    }

    QString baseName = QFileInfo(preferredFileName).completeBaseName().trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("recording");
    }

    const QString tempFileName = QStringLiteral("%1_%2.mp4")
        .arg(baseName)
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    return tempDir.filePath(tempFileName);
}

capture::ScreenRecordingResult MainWindow::finalizeRecordingOutputFile(const QString& temporaryFilePath, bool interactiveStop)
{
    const QFileInfo temporaryInfo(temporaryFilePath);
    if (temporaryFilePath.trimmed().isEmpty() || !temporaryInfo.exists()) {
        return capture::ScreenRecordingResult::fail(
            tr("No recording file was produced."));
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString defaultFileName = temporaryInfo.fileName().trimmed().isEmpty()
        ? QStringLiteral("recording_%1.mp4").arg(timestamp)
        : temporaryInfo.fileName();

    QString finalOutputPath;
    if (interactiveStop && !captureSkipSaveDialog_) {
        finalOutputPath = resolveCaptureOutputPath(
            tr("Save Recording"),
            defaultFileName,
            tr("MP4 Video (*.mp4)"),
            QStringLiteral("mp4"));
        if (finalOutputPath.isEmpty()) {
            QFile::remove(temporaryFilePath);
            return capture::ScreenRecordingResult::fail(QString());
        }
    } else {
        QString outputDirectoryPath = captureSaveDirectory_.trimmed();
        if (outputDirectoryPath.isEmpty()) {
            outputDirectoryPath = defaultCaptureSaveDirectory();
        }

        QDir outputDirectory(QDir::fromNativeSeparators(outputDirectoryPath));
        if (!outputDirectory.mkpath(QStringLiteral("."))) {
            return capture::ScreenRecordingResult::fail(
                tr("Unable to create the recording output folder."));
        }

        finalOutputPath = makeUniqueOutputPath(outputDirectory.filePath(defaultFileName));
    }

    finalOutputPath = QDir::toNativeSeparators(finalOutputPath);
    const QFileInfo finalInfo(finalOutputPath);
    if (!QDir(finalInfo.absolutePath()).mkpath(QStringLiteral("."))) {
        return capture::ScreenRecordingResult::fail(
            tr("Unable to create the recording output folder."));
    }

    bool moved = false;
    if (QDir::toNativeSeparators(temporaryFilePath) == finalOutputPath) {
        moved = true;
    } else {
        if (QFileInfo::exists(finalOutputPath)) {
            QFile::remove(finalOutputPath);
        }
        moved = QFile::rename(temporaryFilePath, finalOutputPath);
        if (!moved) {
            moved = QFile::copy(temporaryFilePath, finalOutputPath);
            if (moved) {
                QFile::remove(temporaryFilePath);
            }
        }
    }

    if (!moved) {
        return capture::ScreenRecordingResult::fail(
            tr("Failed to save recording: %1").arg(finalOutputPath));
    }

    captureSaveDirectory_ = QDir::toNativeSeparators(finalInfo.absolutePath());
    persistWindowSettings();
    recordingOutputFilePath_ = finalOutputPath;
    return capture::ScreenRecordingResult::ok(finalOutputPath);
}

QString MainWindow::resolveCaptureOutputPath(
    const QString& dialogTitle,
    const QString& defaultFileName,
    const QString& filter,
    const QString& requiredSuffix)
{
    QString saveDirectory = captureSaveDirectory_.trimmed();
    if (saveDirectory.isEmpty()) {
        saveDirectory = defaultCaptureSaveDirectory();
    }
    saveDirectory = QDir::toNativeSeparators(QDir::cleanPath(saveDirectory));

    if (captureSkipSaveDialog_) {
        QDir outputDirectory(QDir::fromNativeSeparators(saveDirectory));
        if (!outputDirectory.mkpath(QStringLiteral("."))) {
            showUserMessage(LogLevel::Error, tr("Unable to create the capture folder."), 4500);
            return QString();
        }
        return outputDirectory.filePath(defaultFileName);
    }

    const QString initialPath = QDir(QDir::fromNativeSeparators(saveDirectory)).filePath(defaultFileName);
    QString selectedPath = lasviewer::gui::showStyledSaveFileNameDialog(
        this,
        dialogTitle,
        QDir::toNativeSeparators(initialPath),
        filter);
    if (selectedPath.isEmpty()) {
        return QString();
    }

    if (!requiredSuffix.trimmed().isEmpty() && QFileInfo(selectedPath).suffix().trimmed().isEmpty()) {
        selectedPath += QStringLiteral(".") + requiredSuffix.trimmed();
    }

    captureSaveDirectory_ = QDir::toNativeSeparators(QFileInfo(selectedPath).absolutePath());
    persistWindowSettings();
    return selectedPath;
}

void MainWindow::captureMainWindowScreenshot()
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString defaultFileName = QStringLiteral("screenshot_%1.png").arg(timestamp);
    const QString outputPath = resolveCaptureOutputPath(
        tr("Save Screenshot"),
        defaultFileName,
        tr("PNG Images (*.png)"),
        QStringLiteral("png"));
    if (outputPath.isEmpty()) {
        return;
    }

    const QFileInfo outputInfo(outputPath);
    if (!QDir(outputInfo.absolutePath()).mkpath(QStringLiteral("."))) {
        showUserMessage(LogLevel::Error, tr("Unable to create the screenshot output folder."), 4500);
        return;
    }

    const QPixmap screenshot = grab();
    if (screenshot.isNull()) {
        showUserMessage(LogLevel::Error, tr("Screenshot failed. The window image is empty."), 4500);
        return;
    }
    if (!screenshot.save(outputPath, "PNG")) {
        showUserMessage(LogLevel::Error, tr("Failed to save screenshot: %1").arg(QDir::toNativeSeparators(outputPath)), 5000);
        return;
    }

    showUserMessage(LogLevel::Info, tr("Screenshot saved: %1").arg(outputInfo.fileName()), 3200);
}

void MainWindow::toggleScreenRecording()
{
    const bool processRecordingActive =
        recordingProcess_ != nullptr && recordingProcess_->state() != QProcess::NotRunning;
    const bool embeddedRecordingActive =
        screenRecorder_ != nullptr && screenRecorder_->isRecording();
    if (processRecordingActive || embeddedRecordingActive) {
        stopScreenRecording(true);
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString defaultFileName = QStringLiteral("recording_%1.mp4").arg(timestamp);
    const QString temporaryOutputPath = createTemporaryRecordingOutputPath(defaultFileName);
    if (temporaryOutputPath.isEmpty()) {
        showUserMessage(LogLevel::Error, tr("Unable to create temporary recording file."), 4500);
        return;
    }

    if (screenRecorder_ != nullptr && screenRecorder_->isAvailable()) {
        capture::ScreenRecordingStartOptions options;
        options.outputFilePath = temporaryOutputPath;
        options.frameRate = 30;
        options.nativeWindowHandle = static_cast<quintptr>(winId());

        const capture::ScreenRecordingResult result = screenRecorder_->startRecording(options);
        if (!result.success) {
            const QString diagnostic = result.message.trimmed().isEmpty()
                ? tr("No ffmpeg diagnostic output was captured.")
                : result.message;
            showUserMessage(
                LogLevel::Error,
                tr("Recording failed. %1").arg(diagnostic),
                6500);
            return;
        }

        recordingOutputFilePath_ = temporaryOutputPath;
        showUserMessage(
            LogLevel::Info,
            tr("Recording started. Use %1 or the ribbon button to stop.")
                .arg(toggleScreenRecordingAction_ != nullptr
                    ? toggleScreenRecordingAction_->shortcut().toString(QKeySequence::NativeText)
                    : tr("Stop Recording")),
            4200);
        updateActionState();
        return;
    }

    const QString recorderUnavailableReason =
        screenRecorder_ != nullptr ? screenRecorder_->unavailableReason().trimmed() : QString();
    const QString ffmpegExecutable = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpegExecutable.isEmpty()) {
        const QString message = recorderUnavailableReason.isEmpty()
            ? tr("Recording requires ffmpeg. Add ffmpeg to PATH or place ffmpeg.exe beside the application.")
            : tr("Embedded recording is unavailable: %1. Recording requires ffmpeg. Add ffmpeg to PATH, or enable LAS_VIEWER_ENABLE_WINDOWS_CAPTURE in your build.")
                .arg(recorderUnavailableReason);
        showUserMessage(
            LogLevel::Error,
            message,
            5500);
        return;
    }

    recordingStopRequested_ = false;
    suppressRecordingStopMessage_ = false;
    recordingOutputFilePath_ = temporaryOutputPath;
    recordingProcess_ = new QProcess(this);
    recordingProcess_->setProgram(ffmpegExecutable);
    recordingProcess_->setProcessChannelMode(QProcess::MergedChannels);
    recordingProcess_->setArguments({
        QStringLiteral("-y"),
        QStringLiteral("-f"),
        QStringLiteral("gdigrab"),
        QStringLiteral("-framerate"),
        QStringLiteral("30"),
        QStringLiteral("-i"),
        QStringLiteral("title=%1").arg(windowTitle()),
        QStringLiteral("-vcodec"),
        QStringLiteral("libx264"),
        QStringLiteral("-preset"),
        QStringLiteral("veryfast"),
        QStringLiteral("-pix_fmt"),
        QStringLiteral("yuv420p"),
        temporaryOutputPath
    });

    connect(
        recordingProcess_,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
            QProcess* finishedProcess = recordingProcess_;
            QString processOutput;
            if (finishedProcess != nullptr) {
                processOutput = QString::fromLocal8Bit(finishedProcess->readAllStandardOutput()).trimmed();
                finishedProcess->deleteLater();
            }

            recordingProcess_ = nullptr;
            const bool stoppedByUser = recordingStopRequested_;
            recordingStopRequested_ = false;
            const bool interactiveStop = !suppressRecordingStopMessage_;

            const bool success = exitStatus == QProcess::NormalExit
                && (exitCode == 0 || stoppedByUser)
                && QFileInfo::exists(recordingOutputFilePath_);
            if (success) {
                const capture::ScreenRecordingResult saveResult =
                    finalizeRecordingOutputFile(recordingOutputFilePath_, interactiveStop);
                if (interactiveStop) {
                    if (saveResult.success) {
                        showUserMessage(
                            LogLevel::Info,
                            tr("Recording saved: %1").arg(QFileInfo(recordingOutputFilePath_).fileName()),
                            3800);
                    } else if (!saveResult.message.trimmed().isEmpty()) {
                        showUserMessage(
                            LogLevel::Error,
                            tr("Recording failed. %1").arg(saveResult.message),
                            6500);
                    }
                }
            } else if (interactiveStop) {
                const QString diagnostic = processOutput.isEmpty()
                    ? tr("No ffmpeg diagnostic output was captured.")
                    : processOutput;
                showUserMessage(
                    LogLevel::Error,
                    tr("Recording failed. %1").arg(diagnostic),
                    6500);
            }

            suppressRecordingStopMessage_ = false;
            updateActionState();
        });

    recordingProcess_->start();
    if (!recordingProcess_->waitForStarted(3000)) {
        const QString diagnostic = QString::fromLocal8Bit(recordingProcess_->readAllStandardOutput()).trimmed();
        recordingProcess_->deleteLater();
        recordingProcess_ = nullptr;
        QFile::remove(temporaryOutputPath);
        showUserMessage(
            LogLevel::Error,
            diagnostic.isEmpty()
                ? tr("Failed to start recording process.")
                : tr("Failed to start recording process. %1").arg(diagnostic),
            6000);
        return;
    }

    showUserMessage(
        LogLevel::Info,
        tr("Recording started. Use %1 or the ribbon button to stop.")
            .arg(toggleScreenRecordingAction_ != nullptr
                ? toggleScreenRecordingAction_->shortcut().toString(QKeySequence::NativeText)
                : tr("Stop Recording")),
        4200);
    updateActionState();
}

void MainWindow::stopScreenRecording(bool notifyUser)
{
    if (screenRecorder_ != nullptr && screenRecorder_->isRecording()) {
        const capture::ScreenRecordingResult result = screenRecorder_->stopRecording();
        if (result.success) {
            const capture::ScreenRecordingResult saveResult =
                finalizeRecordingOutputFile(recordingOutputFilePath_, notifyUser);
            if (notifyUser) {
                if (saveResult.success) {
                    showUserMessage(
                        LogLevel::Info,
                        tr("Recording saved: %1").arg(QFileInfo(recordingOutputFilePath_).fileName()),
                        3800);
                } else if (!saveResult.message.trimmed().isEmpty()) {
                    showUserMessage(
                        LogLevel::Error,
                        tr("Recording failed. %1").arg(saveResult.message),
                        6500);
                }
            }
        } else if (notifyUser) {
            const QString diagnostic = result.message.trimmed().isEmpty()
                ? tr("No ffmpeg diagnostic output was captured.")
                : result.message;
                showUserMessage(
                    LogLevel::Error,
                    tr("Recording failed. %1").arg(diagnostic),
                    6500);
        }
        updateActionState();
        return;
    }

    if (recordingProcess_ == nullptr || recordingProcess_->state() == QProcess::NotRunning) {
        return;
    }

    recordingStopRequested_ = true;
    suppressRecordingStopMessage_ = !notifyUser;
    recordingProcess_->write("q\n");
    recordingProcess_->waitForBytesWritten(300);
    if (!recordingProcess_->waitForFinished(2800)) {
        recordingProcess_->kill();
    }
}
