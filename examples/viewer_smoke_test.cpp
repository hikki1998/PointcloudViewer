#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QOpenGLWidget>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSlider>
#include <QSpinBox>
#include <QSurfaceFormat>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTranslator>
#include <QLineEdit>

#include <cmath>
#include <functional>
#include <iostream>

#include "crs/CrsAuthorityService.h"
#include "domain/InspectionData.h"
#include "domain/TowerFileInterop.h"
#include "gui/ApplicationLogDock.h"
#include "gui/IssueController.h"
#include "gui/MainWindow.h"
#include "gui/MeasurementAnalysisController.h"
#include "gui/PointCloudViewer.h"
#include "gui/ProfileClassificationController.h"
#include "gui/ProfileClassificationWidget.h"
#include "gui/ProjectExplorerController.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/RouteController.h"
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
        }
    }

    return allPassed;
}

bool runMainBackstageSmoke(const QStringList&)
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

    std::cout << "[PASS] Main backstage smoke test completed." << std::endl;
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
    leftTarget.targetLocalPoint = leftInsulator.localPoint;
    captureWaypoint.captureTargets.append(leftTarget);

    RouteCaptureTarget rightTarget;
    rightTarget.partIndex = 2;
    rightTarget.partFileId = 102;
    rightTarget.partName = rightInsulator.partName;
    rightTarget.captureCount = 1;
    rightTarget.aircraftYawDeg = 90.0;
    rightTarget.gimbalPitchDeg = -40.0;
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
        << "Modes: viewer-render, main-backstage, log-panel, project-explorer-dock, project-explorer-controller, visualization-panel-controller, measurement-analysis-controller, profile-classification-widget, profile-classification-controller, route-controller, tower-controller, issue-controller, route-json, route-interop, route-roam, tower-file, tower-project-link, all" << std::endl
        << "Categories: render, ui, route, tower, all" << std::endl
        << "Examples:" << std::endl
        << "  LASViewerSmokeTest --mode main-backstage" << std::endl
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
        QStringLiteral("log-panel"),
        QStringLiteral("project-explorer-dock"),
        QStringLiteral("project-explorer-controller"),
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
