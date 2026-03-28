#pragma once

#include <QList>
#include <QString>

#include "domain/ClearanceAnalysis.h"
#include "domain/InspectionData.h"

enum class ProfileMarkerKind
{
    Tower = 0,
    Issue
};

struct ProjectedProfileMarker
{
    ProfileMarkerKind kind = ProfileMarkerKind::Tower;
    int sourceIndex = -1;
    float chainage = 0.0f;
    float elevation = 0.0f;
    float lateralDistance = 0.0f;
    QString title;
    bool selected = false;
};

QList<ProjectedProfileMarker> projectProfileMarkers(
    const ClearanceAnalysisResult& analysisResult,
    const QList<TowerRecord>& towers,
    int selectedTowerIndex,
    const QList<InspectionIssue>& issues,
    int selectedIssueIndex,
    float maxLateralDistance = -1.0f);
