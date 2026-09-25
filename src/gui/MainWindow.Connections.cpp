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
