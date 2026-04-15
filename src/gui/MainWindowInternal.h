#pragma once

#include "gui/MainWindow.h"

#include "crs/CrsTypes.h"

#include <array>
#include <QJsonObject>
#include <QString>
#include <QStringList>

class QDockWidget;
class QAction;
class QColor;
class QFrame;
class QTreeWidgetItem;
class QToolButton;
class QWidget;
template<typename Key, typename T>
class QMap;

namespace mainwindow_internal
{
inline constexpr int kWindowResizeBorder = 8;
inline constexpr int kProjectTreeItemTypeRole = Qt::UserRole;
inline constexpr int kProjectTreeFilePathRole = Qt::UserRole + 1;
inline constexpr int kProjectTreeIssueIndexRole = Qt::UserRole + 2;
inline constexpr int kProjectTreeRouteIndexRole = Qt::UserRole + 3;
inline constexpr int kRouteWaypointColumnPart = 1;
inline constexpr int kRouteWaypointColumnX = 2;
inline constexpr int kRouteWaypointColumnY = 3;
inline constexpr int kRouteWaypointColumnZ = 4;
inline constexpr int kRouteWaypointColumnAircraftYaw = 5;
inline constexpr int kRouteWaypointColumnGimbalPitch = 6;
inline constexpr int kRouteWaypointColumnCameraYaw = 7;
inline constexpr int kRouteWaypointColumnCameraPitch = 8;
inline constexpr int kRouteWaypointTargetColumnPart = 1;
inline constexpr int kRouteWaypointTargetColumnFocalRatio = 2;
inline constexpr int kRouteWaypointTargetColumnCameraYaw = 3;
inline constexpr int kRouteWaypointTargetColumnCameraPitch = 4;
inline constexpr int kRouteWaypointTargetColumnTargetPoint = 5;
inline constexpr int kRouteQaColumnSeverity = 0;
inline constexpr int kRouteQaColumnType = 1;
inline constexpr int kRouteQaColumnLocation = 2;
inline constexpr int kRouteQaColumnPart = 3;
inline constexpr int kRouteQaColumnDescription = 4;
inline constexpr int kRoutePartColumnPartName = 1;
inline constexpr int kRoutePartColumnHardware = 2;
inline constexpr int kRoutePartColumnPhase = 3;
inline constexpr int kRoutePartColumnCameraAngle = 4;
inline constexpr int kRoutePartColumnX = 5;
inline constexpr int kRoutePartColumnY = 6;
inline constexpr int kRoutePartColumnZ = 7;

struct ClassificationDisplayItem
{
    int code;
    const char* sourceText;
};

extern const std::array<ClassificationDisplayItem, 33> kClassificationDisplayItems;

MainWindow::UiLanguage defaultLanguageFromLocale();
bool isSupportedPointCloudFile(const QString& filePath);
void applyDefaultDockWidths(MainWindow* window, QDockWidget* projectDock, QDockWidget* inspectorDock);
QString formatCoordinateSystemCode(const lasviewer::crs::CoordinateSystemRef& crs, const QString& unsetText);
QString formatProjectCoordinateSystemsSummary(const lasviewer::crs::ProjectCoordinateSystems& coordinateSystems);
QString formatCoordinate(float value);
QString formatTriplet(float x, float y, float z);
QString routeTableStyleSheet();
QString normalizedProjectFilePath(const QString& filePath);
int indexOfProjectFilePath(const QStringList& filePaths, const QString& targetPath);
QStringList normalizedRecentProjectFiles(const QStringList& recentPaths, const QString& preferredPath = QString());
QString backstagePageStyleSheet();
QFrame* createBackstageCard(QWidget* parent = nullptr);
QToolButton* createBackstageActionButton(QAction* action, QWidget* parent = nullptr);
QColor showStyledColorDialog(QWidget* parent, const QColor& initialColor, const QString& title);
QJsonObject colorToJson(const QColor& color);
QColor colorFromJson(const QJsonObject& object, const QColor& fallback);
QJsonObject classificationColorMapToJson(const QMap<int, QColor>& colorMap);
QMap<int, QColor> classificationColorMapFromJson(const QJsonObject& object, const QMap<int, QColor>& fallback);
QJsonObject classificationVisibilityMapToJson(const QMap<int, bool>& visibilityMap);
QMap<int, bool> classificationVisibilityMapFromJson(const QJsonObject& object, const QMap<int, bool>& fallback);
QJsonObject classificationNameMapToJson(const QMap<int, QString>& nameMap);
QMap<int, QString> classificationNameMapFromJson(const QJsonObject& object);
QString languageCodeFor(MainWindow::UiLanguage language);
QString projectRelativePathFor(const QString& projectFilePath, const QString& targetFilePath);
QString resolveProjectPath(const QString& projectFilePath, const QString& storedPath);
void recordRecentProjectFilePath(const QString& filePath);
QString projectTreeItemType(const QTreeWidgetItem* item);
QString projectTreeItemFilePath(const QTreeWidgetItem* item);
QString defaultClassificationDisplayName(int classificationCode);
QString classificationDisplayName(int classificationCode, const QMap<int, QString>& customNames);
}
