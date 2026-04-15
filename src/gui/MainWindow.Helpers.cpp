#include "gui/MainWindowInternal.h"

#include <QAction>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QFrame>
#include <QJsonObject>
#include <QLocale>
#include <QMap>
#include <QPalette>
#include <QSet>
#include <QSettings>
#include <QSize>
#include <QToolButton>
#include <QTreeWidgetItem>

#include "gui/support/SettingsKeys.h"
#include "gui/support/UiHelpers.h"

using lasviewer::crs::CoordinateSystemRef;
namespace settingskeys = lasviewer::gui::settingskeys;
using lasviewer::gui::enforceLightDialogButtonStyles;

namespace mainwindow_internal
{
namespace
{
constexpr int kRecentProjectHistoryLimit = 10;
}

const std::array<ClassificationDisplayItem, 33> kClassificationDisplayItems = {
    ClassificationDisplayItem { 0, QT_TRANSLATE_NOOP("MainWindow", "Created / Unclassified") },
    ClassificationDisplayItem { 1, QT_TRANSLATE_NOOP("MainWindow", "Unclassified Point") },
    ClassificationDisplayItem { 2, QT_TRANSLATE_NOOP("MainWindow", "Ground Point") },
    ClassificationDisplayItem { 3, QT_TRANSLATE_NOOP("MainWindow", "Low Vegetation Point") },
    ClassificationDisplayItem { 4, QT_TRANSLATE_NOOP("MainWindow", "Medium Vegetation Point") },
    ClassificationDisplayItem { 5, QT_TRANSLATE_NOOP("MainWindow", "High Vegetation Point") },
    ClassificationDisplayItem { 6, QT_TRANSLATE_NOOP("MainWindow", "Building Point") },
    ClassificationDisplayItem { 7, QT_TRANSLATE_NOOP("MainWindow", "Low Point") },
    ClassificationDisplayItem { 8, QT_TRANSLATE_NOOP("MainWindow", "Model Key Point") },
    ClassificationDisplayItem { 9, QT_TRANSLATE_NOOP("MainWindow", "Temporary Structure") },
    ClassificationDisplayItem { 10, QT_TRANSLATE_NOOP("MainWindow", "Bridge") },
    ClassificationDisplayItem { 11, QT_TRANSLATE_NOOP("MainWindow", "Railway") },
    ClassificationDisplayItem { 12, QT_TRANSLATE_NOOP("MainWindow", "Highway") },
    ClassificationDisplayItem { 13, QT_TRANSLATE_NOOP("MainWindow", "Non-navigable River") },
    ClassificationDisplayItem { 14, QT_TRANSLATE_NOOP("MainWindow", "Lake") },
    ClassificationDisplayItem { 15, QT_TRANSLATE_NOOP("MainWindow", "Substation") },
    ClassificationDisplayItem { 16, QT_TRANSLATE_NOOP("MainWindow", "Conductor") },
    ClassificationDisplayItem { 17, QT_TRANSLATE_NOOP("MainWindow", "Tower") },
    ClassificationDisplayItem { 18, QT_TRANSLATE_NOOP("MainWindow", "Crossing Above") },
    ClassificationDisplayItem { 19, QT_TRANSLATE_NOOP("MainWindow", "Crossing Below") },
    ClassificationDisplayItem { 20, QT_TRANSLATE_NOOP("MainWindow", "Ground Wire") },
    ClassificationDisplayItem { 21, QT_TRANSLATE_NOOP("MainWindow", "Other") },
    ClassificationDisplayItem { 22, QT_TRANSLATE_NOOP("MainWindow", "Boat / Vehicle") },
    ClassificationDisplayItem { 23, QT_TRANSLATE_NOOP("MainWindow", "Other Line") },
    ClassificationDisplayItem { 24, QT_TRANSLATE_NOOP("MainWindow", "Under-Line Structure") },
    ClassificationDisplayItem { 25, QT_TRANSLATE_NOOP("MainWindow", "Navigable River") },
    ClassificationDisplayItem { 26, QT_TRANSLATE_NOOP("MainWindow", "Railway Catenary / Contact Wire") },
    ClassificationDisplayItem { 27, QT_TRANSLATE_NOOP("MainWindow", "Insulator") },
    ClassificationDisplayItem { 28, QT_TRANSLATE_NOOP("MainWindow", "Jumper Wire") },
    ClassificationDisplayItem { 29, QT_TRANSLATE_NOOP("MainWindow", "Tower Body") },
    ClassificationDisplayItem { 30, QT_TRANSLATE_NOOP("MainWindow", "Reserved30") },
    ClassificationDisplayItem { 31, QT_TRANSLATE_NOOP("MainWindow", "Sag Zone") },
    ClassificationDisplayItem { -1, QT_TRANSLATE_NOOP("MainWindow", "Other / Unknown") }
};

MainWindow::UiLanguage defaultLanguageFromLocale()
{
    const QString localeName = QLocale::system().name().toLower();
    return localeName.startsWith(QStringLiteral("zh"))
        ? MainWindow::UiLanguage::Chinese
        : MainWindow::UiLanguage::English;
}

bool isSupportedPointCloudFile(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    const QString suffix = fileInfo.suffix().toLower();
    return suffix == QStringLiteral("las") || suffix == QStringLiteral("laz");
}

void applyDefaultDockWidths(MainWindow* window, QDockWidget* projectDock, QDockWidget* inspectorDock)
{
    if (window != nullptr && projectDock != nullptr && inspectorDock != nullptr) {
        window->resizeDocks({ projectDock, inspectorDock }, { 320, 380 }, Qt::Horizontal);
    }
}

QString formatCoordinateSystemCode(const CoordinateSystemRef& crs, const QString& unsetText)
{
    if (crs.code <= 0) {
        return unsetText;
    }
    return QStringLiteral("%1:%2")
        .arg(crs.authName.trimmed().isEmpty() ? QStringLiteral("EPSG") : crs.authName.trimmed())
        .arg(QLocale().toString(crs.code));
}

QString formatProjectCoordinateSystemsSummary(const lasviewer::crs::ProjectCoordinateSystems& coordinateSystems)
{
    return QCoreApplication::translate("MainWindow", "%1 -> %2")
        .arg(formatCoordinateSystemCode(coordinateSystems.pointCloudCrs, QCoreApplication::translate("MainWindow", "Unset")))
        .arg(formatCoordinateSystemCode(coordinateSystems.geographicCrs, QStringLiteral("EPSG:4326")));
}

QString formatCoordinate(float value)
{
    return QLocale().toString(static_cast<double>(value), 'f', 2);
}

QString formatTriplet(float x, float y, float z)
{
    return QStringLiteral("%1, %2, %3")
        .arg(formatCoordinate(x))
        .arg(formatCoordinate(y))
        .arg(formatCoordinate(z));
}

QString routeTableStyleSheet()
{
    return QStringLiteral(
        "QTableWidget {"
        "background-color: #ffffff;"
        "alternate-background-color: #f8fafc;"
        "gridline-color: #e2e8f0;"
        "color: #0f172a;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "}"
        "QHeaderView::section {"
        "background-color: #e2e8f0;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "padding: 4px 8px;"
        "font-weight: 600;"
        "}"
        "QHeaderView::section:hover {"
        "background-color: #dbeafe;"
        "}"
        "QHeaderView::section:pressed {"
        "background-color: #bfdbfe;"
        "}"
        "QTableCornerButton::section {"
        "background-color: #e2e8f0;"
        "border: 1px solid #cbd5e1;"
        "}");
}

