#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QOpenGLWidget>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSet>
#include <QSlider>
#include <QMouseEvent>
#include <QMenu>
#include <QSpinBox>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTranslator>
#include <QTimer>
#include <QLineEdit>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

#include "crs/CrsAuthorityService.h"
#include "domain/InspectionData.h"
#include "domain/TowerFileInterop.h"
#include "gui/ApplicationLogDock.h"
#include "gui/IssueController.h"
#define private public
#include "gui/MainWindow.h"
#include "gui/PointCloudViewer.h"
#undef private
#include "gui/MeasurementAnalysisController.h"
#include "gui/NavigationSettingsWidget.h"
#include "gui/ProfileClassificationController.h"
#include "gui/ProfileClassificationDock.h"
#include "gui/ProfileClassificationWidget.h"
#include "gui/ProjectExplorerController.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/RouteDetailsDock.h"
#include "gui/RouteController.h"
#include "gui/SceneInspectorDock.h"
#include "gui/SpanProfileDock.h"
#include "gui/TowerController.h"
#include "gui/VisualizationPanelController.h"
#include "logging/ApplicationLogger.h"
#include "route/InspectionRoutePlanning.h"
#include "route/PowerlineRouteBridge.h"
#include "route/PowerlineRouteJson.h"
#include "route/RouteInterop.h"

#include "QtnRibbonBackstageView.h"
#include "QtnRibbonBar.h"
#include "QtnRibbonSystemPopupBar.h"

namespace
{
struct SmokeCase
{
    QString mode;
    QString category;
    QString displayName;
    bool requiresLas = false;
    std::function<bool(const QStringList&)> run;
};

PowerlineRouteDocument buildSyntheticRoute();

void pumpEvents(int durationMs)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < durationMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(25);
    }
}

bool verify(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << std::endl;
        return false;
    }
    return true;
}

bool verifyClose(double left, double right, double tolerance, const std::string& message)
{
    if (std::abs(left - right) > tolerance) {
        std::cerr << "[FAIL] " << message << " left=" << left << " right=" << right << std::endl;
        return false;
    }
    return true;
}

bool invokeTableContextMenuAndClose(QTableWidget* table, const QPoint& position, const std::string& message)
{
    if (table == nullptr) {
        std::cerr << "[FAIL] " << message << " table is null" << std::endl;
        return false;
    }

    bool popupClosed = false;
    const auto closePopup = [&popupClosed]() {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
            popupClosed = true;
            menu->close();
        }
    };
    QTimer::singleShot(0, closePopup);
    QTimer::singleShot(25, closePopup);
    QTimer::singleShot(80, closePopup);

    const bool invoked = QMetaObject::invokeMethod(
        table,
        "customContextMenuRequested",
        Qt::DirectConnection,
        Q_ARG(QPoint, position));
    if (!invoked) {
        std::cerr << "[FAIL] " << message << " invokeMethod failed" << std::endl;
        return false;
    }

    pumpEvents(120);
    if (!popupClosed) {
        std::cerr << "[FAIL] " << message << " popup menu did not close" << std::endl;
        return false;
    }
    return true;
}

bool invokeTreeContextMenuAndClose(QTreeWidget* tree, const QPoint& position, const std::string& message)
{
    if (tree == nullptr) {
        std::cerr << "[FAIL] " << message << " tree is null" << std::endl;
        return false;
    }

    bool popupClosed = false;
    const auto closePopup = [&popupClosed]() {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
            popupClosed = true;
            menu->close();
        }
    };
    QTimer::singleShot(0, closePopup);
    QTimer::singleShot(25, closePopup);
    QTimer::singleShot(80, closePopup);

    const bool invoked = QMetaObject::invokeMethod(
        tree,
        "customContextMenuRequested",
        Qt::DirectConnection,
        Q_ARG(QPoint, position));
    if (!invoked) {
        std::cerr << "[FAIL] " << message << " invokeMethod failed" << std::endl;
        return false;
    }

    pumpEvents(120);
    if (!popupClosed) {
        std::cerr << "[FAIL] " << message << " popup menu did not close" << std::endl;
        return false;
    }
    return true;
}

bool emitTableDoubleClick(QTableWidget* table, int row, int column, const std::string& message)
{
    if (table == nullptr) {
        std::cerr << "[FAIL] " << message << " table is null" << std::endl;
        return false;
    }

    const bool invoked = QMetaObject::invokeMethod(
        table,
        "cellDoubleClicked",
        Qt::DirectConnection,
        Q_ARG(int, row),
        Q_ARG(int, column));
    if (!invoked) {
        std::cerr << "[FAIL] " << message << " invokeMethod failed" << std::endl;
        return false;
    }

    pumpEvents(40);
    return true;
}

bool emitTreeItemDoubleClick(QTreeWidget* tree, QTreeWidgetItem* item, int column, const std::string& message)
{
    if (tree == nullptr) {
        std::cerr << "[FAIL] " << message << " tree is null" << std::endl;
        return false;
    }
    if (item == nullptr) {
        std::cerr << "[FAIL] " << message << " item is null" << std::endl;
        return false;
    }

    const bool invoked = QMetaObject::invokeMethod(
        tree,
        "itemDoubleClicked",
        Qt::DirectConnection,
        Q_ARG(QTreeWidgetItem*, item),
        Q_ARG(int, column));
    if (!invoked) {
        std::cerr << "[FAIL] " << message << " invokeMethod failed" << std::endl;
        return false;
    }

    pumpEvents(40);
    return true;
}

bool clickColorButtonAndAccept(QPushButton* button, const QColor& color, const std::string& message)
{
    if (button == nullptr) {
        std::cerr << "[FAIL] " << message << " button is null" << std::endl;
        return false;
    }

    bool dialogAccepted = false;
    QTimer::singleShot(0, [color, &dialogAccepted]() {
        auto* dialog = qobject_cast<QColorDialog*>(QApplication::activeModalWidget());
        if (dialog != nullptr) {
            dialog->setCurrentColor(color);
            dialogAccepted = true;
            dialog->accept();
        }
    });

    button->click();
    pumpEvents(160);
    if (!dialogAccepted) {
        std::cerr << "[FAIL] " << message << " color dialog was not accepted" << std::endl;
        return false;
    }
    return true;
}

QTreeWidgetItem* findProjectTreeItem(
    QTreeWidget* tree,
    const std::function<bool(QTreeWidgetItem*)>& predicate)
{
    if (tree == nullptr) {
        return nullptr;
    }

    const auto findInBranch = [&](auto&& self, QTreeWidgetItem* item) -> QTreeWidgetItem* {
        if (item == nullptr) {
            return nullptr;
        }
        if (predicate(item)) {
            return item;
        }
        for (int childIndex = 0; childIndex < item->childCount(); ++childIndex) {
            if (QTreeWidgetItem* matchedItem = self(self, item->child(childIndex))) {
                return matchedItem;
            }
        }
        return nullptr;
    };

    for (int rootIndex = 0; rootIndex < tree->topLevelItemCount(); ++rootIndex) {
        if (QTreeWidgetItem* matchedItem = findInBranch(findInBranch, tree->topLevelItem(rootIndex))) {
            return matchedItem;
        }
    }

    return nullptr;
}

InspectionRouteDisplayData buildSmokeRouteDisplayData(const PowerlineRouteDocument& route)
{
    InspectionRouteDisplayData displayData;
    QHash<int, RoutePartPoint> partPointByIndex;
    for (const RoutePartPoint& partPoint : route.partPoints) {
        displayData.partPoints.append(partPoint.localPoint);
        displayData.partLabels.append(partPoint.partName);
        displayData.partPointIndices.append(partPoint.partIndex);
        if (partPoint.partIndex > 0) {
            partPointByIndex.insert(partPoint.partIndex, partPoint);
        }
    }

    for (int waypointIndex = 0; waypointIndex < route.waypoints.size(); ++waypointIndex) {
        const RouteWaypoint& waypoint = route.waypoints.at(waypointIndex);
        displayData.waypoints.append(waypoint.localPoint);
        displayData.labels.append(QString::number(waypointIndex + 1));
        displayData.waypointAircraftYawDegs.append(waypoint.aircraftYawDeg);
        displayData.waypointGimbalPitchDegs.append(waypoint.gimbalPitchDeg);

        QList<PointRecord> allTargetPoints;
        QList<int> allTargetPartIndices;
        QList<double> allCameraYawDegs;
        QList<double> allCameraPitchDegs;
        QList<double> allFocalLengthRatios;
        QStringList allTargetLabels;
        for (const RouteCaptureTarget& captureTarget : waypoint.captureTargets) {
            PointRecord resolvedTargetPoint = captureTarget.targetLocalPoint;
            if (captureTarget.partIndex > 0
                && partPointByIndex.contains(captureTarget.partIndex)
                && resolvedTargetPoint.x == 0.0f
                && resolvedTargetPoint.y == 0.0f
                && resolvedTargetPoint.z == 0.0f) {
                resolvedTargetPoint = partPointByIndex.value(captureTarget.partIndex).localPoint;
            }

            allTargetPoints.append(resolvedTargetPoint);
            allTargetPartIndices.append(captureTarget.partIndex);
            allCameraYawDegs.append(captureTarget.cameraYawDeg);
            allCameraPitchDegs.append(captureTarget.cameraPitchDeg);
            allFocalLengthRatios.append(captureTarget.focalLengthRatio);
            allTargetLabels.append(captureTarget.partName);
        }

        if (!waypoint.captureTargets.isEmpty()) {
            const RouteCaptureTarget& firstTarget = waypoint.captureTargets.first();
            PointRecord firstTargetPoint = firstTarget.targetLocalPoint;
            if (firstTarget.partIndex > 0
                && partPointByIndex.contains(firstTarget.partIndex)
                && firstTargetPoint.x == 0.0f
                && firstTargetPoint.y == 0.0f
                && firstTargetPoint.z == 0.0f) {
                firstTargetPoint = partPointByIndex.value(firstTarget.partIndex).localPoint;
            }

            displayData.waypointHasTargetPoints.append(true);
            displayData.waypointTargetPoints.append(firstTargetPoint);
            displayData.waypointCameraYawDegs.append(firstTarget.cameraYawDeg);
            displayData.waypointCameraPitchDegs.append(firstTarget.cameraPitchDeg);
            displayData.waypointFocalLengthRatios.append(firstTarget.focalLengthRatio);
            displayData.waypointTargetLabels.append(firstTarget.partName);
        } else {
            displayData.waypointHasTargetPoints.append(false);
            displayData.waypointTargetPoints.append(PointRecord());
            displayData.waypointCameraYawDegs.append(0.0);
            displayData.waypointCameraPitchDegs.append(0.0);
            displayData.waypointFocalLengthRatios.append(1.0);
            displayData.waypointTargetLabels.append(QString());
        }

        displayData.waypointAllTargetPoints.append(allTargetPoints);
        displayData.waypointAllTargetPartIndices.append(allTargetPartIndices);
        displayData.waypointAllCameraYawDegs.append(allCameraYawDegs);
        displayData.waypointAllCameraPitchDegs.append(allCameraPitchDegs);
        displayData.waypointAllFocalLengthRatios.append(allFocalLengthRatios);
        displayData.waypointAllTargetLabels.append(allTargetLabels);
    }

    return displayData;
}

#ifdef Q_OS_WIN
HWND findVisibleProcessTopLevelWindow(const QString& expectedTitle = QString())
{
    struct WindowSearchContext
    {
        DWORD processId = 0;
        QString expectedTitle;
        HWND exactMatchWindow = nullptr;
        HWND window = nullptr;
        LONG bestArea = -1;
    } context;

    context.processId = GetCurrentProcessId();
    context.expectedTitle = expectedTitle;
    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* context = reinterpret_cast<WindowSearchContext*>(lParam);
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
        },
        reinterpret_cast<LPARAM>(&context));
    return context.exactMatchWindow != nullptr ? context.exactMatchWindow : context.window;
}

bool verifyWindowHasResizeFrame(HWND hwnd, const std::string& message)
{
    if (hwnd == nullptr) {
        std::cerr << "[FAIL] " << message << " hwnd is null" << std::endl;
        return false;
    }

    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    const bool hasCaption = (style & WS_CAPTION) == WS_CAPTION;
    const bool hasThickFrame = (style & WS_THICKFRAME) == WS_THICKFRAME;
    const bool hasMinimizeBox = (style & WS_MINIMIZEBOX) == WS_MINIMIZEBOX;
    const bool hasMaximizeBox = (style & WS_MAXIMIZEBOX) == WS_MAXIMIZEBOX;
    const bool hasPopup = (style & WS_POPUP) == WS_POPUP;
    if (!hasCaption || !hasThickFrame || !hasMinimizeBox || !hasMaximizeBox || hasPopup) {
        std::cerr << "[FAIL] " << message
                  << " style=0x" << std::hex << static_cast<unsigned long long>(style) << std::dec
                  << " caption=" << hasCaption
                  << " thickFrame=" << hasThickFrame
                  << " minimizeBox=" << hasMinimizeBox
                  << " maximizeBox=" << hasMaximizeBox
                  << " popup=" << hasPopup
                  << std::endl;
        return false;
    }

    return true;
}

bool verifyWindowUsesWorkArea(HWND hwnd, const std::string& message)
{
    if (hwnd == nullptr) {
        std::cerr << "[FAIL] " << message << " hwnd is null" << std::endl;
        return false;
    }

    RECT windowRect {};
    if (!GetWindowRect(hwnd, &windowRect)) {
        std::cerr << "[FAIL] " << message << " GetWindowRect failed" << std::endl;
        return false;
    }

    MONITORINFO monitorInfo {};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitorInfo)) {
        std::cerr << "[FAIL] " << message << " GetMonitorInfo failed" << std::endl;
        return false;
    }

    const RECT& workRect = monitorInfo.rcWork;
    constexpr int kMaximizedBorderTolerance = 12;
    const bool matchesWorkAreaWithFrameTolerance =
        windowRect.left >= workRect.left - kMaximizedBorderTolerance
        && windowRect.left <= workRect.left
        && windowRect.top >= workRect.top - kMaximizedBorderTolerance
        && windowRect.top <= workRect.top
        && windowRect.right >= workRect.right
        && windowRect.right <= workRect.right + kMaximizedBorderTolerance
        && windowRect.bottom >= workRect.bottom
        && windowRect.bottom <= workRect.bottom + kMaximizedBorderTolerance;
    if (!matchesWorkAreaWithFrameTolerance) {
        std::cerr << "[FAIL] " << message
                  << " windowRect=[" << windowRect.left << "," << windowRect.top << "]-[" << windowRect.right << "," << windowRect.bottom << "]"
                  << " workRect=[" << workRect.left << "," << workRect.top << "]-[" << workRect.right << "," << workRect.bottom << "]"
                  << std::endl;
        return false;
    }

    return true;
}

QPoint findCaptionHitPoint(HWND hwnd, Qtitan::RibbonBar* ribbonBar)
{
    if (hwnd == nullptr || ribbonBar == nullptr) {
        return QPoint();
    }

    int titleBandHeight = ribbonBar->titleBarHeight();
    if (titleBandHeight <= 0) {
        titleBandHeight = std::min(48, ribbonBar->height());
    } else {
        titleBandHeight = std::min(titleBandHeight, ribbonBar->height());
    }

    for (int y = 4; y < titleBandHeight; y += 3) {
        for (int x = 8; x < ribbonBar->width() - 8; x += 8) {
            const QPoint globalPoint = ribbonBar->mapToGlobal(QPoint(x, y));
            const LRESULT hitResult = SendMessageW(
                hwnd,
                WM_NCHITTEST,
                0,
                MAKELPARAM(globalPoint.x(), globalPoint.y()));
            if (hitResult == HTCAPTION) {
                return globalPoint;
            }
        }
    }

    return QPoint();
}
#endif

bool hasVisiblePixels(const QImage& image, int* nonBackgroundPixelCount)
{
    if (image.isNull()) {
        if (nonBackgroundPixelCount != nullptr) {
            *nonBackgroundPixelCount = 0;
        }
        return false;
    }

    const QRgb background = image.pixel(0, 0);
    int count = 0;

    for (int y = 0; y < image.height(); ++y) {
        const QRgb* scanLine = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = scanLine[x];
            const int redDelta = std::abs(qRed(pixel) - qRed(background));
            const int greenDelta = std::abs(qGreen(pixel) - qGreen(background));
            const int blueDelta = std::abs(qBlue(pixel) - qBlue(background));
            if (redDelta > 2 || greenDelta > 2 || blueDelta > 2) {
                ++count;
            }
        }
    }

    if (nonBackgroundPixelCount != nullptr) {
        *nonBackgroundPixelCount = count;
    }

    return count > 100;
}

