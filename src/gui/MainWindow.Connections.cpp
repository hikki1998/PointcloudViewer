#include "gui/MainWindow.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPointF>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidgetItem>
#include <QUrl>

#include <algorithm>

#include "QtnRibbonStyle.h"
#include "crs/CrsTransformService.h"
#include "domain/ClearanceAnalysis.h"
#include "pointcloud/ClipFilter.h"
#include "pointcloud/LasWriter.h"
#include "domain/ClearanceReportExporter.h"
#include "domain/InspectionReportExporter.h"
#include "domain/VegetationRiskAnalysis.h"
#include "gui/ApplicationLogDock.h"
#include "gui/IssueController.h"
#include "gui/MeasurementAnalysisController.h"
#include "gui/MainWindowInternal.h"
#include "gui/PointCloudViewer.h"
#include "gui/ProfileClassificationController.h"
#include "gui/ProfileClassificationDock.h"
#include "gui/ProfilePlotWidget.h"
#include "gui/ProjectExplorerController.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/RouteController.h"
#include "gui/RouteDetailsDock.h"
#include "gui/SceneInspectorDock.h"
#include "gui/SpanProfileDock.h"
#include "gui/TowerController.h"
#include "gui/VisualizationPanelController.h"
#include "gui/WebPageDock.h"
#include "gui/support/UiHelpers.h"
#include "route/InspectionRoutePlanning.h"
#include "route/PowerlineRouteBridge.h"
#include "route/RouteInterop.h"

using namespace mainwindow_internal;
using lasviewer::crs::CrsTransformService;
using lasviewer::gui::showLightStyledMessageBox;
using lasviewer::gui::showStyledOpenFileNameDialog;
using lasviewer::gui::showStyledSaveFileNameDialog;

void MainWindow::createConnections()
{
    createControllerConnections();
    createWindowAndViewerConnections();
}

