#pragma once

#include <QString>
#include <QVariant>

namespace lasviewer::gui
{
class UiHistoryStore final
{
public:
    static UiHistoryStore& instance();

    QVariant load(const QString& elementId, const QVariant& defaultValue = QVariant()) const;
    void save(const QString& elementId, const QVariant& value) const;

    bool loadBool(const QString& elementId, bool defaultValue) const;
    int loadInt(const QString& elementId, int defaultValue) const;
    double loadDouble(const QString& elementId, double defaultValue) const;
    QString loadString(const QString& elementId, const QString& defaultValue = QString()) const;

private:
    UiHistoryStore() = default;

    QString toSettingsKey(const QString& elementId) const;
};
}

