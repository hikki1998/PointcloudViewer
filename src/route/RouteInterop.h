#pragma once

#include <QString>

#include "route/InspectionRoutePlanning.h"

bool exportRouteKml(
    const QString& filePath,
    const InspectionRoute& routeWgs84,
    QString* errorMessage = nullptr);

bool importRouteKml(
    const QString& filePath,
    InspectionRoute* routeWgs84,
    QString* errorMessage = nullptr);

bool exportRouteDjiKmz(
    const QString& filePath,
    const InspectionRoute& routeWgs84,
    const RoutePlanningOptions& planningOptions,
    QString* errorMessage = nullptr);
