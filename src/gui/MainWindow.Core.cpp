#include "gui/MainWindow.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QTabBar>
#include <QUrl>
#include <QWindow>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "QtnRibbonBar.h"
#include "gui/MainWindowInternal.h"
#include "gui/PointCloudViewer.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/SceneInspectorDock.h"

using namespace mainwindow_internal;

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
