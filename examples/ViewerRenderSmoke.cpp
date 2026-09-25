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
        const bool gaussianPly = QFileInfo(filePath).suffix().compare(QStringLiteral("ply"), Qt::CaseInsensitive) == 0;
        QElapsedTimer loadStartTimer;
        loadStartTimer.start();
        const bool loadStarted = gaussianPly
            ? viewer.loadPointCloudFilesAsync(QStringList { filePath }, &errorMessage)
            : viewer.loadPointCloud(filePath, &errorMessage);
        const qint64 loadCallElapsedMs = loadStartTimer.elapsed();
        if (!loadStarted) {
            std::cerr << "Load failed for " << filePath.toStdString() << ": "
                      << errorMessage.toStdString() << std::endl;
            allPassed = false;
            continue;
        }
        if (gaussianPly) {
            if (loadCallElapsedMs > 500) {
                std::cerr << "Gaussian background load call blocked the UI thread for "
                          << loadCallElapsedMs << " ms" << std::endl;
                allPassed = false;
                continue;
            }
            QElapsedTimer asyncWaitTimer;
            asyncWaitTimer.start();
            while (viewer.isPointCloudLoadingInProgress() && asyncWaitTimer.elapsed() < 15000) {
                pumpEvents(25);
            }
            if (viewer.isPointCloudLoadingInProgress() || !viewer.hasGaussianModel()) {
                std::cerr << "Gaussian background load did not finish within 15 seconds" << std::endl;
                allPassed = false;
                continue;
            }
            std::cout << "Gaussian async load call=" << loadCallElapsedMs
                      << "ms ready=" << asyncWaitTimer.elapsed() << "ms" << std::endl;
        }

        bool screenshotDelayOk = false;
        const int screenshotDelayMs = qEnvironmentVariableIntValue("LAS_VIEWER_SMOKE_SCREENSHOT_DELAY_MS", &screenshotDelayOk);
        pumpEvents(screenshotDelayOk ? std::max(1000, screenshotDelayMs) : 1000);

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

        const QString screenshotPath = qEnvironmentVariable("LAS_VIEWER_SMOKE_SCREENSHOT").trimmed();
        if (!screenshotPath.isEmpty() && !frame.save(screenshotPath)) {
            std::cerr << "Failed to save viewer screenshot to " << screenshotPath.toStdString() << std::endl;
            allPassed = false;
        }

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

        if (gaussianPly) {
            osgViewer::Viewer* gaussianViewer = osgWidget->getViewer();
            auto* gaussianManipulator = gaussianViewer != nullptr
                ? dynamic_cast<osgGA::TrackballManipulator*>(gaussianViewer->getCameraManipulator())
                : nullptr;
            if (gaussianManipulator == nullptr) {
                std::cerr << "Gaussian pivot smoke could not access the trackball manipulator for "
                          << filePath.toStdString() << std::endl;
                allPassed = false;
                continue;
            }
            if (!verify(!gaussianManipulator->getVerticalAxisFixed(), "Gaussian viewing should use a free screen-space trackball")) {
                allPassed = false;
            }
            const osg::Matrixd viewBeforePress = gaussianViewer->getCamera()->getViewMatrix();

            QMouseEvent interactionPressEvent(
                QEvent::MouseButtonPress,
                QPointF(clickPoint),
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            QApplication::sendEvent(osgWidget, &interactionPressEvent);
            if (!verify(osgWidget->gaussianInteractionActive_, "Gaussian left drag should enable interaction rendering")) {
                allPassed = false;
            }
            const osg::Matrixd viewAfterPress = gaussianViewer->getCamera()->getViewMatrix();
            double maxViewDelta = 0.0;
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 4; ++column) {
                    maxViewDelta = std::max(maxViewDelta, std::abs(viewAfterPress(row, column) - viewBeforePress(row, column)));
                }
            }
            if (!verify(maxViewDelta <= 1e-8, "Gaussian left press should not move the camera before dragging")) {
                allPassed = false;
            }
            QMouseEvent interactionReleaseEvent(
                QEvent::MouseButtonRelease,
                QPointF(clickPoint),
                Qt::LeftButton,
                Qt::NoButton,
                Qt::NoModifier);
            QApplication::sendEvent(osgWidget, &interactionReleaseEvent);
            if (!verify(!osgWidget->gaussianInteractionActive_, "Gaussian left release should restore full rendering")) {
                allPassed = false;
            }
            viewer.resetView();
            pumpEvents(200);

            osgViewer::Viewer* osgViewer = osgWidget->getViewer();
            auto* manipulator = osgViewer != nullptr
                ? dynamic_cast<osgGA::TrackballManipulator*>(osgViewer->getCameraManipulator())
                : nullptr;
            if (manipulator == nullptr) {
                std::cerr << "Gaussian zoom smoke could not access the trackball manipulator for "
                          << filePath.toStdString() << std::endl;
                allPassed = false;
                continue;
            }

            manipulator->setDistance(std::max(0.05, manipulator->getDistance() * 0.03));
            osgWidget->update();
            pumpEvents(500);
            const QImage zoomedFrame = glWidget->grabFramebuffer();
            int zoomedNonBackgroundPixelCount = 0;
            const bool zoomedVisiblePixels = hasVisiblePixels(zoomedFrame, &zoomedNonBackgroundPixelCount);
            const double zoomedCoverage = zoomedFrame.isNull()
                ? 1.0
                : static_cast<double>(zoomedNonBackgroundPixelCount)
                    / static_cast<double>(zoomedFrame.width() * zoomedFrame.height());
            std::cout << "Zoomed Gaussian " << filePath.toStdString()
                      << " nonBackgroundPixels=" << zoomedNonBackgroundPixelCount
                      << " coverage=" << zoomedCoverage << std::endl;
            if (!zoomedVisiblePixels || zoomedCoverage >= 0.98) {
                std::cerr << "Zoomed Gaussian render became empty or saturated for "
                          << filePath.toStdString() << std::endl;
                allPassed = false;
                continue;
            }
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

        if (gaussianPly) {
            const QString switchProjectPath = QFileInfo(QStringLiteral("./out/build/bin/Release/project.lpproj")).absoluteFilePath();
            QFile projectFile(switchProjectPath);
            if (projectFile.open(QIODevice::ReadOnly)) {
                const QJsonArray projectPaths = QJsonDocument::fromJson(projectFile.readAll())
                    .object().value(QStringLiteral("pointCloudFilePaths")).toArray();
                projectFile.close();
                QStringList resolvedProjectPaths;
                for (const QJsonValue& pathValue : projectPaths) {
                    resolvedProjectPaths.append(QFileInfo(QFileInfo(switchProjectPath).dir(), pathValue.toString()).absoluteFilePath());
                }
                QString switchError;
                if (!viewer.loadPointCloudFiles(resolvedProjectPaths, &switchError)) {
                    std::cerr << "Gaussian-to-project point-cloud switch failed: " << switchError.toStdString() << std::endl;
                    allPassed = false;
                    continue;
                }
                pumpEvents(800);
                auto* switchManipulator = dynamic_cast<osgGA::TrackballManipulator*>(osgWidget->getViewer()->getCameraManipulator());
                if (!verify(!viewer.hasGaussianModel(), "Loading project point clouds after Gaussian should clear the Gaussian renderer")) {
                    allPassed = false;
                }
                if (!verify(viewer.hasPointCloud(), "Loading project point clouds after Gaussian should build the point cloud scene")) {
                    allPassed = false;
                }
                if (!verify(switchManipulator != nullptr && switchManipulator->getVerticalAxisFixed(),
                        "Loading project point clouds should restore fixed Z-up orbiting")) {
                    allPassed = false;
                }
                const QImage switchedFrame = glWidget->grabFramebuffer();
                int switchedNonBackgroundPixelCount = 0;
                if (!hasVisiblePixels(switchedFrame, &switchedNonBackgroundPixelCount)) {
                    std::cerr << "Project point-cloud framebuffer is empty after switching from Gaussian" << std::endl;
                    allPassed = false;
                }
            }
        }
    }

    return allPassed;
}
