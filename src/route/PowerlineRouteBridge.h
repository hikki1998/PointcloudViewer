#pragma once

#include <QStringList>

#include "route/InspectionRoutePlanning.h"
#include "route/PowerlineRouteTypes.h"

InspectionRoute toInspectionRouteExportView(const PowerlineRouteDocument& route);
QList<PointRecord> toRouteDisplayPoints(const PowerlineRouteDocument& route);
QStringList toRouteDisplayLabels(const PowerlineRouteDocument& route);
PowerlineRouteDocument createPowerlineRouteFromInspectionRoute(
    const InspectionRoute& route,
    const QString& taskName = QString());
