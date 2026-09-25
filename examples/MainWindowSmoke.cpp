#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDialog>
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
#include <QPointer>
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
#include "gui/WelcomeWorkspaceWidget.h"
#include "gui/WorkspaceThumbnailCache.h"
#include "logging/ApplicationLogger.h"
#include "route/InspectionRoutePlanning.h"
#include "route/PowerlineRouteBridge.h"
#include "route/PowerlineRouteJson.h"
#include "route/RouteInterop.h"

#include "QtnRibbonBackstageView.h"
#include "QtnRibbonBar.h"
#include "QtnRibbonSystemPopupBar.h"

#include "SmokeTestSupport.h"

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

    PointCloudViewer* welcomeViewer = window.findChild<PointCloudViewer*>();
    if (!verify(welcomeViewer != nullptr, "MainWindow should expose the point-cloud viewer")) {
        return false;
    }
    WelcomeWorkspaceWidget* welcomeWorkspace = welcomeViewer->welcomeWorkspace();
    if (!verify(welcomeWorkspace != nullptr, "Empty viewer should create the recent workspace")) {
        return false;
    }
    if (!verify(welcomeWorkspace->isVisible(), "Recent workspace should be visible before loading data")) {
        return false;
    }
    if (!verify(welcomeWorkspace->findChildren<QPushButton*>().size() >= 3,
            "Recent workspace should expose project, open-data, and add-data actions")) {
        return false;
    }
    if (!verify(welcomeWorkspace->findChildren<QListWidget*>().size() == 2,
            "Recent workspace should expose project and data lists")) {
        return false;
    }

    QTemporaryDir thumbnailTempDir;
    if (!verify(thumbnailTempDir.isValid(), "Thumbnail smoke should create a temporary directory")) {
        return false;
    }
    qputenv("LAS_VIEWER_THUMBNAIL_CACHE_DIR", thumbnailTempDir.path().toUtf8());
    const QString thumbnailSourcePath = QDir(thumbnailTempDir.path()).filePath(QStringLiteral("thumbnail-source.las"));
    QFile thumbnailSourceFile(thumbnailSourcePath);
    if (!verify(thumbnailSourceFile.open(QIODevice::WriteOnly), "Thumbnail smoke should create a source file")) {
        return false;
    }
    thumbnailSourceFile.write("thumbnail-v1");
    thumbnailSourceFile.close();
    QImage thumbnailSourceImage(640, 360, QImage::Format_RGB32);
    thumbnailSourceImage.fill(QColor(QStringLiteral("#16a34a")));
    if (!verify(WorkspaceThumbnailCache::save(
            thumbnailSourcePath,
            WorkspaceThumbnailCache::Kind::Data,
            thumbnailSourceImage),
            "Thumbnail cache should save a rendered scene image")) {
        return false;
    }
    const QImage cachedThumbnail = WorkspaceThumbnailCache::imageFor(
        thumbnailSourcePath,
        WorkspaceThumbnailCache::Kind::Data);
    if (!verify(cachedThumbnail.size() == QSize(320, 180), "Thumbnail cache should return the expected card image size")) {
        return false;
    }
    thumbnailSourceFile.open(QIODevice::Append);
    thumbnailSourceFile.write("-changed");
    thumbnailSourceFile.close();
    const QImage invalidatedThumbnail = WorkspaceThumbnailCache::imageFor(
        thumbnailSourcePath,
        WorkspaceThumbnailCache::Kind::Data);
    if (!verify(invalidatedThumbnail.pixelColor(0, 0) != cachedThumbnail.pixelColor(0, 0),
            "Changing the source file should invalidate the cached thumbnail")) {
        qunsetenv("LAS_VIEWER_THUMBNAIL_CACHE_DIR");
        return false;
    }
    qunsetenv("LAS_VIEWER_THUMBNAIL_CACHE_DIR");

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

          if (!verify(
                  QMetaObject::invokeMethod(
                      viewer,
                      "inspectionRouteWaypointDoubleClicked",
                      Qt::DirectConnection,
                      Q_ARG(int, 0)),
                  "Route waypoint double-click signal should stay invokable")) {
              return false;
          }
          pumpEvents(80);
          QPointer<QDialog> routeWaypointEditDialog = window.findChild<QDialog*>(
              QStringLiteral("routeWaypointEditDialog"),
              Qt::FindDirectChildrenOnly);
          if (!verify(routeWaypointEditDialog != nullptr && routeWaypointEditDialog->isVisible(), "Route waypoint editor dialog should be visible")) {
              return false;
          }
          routeWaypointEditDialog->reject();
          pumpEvents(80);
          if (!verify(
                  routeWaypointEditDialog == nullptr || !routeWaypointEditDialog->isVisible(),
                  "Cancelling route waypoint editor should close the dialog")) {
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

bool runScreenRecordingSmoke(const QStringList& filePaths)
{
#ifdef LAS_VIEWER_ENABLE_WINDOWS_CAPTURE
    QTranslator appTranslator;
    QTranslator qtTranslator;
    MainWindow window(&appTranslator, &qtTranslator);
    window.resize(1400, 900);
    window.show();
    pumpEvents(320);

    if (!verify(window.screenRecorder_ != nullptr, "Screen recording smoke should create a recorder backend")) {
        return false;
    }
    if (!verify(window.toggleScreenRecordingAction_ != nullptr, "Screen recording smoke should expose the toggle recording action")) {
        return false;
    }
    if (!verify(window.screenRecorder_->isAvailable(), "Embedded recorder should be available in capture-on build")) {
        std::cerr << "[FAIL] Recorder unavailable reason: "
                  << window.screenRecorder_->unavailableReason().toStdString() << std::endl;
        return false;
    }

    PointCloudViewer* viewer = window.findChild<PointCloudViewer*>();
    if (!verify(viewer != nullptr, "Screen recording smoke should create an embedded viewer")) {
        return false;
    }

    const QString lasFilePath = filePaths.isEmpty() ? QString() : QFileInfo(filePaths.constFirst()).absoluteFilePath();
    if (!lasFilePath.isEmpty() && QFileInfo::exists(lasFilePath)) {
        QString errorMessage;
        if (!viewer->loadPointCloud(lasFilePath, &errorMessage)) {
            std::cerr << "[FAIL] Screen recording smoke failed to load point cloud: "
                      << errorMessage.toStdString() << std::endl;
            return false;
        }
        pumpEvents(900);
    }

    QTemporaryDir captureDir;
    if (!verify(captureDir.isValid(), "Screen recording smoke should create a temporary capture directory")) {
        return false;
    }

    window.captureSkipSaveDialog_ = true;
    window.captureSaveDirectory_ = captureDir.path();

    window.toggleScreenRecordingAction_->trigger();
    pumpEvents(1100);
    if (!verify(window.screenRecorder_->isRecording(), "Screen recording smoke should start recording")) {
        return false;
    }

    const QString firstTemporaryOutputPath = window.recordingOutputFilePath_;
    if (!verify(!firstTemporaryOutputPath.isEmpty(), "Screen recording smoke should produce a temporary output path")) {
        return false;
    }

    window.toggleScreenRecordingAction_->trigger();
    pumpEvents(900);
    if (!verify(!window.screenRecorder_->isRecording(), "Screen recording smoke should stop recording")) {
        return false;
    }

    const QString firstSavedOutputPath = window.recordingOutputFilePath_;
    if (!verify(!firstSavedOutputPath.isEmpty(), "Screen recording smoke should finalize to a saved output path")) {
        return false;
    }
    const QFileInfo firstOutputInfo(firstSavedOutputPath);
    if (!verify(firstOutputInfo.exists(), "Screen recording smoke should write a recording file")) {
        return false;
    }
    if (!verify(firstOutputInfo.size() > 0, "Screen recording smoke output file should be non-empty")) {
        return false;
    }
    if (!verify(!QFileInfo::exists(firstTemporaryOutputPath), "Temporary recording file should be moved after stop")) {
        return false;
    }

    window.toggleScreenRecordingAction_->trigger();
    pumpEvents(1000);
    if (!verify(window.screenRecorder_->isRecording(), "Screen recording smoke second run should start recording")) {
        return false;
    }

    const QString closeWhileRecordingTemporaryPath = window.recordingOutputFilePath_;
    if (!verify(!closeWhileRecordingTemporaryPath.isEmpty(), "Second recording run should produce a temporary output path")) {
        return false;
    }

    window.close();
    pumpEvents(1000);
    if (!verify(!window.screenRecorder_->isRecording(), "Closing MainWindow should stop active recording")) {
        return false;
    }

    const QString closeWhileRecordingSavedPath = window.recordingOutputFilePath_;
    const QFileInfo closeWhileRecordingInfo(closeWhileRecordingSavedPath);
    if (!verify(closeWhileRecordingInfo.exists(), "Closing MainWindow during recording should still finalize output file")) {
        return false;
    }
    if (!verify(closeWhileRecordingInfo.size() > 0, "Output file after close should be non-empty")) {
        return false;
    }
    if (!verify(!QFileInfo::exists(closeWhileRecordingTemporaryPath), "Temporary recording file should be moved when closing MainWindow")) {
        return false;
    }

    std::cout << "[PASS] Screen recording smoke test completed." << std::endl;
    return true;
#else
    (void)filePaths;
    std::cout << "[PASS] Screen recording smoke skipped because LAS_VIEWER_ENABLE_WINDOWS_CAPTURE is OFF." << std::endl;
    return true;
#endif
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
