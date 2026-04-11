#include "gui/UiHistoryStore.h"

#include <QSettings>

namespace
{
const char kUiHistoryPrefix[] = "ui/history/";
}

namespace lasviewer::gui
{
UiHistoryStore& UiHistoryStore::instance()
{
    static UiHistoryStore store;
    return store;
}

QVariant UiHistoryStore::load(const QString& elementId, const QVariant& defaultValue) const
{
    QSettings settings;
    return settings.value(toSettingsKey(elementId), defaultValue);
}

void UiHistoryStore::save(const QString& elementId, const QVariant& value) const
{
    QSettings settings;
    settings.setValue(toSettingsKey(elementId), value);
}

bool UiHistoryStore::loadBool(const QString& elementId, bool defaultValue) const
{
    return load(elementId, defaultValue).toBool();
}

int UiHistoryStore::loadInt(const QString& elementId, int defaultValue) const
{
    return load(elementId, defaultValue).toInt();
}

double UiHistoryStore::loadDouble(const QString& elementId, double defaultValue) const
{
    return load(elementId, defaultValue).toDouble();
}

QString UiHistoryStore::loadString(const QString& elementId, const QString& defaultValue) const
{
    return load(elementId, defaultValue).toString();
}

QString UiHistoryStore::toSettingsKey(const QString& elementId) const
{
    return QString::fromLatin1(kUiHistoryPrefix) + elementId.trimmed();
}
}

