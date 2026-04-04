#pragma once

#include <QList>
#include <QString>

#include "domain/InspectionData.h"

bool importTowerLiTowerFile(
    const QString& filePath,
    QList<TowerRecord>* towers,
    QString* errorMessage = nullptr);

bool exportTowerLiTowerFile(
    const QString& filePath,
    const QList<TowerRecord>& towers,
    QString* errorMessage = nullptr);
