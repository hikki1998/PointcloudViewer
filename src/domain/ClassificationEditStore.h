#pragma once

#include <functional>

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>

struct ClassificationEditBatchItem
{
    QString datasetPath;
    quint32 pointIndex = 0;
    int previousEffectiveClassification = -1;
    bool hadPreviousOverride = false;
    int previousOverrideClassification = -1;
    int targetClassification = -1;
};

struct ClassificationEditBatch
{
    QList<ClassificationEditBatchItem> items;
    int hitCount = 0;
    int changedCount = 0;
    int targetClassification = -1;

    bool isEmpty() const
    {
        return items.isEmpty();
    }
};

class ClassificationEditStore
{
public:
    using DatasetEditMap = QMap<quint32, int>;
    using StoreMap = QMap<QString, DatasetEditMap>;

    void clear();
    bool isEmpty() const;
    int editedPointCount() const;

    int effectiveClassification(const QString& datasetPath, quint32 pointIndex, int rawClassification) const;
    bool tryGetOverride(const QString& datasetPath, quint32 pointIndex, int* overrideClassification = nullptr) const;
    void setOverride(const QString& datasetPath, quint32 pointIndex, int targetClassification);
    void removeOverride(const QString& datasetPath, quint32 pointIndex);

    void applyBatch(const ClassificationEditBatch& batch);
    void revertBatch(const ClassificationEditBatch& batch);

    const StoreMap& editsByDataset() const;

private:
    StoreMap editsByDataset_;
};

QJsonObject classificationEditsToJson(
    const ClassificationEditStore& store,
    const std::function<QString(const QString&)>& datasetPathTransform = {});

ClassificationEditStore classificationEditsFromJson(
    const QJsonObject& object,
    const std::function<QString(const QString&)>& datasetPathTransform = {});
