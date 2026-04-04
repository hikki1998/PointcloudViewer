#include "domain/ClassificationEditStore.h"

#include <QJsonArray>

void ClassificationEditStore::clear()
{
    editsByDataset_.clear();
}

bool ClassificationEditStore::isEmpty() const
{
    return editsByDataset_.isEmpty();
}

int ClassificationEditStore::editedPointCount() const
{
    int pointCount = 0;
    for (auto it = editsByDataset_.constBegin(); it != editsByDataset_.constEnd(); ++it) {
        pointCount += it.value().size();
    }
    return pointCount;
}

int ClassificationEditStore::effectiveClassification(
    const QString& datasetPath,
    quint32 pointIndex,
    int rawClassification) const
{
    int overrideClassification = rawClassification;
    return tryGetOverride(datasetPath, pointIndex, &overrideClassification)
        ? overrideClassification
        : rawClassification;
}

bool ClassificationEditStore::tryGetOverride(
    const QString& datasetPath,
    quint32 pointIndex,
    int* overrideClassification) const
{
    const auto datasetIt = editsByDataset_.constFind(datasetPath);
    if (datasetIt == editsByDataset_.constEnd()) {
        return false;
    }

    const auto pointIt = datasetIt->constFind(pointIndex);
    if (pointIt == datasetIt->constEnd()) {
        return false;
    }

    if (overrideClassification != nullptr) {
        *overrideClassification = pointIt.value();
    }
    return true;
}

void ClassificationEditStore::setOverride(
    const QString& datasetPath,
    quint32 pointIndex,
    int targetClassification)
{
    editsByDataset_[datasetPath].insert(pointIndex, targetClassification);
}

void ClassificationEditStore::removeOverride(const QString& datasetPath, quint32 pointIndex)
{
    auto datasetIt = editsByDataset_.find(datasetPath);
    if (datasetIt == editsByDataset_.end()) {
        return;
    }

    datasetIt->remove(pointIndex);
    if (datasetIt->isEmpty()) {
        editsByDataset_.erase(datasetIt);
    }
}

void ClassificationEditStore::applyBatch(const ClassificationEditBatch& batch)
{
    for (const ClassificationEditBatchItem& item : batch.items) {
        setOverride(item.datasetPath, item.pointIndex, item.targetClassification);
    }
}

void ClassificationEditStore::revertBatch(const ClassificationEditBatch& batch)
{
    for (auto it = batch.items.crbegin(); it != batch.items.crend(); ++it) {
        if (it->hadPreviousOverride) {
            setOverride(it->datasetPath, it->pointIndex, it->previousOverrideClassification);
        } else {
            removeOverride(it->datasetPath, it->pointIndex);
        }
    }
}

const ClassificationEditStore::StoreMap& ClassificationEditStore::editsByDataset() const
{
    return editsByDataset_;
}

QJsonObject classificationEditsToJson(
    const ClassificationEditStore& store,
    const std::function<QString(const QString&)>& datasetPathTransform)
{
    QJsonObject object;
    for (auto datasetIt = store.editsByDataset().constBegin(); datasetIt != store.editsByDataset().constEnd(); ++datasetIt) {
        const QString datasetKey = datasetPathTransform ? datasetPathTransform(datasetIt.key()) : datasetIt.key();
        if (datasetKey.isEmpty()) {
            continue;
        }

        QJsonArray editsArray;
        for (auto pointIt = datasetIt->constBegin(); pointIt != datasetIt->constEnd(); ++pointIt) {
            QJsonArray entry;
            entry.append(static_cast<qint64>(pointIt.key()));
            entry.append(pointIt.value());
            editsArray.append(entry);
        }
        object.insert(datasetKey, editsArray);
    }
    return object;
}

ClassificationEditStore classificationEditsFromJson(
    const QJsonObject& object,
    const std::function<QString(const QString&)>& datasetPathTransform)
{
    ClassificationEditStore store;
    for (auto datasetIt = object.constBegin(); datasetIt != object.constEnd(); ++datasetIt) {
        if (!datasetIt->isArray()) {
            continue;
        }

        const QString datasetPath = datasetPathTransform ? datasetPathTransform(datasetIt.key()) : datasetIt.key();
        if (datasetPath.isEmpty()) {
            continue;
        }

        const QJsonArray editsArray = datasetIt->toArray();
        for (const QJsonValue& editValue : editsArray) {
            if (!editValue.isArray()) {
                continue;
            }

            const QJsonArray entry = editValue.toArray();
            if (entry.size() != 2) {
                continue;
            }

            bool pointIndexOk = false;
            const quint32 pointIndex = static_cast<quint32>(entry.at(0).toVariant().toULongLong(&pointIndexOk));
            const int targetClassification = entry.at(1).toInt(-1);
            if (!pointIndexOk || targetClassification < 0 || targetClassification > 255) {
                continue;
            }

            store.setOverride(datasetPath, pointIndex, targetClassification);
        }
    }

    return store;
}