QString normalizedProjectFilePath(const QString& filePath)
{
    const QString trimmedPath = filePath.trimmed();
    if (trimmedPath.isEmpty()) {
        return QString();
    }

    return QDir::fromNativeSeparators(QFileInfo(trimmedPath).absoluteFilePath());
}

namespace
{
bool isSupportedProjectFilePath(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QStringLiteral("json") || suffix == QStringLiteral("lpproj");
}
}

int indexOfProjectFilePath(const QStringList& filePaths, const QString& targetPath)
{
    for (int index = 0; index < filePaths.size(); ++index) {
        if (QString::compare(filePaths.at(index), targetPath, Qt::CaseInsensitive) == 0) {
            return index;
        }
    }

    return -1;
}

QStringList normalizedRecentProjectFiles(const QStringList& recentPaths, const QString& preferredPath)
{
    QStringList normalizedPaths;
    QSet<QString> dedupKeys;
    const auto appendPath = [&](const QString& path, bool allowMissing) {
        const QString normalizedPath = normalizedProjectFilePath(path);
        if (normalizedPath.isEmpty() || !isSupportedProjectFilePath(normalizedPath)) {
            return;
        }

        const QFileInfo fileInfo(normalizedPath);
        if (!allowMissing && !fileInfo.exists()) {
            return;
        }

        const QString dedupKey = normalizedPath.toLower();
        if (dedupKeys.contains(dedupKey)) {
            return;
        }
        dedupKeys.insert(dedupKey);
        normalizedPaths.append(normalizedPath);
    };

    if (!preferredPath.isEmpty()) {
        appendPath(preferredPath, false);
    }
    for (const QString& recentPath : recentPaths) {
        appendPath(recentPath, false);
    }

    if (normalizedPaths.size() > kRecentProjectHistoryLimit) {
        normalizedPaths = normalizedPaths.mid(0, kRecentProjectHistoryLimit);
    }
    return normalizedPaths;
}

