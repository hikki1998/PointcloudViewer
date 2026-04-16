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
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointF>
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
#include "gui/support/UiHelpers.h"

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

    connect(fitSceneAction_, &QAction::triggered, viewer_, &PointCloudViewer::resetView);
    connect(topViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Top); });
    connect(frontViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Front); });
    connect(rightViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Right); });

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
        if (profileClassificationDock_ != nullptr && profileClassificationDock_->isVisible() != visible) {
            profileClassificationDock_->setVisible(visible);
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
        if (showLogAction_ != nullptr && showLogAction_->isChecked() != visible) {
            showLogAction_->setChecked(visible);
        }
        scheduleDockPanelSizing();
        persistWindowSettings();
    });
    connect(profileDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (showProfileDockAction_ != nullptr && showProfileDockAction_->isChecked() != visible) {
            showProfileDockAction_->setChecked(visible);
        }
        scheduleDockPanelSizing();
        persistWindowSettings();
    });
    connect(profileClassificationDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (showProfileClassificationDockAction_ != nullptr && showProfileClassificationDockAction_->isChecked() != visible) {
            const QSignalBlocker blocker(showProfileClassificationDockAction_);
            showProfileClassificationDockAction_->setChecked(visible);
        }
        scheduleDockPanelSizing();
        persistWindowSettings();
    });
    connect(routeDetailsDock_, &QDockWidget::visibilityChanged, this, [this](bool) {
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
    connect(logDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(projectDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(inspectorDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(profileClassificationDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(profileDock_, &QDockWidget::topLevelChanged, this, persistDockState);
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
}
