#include "domain/DataManager.h"

#include <QFileInfo>

DataManager& DataManager::instance()
{
    static DataManager manager;
    return manager;
}

void DataManager::clear()
{
    pointCloudDatasets_.clear();
    imageItems_.clear();
    trajectoryItem_ = DataTrajectoryItem();
}

void DataManager::setPointCloudDatasets(const QList<PointCloudDatasetInfo>& datasets)
{
    pointCloudDatasets_ = datasets;
}

const QList<PointCloudDatasetInfo>& DataManager::pointCloudDatasets() const
{
    return pointCloudDatasets_;
}

bool DataManager::setPointCloudDatasetVisible(const QString& filePath, bool visible)
{
    for (PointCloudDatasetInfo& dataset : pointCloudDatasets_) {
        if (dataset.filePath.compare(filePath, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (dataset.visible == visible) {
            return true;
        }
        dataset.visible = visible;
        return true;
    }
    return false;
}

void DataManager::setImagesFromIssues(const QList<InspectionIssue>& issues, const QSet<int>& hiddenIssueIndices)
{
    imageItems_.clear();
    imageItems_.reserve(issues.size());
    for (int issueIndex = 0; issueIndex < issues.size(); ++issueIndex) {
        const InspectionIssue& issue = issues.at(issueIndex);
        const QString imagePath = issue.imagePath.trimmed();
        if (imagePath.isEmpty()) {
            continue;
        }

        DataImageItem imageItem;
        imageItem.issueIndex = issueIndex;
        imageItem.title = issue.title.trimmed();
        imageItem.filePath = QFileInfo(imagePath).absoluteFilePath();
        imageItem.point = issue.point;
        imageItem.visible = !hiddenIssueIndices.contains(issueIndex);
        imageItems_.append(imageItem);
    }
}

const QList<DataImageItem>& DataManager::imageItems() const
{
    return imageItems_;
}

bool DataManager::setImageVisible(int issueIndex, bool visible)
{
    for (DataImageItem& imageItem : imageItems_) {
        if (imageItem.issueIndex != issueIndex) {
            continue;
        }
        if (imageItem.visible == visible) {
            return true;
        }
        imageItem.visible = visible;
        return true;
    }
    return false;
}

void DataManager::setTrajectory(const QString& name, const QList<PointRecord>& points, bool visible)
{
    trajectoryItem_.name = name.trimmed();
    trajectoryItem_.points = points;
    trajectoryItem_.visible = visible;
}

void DataManager::clearTrajectory()
{
    trajectoryItem_ = DataTrajectoryItem();
}

const DataTrajectoryItem& DataManager::trajectoryItem() const
{
    return trajectoryItem_;
}

bool DataManager::hasTrajectory() const
{
    return !trajectoryItem_.points.isEmpty();
}