QString backstagePageStyleSheet()
{
    return QStringLiteral(
        "QWidget {"
        "background-color: #f8fbff;"
        "color: #0f172a;"
        "}"
        "QFrame#backstageCard {"
        "background-color: #ffffff;"
        "border: 1px solid #d8e3f0;"
        "border-radius: 14px;"
        "}"
        "QLabel#backstageTitleLabel {"
        "font-size: 28px;"
        "font-weight: 700;"
        "color: #0f172a;"
        "}"
        "QLabel#backstageSubtitleLabel {"
        "font-size: 13px;"
        "color: #475569;"
        "}"
        "QLabel#backstageSectionLabel {"
        "font-size: 15px;"
        "font-weight: 700;"
        "color: #0f172a;"
        "}"
        "QLabel#backstageBodyLabel {"
        "font-size: 13px;"
        "line-height: 1.4;"
        "color: #334155;"
        "}"
        "QLineEdit, QListWidget {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 10px;"
        "padding: 6px 10px;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "}"
        "QListWidget::item {"
        "padding: 8px 10px;"
        "border-radius: 8px;"
        "}"
        "QListWidget::item:selected {"
        "background-color: #dbeafe;"
        "color: #0f172a;"
        "}"
        "QPushButton, QToolButton {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 10px;"
        "padding: 8px 14px;"
        "font-weight: 600;"
        "}"
        "QPushButton:hover, QToolButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QPushButton:pressed, QToolButton:pressed {"
        "background-color: #dbeafe;"
        "}"
        "QPushButton:disabled, QToolButton:disabled {"
        "background-color: #f1f5f9;"
        "color: #94a3b8;"
        "border-color: #e2e8f0;"
        "}"
        "QToolButton:checked {"
        "background-color: #dbeafe;"
        "border-color: #60a5fa;"
        "}"
        "QGroupBox {"
        "font-weight: 700;"
        "color: #0f172a;"
        "border: 1px solid #d8e3f0;"
        "border-radius: 12px;"
        "margin-top: 16px;"
        "padding: 16px 14px 14px 14px;"
        "background-color: #ffffff;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 12px;"
        "padding: 0 6px;"
        "}"
        "QCheckBox {"
        "color: #0f172a;"
        "spacing: 8px;"
        "}"
        "QCheckBox::indicator {"
        "width: 16px;"
        "height: 16px;"
        "}"
        "QCheckBox::indicator:unchecked {"
        "background-color: #ffffff;"
        "border: 1px solid #94a3b8;"
        "border-radius: 4px;"
        "}"
        "QCheckBox::indicator:checked {"
        "background-color: #2563eb;"
        "border: 1px solid #2563eb;"
        "border-radius: 4px;"
        "}");
}

QFrame* createBackstageCard(QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("backstageCard"));
    return card;
}

QToolButton* createBackstageActionButton(QAction* action, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(false);
    button->setDefaultAction(action);
    button->setIconSize(QSize(20, 20));
    return button;
}

