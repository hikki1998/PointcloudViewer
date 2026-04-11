#pragma once

#include <QObject>

#include <functional>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class PointCloudViewer;
class QPushButton;
class QString;

class RouteController final : public QObject
{
    Q_OBJECT

public:
    using VoidCallback = std::function<void()>;
    using BoolCallback = std::function<void(bool)>;
    using IntCallback = std::function<void(int)>;
    using DoubleCallback = std::function<void(double)>;
    using RouteRoamPhotoCapturedCallback = std::function<void(int, int, const QString&, int)>;

    RouteController(
        PointCloudViewer* viewer,
        QAction* generateInspectionRouteAction,
        QAction* regenerateInspectionRouteAction,
        QAction* clearInspectionRouteAction,
        QAction* toggleRouteEditingAction,
        QAction* startInspectionRouteRoamAction,
        QAction* pauseInspectionRouteRoamAction,
        QAction* stopInspectionRouteRoamAction,
        QAction* focusRouteWaypointAction,
        QAction* importRouteFileAction,
        QAction* saveRouteFileAction,
        QAction* saveRouteFileAsAction,
        QAction* reloadRouteFileAction,
        QAction* importRouteKmlAction,
        QAction* exportRouteKmlAction,
        QAction* exportRouteDjiKmzAction,
        QPushButton* routeRoamStartButton,
        QPushButton* routeRoamPauseResumeButton,
        QPushButton* routeRoamStopButton,
        QDoubleSpinBox* routeRoamSpeedSpinBox,
        QComboBox* routeRoamViewModeComboBox,
        VoidCallback regenerateInspectionRoute,
        VoidCallback clearInspectionRoute,
        BoolCallback setRouteEditingEnabled,
        VoidCallback startInspectionRouteRoam,
        VoidCallback pauseResumeInspectionRouteRoam,
        VoidCallback stopInspectionRouteRoam,
        DoubleCallback routeRoamSpeedChanged,
        IntCallback routeRoamViewModeChanged,
        VoidCallback focusRouteWaypoint,
        VoidCallback importRouteFile,
        VoidCallback saveRouteFile,
        VoidCallback saveRouteFileAs,
        VoidCallback reloadRouteFile,
        VoidCallback importRouteKml,
        VoidCallback exportRouteKml,
        VoidCallback exportRouteDjiKmz,
        VoidCallback inspectionRouteRoamStateChanged,
        RouteRoamPhotoCapturedCallback inspectionRouteRoamPhotoCaptured,
        QObject* parent = nullptr);

private:
    PointCloudViewer* viewer_ = nullptr;
};
