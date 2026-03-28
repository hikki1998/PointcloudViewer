#pragma once

#include <cstddef>

#include <QList>
#include <QSet>
#include <QString>

#include "domain/InspectionData.h"

struct PointCloudDatasetInfo
{
    QString filePath;
    std::size_t pointCount = 0;
    PointRecord minBounds;
    PointRecord maxBounds;
    bool hasColor = false;
    bool hasIntensity = false;
    bool hasClassification = false;
    bool hasReturnInfo = false;
    bool hasGpsTime = false;
    bool visible = true;
    QString projectionText;
};

struct DataImageItem
{
    int issueIndex = -1;
    QString title;
    QString filePath;
    PointRecord point;
    bool visible = true;
};

struct DataTrajectoryItem
{
    QString name;
    QList<PointRecord> points;
    bool visible = true;
};

class DataManager final
{
public:
    static DataManager& instance();

    void clear();

    void setPointCloudDatasets(const QList<PointCloudDatasetInfo>& datasets);
    const QList<PointCloudDatasetInfo>& pointCloudDatasets() const;
    bool setPointCloudDatasetVisible(const QString& filePath, bool visible);

    void setImagesFromIssues(const QList<InspectionIssue>& issues, const QSet<int>& hiddenIssueIndices = {});
    const QList<DataImageItem>& imageItems() const;
    bool setImageVisible(int issueIndex, bool visible);

    void setTrajectory(const QString& name, const QList<PointRecord>& points, bool visible);
    void clearTrajectory();
    const DataTrajectoryItem& trajectoryItem() const;
    bool hasTrajectory() const;

private:
    DataManager() = default;

    QList<PointCloudDatasetInfo> pointCloudDatasets_;
    QList<DataImageItem> imageItems_;
    DataTrajectoryItem trajectoryItem_;
};
