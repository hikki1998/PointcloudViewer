#include "gui/RouteController.h"

#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>

#include "gui/PointCloudViewer.h"

RouteController::RouteController(
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
    QObject* parent)
    : QObject(parent)
    , viewer_(viewer)
{
    if (generateInspectionRouteAction != nullptr) {
        connect(generateInspectionRouteAction, &QAction::triggered, this, [regenerateInspectionRoute]() {
            if (regenerateInspectionRoute) {
                regenerateInspectionRoute();
            }
        });
    }
    if (regenerateInspectionRouteAction != nullptr) {
        connect(regenerateInspectionRouteAction, &QAction::triggered, this, [regenerateInspectionRoute]() {
            if (regenerateInspectionRoute) {
                regenerateInspectionRoute();
            }
        });
    }
    if (clearInspectionRouteAction != nullptr) {
        connect(clearInspectionRouteAction, &QAction::triggered, this, [clearInspectionRoute]() {
            if (clearInspectionRoute) {
                clearInspectionRoute();
            }
        });
    }
    if (toggleRouteEditingAction != nullptr) {
        connect(toggleRouteEditingAction, &QAction::toggled, this, [setRouteEditingEnabled](bool enabled) {
            if (setRouteEditingEnabled) {
                setRouteEditingEnabled(enabled);
            }
        });
    }

    if (startInspectionRouteRoamAction != nullptr) {
        connect(startInspectionRouteRoamAction, &QAction::triggered, this, [startInspectionRouteRoam]() {
            if (startInspectionRouteRoam) {
                startInspectionRouteRoam();
            }
        });
    }
    if (pauseInspectionRouteRoamAction != nullptr) {
        connect(pauseInspectionRouteRoamAction, &QAction::triggered, this, [pauseResumeInspectionRouteRoam]() {
            if (pauseResumeInspectionRouteRoam) {
                pauseResumeInspectionRouteRoam();
            }
        });
    }
    if (stopInspectionRouteRoamAction != nullptr) {
        connect(stopInspectionRouteRoamAction, &QAction::triggered, this, [stopInspectionRouteRoam]() {
            if (stopInspectionRouteRoam) {
                stopInspectionRouteRoam();
            }
        });
    }

    if (routeRoamStartButton != nullptr && startInspectionRouteRoamAction != nullptr) {
        connect(routeRoamStartButton, &QPushButton::clicked, startInspectionRouteRoamAction, &QAction::trigger);
    }
    if (routeRoamPauseResumeButton != nullptr && pauseInspectionRouteRoamAction != nullptr) {
        connect(routeRoamPauseResumeButton, &QPushButton::clicked, pauseInspectionRouteRoamAction, &QAction::trigger);
    }
    if (routeRoamStopButton != nullptr && stopInspectionRouteRoamAction != nullptr) {
        connect(routeRoamStopButton, &QPushButton::clicked, stopInspectionRouteRoamAction, &QAction::trigger);
    }

    if (routeRoamSpeedSpinBox != nullptr) {
        connect(routeRoamSpeedSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [routeRoamSpeedChanged](double speed) {
            if (routeRoamSpeedChanged) {
                routeRoamSpeedChanged(speed);
            }
        });
    }
    if (routeRoamViewModeComboBox != nullptr) {
        connect(routeRoamViewModeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [routeRoamViewModeChanged](int index) {
            if (routeRoamViewModeChanged) {
                routeRoamViewModeChanged(index);
            }
        });
    }

    if (focusRouteWaypointAction != nullptr) {
        connect(focusRouteWaypointAction, &QAction::triggered, this, [focusRouteWaypoint]() {
            if (focusRouteWaypoint) {
                focusRouteWaypoint();
            }
        });
    }

    if (importRouteFileAction != nullptr) {
        connect(importRouteFileAction, &QAction::triggered, this, [importRouteFile]() {
            if (importRouteFile) {
                importRouteFile();
            }
        });
    }
    if (saveRouteFileAction != nullptr) {
        connect(saveRouteFileAction, &QAction::triggered, this, [saveRouteFile]() {
            if (saveRouteFile) {
                saveRouteFile();
            }
        });
    }
    if (saveRouteFileAsAction != nullptr) {
        connect(saveRouteFileAsAction, &QAction::triggered, this, [saveRouteFileAs]() {
            if (saveRouteFileAs) {
                saveRouteFileAs();
            }
        });
    }
    if (reloadRouteFileAction != nullptr) {
        connect(reloadRouteFileAction, &QAction::triggered, this, [reloadRouteFile]() {
            if (reloadRouteFile) {
                reloadRouteFile();
            }
        });
    }
    if (importRouteKmlAction != nullptr) {
        connect(importRouteKmlAction, &QAction::triggered, this, [importRouteKml]() {
            if (importRouteKml) {
                importRouteKml();
            }
        });
    }
    if (exportRouteKmlAction != nullptr) {
        connect(exportRouteKmlAction, &QAction::triggered, this, [exportRouteKml]() {
            if (exportRouteKml) {
                exportRouteKml();
            }
        });
    }
    if (exportRouteDjiKmzAction != nullptr) {
        connect(exportRouteDjiKmzAction, &QAction::triggered, this, [exportRouteDjiKmz]() {
            if (exportRouteDjiKmz) {
                exportRouteDjiKmz();
            }
        });
    }

    if (viewer_ != nullptr) {
        connect(viewer_, &PointCloudViewer::inspectionRouteRoamStateChanged, this, [inspectionRouteRoamStateChanged]() {
            if (inspectionRouteRoamStateChanged) {
                inspectionRouteRoamStateChanged();
            }
        });
        connect(
            viewer_,
            &PointCloudViewer::inspectionRouteRoamPhotoCaptured,
            this,
            [inspectionRouteRoamPhotoCaptured](int waypointIndex, int targetIndex, const QString& targetLabel, int captureCount) {
                if (inspectionRouteRoamPhotoCaptured) {
                    inspectionRouteRoamPhotoCaptured(waypointIndex, targetIndex, targetLabel, captureCount);
                }
            });
    }
}
