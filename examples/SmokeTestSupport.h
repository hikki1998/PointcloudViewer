#pragma once

#include <functional>
#include <string>

#include <QColor>
#include <QList>
#include <QPoint>
#include <QStringList>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "gui/PointCloudViewer.h"
#include "route/PowerlineRouteTypes.h"

class QImage;
class QPushButton;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
namespace Qtitan { class RibbonBar; }

void pumpEvents(int durationMs);
bool verify(bool condition, const std::string& message);
bool verifyClose(double left, double right, double tolerance, const std::string& message);
bool invokeTableContextMenuAndClose(QTableWidget* table, const QPoint& position, const std::string& message);
bool invokeTreeContextMenuAndClose(QTreeWidget* tree, const QPoint& position, const std::string& message);
bool emitTableDoubleClick(QTableWidget* table, int row, int column, const std::string& message);
bool emitTreeItemDoubleClick(QTreeWidget* tree, QTreeWidgetItem* item, int column, const std::string& message);
bool clickColorButtonAndAccept(QPushButton* button, const QColor& color, const std::string& message);
QTreeWidgetItem* findProjectTreeItem(QTreeWidget* tree, const std::function<bool(QTreeWidgetItem*)>& predicate);
InspectionRouteDisplayData buildSmokeRouteDisplayData(const PowerlineRouteDocument& route);
bool hasVisiblePixels(const QImage& image, int* nonBackgroundPixelCount);
QList<PointRecord> buildSyntheticWaypoints();
bool verifyRouteRoundTripShape(const PowerlineRouteDocument& original, const PowerlineRouteDocument& roundTripped);
PowerlineRouteDocument buildSyntheticRoute();
void normalizeTowerIndices(QList<TowerRecord>* towers);
QString resolveProjectPath(const QString& projectFilePath, const QString& storedPath);
#ifdef Q_OS_WIN
HWND findVisibleProcessTopLevelWindow(const QString& expectedTitle);
bool verifyWindowHasResizeFrame(HWND hwnd, const std::string& message);
bool verifyWindowUsesWorkArea(HWND hwnd, const std::string& message);
QPoint findCaptionHitPoint(HWND hwnd, Qtitan::RibbonBar* ribbonBar);
#endif

bool runViewerRenderSmoke(const QStringList& filePaths);
bool runMainBackstageSmoke(const QStringList& filePaths);
bool runMainWindowSettingsRestoreSmoke(const QStringList& filePaths);
bool runScreenRecordingSmoke(const QStringList& filePaths);
bool runLogPanelSmoke(const QStringList& filePaths);
bool runProjectExplorerDockSmoke(const QStringList& filePaths);
bool runProjectExplorerControllerSmoke(const QStringList& filePaths);
bool runProjectExplorerMainWindowSmoke(const QStringList& filePaths);
bool runProfileClassificationWidgetSmoke(const QStringList& filePaths);
bool runProfileClassificationControllerSmoke(const QStringList& filePaths);
bool runVisualizationPanelControllerSmoke(const QStringList& filePaths);
bool runMeasurementAnalysisControllerSmoke(const QStringList& filePaths);
bool runRouteControllerSmoke(const QStringList& filePaths);
bool runTowerControllerSmoke(const QStringList& filePaths);
bool runIssueControllerSmoke(const QStringList& filePaths);
bool runRouteRoamStateSmoke(const QStringList& filePaths);
bool runRouteJsonSmoke(const QStringList& filePaths);
bool runRouteInteropSmoke(const QStringList& filePaths);
bool runTowerFileInteropSmoke(const QStringList& filePaths);
bool runTowerProjectLinkSmoke(const QStringList& filePaths);