QColor showStyledColorDialog(QWidget* parent, const QColor& initialColor, const QString& title)
{
    QColorDialog dialog(initialColor, parent);
    dialog.setWindowTitle(title);
    dialog.setCurrentColor(initialColor);
    dialog.setOption(QColorDialog::DontUseNativeDialog, true);
    dialog.setStyleSheet(QStringLiteral(
        "QColorDialog, QColorDialog QWidget {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "}"
        "QColorDialog QGroupBox {"
        "background-color: #ffffff;"
        "border: 1px solid #d6dde8;"
        "border-radius: 8px;"
        "margin-top: 8px;"
        "padding-top: 10px;"
        "font-weight: 600;"
        "}"
        "QColorDialog QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 8px;"
        "padding: 0 4px;"
        "color: #334155;"
        "background-color: #f8fafc;"
        "}"
        "QColorDialog QLineEdit,"
        "QColorDialog QSpinBox,"
        "QColorDialog QDoubleSpinBox,"
        "QColorDialog QComboBox,"
        "QColorDialog QPushButton,"
        "QColorDialog QToolButton {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "min-height: 24px;"
        "padding: 3px 8px;"
        "}"
        "QColorDialog QPushButton:hover,"
        "QColorDialog QToolButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QColorDialog QPushButton:pressed,"
        "QColorDialog QToolButton:pressed {"
        "background-color: #dbeafe;"
        "}"
        "QColorDialog QToolButton:disabled,"
        "QColorDialog QPushButton:disabled {"
        "background-color: #f1f5f9;"
        "color: #94a3b8;"
        "border-color: #dbe3ee;"
        "}"
        "QColorDialog QDialogButtonBox QPushButton {"
        "min-width: 80px;"
        "}"
        "QColorDialog QDialogButtonBox QPushButton:default {"
        "background-color: #e0ecff;"
        "color: #1d4ed8;"
        "border-color: #93c5fd;"
        "}"
        "QColorDialog QDialogButtonBox QPushButton:default:hover {"
        "background-color: #d4e4ff;"
        "}"
        "QColorDialog QAbstractItemView {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "}"
        "QColorDialog QLabel,"
        "QColorDialog QCheckBox,"
        "QColorDialog QRadioButton {"
        "color: #0f172a;"
        "}"));

    QPalette palette = dialog.palette();
    palette.setColor(QPalette::Window, QColor(248, 250, 252));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(241, 245, 249));
    palette.setColor(QPalette::WindowText, QColor(15, 23, 42));
    palette.setColor(QPalette::Text, QColor(15, 23, 42));
    palette.setColor(QPalette::Button, QColor(255, 255, 255));
    palette.setColor(QPalette::ButtonText, QColor(15, 23, 42));
    palette.setColor(QPalette::Highlight, QColor(219, 234, 254));
    palette.setColor(QPalette::HighlightedText, QColor(15, 23, 42));
    dialog.setPalette(palette);
    enforceLightDialogButtonStyles(&dialog);

    if (dialog.exec() == QDialog::Accepted) {
        return dialog.currentColor();
    }
    return QColor();
}

QString projectTreeItemType(const QTreeWidgetItem* item)
{
    return item != nullptr ? item->data(0, kProjectTreeItemTypeRole).toString() : QString();
}

QJsonObject colorToJson(const QColor& color)
{
    return QJsonObject {
        { QStringLiteral("r"), color.red() },
        { QStringLiteral("g"), color.green() },
        { QStringLiteral("b"), color.blue() },
        { QStringLiteral("a"), color.alpha() }
    };
}

QColor colorFromJson(const QJsonObject& object, const QColor& fallback)
{
    if (object.isEmpty()) {
        return fallback;
    }

    return QColor(
        object.value(QStringLiteral("r")).toInt(fallback.red()),
        object.value(QStringLiteral("g")).toInt(fallback.green()),
        object.value(QStringLiteral("b")).toInt(fallback.blue()),
        object.value(QStringLiteral("a")).toInt(fallback.alpha()));
}

QJsonObject classificationColorMapToJson(const QMap<int, QColor>& colorMap)
{
    QJsonObject object;
    for (auto it = colorMap.constBegin(); it != colorMap.constEnd(); ++it) {
        object.insert(QString::number(it.key()), colorToJson(it.value()));
    }
    return object;
}

QMap<int, QColor> classificationColorMapFromJson(const QJsonObject& object, const QMap<int, QColor>& fallback)
{
    QMap<int, QColor> map = fallback;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        bool ok = false;
        const int classificationCode = it.key().toInt(&ok);
        if (!ok || classificationCode < 0 || classificationCode > 255 || !it->isObject()) {
            continue;
        }
        map.insert(classificationCode, colorFromJson(it->toObject(), map.value(classificationCode)));
    }
    return map;
}