bool runViewerRenderSmoke(const QStringList& filePaths)
{
    bool allPassed = true;

    auto runOrbitDragAndCaptureEventPosition = [](OsgWidget* widget, const QPointF& startPoint, const QPointF& dragDelta) {
        if (widget == nullptr) {
            return QPointF();
        }

        const QPointF endPoint = startPoint + dragDelta;

        QMouseEvent pressEvent(
            QEvent::MouseButtonPress,
            startPoint,
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(widget, &pressEvent);

        QMouseEvent moveEvent(
            QEvent::MouseMove,
            endPoint,
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(widget, &moveEvent);

        const QPointF adjustedPosition = widget->lastOrbitEventPosition_;

        QMouseEvent releaseEvent(
            QEvent::MouseButtonRelease,
            endPoint,
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(widget, &releaseEvent);

        return adjustedPosition;
    };

    for (const QString& filePath : filePaths) {
        PointCloudViewer viewer;
        viewer.resize(1024, 768);
        viewer.show();

        pumpEvents(500);

        QString errorMessage;
        if (!viewer.loadPointCloud(filePath, &errorMessage)) {
            std::cerr << "Load failed for " << filePath.toStdString() << ": "
                      << errorMessage.toStdString() << std::endl;
            allPassed = false;
            continue;
        }

        pumpEvents(1000);

        QOpenGLWidget* glWidget = viewer.findChild<QOpenGLWidget*>();
        if (glWidget == nullptr) {
            std::cerr << "No QOpenGLWidget found for " << filePath.toStdString() << std::endl;
            allPassed = false;
            continue;
        }

        const QImage frame = glWidget->grabFramebuffer();
        if (frame.isNull()) {
            std::cerr << "grabFramebuffer() returned a null image for "
                      << filePath.toStdString() << std::endl;
            allPassed = false;
            continue;
        }

        int nonBackgroundPixelCount = 0;
        const bool visiblePixels = hasVisiblePixels(frame, &nonBackgroundPixelCount);

        std::cout << "Loaded " << filePath.toStdString()
                  << " framebuffer=" << frame.width() << "x" << frame.height()
                  << " nonBackgroundPixels=" << nonBackgroundPixelCount << std::endl;

        if (!visiblePixels) {
            std::cerr << "Rendered framebuffer appears empty for "
                      << filePath.toStdString() << std::endl;
            allPassed = false;
            continue;
        }

        OsgWidget* osgWidget = qobject_cast<OsgWidget*>(glWidget);
        if (osgWidget == nullptr) {
            std::cerr << "No OsgWidget found for " << filePath.toStdString() << std::endl;
            allPassed = false;
            continue;
        }

        const QPoint clickPoint = osgWidget->rect().center();
        QMouseEvent pressEvent(
            QEvent::MouseButtonPress,
            QPointF(clickPoint),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(osgWidget, &pressEvent);

        QMouseEvent releaseEvent(
            QEvent::MouseButtonRelease,
            QPointF(clickPoint),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(osgWidget, &releaseEvent);
        pumpEvents(200);

        const QImage clickedFrame = glWidget->grabFramebuffer();
        if (clickedFrame.isNull()) {
            std::cerr << "grabFramebuffer() returned a null image after click for "
                      << filePath.toStdString() << std::endl;
            allPassed = false;
            continue;
        }

        int clickedNonBackgroundPixelCount = 0;
        const bool visiblePixelsAfterClick = hasVisiblePixels(clickedFrame, &clickedNonBackgroundPixelCount);
        std::cout << "After click " << filePath.toStdString()
                  << " framebuffer=" << clickedFrame.width() << "x" << clickedFrame.height()
                  << " nonBackgroundPixels=" << clickedNonBackgroundPixelCount << std::endl;

        if (!visiblePixelsAfterClick) {
            std::cerr << "Rendered framebuffer appears empty after click for "
                      << filePath.toStdString() << std::endl;
            allPassed = false;
            continue;
        }

        const QPointF orbitDragStart = QPointF(clickPoint);
        const QPointF orbitDragDelta(48.0, 24.0);

        InteractionOptions interactionOptions = viewer.interactionOptions();
        interactionOptions.invertOrbitDrag = false;
        viewer.setInteractionOptions(interactionOptions);
        pumpEvents(50);

        const QPointF defaultOrbitAdjustedPosition =
            runOrbitDragAndCaptureEventPosition(osgWidget, orbitDragStart, orbitDragDelta);
        const bool defaultXMirrored = defaultOrbitAdjustedPosition.x() < orbitDragStart.x() - 1.0;
        const bool defaultYPreserved = defaultOrbitAdjustedPosition.y() > orbitDragStart.y() + 1.0;
        if (!verify(
                defaultXMirrored && defaultYPreserved,
                "Viewer render smoke should apply default orbit mapping (X mirrored, Y preserved)")) {
            allPassed = false;
        }

        interactionOptions.invertOrbitDrag = true;
        viewer.setInteractionOptions(interactionOptions);
        pumpEvents(50);

        const QPointF invertedOrbitAdjustedPosition =
            runOrbitDragAndCaptureEventPosition(osgWidget, orbitDragStart, orbitDragDelta);
        const bool invertedXForward = invertedOrbitAdjustedPosition.x() > orbitDragStart.x() + 1.0;
        const bool invertedYMirrored = invertedOrbitAdjustedPosition.y() < orbitDragStart.y() - 1.0;
        if (!verify(
                invertedXForward && invertedYMirrored,
                "Viewer render smoke should invert both orbit axes when invert option is enabled")) {
            allPassed = false;
        }
    }

    return allPassed;
}

bool runMainBackstageSmoke(const QStringList& filePaths)
{
    QTranslator appTranslator;
    QTranslator qtTranslator;
    MainWindow window(&appTranslator, &qtTranslator);
    window.resize(1400, 900);
    window.show();
    pumpEvents(300);

    Qtitan::RibbonBar* ribbonBar = window.ribbonBar();
    if (!verify(ribbonBar != nullptr, "MainWindow should expose a RibbonBar")) {
        return false;
    }

    QScreen* targetScreen = window.screen();
    if (targetScreen == nullptr) {
        targetScreen = QGuiApplication::screenAt(window.frameGeometry().center());
    }
    if (targetScreen == nullptr) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    if (!verify(targetScreen != nullptr, "MainWindow should resolve an active screen")) {
        return false;
    }

    SceneInspectorDock* inspectorDock = window.findChild<SceneInspectorDock*>();
    if (!verify(inspectorDock != nullptr, "MainWindow should create the scene inspector dock")) {
        return false;
    }

    SpanProfileDock* profileDock = window.findChild<SpanProfileDock*>();
    if (!verify(profileDock != nullptr, "MainWindow should create the span profile dock")) {
        return false;
    }

    QAction* showProfileDockAction = window.findChild<QAction*>(QStringLiteral("showProfileDockAction"));
    if (!verify(showProfileDockAction != nullptr, "MainWindow should expose the profile dock toggle action")) {
        return false;
    }
    if (!verify(!profileDock->isVisible(), "Span profile dock should be hidden by default")) {
        return false;
    }

    showProfileDockAction->setChecked(true);
    pumpEvents(200);
    if (!verify(profileDock->isVisible(), "Profile dock toggle action should show the span profile dock")) {
        return false;
    }

    showProfileDockAction->setChecked(false);
    pumpEvents(200);
    if (!verify(!profileDock->isVisible(), "Profile dock toggle action should hide the span profile dock")) {
        return false;
    }

    ProjectExplorerDock* projectDock = window.findChild<ProjectExplorerDock*>();
    if (!verify(projectDock != nullptr, "MainWindow should create the project explorer dock")) {
        return false;
    }

    PointCloudViewer* viewer = window.findChild<PointCloudViewer*>();
    if (!verify(viewer != nullptr, "MainWindow should create the embedded point cloud viewer")) {
        return false;
    }

    QTreeWidget* projectTree = projectDock->treeWidget();
    if (!verify(projectTree != nullptr, "Project explorer dock should expose the project tree")) {
        return false;
    }

    RouteDetailsDock* routeDetailsDock = window.findChild<RouteDetailsDock*>();
    if (!verify(routeDetailsDock != nullptr, "MainWindow should create the route details dock")) {
        return false;
    }

    const QString lasFilePath = filePaths.isEmpty() ? QString() : QFileInfo(filePaths.constFirst()).absoluteFilePath();
      if (!lasFilePath.isEmpty() && QFileInfo::exists(lasFilePath)) {
          QString errorMessage;
          if (!viewer->loadPointCloud(lasFilePath, &errorMessage)) {
            std::cerr << "[FAIL] MainWindow viewer failed to load point cloud: "
                      << errorMessage.toStdString() << std::endl;
            return false;
        }

        pumpEvents(1200);
        if (!verify(projectTree->topLevelItemCount() >= 4, "Loading a point cloud should rebuild the project tree")) {
            return false;
        }

        QTreeWidgetItem* pointCloudGroupItem = projectTree->topLevelItem(1);
        if (!verify(pointCloudGroupItem != nullptr, "Project tree should expose the point cloud group")) {
            return false;
        }
          if (!verify(pointCloudGroupItem->childCount() >= 1, "Project tree should list the loaded point cloud dataset")) {
              return false;
          }

          NavigationSettingsWidget* navigationSettingsWidget = window.findChild<NavigationSettingsWidget*>();
          if (!verify(navigationSettingsWidget != nullptr, "MainWindow should create the navigation settings widget")) {
              return false;
          }
          if (!verify(navigationSettingsWidget->wheelZoomSensitivitySlider() != nullptr, "Navigation settings widget should expose the wheel sensitivity slider")) {
              return false;
          }

          const bool initialInvertOrbit = viewer->interactionOptions().invertOrbitDrag;
          navigationSettingsWidget->invertOrbitCheckBox()->setChecked(!initialInvertOrbit);
          pumpEvents(60);
          if (!verify(
                  viewer->interactionOptions().invertOrbitDrag == !initialInvertOrbit,
                  "Invert orbit checkbox should sync into viewer interaction options")) {
              return false;
          }

          const bool initialInvertPan = viewer->interactionOptions().invertPanDrag;
          navigationSettingsWidget->invertPanCheckBox()->setChecked(!initialInvertPan);
          pumpEvents(60);
          if (!verify(
                  viewer->interactionOptions().invertPanDrag == !initialInvertPan,
                  "Invert pan checkbox should sync into viewer interaction options")) {
              return false;
          }

          const bool initialInvertWheel = viewer->interactionOptions().invertWheelZoom;
          navigationSettingsWidget->invertWheelCheckBox()->setChecked(!initialInvertWheel);
          pumpEvents(60);
          if (!verify(
                  viewer->interactionOptions().invertWheelZoom == !initialInvertWheel,
                  "Invert wheel checkbox should sync into viewer interaction options")) {
              return false;
          }

          const int updatedWheelSensitivity = std::clamp(
              viewer->interactionOptions().wheelZoomSensitivityPercent + 15,
              navigationSettingsWidget->wheelZoomSensitivitySlider()->minimum(),
              navigationSettingsWidget->wheelZoomSensitivitySlider()->maximum());
          navigationSettingsWidget->wheelZoomSensitivitySlider()->setValue(updatedWheelSensitivity);
          pumpEvents(60);
          if (!verify(
                  viewer->interactionOptions().wheelZoomSensitivityPercent == updatedWheelSensitivity,
                  "Wheel zoom sensitivity slider should sync into viewer interaction options")) {
              return false;
          }
          if (!verify(
                  navigationSettingsWidget->wheelZoomSensitivityValueLabel()->text().contains(QString::number(updatedWheelSensitivity)),
                  "Wheel zoom sensitivity slider should update its value label")) {
              return false;
          }

          QTableWidget* classificationTable = nullptr;
          for (QTableWidget* table : window.findChildren<QTableWidget*>()) {
              if (table != nullptr && table->columnCount() == 4 && table->rowCount() > 0) {
                  classificationTable = table;
                  break;
              }
          }
          if (!verify(classificationTable != nullptr, "MainWindow should populate the classification mapping table after loading point cloud")) {
              return false;
          }
          if (!verify(classificationTable->item(0, 0) != nullptr, "Classification mapping table should populate visibility cells")) {
              return false;
          }
          if (!verify(classificationTable->item(0, 2) != nullptr, "Classification mapping table should populate class name cells")) {
              return false;
          }

          const int classificationCode = classificationTable->item(0, 0)->data(Qt::UserRole).toInt();
          const Qt::CheckState toggledVisibility =
              classificationTable->item(0, 0)->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked;
          classificationTable->item(0, 0)->setCheckState(toggledVisibility);
          pumpEvents(80);
          const bool expectedVisibility = toggledVisibility == Qt::Checked;
          if (!verify(
                  viewer->visualizationOptions().classificationVisibility.value(classificationCode, true) == expectedVisibility,
                  "Classification visibility checkbox should sync into viewer visualization options")) {
              return false;
          }

          const QString originalClassificationName = classificationTable->item(0, 2)->text();
          const QString customClassificationName = originalClassificationName + QStringLiteral(" Smoke");
          classificationTable->item(0, 2)->setText(customClassificationName);
          pumpEvents(80);
          if (!verify(
                  classificationTable->item(0, 2) != nullptr && classificationTable->item(0, 2)->text() == customClassificationName,
                  "Classification name edits should round-trip through the MainWindow mapping table")) {
              return false;
          }

          QPushButton* resetClassificationButton = classificationTable->parentWidget() != nullptr
              ? classificationTable->parentWidget()->findChild<QPushButton*>()
              : nullptr;
          if (!verify(resetClassificationButton != nullptr, "Classification mapping group should expose the reset button")) {
              return false;
          }
          resetClassificationButton->click();
          pumpEvents(80);
          if (!verify(
                  classificationTable->item(0, 2) != nullptr && classificationTable->item(0, 2)->text() == originalClassificationName,
                  "Reset classification button should restore default class names")) {
              return false;
          }

          if (!verify(window.measurementAnalysisController_ != nullptr, "MainWindow should create the measurement analysis controller")) {
              return false;
          }
          if (!verify(window.routeController_ != nullptr, "MainWindow should create the route controller")) {
              return false;
          }
          if (!verify(window.vegetationRisksTableWidget_ != nullptr, "MainWindow should keep the vegetation risks table wired")) {
              return false;
          }
          if (!verify(window.clearanceSegmentsTableWidget_ != nullptr, "MainWindow should keep the clearance segments table wired")) {
              return false;
          }
          if (!verify(window.measureAction_ != nullptr, "MainWindow should keep the measurement action wired")) {
              return false;
          }
          if (!verify(window.generateInspectionRouteAction_ != nullptr, "MainWindow should keep the route generation action wired")) {
              return false;
          }

          MeasurementResult measurementResult;
          PointRecord measurementStart;
          measurementStart.x = 0.0f;
          measurementStart.y = 0.0f;
          measurementStart.z = 10.0f;
          measurementResult.points.append(measurementStart);
          PointRecord measurementEnd;
          measurementEnd.x = 80.0f;
          measurementEnd.y = 20.0f;
          measurementEnd.z = 12.0f;
          measurementResult.points.append(measurementEnd);
          measurementResult.hasStartPoint = true;
          measurementResult.hasEndPoint = true;
          measurementResult.startPoint = measurementStart;
          measurementResult.endPoint = measurementEnd;
          measurementResult.distance3d = 82.4864f;
          measurementResult.deltaZ = 2.0f;
          viewer->measurementResult_ = measurementResult;

          window.measureAction_->setChecked(true);
          pumpEvents(80);
          if (!verify(viewer->measurementEnabled(), "Measurement action should enable viewer measurement mode through MainWindow")) {
              return false;
          }
          if (!verify(profileDock->isVisible(), "Measurement action should surface the profile dock through MainWindow")) {
              return false;
          }

          window.clearMeasurementAction_->trigger();
          pumpEvents(80);
          if (!verify(viewer->measurementResult().points.isEmpty(), "Clear measurement action should clear viewer measurement points")) {
              return false;
          }

          viewer->measurementResult_ = measurementResult;
          window.measureAction_->setChecked(false);
          pumpEvents(80);
          if (!verify(!viewer->measurementEnabled(), "Measurement action should disable viewer measurement mode through MainWindow")) {
              return false;
          }

          const double updatedThreshold = window.clearanceWarningThresholdMeters_ + 3.0;
          window.clearanceThresholdSpinBox_->setValue(updatedThreshold);
          pumpEvents(80);
          if (!verifyClose(window.clearanceWarningThresholdMeters_, updatedThreshold, 0.001, "Clearance threshold spin box should sync into MainWindow state")) {
              return false;
          }

          if (window.clearanceRulePresetComboBox_->count() > 1) {
              const int presetIndex = (window.clearanceRulePresetComboBox_->currentIndex() + 1) % window.clearanceRulePresetComboBox_->count();
              const ClearanceRulePreset expectedPreset = static_cast<ClearanceRulePreset>(
                  window.clearanceRulePresetComboBox_->itemData(presetIndex).toInt());
              window.clearanceRulePresetComboBox_->setCurrentIndex(presetIndex);
              pumpEvents(80);
              if (!verify(window.clearanceRulePreset_ == expectedPreset, "Clearance preset combo box should sync into MainWindow state")) {
                  return false;
              }
          }

          QList<VegetationRiskRecord> smokeRisks;
          VegetationRiskRecord firstRisk;
          firstRisk.id = QStringLiteral("risk-main-001");
          firstRisk.title = QStringLiteral("Vegetation Risk A");
          firstRisk.severity = AnalysisSeverity::Warning;
          firstRisk.point.x = 40.0f;
          firstRisk.point.y = 50.0f;
          firstRisk.point.z = 18.0f;
          firstRisk.minimumDistance = 4.5f;
          firstRisk.chainageStart = 10.0f;
          firstRisk.chainageEnd = 20.0f;
          firstRisk.sourceRule = QStringLiteral("Rule-A");
          firstRisk.notes = QStringLiteral("Near conductor");
          smokeRisks.append(firstRisk);

          VegetationRiskRecord secondRisk = firstRisk;
          secondRisk.id = QStringLiteral("risk-main-002");
          secondRisk.title = QStringLiteral("Vegetation Risk B");
          secondRisk.severity = AnalysisSeverity::Critical;
          secondRisk.point.x = 110.0f;
          secondRisk.point.y = 75.0f;
          secondRisk.point.z = 22.0f;
          secondRisk.minimumDistance = 2.5f;
          secondRisk.chainageStart = 60.0f;
          secondRisk.chainageEnd = 72.0f;
          secondRisk.sourceRule = QStringLiteral("Rule-B");
          secondRisk.notes = QStringLiteral("Critical gap");
          smokeRisks.append(secondRisk);

          viewer->clearInspectionIssues();
          pumpEvents(60);
          window.vegetationRiskResults_ = smokeRisks;
          window.selectedVegetationRiskIndex_ = 0;
          window.vegetationRisksTableWidget_->setRowCount(smokeRisks.size());
          for (int riskRow = 0; riskRow < smokeRisks.size(); ++riskRow) {
              if (window.vegetationRisksTableWidget_->item(riskRow, 0) == nullptr) {
                  window.vegetationRisksTableWidget_->setItem(
                      riskRow,
                      0,
                      new QTableWidgetItem(smokeRisks.at(riskRow).title));
              } else {
                  window.vegetationRisksTableWidget_->item(riskRow, 0)->setText(smokeRisks.at(riskRow).title);
              }
          }
          if (!verify(window.vegetationRisksTableWidget_->rowCount() == smokeRisks.size(), "Vegetation risks should populate the MainWindow risk table")) {
              return false;
          }

          window.vegetationRisksTableWidget_->setCurrentCell(1, 0);
          pumpEvents(80);
          if (!verify(window.selectedVegetationRiskIndex_ == 1, "Vegetation risk table selection should sync into MainWindow state")) {
              return false;
          }

          window.createIssueFromRiskAction_->trigger();
          pumpEvents(120);
          if (!verify(viewer->inspectionIssues().size() == 1, "Create issue from risk action should add an inspection issue through MainWindow")) {
              return false;
          }
          if (!verify(viewer->inspectionIssues().first().title == secondRisk.title, "Issue created from vegetation risk should keep the selected risk title")) {
              return false;
          }
          if (!verify(viewer->inspectionIssues().first().severity == IssueSeverity::Critical, "Issue created from vegetation risk should map the risk severity")) {
              return false;
          }

          window.clearVegetationRisksAction_->trigger();
          pumpEvents(80);
          if (!verify(window.vegetationRiskResults_.isEmpty(), "Clear vegetation risks action should clear MainWindow vegetation results")) {
              return false;
          }
          if (!verify(window.vegetationRisksTableWidget_->rowCount() == 0, "Clear vegetation risks action should clear the vegetation risks table")) {
              return false;
          }

          QList<TowerRecord> routePlanningTowers;
          TowerRecord routePlanningTower;
          routePlanningTower.index = 0;
          routePlanningTower.name = QStringLiteral("Route Tower");
          routePlanningTower.point.x = 15.0f;
          routePlanningTower.point.y = 20.0f;
          routePlanningTower.point.z = 25.0f;
          routePlanningTowers.append(routePlanningTower);
          viewer->setTowerMarkers(routePlanningTowers);
          pumpEvents(80);

          window.vegetationRiskResults_ = smokeRisks;
          window.selectedVegetationRiskIndex_ = 0;
          window.generateInspectionRouteAction_->setEnabled(true);
          window.generateInspectionRouteAction_->trigger();
          pumpEvents(160);
          if (!verify(!window.currentPowerlineRoute_.waypoints.isEmpty(), "Generate route action should create route waypoints through MainWindow")) {
              return false;
          }
          if (!verify(!viewer->inspectionRouteWaypoints().isEmpty(), "Generate route action should sync preview waypoints into the viewer")) {
              return false;
          }

          window.toggleRouteEditingAction_->setChecked(true);
          pumpEvents(80);
          if (!verify(window.routeEditingEnabled_, "Toggle route editing action should enable MainWindow route editing")) {
              return false;
          }
          if (!verify(viewer->inspectionRouteEditingEnabled(), "Toggle route editing action should enable viewer route editing")) {
              return false;
          }

          const double updatedRoamSpeed = std::max(1.0, window.routeRoamSpeedSpinBox_->value() + 1.5);
          window.routeRoamSpeedSpinBox_->setValue(updatedRoamSpeed);
          pumpEvents(80);
          if (!verifyClose(viewer->inspectionRouteRoamSpeedMetersPerSecond(), updatedRoamSpeed, 0.001, "Route roam speed spin box should sync into the viewer")) {
              return false;
          }

          if (window.routeRoamViewModeComboBox_->count() > 1) {
              const int roamModeIndex = (window.routeRoamViewModeComboBox_->currentIndex() + 1) % window.routeRoamViewModeComboBox_->count();
              const int expectedRoamMode = window.routeRoamViewModeComboBox_->itemData(roamModeIndex).toInt();
              window.routeRoamViewModeComboBox_->setCurrentIndex(roamModeIndex);
              pumpEvents(80);
              if (!verify(
                      static_cast<int>(viewer->inspectionRouteRoamViewMode()) == expectedRoamMode,
                      "Route roam view mode combo box should sync into the viewer")) {
                  return false;
              }
          }

          viewer->setInspectionRouteVisible(true);
          pumpEvents(80);
          window.startInspectionRouteRoamAction_->trigger();
          pumpEvents(180);
          if (!verify(viewer->inspectionRouteRoamActive(), "Start route roam action should start viewer route roam")) {
              return false;
          }

          window.pauseInspectionRouteRoamAction_->trigger();
          pumpEvents(120);
          if (!verify(viewer->inspectionRouteRoamPaused(), "Pause route roam action should pause viewer route roam")) {
              return false;
          }

          window.pauseInspectionRouteRoamAction_->trigger();
          pumpEvents(120);
          if (!verify(!viewer->inspectionRouteRoamPaused(), "Pause route roam action should resume viewer route roam on second trigger")) {
              return false;
          }

          window.stopInspectionRouteRoamAction_->trigger();
          pumpEvents(120);
          if (!verify(!viewer->inspectionRouteRoamActive(), "Stop route roam action should stop viewer route roam")) {
              return false;
          }

          window.clearInspectionRouteAction_->trigger();
          pumpEvents(120);
          if (!verify(window.currentPowerlineRoute_.waypoints.isEmpty(), "Clear route action should clear MainWindow route data")) {
              return false;
          }
          if (!verify(viewer->inspectionRouteWaypoints().isEmpty(), "Clear route action should clear viewer route preview data")) {
              return false;
          }

          QTableWidget* routeWaypointsTable = nullptr;
          QTableWidget* routePartPointsTable = nullptr;
          for (QTableWidget* table : routeDetailsDock->findChildren<QTableWidget*>()) {
              if (table == nullptr) {
                  continue;
              }
              if (table->columnCount() == 9) {
                  routeWaypointsTable = table;
              } else if (table->columnCount() == 8) {
                  routePartPointsTable = table;
              }
          }
          if (!verify(routeWaypointsTable != nullptr, "Route details dock should expose the waypoint table")) {
              return false;
          }
          if (!verify(routePartPointsTable != nullptr, "Route details dock should expose the part point table")) {
              return false;
          }

          const QList<QCheckBox*> routeDisplayCheckBoxes = routeDetailsDock->findChildren<QCheckBox*>();
          if (!verify(routeDisplayCheckBoxes.size() >= 4, "Route details dock should expose the display toggle checkboxes")) {
              return false;
          }

          QSet<int> matchedRouteToggleIndexes;
          const auto hideColumnsByEffect = [&](QTableWidget* table, const QList<int>& columns, const char* failureMessage) {
              for (int checkBoxIndex = 0; checkBoxIndex < routeDisplayCheckBoxes.size(); ++checkBoxIndex) {
                  if (matchedRouteToggleIndexes.contains(checkBoxIndex) || routeDisplayCheckBoxes.at(checkBoxIndex) == nullptr) {
                      continue;
                  }

                  routeDisplayCheckBoxes.at(checkBoxIndex)->setChecked(false);
                  pumpEvents(60);

                  bool allHidden = true;
                  for (int column : columns) {
                      allHidden = allHidden && table->isColumnHidden(column);
                  }
                  if (allHidden) {
                      matchedRouteToggleIndexes.insert(checkBoxIndex);
                      return true;
                  }

                  routeDisplayCheckBoxes.at(checkBoxIndex)->setChecked(true);
                  pumpEvents(60);
              }

              return verify(false, failureMessage);
          };

          if (!hideColumnsByEffect(routeWaypointsTable, { 2, 3, 4 }, "Waypoint coordinates checkbox should hide waypoint coordinate columns")) {
              return false;
          }
          if (!hideColumnsByEffect(routeWaypointsTable, { 5, 6, 7, 8 }, "Waypoint capture angles checkbox should hide waypoint angle columns")) {
              return false;
          }
          if (!hideColumnsByEffect(routePartPointsTable, { 5, 6, 7 }, "Part coordinates checkbox should hide route part coordinate columns")) {
              return false;
          }
          if (!hideColumnsByEffect(routePartPointsTable, { 4 }, "Part capture angles checkbox should hide the route part angle column")) {
              return false;
          }

          QTemporaryDir routeTempDir;
          if (!verify(routeTempDir.isValid(), "MainWindow route smoke should create a temporary directory")) {
              return false;
          }

          const PowerlineRouteDocument syntheticRoute = buildSyntheticRoute();
          QString routeErrorMessage;
          const QString routeFilePath = QDir(routeTempDir.path()).filePath(QStringLiteral("main_backstage_route.json"));
          if (!exportPowerlineRouteJson(routeFilePath, syntheticRoute, &routeErrorMessage)) {
              std::cerr << "[FAIL] exportPowerlineRouteJson(main-backstage): "
                        << routeErrorMessage.toStdString() << std::endl;
              return false;
          }

          PowerlineRouteDocument importedRoute;
          if (!importPowerlineRouteJson(routeFilePath, &importedRoute, &routeErrorMessage)) {
              std::cerr << "[FAIL] importPowerlineRouteJson(main-backstage): "
                        << routeErrorMessage.toStdString() << std::endl;
              return false;
          }

          window.currentPowerlineRoute_ = importedRoute;
          window.linkedRouteFilePath_ = routeFilePath;
          window.selectedRoutePartIndex_ = -1;
          window.selectedRouteWaypointIndex_ = importedRoute.waypoints.isEmpty() ? -1 : 0;
          window.selectedRouteWaypointTargetIndex_ = -1;
          routeDetailsDock->show();
          routeDetailsDock->raise();
          viewer->setInspectionRouteDisplayData(buildSmokeRouteDisplayData(importedRoute));
          viewer->setSelectedInspectionRouteWaypointTargetIndex(-1);
          viewer->setSelectedInspectionRouteWaypointIndex(window.selectedRouteWaypointIndex_);

          pumpEvents(250);
          if (!verify(!window.currentPowerlineRoute_.waypoints.isEmpty(), "MainWindow route smoke should keep imported route data")) {
              return false;
          }
          if (!verify(routeDetailsDock->isVisible(), "Route smoke should show the route details dock")) {
              return false;
          }
          if (!verify(window.routeWaypointsTableWidget_ != nullptr, "MainWindow should keep the waypoint table wired after route import")) {
              return false;
          }
          if (!verify(window.routePartPointsTableWidget_ != nullptr, "MainWindow should keep the part point table wired after route import")) {
              return false;
          }
          if (!verify(window.routeWaypointTargetsTableWidget_ != nullptr, "MainWindow should keep the waypoint target table wired after route import")) {
              return false;
          }
          if (!verify(window.routeQaIssuesTableWidget_ != nullptr, "MainWindow should keep the route QA table wired after route import")) {
              return false;
          }
          if (!verify(window.routeWaypointColorButton_ != nullptr, "MainWindow should keep the waypoint color button wired")) {
              return false;
          }
          if (!verify(window.routePartPointColorButton_ != nullptr, "MainWindow should keep the part point color button wired")) {
              return false;
          }
          if (!verify(window.routeTrajectoryColorButton_ != nullptr, "MainWindow should keep the trajectory color button wired")) {
              return false;
          }
          if (!verify(window.routeWaypointsTableWidget_->rowCount() == syntheticRoute.waypoints.size(), "Imported route should populate the waypoint table")) {
              return false;
          }
          if (!verify(window.routePartPointsTableWidget_->rowCount() == syntheticRoute.partPoints.size(), "Imported route should populate the part point table")) {
              return false;
          }
          if (!verify(window.routeWaypointTargetsTableWidget_->rowCount() == syntheticRoute.waypoints.first().captureTargets.size(), "Imported route should populate the waypoint target table for the selected waypoint")) {
              return false;
          }
          if (!verify(window.routeQaIssuesTableWidget_->rowCount() > 0, "Imported route should populate at least one QA issue row")) {
              return false;
          }
          if (!verify(viewer->inspectionRouteWaypoints().size() == syntheticRoute.waypoints.size(), "Imported route should sync waypoint preview data into the viewer")) {
              return false;
          }
          if (!verify(window.selectedRouteWaypointIndex_ == 0, "Imported route should select the first waypoint by default")) {
              return false;
          }
          if (!verify(window.selectedRoutePartIndex_ == syntheticRoute.partPoints.first().partIndex, "Imported route should select the primary part point for the first waypoint")) {
              return false;
          }

          const int routeWaypointCountBeforeMeasurementToggle = viewer->inspectionRouteWaypoints().size();
          window.measureAction_->setChecked(true);
          pumpEvents(80);
          if (!verify(viewer->measurementEnabled(), "Route smoke should allow entering measurement mode")) {
              return false;
          }
          if (!verify(
                  viewer->inspectionRouteVisible()
                      && viewer->inspectionRouteWaypoints().size() == routeWaypointCountBeforeMeasurementToggle,
                  "Entering measurement mode should not hide or clear route waypoints")) {
              return false;
          }

          window.measureAction_->setChecked(false);
          pumpEvents(80);
          if (!verify(!viewer->measurementEnabled(), "Route smoke should allow leaving measurement mode")) {
              return false;
          }
          if (!verify(
                  viewer->inspectionRouteVisible()
                      && viewer->inspectionRouteWaypoints().size() == routeWaypointCountBeforeMeasurementToggle,
                  "Leaving measurement mode should keep route visibility and waypoint data")) {
              return false;
          }

          const int kRouteWaypointPartColumn = 1;
          const int kRoutePartNameColumn = 1;
          const int kRouteWaypointTargetPartColumn = 1;
          const int kRouteQaSeverityColumn = 0;

          window.routeWaypointsTableWidget_->setCurrentCell(1, kRouteWaypointPartColumn);
          pumpEvents(80);
          if (!verify(window.selectedRouteWaypointIndex_ == 1, "Waypoint table currentCellChanged should update the selected waypoint index")) {
              return false;
          }
          if (!verify(viewer->selectedInspectionRouteWaypointIndex() == 1, "Waypoint table currentCellChanged should sync the selected waypoint into the viewer")) {
              return false;
          }
          if (!verify(window.selectedRoutePartIndex_ == -1, "Selecting the helper waypoint should clear the linked part selection")) {
              return false;
          }
          if (!verify(window.routeWaypointTargetsTableWidget_->rowCount() == 0, "Selecting the helper waypoint should clear the waypoint target table")) {
              return false;
          }

          window.routeWaypointsTableWidget_->setCurrentCell(0, kRouteWaypointPartColumn);
          pumpEvents(80);
          if (!verify(window.selectedRouteWaypointIndex_ == 0, "Waypoint table should allow returning to the first waypoint")) {
              return false;
          }
          if (!verify(window.selectedRoutePartIndex_ == syntheticRoute.partPoints.first().partIndex, "Selecting the first waypoint should restore the linked part selection")) {
              return false;
          }
          if (!verify(window.routeWaypointTargetsTableWidget_->rowCount() == syntheticRoute.waypoints.first().captureTargets.size(), "Selecting the first waypoint should repopulate the waypoint target table")) {
              return false;
          }

          window.routeWaypointTargetsTableWidget_->setCurrentCell(1, kRouteWaypointTargetPartColumn);
          pumpEvents(80);
          if (!verify(window.selectedRouteWaypointTargetIndex_ == 1, "Waypoint target table currentCellChanged should update the selected target index")) {
              return false;
          }
          if (!verify(viewer->selectedInspectionRouteWaypointTargetIndex() == 1, "Waypoint target table currentCellChanged should sync the selected target into the viewer")) {
              return false;
          }

          window.routePartPointsTableWidget_->setCurrentCell(1, kRoutePartNameColumn);
          pumpEvents(80);
          if (!verify(window.selectedRoutePartIndex_ == syntheticRoute.partPoints.at(1).partIndex, "Part table currentCellChanged should update the selected part index")) {
              return false;
          }

          window.routeQaIssuesTableWidget_->setCurrentCell(0, kRouteQaSeverityColumn);
          pumpEvents(80);
          if (!verify(window.selectedRouteQaIssueIndex_ == 0, "Route QA table currentCellChanged should update the selected QA issue index")) {
              return false;
          }

          if (!emitTableDoubleClick(window.routeWaypointsTableWidget_, 1, kRouteWaypointPartColumn, "Waypoint table double click should stay invokable")) {
              return false;
          }
          if (!emitTableDoubleClick(window.routePartPointsTableWidget_, 0, kRoutePartNameColumn, "Part table double click should stay invokable")) {
              return false;
          }
          if (!emitTableDoubleClick(window.routeQaIssuesTableWidget_, 0, kRouteQaSeverityColumn, "Route QA table double click should stay invokable")) {
              return false;
          }

          window.routeWaypointsTableWidget_->setCurrentCell(0, kRouteWaypointPartColumn);
          pumpEvents(60);
          const QPoint waypointContextPosition =
              window.routeWaypointsTableWidget_->visualRect(window.routeWaypointsTableWidget_->model()->index(1, kRouteWaypointPartColumn)).center();
          if (!invokeTableContextMenuAndClose(window.routeWaypointsTableWidget_, waypointContextPosition, "Waypoint table context menu should stay invokable")) {
              return false;
          }
          if (!verify(window.selectedRouteWaypointIndex_ == 1, "Waypoint table context menu should update the current waypoint row before opening")) {
              return false;
          }

          window.routePartPointsTableWidget_->setCurrentCell(1, kRoutePartNameColumn);
          pumpEvents(60);
          const QPoint partContextPosition =
              window.routePartPointsTableWidget_->visualRect(window.routePartPointsTableWidget_->model()->index(0, kRoutePartNameColumn)).center();
          if (!invokeTableContextMenuAndClose(window.routePartPointsTableWidget_, partContextPosition, "Part table context menu should stay invokable")) {
              return false;
          }
          if (!verify(window.selectedRoutePartIndex_ == syntheticRoute.partPoints.first().partIndex, "Part table context menu should update the current part row before opening")) {
              return false;
          }

          const QColor waypointSmokeColor(12, 160, 210);
          const QColor partSmokeColor(228, 92, 29);
          const QColor trajectorySmokeColor(32, 178, 120);
          if (!clickColorButtonAndAccept(window.routeWaypointColorButton_, waypointSmokeColor, "Waypoint color button should open an accept-able color dialog")) {
              return false;
          }
          if (!verify(viewer->inspectionRouteWaypointColor() == waypointSmokeColor, "Waypoint color button should sync the chosen color into the viewer")) {
              return false;
          }
          if (!clickColorButtonAndAccept(window.routePartPointColorButton_, partSmokeColor, "Part point color button should open an accept-able color dialog")) {
              return false;
          }
          if (!verify(viewer->inspectionRoutePartPointColor() == partSmokeColor, "Part point color button should sync the chosen color into the viewer")) {
              return false;
          }
          if (!clickColorButtonAndAccept(window.routeTrajectoryColorButton_, trajectorySmokeColor, "Trajectory color button should open an accept-able color dialog")) {
              return false;
          }
          if (!verify(viewer->inspectionRouteTrajectoryColor() == trajectorySmokeColor, "Trajectory color button should sync the chosen color into the viewer")) {
              return false;
          }

          if (!verify(window.towerController_ != nullptr, "MainWindow should create the tower controller")) {
              return false;
          }
          if (!verify(window.issueController_ != nullptr, "MainWindow should create the issue controller")) {
              return false;
          }
          if (!verify(window.towerTableWidget_ != nullptr, "MainWindow should keep the tower table wired")) {
              return false;
          }
          if (!verify(window.issueTableWidget_ != nullptr, "MainWindow should keep the issue table wired")) {
              return false;
          }

          QList<TowerRecord> smokeTowers;
          TowerRecord firstTower;
          firstTower.index = 0;
          firstTower.name = QStringLiteral("Smoke Tower A");
          firstTower.point.x = 10.0f;
          firstTower.point.y = 15.0f;
          firstTower.point.z = 20.0f;
          smokeTowers.append(firstTower);

          TowerRecord secondTower;
          secondTower.index = 1;
          secondTower.name = QStringLiteral("Smoke Tower B");
          secondTower.point.x = 25.0f;
          secondTower.point.y = 30.0f;
          secondTower.point.z = 35.0f;
          smokeTowers.append(secondTower);

          viewer->setTowerMarkers(smokeTowers);
          viewer->setSelectedTowerIndex(0);
          pumpEvents(150);
          if (!verify(window.towerTableWidget_->rowCount() == smokeTowers.size(), "Tower markers should populate the tower table through MainWindow")) {
              return false;
          }

          window.startTowerEditAction_->trigger();
          pumpEvents(80);
          if (!verify(window.towerEditingEnabled_, "Start tower editing action should enable tower editing")) {
              return false;
          }

          window.towerTableWidget_->setCurrentCell(1, 1);
          pumpEvents(80);
          if (!verify(viewer->selectedTowerIndex() == 1, "Tower table selection should sync into the viewer")) {
              return false;
          }

          viewer->setSelectedTowerIndex(0);
          pumpEvents(80);
          if (!verify(window.towerTableWidget_->currentRow() == 0, "Viewer tower selection should sync back into the tower table")) {
              return false;
          }

          if (window.towerTableWidget_->item(0, 1) == nullptr) {
              std::cerr << "[FAIL] Tower table should populate editable name cells" << std::endl;
              return false;
          }
          window.towerTableWidget_->item(0, 1)->setText(QStringLiteral("Smoke Tower Alpha"));
          pumpEvents(80);
          if (!verify(viewer->towerMarkers().at(0).name == QStringLiteral("Smoke Tower Alpha"), "Tower name edits should sync into viewer tower markers")) {
              return false;
          }

          window.towerCodeEdit_->setText(QStringLiteral("T-ALPHA"));
          window.towerCodeEdit_->editingFinished();
          window.towerLineNameEdit_->setText(QStringLiteral("Line-A"));
          window.towerLineNameEdit_->editingFinished();
          if (window.towerTypeComboBox_->count() > 1) {
              window.towerTypeComboBox_->setCurrentIndex(1);
          }
          window.towerNotesEdit_->setPlainText(QStringLiteral("tower smoke note"));
          pumpEvents(120);
          if (!verify(viewer->towerMarkers().at(0).code == QStringLiteral("T-ALPHA"), "Tower detail edits should sync code into viewer tower markers")) {
              return false;
          }
          if (!verify(viewer->towerMarkers().at(0).lineName == QStringLiteral("Line-A"), "Tower detail edits should sync line name into viewer tower markers")) {
              return false;
          }
          if (!verify(viewer->towerMarkers().at(0).notes == QStringLiteral("tower smoke note"), "Tower detail edits should sync notes into viewer tower markers")) {
              return false;
          }

          window.showTowerXAction_->setChecked(false);
          window.showTowerYAction_->setChecked(false);
          window.showTowerZAction_->setChecked(false);
          pumpEvents(80);
          if (!verify(window.towerTableWidget_->isColumnHidden(2), "Tower X visibility action should hide the X column")) {
              return false;
          }
          if (!verify(window.towerTableWidget_->isColumnHidden(3), "Tower Y visibility action should hide the Y column")) {
              return false;
          }
          if (!verify(window.towerTableWidget_->isColumnHidden(4), "Tower Z visibility action should hide the Z column")) {
              return false;
          }
          window.showTowerXAction_->setChecked(true);
          window.showTowerYAction_->setChecked(true);
          window.showTowerZAction_->setChecked(true);
          pumpEvents(80);

          window.addTowerAction_->trigger();
          pumpEvents(80);
          if (!verify(viewer->towerEditMode() == TowerEditMode::AddAfterLast, "Add tower action should enter add mode")) {
              return false;
          }
          window.cancelTowerToolAction_->trigger();
          pumpEvents(80);
          if (!verify(viewer->towerEditMode() == TowerEditMode::None, "Cancel tower tool action should leave tower edit mode")) {
              return false;
          }

          viewer->setSelectedTowerIndex(0);
          pumpEvents(60);
          window.insertTowerAction_->trigger();
          pumpEvents(80);
          if (!verify(viewer->towerEditMode() == TowerEditMode::InsertBeforeSelected, "Insert tower action should enter insert mode")) {
              return false;
          }
          window.cancelTowerToolAction_->trigger();
          pumpEvents(80);

          viewer->setSelectedTowerIndex(0);
          pumpEvents(60);
          window.moveTowerAction_->trigger();
          pumpEvents(80);
          if (!verify(viewer->towerEditMode() == TowerEditMode::MoveSelected, "Move tower action should enter move mode")) {
              return false;
          }
          window.cancelTowerToolAction_->trigger();
          pumpEvents(80);

          window.towerTableWidget_->setCurrentCell(1, 1);
          pumpEvents(80);
          window.removeTowerAction_->trigger();
          pumpEvents(120);
          if (!verify(window.towerTableWidget_->rowCount() == 1, "Remove tower action should remove the selected tower")) {
              return false;
          }
          window.clearTowersAction_->trigger();
          pumpEvents(120);
          if (!verify(window.towerTableWidget_->rowCount() == 0, "Clear towers action should clear the tower table")) {
              return false;
          }

          QList<TowerRecord> issueRelatedTowers;
          TowerRecord relatedTower;
          relatedTower.index = 0;
          relatedTower.name = QStringLiteral("Issue Tower");
          relatedTower.point.x = 40.0f;
          relatedTower.point.y = 45.0f;
          relatedTower.point.z = 50.0f;
          issueRelatedTowers.append(relatedTower);
          viewer->setTowerMarkers(issueRelatedTowers);
          viewer->setSelectedTowerIndex(0);
          pumpEvents(120);

          QList<InspectionIssue> smokeIssues;
          InspectionIssue firstIssue;
          firstIssue.id = QStringLiteral("ISSUE-001");
          firstIssue.title = QStringLiteral("Smoke Issue A");
          firstIssue.category = QStringLiteral("Vegetation");
          firstIssue.severity = IssueSeverity::Major;
          firstIssue.status = IssueStatus::Open;
          firstIssue.point.x = 12.0f;
          firstIssue.point.y = 18.0f;
          firstIssue.point.z = 22.0f;
          firstIssue.relatedTowerIndex = 0;
          firstIssue.relatedTowerName = QStringLiteral("Issue Tower");
          firstIssue.createdAt = QStringLiteral("2026-04-17T09:00:00");
          smokeIssues.append(firstIssue);

          InspectionIssue secondIssue = firstIssue;
          secondIssue.id = QStringLiteral("ISSUE-002");
          secondIssue.title = QStringLiteral("Smoke Issue B");
          secondIssue.category = QStringLiteral("Other");
          secondIssue.point.x = 30.0f;
          secondIssue.point.y = 35.0f;
          secondIssue.point.z = 40.0f;
          secondIssue.createdAt = QStringLiteral("2026-04-17T09:05:00");
          smokeIssues.append(secondIssue);

          viewer->setInspectionIssues(smokeIssues);
          viewer->setSelectedIssueIndex(0);
          pumpEvents(150);
          if (!verify(window.issueTableWidget_->rowCount() == smokeIssues.size(), "Inspection issues should populate the issue table through MainWindow")) {
              return false;
          }

          window.issueTableWidget_->setCurrentCell(1, 1);
          pumpEvents(80);
          if (!verify(viewer->selectedIssueIndex() == 1, "Issue table selection should sync into the viewer")) {
              return false;
          }

          viewer->setSelectedIssueIndex(0);
          pumpEvents(80);
          if (!verify(window.issueTableWidget_->currentRow() == 0, "Viewer issue selection should sync back into the issue table")) {
              return false;
          }

          window.startIssueMarkAction_->trigger();
          pumpEvents(80);
          if (!verify(viewer->issueEditMode() == IssueEditMode::Add, "Start issue action should enter issue add mode")) {
              return false;
          }
          window.cancelIssueToolAction_->trigger();
          pumpEvents(80);
          if (!verify(viewer->issueEditMode() == IssueEditMode::None, "Cancel issue action should leave issue add mode")) {
              return false;
          }

          window.issueTitleEdit_->setText(QStringLiteral("Smoke Issue Alpha"));
          window.issueTitleEdit_->editingFinished();
          window.issueCategoryComboBox_->setEditText(QStringLiteral("Channel Risk"));
          if (window.issueSeverityComboBox_->count() > 3) {
              window.issueSeverityComboBox_->setCurrentIndex(3);
          }
          if (window.issueStatusComboBox_->count() > 1) {
              window.issueStatusComboBox_->setCurrentIndex(1);
          }
          if (window.issueRelatedTowerComboBox_->count() > 1) {
              window.issueRelatedTowerComboBox_->setCurrentIndex(1);
          }
          window.issueImagePathEdit_->setText(QStringLiteral("images/smoke-issue.jpg"));
          window.issueImagePathEdit_->editingFinished();
          window.issueDescriptionEdit_->setPlainText(QStringLiteral("issue smoke note"));
          pumpEvents(120);
          if (!verify(viewer->inspectionIssues().at(0).title == QStringLiteral("Smoke Issue Alpha"), "Issue detail edits should sync title into viewer issues")) {
              return false;
          }
          if (!verify(viewer->inspectionIssues().at(0).category == QStringLiteral("Channel Risk"), "Issue detail edits should sync category into viewer issues")) {
              return false;
          }
          if (!verify(viewer->inspectionIssues().at(0).severity == IssueSeverity::Critical, "Issue detail edits should sync severity into viewer issues")) {
              return false;
          }
          if (!verify(viewer->inspectionIssues().at(0).status == IssueStatus::Monitoring, "Issue detail edits should sync status into viewer issues")) {
              return false;
          }
          if (!verify(viewer->inspectionIssues().at(0).imagePath == QStringLiteral("images/smoke-issue.jpg"), "Issue detail edits should sync image path into viewer issues")) {
              return false;
          }
          if (!verify(viewer->inspectionIssues().at(0).description == QStringLiteral("issue smoke note"), "Issue detail edits should sync description into viewer issues")) {
              return false;
          }

          window.issueTableWidget_->setCurrentCell(1, 1);
          pumpEvents(80);
          window.removeIssueAction_->trigger();
          pumpEvents(120);
          if (!verify(window.issueTableWidget_->rowCount() == 1, "Remove issue action should remove the selected issue")) {
              return false;
          }
          window.clearIssuesAction_->trigger();
          pumpEvents(120);
          if (!verify(window.issueTableWidget_->rowCount() == 0, "Clear issues action should clear the issue table")) {
              return false;
          }

          viewer->clearPointCloud();
          pumpEvents(300);
          if (!verify(projectTree->topLevelItemCount() >= 4, "Clearing the point cloud should keep the project tree structure")) {
            return false;
        }
        pointCloudGroupItem = projectTree->topLevelItem(1);
        if (!verify(pointCloudGroupItem != nullptr, "Project tree should keep the point cloud group after clearing")) {
            return false;
        }
        if (!verify(pointCloudGroupItem->childCount() == 0, "Clearing the point cloud should remove dataset entries from the project tree")) {
            return false;
        }
    }

    const int inspectorWidthCap = std::min(
        400,
        static_cast<int>(std::lround(static_cast<double>(targetScreen->availableGeometry().width()) * 0.22)));
    if (!verify(
            inspectorDock->width() <= inspectorWidthCap + 24,
            "Scene inspector dock should adapt its width to the current screen")) {
        return false;
    }

    routeDetailsDock->show();
    routeDetailsDock->raise();
    pumpEvents(250);
    const int routeDockWidthCap = std::min(
        340,
        static_cast<int>(std::lround(static_cast<double>(targetScreen->availableGeometry().width()) * 0.18)));
    if (!verify(
            routeDetailsDock->width() <= routeDockWidthCap + 24,
            "Route details dock should adapt its width to the current screen")) {
        return false;
    }

    const int routeDockShrinkTarget = std::min(
        280,
        std::max(240, static_cast<int>(std::lround(static_cast<double>(targetScreen->availableGeometry().width()) * 0.14))));
    window.resizeDocks({ routeDetailsDock }, { routeDockShrinkTarget }, Qt::Horizontal);
    pumpEvents(250);
    if (routeDetailsDock->width() > routeDockShrinkTarget + 24) {
        std::cerr << "[INFO] route details shrink target=" << routeDockShrinkTarget
                  << " actual=" << routeDetailsDock->width()
                  << " minimumWidth=" << routeDetailsDock->minimumWidth()
                  << " minimumSizeHint=" << routeDetailsDock->minimumSizeHint().width()
                  << std::endl;
    }
    if (!verify(
            routeDetailsDock->width() <= routeDockShrinkTarget + 24,
            "Route details dock should remain shrinkable after showing its contents")) {
        return false;
    }

#ifdef Q_OS_WIN
    pumpEvents(200);
    HWND visibleMainWindow = findVisibleProcessTopLevelWindow(window.windowTitle());
    if (!verifyWindowHasResizeFrame(visibleMainWindow, "Main window should keep a standard resize frame style")) {
        return false;
    }

    window.showMaximized();
    pumpEvents(300);
    visibleMainWindow = findVisibleProcessTopLevelWindow(window.windowTitle());
    if (!verifyWindowHasResizeFrame(visibleMainWindow, "Maximized main window should keep resize frame style")) {
        return false;
    }
    if (!verifyWindowUsesWorkArea(visibleMainWindow, "Maximized frameless main window should fit monitor work area")) {
        return false;
    }

    window.showNormal();
    pumpEvents(300);
    visibleMainWindow = findVisibleProcessTopLevelWindow(window.windowTitle());
    const QPoint captionGlobalPoint = findCaptionHitPoint(visibleMainWindow, ribbonBar);
    if (!verify(captionGlobalPoint != QPoint(), "Ribbon title area should expose a draggable HTCAPTION point")) {
        return false;
    }

    QWidget* captionTarget = QApplication::widgetAt(captionGlobalPoint);
    if (captionTarget == nullptr || (captionTarget != ribbonBar && !ribbonBar->isAncestorOf(captionTarget))) {
        captionTarget = ribbonBar;
    }

    const QPoint localCaptionPoint = captionTarget->mapFromGlobal(captionGlobalPoint);
    QMouseEvent doubleClickEvent(
        QEvent::MouseButtonDblClick,
        QPointF(localCaptionPoint),
        QPointF(captionGlobalPoint),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(captionTarget, &doubleClickEvent);
    pumpEvents(250);
    if (!verify(window.isMaximized(), "Double-clicking ribbon blank area should maximize the window")) {
        return false;
    }

    visibleMainWindow = findVisibleProcessTopLevelWindow(window.windowTitle());
    const QPoint maximizedCaptionPoint = findCaptionHitPoint(visibleMainWindow, ribbonBar);
    if (!verify(maximizedCaptionPoint != QPoint(), "Maximized window should keep a draggable HTCAPTION point")) {
        return false;
    }

    QWidget* maximizedCaptionTarget = QApplication::widgetAt(maximizedCaptionPoint);
    if (maximizedCaptionTarget == nullptr || (maximizedCaptionTarget != ribbonBar && !ribbonBar->isAncestorOf(maximizedCaptionTarget))) {
        maximizedCaptionTarget = ribbonBar;
    }

    const QPoint maximizedLocalCaptionPoint = maximizedCaptionTarget->mapFromGlobal(maximizedCaptionPoint);
    QMouseEvent restoreDoubleClickEvent(
        QEvent::MouseButtonDblClick,
        QPointF(maximizedLocalCaptionPoint),
        QPointF(maximizedCaptionPoint),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(maximizedCaptionTarget, &restoreDoubleClickEvent);
    pumpEvents(250);
    if (!verify(!window.isMaximized(), "Double-clicking ribbon blank area again should restore the window")) {
        return false;
    }
#endif

    Qtitan::RibbonSystemButton* systemButton = ribbonBar->getSystemButton();
    if (!verify(systemButton != nullptr, "Ribbon system button should exist")) {
        return false;
    }

    Qtitan::RibbonBackstageView* backstageView =
        window.findChild<Qtitan::RibbonBackstageView*>(QStringLiteral("mainBackstageView"));
    if (!verify(backstageView != nullptr, "Backstage view should be created")) {
        return false;
    }

    systemButton->click();
    pumpEvents(200);
    if (!verify(ribbonBar->isBackstageVisible(), "Backstage should become visible after clicking system button")) {
        return false;
    }

    QWidget* applicationSettingsPage =
        window.findChild<QWidget*>(QStringLiteral("backstageApplicationSettingsPage"));
    QWidget* aboutPage = window.findChild<QWidget*>(QStringLiteral("backstageAboutPage"));
    if (!verify(applicationSettingsPage != nullptr, "Application Settings backstage page should exist")) {
        return false;
    }
    if (!verify(aboutPage != nullptr, "About backstage page should exist")) {
        return false;
    }

    backstageView->setActivePage(applicationSettingsPage);
    if (!verify(
            backstageView->getActivePage() == applicationSettingsPage,
            "Backstage should switch to Application Settings page")) {
        return false;
    }

    if (!verify(window.backstageCaptureSaveDirectoryLineEdit_ != nullptr, "Application Settings should expose capture save directory input")) {
        return false;
    }
    if (!verify(window.backstageCaptureBrowseButton_ != nullptr, "Application Settings should expose capture folder browse button")) {
        return false;
    }
    if (!verify(window.backstageCaptureAutoSaveCheckBox_ != nullptr, "Application Settings should expose capture auto-save checkbox")) {
        return false;
    }
    if (!verify(window.backstageCaptureShortcutHintLabel_ != nullptr, "Application Settings should expose capture shortcut hint label")) {
        return false;
    }
    if (!verify(!window.backstageCaptureShortcutHintLabel_->text().trimmed().isEmpty(), "Capture shortcut hint label should not be empty")) {
        return false;
    }

    backstageView->setActivePage(aboutPage);
    if (!verify(
            backstageView->getActivePage() == aboutPage,
            "Backstage should switch to About page")) {
        return false;
    }

    backstageView->hide();
    pumpEvents(120);
    if (!verify(!ribbonBar->isBackstageVisible(), "Backstage should hide when requested")) {
        return false;
    }

    window.close();
    pumpEvents(200);
    std::cout << "[PASS] Main backstage smoke test completed." << std::endl;
    return true;
}

bool runMainWindowSettingsRestoreSmoke(const QStringList&)
{
    QTemporaryDir settingsDir;
    if (!verify(settingsDir.isValid(), "Settings restore smoke should create a temporary settings directory")) {
        return false;
    }

    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QCoreApplication::setOrganizationName(QStringLiteral("LASViewerSmokeTest"));
    QCoreApplication::setApplicationName(QStringLiteral("MainWindowSettingsRestoreSmoke"));

    {
        QSettings settings;
        settings.clear();
        settings.sync();
    }

    QTranslator appTranslator;
    QTranslator qtTranslator;

    int expectedInspectorTabIndex = 0;
    int expectedRouteDetailsTabIndex = 0;
    int expectedWaypointLabelMode = static_cast<int>(RouteLabelDisplayMode::Name);
    int expectedPartLabelMode = static_cast<int>(RouteLabelDisplayMode::Name);
    const int expectedLogFilterLevel = 1;
    const QString expectedLogKeyword = QStringLiteral("restore smoke");
    const bool expectedLogAutoScroll = false;
    const bool expectedWaypointShowCoordinates = false;
    const bool expectedWaypointShowCaptureAngles = false;
    const bool expectedPartShowCoordinates = false;
    const bool expectedPartShowCaptureAngles = false;
    const double expectedRoamSpeed = 4.5;
    const int expectedRoamViewMode = static_cast<int>(RouteRoamViewMode::FirstPerson);
    const bool expectedInvertOrbit = true;
    const bool expectedInvertPan = true;
    const bool expectedInvertWheel = true;
    const int expectedWheelZoomSensitivity = 145;
    const int expectedManualRightDockWidth = 460;
    const QString expectedCaptureDirectory = QDir::toNativeSeparators(
        QDir(settingsDir.path()).filePath(QStringLiteral("captures")));
    const bool expectedCaptureAutoSave = true;
    bool savedShowLog = false;
    bool savedShowProfileClassification = false;
    bool savedShowRouteDetails = false;
    bool savedStatePresent = false;
    bool savedGeometryPresent = false;
    bool savedInvertOrbit = false;
    bool savedInvertPan = false;
    bool savedInvertWheel = false;
    int savedWheelZoomSensitivity = 0;
    int savedRightDockWidth = 0;
    QString savedCaptureDirectory;
    bool savedCaptureAutoSave = false;
    int expectedRightDockWidth = 0;

    {
        MainWindow window(&appTranslator, &qtTranslator);
        window.resize(1400, 900);
        window.show();
        pumpEvents(300);

        if (!verify(window.logDock_ != nullptr, "Settings restore smoke should create the log dock")) {
            return false;
        }
        if (!verify(window.profileClassificationDock_ != nullptr, "Settings restore smoke should create the profile classification dock")) {
            return false;
        }
        if (!verify(window.routeDetailsDock_ != nullptr, "Settings restore smoke should create the route details dock")) {
            return false;
        }
        if (!verify(window.inspectorTabWidget_ != nullptr, "Settings restore smoke should create the inspector tab widget")) {
            return false;
        }
        if (!verify(window.routeDetailsTabWidget_ != nullptr, "Settings restore smoke should create the route details tab widget")) {
            return false;
        }
        if (!verify(window.routeWaypointLabelModeComboBox_ != nullptr, "Settings restore smoke should create the waypoint label mode combo box")) {
            return false;
        }
        if (!verify(window.routePartLabelModeComboBox_ != nullptr, "Settings restore smoke should create the part label mode combo box")) {
            return false;
        }
        if (!verify(window.routeWaypointShowCoordinatesCheckBox_ != nullptr, "Settings restore smoke should create the waypoint coordinate checkbox")) {
            return false;
        }
        if (!verify(window.routeWaypointShowCaptureAnglesCheckBox_ != nullptr, "Settings restore smoke should create the waypoint angle checkbox")) {
            return false;
        }
        if (!verify(window.routePartShowCoordinatesCheckBox_ != nullptr, "Settings restore smoke should create the part coordinate checkbox")) {
            return false;
        }
        if (!verify(window.routePartShowCaptureAnglesCheckBox_ != nullptr, "Settings restore smoke should create the part angle checkbox")) {
            return false;
        }
        if (!verify(window.routeRoamSpeedSpinBox_ != nullptr, "Settings restore smoke should create the route roam speed spin box")) {
            return false;
        }
        if (!verify(window.routeRoamViewModeComboBox_ != nullptr, "Settings restore smoke should create the route roam view mode combo box")) {
            return false;
        }
        if (!verify(window.invertOrbitCheckBox_ != nullptr, "Settings restore smoke should create the invert orbit checkbox")) {
            return false;
        }
        if (!verify(window.invertPanCheckBox_ != nullptr, "Settings restore smoke should create the invert pan checkbox")) {
            return false;
        }
        if (!verify(window.invertWheelCheckBox_ != nullptr, "Settings restore smoke should create the invert wheel checkbox")) {
            return false;
        }
        if (!verify(window.wheelZoomSensitivitySlider_ != nullptr, "Settings restore smoke should create the wheel sensitivity slider")) {
            return false;
        }
        if (!verify(window.backstageCaptureSaveDirectoryLineEdit_ != nullptr, "Settings restore smoke should create the capture save directory input")) {
            return false;
        }
        if (!verify(window.backstageCaptureAutoSaveCheckBox_ != nullptr, "Settings restore smoke should create the capture auto-save checkbox")) {
            return false;
        }

        window.showLogAction_->setChecked(true);
        window.showProfileClassificationDockAction_->setChecked(true);
        window.routeDetailsDock_->show();
        window.routeDetailsDock_->raise();
        pumpEvents(120);

        expectedInspectorTabIndex = window.inspectorTabWidget_->count() > 1 ? 1 : 0;
        expectedRouteDetailsTabIndex = window.routeDetailsTabWidget_->count() > 1 ? 1 : 0;
        window.inspectorTabWidget_->setCurrentIndex(expectedInspectorTabIndex);
        window.routeDetailsTabWidget_->setCurrentIndex(expectedRouteDetailsTabIndex);

        if (window.routeWaypointLabelModeComboBox_->count() > 1) {
            window.routeWaypointLabelModeComboBox_->setCurrentIndex(1);
        }
        if (window.routePartLabelModeComboBox_->count() > 1) {
            window.routePartLabelModeComboBox_->setCurrentIndex(1);
        }
        expectedWaypointLabelMode = window.routeWaypointLabelModeComboBox_->currentData().toInt();
        expectedPartLabelMode = window.routePartLabelModeComboBox_->currentData().toInt();
        if (!verify(
                window.viewer_ != nullptr
                    && static_cast<int>(window.viewer_->inspectionRouteWaypointLabelDisplayMode()) == expectedWaypointLabelMode,
                "Waypoint label mode combo box should sync into viewer route label mode")) {
            return false;
        }
        if (!verify(
                window.viewer_ != nullptr
                    && static_cast<int>(window.viewer_->inspectionRoutePartLabelDisplayMode()) == expectedPartLabelMode,
                "Part label mode combo box should sync into viewer route label mode")) {
            return false;
        }

        window.routeWaypointShowCoordinatesCheckBox_->setChecked(expectedWaypointShowCoordinates);
        window.routeWaypointShowCaptureAnglesCheckBox_->setChecked(expectedWaypointShowCaptureAngles);
        window.routePartShowCoordinatesCheckBox_->setChecked(expectedPartShowCoordinates);
        window.routePartShowCaptureAnglesCheckBox_->setChecked(expectedPartShowCaptureAngles);
        window.logDock_->setSelectedFilterLevel(expectedLogFilterLevel);
        window.logDock_->setSearchKeyword(expectedLogKeyword);
        window.logDock_->setAutoScrollEnabled(expectedLogAutoScroll);
        window.routeRoamSpeedSpinBox_->setValue(expectedRoamSpeed);
        const int roamViewModeIndex =
            window.routeRoamViewModeComboBox_->findData(expectedRoamViewMode);
        if (!verify(roamViewModeIndex >= 0, "Settings restore smoke should expose the first-person roam view mode")) {
            return false;
        }
        window.routeRoamViewModeComboBox_->setCurrentIndex(roamViewModeIndex);
        window.invertOrbitCheckBox_->setChecked(expectedInvertOrbit);
        window.invertPanCheckBox_->setChecked(expectedInvertPan);
        window.invertWheelCheckBox_->setChecked(expectedInvertWheel);
        window.wheelZoomSensitivitySlider_->setValue(expectedWheelZoomSensitivity);
        window.backstageCaptureSaveDirectoryLineEdit_->setText(expectedCaptureDirectory);
        QMetaObject::invokeMethod(
            window.backstageCaptureSaveDirectoryLineEdit_,
            "editingFinished",
            Qt::DirectConnection);
        window.backstageCaptureAutoSaveCheckBox_->setChecked(expectedCaptureAutoSave);
        window.routeDetailsDock_->raise();
        pumpEvents(80);
        window.resizeDocks(
            { window.inspectorDock_, window.routeDetailsDock_ },
            { expectedManualRightDockWidth, expectedManualRightDockWidth },
            Qt::Horizontal);
        pumpEvents(120);
        expectedRightDockWidth = window.routeDetailsDock_->width();
        window.inspectorDock_->raise();
        pumpEvents(120);
        const int firstWindowInspectorWidth = window.inspectorDock_->width();
        if (!verify(
                std::abs(firstWindowInspectorWidth - expectedRightDockWidth) <= 8,
                "Switching to inspector should not rewrite the manually widened right dock width")) {
            return false;
        }
        window.routeDetailsDock_->raise();
        pumpEvents(120);
        const int firstWindowRouteWidthAfterSwitch = window.routeDetailsDock_->width();
        if (!verify(
                std::abs(firstWindowRouteWidthAfterSwitch - expectedRightDockWidth) <= 8,
                "Switching back to route details should keep the widened right dock width stable")) {
            return false;
        }

        if (!verify(window.showLogAction_ != nullptr && window.showLogAction_->isChecked(),
                "Settings restore smoke should keep the first window log action checked")) {
            return false;
        }
        if (!verify(window.logDock_->isVisible(),
                "Settings restore smoke should keep the first window log dock visible before close")) {
            return false;
        }
        if (!verify(
                window.showProfileClassificationDockAction_ != nullptr
                    && window.showProfileClassificationDockAction_->isChecked(),
                "Settings restore smoke should keep the first window profile classification action checked")) {
            return false;
        }
        if (!verify(window.profileClassificationDock_->isVisible(),
                "Settings restore smoke should keep the first window profile classification dock visible before close")) {
            return false;
        }
        {
            QSettings settings;
            if (!verify(settings.value(QStringLiteral("window/showLog"), false).toBool(),
                    "Settings restore smoke should persist window/showLog before close")) {
                return false;
            }
            if (!verify(settings.value(QStringLiteral("window/showProfileClassification"), false).toBool(),
                    "Settings restore smoke should persist window/showProfileClassification before close")) {
                return false;
            }
        }

        window.close();
        pumpEvents(120);
        QSettings().sync();

        QSettings settings;
        savedShowLog = settings.value(QStringLiteral("window/showLog"), false).toBool();
        savedShowProfileClassification = settings.value(
            QStringLiteral("window/showProfileClassification"), false).toBool();
        savedShowRouteDetails = settings.value(QStringLiteral("window/showRouteDetails"), false).toBool();
        savedStatePresent = !settings.value(QStringLiteral("window/state")).toByteArray().isEmpty();
        savedGeometryPresent = !settings.value(QStringLiteral("window/geometry")).toByteArray().isEmpty();
        savedInvertOrbit = settings.value(QStringLiteral("interaction/invertOrbitDrag"), false).toBool();
        savedInvertPan = settings.value(QStringLiteral("interaction/invertPanDrag"), false).toBool();
        savedInvertWheel = settings.value(QStringLiteral("interaction/invertWheelZoom"), false).toBool();
        savedWheelZoomSensitivity = settings.value(QStringLiteral("interaction/wheelZoomSensitivityPercent"), 0).toInt();
        savedRightDockWidth = settings.value(QStringLiteral("window/rightDockWidth"), 0).toInt();
        savedCaptureDirectory = settings.value(QStringLiteral("capture/saveDirectory")).toString();
        savedCaptureAutoSave = settings.value(QStringLiteral("capture/skipSaveDialog"), false).toBool();
    }

    if (!verify(savedShowLog, "Settings restore smoke should persist window/showLog as true")) {
        return false;
    }
    if (!verify(savedShowProfileClassification, "Settings restore smoke should persist window/showProfileClassification as true")) {
        return false;
    }
    if (!verify(savedShowRouteDetails, "Settings restore smoke should persist window/showRouteDetails as true")) {
        return false;
    }
    if (!verify(savedStatePresent, "Settings restore smoke should persist window/state")) {
        return false;
    }
    if (!verify(savedGeometryPresent, "Settings restore smoke should persist window/geometry")) {
        return false;
    }
    if (!verify(savedInvertOrbit == expectedInvertOrbit, "Settings restore smoke should persist interaction/invertOrbitDrag")) {
        return false;
    }
    if (!verify(savedInvertPan == expectedInvertPan, "Settings restore smoke should persist interaction/invertPanDrag")) {
        return false;
    }
    if (!verify(savedInvertWheel == expectedInvertWheel, "Settings restore smoke should persist interaction/invertWheelZoom")) {
        return false;
    }
    if (!verify(savedWheelZoomSensitivity == expectedWheelZoomSensitivity, "Settings restore smoke should persist interaction/wheelZoomSensitivityPercent")) {
        return false;
    }
    if (!verify(savedRightDockWidth > 0, "Settings restore smoke should persist window/rightDockWidth")) {
        return false;
    }
    if (!verify(savedCaptureDirectory == expectedCaptureDirectory, "Settings restore smoke should persist capture/saveDirectory")) {
        return false;
    }
    if (!verify(savedCaptureAutoSave == expectedCaptureAutoSave, "Settings restore smoke should persist capture/skipSaveDialog")) {
        return false;
    }

    {
        MainWindow restoredWindow(&appTranslator, &qtTranslator);
        restoredWindow.resize(1400, 900);
        restoredWindow.show();
        pumpEvents(350);

        if (!verify(restoredWindow.logDock_ != nullptr, "Restored window should keep the log dock")) {
            return false;
        }
        if (!verify(restoredWindow.profileClassificationDock_ != nullptr, "Restored window should keep the profile classification dock")) {
            return false;
        }
        if (!verify(restoredWindow.routeDetailsDock_ != nullptr, "Restored window should keep the route details dock")) {
            return false;
        }
        if (!verify(
                restoredWindow.showLogAction_ != nullptr && restoredWindow.showLogAction_->isChecked(),
                "Window settings restore should keep the log action checked")) {
            return false;
        }
        if (!verify(restoredWindow.logDock_->isVisible(), "Window settings restore should keep the log dock visible")) {
            return false;
        }
        if (!verify(restoredWindow.profileClassificationDock_->isVisible(), "Window settings restore should keep the profile classification dock visible")) {
            return false;
        }
        if (!verify(restoredWindow.routeDetailsDock_->isVisible(), "Window settings restore should keep the route details dock visible")) {
            return false;
        }
        if (!verify(restoredWindow.profileDock_ != nullptr && !restoredWindow.profileDock_->isVisible(), "Profile dock should still follow measurement mode after restore")) {
            return false;
        }
        if (!verify(
                restoredWindow.inspectorTabWidget_ != nullptr
                    && restoredWindow.inspectorTabWidget_->currentIndex() == expectedInspectorTabIndex,
                "Window settings restore should recover the inspector tab index")) {
            return false;
        }
        if (!verify(
                restoredWindow.routeDetailsTabWidget_ != nullptr
                    && restoredWindow.routeDetailsTabWidget_->currentIndex() == expectedRouteDetailsTabIndex,
                "Window settings restore should recover the route details tab index")) {
            return false;
        }
        if (!verify(
                restoredWindow.logDock_->selectedFilterLevel() == expectedLogFilterLevel,
                "Window settings restore should recover the log filter level")) {
            return false;
        }
        if (!verify(
                restoredWindow.logDock_->searchKeyword() == expectedLogKeyword,
                "Window settings restore should recover the log search keyword")) {
            return false;
        }
        if (!verify(
                restoredWindow.logDock_->autoScrollEnabled() == expectedLogAutoScroll,
                "Window settings restore should recover the log auto-scroll state")) {
            return false;
        }
        if (!verify(
                restoredWindow.routeWaypointLabelModeComboBox_ != nullptr
                    && restoredWindow.routeWaypointLabelModeComboBox_->currentData().toInt() == expectedWaypointLabelMode,
                "Window settings restore should recover the waypoint label mode")) {
            return false;
        }
        if (!verify(
                restoredWindow.routePartLabelModeComboBox_ != nullptr
                    && restoredWindow.routePartLabelModeComboBox_->currentData().toInt() == expectedPartLabelMode,
                "Window settings restore should recover the part label mode")) {
            return false;
        }
        if (!verify(
                restoredWindow.viewer_ != nullptr
                    && static_cast<int>(restoredWindow.viewer_->inspectionRouteWaypointLabelDisplayMode()) == expectedWaypointLabelMode,
                "Window settings restore should sync waypoint label mode back into the viewer")) {
            return false;
        }
        if (!verify(
                restoredWindow.viewer_ != nullptr
                    && static_cast<int>(restoredWindow.viewer_->inspectionRoutePartLabelDisplayMode()) == expectedPartLabelMode,
                "Window settings restore should sync part label mode back into the viewer")) {
            return false;
        }
        if (!verify(
                restoredWindow.routeWaypointShowCoordinatesCheckBox_ != nullptr
                    && restoredWindow.routeWaypointShowCoordinatesCheckBox_->isChecked() == expectedWaypointShowCoordinates,
                "Window settings restore should recover the waypoint coordinate toggle")) {
            return false;
        }
        if (!verify(
                restoredWindow.routeWaypointShowCaptureAnglesCheckBox_ != nullptr
                    && restoredWindow.routeWaypointShowCaptureAnglesCheckBox_->isChecked() == expectedWaypointShowCaptureAngles,
                "Window settings restore should recover the waypoint angle toggle")) {
            return false;
        }
        if (!verify(
                restoredWindow.routePartShowCoordinatesCheckBox_ != nullptr
                    && restoredWindow.routePartShowCoordinatesCheckBox_->isChecked() == expectedPartShowCoordinates,
                "Window settings restore should recover the part coordinate toggle")) {
            return false;
        }
        if (!verify(
                restoredWindow.routePartShowCaptureAnglesCheckBox_ != nullptr
                    && restoredWindow.routePartShowCaptureAnglesCheckBox_->isChecked() == expectedPartShowCaptureAngles,
                "Window settings restore should recover the part angle toggle")) {
            return false;
        }
        if (!verify(
                restoredWindow.routeRoamSpeedSpinBox_ != nullptr
                    && std::abs(restoredWindow.routeRoamSpeedSpinBox_->value() - expectedRoamSpeed) < 0.001,
                "Window settings restore should recover the route roam speed")) {
            return false;
        }
        if (!verify(
                restoredWindow.routeRoamViewModeComboBox_ != nullptr
                    && restoredWindow.routeRoamViewModeComboBox_->currentData().toInt() == expectedRoamViewMode,
                "Window settings restore should recover the route roam view mode")) {
            return false;
        }
        if (!verify(
                restoredWindow.backstageCaptureSaveDirectoryLineEdit_ != nullptr
                    && restoredWindow.backstageCaptureSaveDirectoryLineEdit_->text() == expectedCaptureDirectory,
                "Window settings restore should recover the capture save directory")) {
            return false;
        }
        if (!verify(
                restoredWindow.backstageCaptureAutoSaveCheckBox_ != nullptr
                    && restoredWindow.backstageCaptureAutoSaveCheckBox_->isChecked() == expectedCaptureAutoSave,
                "Window settings restore should recover capture auto-save state")) {
            return false;
        }
        if (!verify(
                restoredWindow.viewer_ != nullptr
                    && std::abs(restoredWindow.viewer_->inspectionRouteRoamSpeedMetersPerSecond() - expectedRoamSpeed) < 0.001,
                "Window settings restore should sync the route roam speed back into the viewer")) {
            return false;
        }
        if (!verify(
                restoredWindow.viewer_ != nullptr
                    && static_cast<int>(restoredWindow.viewer_->inspectionRouteRoamViewMode()) == expectedRoamViewMode,
                "Window settings restore should sync the route roam view mode back into the viewer")) {
            return false;
        }
        if (!verify(
                restoredWindow.viewer_ != nullptr
                    && restoredWindow.viewer_->interactionOptions().invertOrbitDrag == expectedInvertOrbit,
                "Window settings restore should recover invert orbit drag")) {
            return false;
        }
        if (!verify(
                restoredWindow.viewer_ != nullptr
                    && restoredWindow.viewer_->interactionOptions().invertPanDrag == expectedInvertPan,
                "Window settings restore should recover invert pan drag")) {
            return false;
        }
        if (!verify(
                restoredWindow.viewer_ != nullptr
                    && restoredWindow.viewer_->interactionOptions().invertWheelZoom == expectedInvertWheel,
                "Window settings restore should recover invert wheel zoom")) {
            return false;
        }
        if (!verify(
                restoredWindow.viewer_ != nullptr
                    && restoredWindow.viewer_->interactionOptions().wheelZoomSensitivityPercent == expectedWheelZoomSensitivity,
                "Window settings restore should recover wheel zoom sensitivity")) {
            return false;
        }
        if (!verify(
                restoredWindow.invertOrbitCheckBox_ != nullptr
                    && restoredWindow.invertOrbitCheckBox_->isChecked() == expectedInvertOrbit,
                "Window settings restore should sync invert orbit checkbox")) {
            return false;
        }
        if (!verify(
                restoredWindow.wheelZoomSensitivitySlider_ != nullptr
                    && restoredWindow.wheelZoomSensitivitySlider_->value() == expectedWheelZoomSensitivity,
                "Window settings restore should sync wheel sensitivity slider")) {
            return false;
        }

        restoredWindow.routeDetailsDock_->raise();
        pumpEvents(120);
        const int restoredRouteDetailsWidth = restoredWindow.routeDetailsDock_->width();
        if (!verify(
                std::abs(restoredRouteDetailsWidth - expectedRightDockWidth) <= 24,
                "Window settings restore should keep the route details dock width near the saved value")) {
            return false;
        }
        restoredWindow.inspectorDock_->raise();
        pumpEvents(120);
        const int restoredInspectorWidth = restoredWindow.inspectorDock_->width();
        if (!verify(
                std::abs(restoredInspectorWidth - restoredRouteDetailsWidth) <= 8,
                "Switching right dock tabs should keep inspector and route details widths aligned")) {
            return false;
        }

        restoredWindow.close();
        pumpEvents(120);
    }

    std::cout << "[PASS] Main window settings restore smoke test completed." << std::endl;
    return true;
}

bool runLogPanelSmoke(const QStringList&)
{
    lasviewer::logging::ApplicationLogger::instance().clear();
    lasviewer::logging::ApplicationLogger::instance().log(
        lasviewer::logging::LogLevel::Info,
        QStringLiteral("UI"),
        QStringLiteral("Dock created"));
    lasviewer::logging::ApplicationLogger::instance().log(
        lasviewer::logging::LogLevel::Warning,
        QStringLiteral("Route"),
        QStringLiteral("Route review warning"));
    lasviewer::logging::ApplicationLogger::instance().log(
        lasviewer::logging::LogLevel::Error,
        QStringLiteral("Tower"),
        QStringLiteral("Tower sync failed"));

    ApplicationLogDock dock;
    dock.resize(900, 320);
    dock.show();
    pumpEvents(120);

    if (!verify(dock.totalEntryCount() == 3, "Log dock should load all logger entries")) {
        return false;
    }
    if (!verify(dock.visibleEntryCount() == 3, "Log dock should show all entries by default")) {
        return false;
    }

    dock.setSelectedFilterLevel(static_cast<int>(lasviewer::logging::LogLevel::Warning));
    pumpEvents(60);
    if (!verify(dock.visibleEntryCount() == 1, "Warning filter should show one entry")) {
        return false;
    }

    dock.setSelectedFilterLevel(-1);
    dock.setSearchKeyword(QStringLiteral("tower"));
    pumpEvents(60);
    if (!verify(dock.visibleEntryCount() == 1, "Search keyword should narrow results to one entry")) {
        return false;
    }

    lasviewer::logging::ApplicationLogger::instance().clear();
    pumpEvents(60);
    if (!verify(dock.totalEntryCount() == 0, "Clearing logger should refresh dock state")) {
        return false;
    }

    std::cout << "[PASS] Log panel smoke test completed." << std::endl;
    return true;
}

bool runProjectExplorerDockSmoke(const QStringList&)
{
    ProjectExplorerDock dock;
    dock.resize(960, 420);

    QAction openAction(QStringLiteral("Open"), &dock);
    QAction addAction(QStringLiteral("Add"), &dock);
    QAction removeAction(QStringLiteral("Remove"), &dock);
    dock.toolBar()->addAction(&openAction);
    dock.toolBar()->addAction(&addAction);
    dock.toolBar()->addAction(&removeAction);

    auto* projectItem = new QTreeWidgetItem(QStringList { QStringLiteral("Project Properties") });
    dock.treeWidget()->addTopLevelItem(projectItem);
    dock.searchEdit()->setText(QStringLiteral("project"));
    dock.show();
    pumpEvents(120);

    if (!verify(dock.toolBar()->actions().size() == 3, "Project explorer toolbar should expose injected actions")) {
        return false;
    }
    if (!verify(dock.treeWidget()->topLevelItemCount() == 1, "Project explorer should expose the tree widget")) {
        return false;
    }
    if (!verify(dock.searchEdit()->text() == QStringLiteral("project"), "Project explorer should expose the search field")) {
        return false;
    }

    std::cout << "[PASS] Project explorer dock smoke test completed." << std::endl;
    return true;
}

bool runProjectExplorerControllerSmoke(const QStringList&)
{
    ProjectExplorerDock dock;
    QAction openAction(QStringLiteral("Open"), &dock);
    QAction addAction(QStringLiteral("Add"), &dock);
    QAction removeAction(QStringLiteral("Remove"), &dock);
    QAction locateAction(QStringLiteral("Locate"), &dock);
    QAction copyAction(QStringLiteral("Copy"), &dock);
    QAction expandAction(QStringLiteral("Expand"), &dock);
    QAction collapseAction(QStringLiteral("Collapse"), &dock);

    ProjectExplorerController controller(
        &dock,
        &openAction,
        &addAction,
        &removeAction,
        &locateAction,
        &copyAction,
        &expandAction,
        &collapseAction);

    auto* root = new QTreeWidgetItem(QStringList { QStringLiteral("Root") });
    auto* child = new QTreeWidgetItem(QStringList { QStringLiteral("Child") });
    root->addChild(child);
    dock.treeWidget()->addTopLevelItem(root);
    auto* anotherRoot = new QTreeWidgetItem(QStringList { QStringLiteral("Images") });
    dock.treeWidget()->addTopLevelItem(anotherRoot);

    int searchSignalCount = 0;
    int openRequestedCount = 0;
    int locateRequestedCount = 0;
    QObject::connect(&controller, &ProjectExplorerController::searchTextChanged, &dock, [&](const QString&) {
        ++searchSignalCount;
    });
    QObject::connect(&controller, &ProjectExplorerController::openRequested, &dock, [&]() {
        ++openRequestedCount;
    });
    QObject::connect(&controller, &ProjectExplorerController::locateSelectedRequested, &dock, [&]() {
        ++locateRequestedCount;
    });

    dock.show();
    pumpEvents(100);

    int nonSeparatorActionCount = 0;
    for (QAction* action : dock.toolBar()->actions()) {
        if (action != nullptr && !action->isSeparator()) {
            ++nonSeparatorActionCount;
        }
    }
    if (!verify(nonSeparatorActionCount == 7, "Project explorer controller should populate toolbar actions")) {
        return false;
    }

    dock.searchEdit()->setText(QStringLiteral("child"));
    pumpEvents(50);
    if (!verify(searchSignalCount == 1, "Controller should forward search text changes")) {
        return false;
    }
    controller.refreshFilter();
    if (!verify(!root->isHidden(), "Matching branch should stay visible after filter")) {
        return false;
    }
    if (!verify(anotherRoot->isHidden(), "Non-matching branch should hide after filter")) {
        return false;
    }

    openAction.trigger();
    locateAction.trigger();
    pumpEvents(30);
    if (!verify(openRequestedCount == 1, "Controller should emit openRequested when open action triggers")) {
        return false;
    }
    if (!verify(locateRequestedCount == 1, "Controller should emit locateSelectedRequested when locate action triggers")) {
        return false;
    }

    expandAction.trigger();
    pumpEvents(50);
    if (!verify(root->isExpanded(), "Expand action should expand tree items")) {
        return false;
    }

    collapseAction.trigger();
    pumpEvents(50);
    if (!verify(root->isExpanded(), "Collapse action should keep the first root expanded")) {
        return false;
    }

    std::cout << "[PASS] Project explorer controller smoke test completed." << std::endl;
    return true;
}

bool runProjectExplorerMainWindowSmoke(const QStringList& filePaths)
{
    QTranslator appTranslator;
    QTranslator qtTranslator;
    MainWindow window(&appTranslator, &qtTranslator);
    window.resize(1400, 900);
    window.show();
    pumpEvents(300);

    ProjectExplorerDock* projectDock = window.findChild<ProjectExplorerDock*>();
    if (!verify(projectDock != nullptr, "MainWindow project explorer smoke should create the project explorer dock")) {
        return false;
    }

    PointCloudViewer* viewer = window.findChild<PointCloudViewer*>();
    if (!verify(viewer != nullptr, "MainWindow project explorer smoke should create the embedded point cloud viewer")) {
        return false;
    }

    QTreeWidget* projectTree = projectDock->treeWidget();
    QLineEdit* searchEdit = projectDock->searchEdit();
    if (!verify(projectTree != nullptr, "Project explorer dock should expose the tree widget")) {
        return false;
    }
    if (!verify(searchEdit != nullptr, "Project explorer dock should expose the search edit")) {
        return false;
    }

    const QString lasFilePath = filePaths.isEmpty() ? QString() : QFileInfo(filePaths.constFirst()).absoluteFilePath();
    if (!verify(!lasFilePath.isEmpty() && QFileInfo::exists(lasFilePath), "Project explorer MainWindow smoke requires an existing LAS file")) {
        return false;
    }

    QString pointCloudErrorMessage;
    if (!viewer->loadPointCloud(lasFilePath, &pointCloudErrorMessage)) {
        std::cerr << "[FAIL] Project explorer MainWindow smoke failed to load point cloud: "
                  << pointCloudErrorMessage.toStdString() << std::endl;
        return false;
    }

    pumpEvents(1200);
    if (!verify(!viewer->pointCloudDatasets().isEmpty(), "Loaded point cloud dataset should be available in viewer state")) {
        return false;
    }

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Project explorer MainWindow smoke should create a temporary directory")) {
        return false;
    }

    const QString smokeImagePath = QDir(tempDir.path()).filePath(QStringLiteral("project_explorer_smoke.png"));
    QImage smokeImage(64, 48, QImage::Format_ARGB32_Premultiplied);
    smokeImage.fill(QColor(248, 250, 252));
    if (!verify(smokeImage.save(smokeImagePath), "Project explorer MainWindow smoke should create a temporary image attachment")) {
        return false;
    }

    InspectionIssue smokeIssue;
    smokeIssue.id = QStringLiteral("project-explorer-issue-001");
    smokeIssue.title = QStringLiteral("Project Explorer Smoke Issue");
    smokeIssue.description = QStringLiteral("MainWindow integration smoke");
    smokeIssue.severity = IssueSeverity::Major;
    smokeIssue.imagePath = smokeImagePath;
    smokeIssue.point = PointRecord { 18.0f, 22.0f, 12.0f };
    viewer->setInspectionIssues({ smokeIssue });
    viewer->setSelectedIssueIndex(-1);

    QList<TowerRecord> routePlanningTowers;
    TowerRecord routePlanningTower;
    routePlanningTower.index = 0;
    routePlanningTower.name = QStringLiteral("Project Explorer Tower");
    routePlanningTower.point = PointRecord { 15.0f, 20.0f, 25.0f };
    routePlanningTowers.append(routePlanningTower);
    viewer->setTowerMarkers(routePlanningTowers);
    pumpEvents(80);

    VegetationRiskRecord routeRisk;
    routeRisk.id = QStringLiteral("project-explorer-risk-001");
    routeRisk.title = QStringLiteral("Project Explorer Risk");
    routeRisk.severity = AnalysisSeverity::Warning;
    routeRisk.point = PointRecord { 40.0f, 50.0f, 18.0f };
    routeRisk.minimumDistance = 4.5f;
    routeRisk.chainageStart = 10.0f;
    routeRisk.chainageEnd = 20.0f;
    routeRisk.sourceRule = QStringLiteral("Smoke-Rule");
    routeRisk.notes = QStringLiteral("Project Explorer route generation smoke");
    window.vegetationRiskResults_ = { routeRisk };
    window.selectedVegetationRiskIndex_ = 0;

    if (!verify(window.generateInspectionRouteAction_ != nullptr, "MainWindow project explorer smoke should expose the generate route action")) {
        return false;
    }
    window.generateInspectionRouteAction_->setEnabled(true);
    window.generateInspectionRouteAction_->trigger();
    pumpEvents(200);
    if (!verify(!window.currentPowerlineRoute_.waypoints.isEmpty(), "Project explorer MainWindow smoke should generate a route through MainWindow")) {
        return false;
    }
    if (!verify(!viewer->inspectionRouteWaypoints().isEmpty(), "Generated route should sync preview waypoints into the viewer")) {
        return false;
    }

    pumpEvents(250);

    QTreeWidgetItem* coordinateSystemsItem = findProjectTreeItem(projectTree, [](QTreeWidgetItem* item) {
        return item != nullptr && item->data(0, Qt::UserRole).toString() == QStringLiteral("coordinateSystemsItem");
    });
    QTreeWidgetItem* pointCloudItem = findProjectTreeItem(projectTree, [&](QTreeWidgetItem* item) {
        return item != nullptr
            && item->data(0, Qt::UserRole).toString() == QStringLiteral("pointCloudItem")
            && item->data(0, Qt::UserRole + 1).toString().compare(lasFilePath, Qt::CaseInsensitive) == 0;
    });
    QTreeWidgetItem* imageItem = findProjectTreeItem(projectTree, [&](QTreeWidgetItem* item) {
        return item != nullptr
            && item->data(0, Qt::UserRole).toString() == QStringLiteral("imageItem")
            && item->data(0, Qt::UserRole + 1).toString().compare(smokeImagePath, Qt::CaseInsensitive) == 0;
    });
    QTreeWidgetItem* trajectoryItem = findProjectTreeItem(projectTree, [](QTreeWidgetItem* item) {
        return item != nullptr && item->data(0, Qt::UserRole).toString() == QStringLiteral("trajectoryItem");
    });

    if (!verify(coordinateSystemsItem != nullptr, "Project tree should keep the project management item")) {
        return false;
    }
    if (!verify(pointCloudItem != nullptr, "Project tree should expose the loaded point cloud item")) {
        return false;
    }
    if (!verify(imageItem != nullptr, "Project tree should expose the inspection image item")) {
        return false;
    }
    if (!verify(trajectoryItem != nullptr, "Project tree should expose the trajectory item")) {
        return false;
    }

    projectTree->setCurrentItem(pointCloudItem);
    pumpEvents(80);
    searchEdit->setText(QStringLiteral("project_explorer_smoke"));
    pumpEvents(120);
    if (!verify(projectTree->currentItem() == nullptr, "Filtering out the current point cloud row should clear current tree selection")) {
        return false;
    }
    if (!verify(!imageItem->isHidden(), "Project tree filter should keep the matching image item visible")) {
        return false;
    }
    if (!verify(pointCloudItem->isHidden(), "Project tree filter should hide the non-matching point cloud item")) {
        return false;
    }
    if (!verify(trajectoryItem->isHidden(), "Project tree filter should hide the non-matching trajectory item")) {
        return false;
    }

    searchEdit->clear();
    pumpEvents(120);
    if (!verify(!pointCloudItem->isHidden() && !imageItem->isHidden() && !trajectoryItem->isHidden(), "Clearing the project tree filter should restore all project items")) {
        return false;
    }

    projectTree->setCurrentItem(imageItem);
    pumpEvents(80);
    if (!verify(viewer->selectedIssueIndex() == 0, "Selecting the image item should sync the selected issue into the viewer")) {
        return false;
    }

    window.selectedRouteWaypointIndex_ = -1;
    viewer->setSelectedInspectionRouteWaypointIndex(-1);
    projectTree->setCurrentItem(trajectoryItem);
    pumpEvents(80);
    if (!verify(window.selectedRouteWaypointIndex_ == 0, "Selecting the trajectory item should restore the first route waypoint selection")) {
        return false;
    }
    if (!verify(viewer->selectedInspectionRouteWaypointIndex() == 0, "Selecting the trajectory item should sync the first route waypoint into the viewer")) {
        return false;
    }

    projectTree->setCurrentItem(pointCloudItem);
    pumpEvents(80);
    if (!verify(viewer->selectedIssueIndex() == -1, "Selecting the point cloud item should clear the selected issue")) {
        return false;
    }

    pointCloudItem->setCheckState(0, Qt::Unchecked);
    pumpEvents(80);
    if (!verify(!viewer->pointCloudDatasets().constFirst().visible, "Point cloud item check state should sync dataset visibility into viewer state")) {
        return false;
    }
    pointCloudItem->setCheckState(0, Qt::Checked);
    pumpEvents(80);
    if (!verify(viewer->pointCloudDatasets().constFirst().visible, "Re-checking point cloud item should restore dataset visibility")) {
        return false;
    }

    imageItem->setCheckState(0, Qt::Unchecked);
    pumpEvents(80);
    if (!verify(!viewer->isInspectionIssueVisible(0), "Image item check state should sync inspection issue visibility into viewer state")) {
        return false;
    }
    imageItem->setCheckState(0, Qt::Checked);
    pumpEvents(80);
    if (!verify(viewer->isInspectionIssueVisible(0), "Re-checking image item should restore inspection issue visibility")) {
        return false;
    }

    trajectoryItem->setCheckState(0, Qt::Unchecked);
    pumpEvents(80);
    if (!verify(!viewer->inspectionRouteVisible(), "Trajectory item check state should sync route visibility into viewer state")) {
        return false;
    }
    trajectoryItem->setCheckState(0, Qt::Checked);
    pumpEvents(80);
    if (!verify(viewer->inspectionRouteVisible(), "Re-checking trajectory item should restore route visibility")) {
        return false;
    }

    projectTree->setCurrentItem(pointCloudItem);
    pumpEvents(60);
    const QPoint imageContextPosition = projectTree->visualItemRect(imageItem).center();
    if (!invokeTreeContextMenuAndClose(projectTree, imageContextPosition, "Project tree image context menu should stay invokable")) {
        return false;
    }
    if (!verify(projectTree->currentItem() == imageItem, "Project tree context menu should update the current row before opening the image menu")) {
        return false;
    }

    projectTree->setCurrentItem(imageItem);
    pumpEvents(60);
    const QPoint trajectoryContextPosition = projectTree->visualItemRect(trajectoryItem).center();
    if (!invokeTreeContextMenuAndClose(projectTree, trajectoryContextPosition, "Project tree trajectory context menu should stay invokable")) {
        return false;
    }
    if (!verify(projectTree->currentItem() == trajectoryItem, "Project tree context menu should update the current row before opening the trajectory menu")) {
        return false;
    }

    viewer->setSelectedIssueIndex(-1);
    projectTree->setCurrentItem(coordinateSystemsItem);
    pumpEvents(60);
    if (!emitTreeItemDoubleClick(projectTree, imageItem, 0, "Project tree image double click should stay invokable")) {
        return false;
    }
    if (!verify(viewer->selectedIssueIndex() == 0, "Project tree image double click should focus the corresponding inspection issue")) {
        return false;
    }

    std::cout << "[PASS] Project explorer MainWindow smoke test completed." << std::endl;
    return true;
}

bool runProfileClassificationWidgetSmoke(const QStringList&)
{
    ProfileClassificationWidget widget;
    widget.resize(420, 760);
    widget.show();
    pumpEvents(120);

    if (!verify(widget.title() == QString::fromUtf8("3D Profile Classification"), "Profile classification widget should set its group title")) {
        return false;
    }
    if (!verify(widget.modeComboBox() != nullptr, "Profile classification widget should expose mode combo box")) {
        return false;
    }
    if (!verify(widget.modeComboBox()->count() == 2, "Profile classification widget should provide 2 selection modes")) {
        return false;
    }
    if (!verify(widget.sourceListWidget() != nullptr, "Profile classification widget should expose source list widget")) {
        return false;
    }
    if (!verify(widget.targetListWidget() != nullptr, "Profile classification widget should expose target list widget")) {
        return false;
    }

    std::cout << "[PASS] Profile classification widget smoke test completed." << std::endl;
    return true;
}

bool runProfileClassificationControllerSmoke(const QStringList& filePaths)
{
    PointCloudViewer viewer;
    viewer.resize(1024, 768);
    viewer.show();
    pumpEvents(300);

    const QString lasPath = filePaths.isEmpty() ? QString() : filePaths.first();
    if (!verify(!lasPath.isEmpty(), "Profile classification controller smoke requires LAS input")) {
        return false;
    }
    if (!verify(QFileInfo::exists(lasPath), "Profile classification controller smoke LAS file should exist")) {
        return false;
    }

    QString errorMessage;
    if (!viewer.loadPointCloud(lasPath, &errorMessage)) {
        std::cerr << "[FAIL] loadPointCloud: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    pumpEvents(900);

    ProfileClassificationWidget widget;
    QAction profileAction(QStringLiteral("Profile"), &widget);
    profileAction.setCheckable(true);
    QAction saveAction(QStringLiteral("Save"), &widget);
    QAction undoAction(QStringLiteral("Undo"), &widget);
    QAction redoAction(QStringLiteral("Redo"), &widget);
    QAction clearAction(QStringLiteral("Clear"), &widget);

    ProfileClassificationController controller(
        &widget,
        &viewer,
        &profileAction,
        &saveAction,
        &undoAction,
        &redoAction,
        &clearAction,
        [](int classificationCode) {
            return QStringLiteral("Class %1").arg(classificationCode);
        });

    controller.initializeClassificationItems(QList<int> { 2, 5, 16 });

    widget.resize(420, 760);
    widget.show();
    pumpEvents(120);

    if (!verify(widget.sourceListWidget()->count() == 3, "Profile classification controller should initialize source list items")) {
        return false;
    }
    if (!verify(widget.targetListWidget()->count() == 3, "Profile classification controller should initialize target list items")) {
        return false;
    }
    if (!verify(widget.sourceListWidget()->item(0)->text().contains(QStringLiteral("Class")), "Controller should apply classification display names")) {
        return false;
    }

    widget.selectAllButton()->click();
    pumpEvents(60);
    if (!verify(viewer.profileClassificationSourceClasses().size() == 3, "Select all should sync source classes into viewer")) {
        return false;
    }

    widget.clearSelectionButton()->click();
    pumpEvents(60);
    if (!verify(viewer.profileClassificationSourceClasses().isEmpty(), "Clear sources should clear selected classes in viewer")) {
        return false;
    }

    widget.targetListWidget()->setCurrentRow(1);
    pumpEvents(40);
    if (!verify(viewer.profileClassificationTargetClass() == 5, "Target list selection should sync target class into viewer")) {
        return false;
    }

    widget.modeComboBox()->setCurrentIndex(1);
    pumpEvents(40);
    if (!verify(
            viewer.profileClassificationSelectionMode() == ProfileClassificationSelectionMode::Polygon,
            "Mode combo box should sync selection mode into viewer")) {
        return false;
    }

    std::cout << "[PASS] Profile classification controller smoke test completed." << std::endl;
    return true;
}

bool runVisualizationPanelControllerSmoke(const QStringList&)
{
    PointCloudViewer viewer;

    QAction showAxesAction(QStringLiteral("Axes"), &viewer);
    showAxesAction.setCheckable(true);
    QAction showBoundingBoxAction(QStringLiteral("Bounds"), &viewer);
    showBoundingBoxAction.setCheckable(true);
    QAction darkBackgroundAction(QStringLiteral("Dark"), &viewer);
    QAction lightBackgroundAction(QStringLiteral("Light"), &viewer);
    QAction rgbColorAction(QStringLiteral("RGB"), &viewer);
    QAction elevationColorAction(QStringLiteral("Elevation"), &viewer);
    QAction singleColorAction(QStringLiteral("Single"), &viewer);
    QAction classificationColorAction(QStringLiteral("Classification"), &viewer);

    QSlider pointSizeSlider(Qt::Horizontal);
    pointSizeSlider.setRange(1, 20);
    QLabel pointSizeLabel;
    QSlider pointOpacitySlider(Qt::Horizontal);
    pointOpacitySlider.setRange(10, 100);
    QLabel pointOpacityLabel;
    QSlider depthCueSlider(Qt::Horizontal);
    depthCueSlider.setRange(0, 100);
    QLabel depthCueLabel;
    QSlider edlStrengthSlider(Qt::Horizontal);
    edlStrengthSlider.setRange(0, 100);
    QLabel edlStrengthLabel;

    QComboBox colorModeComboBox;
    colorModeComboBox.addItem(QStringLiteral("RGB"));
    colorModeComboBox.addItem(QStringLiteral("Elevation"));
    colorModeComboBox.addItem(QStringLiteral("Single"));
    colorModeComboBox.addItem(QStringLiteral("Classification"));

    QPushButton pointColorButton(QStringLiteral("Point"));
    QPushButton backgroundColorButton(QStringLiteral("Background"));
    int choosePointColorCount = 0;
    int chooseBackgroundColorCount = 0;

    VisualizationPanelController controller(
        &viewer,
        &showAxesAction,
        &showBoundingBoxAction,
        &darkBackgroundAction,
        &lightBackgroundAction,
        &rgbColorAction,
        &elevationColorAction,
        &singleColorAction,
        &classificationColorAction,
        &pointSizeSlider,
        &pointSizeLabel,
        &pointOpacitySlider,
        &pointOpacityLabel,
        &depthCueSlider,
        &depthCueLabel,
        &edlStrengthSlider,
        &edlStrengthLabel,
        &colorModeComboBox,
        &pointColorButton,
        &backgroundColorButton,
        [&choosePointColorCount]() { ++choosePointColorCount; },
        [&chooseBackgroundColorCount]() { ++chooseBackgroundColorCount; });
    Q_UNUSED(controller);

    const bool initialAxesVisible = viewer.visualizationOptions().showAxes;
    showAxesAction.setChecked(initialAxesVisible);
    showAxesAction.setChecked(!initialAxesVisible);
    if (!verify(
            viewer.visualizationOptions().showAxes == !initialAxesVisible,
            "Visualization controller should sync axes action to viewer")) {
        return false;
    }

    const bool initialBoundsVisible = viewer.visualizationOptions().showBoundingBox;
    showBoundingBoxAction.setChecked(initialBoundsVisible);
    showBoundingBoxAction.setChecked(!initialBoundsVisible);
    if (!verify(
            viewer.visualizationOptions().showBoundingBox == !initialBoundsVisible,
            "Visualization controller should sync bounds action to viewer")) {
        return false;
    }

    darkBackgroundAction.trigger();
    if (!verify(
            viewer.visualizationOptions().backgroundColor == QColor(20, 28, 38),
            "Visualization controller should apply dark background action")) {
        return false;
    }

    lightBackgroundAction.trigger();
    if (!verify(
            viewer.visualizationOptions().backgroundColor == QColor(241, 244, 249),
            "Visualization controller should apply light background action")) {
        return false;
    }

    classificationColorAction.trigger();
    if (!verify(
            viewer.visualizationOptions().colorMode == PointCloudColorMode::Classification,
            "Visualization controller should sync classification color action")) {
        return false;
    }

    pointSizeSlider.setValue(12);
    if (!verify(
            viewer.visualizationOptions().pointSize == 12,
            "Visualization controller should sync point size slider to viewer")) {
        return false;
    }
    if (!verify(
            pointSizeLabel.text().contains(QStringLiteral("12")),
            "Visualization controller should update point size label text")) {
        return false;
    }

    pointOpacitySlider.setValue(65);
    if (!verifyClose(
            viewer.visualizationOptions().pointOpacity,
            0.65,
            0.01,
            "Visualization controller should sync point opacity slider to viewer")) {
        return false;
    }

    colorModeComboBox.setCurrentIndex(2);
    if (!verify(
            viewer.visualizationOptions().colorMode == PointCloudColorMode::SingleColor,
            "Visualization controller should sync color mode combo box")) {
        return false;
    }

    pointColorButton.click();
    backgroundColorButton.click();
    if (!verify(choosePointColorCount == 1, "Visualization controller should forward point color button click")) {
        return false;
    }
    if (!verify(chooseBackgroundColorCount == 1, "Visualization controller should forward background button click")) {
        return false;
    }

    std::cout << "[PASS] Visualization panel controller smoke test completed." << std::endl;
    return true;
}

bool runMeasurementAnalysisControllerSmoke(const QStringList& filePaths)
{
    PointCloudViewer viewer;
    viewer.resize(1024, 768);
    viewer.show();
    pumpEvents(300);

    const QString lasPath = filePaths.isEmpty() ? QString() : filePaths.first();
    if (!verify(!lasPath.isEmpty(), "Measurement analysis controller smoke requires LAS input")) {
        return false;
    }
    if (!verify(QFileInfo::exists(lasPath), "Measurement analysis controller smoke LAS file should exist")) {
        return false;
    }

    QString errorMessage;
    if (!viewer.loadPointCloud(lasPath, &errorMessage)) {
        std::cerr << "[FAIL] loadPointCloud: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    pumpEvents(900);

    InspectionRouteDisplayData measurementSmokeRoute;
    PointRecord measurementRouteWaypointA;
    measurementRouteWaypointA.x = 0.0f;
    measurementRouteWaypointA.y = 0.0f;
    measurementRouteWaypointA.z = 20.0f;
    PointRecord measurementRouteWaypointB;
    measurementRouteWaypointB.x = 40.0f;
    measurementRouteWaypointB.y = 25.0f;
    measurementRouteWaypointB.z = 24.0f;
    measurementSmokeRoute.waypoints.append(measurementRouteWaypointA);
    measurementSmokeRoute.waypoints.append(measurementRouteWaypointB);
    measurementSmokeRoute.labels.append(QStringLiteral("1"));
    measurementSmokeRoute.labels.append(QStringLiteral("2"));
    viewer.setInspectionRouteDisplayData(measurementSmokeRoute);
    const int initialRouteWaypointCount = viewer.inspectionRouteWaypoints().size();
    if (!verify(initialRouteWaypointCount == 2, "Measurement controller smoke should initialize route waypoints")) {
        return false;
    }
    if (!verify(viewer.inspectionRouteVisible(), "Measurement controller smoke should keep route visible before measurement toggle")) {
        return false;
    }

    QAction measureAction(QStringLiteral("Measure"), &viewer);
    measureAction.setCheckable(true);
    QAction clearMeasurementAction(QStringLiteral("Clear"), &viewer);
    QAction exportClearanceCsvAction(QStringLiteral("Export CSV"), &viewer);
    QAction analyzeVegetationRisksAction(QStringLiteral("Analyze"), &viewer);
    QAction focusVegetationRiskAction(QStringLiteral("Focus"), &viewer);
    QAction createIssueFromRiskAction(QStringLiteral("Create One"), &viewer);
    QAction createIssuesFromRisksAction(QStringLiteral("Create All"), &viewer);
    QAction clearVegetationRisksAction(QStringLiteral("Clear Risks"), &viewer);

    QPushButton measurementToggleButton(QStringLiteral("Toggle"));
    QPushButton measurementClearButton(QStringLiteral("Clear"));
    QDoubleSpinBox clearanceThresholdSpinBox;
    QComboBox clearanceRulePresetComboBox;
    clearanceRulePresetComboBox.addItem(QStringLiteral("Preset A"), 1);
    clearanceRulePresetComboBox.addItem(QStringLiteral("Preset B"), 2);
    QDoubleSpinBox vegetationSearchRadiusSpinBox;
    QDoubleSpinBox vegetationClusterGapSpinBox;
    QSpinBox vegetationClusterPointCountSpinBox;
    QCheckBox preferVegetationClassificationCheckBox;
    QTableWidget clearanceSegmentsTableWidget(2, 1);
    QTableWidget vegetationRisksTableWidget(2, 1);

    int syncProfileDockCallCount = 0;
    int exportClearanceCsvCallCount = 0;
    int analyzeVegetationRisksCallCount = 0;
    int focusVegetationRiskCallCount = 0;
    int createIssueFromSelectedRiskCallCount = 0;
    int createIssuesFromRisksCallCount = 0;
    int clearVegetationRisksCallCount = 0;
    double latestClearanceThreshold = 0.0;
    int latestClearanceRulePresetIndex = -1;
    double latestVegetationSearchRadius = 0.0;
    double latestVegetationClusterGap = 0.0;
    int latestVegetationClusterPointCount = -1;
    bool latestPreferVegetationClassification = false;
    int latestSelectedClearanceSegment = -1;
    int latestSelectedVegetationRisk = -1;

    MeasurementAnalysisController controller(
        &viewer,
        &measureAction,
        &clearMeasurementAction,
        &exportClearanceCsvAction,
        &analyzeVegetationRisksAction,
        &focusVegetationRiskAction,
        &createIssueFromRiskAction,
        &createIssuesFromRisksAction,
        &clearVegetationRisksAction,
        &measurementToggleButton,
        &measurementClearButton,
        &clearanceThresholdSpinBox,
        &clearanceRulePresetComboBox,
        &vegetationSearchRadiusSpinBox,
        &vegetationClusterGapSpinBox,
        &vegetationClusterPointCountSpinBox,
        &preferVegetationClassificationCheckBox,
        &clearanceSegmentsTableWidget,
        &vegetationRisksTableWidget,
        [&syncProfileDockCallCount](bool) { ++syncProfileDockCallCount; },
        [&exportClearanceCsvCallCount]() { ++exportClearanceCsvCallCount; },
        [&analyzeVegetationRisksCallCount]() { ++analyzeVegetationRisksCallCount; },
        [&focusVegetationRiskCallCount]() { ++focusVegetationRiskCallCount; },
        [&createIssueFromSelectedRiskCallCount]() { ++createIssueFromSelectedRiskCallCount; },
        [&createIssuesFromRisksCallCount]() { ++createIssuesFromRisksCallCount; },
        [&clearVegetationRisksCallCount]() { ++clearVegetationRisksCallCount; },
        [&latestClearanceThreshold](double value) { latestClearanceThreshold = value; },
        [&latestClearanceRulePresetIndex](int index) { latestClearanceRulePresetIndex = index; },
        [&latestVegetationSearchRadius](double value) { latestVegetationSearchRadius = value; },
        [&latestVegetationClusterGap](double value) { latestVegetationClusterGap = value; },
        [&latestVegetationClusterPointCount](int value) { latestVegetationClusterPointCount = value; },
        [&latestPreferVegetationClassification](bool checked) { latestPreferVegetationClassification = checked; },
        [&latestSelectedClearanceSegment](int row) { latestSelectedClearanceSegment = row; },
        [&latestSelectedVegetationRisk](int row) { latestSelectedVegetationRisk = row; });
    Q_UNUSED(controller);

    const bool initialMeasurementEnabledFromAction = viewer.measurementEnabled();
    measureAction.setChecked(initialMeasurementEnabledFromAction);
    measureAction.setChecked(!initialMeasurementEnabledFromAction);
    if (!verify(
            viewer.measurementEnabled() == !initialMeasurementEnabledFromAction,
            "Measurement controller should sync measure action to viewer")) {
        return false;
    }
    if (!verify(
            viewer.inspectionRouteVisible()
                && viewer.inspectionRouteWaypoints().size() == initialRouteWaypointCount,
            "Measurement controller should not hide or clear route when enabling measurement")) {
        return false;
    }
    if (!verify(syncProfileDockCallCount == 1, "Measurement controller should trigger profile dock sync callback")) {
        return false;
    }

    const bool initialMeasurementEnabled = viewer.measurementEnabled();
    measurementToggleButton.click();
    if (!verify(
            viewer.measurementEnabled() == !initialMeasurementEnabled,
            "Measurement controller should toggle measurement mode from button")) {
        return false;
    }
    if (!verify(
            viewer.inspectionRouteVisible()
                && viewer.inspectionRouteWaypoints().size() == initialRouteWaypointCount,
            "Measurement controller should keep route visibility and data when disabling measurement")) {
        return false;
    }

    exportClearanceCsvAction.trigger();
    analyzeVegetationRisksAction.trigger();
    focusVegetationRiskAction.trigger();
    createIssueFromRiskAction.trigger();
    createIssuesFromRisksAction.trigger();
    clearVegetationRisksAction.trigger();
    if (!verify(exportClearanceCsvCallCount == 1, "Measurement controller should forward export action")) {
        return false;
    }
    if (!verify(analyzeVegetationRisksCallCount == 1, "Measurement controller should forward analyze action")) {
        return false;
    }
    if (!verify(focusVegetationRiskCallCount == 1, "Measurement controller should forward focus action")) {
        return false;
    }
    if (!verify(createIssueFromSelectedRiskCallCount == 1, "Measurement controller should forward create-one action")) {
        return false;
    }
    if (!verify(createIssuesFromRisksCallCount == 1, "Measurement controller should forward create-all action")) {
        return false;
    }
    if (!verify(clearVegetationRisksCallCount == 1, "Measurement controller should forward clear-risks action")) {
        return false;
    }

    clearanceThresholdSpinBox.setValue(12.5);
    if (!verifyClose(latestClearanceThreshold, 12.5, 0.001, "Measurement controller should forward threshold changes")) {
        return false;
    }
    clearanceRulePresetComboBox.setCurrentIndex(1);
    if (!verify(latestClearanceRulePresetIndex == 1, "Measurement controller should forward rule preset index")) {
        return false;
    }
    vegetationSearchRadiusSpinBox.setValue(24.0);
    if (!verifyClose(
            latestVegetationSearchRadius,
            24.0,
            0.001,
            "Measurement controller should forward vegetation search radius")) {
        return false;
    }
    vegetationClusterGapSpinBox.setValue(6.0);
    if (!verifyClose(
            latestVegetationClusterGap,
            6.0,
            0.001,
            "Measurement controller should forward vegetation cluster gap")) {
        return false;
    }
    vegetationClusterPointCountSpinBox.setValue(5);
    if (!verify(
            latestVegetationClusterPointCount == 5,
            "Measurement controller should forward vegetation cluster point count")) {
        return false;
    }
    preferVegetationClassificationCheckBox.setChecked(true);
    if (!verify(
            latestPreferVegetationClassification,
            "Measurement controller should forward prefer vegetation toggle")) {
        return false;
    }

    clearanceSegmentsTableWidget.setCurrentCell(1, 0);
    vegetationRisksTableWidget.setCurrentCell(1, 0);
    if (!verify(
            latestSelectedClearanceSegment == 1,
            "Measurement controller should forward clearance table row selection")) {
        return false;
    }
    if (!verify(
            latestSelectedVegetationRisk == 1,
            "Measurement controller should forward vegetation table row selection")) {
        return false;
    }

    std::cout << "[PASS] Measurement analysis controller smoke test completed." << std::endl;
    return true;
}

bool runRouteControllerSmoke(const QStringList&)
{
    PointCloudViewer viewer;

    QAction generateInspectionRouteAction(QStringLiteral("Generate"), &viewer);
    QAction regenerateInspectionRouteAction(QStringLiteral("Regenerate"), &viewer);
    QAction clearInspectionRouteAction(QStringLiteral("Clear Route"), &viewer);
    QAction toggleRouteEditingAction(QStringLiteral("Edit"), &viewer);
    toggleRouteEditingAction.setCheckable(true);
    QAction startInspectionRouteRoamAction(QStringLiteral("Start Roam"), &viewer);
    QAction pauseInspectionRouteRoamAction(QStringLiteral("Pause Roam"), &viewer);
    QAction stopInspectionRouteRoamAction(QStringLiteral("Stop Roam"), &viewer);
    QAction focusRouteWaypointAction(QStringLiteral("Focus Waypoint"), &viewer);
    QAction importRouteFileAction(QStringLiteral("Import Route"), &viewer);
    QAction saveRouteFileAction(QStringLiteral("Save Route"), &viewer);
    QAction saveRouteFileAsAction(QStringLiteral("Save Route As"), &viewer);
    QAction reloadRouteFileAction(QStringLiteral("Reload Route"), &viewer);
    QAction importRouteKmlAction(QStringLiteral("Import KML"), &viewer);
    QAction exportRouteKmlAction(QStringLiteral("Export KML"), &viewer);
    QAction exportRouteDjiKmzAction(QStringLiteral("Export KMZ"), &viewer);

    QPushButton routeRoamStartButton(QStringLiteral("Start"));
    QPushButton routeRoamPauseResumeButton(QStringLiteral("Pause"));
    QPushButton routeRoamStopButton(QStringLiteral("Stop"));
    QDoubleSpinBox routeRoamSpeedSpinBox;
    QComboBox routeRoamViewModeComboBox;
    routeRoamViewModeComboBox.addItem(QStringLiteral("First"), 0);
    routeRoamViewModeComboBox.addItem(QStringLiteral("Third"), 1);

    int regenerateInspectionRouteCount = 0;
    int clearInspectionRouteCount = 0;
    int setRouteEditingEnabledCount = 0;
    bool latestRouteEditingEnabled = false;
    int startInspectionRouteRoamCount = 0;
    int pauseResumeInspectionRouteRoamCount = 0;
    int stopInspectionRouteRoamCount = 0;
    double latestRouteRoamSpeed = 0.0;
    int latestRouteRoamViewModeIndex = -1;
    int focusRouteWaypointCount = 0;
    int importRouteFileCount = 0;
    int saveRouteFileCount = 0;
    int saveRouteFileAsCount = 0;
    int reloadRouteFileCount = 0;
    int importRouteKmlCount = 0;
    int exportRouteKmlCount = 0;
    int exportRouteDjiKmzCount = 0;
    int inspectionRouteRoamStateChangedCount = 0;
    int inspectionRouteRoamPhotoCapturedCount = 0;

    QList<PointRecord> roamWaypoints;
    PointRecord first;
    first.x = 0.0f;
    first.y = 0.0f;
    first.z = 30.0f;
    roamWaypoints.append(first);
    PointRecord second;
    second.x = 120.0f;
    second.y = 40.0f;
    second.z = 32.0f;
    roamWaypoints.append(second);
    PointRecord third;
    third.x = 240.0f;
    third.y = 80.0f;
    third.z = 35.0f;
    roamWaypoints.append(third);

    RouteController controller(
        &viewer,
        &generateInspectionRouteAction,
        &regenerateInspectionRouteAction,
        &clearInspectionRouteAction,
        &toggleRouteEditingAction,
        &startInspectionRouteRoamAction,
        &pauseInspectionRouteRoamAction,
        &stopInspectionRouteRoamAction,
        &focusRouteWaypointAction,
        &importRouteFileAction,
        &saveRouteFileAction,
        &saveRouteFileAsAction,
        &reloadRouteFileAction,
        &importRouteKmlAction,
        &exportRouteKmlAction,
        &exportRouteDjiKmzAction,
        &routeRoamStartButton,
        &routeRoamPauseResumeButton,
        &routeRoamStopButton,
        &routeRoamSpeedSpinBox,
        &routeRoamViewModeComboBox,
        [&regenerateInspectionRouteCount]() { ++regenerateInspectionRouteCount; },
        [&clearInspectionRouteCount]() { ++clearInspectionRouteCount; },
        [&setRouteEditingEnabledCount, &latestRouteEditingEnabled](bool enabled) {
            ++setRouteEditingEnabledCount;
            latestRouteEditingEnabled = enabled;
        },
        [&startInspectionRouteRoamCount]() { ++startInspectionRouteRoamCount; },
        [&pauseResumeInspectionRouteRoamCount]() { ++pauseResumeInspectionRouteRoamCount; },
        [&stopInspectionRouteRoamCount]() { ++stopInspectionRouteRoamCount; },
        [&latestRouteRoamSpeed](double speed) { latestRouteRoamSpeed = speed; },
        [&latestRouteRoamViewModeIndex](int index) { latestRouteRoamViewModeIndex = index; },
        [&focusRouteWaypointCount]() { ++focusRouteWaypointCount; },
        [&importRouteFileCount]() { ++importRouteFileCount; },
        [&saveRouteFileCount]() { ++saveRouteFileCount; },
        [&saveRouteFileAsCount]() { ++saveRouteFileAsCount; },
        [&reloadRouteFileCount]() { ++reloadRouteFileCount; },
        [&importRouteKmlCount]() { ++importRouteKmlCount; },
        [&exportRouteKmlCount]() { ++exportRouteKmlCount; },
        [&exportRouteDjiKmzCount]() { ++exportRouteDjiKmzCount; },
        [&inspectionRouteRoamStateChangedCount]() { ++inspectionRouteRoamStateChangedCount; },
        [&inspectionRouteRoamPhotoCapturedCount](int, int, const QString&, int) {
            ++inspectionRouteRoamPhotoCapturedCount;
        });
    Q_UNUSED(controller);

    generateInspectionRouteAction.trigger();
    regenerateInspectionRouteAction.trigger();
    if (!verify(regenerateInspectionRouteCount == 2, "Route controller should route generate/regenerate actions")) {
        return false;
    }

    clearInspectionRouteAction.trigger();
    if (!verify(clearInspectionRouteCount == 1, "Route controller should route clear route action")) {
        return false;
    }

    toggleRouteEditingAction.setChecked(true);
    if (!verify(setRouteEditingEnabledCount == 1 && latestRouteEditingEnabled, "Route controller should route route-edit toggles")) {
        return false;
    }

    startInspectionRouteRoamAction.trigger();
    pauseInspectionRouteRoamAction.trigger();
    stopInspectionRouteRoamAction.trigger();
    if (!verify(startInspectionRouteRoamCount == 1, "Route controller should route start roam action")) {
        return false;
    }
    if (!verify(pauseResumeInspectionRouteRoamCount == 1, "Route controller should route pause/resume roam action")) {
        return false;
    }
    if (!verify(stopInspectionRouteRoamCount == 1, "Route controller should route stop roam action")) {
        return false;
    }

    routeRoamStartButton.click();
    routeRoamPauseResumeButton.click();
    routeRoamStopButton.click();
    if (!verify(startInspectionRouteRoamCount == 2, "Route controller should bridge start roam button")) {
        return false;
    }
    if (!verify(pauseResumeInspectionRouteRoamCount == 2, "Route controller should bridge pause/resume roam button")) {
        return false;
    }
    if (!verify(stopInspectionRouteRoamCount == 2, "Route controller should bridge stop roam button")) {
        return false;
    }

    routeRoamSpeedSpinBox.setValue(6.5);
    routeRoamViewModeComboBox.setCurrentIndex(1);
    if (!verifyClose(latestRouteRoamSpeed, 6.5, 0.001, "Route controller should route roam speed changes")) {
        return false;
    }
    if (!verify(latestRouteRoamViewModeIndex == 1, "Route controller should route roam view mode changes")) {
        return false;
    }

    focusRouteWaypointAction.trigger();
    importRouteFileAction.trigger();
    saveRouteFileAction.trigger();
    saveRouteFileAsAction.trigger();
    reloadRouteFileAction.trigger();
    importRouteKmlAction.trigger();
    exportRouteKmlAction.trigger();
    exportRouteDjiKmzAction.trigger();
    if (!verify(focusRouteWaypointCount == 1, "Route controller should route focus waypoint action")) {
        return false;
    }
    if (!verify(importRouteFileCount == 1, "Route controller should route import route action")) {
        return false;
    }
    if (!verify(saveRouteFileCount == 1, "Route controller should route save route action")) {
        return false;
    }
    if (!verify(saveRouteFileAsCount == 1, "Route controller should route save-as route action")) {
        return false;
    }
    if (!verify(reloadRouteFileCount == 1, "Route controller should route reload route action")) {
        return false;
    }
    if (!verify(importRouteKmlCount == 1, "Route controller should route import KML action")) {
        return false;
    }
    if (!verify(exportRouteKmlCount == 1, "Route controller should route export KML action")) {
        return false;
    }
    if (!verify(exportRouteDjiKmzCount == 1, "Route controller should route export KMZ action")) {
        return false;
    }

    if (!verify(
            inspectionRouteRoamStateChangedCount == 0,
            "Route controller roam state callback should remain idle without viewer roam transitions")) {
        return false;
    }
    if (!verify(
            inspectionRouteRoamPhotoCapturedCount == 0,
            "Route controller photo callback should remain idle when no viewer photo capture occurs")) {
        return false;
    }

    std::cout << "[PASS] Route controller smoke test completed." << std::endl;
    return true;
}

bool runTowerControllerSmoke(const QStringList&)
{
    QAction startTowerEditAction(QStringLiteral("Start Edit"), nullptr);
    QAction finishTowerEditAction(QStringLiteral("Finish Edit"), nullptr);
    QAction addTowerAction(QStringLiteral("Add Tower"), nullptr);
    QAction insertTowerAction(QStringLiteral("Insert Tower"), nullptr);
    QAction moveTowerAction(QStringLiteral("Move Tower"), nullptr);
    QAction editCurrentTowerAction(QStringLiteral("Edit Current"), nullptr);
    QAction focusTowerAction(QStringLiteral("Focus Tower"), nullptr);
    QAction removeTowerAction(QStringLiteral("Remove Tower"), nullptr);
    QAction clearTowersAction(QStringLiteral("Clear Towers"), nullptr);
    QAction cancelTowerToolAction(QStringLiteral("Cancel Tool"), nullptr);
    QAction importTowerFileAction(QStringLiteral("Import"), nullptr);
    QAction saveTowerFileAction(QStringLiteral("Save"), nullptr);
    QAction saveTowerFileAsAction(QStringLiteral("Save As"), nullptr);
    QAction reloadTowerFileAction(QStringLiteral("Reload"), nullptr);
    QAction showTowerXAction(QStringLiteral("Show X"), nullptr);
    QAction showTowerYAction(QStringLiteral("Show Y"), nullptr);
    QAction showTowerZAction(QStringLiteral("Show Z"), nullptr);
    showTowerXAction.setCheckable(true);
    showTowerYAction.setCheckable(true);
    showTowerZAction.setCheckable(true);

    QTableWidget towerTableWidget(2, 5);
    towerTableWidget.setItem(0, 1, new QTableWidgetItem(QStringLiteral("T-001")));
    towerTableWidget.setItem(1, 1, new QTableWidgetItem(QStringLiteral("T-002")));

    QLineEdit towerCodeEdit;
    QLineEdit towerLineNameEdit;
    QLineEdit towerVoltageLevelEdit;
    QComboBox towerTypeComboBox;
    towerTypeComboBox.addItem(QStringLiteral("Unknown"), 0);
    towerTypeComboBox.addItem(QStringLiteral("Tangent"), 1);
    QLineEdit towerStructureTypeEdit;
    QLineEdit towerInspectionDateEdit;
    QLineEdit towerStatusEdit;
    QPlainTextEdit towerNotesEdit;

    int startEditCount = 0;
    int finishEditCount = 0;
    int addTowerCount = 0;
    int insertTowerCount = 0;
    int moveTowerCount = 0;
    int editCurrentCount = 0;
    int focusTowerCount = 0;
    int removeTowerCount = 0;
    int clearTowersCount = 0;
    int cancelToolCount = 0;
    int importTowerFileCount = 0;
    int saveTowerFileCount = 0;
    int saveTowerFileAsCount = 0;
    int reloadTowerFileCount = 0;
    int showColumnToggleCount = 0;
    int selectionChangedCount = 0;
    int latestSelectedRow = -1;
    int towerNameEditedCount = 0;
    int latestEditedRow = -1;
    QString latestEditedName;
    int commitTowerDetailsCount = 0;

    TowerController controller(
        &startTowerEditAction,
        &finishTowerEditAction,
        &addTowerAction,
        &insertTowerAction,
        &moveTowerAction,
        &editCurrentTowerAction,
        &focusTowerAction,
        &removeTowerAction,
        &clearTowersAction,
        &cancelTowerToolAction,
        &importTowerFileAction,
        &saveTowerFileAction,
        &saveTowerFileAsAction,
        &reloadTowerFileAction,
        &showTowerXAction,
        &showTowerYAction,
        &showTowerZAction,
        &towerTableWidget,
        &towerCodeEdit,
        &towerLineNameEdit,
        &towerVoltageLevelEdit,
        &towerTypeComboBox,
        &towerStructureTypeEdit,
        &towerInspectionDateEdit,
        &towerStatusEdit,
        &towerNotesEdit,
        [&startEditCount]() { ++startEditCount; },
        [&finishEditCount]() { ++finishEditCount; },
        [&addTowerCount]() { ++addTowerCount; },
        [&insertTowerCount]() { ++insertTowerCount; },
        [&moveTowerCount]() { ++moveTowerCount; },
        [&editCurrentCount]() { ++editCurrentCount; },
        [&focusTowerCount]() { ++focusTowerCount; },
        [&removeTowerCount]() { ++removeTowerCount; },
        [&clearTowersCount]() { ++clearTowersCount; },
        [&cancelToolCount]() { ++cancelToolCount; },
        [&importTowerFileCount]() { ++importTowerFileCount; },
        [&saveTowerFileCount]() { ++saveTowerFileCount; },
        [&saveTowerFileAsCount]() { ++saveTowerFileAsCount; },
        [&reloadTowerFileCount]() { ++reloadTowerFileCount; },
        [&showColumnToggleCount](bool) { ++showColumnToggleCount; },
        [&showColumnToggleCount](bool) { ++showColumnToggleCount; },
        [&showColumnToggleCount](bool) { ++showColumnToggleCount; },
        [&selectionChangedCount, &latestSelectedRow](int currentRow) {
            ++selectionChangedCount;
            latestSelectedRow = currentRow;
        },
        [&towerNameEditedCount, &latestEditedRow, &latestEditedName](int row, const QString& name) {
            ++towerNameEditedCount;
            latestEditedRow = row;
            latestEditedName = name;
        },
        [&commitTowerDetailsCount]() {
            ++commitTowerDetailsCount;
        });
    Q_UNUSED(controller);

    startTowerEditAction.trigger();
    finishTowerEditAction.trigger();
    addTowerAction.trigger();
    insertTowerAction.trigger();
    moveTowerAction.trigger();
    editCurrentTowerAction.trigger();
    focusTowerAction.trigger();
    removeTowerAction.trigger();
    clearTowersAction.trigger();
    cancelTowerToolAction.trigger();
    importTowerFileAction.trigger();
    saveTowerFileAction.trigger();
    saveTowerFileAsAction.trigger();
    reloadTowerFileAction.trigger();

    if (!verify(startEditCount == 1 && finishEditCount == 1, "Tower controller should forward start/finish edit actions")) {
        return false;
    }
    if (!verify(addTowerCount == 1 && insertTowerCount == 1 && moveTowerCount == 1, "Tower controller should forward tower edit mode actions")) {
        return false;
    }
    if (!verify(editCurrentCount == 1 && focusTowerCount == 1 && removeTowerCount == 1, "Tower controller should forward current tower operations")) {
        return false;
    }
    if (!verify(clearTowersCount == 1 && cancelToolCount == 1, "Tower controller should forward clear/cancel actions")) {
        return false;
    }
    if (!verify(
            importTowerFileCount == 1
                && saveTowerFileCount == 1
                && saveTowerFileAsCount == 1
                && reloadTowerFileCount == 1,
            "Tower controller should forward tower file actions")) {
        return false;
    }

    showTowerXAction.setChecked(true);
    showTowerYAction.setChecked(true);
    showTowerZAction.setChecked(true);
    showTowerXAction.setChecked(false);
    showTowerYAction.setChecked(false);
    showTowerZAction.setChecked(false);
    if (!verify(towerTableWidget.isColumnHidden(2), "Tower controller should toggle X column visibility")) {
        return false;
    }
    if (!verify(towerTableWidget.isColumnHidden(3), "Tower controller should toggle Y column visibility")) {
        return false;
    }
    if (!verify(towerTableWidget.isColumnHidden(4), "Tower controller should toggle Z column visibility")) {
        return false;
    }
    if (!verify(showColumnToggleCount == 6, "Tower controller should invoke visibility callbacks for each toggle transition")) {
        return false;
    }

    towerTableWidget.setCurrentCell(1, 1);
    if (!verify(selectionChangedCount > 0 && latestSelectedRow == 1, "Tower controller should forward table selection changes")) {
        return false;
    }

    if (QTableWidgetItem* item = towerTableWidget.item(1, 1)) {
        item->setText(QStringLiteral("T-002-EDITED"));
    }
    if (!verify(
            towerNameEditedCount > 0 && latestEditedRow == 1 && latestEditedName == QStringLiteral("T-002-EDITED"),
            "Tower controller should forward tower name edits")) {
        return false;
    }

    const int commitBaseline = commitTowerDetailsCount;
    towerTypeComboBox.setCurrentIndex(1);
    towerNotesEdit.setPlainText(QStringLiteral("updated"));
    if (!verify(commitTowerDetailsCount >= commitBaseline + 2, "Tower controller should forward detail field edits")) {
        return false;
    }

    std::cout << "[PASS] Tower controller smoke test completed." << std::endl;
    return true;
}

bool runIssueControllerSmoke(const QStringList&)
{
    QAction startIssueMarkAction(QStringLiteral("Mark Issue"), nullptr);
    QAction cancelIssueToolAction(QStringLiteral("Cancel"), nullptr);
    QAction focusIssueAction(QStringLiteral("Focus"), nullptr);
    QAction removeIssueAction(QStringLiteral("Remove"), nullptr);
    QAction clearIssuesAction(QStringLiteral("Clear"), nullptr);
    QAction exportIssuesCsvAction(QStringLiteral("Export CSV"), nullptr);
    QAction exportInspectionReportAction(QStringLiteral("Export Report"), nullptr);

    QTableWidget issueTableWidget(2, 6);
    issueTableWidget.setItem(0, 1, new QTableWidgetItem(QStringLiteral("Issue 1")));
    issueTableWidget.setItem(1, 1, new QTableWidgetItem(QStringLiteral("Issue 2")));

    QLineEdit issueTitleEdit;
    QComboBox issueCategoryComboBox;
    issueCategoryComboBox.setEditable(true);
    issueCategoryComboBox.addItem(QStringLiteral("Other"));
    issueCategoryComboBox.addItem(QStringLiteral("Vegetation"));
    QComboBox issueSeverityComboBox;
    issueSeverityComboBox.addItem(QStringLiteral("Info"));
    issueSeverityComboBox.addItem(QStringLiteral("Major"));
    QComboBox issueStatusComboBox;
    issueStatusComboBox.addItem(QStringLiteral("Open"));
    issueStatusComboBox.addItem(QStringLiteral("Resolved"));
    QComboBox issueRelatedTowerComboBox;
    issueRelatedTowerComboBox.addItem(QStringLiteral("None"), -1);
    issueRelatedTowerComboBox.addItem(QStringLiteral("T-001"), 0);
    QLineEdit issueImagePathEdit;
    QPlainTextEdit issueDescriptionEdit;

    int beginIssueMarkingCount = 0;
    int cancelIssueToolCount = 0;
    int focusSelectedIssueCount = 0;
    int removeSelectedIssueCount = 0;
    int clearAllIssuesCount = 0;
    int exportIssuesCsvCount = 0;
    int exportInspectionReportCount = 0;
    int issueSelectionChangedCount = 0;
    int latestIssueSelection = -1;
    int commitIssueDetailsCount = 0;

    IssueController controller(
        &startIssueMarkAction,
        &cancelIssueToolAction,
        &focusIssueAction,
        &removeIssueAction,
        &clearIssuesAction,
        &exportIssuesCsvAction,
        &exportInspectionReportAction,
        &issueTableWidget,
        &issueTitleEdit,
        &issueCategoryComboBox,
        &issueSeverityComboBox,
        &issueStatusComboBox,
        &issueRelatedTowerComboBox,
        &issueImagePathEdit,
        &issueDescriptionEdit,
        [&beginIssueMarkingCount]() { ++beginIssueMarkingCount; },
        [&cancelIssueToolCount]() { ++cancelIssueToolCount; },
        [&focusSelectedIssueCount]() { ++focusSelectedIssueCount; },
        [&removeSelectedIssueCount]() { ++removeSelectedIssueCount; },
        [&clearAllIssuesCount]() { ++clearAllIssuesCount; },
        [&exportIssuesCsvCount]() { ++exportIssuesCsvCount; },
        [&exportInspectionReportCount]() { ++exportInspectionReportCount; },
        [&issueSelectionChangedCount, &latestIssueSelection](int row) {
            ++issueSelectionChangedCount;
            latestIssueSelection = row;
        },
        [&commitIssueDetailsCount]() { ++commitIssueDetailsCount; });
    Q_UNUSED(controller);

    startIssueMarkAction.trigger();
    cancelIssueToolAction.trigger();
    focusIssueAction.trigger();
    removeIssueAction.trigger();
    clearIssuesAction.trigger();
    exportIssuesCsvAction.trigger();
    exportInspectionReportAction.trigger();

    if (!verify(beginIssueMarkingCount == 1, "Issue controller should forward start issue marking action")) {
        return false;
    }
    if (!verify(cancelIssueToolCount == 1, "Issue controller should forward cancel issue tool action")) {
        return false;
    }
    if (!verify(focusSelectedIssueCount == 1, "Issue controller should forward focus issue action")) {
        return false;
    }
    if (!verify(removeSelectedIssueCount == 1 && clearAllIssuesCount == 1, "Issue controller should forward remove/clear issue actions")) {
        return false;
    }
    if (!verify(exportIssuesCsvCount == 1 && exportInspectionReportCount == 1, "Issue controller should forward issue export actions")) {
        return false;
    }

    issueTableWidget.setCurrentCell(1, 1);
    if (!verify(issueSelectionChangedCount > 0 && latestIssueSelection == 1, "Issue controller should forward issue table selection")) {
        return false;
    }

    const int commitBaseline = commitIssueDetailsCount;
    issueTitleEdit.setText(QStringLiteral("Updated Issue"));
    issueTitleEdit.editingFinished();
    issueCategoryComboBox.setEditText(QStringLiteral("Vegetation"));
    issueSeverityComboBox.setCurrentIndex(1);
    issueStatusComboBox.setCurrentIndex(1);
    issueRelatedTowerComboBox.setCurrentIndex(1);
    issueImagePathEdit.setText(QStringLiteral("images/issue.jpg"));
    issueImagePathEdit.editingFinished();
    issueDescriptionEdit.setPlainText(QStringLiteral("updated notes"));
    if (!verify(commitIssueDetailsCount >= commitBaseline + 7, "Issue controller should forward detail edits")) {
        return false;
    }

    std::cout << "[PASS] Issue controller smoke test completed." << std::endl;
    return true;
}

QList<PointRecord> buildSyntheticWaypoints()
{
    QList<PointRecord> waypoints;

    PointRecord first;
    first.x = 0.0f;
    first.y = 0.0f;
    first.z = 30.0f;
    waypoints.append(first);

    PointRecord second;
    second.x = 120.0f;
    second.y = 40.0f;
    second.z = 32.0f;
    waypoints.append(second);

    PointRecord third;
    third.x = 240.0f;
    third.y = 80.0f;
    third.z = 35.0f;
    waypoints.append(third);

    return waypoints;
}

bool runRouteRoamStateSmoke(const QStringList& filePaths)
{
    if (filePaths.isEmpty()) {
        std::cerr << "[FAIL] Route roam smoke requires at least one LAS/LAZ file." << std::endl;
        return false;
    }

    PointCloudViewer viewer;
    viewer.resize(1280, 800);
    viewer.show();
    pumpEvents(500);

    QString errorMessage;
    if (!viewer.loadPointCloud(filePaths.first(), &errorMessage)) {
        std::cerr << "[FAIL] loadPointCloud: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    pumpEvents(1000);

    int stateChangedCount = 0;
    int photoCapturedCount = 0;
    QObject::connect(&viewer, &PointCloudViewer::inspectionRouteRoamStateChanged, &viewer, [&stateChangedCount]() {
        ++stateChangedCount;
    });
    QObject::connect(
        &viewer,
        &PointCloudViewer::inspectionRouteRoamPhotoCaptured,
        &viewer,
        [&photoCapturedCount](int, int, const QString&, int) {
            ++photoCapturedCount;
        });

    viewer.setInspectionRouteWaypoints(buildSyntheticWaypoints());
    viewer.setInspectionRouteVisible(true);
    pumpEvents(200);

    if (!verify(!viewer.inspectionRouteRoamActive(), "Roam should be inactive before start")) {
        return false;
    }

    viewer.setInspectionRouteRoamSpeedMetersPerSecond(-5.0);
    if (!verifyClose(
            viewer.inspectionRouteRoamSpeedMetersPerSecond(),
            0.1,
            1e-6,
            "Roam speed should clamp to lower bound")) {
        return false;
    }

    viewer.setInspectionRouteRoamSpeedMetersPerSecond(500.0);
    if (!verifyClose(
            viewer.inspectionRouteRoamSpeedMetersPerSecond(),
            80.0,
            1e-6,
            "Roam speed should clamp to upper bound")) {
        return false;
    }

    viewer.setInspectionRouteRoamSpeedMetersPerSecond(6.0);
    viewer.setInspectionRouteRoamViewMode(RouteRoamViewMode::FirstPerson);
    if (!verify(
            viewer.inspectionRouteRoamViewMode() == RouteRoamViewMode::FirstPerson,
            "Roam view mode should switch to first-person")) {
        return false;
    }

    viewer.startInspectionRouteRoam(0);
    pumpEvents(250);
    if (!verify(viewer.inspectionRouteRoamActive(), "Roam should become active after start")) {
        return false;
    }
    if (!verify(viewer.inspectionRouteRoamPlaying(), "Roam should be playing after start")) {
        return false;
    }
    if (!verify(photoCapturedCount >= 1, "Roam should emit at least one photo capture signal during start")) {
        return false;
    }

    viewer.pauseInspectionRouteRoam();
    pumpEvents(120);
    if (!verify(viewer.inspectionRouteRoamPaused(), "Roam should be paused after pause")) {
        return false;
    }

    viewer.resumeInspectionRouteRoam();
    pumpEvents(120);
    if (!verify(viewer.inspectionRouteRoamPlaying(), "Roam should resume to playing state")) {
        return false;
    }

    viewer.stopInspectionRouteRoam(true);
    pumpEvents(120);
    if (!verify(!viewer.inspectionRouteRoamActive(), "Roam should stop after explicit stop")) {
        return false;
    }

    viewer.startInspectionRouteRoam(0);
    pumpEvents(120);
    if (!verify(viewer.inspectionRouteRoamActive(), "Roam should start again before visibility-stop check")) {
        return false;
    }
    viewer.setInspectionRouteVisible(false);
    pumpEvents(120);
    if (!verify(!viewer.inspectionRouteRoamActive(), "Roam should auto-stop when route is hidden")) {
        return false;
    }

    viewer.setInspectionRouteVisible(true);
    viewer.startInspectionRouteRoam(0);
    pumpEvents(120);
    if (!verify(viewer.inspectionRouteRoamActive(), "Roam should start before clear-waypoints stop check")) {
        return false;
    }
    viewer.clearInspectionRouteWaypoints();
    pumpEvents(120);
    if (!verify(!viewer.inspectionRouteRoamActive(), "Roam should auto-stop when waypoints are cleared")) {
        return false;
    }

    viewer.setInspectionRouteWaypoints(buildSyntheticWaypoints());
    viewer.setInspectionRouteVisible(true);
    viewer.startInspectionRouteRoam(0);
    pumpEvents(120);
    if (!verify(viewer.inspectionRouteRoamActive(), "Roam should start before clear-pointcloud stop check")) {
        return false;
    }
    viewer.clearPointCloud();
    pumpEvents(120);
    if (!verify(!viewer.inspectionRouteRoamActive(), "Roam should auto-stop when point cloud is cleared")) {
        return false;
    }

    if (!verify(stateChangedCount >= 8, "Roam state-changed signal count is unexpectedly low")) {
        return false;
    }

    std::cout << "StateChangedSignals=" << stateChangedCount
              << " PhotoCapturedSignals=" << photoCapturedCount << std::endl;
    std::cout << "[PASS] Route roam state smoke test completed." << std::endl;
    return true;
}

bool verifyRouteRoundTripShape(
    const PowerlineRouteDocument& original,
    const PowerlineRouteDocument& roundTripped)
{
    if (!verify(original.partPoints.size() == roundTripped.partPoints.size(), "Part point count mismatch after roundtrip")) {
        return false;
    }
    if (!verify(original.waypoints.size() == roundTripped.waypoints.size(), "Waypoint count mismatch after roundtrip")) {
        return false;
    }

    for (int index = 0; index < original.partPoints.size(); ++index) {
        const RoutePartPoint& left = original.partPoints.at(index);
        const RoutePartPoint& right = roundTripped.partPoints.at(index);
        if (!verify(left.partIndex == right.partIndex, "Part index mismatch after roundtrip")) {
            return false;
        }
        if (!verify(left.fileId == right.fileId, "Part file ID mismatch after roundtrip")) {
            return false;
        }
    }

    for (int index = 0; index < original.waypoints.size(); ++index) {
        const RouteWaypoint& left = original.waypoints.at(index);
        const RouteWaypoint& right = roundTripped.waypoints.at(index);
        if (!verify(left.primaryPartIndex == right.primaryPartIndex, "Primary part index mismatch after roundtrip")) {
            return false;
        }
        if (!verify(left.isHelperWaypoint == right.isHelperWaypoint, "Helper waypoint flag mismatch after roundtrip")) {
            return false;
        }
        if (!verify(left.captureTargets.size() == right.captureTargets.size(), "Capture target count mismatch after roundtrip")) {
            return false;
        }
    }

    return true;
}

PowerlineRouteDocument buildSyntheticRoute()
{
    PowerlineRouteDocument route;
    route.taskName = QStringLiteral("Synthetic Route");
    route.createdAt = QDateTime::currentDateTimeUtc();
    route.updatedAt = route.createdAt;

    RoutePartPoint leftInsulator;
    leftInsulator.partIndex = 1;
    leftInsulator.fileId = 101;
    leftInsulator.partName = QStringLiteral("Left Insulator");
    leftInsulator.longitude = 114.1001;
    leftInsulator.latitude = 30.2001;
    leftInsulator.dh = 55.0;
    leftInsulator.localPoint = PointRecord { 10.0f, 20.0f, 55.0f };
    route.partPoints.append(leftInsulator);

    RoutePartPoint rightInsulator;
    rightInsulator.partIndex = 2;
    rightInsulator.fileId = 102;
    rightInsulator.partName = QStringLiteral("Right Insulator");
    rightInsulator.longitude = 114.1002;
    rightInsulator.latitude = 30.2002;
    rightInsulator.dh = 55.5;
    rightInsulator.localPoint = PointRecord { 11.0f, 21.0f, 55.5f };
    route.partPoints.append(rightInsulator);

    RouteWaypoint captureWaypoint;
    captureWaypoint.sequenceIndex = 0;
    captureWaypoint.primaryPartIndex = 1;
    captureWaypoint.rawKeyId = 101;
    captureWaypoint.isHelperWaypoint = false;
    captureWaypoint.towerName = QStringLiteral("Tower 45");
    captureWaypoint.phaseSequence = QStringLiteral("ABC");
    captureWaypoint.isStart = true;
    captureWaypoint.turnMode = 0;
    captureWaypoint.waypointSpeed = 6.0;
    captureWaypoint.cornerRadiusMeters = 2.0;
    captureWaypoint.longitude = 114.10015;
    captureWaypoint.latitude = 30.20015;
    captureWaypoint.dh = 56.0;
    captureWaypoint.height = 56.0;
    captureWaypoint.aircraftYawDeg = 90.0;
    captureWaypoint.gimbalPitchDeg = -35.0;
    captureWaypoint.localPoint = PointRecord { 10.5f, 20.5f, 56.0f };

    RouteCaptureTarget leftTarget;
    leftTarget.partIndex = 1;
    leftTarget.partFileId = 101;
    leftTarget.partName = leftInsulator.partName;
    leftTarget.captureCount = 1;
    leftTarget.aircraftYawDeg = 90.0;
    leftTarget.gimbalPitchDeg = -35.0;
    leftTarget.cameraPitchDeg = 110.0;
    leftTarget.targetLocalPoint = leftInsulator.localPoint;
    captureWaypoint.captureTargets.append(leftTarget);

    RouteCaptureTarget rightTarget;
    rightTarget.partIndex = 2;
    rightTarget.partFileId = 102;
    rightTarget.partName = rightInsulator.partName;
    rightTarget.captureCount = 1;
    rightTarget.aircraftYawDeg = 90.0;
    rightTarget.gimbalPitchDeg = -40.0;
    rightTarget.cameraPitchDeg = -20.0;
    rightTarget.targetLocalPoint = rightInsulator.localPoint;
    captureWaypoint.captureTargets.append(rightTarget);

    route.waypoints.append(captureWaypoint);

    RouteWaypoint helperWaypoint;
    helperWaypoint.sequenceIndex = 1;
    helperWaypoint.primaryPartIndex = -1;
    helperWaypoint.rawKeyId = -7;
    helperWaypoint.isHelperWaypoint = true;
    helperWaypoint.turnMode = 1;
    helperWaypoint.waypointSpeed = 5.0;
    helperWaypoint.longitude = 114.1003;
    helperWaypoint.latitude = 30.2003;
    helperWaypoint.dh = 57.0;
    helperWaypoint.height = 57.0;
    helperWaypoint.localPoint = PointRecord { 12.0f, 22.0f, 57.0f };
    helperWaypoint.rotationCenter = PointRecord { 11.5f, 21.5f, 56.5f };
    route.waypoints.append(helperWaypoint);

    return route;
}

bool runRouteJsonSmoke(const QStringList&)
{
    const QString templatePath = QDir::current().absoluteFilePath(QStringLiteral("templates/N#045.json"));
    PowerlineRouteDocument importedRoute;
    QString errorMessage;
    if (!importPowerlineRouteJson(templatePath, &importedRoute, &errorMessage)) {
        std::cerr << "[FAIL] importPowerlineRouteJson(template): " << errorMessage.toStdString() << std::endl;
        return false;
    }

    if (!verify(importedRoute.partPoints.size() == 25, "Template should contain 25 part points")) {
        return false;
    }
    if (!verify(importedRoute.waypoints.size() == 37, "Template should contain 37 waypoints")) {
        return false;
    }
    if (!verify(toRouteDisplayPoints(importedRoute).size() == importedRoute.waypoints.size(), "Display point count mismatch")) {
        return false;
    }
    if (!verify(toRouteDisplayLabels(importedRoute).size() == importedRoute.waypoints.size(), "Display label count mismatch")) {
        return false;
    }

    for (const RouteWaypoint& waypoint : importedRoute.waypoints) {
        if (waypoint.rawKeyId > 0 && !verify(waypoint.primaryPartIndex > 0, "Positive keyID should map to partIndex")) {
            return false;
        }
        for (const RouteCaptureTarget& captureTarget : waypoint.captureTargets) {
            if (captureTarget.partFileId > 0
                && !verify(captureTarget.partIndex > 0, "Capture target should resolve to partIndex")) {
                return false;
            }
        }
    }

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString roundTripPath = QDir(tempDir.path()).filePath(QStringLiteral("roundtrip_route.json"));
    if (!exportPowerlineRouteJson(roundTripPath, importedRoute, &errorMessage)) {
        std::cerr << "[FAIL] exportPowerlineRouteJson(template): " << errorMessage.toStdString() << std::endl;
        return false;
    }

    PowerlineRouteDocument roundTrippedRoute;
    if (!importPowerlineRouteJson(roundTripPath, &roundTrippedRoute, &errorMessage)) {
        std::cerr << "[FAIL] importPowerlineRouteJson(roundtrip): " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verifyRouteRoundTripShape(importedRoute, roundTrippedRoute)) {
        return false;
    }

    const PowerlineRouteDocument syntheticRoute = buildSyntheticRoute();
    const QString syntheticPath = QDir(tempDir.path()).filePath(QStringLiteral("synthetic_route.json"));
    if (!exportPowerlineRouteJson(syntheticPath, syntheticRoute, &errorMessage)) {
        std::cerr << "[FAIL] exportPowerlineRouteJson(synthetic): " << errorMessage.toStdString() << std::endl;
        return false;
    }

    PowerlineRouteDocument importedSyntheticRoute;
    if (!importPowerlineRouteJson(syntheticPath, &importedSyntheticRoute, &errorMessage)) {
        std::cerr << "[FAIL] importPowerlineRouteJson(synthetic): " << errorMessage.toStdString() << std::endl;
        return false;
    }

    if (!verify(importedSyntheticRoute.waypoints.size() == 2, "Synthetic route should contain 2 waypoints")) {
        return false;
    }
    if (!verify(importedSyntheticRoute.waypoints.first().captureTargets.size() == 2, "Synthetic capture waypoint should keep 2 targets")) {
        return false;
    }
    if (!verify(importedSyntheticRoute.waypoints.last().isHelperWaypoint, "Synthetic helper waypoint flag should persist")) {
        return false;
    }
    if (!verify(importedSyntheticRoute.waypoints.last().rawKeyId < 0, "Synthetic helper waypoint should keep negative keyID")) {
        return false;
    }
    if (!verify(importedSyntheticRoute.waypoints.last().rotationCenter.has_value(), "Synthetic helper waypoint rotation center should persist")) {
        return false;
    }

    std::cout << "[PASS] Route JSON smoke test completed." << std::endl;
    return true;
}

bool runRouteInteropSmoke(const QStringList&)
{
    const lasviewer::crs::CrsResolveResult authorityResult =
        lasviewer::crs::CrsAuthorityService::resolveFromAuthority(QStringLiteral("EPSG"), 4326);
    if (!verify(authorityResult.ok, "EPSG:4326 should resolve from authority database")) {
        return false;
    }
    if (!verify(!authorityResult.definition.reference.wkt.trimmed().isEmpty(), "Resolved EPSG:4326 should provide WKT")) {
        return false;
    }

    const lasviewer::crs::CrsResolveResult wktResult =
        lasviewer::crs::CrsAuthorityService::resolveFromWkt(authorityResult.definition.reference.wkt);
    if (!verify(wktResult.ok, "WKT should resolve back to a CRS definition")) {
        return false;
    }
    if (!verify(
            wktResult.definition.reference.authName.compare(QStringLiteral("EPSG"), Qt::CaseInsensitive) == 0
                && wktResult.definition.reference.code == 4326,
            "WKT should identify back to EPSG:4326")) {
        return false;
    }

    const QList<lasviewer::crs::CoordinateSystemDefinition> nameMatches =
        lasviewer::crs::CrsAuthorityService::findByName(
            QStringLiteral("WGS 84"),
            lasviewer::crs::CoordinateSystemKindFilter::Geographic,
            5);
    if (!verify(!nameMatches.isEmpty(), "Name lookup for WGS 84 should return candidates")) {
        return false;
    }

    QList<VegetationRiskRecord> risks;
    for (int index = 0; index < 4; ++index) {
        VegetationRiskRecord risk;
        risk.id = QStringLiteral("risk_%1").arg(index + 1);
        risk.title = QStringLiteral("Risk %1").arg(index + 1);
        risk.point.x = static_cast<float>(120.0 + index * 60.0);
        risk.point.y = static_cast<float>(240.0 + index * 25.0);
        risk.point.z = static_cast<float>(30.0 + index * 2.0);
        risk.representativeChainage = static_cast<float>(index * 65.0);
        risks.append(risk);
    }

    QList<TowerRecord> towers;
    TowerRecord towerA;
    towerA.name = QStringLiteral("Tower A");
    towerA.point.x = 140.0f;
    towerA.point.y = 245.0f;
    towerA.point.z = 35.0f;
    towers.append(towerA);

    RouteGenerationOptions generationOptions;
    generationOptions.waypointSpacingMeters = 25.0f;
    generationOptions.smoothingStrengthPercent = 20.0f;

    RouteSafetyOptions safetyOptions;
    safetyOptions.heightOffsetMeters = 18.0f;
    safetyOptions.defaultWaypointSpeedMps = 6.0f;

    const InspectionRoute localRoute = generateInspectionRouteFromRisks(
        risks, towers, generationOptions, safetyOptions);
    if (!verify(localRoute.waypoints.size() >= 3, "Route generation should produce at least 3 waypoints")) {
        return false;
    }

    const PowerlineRouteDocument routeDocument =
        createPowerlineRouteFromInspectionRoute(localRoute, QStringLiteral("Smoke Route"));
    if (!verify(routeDocument.waypoints.size() == localRoute.waypoints.size(), "Bridge document size mismatch")) {
        return false;
    }

    const InspectionRoute bridgedLocalRoute = toInspectionRouteExportView(routeDocument);
    if (!verify(bridgedLocalRoute.waypoints.size() == localRoute.waypoints.size(), "Bridge export size mismatch")) {
        return false;
    }
    if (!verify(
            !bridgedLocalRoute.waypoints.isEmpty()
                && bridgedLocalRoute.waypoints.first().localPoint.x == localRoute.waypoints.first().localPoint.x
                && bridgedLocalRoute.waypoints.first().localPoint.y == localRoute.waypoints.first().localPoint.y,
            "Bridge export should preserve waypoint coordinates")) {
        return false;
    }

    RoutePlanningOptions planningOptions;
    planningOptions.generation = generationOptions;
    planningOptions.safety = safetyOptions;
    planningOptions.crs.sourceEpsg = 4326;
    planningOptions.crs.targetEpsg = 4326;
    planningOptions.aircraftProfile = DjiAircraftProfile::M30Series;

    ProjectCoordinateSystems coordinateSystems;
    coordinateSystems.pointCloudCrs.authName = QStringLiteral("EPSG");
    coordinateSystems.pointCloudCrs.code = 4326;
    coordinateSystems.pointCloudCrs.displayName = QStringLiteral("WGS 84");
    coordinateSystems.pointCloudCrs.kind = CoordinateSystemKind::Projected;
    coordinateSystems.geographicCrs = defaultGeographicCoordinateSystem();

    InspectionRoute routeWgs84;
    QString errorMessage;
    if (!transformRouteToWgs84(bridgedLocalRoute, coordinateSystems, &routeWgs84, &errorMessage)) {
        std::cerr << "[FAIL] transformRouteToWgs84: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(routeWgs84.waypoints.size() == bridgedLocalRoute.waypoints.size(), "Transformed route size mismatch")) {
        return false;
    }

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString kmlPath = QDir(tempDir.path()).filePath(QStringLiteral("route_test.kml"));
    if (!exportRouteKml(kmlPath, routeWgs84, &errorMessage)) {
        std::cerr << "[FAIL] exportRouteKml: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(QFile::exists(kmlPath), "KML file should exist")) {
        return false;
    }

    InspectionRoute importedRouteWgs84;
    if (!importRouteKml(kmlPath, &importedRouteWgs84, &errorMessage)) {
        std::cerr << "[FAIL] importRouteKml: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(
            importedRouteWgs84.waypoints.size() == routeWgs84.waypoints.size(),
            "KML roundtrip waypoint size mismatch")) {
        return false;
    }

    const QString kmzPath = QDir(tempDir.path()).filePath(QStringLiteral("route_test.kmz"));
    if (!exportRouteDjiKmz(kmzPath, routeWgs84, planningOptions, &errorMessage)) {
        std::cerr << "[FAIL] exportRouteDjiKmz: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(QFile::exists(kmzPath), "KMZ file should exist")) {
        return false;
    }

    QFile kmzFile(kmzPath);
    if (!verify(kmzFile.open(QIODevice::ReadOnly), "KMZ file should be readable")) {
        return false;
    }
    const QByteArray kmzData = kmzFile.readAll();
    kmzFile.close();
    if (!verify(kmzData.contains("wpmz/template.kml"), "KMZ should contain template.kml entry")) {
        return false;
    }
    if (!verify(kmzData.contains("wpmz/waylines.wpml"), "KMZ should contain waylines.wpml entry")) {
        return false;
    }

    std::cout << "[PASS] Route interop smoke test completed." << std::endl;
    return true;
}

void normalizeTowerIndices(QList<TowerRecord>* towers)
{
    if (towers == nullptr) {
        return;
    }
    for (int index = 0; index < towers->size(); ++index) {
        (*towers)[index].index = index;
    }
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

bool runTowerFileInteropSmoke(const QStringList&)
{
    QList<TowerRecord> expectedTowers;
    TowerRecord tower0;
    tower0.index = 0;
    tower0.name = QStringLiteral("#001");
    tower0.point.x = 100.5f;
    tower0.point.y = 200.25f;
    tower0.point.z = 300.125f;
    tower0.towerType = TowerType::Unknown;
    expectedTowers.append(tower0);

    TowerRecord tower1;
    tower1.index = 1;
    tower1.name = QStringLiteral("#002");
    tower1.point.x = 110.5f;
    tower1.point.y = 210.25f;
    tower1.point.z = 310.125f;
    tower1.towerType = TowerType::Tangent;
    expectedTowers.append(tower1);

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString towerPath = QDir(tempDir.path()).filePath(QStringLiteral("tower.LiTower"));
    QString errorMessage;
    if (!exportTowerLiTowerFile(towerPath, expectedTowers, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(QFile::exists(towerPath), "Exported tower file should exist")) {
        return false;
    }

    QFile file(towerPath);
    if (!verify(file.open(QIODevice::ReadOnly | QIODevice::Text), "Exported tower file should be readable")) {
        return false;
    }
    const QString firstLine = QString::fromUtf8(file.readLine()).trimmed();
    file.close();
    if (!verify(firstLine == QStringLiteral("Index,X,Y,Z,Type,Name"), "Tower file header should match LiTower format")) {
        return false;
    }

    QList<TowerRecord> importedTowers;
    if (!importTowerLiTowerFile(towerPath, &importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] importTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return false;
    }

    if (!verify(importedTowers.size() == expectedTowers.size(), "Imported tower count mismatch")) {
        return false;
    }

    for (int index = 0; index < expectedTowers.size(); ++index) {
        const TowerRecord& expected = expectedTowers.at(index);
        const TowerRecord& actual = importedTowers.at(index);
        if (!verify(actual.index == expected.index, "Tower index mismatch")) {
            return false;
        }
        if (!verify(actual.name == expected.name, "Tower name mismatch")) {
            return false;
        }
        if (!verify(actual.towerType == expected.towerType, "Tower type mismatch")) {
            return false;
        }
        if (!verify(std::fabs(actual.point.x - expected.point.x) < 1e-4f, "Tower X mismatch")) {
            return false;
        }
        if (!verify(std::fabs(actual.point.y - expected.point.y) < 1e-4f, "Tower Y mismatch")) {
            return false;
        }
        if (!verify(std::fabs(actual.point.z - expected.point.z) < 1e-4f, "Tower Z mismatch")) {
            return false;
        }
    }

    std::cout << "[PASS] Tower file interop smoke test completed." << std::endl;
    return true;
}

bool runTowerProjectLinkSmoke(const QStringList&)
{
    const QString sourceTowerFilePath = QFileInfo(
        QDir::current().absoluteFilePath(QStringLiteral("templates/tower.LiTower"))).absoluteFilePath();
    if (!verify(QFileInfo::exists(sourceTowerFilePath), "templates/tower.LiTower should exist")) {
        return false;
    }

    QList<TowerRecord> importedTowers;
    QString errorMessage;
    if (!importTowerLiTowerFile(sourceTowerFilePath, &importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] importTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(importedTowers.size() >= 4, "Expected at least 4 towers from template")) {
        return false;
    }
    if (!verify(importedTowers.first().index == 44, "Template first index should be 44 before editing")) {
        return false;
    }

    normalizeTowerIndices(&importedTowers);
    if (!verify(importedTowers.first().index == 0, "After edit normalization, first index should be 0")) {
        return false;
    }
    if (!verify(importedTowers.last().index == importedTowers.size() - 1, "After edit normalization, last index should be N-1")) {
        return false;
    }

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString projectFilePath = QDir(tempDir.path()).filePath(QStringLiteral("tower_project.lpproj"));
    const QString linkedTowerFilePath = QDir(tempDir.path()).filePath(QStringLiteral("tower_linked.LiTower"));
    if (!exportTowerLiTowerFile(linkedTowerFilePath, importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile initial: " << errorMessage.toStdString() << std::endl;
        return false;
    }

    QJsonArray towersArray;
    for (const TowerRecord& towerRecord : importedTowers) {
        towersArray.append(towerRecordToJson(towerRecord));
    }

    QJsonArray pointCloudFilesArray;
    pointCloudFilesArray.append(QStringLiteral("./test_data/ezhou_powerline_sample.las"));
    QJsonObject towerFileObject {
        { QStringLiteral("format"), QStringLiteral("LiTower") },
        { QStringLiteral("relativePath"), QStringLiteral("./tower_linked.LiTower") }
    };

    QJsonObject projectObject {
        { QStringLiteral("version"), 8 },
        { QStringLiteral("pointCloudFilePaths"), pointCloudFilesArray },
        { QStringLiteral("towerFile"), towerFileObject },
        { QStringLiteral("towerMarkers"), towersArray }
    };

    QFile projectFile(projectFilePath);
    if (!verify(projectFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Project file should be writable")) {
        return false;
    }
    projectFile.write(QJsonDocument(projectObject).toJson(QJsonDocument::Indented));
    projectFile.close();

    QFile projectFileRead(projectFilePath);
    if (!verify(projectFileRead.open(QIODevice::ReadOnly), "Project file should be readable")) {
        return false;
    }
    const QJsonDocument loadedDocument = QJsonDocument::fromJson(projectFileRead.readAll());
    projectFileRead.close();
    if (!verify(loadedDocument.isObject(), "Loaded project JSON must be an object")) {
        return false;
    }

    const QJsonObject loadedProject = loadedDocument.object();
    const QString loadedRelativeTowerPath = loadedProject.value(QStringLiteral("towerFile")).toObject().value(QStringLiteral("relativePath")).toString();
    const QString resolvedTowerPath = resolveProjectPath(projectFilePath, loadedRelativeTowerPath);
    if (!verify(QFileInfo::exists(resolvedTowerPath), "Resolved linked tower file should exist")) {
        return false;
    }

    QList<TowerRecord> loadedTowerRecords;
    const QJsonArray loadedTowersArray = loadedProject.value(QStringLiteral("towerMarkers")).toArray();
    for (const QJsonValue& towerValue : loadedTowersArray) {
        loadedTowerRecords.append(towerRecordFromJson(towerValue.toObject()));
    }
    if (!verify(loadedTowerRecords.size() == importedTowers.size(), "Loaded tower record count mismatch")) {
        return false;
    }
    if (!verify(loadedTowerRecords.first().index == 0, "Loaded project first index should be 0")) {
        return false;
    }

    if (!exportTowerLiTowerFile(resolvedTowerPath, loadedTowerRecords, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile sync: " << errorMessage.toStdString() << std::endl;
        return false;
    }

    QFile linkedTowerFile(resolvedTowerPath);
    if (!verify(linkedTowerFile.open(QIODevice::ReadOnly | QIODevice::Text), "Linked tower file should be readable")) {
        return false;
    }
    QTextStream stream(&linkedTowerFile);
    stream.setCodec("UTF-8");
    const QString header = stream.readLine().trimmed();
    const QString firstRow = stream.readLine().trimmed();
    linkedTowerFile.close();

    if (!verify(header == QStringLiteral("Index,X,Y,Z,Type,Name"), "Linked tower header should match LiTower format")) {
        return false;
    }
    if (!verify(firstRow.startsWith(QStringLiteral("0,")), "First linked tower row should start with index 0")) {
        return false;
    }

    std::cout << "[PASS] Tower project link smoke test completed." << std::endl;
    return true;
}

QSet<QString> parseCsvValues(const QStringList& rawValues)
{
    QSet<QString> values;
    for (const QString& raw : rawValues) {
        const QStringList split = raw.split(',', Qt::SkipEmptyParts);
        for (const QString& item : split) {
            const QString normalized = item.trimmed().toLower();
            if (!normalized.isEmpty()) {
                values.insert(normalized);
            }
        }
    }
    return values;
}

void printUsageSummary()
{
    std::cout
        << "Modes: viewer-render, main-backstage, main-settings-restore, log-panel, project-explorer-dock, project-explorer-controller, project-explorer-mainwindow, visualization-panel-controller, measurement-analysis-controller, profile-classification-widget, profile-classification-controller, route-controller, tower-controller, issue-controller, route-json, route-interop, route-roam, tower-file, tower-project-link, all" << std::endl
        << "Categories: render, ui, route, tower, all" << std::endl
        << "Examples:" << std::endl
        << "  LASViewerSmokeTest --mode main-backstage" << std::endl
        << "  LASViewerSmokeTest --mode main-settings-restore" << std::endl
        << "  LASViewerSmokeTest --mode visualization-panel-controller" << std::endl
        << "  LASViewerSmokeTest --mode measurement-analysis-controller" << std::endl
        << "  LASViewerSmokeTest --mode profile-classification-widget" << std::endl
        << "  LASViewerSmokeTest --mode profile-classification-controller" << std::endl
        << "  LASViewerSmokeTest --mode route-controller" << std::endl
        << "  LASViewerSmokeTest --mode tower-controller" << std::endl
        << "  LASViewerSmokeTest --mode issue-controller" << std::endl
        << "  LASViewerSmokeTest --mode route-roam --las .\\test_data\\ezhou_powerline_sample.las" << std::endl
        << "  LASViewerSmokeTest --category route --las .\\test_data\\ezhou_powerline_sample.las" << std::endl
        << "  LASViewerSmokeTest --mode all --las .\\test_data\\ezhou_powerline_sample.las" << std::endl;
}

QStringList resolveLasInputs(const QCommandLineParser& parser)
{
    QStringList lasFiles = parser.values(QStringLiteral("las"));
    const QStringList positional = parser.positionalArguments();
    for (const QString& argument : positional) {
        if (!argument.startsWith('-')) {
            lasFiles.append(argument);
        }
    }

    if (lasFiles.isEmpty()) {
        lasFiles.append(QStringLiteral("./test_data/ezhou_powerline_sample.las"));
    }

    for (QString& lasFile : lasFiles) {
        lasFile = QDir::fromNativeSeparators(lasFile.trimmed());
    }

    return lasFiles;
}

bool validateSelections(const QSet<QString>& modeSet, const QSet<QString>& categorySet)
{
    const QSet<QString> validModes {
        QStringLiteral("viewer-render"),
        QStringLiteral("main-backstage"),
        QStringLiteral("main-settings-restore"),
        QStringLiteral("log-panel"),
        QStringLiteral("project-explorer-dock"),
        QStringLiteral("project-explorer-controller"),
        QStringLiteral("project-explorer-mainwindow"),
        QStringLiteral("visualization-panel-controller"),
        QStringLiteral("measurement-analysis-controller"),
        QStringLiteral("profile-classification-widget"),
        QStringLiteral("profile-classification-controller"),
        QStringLiteral("route-controller"),
        QStringLiteral("tower-controller"),
        QStringLiteral("issue-controller"),
        QStringLiteral("route-json"),
        QStringLiteral("route-interop"),
        QStringLiteral("route-roam"),
        QStringLiteral("tower-file"),
        QStringLiteral("tower-project-link"),
        QStringLiteral("all")
    };
    const QSet<QString> validCategories {
        QStringLiteral("render"),
        QStringLiteral("ui"),
        QStringLiteral("route"),
        QStringLiteral("tower"),
        QStringLiteral("all")
    };

    for (const QString& mode : modeSet) {
        if (!validModes.contains(mode)) {
            std::cerr << "Invalid mode: " << mode.toStdString() << std::endl;
            return false;
        }
    }
    for (const QString& category : categorySet) {
        if (!validCategories.contains(category)) {
            std::cerr << "Invalid category: " << category.toStdString() << std::endl;
            return false;
        }
    }

    if ((modeSet.size() > 1 && modeSet.contains(QStringLiteral("all")))
        || (categorySet.size() > 1 && categorySet.contains(QStringLiteral("all")))) {
        std::cerr << "Invalid argument: all cannot be combined with other values." << std::endl;
        return false;
    }

    return true;
}

bool runSelectedSmokes(
    const QList<SmokeCase>& cases,
    const QSet<QString>& modeSet,
    const QSet<QString>& categorySet,
    const QStringList& lasFiles)
{
    QList<SmokeCase> selectedCases;
    const bool selectAllByDefault = modeSet.isEmpty() && categorySet.isEmpty();

    for (const SmokeCase& smokeCase : cases) {
        const bool modeMatched = modeSet.contains(QStringLiteral("all")) || modeSet.contains(smokeCase.mode);
        const bool categoryMatched = categorySet.contains(QStringLiteral("all")) || categorySet.contains(smokeCase.category);
        if (selectAllByDefault || modeMatched || categoryMatched) {
            selectedCases.append(smokeCase);
        }
    }

    if (selectedCases.isEmpty()) {
        std::cerr << "No smoke test selected. Please provide valid --mode or --category." << std::endl;
        printUsageSummary();
        return false;
    }

    bool allPassed = true;
    int passCount = 0;
    int failCount = 0;

    for (const SmokeCase& smokeCase : selectedCases) {
        std::cout << "[RUN] " << smokeCase.displayName.toStdString()
                  << " (mode=" << smokeCase.mode.toStdString()
                  << ", category=" << smokeCase.category.toStdString() << ")" << std::endl;

        if (smokeCase.requiresLas) {
            for (const QString& lasFile : lasFiles) {
                if (!QFileInfo::exists(lasFile)) {
                    std::cerr << "[FAIL] Required LAS/LAZ file not found: " << lasFile.toStdString() << std::endl;
                    return false;
                }
            }
        }

        const bool casePassed = smokeCase.run(lasFiles);
        if (casePassed) {
            ++passCount;
            std::cout << "[PASS] " << smokeCase.displayName.toStdString() << std::endl;
        } else {
            ++failCount;
            allPassed = false;
            std::cout << "[FAIL] " << smokeCase.displayName.toStdString() << std::endl;
        }
    }

    std::cout << "Smoke summary: selected=" << selectedCases.size()
              << " passed=" << passCount
              << " failed=" << failCount << std::endl;
    return allPassed;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setVersion(2, 1);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    QCoreApplication::setApplicationName(QStringLiteral("LASViewerSmokeTest"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Unified smoke test runner for LAS Point Cloud Viewer"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(
        QStringList() << QStringLiteral("m") << QStringLiteral("mode"),
        QStringLiteral("Run by mode. Supports comma-separated values."),
        QStringLiteral("mode")));
    parser.addOption(QCommandLineOption(
        QStringList() << QStringLiteral("c") << QStringLiteral("category"),
        QStringLiteral("Run by category. Supports comma-separated values."),
        QStringLiteral("category")));
    parser.addOption(QCommandLineOption(
        QStringList() << QStringLiteral("l") << QStringLiteral("las"),
        QStringLiteral("LAS/LAZ input path. Repeatable for multiple files."),
        QStringLiteral("path")));
    parser.addPositionalArgument(
        QStringLiteral("las_files"),
        QStringLiteral("Optional LAS/LAZ file list used by rendering and route-roam modes."));
    parser.process(app);

    const QSet<QString> modeSet = parseCsvValues(parser.values(QStringLiteral("mode")));
    const QSet<QString> categorySet = parseCsvValues(parser.values(QStringLiteral("category")));
    if (!validateSelections(modeSet, categorySet)) {
        printUsageSummary();
        return 2;
    }

    const QStringList lasFiles = resolveLasInputs(parser);

    const QList<SmokeCase> smokeCases {
        SmokeCase {
            QStringLiteral("viewer-render"),
            QStringLiteral("render"),
            QStringLiteral("Viewer Render Smoke"),
            true,
            runViewerRenderSmoke },
        SmokeCase {
            QStringLiteral("main-backstage"),
            QStringLiteral("ui"),
            QStringLiteral("Main Backstage Smoke"),
            false,
            runMainBackstageSmoke },
        SmokeCase {
            QStringLiteral("main-settings-restore"),
            QStringLiteral("ui"),
            QStringLiteral("Main Window Settings Restore Smoke"),
            false,
            runMainWindowSettingsRestoreSmoke },
        SmokeCase {
            QStringLiteral("log-panel"),
            QStringLiteral("ui"),
            QStringLiteral("Log Panel Smoke"),
            false,
            runLogPanelSmoke },
        SmokeCase {
            QStringLiteral("project-explorer-dock"),
            QStringLiteral("ui"),
            QStringLiteral("Project Explorer Dock Smoke"),
            false,
            runProjectExplorerDockSmoke },
        SmokeCase {
            QStringLiteral("project-explorer-controller"),
            QStringLiteral("ui"),
            QStringLiteral("Project Explorer Controller Smoke"),
            false,
            runProjectExplorerControllerSmoke },
        SmokeCase {
            QStringLiteral("project-explorer-mainwindow"),
            QStringLiteral("ui"),
            QStringLiteral("Project Explorer MainWindow Smoke"),
            true,
            runProjectExplorerMainWindowSmoke },
        SmokeCase {
            QStringLiteral("visualization-panel-controller"),
            QStringLiteral("ui"),
            QStringLiteral("Visualization Panel Controller Smoke"),
            false,
            runVisualizationPanelControllerSmoke },
        SmokeCase {
            QStringLiteral("measurement-analysis-controller"),
            QStringLiteral("ui"),
            QStringLiteral("Measurement Analysis Controller Smoke"),
            true,
            runMeasurementAnalysisControllerSmoke },
        SmokeCase {
            QStringLiteral("profile-classification-widget"),
            QStringLiteral("ui"),
            QStringLiteral("Profile Classification Widget Smoke"),
            false,
            runProfileClassificationWidgetSmoke },
        SmokeCase {
            QStringLiteral("profile-classification-controller"),
            QStringLiteral("ui"),
            QStringLiteral("Profile Classification Controller Smoke"),
            true,
            runProfileClassificationControllerSmoke },
        SmokeCase {
            QStringLiteral("route-controller"),
            QStringLiteral("ui"),
            QStringLiteral("Route Controller Smoke"),
            false,
            runRouteControllerSmoke },
        SmokeCase {
            QStringLiteral("tower-controller"),
            QStringLiteral("ui"),
            QStringLiteral("Tower Controller Smoke"),
            false,
            runTowerControllerSmoke },
        SmokeCase {
            QStringLiteral("issue-controller"),
            QStringLiteral("ui"),
            QStringLiteral("Issue Controller Smoke"),
            false,
            runIssueControllerSmoke },
        SmokeCase {
            QStringLiteral("route-json"),
            QStringLiteral("route"),
            QStringLiteral("Route Json Smoke"),
            false,
            runRouteJsonSmoke },
        SmokeCase {
            QStringLiteral("route-interop"),
            QStringLiteral("route"),
            QStringLiteral("Route Interop Smoke"),
            false,
            runRouteInteropSmoke },
        SmokeCase {
            QStringLiteral("route-roam"),
            QStringLiteral("route"),
            QStringLiteral("Route Roam State Smoke"),
            true,
            runRouteRoamStateSmoke },
        SmokeCase {
            QStringLiteral("tower-file"),
            QStringLiteral("tower"),
            QStringLiteral("Tower File Interop Smoke"),
            false,
            runTowerFileInteropSmoke },
        SmokeCase {
            QStringLiteral("tower-project-link"),
            QStringLiteral("tower"),
            QStringLiteral("Tower Project Link Smoke"),
            false,
            runTowerProjectLinkSmoke }
    };

    const bool allPassed = runSelectedSmokes(smokeCases, modeSet, categorySet, lasFiles);
    return allPassed ? 0 : 1;
}
