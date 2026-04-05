#pragma once

#include <QString>
#include <QStringList>

#include "route/PowerlineRouteTypes.h"

bool importPowerlineRouteJson(
    const QString& filePath,
    PowerlineRouteDocument* route,
    QString* errorMessage = nullptr);

bool exportPowerlineRouteJson(
    const QString& filePath,
    const PowerlineRouteDocument& route,
    QString* errorMessage = nullptr);

bool validatePowerlineRoute(
    const PowerlineRouteDocument& route,
    QStringList* errors = nullptr,
    QStringList* warnings = nullptr);
