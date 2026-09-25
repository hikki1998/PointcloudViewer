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
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoadingProgress, this, [this](const QString& message, int value, int maximum) {
        updateOperationProgress(message, value, maximum);
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoadingFinished, this, [this]() {
        endOperationProgress();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoadingFailed, this, [this](const QString& message) {
        endOperationProgress();
        showUserMessage(LogLevel::Error, message, 6000);
        syncUiFromViewer();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoaded, this, [this]() {
        endOperationProgress();
        classificationEditsDirty_ = false;
        rebuildProjectTree();
        syncUiFromViewer();
    });
    connect(viewer_, &PointCloudViewer::pointCloudCleared, this, [this]() {
        const bool replacingScene = viewer_->isPointCloudLoadingInProgress();
        if (!replacingScene) {
            endOperationProgress();
        }
        currentProjectFilePath_.clear();
        classificationEditsDirty_ = false;
        linkedTowerFilePath_.clear();
        linkedRouteFilePath_.clear();
        setTowerEditingEnabled(false);
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        currentPowerlineRoute_ = PowerlineRouteDocument();
        selectedRouteWaypointIndex_ = -1;
        selectedRouteWaypointTargetIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        if (!replacingScene) {
            showUserMessage(LogLevel::Info, tr("Scene cleared."), 3000);
        }
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