void MainWindow::createControllerConnections()
{
    if (projectExplorerController_ != nullptr) {
        connect(projectExplorerController_, &ProjectExplorerController::openRequested, this, [this]() {
            openProjectExplorerFile();
        });
        connect(projectExplorerController_, &ProjectExplorerController::searchTextChanged, this, [this](const QString&) {
            refreshProjectTreeFilter();
        });
        connect(projectExplorerController_, &ProjectExplorerController::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            applyProjectTreeItemCheckState(item);
        });
        connect(projectExplorerController_, &ProjectExplorerController::currentItemChanged, this, [this](QTreeWidgetItem* currentItem, QTreeWidgetItem*) {
            if (viewer_ == nullptr) {
                updateActionState();
                return;
            }
            if (currentItem == nullptr) {
                viewer_->setClipActiveDatasetPath(QString());
                updateActionState();
                return;
            }

            viewer_->setClipActiveDatasetPath(selectedDatasetPath());

            const QString itemType = projectTreeItemType(currentItem);
            if (itemType == QStringLiteral("imageItem")) {
                const int issueIndex = currentItem->data(0, kProjectTreeIssueIndexRole).toInt();
                viewer_->setSelectedIssueIndex(issueIndex);
                updateIssuePanel();
            } else if (itemType == QStringLiteral("trajectoryItem")) {
                selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty() ? -1 : 0;
                viewer_->setSelectedInspectionRouteWaypointIndex(selectedRouteWaypointIndex_);
                updateRoutePlanningPanel();
            } else if (itemType == QStringLiteral("pointCloudItem")) {
                viewer_->setSelectedIssueIndex(-1);
                updateIssuePanel();
            }
            updateActionState();
        });
        connect(projectExplorerController_, &ProjectExplorerController::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
            focusProjectTreeItem(item);
        });
        connect(projectExplorerController_, &ProjectExplorerController::customContextMenuRequested, this, [this](const QPoint& pos) {
            showProjectTreeContextMenu(pos);
        });
        connect(projectExplorerController_, &ProjectExplorerController::addPointCloudRequested, this, [this]() {
            addPointCloudFiles();
        });
        connect(projectExplorerController_, &ProjectExplorerController::removeDatasetRequested, this, [this]() {
            removeSelectedDataset();
        });
        connect(projectExplorerController_, &ProjectExplorerController::locateSelectedRequested, this, [this]() {
            const QTreeWidgetItem* currentItem = projectTreeWidget_ != nullptr ? projectTreeWidget_->currentItem() : nullptr;
            const QString filePath = projectTreeItemFilePath(currentItem);
            if (filePath.isEmpty()) {
                return;
            }

            const QString folderPath = QFileInfo(filePath).absolutePath();
            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath))) {
                showUserMessage(LogLevel::Warning, tr("Unable to open the selected file folder."), 3000);
            }
        });
        connect(projectExplorerController_, &ProjectExplorerController::copySelectedPathRequested, this, [this]() {
            const QTreeWidgetItem* currentItem = projectTreeWidget_ != nullptr ? projectTreeWidget_->currentItem() : nullptr;
            const QString filePath = projectTreeItemFilePath(currentItem);
            if (filePath.isEmpty()) {
                return;
            }

            if (QGuiApplication::clipboard() != nullptr) {
                QGuiApplication::clipboard()->setText(filePath);
                showUserMessage(LogLevel::Info, tr("Selected path copied."), 2000);
            }
        });
    }
    connect(openProjectAction_, &QAction::triggered, this, [this]() { openProject(); });
    connect(saveProjectAction_, &QAction::triggered, this, [this]() { saveProject(); });
    connect(saveProjectAsAction_, &QAction::triggered, this, [this]() { saveProjectAs(); });
    connect(projectCoordinateSystemsAction_, &QAction::triggered, this, [this]() { openBackstagePage(backstageProjectPropertiesPage_); });
    connect(clearAction_, &QAction::triggered, this, [this]() { clearPointCloud(); });
    connect(exitAction_, &QAction::triggered, this, &QWidget::close);
    connect(captureScreenshotAction_, &QAction::triggered, this, [this]() { captureMainWindowScreenshot(); });
    connect(toggleScreenRecordingAction_, &QAction::triggered, this, [this]() { toggleScreenRecording(); });

    connect(fitSceneAction_, &QAction::triggered, viewer_, &PointCloudViewer::resetView);
    connect(topViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Top); });
    connect(frontViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Front); });
    connect(rightViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Right); });

    const auto exportClearanceCsv = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        const ClearanceAnalysisResult analysisResult = analyzeClearancePath(
            viewer_->measurementResult().points,
            static_cast<float>(clearanceWarningThresholdMeters_));
        if (!analysisResult.isValid()) {
            showUserMessage(LogLevel::Warning, tr("Add at least two measured points before exporting clearance details."), 3000);
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Clearance CSV"),
            QStringLiteral("clearance_segments.csv"),
            tr("CSV Files (*.csv)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        const ClearanceRuleEvaluationResult ruleEvaluation = evaluateClearanceRules(
            analysisResult,
            { clearanceRulePreset_, static_cast<float>(clearanceWarningThresholdMeters_) });
        if (!ClearanceReportExporter::exportSegmentsCsv(filePath, analysisResult, &ruleEvaluation, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        showUserMessage(LogLevel::Info, tr("Clearance CSV exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    };
    const auto analyzeCurrentVegetationRisks = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before running vegetation risk analysis."), 3000);
            return;
        }
        if (viewer_->pointCloudData() == nullptr) {
            return;
        }

        const ClearanceAnalysisResult pathAnalysis = analyzeClearancePath(
            viewer_->measurementResult().points,
            static_cast<float>(clearanceWarningThresholdMeters_));
        if (!pathAnalysis.isValid()) {
            showUserMessage(LogLevel::Warning, tr("Add at least two measured points before analyzing corridor risks."), 3500);
            return;
        }

        VegetationRiskAnalysisParameters parameters;
        parameters.searchRadius = static_cast<float>(vegetationSearchRadiusMeters_);
        parameters.criticalThreshold = static_cast<float>(clearanceWarningThresholdMeters_);
        parameters.clusterGap = static_cast<float>(vegetationClusterGapMeters_);
        parameters.minimumClusterPoints = vegetationClusterPointCount_;
        parameters.preferVegetationClassification = preferVegetationClassification_;
        parameters.preset = clearanceRulePreset_;

        vegetationRiskResults_ = analyzeVegetationRisks(
            *viewer_->pointCloudData(),
            pathAnalysis,
            viewer_->towerMarkers(),
            parameters).records;
        selectedVegetationRiskIndex_ = vegetationRiskResults_.isEmpty() ? -1 : 0;
        updateVegetationRiskPanel();
        rebuildProjectTree();
        updateActionState();
        if (inspectorTabWidget_ != nullptr) {
            inspectorTabWidget_->setCurrentIndex(5);
        }
        showUserMessage(
            LogLevel::Info,
            vegetationRiskResults_.isEmpty()
                ? tr("Vegetation risk analysis completed. No clusters were found near the current corridor.")
                : tr("Vegetation risk analysis completed. %1 cluster(s) detected.")
                      .arg(QLocale().toString(vegetationRiskResults_.size())),
            4000);
    };
    const auto createIssueFromRisk = [this](int riskIndex) -> bool {
        if (viewer_ == nullptr || riskIndex < 0 || riskIndex >= vegetationRiskResults_.size()) {
            return false;
        }

        const VegetationRiskRecord& risk = vegetationRiskResults_.at(riskIndex);
        InspectionIssue issue;
        issue.id = issueDefaultId();
        issue.title = risk.title.trimmed().isEmpty() ? nextDefaultIssueTitle() : risk.title;
        issue.category = tr("Vegetation");
        switch (risk.severity) {
        case AnalysisSeverity::Advisory:
            issue.severity = IssueSeverity::Minor;
            break;
        case AnalysisSeverity::Warning:
            issue.severity = IssueSeverity::Major;
            break;
        case AnalysisSeverity::Critical:
            issue.severity = IssueSeverity::Critical;
            break;
        case AnalysisSeverity::None:
        default:
            issue.severity = IssueSeverity::Info;
            break;
        }
        issue.status = IssueStatus::Open;
        issue.point = risk.point;
        issue.relatedTowerIndex = risk.nearestTowerIndex;
        issue.relatedTowerName = risk.nearestTowerName;
        issue.description = tr("%1\nRule: %2\nMin distance: %3 m\nChainage: %4 - %5 m")
            .arg(risk.notes)
            .arg(risk.sourceRule)
            .arg(formatCoordinate(risk.minimumDistance))
            .arg(formatCoordinate(risk.chainageStart))
            .arg(formatCoordinate(risk.chainageEnd));
        issue.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
        return viewer_->addInspectionIssue(issue);
    };

    measurementAnalysisController_ = new MeasurementAnalysisController(
        viewer_,
        measureAction_,
        clearMeasurementAction_,
        exportClearanceCsvAction_,
        analyzeVegetationRisksAction_,
        focusVegetationRiskAction_,
        createIssueFromRiskAction_,
        createIssuesFromRisksAction_,
        clearVegetationRisksAction_,
        measurementToggleButton_,
        measurementClearButton_,
        clearanceThresholdSpinBox_,
        clearanceRulePresetComboBox_,
        vegetationSearchRadiusSpinBox_,
        vegetationClusterGapSpinBox_,
        vegetationClusterPointCountSpinBox_,
        preferVegetationClassificationCheckBox_,
        clearanceSegmentsTableWidget_,
        vegetationRisksTableWidget_,
        [this](bool enabled) {
            syncProfileDockForMeasurementMode(enabled);
        },
        exportClearanceCsv,
        analyzeCurrentVegetationRisks,
        [this]() {
            if (viewer_ == nullptr || selectedVegetationRiskIndex_ < 0 || selectedVegetationRiskIndex_ >= vegetationRiskResults_.size()) {
                return;
            }
            viewer_->focusOnPoint(vegetationRiskResults_.at(selectedVegetationRiskIndex_).point);
        },
        [this, createIssueFromRisk]() {
            if (createIssueFromRisk(selectedVegetationRiskIndex_)) {
                if (inspectorTabWidget_ != nullptr) {
                    inspectorTabWidget_->setCurrentIndex(2);
                }
                updateIssuePanel();
                showUserMessage(LogLevel::Info, tr("Created an inspection issue from the selected vegetation risk."), 3000);
            }
        },
        [this, createIssueFromRisk]() {
            int createdCount = 0;
            for (int riskIndex = 0; riskIndex < vegetationRiskResults_.size(); ++riskIndex) {
                if (createIssueFromRisk(riskIndex)) {
                    ++createdCount;
                }
            }
            updateIssuePanel();
            showUserMessage(
                LogLevel::Info,
                createdCount <= 0
                    ? tr("No vegetation risks were converted into issues.")
                    : tr("Created %1 inspection issue(s) from vegetation risks.").arg(QLocale().toString(createdCount)),
                3500);
        },
        [this]() {
            vegetationRiskResults_.clear();
            selectedVegetationRiskIndex_ = -1;
            updateVegetationRiskPanel();
            rebuildProjectTree();
            updateActionState();
            showUserMessage(LogLevel::Info, tr("Vegetation risk results cleared."), 2500);
        },
        [this](double value) {
            clearanceWarningThresholdMeters_ = value;
            persistMeasurementSettings();
            updateMeasurementPanel();
        },
        [this](int index) {
            if (index < 0 || clearanceRulePresetComboBox_ == nullptr) {
                return;
            }
            clearanceRulePreset_ = static_cast<ClearanceRulePreset>(clearanceRulePresetComboBox_->itemData(index).toInt());
            persistMeasurementSettings();
            updateMeasurementPanel();
            updateVegetationRiskPanel();
        },
        [this](double value) {
            vegetationSearchRadiusMeters_ = value;
            persistMeasurementSettings();
            updateVegetationRiskPanel();
        },
        [this](double value) {
            vegetationClusterGapMeters_ = value;
            persistMeasurementSettings();
        },
        [this](int value) {
            vegetationClusterPointCount_ = value;
            persistMeasurementSettings();
        },
        [this](bool checked) {
            preferVegetationClassification_ = checked;
            persistMeasurementSettings();
            updateVegetationRiskPanel();
        },
        [this](int currentRow) {
            if (profilePlotWidget_ != nullptr) {
                profilePlotWidget_->setSelectedSegmentIndex(currentRow);
            }
        },
        [this](int currentRow) {
            selectedVegetationRiskIndex_ = (currentRow >= 0 && currentRow < vegetationRiskResults_.size()) ? currentRow : -1;
            updateVegetationRiskPanel();
        },
        this);

    const auto syncRoutePlanningOptionsFromUi = [this]() {
        syncRoutePlanningFromProjectCoordinateSystems();
        if (aircraftProfileComboBox_ != nullptr) {
            const QVariant profileValue = aircraftProfileComboBox_->currentData();
            const int profileIndex = profileValue.isValid() ? profileValue.toInt() : static_cast<int>(routePlanningOptions_.aircraftProfile);
            routePlanningOptions_.aircraftProfile = static_cast<DjiAircraftProfile>(profileIndex);
        }
        if (routeSafetyHeightSpinBox_ != nullptr) {
            routePlanningOptions_.safety.safetyHeightMeters = static_cast<float>(routeSafetyHeightSpinBox_->value());
        }
        routePlanningOptions_.safety.globalTransitionalSpeedMps = 8.0f;
        routePlanningOptions_.safety.globalRthHeightMeters = 60.0f;
        routePlanningOptions_.safety.defaultGimbalPitchDeg = -45.0f;
        if (routeWaypointSpeedSpinBox_ != nullptr) {
            routePlanningOptions_.safety.defaultWaypointSpeedMps = static_cast<float>(routeWaypointSpeedSpinBox_->value());
        }
        if (routeWaypointSpacingSpinBox_ != nullptr) {
            routePlanningOptions_.generation.waypointSpacingMeters = static_cast<float>(routeWaypointSpacingSpinBox_->value());
        }
        if (routeSmoothingStrengthSpinBox_ != nullptr) {
            routePlanningOptions_.generation.smoothingStrengthPercent = static_cast<float>(routeSmoothingStrengthSpinBox_->value());
        }
        if (routeHeightOffsetSpinBox_ != nullptr) {
            routePlanningOptions_.safety.heightOffsetMeters = static_cast<float>(routeHeightOffsetSpinBox_->value());
        }
    };
    const auto regenerateInspectionRoute = [this, syncRoutePlanningOptionsFromUi]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before generating an inspection route."), 3000);
            return;
        }
        if (vegetationRiskResults_.isEmpty()) {
            showUserMessage(LogLevel::Warning, tr("Run vegetation risk analysis before generating an inspection route."), 3500);
            return;
        }

        syncRoutePlanningOptionsFromUi();
        const InspectionRoute generatedRoute = generateInspectionRouteFromRisks(
            vegetationRiskResults_,
            viewer_->towerMarkers(),
            routePlanningOptions_.generation,
            routePlanningOptions_.safety);
        currentPowerlineRoute_ = createPowerlineRouteFromInspectionRoute(
            generatedRoute,
            tr("Generated Inspection Route"));
        selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty() ? -1 : 0;
        selectedRouteWaypointTargetIndex_ = -1;
        applyCurrentRouteToViewer();
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();
        showUserMessage(
            LogLevel::Info,
            currentPowerlineRoute_.waypoints.isEmpty()
                ? tr("No route waypoints were generated.")
                : tr("Generated inspection route with %1 waypoint(s).")
                      .arg(QLocale().toString(currentPowerlineRoute_.waypoints.size())),
            3500);
    };
    const auto clearInspectionRoute = [this]() {
        currentPowerlineRoute_ = PowerlineRouteDocument();
        selectedRouteWaypointIndex_ = -1;
        selectedRouteWaypointTargetIndex_ = -1;
        linkedRouteFilePath_.clear();
        setRouteEditingEnabled(false, false);
        viewer_->clearInspectionRouteWaypoints();
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();
        showUserMessage(LogLevel::Info, tr("Inspection route cleared."), 2500);
    };
    const auto syncRouteRoamControls = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        const bool hasRoute = !currentPowerlineRoute_.waypoints.isEmpty();
        const bool roamReady = hasRoute && viewer_->hasPointCloud() && viewer_->inspectionRouteVisible();
        const bool roamActive = viewer_->inspectionRouteRoamActive();
        const bool roamPaused = viewer_->inspectionRouteRoamPaused();

        if (routeRoamSpeedSpinBox_ != nullptr) {
            routeRoamSpeedSpinBox_->setEnabled(hasRoute);
            const QSignalBlocker blocker(routeRoamSpeedSpinBox_);
            routeRoamSpeedSpinBox_->setValue(viewer_->inspectionRouteRoamSpeedMetersPerSecond());
        }
        if (routeRoamViewModeComboBox_ != nullptr) {
            routeRoamViewModeComboBox_->setEnabled(hasRoute);
            const QSignalBlocker blocker(routeRoamViewModeComboBox_);
            const int modeIndex = routeRoamViewModeComboBox_->findData(static_cast<int>(viewer_->inspectionRouteRoamViewMode()));
            routeRoamViewModeComboBox_->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
        }

        if (startInspectionRouteRoamAction_ != nullptr) {
            startInspectionRouteRoamAction_->setEnabled(roamReady && !roamActive);
        }
        if (pauseInspectionRouteRoamAction_ != nullptr) {
            pauseInspectionRouteRoamAction_->setEnabled(roamReady && roamActive);
            pauseInspectionRouteRoamAction_->setText(roamPaused ? tr("Resume Roam") : tr("Pause Roam"));
        }
        if (stopInspectionRouteRoamAction_ != nullptr) {
            stopInspectionRouteRoamAction_->setEnabled(roamReady && roamActive);
        }

        if (routeRoamStartButton_ != nullptr) {
            routeRoamStartButton_->setEnabled(roamReady && !roamActive);
        }
        if (routeRoamPauseResumeButton_ != nullptr) {
            routeRoamPauseResumeButton_->setEnabled(roamReady && roamActive);
            routeRoamPauseResumeButton_->setText(roamPaused ? tr("Resume Roam") : tr("Pause Roam"));
        }
        if (routeRoamStopButton_ != nullptr) {
            routeRoamStopButton_->setEnabled(roamReady && roamActive);
        }
        syncRouteRoamFloatingDialog();
    };
    const auto startInspectionRouteRoam = [this, syncRouteRoamControls]() {
        if (viewer_ == nullptr || currentPowerlineRoute_.waypoints.isEmpty()) {
            return;
        }
        if (!viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before starting route roam."), 3200);
            return;
        }
        if (!viewer_->inspectionRouteVisible()) {
            showUserMessage(LogLevel::Warning, tr("Show the inspection route before starting camera roam."), 3200);
            return;
        }

        const int startIndex = std::clamp(selectedRouteWaypointIndex_, 0, currentPowerlineRoute_.waypoints.size() - 1);
        routeRoamLastCaptureSummary_.clear();
        viewer_->startInspectionRouteRoam(startIndex);
        syncRouteRoamControls();
    };
    const auto pauseResumeInspectionRouteRoam = [this, syncRouteRoamControls]() {
        if (viewer_ == nullptr || !viewer_->inspectionRouteRoamActive()) {
            return;
        }
        if (viewer_->inspectionRouteRoamPlaying()) {
            viewer_->pauseInspectionRouteRoam();
        } else {
            viewer_->resumeInspectionRouteRoam();
        }
        syncRouteRoamControls();
    };
    const auto stopInspectionRouteRoam = [this, syncRouteRoamControls]() {
        if (viewer_ == nullptr) {
            return;
        }
        routeRoamLastCaptureSummary_.clear();
        viewer_->stopInspectionRouteRoam(true);
        syncRouteRoamControls();
    };
    const auto handleInspectionRouteRoamStateChanged = [this, syncRouteRoamControls]() {
        syncRouteRoamControls();
        updateRoutePlanningPanel();
        updateActionState();
    };
    const auto focusSelectedRouteWaypoint = [this]() {
        if (viewer_ == nullptr
            || selectedRouteWaypointIndex_ < 0
            || selectedRouteWaypointIndex_ >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }
        viewer_->focusOnPoint(currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).localPoint, 0.22);
    };
    const auto importRouteFileFromDisk = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before importing route files."), 3000);
            return;
        }
        const QString filePath = showStyledOpenFileNameDialog(
            this,
            tr("Import Route File"),
            QString(),
            tr("Route JSON Files (*.json);;All Files (*.*)"));
        if (filePath.isEmpty()) {
            return;
        }
        importRouteFile(filePath, true, true);
    };
    const auto saveRouteFileToDisk = [this]() {
        if (currentPowerlineRoute_.waypoints.isEmpty()) {
            return;
        }
        if (linkedRouteFilePath_.trimmed().isEmpty()) {
            const QString filePath = showStyledSaveFileNameDialog(
                this,
                tr("Save Route File"),
                QStringLiteral("route.json"),
                tr("Route JSON Files (*.json)"));
            if (filePath.isEmpty()) {
                return;
            }
            exportRouteFile(filePath, true, true);
            return;
        }
        exportRouteFile(linkedRouteFilePath_, true, true);
    };
    const auto saveRouteFileAsToDisk = [this]() {
        if (currentPowerlineRoute_.waypoints.isEmpty()) {
            return;
        }
        const QString initialPath = linkedRouteFilePath_.trimmed().isEmpty()
            ? QStringLiteral("route.json")
            : linkedRouteFilePath_;
        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Save Route File As"),
            initialPath,
            tr("Route JSON Files (*.json)"));
        if (filePath.isEmpty()) {
            return;
        }
        exportRouteFile(filePath, true, true);
    };
    const auto reloadLinkedRouteFileFromDisk = [this]() {
        reloadLinkedRouteFile(true);
    };
    const auto importRouteKmlFromDisk = [this, syncRoutePlanningOptionsFromUi]() {
        syncRoutePlanningOptionsFromUi();
        if (projectCoordinateSystems_.pointCloudCrs.code <= 0) {
            showUserMessage(LogLevel::Error, tr("Set the project point cloud CRS before importing route KML."), 4500);
            return;
        }

        const QString filePath = showStyledOpenFileNameDialog(
            this,
            tr("Import Route KML"),
            QString(),
            tr("KML Files (*.kml)"));
        if (filePath.isEmpty()) {
            return;
        }

        InspectionRoute importedWgs84;
        QString errorMessage;
        if (!importRouteKml(filePath, &importedWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        InspectionRoute importedLocal;
        if (!transformRouteFromWgs84(importedWgs84, projectCoordinateSystems_, &importedLocal, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        currentPowerlineRoute_ = createPowerlineRouteFromInspectionRoute(importedLocal, QFileInfo(filePath).baseName());
        selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty() ? -1 : 0;
        selectedRouteWaypointTargetIndex_ = -1;
        linkedRouteFilePath_.clear();
        applyCurrentRouteToViewer();
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();
        showRouteDetailsDock(0);
        showUserMessage(LogLevel::Info, tr("Imported route KML: %1").arg(QFileInfo(filePath).fileName()), 3500);
    };
    const auto exportRouteKmlToDisk = [this, syncRoutePlanningOptionsFromUi]() {
        if (currentPowerlineRoute_.waypoints.isEmpty()) {
            showUserMessage(LogLevel::Warning, tr("Generate a route before exporting KML."), 3000);
            return;
        }

        syncRoutePlanningOptionsFromUi();
        if (projectCoordinateSystems_.pointCloudCrs.code <= 0) {
            showUserMessage(LogLevel::Error, tr("Set the project point cloud CRS before exporting route KML."), 4500);
            return;
        }

        const InspectionRoute routeLocal = toInspectionRouteExportView(currentPowerlineRoute_);
        InspectionRoute routeWgs84;
        QString errorMessage;
        if (!transformRouteToWgs84(routeLocal, projectCoordinateSystems_, &routeWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Route KML"),
            QStringLiteral("inspection_route.kml"),
            tr("KML Files (*.kml)"));
        if (filePath.isEmpty()) {
            return;
        }

        if (!exportRouteKml(filePath, routeWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }
        showUserMessage(LogLevel::Info, tr("Route KML exported: %1").arg(QFileInfo(filePath).fileName()), 3500);
    };
    const auto exportRouteDjiKmzToDisk = [this, syncRoutePlanningOptionsFromUi]() {
        if (currentPowerlineRoute_.waypoints.size() < 2) {
            showUserMessage(LogLevel::Warning, tr("Route needs at least 2 waypoints for DJI KMZ export."), 3500);
            return;
        }

        updateRoutePlanningPanel();
        if (routeQaReport_.hasBlockingIssues()) {
            showRouteDetailsDock(2);
            showUserMessage(
                LogLevel::Error,
                tr("Route QA found %1 blocking issue(s). Fix them before exporting DJI KMZ.")
                    .arg(QLocale().toString(routeQaReport_.blockingIssueCount)),
                5200);
            return;
        }

        if (routeQaReport_.hasWarnings()) {
            showRouteDetailsDock(2);
            const QMessageBox::StandardButton choice = showLightStyledMessageBox(
                this,
                QMessageBox::Warning,
                tr("Route QA Warning"),
                tr("Route QA found %1 warning issue(s). Continue exporting DJI KMZ?")
                    .arg(QLocale().toString(routeQaReport_.warningIssueCount)),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (choice != QMessageBox::Yes) {
                return;
            }
        }

        syncRoutePlanningOptionsFromUi();
        if (projectCoordinateSystems_.pointCloudCrs.code <= 0) {
            showUserMessage(LogLevel::Error, tr("Set the project point cloud CRS before exporting DJI KMZ."), 4500);
            return;
        }

        const InspectionRoute routeLocal = toInspectionRouteExportView(currentPowerlineRoute_);
        InspectionRoute routeWgs84;
        QString errorMessage;
        if (!transformRouteToWgs84(routeLocal, projectCoordinateSystems_, &routeWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export DJI KMZ"),
            QStringLiteral("inspection_route.kmz"),
            tr("DJI Wayline KMZ (*.kmz)"));
        if (filePath.isEmpty()) {
            return;
        }

        if (!exportRouteDjiKmz(filePath, routeWgs84, routePlanningOptions_, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }
        showUserMessage(LogLevel::Info, tr("DJI KMZ exported: %1").arg(QFileInfo(filePath).fileName()), 3500);
    };

    routeController_ = new RouteController(
        viewer_,
        generateInspectionRouteAction_,
        regenerateInspectionRouteAction_,
        clearInspectionRouteAction_,
        toggleRouteEditingAction_,
        startInspectionRouteRoamAction_,
        pauseInspectionRouteRoamAction_,
        stopInspectionRouteRoamAction_,
        focusRouteWaypointAction_,
        importRouteFileAction_,
        saveRouteFileAction_,
        saveRouteFileAsAction_,
        reloadRouteFileAction_,
        importRouteKmlAction_,
        exportRouteKmlAction_,
        exportRouteDjiKmzAction_,
        routeRoamStartButton_,
        routeRoamPauseResumeButton_,
        routeRoamStopButton_,
        routeRoamSpeedSpinBox_,
        routeRoamViewModeComboBox_,
        regenerateInspectionRoute,
        clearInspectionRoute,
        [this](bool enabled) {
            setRouteEditingEnabled(enabled, true);
        },
        startInspectionRouteRoam,
        pauseResumeInspectionRouteRoam,
        stopInspectionRouteRoam,
        [this](double speed) {
            if (closingWindow_ || viewer_ == nullptr) {
                return;
            }

            viewer_->setInspectionRouteRoamSpeedMetersPerSecond(speed);
            syncRouteRoamFloatingDialog();
            persistWindowSettings();
        },
        [this](int) {
            if (closingWindow_ || viewer_ == nullptr || routeRoamViewModeComboBox_ == nullptr) {
                return;
            }

            viewer_->setInspectionRouteRoamViewMode(static_cast<RouteRoamViewMode>(
                routeRoamViewModeComboBox_->currentData().toInt()));
            syncRouteRoamFloatingDialog();
            persistWindowSettings();
        },
        focusSelectedRouteWaypoint,
        importRouteFileFromDisk,
        saveRouteFileToDisk,
        saveRouteFileAsToDisk,
        reloadLinkedRouteFileFromDisk,
        importRouteKmlFromDisk,
        exportRouteKmlToDisk,
        exportRouteDjiKmzToDisk,
        handleInspectionRouteRoamStateChanged,
        [this](int waypointIndex, int targetIndex, const QString& targetLabel, int captureCount) {
            handleRouteRoamPhotoCaptured(waypointIndex, targetIndex, targetLabel, captureCount);
        },
        this);

    const auto beginAddTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before adding tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }
        viewer_->beginTowerAddMode();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower add mode enabled. Click points continuously to add tower markers, or cancel the tool when finished."), 4500);
    };
    const auto beginInsertTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before inserting tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }

        const int currentRow = viewer_->selectedTowerIndex();
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            showUserMessage(LogLevel::Warning, tr("Select the current tower before inserting a new one."), 3000);
            return;
        }

        viewer_->setSelectedTowerIndex(currentRow);
        viewer_->beginTowerInsertMode(currentRow);
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Click a point in the view to insert a tower marker before the current one."), 4000);
    };
    const auto beginMoveTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before moving tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }

        const int currentRow = viewer_->selectedTowerIndex();
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            showUserMessage(LogLevel::Warning, tr("Select the current tower before moving it."), 3000);
            return;
        }

        viewer_->setSelectedTowerIndex(currentRow);
        viewer_->beginTowerMoveMode(currentRow);
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Click a point in the view to move the current tower marker."), 4000);
    };
    const auto editCurrentTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before moving tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }

        const int currentRow = viewer_->selectedTowerIndex();
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            showUserMessage(LogLevel::Warning, tr("Select the current tower before moving it."), 3000);
            return;
        }

        viewer_->setSelectedTowerIndex(currentRow);
        viewer_->beginTowerMoveMode(currentRow);
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Click a point in the view to move the current tower marker."), 4000);
    };
    const auto focusSelectedTower = [this]() {
        if (viewer_ == nullptr || towerTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = towerTableWidget_->currentRow();
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            return;
        }

        viewer_->focusOnPoint(viewer_->towerMarkers().at(currentRow).point);
    };
    const auto removeSelectedTower = [this]() {
        if (viewer_ == nullptr || towerTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = towerTableWidget_->currentRow();
        if (!viewer_->removeTowerMarker(currentRow)) {
            return;
        }

        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower marker removed."), 2500);
    };
    const auto clearAllTowers = [this]() {
        if (viewer_ == nullptr || viewer_->towerMarkers().isEmpty()) {
            return;
        }

        viewer_->clearTowerMarkers();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower markers cleared."), 2500);
    };
    const auto cancelTowerTool = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        viewer_->cancelTowerEditMode();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower tool cancelled."), 2500);
    };
    const auto importTowerFileFromDialog = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before importing tower files."), 3000);
            return;
        }
        const QString filePath = showStyledOpenFileNameDialog(
            this,
            tr("Import Tower File"),
            QString(),
            tr("LiTower Files (*.LiTower);;CSV Files (*.csv);;All Files (*.*)"));
        if (filePath.isEmpty()) {
            return;
        }
        importTowerFile(filePath, true, true);
    };
    const auto saveTowerFileToLinkedPath = [this]() {
        if (viewer_ == nullptr) {
            return;
        }
        if (linkedTowerFilePath_.trimmed().isEmpty()) {
            const QString filePath = showStyledSaveFileNameDialog(
                this,
                tr("Save Tower File"),
                QStringLiteral("tower.LiTower"),
                tr("LiTower Files (*.LiTower);;CSV Files (*.csv)"));
            if (filePath.isEmpty()) {
                return;
            }
            exportTowerFile(filePath, true, true);
            return;
        }

        exportTowerFile(linkedTowerFilePath_, true, true);
    };
    const auto saveTowerFileAs = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Save Tower File As"),
            linkedTowerFilePath_.trimmed().isEmpty() ? QStringLiteral("tower.LiTower") : linkedTowerFilePath_,
            tr("LiTower Files (*.LiTower);;CSV Files (*.csv)"));
        if (filePath.isEmpty()) {
            return;
        }
        exportTowerFile(filePath, true, true);
    };
    const auto reloadTowerFileFromLinkedPath = [this]() {
        reloadLinkedTowerFile(true);
    };
    const auto startTowerEditing = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before editing tower markers."), 3000);
            return;
        }

        setTowerEditingEnabled(true);
        if (inspectorTabWidget_ != nullptr) {
            inspectorTabWidget_->setCurrentIndex(1);
        }
        showUserMessage(LogLevel::Info, tr("Tower editing started. Use the tools in the right dock to add, insert, move, rename, or remove tower markers."), 4500);
    };
    const auto finishTowerEditing = [this]() {
        setTowerEditingEnabled(false);
        showUserMessage(LogLevel::Info, tr("Tower editing finished."), 2500);
    };
    const auto commitTowerDetails = [this]() {
        if (updatingTowerDetails_ || viewer_ == nullptr || towerTypeComboBox_ == nullptr) {
            return;
        }

        const int selectedTowerIndex = viewer_->selectedTowerIndex();
        if (selectedTowerIndex < 0 || selectedTowerIndex >= viewer_->towerMarkers().size()) {
            return;
        }

        TowerRecord towerRecord = viewer_->towerMarkers().at(selectedTowerIndex);
        towerRecord.code = towerCodeEdit_->text().trimmed();
        towerRecord.lineName = towerLineNameEdit_->text().trimmed();
        towerRecord.voltageLevel = towerVoltageLevelEdit_->text().trimmed();
        towerRecord.towerType = static_cast<TowerType>(towerTypeComboBox_->currentData().toInt());
        towerRecord.structureType = towerStructureTypeEdit_->text().trimmed();
        towerRecord.inspectionDate = towerInspectionDateEdit_->text().trimmed();
        towerRecord.status = towerStatusEdit_->text().trimmed();
        towerRecord.notes = towerNotesEdit_->toPlainText().trimmed();
        if (viewer_->setTowerRecord(selectedTowerIndex, towerRecord)) {
            updateTowerPanel();
        }
    };

    towerController_ = new TowerController(
        startTowerEditAction_,
        finishTowerEditAction_,
        addTowerAction_,
        insertTowerAction_,
        moveTowerAction_,
        editCurrentTowerAction_,
        focusTowerAction_,
        removeTowerAction_,
        clearTowersAction_,
        cancelTowerToolAction_,
        importTowerFileAction_,
        saveTowerFileAction_,
        saveTowerFileAsAction_,
        reloadTowerFileAction_,
        showTowerXAction_,
        showTowerYAction_,
        showTowerZAction_,
        towerTableWidget_,
        towerCodeEdit_,
        towerLineNameEdit_,
        towerVoltageLevelEdit_,
        towerTypeComboBox_,
        towerStructureTypeEdit_,
        towerInspectionDateEdit_,
        towerStatusEdit_,
        towerNotesEdit_,
        startTowerEditing,
        finishTowerEditing,
        beginAddTower,
        beginInsertTower,
        beginMoveTower,
        editCurrentTower,
        focusSelectedTower,
        removeSelectedTower,
        clearAllTowers,
        cancelTowerTool,
        importTowerFileFromDialog,
        saveTowerFileToLinkedPath,
        saveTowerFileAs,
        reloadTowerFileFromLinkedPath,
        [this](bool) {
            persistWindowSettings();
        },
        [this](bool) {
            persistWindowSettings();
        },
        [this](bool) {
            persistWindowSettings();
        },
        [this](int currentRow) {
            if (viewer_ != nullptr) {
                viewer_->setSelectedTowerIndex(currentRow);
            }
            updateActionState();
            updateTowerPanel();
        },
        [this](int row, const QString& towerName) {
            if (viewer_ == nullptr) {
                return;
            }

            if (!towerEditingEnabled_) {
                updateTowerPanel();
                return;
            }

            if (!viewer_->setTowerMarkerName(row, towerName)) {
                showUserMessage(LogLevel::Warning, tr("Tower marker name cannot be empty."), 3000);
                updateTowerPanel();
                return;
            }

            updateTowerPanel();
        },
        commitTowerDetails,
        this);

    const auto beginIssueMarking = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before marking issues."), 3000);
            return;
        }

        viewer_->beginIssueAddMode();
        if (inspectorTabWidget_ != nullptr) {
            inspectorTabWidget_->setCurrentIndex(2);
        }
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Issue marking enabled. Click a point in the view to add an issue, or right-click to cancel."), 4500);
    };
    const auto cancelIssueTool = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        viewer_->cancelIssueEditMode();
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Issue tool cancelled."), 2500);
    };
    const auto focusSelectedIssue = [this]() {
        if (viewer_ == nullptr || issueTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = issueTableWidget_->currentRow();
        if (currentRow < 0 || currentRow >= viewer_->inspectionIssues().size()) {
            return;
        }

        viewer_->focusOnPoint(viewer_->inspectionIssues().at(currentRow).point, 0.2);
    };
    const auto removeSelectedIssue = [this]() {
        if (viewer_ == nullptr || issueTableWidget_ == nullptr) {
            return;
        }

        if (viewer_->removeInspectionIssue(issueTableWidget_->currentRow())) {
            updateIssuePanel();
            showUserMessage(LogLevel::Info, tr("Inspection issue removed."), 2500);
        }
    };
    const auto clearAllIssues = [this]() {
        if (viewer_ == nullptr || viewer_->inspectionIssues().isEmpty()) {
            return;
        }

        viewer_->clearInspectionIssues();
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Inspection issues cleared."), 2500);
    };
    const auto exportIssuesCsv = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Issues CSV"),
            QStringLiteral("inspection_issues.csv"),
            tr("CSV Files (*.csv)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        if (!InspectionReportExporter::exportIssuesCsv(filePath, viewer_->inspectionIssues(), &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }
        showUserMessage(LogLevel::Info, tr("Issue CSV exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    };
    const auto exportInspectionReport = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Inspection Report"),
            QStringLiteral("inspection_report.html"),
            tr("HTML Files (*.html)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        const QString projectName = currentProjectFilePath_.isEmpty()
            ? tr("Current Project")
            : QFileInfo(currentProjectFilePath_).completeBaseName();
        if (!InspectionReportExporter::exportProjectHtml(
                filePath,
                projectName,
                viewer_->currentFilePaths(),
                viewer_->towerMarkers(),
                viewer_->inspectionIssues(),
                &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        showUserMessage(LogLevel::Info, tr("Inspection report exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    };
    const auto commitIssueDetails = [this]() {
        if (updatingIssueDetails_ || viewer_ == nullptr) {
            return;
        }

        const int selectedIssueIndex = viewer_->selectedIssueIndex();
        if (selectedIssueIndex < 0 || selectedIssueIndex >= viewer_->inspectionIssues().size()) {
            return;
        }

        InspectionIssue issue = viewer_->inspectionIssues().at(selectedIssueIndex);
        issue.title = issueTitleEdit_->text().trimmed();
        issue.category = issueCategoryComboBox_->currentText().trimmed();
        issue.severity = static_cast<IssueSeverity>(issueSeverityComboBox_->currentIndex());
        issue.status = static_cast<IssueStatus>(issueStatusComboBox_->currentIndex());
        issue.relatedTowerIndex = issueRelatedTowerComboBox_->currentData().toInt();
        issue.relatedTowerName = issueRelatedTowerComboBox_->currentText().trimmed();
        issue.imagePath = issueImagePathEdit_->text().trimmed();
        issue.description = issueDescriptionEdit_->toPlainText().trimmed();
        if (viewer_->updateInspectionIssue(selectedIssueIndex, issue)) {
            updateIssuePanel();
        }
    };

    issueController_ = new IssueController(
        startIssueMarkAction_,
        cancelIssueToolAction_,
        focusIssueAction_,
        removeIssueAction_,
        clearIssuesAction_,
        exportIssuesCsvAction_,
        exportInspectionReportAction_,
        issueTableWidget_,
        issueTitleEdit_,
        issueCategoryComboBox_,
        issueSeverityComboBox_,
        issueStatusComboBox_,
        issueRelatedTowerComboBox_,
        issueImagePathEdit_,
        issueDescriptionEdit_,
        beginIssueMarking,
        cancelIssueTool,
        focusSelectedIssue,
        removeSelectedIssue,
        clearAllIssues,
        exportIssuesCsv,
        exportInspectionReport,
        [this](int currentRow) {
            if (viewer_ != nullptr) {
                viewer_->setSelectedIssueIndex(currentRow);
            }
            updateActionState();
            updateIssuePanel();
        },
        commitIssueDetails,
        this);

    visualizationPanelController_ = new VisualizationPanelController(
        viewer_,
        showAxesAction_,
        showBoundingBoxAction_,
        darkBackgroundAction_,
        lightBackgroundAction_,
        rgbColorAction_,
        elevationColorAction_,
        singleColorAction_,
        classificationColorAction_,
        pointSizeSlider_,
        pointSizeValueLabel_,
        pointOpacitySlider_,
        pointOpacityValueLabel_,
        depthCueSlider_,
        depthCueValueLabel_,
        edlStrengthSlider_,
        edlStrengthValueLabel_,
        colorModeComboBox_,
        pointColorButton_,
        backgroundColorButton_,
        [this]() { choosePointColor(); },
        [this]() { chooseBackgroundColor(); },
        this);

    connect(themeColorfulAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016Colorful); });
    connect(themeWhiteAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016White); });
    connect(themeDarkGrayAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016DarkGray); });

    profileClassificationController_ = new ProfileClassificationController(
        profileClassificationGroupBox_,
        viewer_,
        profileClassificationAction_,
        saveProfileClassificationEditsAction_,
        undoProfileClassificationAction_,
        redoProfileClassificationAction_,
        clearProfileClassificationEditsAction_,
        [this](int classificationCode) {
            return classificationDisplayName(classificationCode, classificationNameOverrides_);
        },
        this);

    QList<int> profileClassificationCodes;
    profileClassificationCodes.reserve(static_cast<int>(kClassificationDisplayItems.size()));
    for (const ClassificationDisplayItem& item : kClassificationDisplayItems) {
        if (item.code >= 0) {
            profileClassificationCodes.append(item.code);
        }
    }
    profileClassificationController_->initializeClassificationItems(profileClassificationCodes);

    connect(profileClassificationController_, &ProfileClassificationController::saveRequested, this, [this]() {
        saveProfileClassificationEditsToLas();
    });
    connect(profileClassificationController_, &ProfileClassificationController::modeChanged, this, [this](bool enabled) {
        if (enabled && profileClassificationDock_ != nullptr) {
            profileClassificationDock_->show();
            profileClassificationDock_->raise();
        }
        if (!enabled) {
            promptSaveProfileClassificationEditsIfNeeded();
        }
    });
    connect(profileClassificationController_, &ProfileClassificationController::editsDirtyChanged, this, [this](bool dirty) {
        classificationEditsDirty_ = dirty;
    });
    connect(profileClassificationController_, &ProfileClassificationController::stateChanged, this, [this]() {
        updateActionState();
    });

    connect(showProfileClassificationDockAction_, &QAction::toggled, this, [this](bool visible) {
        if (profileClassificationDock_ == nullptr) {
            return;
        }

        if (visible) {
            profileClassificationDock_->show();
            profileClassificationDock_->raise();
        } else if (profileClassificationDock_->isVisible()) {
            profileClassificationDock_->hide();
        }
    });
    connect(showProfileDockAction_, &QAction::toggled, this, [this](bool visible) {
        if (profileDock_ == nullptr) {
            return;
        }

        if (visible) {
            profileDock_->show();
            profileDock_->raise();
        } else if (profileDock_->isVisible()) {
            profileDock_->hide();
        }
    });
    connect(showWebPanelAction_, &QAction::toggled, this, [this](bool visible) {
        if (webPageDock_ == nullptr) {
            return;
        }

        if (visible) {
            webPageDock_->show();
            webPageDock_->raise();
        } else if (webPageDock_->isVisible()) {
            webPageDock_->hide();
        }
    });
}

void MainWindow::createWindowAndViewerConnections()
{
    connect(languageEnglishAction_, &QAction::triggered, this, [this]() { applyLanguage(UiLanguage::English); });
    connect(languageChineseAction_, &QAction::triggered, this, [this]() { applyLanguage(UiLanguage::Chinese); });

    if (logDock_ != nullptr) {
        connect(logDock_, &ApplicationLogDock::filterStateChanged, this, [this]() {
            persistWindowSettings();
        });
        connect(logDock_, &ApplicationLogDock::autoScrollToggled, this, [this](bool) {
            persistWindowSettings();
        });
        connect(logDock_, &ApplicationLogDock::entriesClearedByUser, this, [this]() {
            if (statusBar() != nullptr) {
                statusBar()->showMessage(tr("Log entries cleared."), 2500);
            }
        });
        connect(logDock_, &ApplicationLogDock::exportRequested, this, [this]() {
            exportLogEntries();
        });
    }

    if (logDock_ != nullptr) {
        auto* focusLogSearchShortcut = new QShortcut(QKeySequence::Find, this);
        focusLogSearchShortcut->setContext(Qt::WindowShortcut);
        connect(focusLogSearchShortcut, &QShortcut::activated, this, [this]() {
            if (logDock_ == nullptr || logDock_->searchLineEdit() == nullptr) {
                return;
            }

            if (!logDock_->isVisible()) {
                if (showLogAction_ != nullptr) {
                    showLogAction_->setChecked(true);
                } else {
                    logDock_->show();
                }
            }

            logDock_->raise();
            logDock_->searchLineEdit()->setFocus(Qt::ShortcutFocusReason);
            logDock_->searchLineEdit()->selectAll();
        });

        auto* clearLogSearchShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), logDock_);
        clearLogSearchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(clearLogSearchShortcut, &QShortcut::activated, this, [this]() {
            if (logDock_ == nullptr || logDock_->searchLineEdit() == nullptr) {
                return;
            }

            if (!logDock_->searchLineEdit()->text().isEmpty()) {
                logDock_->searchLineEdit()->clear();
            } else if (logDock_->searchLineEdit()->hasFocus()) {
                logDock_->searchLineEdit()->clearFocus();
            }
        });
    }

    connect(showLogAction_, &QAction::toggled, this, [this](bool visible) {
        if (logDock_ != nullptr) {
            if (visible) {
                logDock_->show();
                logDock_->raise();
                resizeDocks({ logDock_ }, { 280 }, Qt::Vertical);
                logDock_->refreshEntries();
            } else {
                logDock_->hide();
            }
            persistWindowSettings();
        }
    });
    connect(logDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (closingWindow_) {
            return;
        }
        if (showLogAction_ != nullptr && showLogAction_->isChecked() != visible) {
            showLogAction_->setChecked(visible);
        }
        scheduleDockPanelSizing();
        persistWindowSettings();
    });
    connect(profileDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (closingWindow_) {
            return;
        }
        if (showProfileDockAction_ != nullptr && showProfileDockAction_->isChecked() != visible) {
            showProfileDockAction_->setChecked(visible);
        }
        scheduleDockPanelSizing();
        persistWindowSettings();
    });
    connect(profileClassificationDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (closingWindow_) {
            return;
        }
        if (showProfileClassificationDockAction_ != nullptr && showProfileClassificationDockAction_->isChecked() != visible) {
            const QSignalBlocker blocker(showProfileClassificationDockAction_);
            showProfileClassificationDockAction_->setChecked(visible);
        }
        scheduleDockPanelSizing();
        persistWindowSettings();
    });
    connect(routeDetailsDock_, &QDockWidget::visibilityChanged, this, [this](bool) {
        if (closingWindow_) {
            return;
        }
        scheduleDockPanelSizing();
        persistWindowSettings();
    });
    connect(webPageDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (closingWindow_) {
            return;
        }
        if (showWebPanelAction_ != nullptr && showWebPanelAction_->isChecked() != visible) {
            const QSignalBlocker blocker(showWebPanelAction_);
            showWebPanelAction_->setChecked(visible);
        }
        scheduleDockPanelSizing();
        persistWindowSettings();
    });

    const auto persistDockState = [this]() {
        scheduleDockPanelSizing();
        persistWindowSettings();
    };
    connect(projectDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(inspectorDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(profileClassificationDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(profileDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(webPageDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(logDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(projectDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(inspectorDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(profileClassificationDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(profileDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(webPageDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(logDock_, &QDockWidget::topLevelChanged, this, persistDockState);

    if (inspectorTabWidget_ != nullptr) {
        connect(inspectorTabWidget_, &QTabWidget::currentChanged, this, [this](int) {
            persistWindowSettings();
        });
    }
    if (routeDetailsTabWidget_ != nullptr) {
        connect(routeDetailsTabWidget_, &QTabWidget::currentChanged, this, [this](int) {
            persistWindowSettings();
        });
    }

    if (routeWaypointLabelModeComboBox_ != nullptr) {
        connect(routeWaypointLabelModeComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (viewer_ == nullptr || routeWaypointLabelModeComboBox_ == nullptr || index < 0) {
                return;
            }

            viewer_->setInspectionRouteWaypointLabelDisplayMode(static_cast<RouteLabelDisplayMode>(
                routeWaypointLabelModeComboBox_->itemData(index).toInt()));
            persistWindowSettings();
        });
    }
    if (routePartLabelModeComboBox_ != nullptr) {
        connect(routePartLabelModeComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (viewer_ == nullptr || routePartLabelModeComboBox_ == nullptr || index < 0) {
                return;
            }

            viewer_->setInspectionRoutePartLabelDisplayMode(static_cast<RouteLabelDisplayMode>(
                routePartLabelModeComboBox_->itemData(index).toInt()));
            persistWindowSettings();
        });
    }

    if (routeWaypointShowCoordinatesCheckBox_ != nullptr) {
        connect(routeWaypointShowCoordinatesCheckBox_, &QCheckBox::toggled, this, [this](bool) {
            applyRouteWaypointTableColumnVisibility();
            persistWindowSettings();
        });
    }
    if (routeWaypointShowCaptureAnglesCheckBox_ != nullptr) {
        connect(routeWaypointShowCaptureAnglesCheckBox_, &QCheckBox::toggled, this, [this](bool) {
            applyRouteWaypointTableColumnVisibility();
            persistWindowSettings();
        });
    }
    if (routePartShowCoordinatesCheckBox_ != nullptr) {
        connect(routePartShowCoordinatesCheckBox_, &QCheckBox::toggled, this, [this](bool) {
            applyRoutePartTableColumnVisibility();
            persistWindowSettings();
        });
    }
    if (routePartShowCaptureAnglesCheckBox_ != nullptr) {
        connect(routePartShowCaptureAnglesCheckBox_, &QCheckBox::toggled, this, [this](bool) {
            applyRoutePartTableColumnVisibility();
            persistWindowSettings();
        });
    }
    if (routeWaypointColorButton_ != nullptr) {
        connect(routeWaypointColorButton_, &QPushButton::clicked, this, [this]() { chooseRouteWaypointColor(); });
    }
    if (routePartPointColorButton_ != nullptr) {
        connect(routePartPointColorButton_, &QPushButton::clicked, this, [this]() { chooseRoutePartPointColor(); });
    }
    if (routeTrajectoryColorButton_ != nullptr) {
        connect(routeTrajectoryColorButton_, &QPushButton::clicked, this, [this]() { chooseRouteTrajectoryColor(); });
    }

    if (routePartPointsTableWidget_ != nullptr) {
        routePartPointsTableWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(routePartPointsTableWidget_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            if (routePartPointsTableWidget_ == nullptr) {
                return;
            }

            const QModelIndex index = routePartPointsTableWidget_->indexAt(pos);
            if (index.isValid()) {
                routePartPointsTableWidget_->setCurrentCell(index.row(), kRoutePartColumnPartName);
            }

            QMenu menu(routePartPointsTableWidget_);
            QAction* focusAction = menu.addAction(tr("Focus Part Point"));
            QAction* removeAction = menu.addAction(tr("Delete Part Point"));
            removeAction->setEnabled(selectedRoutePartIndex_ > 0 && routeEditingEnabled_);
            QAction* chosenAction = menu.exec(routePartPointsTableWidget_->viewport()->mapToGlobal(pos));
            if (chosenAction == focusAction) {
                focusRoutePartPoint(selectedRoutePartIndex_);
            } else if (chosenAction == removeAction) {
                removeRoutePartPoint(selectedRoutePartIndex_, true);
            }
        });
        connect(routePartPointsTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
            if (updatingRouteTables_ || routePartPointsTableWidget_ == nullptr) {
                return;
            }

            if (currentRow >= 0 && currentRow < routePartPointsTableWidget_->rowCount()) {
                QTableWidgetItem* indexItem = routePartPointsTableWidget_->item(currentRow, 0);
                selectedRoutePartIndex_ = indexItem != nullptr ? indexItem->data(Qt::UserRole).toInt() : -1;
            } else {
                selectedRoutePartIndex_ = -1;
            }
        });
        connect(routePartPointsTableWidget_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            if (routePartPointsTableWidget_ == nullptr || row < 0 || row >= routePartPointsTableWidget_->rowCount()) {
                return;
            }

            QTableWidgetItem* indexItem = routePartPointsTableWidget_->item(row, 0);
            focusRoutePartPoint(indexItem != nullptr ? indexItem->data(Qt::UserRole).toInt() : -1);
        });
    }

    if (routeWaypointsTableWidget_ != nullptr) {
        routeWaypointsTableWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(routeWaypointsTableWidget_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            if (routeWaypointsTableWidget_ == nullptr) {
                return;
            }

            const QModelIndex index = routeWaypointsTableWidget_->indexAt(pos);
            const int contextWaypointIndex = index.isValid() ? index.row() : selectedRouteWaypointIndex_;
            if (index.isValid()) {
                routeWaypointsTableWidget_->setCurrentCell(index.row(), kRouteWaypointColumnPart);
            }

            QMenu menu(routeWaypointsTableWidget_);
            QAction* editAction = menu.addAction(tr("Edit Waypoint"));
            QAction* focusAction = menu.addAction(tr("Focus Waypoint"));
            QAction* removeAction = menu.addAction(tr("Delete Waypoint"));
            editAction->setEnabled(contextWaypointIndex >= 0 && contextWaypointIndex < currentPowerlineRoute_.waypoints.size());
            focusAction->setEnabled(contextWaypointIndex >= 0 && contextWaypointIndex < currentPowerlineRoute_.waypoints.size());
            removeAction->setEnabled(
                contextWaypointIndex >= 0
                && contextWaypointIndex < currentPowerlineRoute_.waypoints.size()
                && routeEditingEnabled_);
            QAction* chosenAction = menu.exec(routeWaypointsTableWidget_->viewport()->mapToGlobal(pos));
            if (chosenAction == editAction) {
                editRouteWaypoint(contextWaypointIndex);
            } else if (chosenAction == focusAction) {
                focusRouteWaypoint(contextWaypointIndex);
            } else if (chosenAction == removeAction) {
                removeRouteWaypoint(contextWaypointIndex, true);
            }
        });
        connect(routeWaypointsTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
            if (updatingRouteTables_) {
                return;
            }

            selectedRouteWaypointIndex_ =
                (currentRow >= 0 && currentRow < currentPowerlineRoute_.waypoints.size())
                    ? currentRow
                    : -1;
            if (selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < currentPowerlineRoute_.waypoints.size()) {
                const int targetCount = currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).captureTargets.size();
                selectedRouteWaypointTargetIndex_ =
                    targetCount > 0 ? std::clamp(selectedRouteWaypointTargetIndex_, 0, targetCount - 1) : -1;
            } else {
                selectedRouteWaypointTargetIndex_ = -1;
            }
            if (viewer_ != nullptr) {
                viewer_->setSelectedInspectionRouteWaypointIndex(selectedRouteWaypointIndex_);
                viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
            }

            if (routePartPointsTableWidget_ != nullptr) {
                const int partIndex =
                    selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < currentPowerlineRoute_.waypoints.size()
                    ? (!currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).captureTargets.isEmpty()
                            && currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).captureTargets.first().partIndex > 0
                        ? currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).captureTargets.first().partIndex
                        : currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).primaryPartIndex)
                    : -1;
                if (partIndex > 0) {
                    for (int row = 0; row < routePartPointsTableWidget_->rowCount(); ++row) {
                        QTableWidgetItem* item = routePartPointsTableWidget_->item(row, 0);
                        if (item != nullptr && item->data(Qt::UserRole).toInt() == partIndex) {
                            routePartPointsTableWidget_->setCurrentCell(row, kRoutePartColumnPartName);
                            break;
                        }
                    }
                } else {
                    selectedRoutePartIndex_ = -1;
                    routePartPointsTableWidget_->clearSelection();
                }
            }

            updateRoutePlanningPanel();
            updateActionState();
        });
        connect(routeWaypointsTableWidget_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            focusRouteWaypoint(row);
        });
    }

    if (routeWaypointTargetsTableWidget_ != nullptr) {
        connect(routeWaypointTargetsTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
            if (updatingRouteTables_ || selectedRouteWaypointIndex_ < 0 || selectedRouteWaypointIndex_ >= currentPowerlineRoute_.waypoints.size()) {
                return;
            }

            const int targetCount = currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).captureTargets.size();
            selectedRouteWaypointTargetIndex_ =
                (currentRow >= 0 && currentRow < targetCount) ? currentRow : -1;
            if (viewer_ != nullptr) {
                viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
            }
            updateActionState();
        });
    }

    if (routeQaIssuesTableWidget_ != nullptr) {
        connect(routeQaIssuesTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
            if (updatingRouteTables_ || routeQaIssuesTableWidget_ == nullptr) {
                return;
            }

            selectedRouteQaIssueIndex_ =
                (currentRow >= 0 && currentRow < routeQaReport_.issues.size())
                    ? currentRow
                    : -1;
        });
        connect(routeQaIssuesTableWidget_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            focusRouteQaIssue(row);
        });
    }

    if (resetClassificationColorsButton_ != nullptr) {
        connect(resetClassificationColorsButton_, &QPushButton::clicked, this, [this]() {
            if (viewer_ == nullptr) {
                return;
            }

            classificationNameOverrides_.clear();
            viewer_->resetClassificationColors();
            updateClassificationColorTable();
            updateProfileClassificationPanel();
        });
    }
    if (classificationColorsTableWidget_ != nullptr) {
        connect(classificationColorsTableWidget_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
            if (viewer_ == nullptr || item == nullptr || updatingClassificationColorTable_) {
                return;
            }

            const int classificationCode = item->data(Qt::UserRole).toInt();
            if (item->column() == 0) {
                viewer_->setClassificationVisible(classificationCode, item->checkState() == Qt::Checked);
                return;
            }

            if (item->column() != 2) {
                return;
            }

            const QString trimmedName = item->text().trimmed();
            const QString defaultName = defaultClassificationDisplayName(classificationCode);
            if (trimmedName.isEmpty() || trimmedName == defaultName) {
                classificationNameOverrides_.remove(classificationCode);
            } else {
                classificationNameOverrides_.insert(classificationCode, trimmedName);
            }
            persistVisualizationSettings();
            updateClassificationColorTable();
            updateProfileClassificationPanel();
        });
        connect(classificationColorsTableWidget_, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
            if (viewer_ == nullptr || classificationColorsTableWidget_ == nullptr || updatingClassificationColorTable_) {
                return;
            }
            if (column != 3) {
                return;
            }

            QTableWidgetItem* colorItem = classificationColorsTableWidget_->item(row, 3);
            if (colorItem == nullptr) {
                return;
            }

            const int classificationCode = colorItem->data(Qt::UserRole).toInt();
            const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();
            const QColor currentColor = classificationCode < 0
                ? options.classificationFallbackColor
                : options.classificationColors.value(classificationCode, options.classificationFallbackColor);
            const QColor selectedColor = showStyledColorDialog(this, currentColor, tr("Choose Classification Color"));
            if (!selectedColor.isValid()) {
                return;
            }

            if (classificationCode < 0) {
                viewer_->setClassificationFallbackColor(selectedColor);
            } else {
                viewer_->setClassificationColor(classificationCode, selectedColor);
            }

            if (viewer_->visualizationOptions().colorMode != PointCloudColorMode::Classification) {
                viewer_->setColorMode(PointCloudColorMode::Classification);
            }
            updateClassificationColorTable();
        });
    }

    if (roundSplatsCheckBox_ != nullptr) {
        connect(roundSplatsCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setUseRoundSplats);
    }
    if (axesCheckBox_ != nullptr) {
        connect(axesCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setShowAxes);
    }
    if (boundingBoxCheckBox_ != nullptr) {
        connect(boundingBoxCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setShowBoundingBox);
    }
    if (invertOrbitCheckBox_ != nullptr) {
        connect(invertOrbitCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertOrbitDrag);
    }
    if (invertPanCheckBox_ != nullptr) {
        connect(invertPanCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertPanDrag);
    }
    if (invertWheelCheckBox_ != nullptr) {
        connect(invertWheelCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertWheelZoom);
    }
    if (wheelZoomSensitivitySlider_ != nullptr) {
        connect(wheelZoomSensitivitySlider_, &QSlider::sliderMoved, this, [this](int value) {
            if (wheelZoomSensitivityValueLabel_ != nullptr) {
                wheelZoomSensitivityValueLabel_->setText(tr("%1%").arg(QLocale().toString(value)));
            }
        });
        connect(wheelZoomSensitivitySlider_, &QSlider::valueChanged, this, [this](int value) {
            if (wheelZoomSensitivityValueLabel_ != nullptr) {
                wheelZoomSensitivityValueLabel_->setText(tr("%1%").arg(QLocale().toString(value)));
            }
            viewer_->setWheelZoomSensitivityPercent(value);
        });
    }

    connect(viewer_, &PointCloudViewer::selectedInspectionRouteWaypointChanged, this, [this](int index) {
        selectedRouteWaypointIndex_ = index;
        if (viewer_ != nullptr) {
            selectedRouteWaypointTargetIndex_ = viewer_->selectedInspectionRouteWaypointTargetIndex();
        }
        updateRoutePlanningPanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::inspectionRouteWaypointDoubleClicked, this, [this](int index) {
        editRouteWaypoint(index);
    });
    connect(viewer_, &PointCloudViewer::inspectionRouteWaypointDragFinished, this, [this](int index, const PointRecord& point) {
        if (!ensureRouteEditingEnabled(true)) {
            applyCurrentRouteToViewer();
            updateRoutePlanningPanel();
            return;
        }

        if (index < 0 || index >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        RouteWaypoint& waypoint = currentPowerlineRoute_.waypoints[index];
        waypoint.localPoint = point;
        waypoint.dh = point.z;
        waypoint.height = point.z;

        QString geographicWarning;
        if (projectCoordinateSystems_.pointCloudCrs.code > 0 && projectCoordinateSystems_.geographicCrs.code > 0) {
            QPointF geographicPoint;
            QString errorMessage;
            if (CrsTransformService::transformPoint(
                    projectCoordinateSystems_.pointCloudCrs,
                    projectCoordinateSystems_.geographicCrs,
                    QPointF(point.x, point.y),
                    &geographicPoint,
                    &errorMessage)) {
                waypoint.longitude = geographicPoint.x();
                waypoint.latitude = geographicPoint.y();
            } else {
                geographicWarning = errorMessage.isEmpty()
                    ? tr("Waypoint local position was updated, but geographic coordinates could not be synchronized.")
                    : errorMessage;
            }
        }

        selectedRouteWaypointIndex_ = index;
        selectedRouteWaypointTargetIndex_ = waypoint.captureTargets.isEmpty()
            ? -1
            : std::clamp(selectedRouteWaypointTargetIndex_, 0, waypoint.captureTargets.size() - 1);
        applyCurrentRouteToViewer();
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();

        if (!geographicWarning.isEmpty()) {
            showUserMessage(LogLevel::Warning, geographicWarning, 4500);
        } else {
            showUserMessage(
                LogLevel::Info,
                tr("Updated route waypoint #%1.").arg(QLocale().toString(index + 1)),
                2200);
        }
    });

    connect(viewer_, &PointCloudViewer::pointCloudLoadingStarted, this, [this](const QString& message) {
        beginOperationProgress(message);
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoadingProgress, this, [this](const QString& message, int value, int maximum) {
        updateOperationProgress(message, value, maximum);
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoadingFinished, this, [this]() {
        endOperationProgress();
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoaded, this, [this]() {
        endOperationProgress();
        classificationEditsDirty_ = false;
        rebuildProjectTree();
        syncUiFromViewer();
    });
    connect(viewer_, &PointCloudViewer::pointCloudCleared, this, [this]() {
        endOperationProgress();
        classificationEditsDirty_ = false;
        linkedTowerFilePath_.clear();
        linkedRouteFilePath_.clear();
        setTowerEditingEnabled(false);
        currentPowerlineRoute_ = PowerlineRouteDocument();
        selectedRouteWaypointIndex_ = -1;
        selectedRouteWaypointTargetIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        showUserMessage(LogLevel::Info, tr("Scene cleared."), 3000);
    });
    connect(viewer_, &PointCloudViewer::visualizationOptionsChanged, this, [this]() { syncUiFromViewer(); });
    connect(viewer_, &PointCloudViewer::visualizationOptionsChanged, this, [this]() { persistVisualizationSettings(); });
    connect(viewer_, &PointCloudViewer::interactionOptionsChanged, this, [this]() {
        persistInteractionSettings();
        syncUiFromViewer();
        updateNavigationHelpText();
        showUserMessage(LogLevel::Info, tr("Navigation preferences updated."), 2500);
    });
    connect(viewer_, &PointCloudViewer::measurementChanged, this, [this]() {
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        syncUiFromViewer();
        updateMeasurementPanel();
    });
    connect(viewer_, &PointCloudViewer::measurementModeChanged, this, [this]() {
        if (!viewer_->measurementEnabled()) {
            vegetationRiskResults_.clear();
            selectedVegetationRiskIndex_ = -1;
        }
        syncProfileDockForMeasurementMode(viewer_->measurementEnabled());
        syncUiFromViewer();
        updateMeasurementPanel();
    });
    connect(viewer_, &PointCloudViewer::towerMarkersChanged, this, [this]() {
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        currentPowerlineRoute_ = PowerlineRouteDocument();
        linkedRouteFilePath_.clear();
        selectedRouteWaypointIndex_ = -1;
        selectedRouteWaypointTargetIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        updateMeasurementPanel();
        updateTowerPanel();
        updateVegetationRiskPanel();
    });
    connect(viewer_, &PointCloudViewer::selectedTowerChanged, this, [this](int index) {
        if (towerTableWidget_ != nullptr && towerTableWidget_->currentRow() != index) {
            const QSignalBlocker blocker(towerTableWidget_);
            if (index >= 0) {
                towerTableWidget_->setCurrentCell(index, 1);
            } else {
                towerTableWidget_->clearSelection();
                towerTableWidget_->setCurrentItem(nullptr);
            }
        }
        updateActionState();
        updateMeasurementPanel();
        updateTowerPanel();
    });
    connect(viewer_, &PointCloudViewer::towerEditModeChanged, this, [this]() {
        updateTowerPanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::towerEditRequested, this, [this](const PointRecord& point, int modeValue, int targetIndex) {
        const TowerEditMode mode = static_cast<TowerEditMode>(modeValue);
        if (viewer_ == nullptr) {
            return;
        }

        if (mode == TowerEditMode::MoveSelected) {
            if (viewer_->moveTowerMarker(targetIndex, point)) {
                updateTowerPanel();
                showUserMessage(LogLevel::Info, tr("Tower marker moved."), 2500);
            }
            return;
        }

        const QString towerName = nextDefaultTowerName();

        const bool inserted = mode == TowerEditMode::InsertBeforeSelected
            ? viewer_->insertTowerMarker(targetIndex, towerName, point)
            : viewer_->addTowerMarker(towerName, point);
        if (!inserted) {
            showUserMessage(LogLevel::Warning, tr("Tower marker name cannot be empty."), 3000);
            return;
        }

        updateTowerPanel();
        showUserMessage(
            LogLevel::Info,
            mode == TowerEditMode::AddAfterLast
                ? tr("Tower marker added. Continue clicking points to add more, or cancel the tool when finished.")
                : tr("Tower marker added."),
            mode == TowerEditMode::AddAfterLast ? 3500 : 2500);
    });
    connect(viewer_, &PointCloudViewer::inspectionIssuesChanged, this, [this]() {
        rebuildProjectTree();
        syncUiFromViewer();
        updateMeasurementPanel();
        updateIssuePanel();
        updateVegetationRiskPanel();
    });
    connect(viewer_, &PointCloudViewer::selectedIssueChanged, this, [this](int index) {
        if (issueTableWidget_ != nullptr && issueTableWidget_->currentRow() != index) {
            const QSignalBlocker blocker(issueTableWidget_);
            if (index >= 0) {
                issueTableWidget_->setCurrentCell(index, 1);
            } else {
                issueTableWidget_->clearSelection();
                issueTableWidget_->setCurrentItem(nullptr);
            }
        }
        updateActionState();
        updateMeasurementPanel();
        updateIssuePanel();
    });
    connect(viewer_, &PointCloudViewer::issueEditModeChanged, this, [this]() {
        updateIssuePanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::issueEditRequested, this, [this](const PointRecord& point) {
        if (viewer_ == nullptr) {
            return;
        }

        InspectionIssue issue;
        issue.id = issueDefaultId();
        issue.title = nextDefaultIssueTitle();
        issue.category = tr("Other");
        issue.severity = IssueSeverity::Major;
        issue.status = IssueStatus::Open;
        issue.point = point;
        issue.relatedTowerIndex = viewer_->selectedTowerIndex();
        if (issue.relatedTowerIndex >= 0 && issue.relatedTowerIndex < viewer_->towerMarkers().size()) {
            issue.relatedTowerName = viewer_->towerMarkers().at(issue.relatedTowerIndex).name;
        }
        issue.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
        if (viewer_->addInspectionIssue(issue)) {
            if (inspectorTabWidget_ != nullptr) {
                inspectorTabWidget_->setCurrentIndex(2);
            }
            updateIssuePanel();
            showUserMessage(LogLevel::Info, tr("Inspection issue added. Continue clicking points to add more, or right-click to cancel."), 3500);
        }
    });
    connect(viewer_, &PointCloudViewer::measurementMessage, this, [this](const QString& message, bool error) {
        showUserMessage(error ? LogLevel::Error : LogLevel::Info, message, error ? 4000 : 3000);
    });
    viewer_->setClipActiveDatasetPath(selectedDatasetPath());
    viewer_->setClipKeepInside(clipToggleInsideAction_ == nullptr || clipToggleInsideAction_->isChecked());

    connect(clipModeNoneAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }
        viewer_->clearClip();
    });
    connect(clipModeBoxAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }
        viewer_->setClipActiveDatasetPath(selectedDatasetPath());
        viewer_->beginBoxClip();
    });
    connect(clipModePolygonAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }
        viewer_->setClipActiveDatasetPath(selectedDatasetPath());
        viewer_->beginPolygonClip();
    });
    connect(clipBoxWorldAlignedAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }
        viewer_->setClipBoxAlignment(ClipRegion::WorldAligned);
    });
    connect(clipBoxViewAlignedAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }
        viewer_->setClipBoxAlignment(ClipRegion::ViewAligned);
    });
    connect(clipScopeActiveDatasetAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }
        viewer_->setClipActiveDatasetPath(selectedDatasetPath());
        viewer_->setClipScope(ClipRegion::ActiveDataset);
    });
    connect(clipScopeVisibleDatasetsAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }
        viewer_->setClipScope(ClipRegion::VisibleDatasets);
    });
    connect(clipToggleInsideAction_, &QAction::toggled, this, [this](bool checked) {
        if (viewer_ == nullptr) {
            return;
        }
        viewer_->setClipKeepInside(checked);
    });

    connect(clipApplyExportAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }
        viewer_->setClipActiveDatasetPath(selectedDatasetPath());
        beginOperationProgress(tr("Preparing clipped export..."));
        QString filterError;
        auto filteredData = viewer_->buildClipExportData(selectedDatasetPath(), &filterError);
        endOperationProgress();
        if (filteredData == nullptr) {
            showUserMessage(LogLevel::Warning, filterError.isEmpty() ? tr("No active clip region to apply.") : filterError, 3000);
            return;
        }
        if (filteredData->empty()) {
            showUserMessage(LogLevel::Warning, tr("Clip produced an empty result."), 3000);
            return;
        }
        const QString exportPath = resolveCaptureOutputPath(
            tr("Save Clipped Point Cloud"),
            QStringLiteral("clipped.las"),
            tr("LAS Point Cloud (*.las)"),
            QStringLiteral("las"));
        if (exportPath.isEmpty()) {
            return;
        }

        QString writeError;
        if (!LasWriter().write(exportPath, *filteredData, &writeError)) {
            showUserMessage(LogLevel::Error, tr("Export failed: %1").arg(writeError), 5000);
            return;
        }

        appendPointCloudFiles({ exportPath });
        showUserMessage(LogLevel::Info, tr("Clip export complete. %1 points written to %2")
            .arg(filteredData->size()).arg(exportPath), 5000);
    });
}
