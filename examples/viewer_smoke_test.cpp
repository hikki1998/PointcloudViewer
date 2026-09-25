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

#include <osgGA/TrackballManipulator>

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

#include "SmokeTestSupport.h"

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
        << "Modes: viewer-render, main-backstage, main-settings-restore, screen-recording, log-panel, project-explorer-dock, project-explorer-controller, project-explorer-mainwindow, visualization-panel-controller, measurement-analysis-controller, profile-classification-widget, profile-classification-controller, route-controller, tower-controller, issue-controller, route-json, route-interop, route-roam, tower-file, tower-project-link, all" << std::endl
        << "Categories: render, ui, route, tower, all" << std::endl
        << "Examples:" << std::endl
        << "  LASViewerSmokeTest --mode main-backstage" << std::endl
        << "  LASViewerSmokeTest --mode main-settings-restore" << std::endl
        << "  LASViewerSmokeTest --mode screen-recording --las .\\test_data\\ezhou_powerline_sample.las" << std::endl
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
        QStringLiteral("screen-recording"),
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

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setVersion(4, 3);
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
            QStringLiteral("screen-recording"),
            QStringLiteral("ui"),
            QStringLiteral("Screen Recording Smoke"),
            false,
            runScreenRecordingSmoke },
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