QJsonObject classificationVisibilityMapToJson(const QMap<int, bool>& visibilityMap)
{
    QJsonObject object;
    for (auto it = visibilityMap.constBegin(); it != visibilityMap.constEnd(); ++it) {
        object.insert(QString::number(it.key()), it.value());
    }
    return object;
}

QMap<int, bool> classificationVisibilityMapFromJson(const QJsonObject& object, const QMap<int, bool>& fallback)
{
    QMap<int, bool> map = fallback;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        bool ok = false;
        const int classificationCode = it.key().toInt(&ok);
        if (!ok || classificationCode < -1 || classificationCode > 255 || !it->isBool()) {
            continue;
        }
        map.insert(classificationCode, it->toBool());
    }
    if (!map.contains(-1)) {
        map.insert(-1, true);
    }
    return map;
}

QJsonObject classificationNameMapToJson(const QMap<int, QString>& nameMap)
{
    QJsonObject object;
    for (auto it = nameMap.constBegin(); it != nameMap.constEnd(); ++it) {
        const QString trimmedName = it.value().trimmed();
        if (!trimmedName.isEmpty()) {
            object.insert(QString::number(it.key()), trimmedName);
        }
    }
    return object;
}

QMap<int, QString> classificationNameMapFromJson(const QJsonObject& object)
{
    QMap<int, QString> map;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        bool ok = false;
        const int classificationCode = it.key().toInt(&ok);
        if (!ok || classificationCode < -1 || classificationCode > 255 || !it->isString()) {
            continue;
        }
        const QString trimmedName = it->toString().trimmed();
        if (!trimmedName.isEmpty()) {
            map.insert(classificationCode, trimmedName);
        }
    }
    return map;
}

QString languageCodeFor(MainWindow::UiLanguage language)
{
    switch (language) {
    case MainWindow::UiLanguage::Chinese:
        return QStringLiteral("zh_CN");
    case MainWindow::UiLanguage::English:
    default:
        return QStringLiteral("en");
    }
}

QString projectRelativePathFor(const QString& projectFilePath, const QString& targetFilePath)
{
    const QFileInfo targetInfo(targetFilePath);
    if (!targetInfo.exists()) {
        return targetFilePath;
    }
    const QDir projectDir = QFileInfo(projectFilePath).absoluteDir();
    return projectDir.relativeFilePath(targetInfo.absoluteFilePath());
}

QString resolveProjectPath(const QString& projectFilePath, const QString& storedPath)
{
    if (storedPath.isEmpty()) {
        return QString();
    }
    const QFileInfo storedInfo(storedPath);
    if (storedInfo.isAbsolute()) {
        return storedPath;
    }
    return QFileInfo(QFileInfo(projectFilePath).absoluteDir(), storedPath).absoluteFilePath();
}

void recordRecentProjectFilePath(const QString& filePath)
{
    const QString normalizedPath = normalizedProjectFilePath(filePath);
    if (normalizedPath.isEmpty() || !QFileInfo::exists(normalizedPath)) {
        return;
    }

    QSettings settings;
    const QStringList recentProjects = normalizedRecentProjectFiles(
        settings.value(settingskeys::kProjectRecentProjects).toStringList(),
        normalizedPath);
    settings.setValue(settingskeys::kProjectRecentProjects, recentProjects);
    settings.setValue(settingskeys::kProjectLastOpenedProject, normalizedPath);
}

QString projectTreeItemFilePath(const QTreeWidgetItem* item)
{
    const QString itemType = projectTreeItemType(item);
    if (itemType == QStringLiteral("pointCloudItem") || itemType == QStringLiteral("imageItem")) {
        return item->data(0, kProjectTreeFilePathRole).toString();
    }
    return QString();
}

QString defaultClassificationDisplayName(int classificationCode)
{
    for (const ClassificationDisplayItem& item : kClassificationDisplayItems) {
        if (item.code == classificationCode) {
            return QCoreApplication::translate("MainWindow", item.sourceText);
        }
    }

    return QCoreApplication::translate("MainWindow", "Custom Class %1")
        .arg(QLocale().toString(classificationCode));
}

QString classificationDisplayName(int classificationCode, const QMap<int, QString>& customNames)
{
    const QString customName = customNames.value(classificationCode).trimmed();
    return customName.isEmpty()
        ? defaultClassificationDisplayName(classificationCode)
        : customName;
}
}
