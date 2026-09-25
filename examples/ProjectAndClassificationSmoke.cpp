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
