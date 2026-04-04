#include "gui/MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QEventLoop>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLinearGradient>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMap>
#include <QMessageBox>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QProgressBar>
#include <QPushButton>
#include <QDropEvent>
#include <QFile>
#include <QPalette>
#include <QPlainTextEdit>
#include <QGuiApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QSlider>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QToolButton>
#include <QToolBar>
#include <QToolTip>
#include <QTranslator>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <QMouseEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <set>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "QtnRibbonBar.h"
#include "QtnRibbonGroup.h"
#include "QtnRibbonPage.h"
#include "QtnRibbonQuickAccessBar.h"

#include "crs/CrsAuthorityService.h"
#include "crs/ProjectCoordinateSystemsDialog.h"
#include "crs/CrsTypes.h"
#include "domain/ClearanceAnalysis.h"
#include "domain/ClearanceReportExporter.h"
#include "domain/ClassificationEditStore.h"
#include "domain/DataManager.h"
#include "domain/InspectionData.h"
#include "domain/InspectionRoutePlanning.h"
#include "domain/InspectionReportExporter.h"
#include "domain/ProfileMarkerProjection.h"
#include "domain/RouteInterop.h"
#include "domain/RuleBasedClearanceEngine.h"
#include "domain/VegetationRiskAnalysis.h"
#include "gui/PointCloudViewer.h"
#include "gui/ProfilePlotWidget.h"
#include "osg/PointCloudVisualization.h"
#include "pointcloud/LasReader.h"
#include "pointcloud/PointCloudData.h"

#ifdef LAS_VIEWER_HAS_LASLIB
#include "lasreader.hpp"
#include "laswriter.hpp"
#endif

using lasviewer::crs::CoordinateSystemKind;
using lasviewer::crs::CoordinateSystemRef;
using lasviewer::crs::CrsAuthorityService;
using lasviewer::crs::ProjectCoordinateSystemsDialog;

namespace
{
const QColor kDarkBackground(20, 28, 38);
const QColor kLightBackground(241, 244, 249);
const QColor kWindowChromeLight(243, 246, 251);
const QColor kWindowChromeDark(51, 65, 85);
const QColor kRibbonGlyphColor(28, 64, 111);
const QColor kRibbonAccentColor(59, 130, 246);
constexpr int kMainWindowStateVersion = 1;
constexpr int kWindowResizeBorder = 8;
constexpr int kProjectTreeItemTypeRole = Qt::UserRole;
constexpr int kProjectTreeFilePathRole = Qt::UserRole + 1;
constexpr int kProjectTreeIssueIndexRole = Qt::UserRole + 2;
constexpr int kProjectTreeRouteIndexRole = Qt::UserRole + 3;

struct ClassificationDisplayItem
{
    int code;
    const char* sourceText;
};

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

bool boundsFromPoints(const QList<PointRecord>& points, PointRecord* minBounds, PointRecord* maxBounds)
{
    if (points.isEmpty() || minBounds == nullptr || maxBounds == nullptr) {
        return false;
    }

    *minBounds = points.constFirst();
    *maxBounds = points.constFirst();
    for (int index = 1; index < points.size(); ++index) {
        const PointRecord& point = points.at(index);
        minBounds->x = std::min(minBounds->x, point.x);
        minBounds->y = std::min(minBounds->y, point.y);
        minBounds->z = std::min(minBounds->z, point.z);
        maxBounds->x = std::max(maxBounds->x, point.x);
        maxBounds->y = std::max(maxBounds->y, point.y);
        maxBounds->z = std::max(maxBounds->z, point.z);
    }
    return true;
}

enum class RibbonGlyph
{
    Open,
    Clear,
    Exit,
    Fit,
    Top,
    Front,
    Right,
    Axes,
    Bounds,
    DarkBackground,
    LightBackground,
    Rgb,
    Elevation,
    SingleColor,
    Classification,
    ThemeColorful,
    ThemeWhite,
    ThemeDarkGray,
    Log,
    Measure,
    Tower,
    TowerAdd,
    TowerInsert,
    TowerMove,
    TowerAdjust,
    TowerFocus,
    TowerRemove,
    Language
};

enum class WindowControlGlyph
{
    Minimize,
    Maximize,
    Restore,
    Close
};

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

QString projectTreeItemType(const QTreeWidgetItem* item)
{
    return item != nullptr ? item->data(0, kProjectTreeItemTypeRole).toString() : QString();
}

QString projectTreeItemFilePath(const QTreeWidgetItem* item)
{
    const QString itemType = projectTreeItemType(item);
    if (itemType == QStringLiteral("pointCloudItem") || itemType == QStringLiteral("imageItem")) {
        return item->data(0, kProjectTreeFilePathRole).toString();
    }
    return QString();
}

QString colorModeName(PointCloudColorMode colorMode)
{
    switch (colorMode) {
    case PointCloudColorMode::Elevation:
        return QCoreApplication::translate("MainWindow", "Elevation ramp");
    case PointCloudColorMode::SingleColor:
        return QCoreApplication::translate("MainWindow", "Single color");
    case PointCloudColorMode::Classification:
        return QCoreApplication::translate("MainWindow", "Classification");
    case PointCloudColorMode::Rgb:
    default:
        return QCoreApplication::translate("MainWindow", "RGB");
    }
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

QString classificationDisplayName(
    int classificationCode,
    const QMap<int, QString>& customNames)
{
    const QString customName = customNames.value(classificationCode).trimmed();
    return customName.isEmpty()
        ? defaultClassificationDisplayName(classificationCode)
        : customName;
}

QJsonObject colorToJson(const QColor& color);
QColor colorFromJson(const QJsonObject& object, const QColor& fallback);

QString measurementPointText(const MeasurementResult& measurementResult, bool useStartPoint)
{
    const bool hasPoint = useStartPoint ? measurementResult.hasStartPoint : measurementResult.hasEndPoint;
    if (!hasPoint) {
        return QCoreApplication::translate("MainWindow", "Not set");
    }

    const PointRecord& point = useStartPoint ? measurementResult.startPoint : measurementResult.endPoint;
    return formatTriplet(point.x, point.y, point.z);
}

QJsonObject classificationColorMapToJson(const QMap<int, QColor>& colorMap)
{
    QJsonObject object;
    for (auto it = colorMap.constBegin(); it != colorMap.constEnd(); ++it) {
        object.insert(QString::number(it.key()), colorToJson(it.value()));
    }
    return object;
}

QMap<int, QColor> classificationColorMapFromJson(
    const QJsonObject& object,
    const QMap<int, QColor>& fallback)
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

QMap<int, bool> classificationVisibilityMapFromJson(
    const QJsonObject& object,
    const QMap<int, bool>& fallback)
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
        if (trimmedName.isEmpty()) {
            continue;
        }
        object.insert(QString::number(it.key()), trimmedName);
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

QString datasetPathSummary(const QStringList& filePaths)
{
    if (filePaths.isEmpty()) {
        return QString();
    }

    QStringList lines;
    const int visibleCount = std::min(4, filePaths.size());
    for (int index = 0; index < visibleCount; ++index) {
        lines.append(filePaths.at(index));
    }
    if (filePaths.size() > visibleCount) {
        lines.append(
            QCoreApplication::translate("MainWindow", "... and %1 more")
                .arg(QLocale().toString(filePaths.size() - visibleCount)));
    }
    return lines.join(QLatin1Char('\n'));
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

QColor analysisSeverityColor(AnalysisSeverity severity)
{
    switch (severity) {
    case AnalysisSeverity::Advisory:
        return QColor(217, 119, 6);
    case AnalysisSeverity::Warning:
        return QColor(220, 38, 38);
    case AnalysisSeverity::Critical:
        return QColor(127, 29, 29);
    case AnalysisSeverity::None:
    default:
        return QColor(22, 101, 52);
    }
}

IssueSeverity issueSeverityFromAnalysisSeverity(AnalysisSeverity severity)
{
    switch (severity) {
    case AnalysisSeverity::Advisory:
        return IssueSeverity::Minor;
    case AnalysisSeverity::Warning:
        return IssueSeverity::Major;
    case AnalysisSeverity::Critical:
        return IssueSeverity::Critical;
    case AnalysisSeverity::None:
    default:
        return IssueSeverity::Info;
    }
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

QLabel* createDetailsValueLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("detailsValueLabel"));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return label;
}

QFrame* createDetailsStatCard(const QString& labelText, const QString& valueText, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("detailsStatCard"));

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(4);

    auto* valueLabel = new QLabel(valueText, card);
    valueLabel->setObjectName(QStringLiteral("detailsStatValue"));
    valueLabel->setWordWrap(true);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(valueLabel);

    auto* label = new QLabel(labelText, card);
    label->setObjectName(QStringLiteral("detailsStatLabel"));
    label->setWordWrap(true);
    layout->addWidget(label);

    return card;
}

void showStyledDetailsDialog(
    QWidget* parent,
    const QString& title,
    const QString& subtitle,
    const QList<QPair<QString, QString>>& detailRows,
    const QList<QPair<QString, QString>>& statRows = {})
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.resize(760, 560);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog {"
        "background-color: #f3f7fb;"
        "}"
        "QFrame#detailsHeaderCard, QFrame#detailsBodyCard, QFrame#detailsStatCard {"
        "background-color: rgba(255, 255, 255, 0.96);"
        "border: 1px solid rgba(148, 163, 184, 0.20);"
        "border-radius: 16px;"
        "}"
        "QLabel#detailsTitleLabel {"
        "color: #0f172a;"
        "font-size: 22px;"
        "font-weight: 700;"
        "}"
        "QLabel#detailsSubtitleLabel {"
        "color: #475569;"
        "font-size: 13px;"
        "line-height: 1.35em;"
        "}"
        "QLabel#detailsSectionLabel {"
        "color: #0f172a;"
        "font-size: 14px;"
        "font-weight: 600;"
        "}"
        "QLabel#detailsKeyLabel {"
        "color: #64748b;"
        "font-size: 12px;"
        "font-weight: 600;"
        "letter-spacing: 0.04em;"
        "text-transform: uppercase;"
        "padding-top: 2px;"
        "}"
        "QLabel#detailsValueLabel {"
        "color: #0f172a;"
        "font-size: 13px;"
        "font-weight: 500;"
        "padding-bottom: 10px;"
        "}"
        "QLabel#detailsStatValue {"
        "color: #0f172a;"
        "font-size: 18px;"
        "font-weight: 700;"
        "}"
        "QLabel#detailsStatLabel {"
        "color: #64748b;"
        "font-size: 12px;"
        "font-weight: 500;"
        "}"
        "QPushButton {"
        "min-width: 96px;"
        "padding: 8px 18px;"
        "border-radius: 10px;"
        "border: 1px solid rgba(37, 99, 235, 0.18);"
        "background-color: #e0ecff;"
        "color: #1d4ed8;"
        "font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "background-color: #d4e4ff;"
        "}"));

    auto* rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(24, 22, 24, 20);
    rootLayout->setSpacing(16);

    auto* headerCard = new QFrame(&dialog);
    headerCard->setObjectName(QStringLiteral("detailsHeaderCard"));
    auto* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(22, 20, 22, 20);
    headerLayout->setSpacing(8);

    auto* titleLabel = new QLabel(title, headerCard);
    titleLabel->setObjectName(QStringLiteral("detailsTitleLabel"));
    titleLabel->setWordWrap(true);
    headerLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel(subtitle, headerCard);
    subtitleLabel->setObjectName(QStringLiteral("detailsSubtitleLabel"));
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerLayout->addWidget(subtitleLabel);

    if (!statRows.isEmpty()) {
        auto* statsGrid = new QGridLayout();
        statsGrid->setContentsMargins(0, 8, 0, 0);
        statsGrid->setHorizontalSpacing(12);
        statsGrid->setVerticalSpacing(12);

        for (int statIndex = 0; statIndex < statRows.size(); ++statIndex) {
            const auto& stat = statRows.at(statIndex);
            statsGrid->addWidget(
                createDetailsStatCard(stat.first, stat.second, headerCard),
                statIndex / 2,
                statIndex % 2);
        }
        headerLayout->addLayout(statsGrid);
    }

    rootLayout->addWidget(headerCard);

    auto* bodyCard = new QFrame(&dialog);
    bodyCard->setObjectName(QStringLiteral("detailsBodyCard"));
    auto* bodyLayout = new QVBoxLayout(bodyCard);
    bodyLayout->setContentsMargins(22, 18, 22, 16);
    bodyLayout->setSpacing(12);

    auto* sectionLabel = new QLabel(QCoreApplication::translate("MainWindow", "Detailed Information"), bodyCard);
    sectionLabel->setObjectName(QStringLiteral("detailsSectionLabel"));
    bodyLayout->addWidget(sectionLabel);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(2);
    grid->setColumnStretch(1, 1);

    for (int rowIndex = 0; rowIndex < detailRows.size(); ++rowIndex) {
        const auto& row = detailRows.at(rowIndex);
        auto* keyLabel = new QLabel(row.first, bodyCard);
        keyLabel->setObjectName(QStringLiteral("detailsKeyLabel"));
        keyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        grid->addWidget(keyLabel, rowIndex, 0);
        grid->addWidget(createDetailsValueLabel(row.second, bodyCard), rowIndex, 1);
    }

    bodyLayout->addLayout(grid);
    rootLayout->addWidget(bodyCard, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    rootLayout->addWidget(buttonBox);

    dialog.exec();
}

QColor showStyledColorDialog(
    QWidget* parent,
    const QColor& initialColor,
    const QString& title)
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
        "QColorDialog QPushButton {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "min-height: 24px;"
        "padding: 3px 8px;"
        "}"
        "QColorDialog QPushButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QColorDialog QPushButton:pressed {"
        "background-color: #dbeafe;"
        "}"
        "QColorDialog QDialogButtonBox QPushButton {"
        "min-width: 80px;"
        "}"
        "QColorDialog QAbstractItemView {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "}"
        "QColorDialog QLabel {"
        "color: #0f172a;"
        "}"
    ));

    QPalette palette = dialog.palette();
    palette.setColor(QPalette::Window, QColor(248, 250, 252));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(241, 245, 249));
    palette.setColor(QPalette::WindowText, QColor(15, 23, 42));
    palette.setColor(QPalette::Text, QColor(15, 23, 42));
    palette.setColor(QPalette::ButtonText, QColor(15, 23, 42));
    palette.setColor(QPalette::Highlight, QColor(219, 234, 254));
    palette.setColor(QPalette::HighlightedText, QColor(15, 23, 42));
    dialog.setPalette(palette);

    if (dialog.exec() == QDialog::Accepted) {
        return dialog.currentColor();
    }
    return QColor();
}

QString styledDialogStyleSheet()
{
    return QStringLiteral(
        "QDialog, QFileDialog, QFileDialog QWidget {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "}"
        "QFileDialog QFrame, QFileDialog QStackedWidget, QFileDialog QSplitter {"
        "background-color: #f8fafc;"
        "}"
        "QFileDialog QLabel {"
        "color: #0f172a;"
        "}"
        "QFileDialog QLineEdit,"
        "QFileDialog QComboBox,"
        "QFileDialog QListView,"
        "QFileDialog QTreeView,"
        "QFileDialog QAbstractItemView,"
        "QFileDialog QSpinBox {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "}"
        "QFileDialog QLineEdit, QFileDialog QComboBox {"
        "min-height: 26px;"
        "padding: 2px 8px;"
        "}"
        "QFileDialog QPushButton {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "min-height: 28px;"
        "padding: 4px 10px;"
        "}"
        "QFileDialog QPushButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QFileDialog QPushButton:pressed {"
        "background-color: #dbeafe;"
        "}"
        "QFileDialog QPushButton:disabled {"
        "background-color: #f1f5f9;"
        "border-color: #e2e8f0;"
        "color: #94a3b8;"
        "}"
        "QFileDialog QHeaderView::section {"
        "background-color: #e2e8f0;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "padding: 4px 8px;"
        "font-weight: 600;"
        "}"
        "QFileDialog QHeaderView::section:hover {"
        "background-color: #dbeafe;"
        "color: #0f172a;"
        "}"
        "QFileDialog QHeaderView::section:pressed {"
        "background-color: #bfdbfe;"
        "color: #0f172a;"
        "}"
        "QFileDialog QToolButton {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "padding: 3px 8px;"
        "}"
        "QFileDialog QToolButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QFileDialog QToolButton:pressed {"
        "background-color: #dbeafe;"
        "}"
    );
}

void applyStyledDialogPalette(QDialog* dialog)
{
    if (dialog == nullptr) {
        return;
    }

    dialog->setStyleSheet(styledDialogStyleSheet());

    QPalette palette = dialog->palette();
    palette.setColor(QPalette::Window, QColor(248, 250, 252));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(241, 245, 249));
    palette.setColor(QPalette::WindowText, QColor(15, 23, 42));
    palette.setColor(QPalette::Text, QColor(15, 23, 42));
    palette.setColor(QPalette::Button, QColor(255, 255, 255));
    palette.setColor(QPalette::ButtonText, QColor(15, 23, 42));
    palette.setColor(QPalette::Highlight, QColor(219, 234, 254));
    palette.setColor(QPalette::HighlightedText, QColor(15, 23, 42));
    dialog->setPalette(palette);
}

QString showStyledOpenFileNameDialog(
    QWidget* parent,
    const QString& title,
    const QString& initialPath,
    const QString& filter)
{
    QFileDialog dialog(parent, title, initialPath, filter);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    applyStyledDialogPalette(&dialog);

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return QString();
    }
    return dialog.selectedFiles().constFirst();
}

QStringList showStyledOpenFileNamesDialog(
    QWidget* parent,
    const QString& title,
    const QString& initialPath,
    const QString& filter)
{
    QFileDialog dialog(parent, title, initialPath, filter);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    applyStyledDialogPalette(&dialog);

    if (dialog.exec() != QDialog::Accepted) {
        return QStringList();
    }
    return dialog.selectedFiles();
}

QString showStyledSaveFileNameDialog(
    QWidget* parent,
    const QString& title,
    const QString& initialPath,
    const QString& filter)
{
    QFileDialog dialog(parent, title, initialPath, filter);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    applyStyledDialogPalette(&dialog);

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return QString();
    }
    return dialog.selectedFiles().constFirst();
}

QIcon createRibbonIcon(RibbonGlyph glyph)
{
    constexpr int iconSize = 48;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF canvas(2.0, 2.0, iconSize - 4.0, iconSize - 4.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(248, 250, 252));
    painter.drawRoundedRect(canvas, 12.0, 12.0);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(203, 213, 225), 1.2));
    painter.drawRoundedRect(canvas, 12.0, 12.0);

    QPen glyphPen(kRibbonGlyphColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(glyphPen);

    const QRectF r(11.0, 11.0, 26.0, 26.0);
    const auto drawTowerBase = [&painter, &r]() {
        painter.setPen(QPen(kRibbonGlyphColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.top() + 4.0), QPointF(r.center().x(), r.bottom() - 2.0));
        painter.drawLine(QPointF(r.center().x() - 7.0, r.top() + 10.0), QPointF(r.center().x() + 7.0, r.top() + 10.0));
        painter.drawLine(QPointF(r.center().x() - 5.0, r.top() + 17.0), QPointF(r.center().x() + 5.0, r.top() + 17.0));
        painter.setBrush(QColor(249, 115, 22));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(r.center().x() - 5.0, r.bottom() - 9.0, 10.0, 10.0));
    };

    switch (glyph) {
    case RibbonGlyph::Open:
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 8.0, 20.0, 12.0), 3.0, 3.0);
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 8.0), QPointF(r.left() + 12.0, r.top() + 4.5));
        painter.drawLine(QPointF(r.left() + 12.0, r.top() + 4.5), QPointF(r.left() + 18.0, r.top() + 4.5));
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.top() + 10.0), QPointF(r.center().x(), r.bottom() - 2.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom() - 2.0), QPointF(r.center().x() - 5.0, r.bottom() - 7.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom() - 2.0), QPointF(r.center().x() + 5.0, r.bottom() - 7.0));
        break;
    case RibbonGlyph::Clear:
        painter.drawRoundedRect(QRectF(r.left() + 3.0, r.top() + 10.0, 18.0, 12.0), 3.0, 3.0);
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 10.0), QPointF(r.left() + 12.0, r.top() + 5.0));
        painter.drawLine(QPointF(r.left() + 12.0, r.top() + 5.0), QPointF(r.left() + 18.0, r.top() + 5.0));
        painter.setPen(QPen(QColor(220, 38, 38), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 21.0, r.top() + 9.0), QPointF(r.right(), r.bottom() - 1.0));
        painter.drawLine(QPointF(r.right(), r.top() + 9.0), QPointF(r.left() + 21.0, r.bottom() - 1.0));
        break;
    case RibbonGlyph::Exit:
        painter.drawRoundedRect(QRectF(r.left() + 4.0, r.top() + 4.0, 14.0, 18.0), 3.0, 3.0);
        painter.setPen(QPen(QColor(220, 38, 38), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 20.0, r.center().y()), QPointF(r.right(), r.center().y()));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 5.0, r.center().y() - 5.0));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 5.0, r.center().y() + 5.0));
        break;
    case RibbonGlyph::Fit:
        painter.drawRect(QRectF(r.left() + 4.0, r.top() + 4.0, 18.0, 18.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left(), r.top() + 8.0), QPointF(r.left() + 6.0, r.top() + 8.0));
        painter.drawLine(QPointF(r.left() + 8.0, r.top()), QPointF(r.left() + 8.0, r.top() + 6.0));
        painter.drawLine(QPointF(r.right() - 6.0, r.top()), QPointF(r.right() - 6.0, r.top() + 6.0));
        painter.drawLine(QPointF(r.right() - 8.0, r.top() + 8.0), QPointF(r.right(), r.top() + 8.0));
        painter.drawLine(QPointF(r.left(), r.bottom() - 8.0), QPointF(r.left() + 6.0, r.bottom() - 8.0));
        painter.drawLine(QPointF(r.left() + 8.0, r.bottom() - 6.0), QPointF(r.left() + 8.0, r.bottom()));
        painter.drawLine(QPointF(r.right() - 6.0, r.bottom() - 6.0), QPointF(r.right() - 6.0, r.bottom()));
        painter.drawLine(QPointF(r.right() - 8.0, r.bottom() - 8.0), QPointF(r.right(), r.bottom() - 8.0));
        break;
    case RibbonGlyph::Top:
        painter.drawEllipse(QRectF(r.left() + 6.0, r.top() + 3.0, 14.0, 6.0));
        painter.drawLine(QPointF(r.left() + 6.0, r.top() + 6.0), QPointF(r.left() + 6.0, r.bottom() - 2.0));
        painter.drawLine(QPointF(r.left() + 20.0, r.top() + 6.0), QPointF(r.left() + 20.0, r.bottom() - 2.0));
        painter.drawArc(QRectF(r.left() + 6.0, r.bottom() - 8.0, 14.0, 6.0), 180 * 16, 180 * 16);
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x(), r.top() + 10.0));
        painter.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x() - 4.0, r.top() + 4.0));
        painter.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x() + 4.0, r.top() + 4.0));
        break;
    case RibbonGlyph::Front:
        painter.drawRect(QRectF(r.left() + 4.0, r.top() + 5.0, 18.0, 16.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.bottom()), QPointF(r.center().x(), r.top() + 15.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom()), QPointF(r.center().x() - 4.0, r.bottom() - 4.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom()), QPointF(r.center().x() + 4.0, r.bottom() - 4.0));
        break;
    case RibbonGlyph::Right:
        painter.drawRect(QRectF(r.left() + 5.0, r.top() + 5.0, 8.0, 16.0));
        painter.drawRect(QRectF(r.left() + 13.0, r.top() + 8.0, 8.0, 13.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.left() + 17.0, r.center().y()));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 4.0, r.center().y() - 4.0));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 4.0, r.center().y() + 4.0));
        break;
    case RibbonGlyph::Axes:
        painter.setPen(QPen(QColor(220, 38, 38), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 6.0, r.bottom() - 6.0), QPointF(r.right() - 2.0, r.bottom() - 6.0));
        painter.setPen(QPen(QColor(22, 163, 74), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 6.0, r.bottom() - 6.0), QPointF(r.left() + 6.0, r.top() + 2.0));
        painter.setPen(QPen(QColor(37, 99, 235), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 6.0, r.bottom() - 6.0), QPointF(r.right() - 6.0, r.top() + 6.0));
        break;
    case RibbonGlyph::Bounds:
        painter.drawRect(QRectF(r.left() + 5.0, r.top() + 8.0, 14.0, 14.0));
        painter.drawLine(QPointF(r.left() + 11.0, r.top() + 4.0), QPointF(r.right() - 1.0, r.top() + 10.0));
        painter.drawLine(QPointF(r.right() - 1.0, r.top() + 10.0), QPointF(r.right() - 1.0, r.bottom() - 4.0));
        painter.drawLine(QPointF(r.left() + 19.0, r.top() + 8.0), QPointF(r.right() - 1.0, r.top() + 14.0));
        painter.drawLine(QPointF(r.left() + 19.0, r.bottom() - 2.0), QPointF(r.right() - 1.0, r.bottom() - 8.0));
        break;
    case RibbonGlyph::DarkBackground:
        painter.setBrush(kDarkBackground);
        painter.setPen(QPen(QColor(51, 65, 85), 1.2));
        painter.drawRoundedRect(QRectF(r.left() + 1.0, r.top() + 5.0, 24.0, 16.0), 6.0, 6.0);
        painter.setPen(QPen(QColor(248, 250, 252), 1.8));
        painter.setBrush(QColor(248, 250, 252));
        painter.drawEllipse(QRectF(r.left() + 6.0, r.top() + 9.0, 5.0, 5.0));
        painter.drawEllipse(QRectF(r.left() + 14.0, r.top() + 12.0, 4.0, 4.0));
        break;
    case RibbonGlyph::LightBackground:
        painter.setBrush(kLightBackground);
        painter.setPen(QPen(QColor(148, 163, 184), 1.2));
        painter.drawRoundedRect(QRectF(r.left() + 1.0, r.top() + 5.0, 24.0, 16.0), 6.0, 6.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(251, 191, 36));
        painter.drawEllipse(QRectF(r.left() + 17.0, r.top() + 7.0, 7.0, 7.0));
        painter.setBrush(QColor(148, 163, 184));
        painter.drawEllipse(QRectF(r.left() + 7.0, r.top() + 12.0, 5.0, 5.0));
        break;
    case RibbonGlyph::Rgb:
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(239, 68, 68));
        painter.drawEllipse(QRectF(r.left() + 3.0, r.top() + 9.0, 8.0, 8.0));
        painter.setBrush(QColor(34, 197, 94));
        painter.drawEllipse(QRectF(r.left() + 11.0, r.top() + 9.0, 8.0, 8.0));
        painter.setBrush(QColor(59, 130, 246));
        painter.drawEllipse(QRectF(r.left() + 7.0, r.top() + 16.0, 8.0, 8.0));
        break;
    case RibbonGlyph::Elevation: {
        painter.setPen(QPen(QColor(148, 163, 184), 2.0));
        painter.drawLine(QPointF(r.left() + 2.0, r.bottom() - 3.0), QPointF(r.right() - 2.0, r.bottom() - 3.0));
        QLinearGradient gradient(QPointF(r.left(), r.bottom()), QPointF(r.right(), r.top()));
        gradient.setColorAt(0.0, QColor(37, 99, 235));
        gradient.setColorAt(0.5, QColor(16, 185, 129));
        gradient.setColorAt(1.0, QColor(249, 115, 22));
        painter.setPen(QPen(QBrush(gradient), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(QPolygonF({
            QPointF(r.left() + 2.0, r.bottom() - 6.0),
            QPointF(r.left() + 8.0, r.top() + 14.0),
            QPointF(r.left() + 15.0, r.top() + 8.0),
            QPointF(r.right() - 1.0, r.top() + 2.0)
        }));
        break;
    }
    case RibbonGlyph::SingleColor:
        painter.setPen(QPen(kRibbonGlyphColor, 2.2));
        painter.drawLine(QPointF(r.left() + 5.0, r.bottom() - 4.0), QPointF(r.right() - 6.0, r.top() + 5.0));
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 5.0), QPointF(r.right() - 3.0, r.bottom() - 4.0));
        painter.setBrush(QColor(53, 142, 255));
        painter.setPen(QPen(QColor(37, 99, 235), 1.2));
        painter.drawEllipse(QRectF(r.left() + 9.0, r.top() + 9.0, 8.0, 8.0));
        break;
    case RibbonGlyph::Classification:
        painter.setPen(QPen(QColor(148, 163, 184), 1.8));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(r.left() + 3.0, r.top() + 4.0, 20.0, 18.0), 4.0, 4.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(190, 242, 100));
        painter.drawRoundedRect(QRectF(r.left() + 5.0, r.top() + 6.0, 6.0, 5.0), 1.5, 1.5);
        painter.setBrush(QColor(22, 163, 74));
        painter.drawRoundedRect(QRectF(r.left() + 12.0, r.top() + 6.0, 9.0, 5.0), 1.5, 1.5);
        painter.setBrush(QColor(251, 146, 60));
        painter.drawRoundedRect(QRectF(r.left() + 5.0, r.top() + 12.0, 8.0, 5.0), 1.5, 1.5);
        painter.setBrush(QColor(14, 165, 233));
        painter.drawRoundedRect(QRectF(r.left() + 14.0, r.top() + 12.0, 7.0, 5.0), 1.5, 1.5);
        painter.setBrush(QColor(168, 85, 247));
        painter.drawRoundedRect(QRectF(r.left() + 5.0, r.top() + 18.0, 16.0, 3.5), 1.2, 1.2);
        break;
    case RibbonGlyph::ThemeColorful:
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(59, 130, 246));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 5.0, 22.0, 5.0), 2.5, 2.5);
        painter.setBrush(QColor(244, 114, 182));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 13.0, 15.0, 5.0), 2.5, 2.5);
        painter.setBrush(QColor(16, 185, 129));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 21.0, 19.0, 5.0), 2.5, 2.5);
        break;
    case RibbonGlyph::ThemeWhite:
        painter.setPen(QPen(QColor(148, 163, 184), 1.8));
        painter.setBrush(QColor(255, 255, 255));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 4.0, 22.0, 18.0), 5.0, 5.0);
        painter.setPen(QPen(QColor(59, 130, 246), 2.5));
        painter.drawLine(QPointF(r.left() + 4.0, r.top() + 9.0), QPointF(r.right() - 4.0, r.top() + 9.0));
        break;
    case RibbonGlyph::ThemeDarkGray:
        painter.setPen(QPen(QColor(71, 85, 105), 1.8));
        painter.setBrush(QColor(51, 65, 85));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 4.0, 22.0, 18.0), 5.0, 5.0);
        painter.setPen(QPen(QColor(148, 163, 184), 2.5));
        painter.drawLine(QPointF(r.left() + 4.0, r.top() + 9.0), QPointF(r.right() - 4.0, r.top() + 9.0));
        break;
    case RibbonGlyph::Log:
        painter.drawRoundedRect(QRectF(r.left() + 3.0, r.top() + 4.0, 20.0, 18.0), 4.0, 4.0);
        painter.drawLine(QPointF(r.left() + 7.0, r.top() + 9.0), QPointF(r.right() - 3.0, r.top() + 9.0));
        painter.drawLine(QPointF(r.left() + 7.0, r.top() + 14.0), QPointF(r.right() - 6.0, r.top() + 14.0));
        painter.drawLine(QPointF(r.left() + 7.0, r.top() + 19.0), QPointF(r.right() - 9.0, r.top() + 19.0));
        painter.setBrush(kRibbonAccentColor);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 7.0, 2.8, 2.8));
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 12.0, 2.8, 2.8));
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 17.0, 2.8, 2.8));
        break;
    case RibbonGlyph::Measure:
        painter.drawEllipse(QRectF(r.left() + 3.0, r.top() + 7.0, 6.0, 6.0));
        painter.drawEllipse(QRectF(r.right() - 9.0, r.bottom() - 9.0, 6.0, 6.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 12.0), QPointF(r.right() - 6.0, r.bottom() - 6.0));
        break;
    case RibbonGlyph::Tower:
        drawTowerBase();
        break;
    case RibbonGlyph::TowerAdd:
        drawTowerBase();
        painter.setPen(QPen(QColor(22, 163, 74), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.right() - 3.0, r.top() + 2.0), QPointF(r.right() - 3.0, r.top() + 10.0));
        painter.drawLine(QPointF(r.right() - 7.0, r.top() + 6.0), QPointF(r.right() + 1.0, r.top() + 6.0));
        break;
    case RibbonGlyph::TowerInsert:
        drawTowerBase();
        painter.setPen(QPen(QColor(37, 99, 235), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 2.0, r.top() + 5.0), QPointF(r.left() + 9.0, r.top() + 5.0));
        painter.drawLine(QPointF(r.left() + 9.0, r.top() + 5.0), QPointF(r.left() + 6.0, r.top() + 2.5));
        painter.drawLine(QPointF(r.left() + 9.0, r.top() + 5.0), QPointF(r.left() + 6.0, r.top() + 7.5));
        break;
    case RibbonGlyph::TowerMove:
        drawTowerBase();
        painter.setPen(QPen(QColor(37, 99, 235), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.top() + 1.0), QPointF(r.center().x(), r.top() + 8.0));
        painter.drawLine(QPointF(r.center().x(), r.top() + 1.0), QPointF(r.center().x() - 2.5, r.top() + 3.5));
        painter.drawLine(QPointF(r.center().x(), r.top() + 1.0), QPointF(r.center().x() + 2.5, r.top() + 3.5));
        painter.drawLine(QPointF(r.left() + 2.0, r.top() + 12.0), QPointF(r.left() + 8.0, r.top() + 12.0));
        painter.drawLine(QPointF(r.left() + 2.0, r.top() + 12.0), QPointF(r.left() + 4.5, r.top() + 9.5));
        painter.drawLine(QPointF(r.left() + 2.0, r.top() + 12.0), QPointF(r.left() + 4.5, r.top() + 14.5));
        break;
    case RibbonGlyph::TowerAdjust:
        drawTowerBase();
        painter.setPen(QPen(QColor(2, 132, 199), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(QRectF(r.right() - 11.0, r.top() + 1.5, 8.0, 8.0));
        painter.drawLine(QPointF(r.right() - 7.0, r.top() + 1.5), QPointF(r.right() - 7.0, r.top() - 1.0));
        painter.drawLine(QPointF(r.right() - 7.0, r.top() + 9.5), QPointF(r.right() - 7.0, r.top() + 12.0));
        painter.drawLine(QPointF(r.right() - 11.0, r.top() + 5.5), QPointF(r.right() - 13.5, r.top() + 5.5));
        painter.drawLine(QPointF(r.right() - 3.0, r.top() + 5.5), QPointF(r.right() - 0.5, r.top() + 5.5));
        break;
    case RibbonGlyph::TowerFocus:
        drawTowerBase();
        painter.setPen(QPen(QColor(37, 99, 235), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 1.5, r.top() + 1.5), QPointF(r.left() + 6.0, r.top() + 1.5));
        painter.drawLine(QPointF(r.left() + 1.5, r.top() + 1.5), QPointF(r.left() + 1.5, r.top() + 6.0));
        painter.drawLine(QPointF(r.right() - 1.5, r.top() + 1.5), QPointF(r.right() - 6.0, r.top() + 1.5));
        painter.drawLine(QPointF(r.right() - 1.5, r.top() + 1.5), QPointF(r.right() - 1.5, r.top() + 6.0));
        painter.drawLine(QPointF(r.left() + 1.5, r.bottom() - 1.5), QPointF(r.left() + 6.0, r.bottom() - 1.5));
        painter.drawLine(QPointF(r.left() + 1.5, r.bottom() - 1.5), QPointF(r.left() + 1.5, r.bottom() - 6.0));
        painter.drawLine(QPointF(r.right() - 1.5, r.bottom() - 1.5), QPointF(r.right() - 6.0, r.bottom() - 1.5));
        painter.drawLine(QPointF(r.right() - 1.5, r.bottom() - 1.5), QPointF(r.right() - 1.5, r.bottom() - 6.0));
        break;
    case RibbonGlyph::TowerRemove:
        drawTowerBase();
        painter.setPen(QPen(QColor(220, 38, 38), 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.right() - 9.0, r.top() + 2.0), QPointF(r.right() - 1.0, r.top() + 10.0));
        painter.drawLine(QPointF(r.right() - 1.0, r.top() + 2.0), QPointF(r.right() - 9.0, r.top() + 10.0));
        break;
    case RibbonGlyph::Language:
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 4.0, 18.0, 18.0));
        painter.drawLine(QPointF(r.center().x(), r.top() + 4.0), QPointF(r.center().x(), r.bottom() + 4.0));
        painter.drawLine(QPointF(r.left() + 4.0, r.center().y()), QPointF(r.right() + 4.0, r.center().y()));
        painter.drawArc(QRectF(r.left() + 7.0, r.top() + 4.0, 12.0, 18.0), 90 * 16, 180 * 16);
        painter.drawArc(QRectF(r.left() + 7.0, r.top() + 4.0, 12.0, 18.0), 270 * 16, 180 * 16);
        break;
    }

    return QIcon(pixmap);
}

QIcon createResourceIconOrFallback(const QString& resourcePath, RibbonGlyph fallbackGlyph)
{
    const QIcon resourceIcon(resourcePath);
    return resourceIcon.isNull() ? createRibbonIcon(fallbackGlyph) : resourceIcon;
}

QMessageBox::StandardButton showLightStyledMessageBox(
    QWidget* parent,
    QMessageBox::Icon icon,
    const QString& title,
    const QString& text,
    QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
{
    QMessageBox messageBox(icon, title, text, buttons, parent);
    messageBox.setStyleSheet(QStringLiteral(
        "QMessageBox {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "}"
        "QMessageBox QLabel {"
        "color: #0f172a;"
        "}"
        "QMessageBox QPushButton {"
        "background-color: #ffffff;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "padding: 6px 14px;"
        "color: #0f172a;"
        "min-width: 84px;"
        "}"
        "QMessageBox QPushButton:hover {"
        "background-color: #f1f5f9;"
        "border-color: #94a3b8;"
        "}"
        "QMessageBox QPushButton:pressed {"
        "background-color: #e2e8f0;"
        "}"));
    if (defaultButton != QMessageBox::NoButton) {
        messageBox.setDefaultButton(defaultButton);
    }
    return static_cast<QMessageBox::StandardButton>(messageBox.exec());
}

QIcon createWindowControlIcon(WindowControlGlyph glyph, const QColor& color)
{
    constexpr int iconSize = 12;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPen pen(color, glyph == WindowControlGlyph::Close ? 1.8 : 1.4, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    painter.setPen(pen);

    switch (glyph) {
    case WindowControlGlyph::Minimize:
        painter.drawLine(QPointF(2.0, 8.0), QPointF(10.0, 8.0));
        break;
    case WindowControlGlyph::Maximize:
        painter.drawRect(QRectF(2.0, 2.0, 8.0, 8.0));
        break;
    case WindowControlGlyph::Restore:
        painter.drawRect(QRectF(4.0, 2.0, 6.0, 6.0));
        painter.drawLine(QPointF(4.0, 4.0), QPointF(2.0, 4.0));
        painter.drawLine(QPointF(2.0, 4.0), QPointF(2.0, 10.0));
        painter.drawLine(QPointF(2.0, 10.0), QPointF(8.0, 10.0));
        break;
    case WindowControlGlyph::Close:
        painter.drawLine(QPointF(2.5, 2.5), QPointF(9.5, 9.5));
        painter.drawLine(QPointF(9.5, 2.5), QPointF(2.5, 9.5));
        break;
    }

    return QIcon(pixmap);
}
}

MainWindow::MainWindow(QTranslator* appTranslator, QTranslator* qtTranslator, QWidget* parent)
    : Qtitan::RibbonMainWindow(parent)
    , appTranslator_(appTranslator)
    , qtTranslator_(qtTranslator)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::WindowMinimizeButtonHint, true);
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    setWindowFlag(Qt::WindowCloseButtonHint, true);
    resize(1520, 920);
    setMinimumSize(960, 640);
    setAcceptDrops(true);

    loadLanguageSettings();

    viewer_ = new PointCloudViewer(this);
    setCentralWidget(viewer_);

    createActions();
    createRibbon();
    createProjectDock();
    createInspectorPanel();
    createProfileClassificationDock();
    createProfileDock();
    createLogDock();
    createStatusBar();
    setDockNestingEnabled(true);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    if (projectDock_ != nullptr && inspectorDock_ != nullptr) {
        resizeDocks({ projectDock_, inspectorDock_ }, { 320, 380 }, Qt::Horizontal);
    }
    loadInteractionSettings();
    loadMeasurementSettings();
    loadVisualizationSettings();
    createConnections();
    applyLanguage(currentLanguage_);
    loadThemeSettings();
    loadWindowSettings();

    syncUiFromViewer();
    updateNavigationHelpText();
    showUserMessage(LogLevel::Info, tr("Ready. Open, add, or drag LAS/LAZ files to begin."), 4000);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    persistVisualizationSettings();
    persistInteractionSettings();
    persistMeasurementSettings();
    persistLanguageSettings();
    persistThemeSettings();
    persistWindowSettings();
    Qtitan::RibbonMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event == nullptr || event->mimeData() == nullptr || !event->mimeData()->hasUrls()) {
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile() && isSupportedPointCloudFile(url.toLocalFile())) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (event == nullptr || event->mimeData() == nullptr || !event->mimeData()->hasUrls()) {
        return;
    }

    QStringList filePaths;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }

        const QString filePath = url.toLocalFile();
        if (!isSupportedPointCloudFile(filePath)) {
            continue;
        }

        filePaths.append(filePath);
    }

    if (!filePaths.isEmpty()) {
    const bool hasExistingPointCloud = viewer_ != nullptr && !viewer_->currentFilePaths().isEmpty();
        const bool handled = hasExistingPointCloud
            ? appendPointCloudFiles(filePaths)
            : loadPointCloudFiles(filePaths);
        if (handled) {
            event->acceptProposedAction();
        }
        return;
    }

    showUserMessage(LogLevel::Warning, tr("Only LAS and LAZ files can be dropped here."), 4000);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ribbonBar_ && event != nullptr) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && isDraggableRibbonArea(mouseEvent->pos())) {
                toggleMaximizedWindow();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && !isMaximized() && isDraggableRibbonArea(mouseEvent->pos())) {
                if (QWindow* win = windowHandle()) {
                    win->startSystemMove();
                    return true;
                }
            }
        }
    }

    return Qtitan::RibbonMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent* event)
{
    Qtitan::RibbonMainWindow::changeEvent(event);

    if (event == nullptr) {
        return;
    }

    if (event->type() == QEvent::WindowStateChange) {
        updateWindowControlButtons();
    } else if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, long* result)
{
    Q_UNUSED(eventType);

    if (message != nullptr && result != nullptr) {
        MSG* nativeMessage = static_cast<MSG*>(message);
        if (nativeMessage->message == WM_NCHITTEST && !isFullScreen()) {
            const int globalX = static_cast<short>(LOWORD(nativeMessage->lParam));
            const int globalY = static_cast<short>(HIWORD(nativeMessage->lParam));
            const QPoint localPosition = mapFromGlobal(QPoint(globalX, globalY));
            if (rect().contains(localPosition)) {
                if (!isMaximized() && !isWindowControlWidget(childAt(localPosition))) {
                    const bool onLeft = localPosition.x() >= 0 && localPosition.x() < kWindowResizeBorder;
                    const bool onRight = localPosition.x() < width() && localPosition.x() >= width() - kWindowResizeBorder;
                    const bool onTop = localPosition.y() >= 0 && localPosition.y() < kWindowResizeBorder;
                    const bool onBottom = localPosition.y() < height() && localPosition.y() >= height() - kWindowResizeBorder;

                    if (onTop && onLeft) {
                        *result = HTTOPLEFT;
                        return true;
                    }
                    if (onTop && onRight) {
                        *result = HTTOPRIGHT;
                        return true;
                    }
                    if (onBottom && onLeft) {
                        *result = HTBOTTOMLEFT;
                        return true;
                    }
                    if (onBottom && onRight) {
                        *result = HTBOTTOMRIGHT;
                        return true;
                    }
                    if (onLeft) {
                        *result = HTLEFT;
                        return true;
                    }
                    if (onRight) {
                        *result = HTRIGHT;
                        return true;
                    }
                    if (onTop) {
                        *result = HTTOP;
                        return true;
                    }
                    if (onBottom) {
                        *result = HTBOTTOM;
                        return true;
                    }
                }

                if (ribbonBar_ != nullptr && ribbonBar_->geometry().contains(localPosition)) {
                    const QPoint ribbonPosition = ribbonBar_->mapFrom(this, localPosition);
                    if (isDraggableRibbonArea(ribbonPosition)) {
                        *result = HTCAPTION;
                        return true;
                    }
                }
            }
        }
    }

    return Qtitan::RibbonMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::createActions()
{
    openAction_ = new QAction(createRibbonIcon(RibbonGlyph::Open), tr("Open"), this);
    openAction_->setShortcut(QKeySequence::Open);
    openAction_->setToolTip(tr("Open one or more LAS or LAZ datasets"));
    addPointCloudAction_ = new QAction(createRibbonIcon(RibbonGlyph::Open), tr("Add LAS Files"), this);
    addPointCloudAction_->setToolTip(tr("Add one or more LAS or LAZ datasets to the current project"));
    removeDatasetAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Remove Selected Dataset"), this);
    removeDatasetAction_->setToolTip(tr("Remove the selected LAS or LAZ dataset from the project"));
    locateDatasetAction_ = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Open Folder"), this);
    locateDatasetAction_->setToolTip(tr("Open the folder that contains the selected dataset"));
    copyDatasetPathAction_ = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("Copy Path"), this);
    copyDatasetPathAction_->setToolTip(tr("Copy the full path of the selected dataset"));
    expandProjectTreeAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowDown), tr("Expand All"), this);
    expandProjectTreeAction_->setToolTip(tr("Expand the project explorer tree"));
    collapseProjectTreeAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("Collapse All"), this);
    collapseProjectTreeAction_->setToolTip(tr("Collapse the project explorer tree"));

    openProjectAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Open Project"), this);
    saveProjectAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Project"), this);
    saveProjectAsAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Project As"), this);
    projectCoordinateSystemsAction_ = new QAction(
        style()->standardIcon(QStyle::SP_FileDialogDetailedView),
        tr("Project Properties"),
        this);
    projectCoordinateSystemsAction_->setToolTip(tr("Open project coordinate system settings"));

    clearAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear"), this);
    clearAction_->setToolTip(tr("Clear the current scene"));

    exitAction_ = new QAction(createRibbonIcon(RibbonGlyph::Exit), tr("Exit"), this);
    exitAction_->setShortcut(QKeySequence::Quit);

    fitSceneAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Fit Scene"), this);
    fitSceneAction_->setToolTip(tr("Reset to a fitted isometric view"));

    topViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Top), tr("Top"), this);
    frontViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Front), tr("Front"), this);
    rightViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Right), tr("Right"), this);

    showAxesAction_ = new QAction(createRibbonIcon(RibbonGlyph::Axes), tr("Axes"), this);
    showAxesAction_->setCheckable(true);

    showBoundingBoxAction_ = new QAction(createRibbonIcon(RibbonGlyph::Bounds), tr("Bounds"), this);
    showBoundingBoxAction_->setCheckable(true);

    darkBackgroundAction_ = new QAction(createRibbonIcon(RibbonGlyph::DarkBackground), tr("Dark"), this);
    lightBackgroundAction_ = new QAction(createRibbonIcon(RibbonGlyph::LightBackground), tr("Light"), this);

    colorModeActionGroup_ = new QActionGroup(this);
    colorModeActionGroup_->setExclusive(true);

    rgbColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::Rgb), tr("RGB"), this);
    rgbColorAction_->setCheckable(true);
    elevationColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::Elevation), tr("Elevation"), this);
    elevationColorAction_->setCheckable(true);
    singleColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::SingleColor), tr("Single"), this);
    singleColorAction_->setCheckable(true);
    classificationColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::Classification), tr("Classification"), this);
    classificationColorAction_->setCheckable(true);

    colorModeActionGroup_->addAction(rgbColorAction_);
    colorModeActionGroup_->addAction(elevationColorAction_);
    colorModeActionGroup_->addAction(singleColorAction_);
    colorModeActionGroup_->addAction(classificationColorAction_);

    themeActionGroup_ = new QActionGroup(this);
    themeActionGroup_->setExclusive(true);

    themeColorfulAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeColorful), tr("Colorful"), this);
    themeColorfulAction_->setCheckable(true);
    themeWhiteAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeWhite), tr("White"), this);
    themeWhiteAction_->setCheckable(true);
    themeDarkGrayAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeDarkGray), tr("Dark Gray"), this);
    themeDarkGrayAction_->setCheckable(true);

    themeActionGroup_->addAction(themeColorfulAction_);
    themeActionGroup_->addAction(themeWhiteAction_);
    themeActionGroup_->addAction(themeDarkGrayAction_);

    measureAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Measure"), this);
    measureAction_->setCheckable(true);
    profileClassificationAction_ = new QAction(createRibbonIcon(RibbonGlyph::Classification), tr("Profile Classify"), this);
    profileClassificationAction_->setCheckable(true);
    profileClassificationAction_->setToolTip(tr("Drag a rectangle to reclassify points. Hold Alt and drag left mouse to adjust view while the tool is active"));
    showProfileClassificationDockAction_ = new QAction(createRibbonIcon(RibbonGlyph::Classification), tr("Classify Panel"), this);
    showProfileClassificationDockAction_->setCheckable(true);
    showProfileClassificationDockAction_->setChecked(false);
    saveProfileClassificationEditsAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Classify Result"), this);
    undoProfileClassificationAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowBack), tr("Undo Classify"), this);
    redoProfileClassificationAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowForward), tr("Redo Classify"), this);
    clearProfileClassificationEditsAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Classify Edits"), this);

    clearMeasurementAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Measure"), this);
    exportClearanceCsvAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Clearance CSV"), this);
    showProfileDockAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Profile View"), this);
    showProfileDockAction_->setCheckable(true);
    showProfileDockAction_->setChecked(false);
    analyzeVegetationRisksAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Analyze Risks"), this);
    analyzeVegetationRisksAction_->setToolTip(tr("Analyze vegetation risks around the current measured corridor"));
    focusVegetationRiskAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Focus Current Risk"), this);
    createIssueFromRiskAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Create Issue"), this);
    createIssuesFromRisksAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Create All Issues"), this);
    clearVegetationRisksAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Risks"), this);
    generateInspectionRouteAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Generate Route"), this);
    regenerateInspectionRouteAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Regenerate Route"), this);
    clearInspectionRouteAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Route"), this);
    focusRouteWaypointAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Focus Route Point"), this);
    importRouteKmlAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Import Route KML"), this);
    exportRouteKmlAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Route KML"), this);
    exportRouteDjiKmzAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export DJI KMZ"), this);

    startTowerEditAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Start Editing"), this);
    finishTowerEditAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Finish Editing"), this);
    addTowerAction_ = new QAction(
        createResourceIconOrFallback(QStringLiteral(":/assets/icon/addTower.png"), RibbonGlyph::TowerAdd),
        tr("Click To Add Tower"),
        this);
    insertTowerAction_ = new QAction(
        createResourceIconOrFallback(QStringLiteral(":/assets/icon/addTowerPrevious.png"), RibbonGlyph::TowerInsert),
        tr("Insert Before Current"),
        this);
    moveTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::TowerMove), tr("Move Current Tower"), this);
    editCurrentTowerAction_ = new QAction(
        createResourceIconOrFallback(QStringLiteral(":/assets/icon/modifyTower.png"), RibbonGlyph::TowerAdjust),
        tr("Edit Current Tower"),
        this);
    focusTowerAction_ = new QAction(
        createResourceIconOrFallback(QStringLiteral(":/assets/icon/focusTower.png"), RibbonGlyph::TowerFocus),
        tr("Focus Current Tower"),
        this);
    removeTowerAction_ = new QAction(
        createResourceIconOrFallback(QStringLiteral(":/assets/icon/deleteTower.png"), RibbonGlyph::TowerRemove),
        tr("Remove Current Tower"),
        this);
    clearTowersAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Tower Markers"), this);
    cancelTowerToolAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Cancel Tower Tool"), this);
    showTowerXAction_ = new QAction(tr("Show X"), this);
    showTowerYAction_ = new QAction(tr("Show Y"), this);
    showTowerZAction_ = new QAction(tr("Show Z"), this);
    showTowerXAction_->setCheckable(true);
    showTowerYAction_->setCheckable(true);
    showTowerZAction_->setCheckable(true);
    startIssueMarkAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Mark Issue"), this);
    startIssueMarkAction_->setToolTip(tr("Click a point in the view to add an inspection issue"));
    cancelIssueToolAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Cancel Issue Tool"), this);
    focusIssueAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Focus Current Issue"), this);
    removeIssueAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Remove Current Issue"), this);
    clearIssuesAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Issues"), this);
    exportIssuesCsvAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Issues CSV"), this);
    exportInspectionReportAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Inspection Report"), this);

    showLogAction_ = new QAction(createRibbonIcon(RibbonGlyph::Log), tr("Log"), this);
    showLogAction_->setCheckable(true);
    showLogAction_->setChecked(false);
    showLogAction_->setToolTip(tr("Show or hide the log panel"));

    languageActionGroup_ = new QActionGroup(this);
    languageActionGroup_->setExclusive(true);

    languageEnglishAction_ = new QAction(createRibbonIcon(RibbonGlyph::Language), QStringLiteral("English"), this);
    languageEnglishAction_->setCheckable(true);
    languageChineseAction_ = new QAction(createRibbonIcon(RibbonGlyph::Language), QStringLiteral("\u4e2d\u6587"), this);
    languageChineseAction_->setCheckable(true);

    languageActionGroup_->addAction(languageEnglishAction_);
    languageActionGroup_->addAction(languageChineseAction_);
}

void MainWindow::createRibbon()
{
    ribbonBar_ = new Qtitan::RibbonBar(this);
    // Avoid Qtitan's DWM-integrated frame path, which paints a black caption area on this setup.
    ribbonBar_->setFrameThemeEnabled(false);
    ribbonBar_->setTitleBarVisible(false);
    ribbonBar_->showQuickAccess(true);
    ribbonBar_->setStyleSheet(QStringLiteral(
        "QAbstractButton {"
        "background-color: transparent;"
        "border: none;"
        "color: #0f172a;"
        "}"
        "QAbstractButton:hover {"
        "background-color: rgba(37, 99, 235, 0.16);"
        "color: #0b1220;"
        "}"
        "QAbstractButton:checked, QAbstractButton:pressed {"
        "background-color: #1d4ed8;"
        "color: #ffffff;"
        "}"
        "QAbstractButton:disabled {"
        "background-color: transparent;"
        "color: #64748b;"
        "}"));
    ribbonBar_->installEventFilter(this);
    setRibbonBar(ribbonBar_);
    createWindowControls();

    ribbonBar_->quickAccessBar()->addAction(openAction_);
    ribbonBar_->quickAccessBar()->addAction(saveProjectAction_);
    ribbonBar_->quickAccessBar()->addAction(fitSceneAction_);
    ribbonBar_->quickAccessBar()->addAction(showAxesAction_);
    ribbonBar_->quickAccessBar()->addAction(measureAction_);

    homePage_ = ribbonBar_->addPage(tr("Home"));
    datasetRibbonGroup_ = homePage_->addGroup(tr("Dataset"));
    datasetRibbonGroup_->addAction(openAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(addPointCloudAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(openProjectAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(saveProjectAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(clearAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(exitAction_, Qt::ToolButtonTextUnderIcon);

    cameraRibbonGroup_ = homePage_->addGroup(tr("Camera"));
    cameraRibbonGroup_->addAction(fitSceneAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(topViewAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(frontViewAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(rightViewAction_, Qt::ToolButtonTextUnderIcon);

    sceneRibbonGroup_ = homePage_->addGroup(tr("Scene Guides"));
    sceneRibbonGroup_->addAction(showAxesAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(showBoundingBoxAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(darkBackgroundAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(lightBackgroundAction_, Qt::ToolButtonTextUnderIcon);

    measureRibbonGroup_ = homePage_->addGroup(tr("Measure"));
    measureRibbonGroup_->addAction(measureAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(profileClassificationAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(showProfileClassificationDockAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(saveProfileClassificationEditsAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(showProfileDockAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(clearMeasurementAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(undoProfileClassificationAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(redoProfileClassificationAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(clearProfileClassificationEditsAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(exportClearanceCsvAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(analyzeVegetationRisksAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(generateInspectionRouteAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(exportRouteDjiKmzAction_, Qt::ToolButtonTextUnderIcon);

    workspaceRibbonGroup_ = homePage_->addGroup(tr("Workspace"));
    workspaceRibbonGroup_->addAction(saveProjectAsAction_, Qt::ToolButtonTextUnderIcon);
    workspaceRibbonGroup_->addAction(projectCoordinateSystemsAction_, Qt::ToolButtonTextUnderIcon);
    workspaceRibbonGroup_->addAction(startIssueMarkAction_, Qt::ToolButtonTextUnderIcon);
    workspaceRibbonGroup_->addAction(importRouteKmlAction_, Qt::ToolButtonTextUnderIcon);
    workspaceRibbonGroup_->addAction(exportRouteKmlAction_, Qt::ToolButtonTextUnderIcon);
    workspaceRibbonGroup_->addAction(exportInspectionReportAction_, Qt::ToolButtonTextUnderIcon);
    workspaceRibbonGroup_->addAction(showLogAction_, Qt::ToolButtonTextUnderIcon);

    towerPage_ = ribbonBar_->addPage(tr("Tower"));
    towerRibbonGroup_ = towerPage_->addGroup(tr("Tower Editing"));
    towerRibbonGroup_->addAction(startTowerEditAction_, Qt::ToolButtonTextUnderIcon);
    towerRibbonGroup_->addAction(finishTowerEditAction_, Qt::ToolButtonTextUnderIcon);

    appearancePage_ = ribbonBar_->addPage(tr("Appearance"));
    colorRibbonGroup_ = appearancePage_->addGroup(tr("Point Colors"));
    colorRibbonGroup_->addAction(rgbColorAction_, Qt::ToolButtonTextUnderIcon);
    colorRibbonGroup_->addAction(elevationColorAction_, Qt::ToolButtonTextUnderIcon);
    colorRibbonGroup_->addAction(singleColorAction_, Qt::ToolButtonTextUnderIcon);
    colorRibbonGroup_->addAction(classificationColorAction_, Qt::ToolButtonTextUnderIcon);

    themeRibbonGroup_ = appearancePage_->addGroup(tr("Office Theme"));
    themeRibbonGroup_->addAction(themeColorfulAction_, Qt::ToolButtonTextUnderIcon);
    themeRibbonGroup_->addAction(themeWhiteAction_, Qt::ToolButtonTextUnderIcon);
    themeRibbonGroup_->addAction(themeDarkGrayAction_, Qt::ToolButtonTextUnderIcon);

    languageRibbonGroup_ = appearancePage_->addGroup(tr("Language"));
    languageRibbonGroup_->addAction(languageEnglishAction_, Qt::ToolButtonTextUnderIcon);
    languageRibbonGroup_->addAction(languageChineseAction_, Qt::ToolButtonTextUnderIcon);
}

void MainWindow::createWindowControls()
{
    if (ribbonBar_ == nullptr) {
        return;
    }

    windowControlsWidget_ = new QWidget(ribbonBar_);
    auto* controlsLayout = new QHBoxLayout(windowControlsWidget_);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(0);

    minimizeButton_ = new QToolButton(windowControlsWidget_);
    maximizeButton_ = new QToolButton(windowControlsWidget_);
    closeButton_ = new QToolButton(windowControlsWidget_);

    const QList<QToolButton*> buttons = { minimizeButton_, maximizeButton_, closeButton_ };
    for (QToolButton* button : buttons) {
        button->setAutoRaise(true);
        button->setCursor(Qt::ArrowCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIconSize(QSize(12, 12));
        button->setFixedSize(34, 26);
        controlsLayout->addWidget(button);
    }

    minimizeButton_->setObjectName(QStringLiteral("windowMinimizeButton"));
    maximizeButton_->setObjectName(QStringLiteral("windowMaximizeButton"));
    closeButton_->setObjectName(QStringLiteral("windowCloseButton"));

    connect(minimizeButton_, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(maximizeButton_, &QToolButton::clicked, this, [this]() { toggleMaximizedWindow(); });
    connect(closeButton_, &QToolButton::clicked, this, &QWidget::close);

    ribbonBar_->setCornerWidget(windowControlsWidget_, Qt::TopRightCorner);
    updateWindowControlButtons();
}

void MainWindow::createProjectDock()
{
    projectDock_ = new QDockWidget(tr("Project Explorer"), this);
    projectDock_->setObjectName(QStringLiteral("projectExplorerDock"));
    projectDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    projectDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    projectDock_->setMinimumWidth(280);

    auto* projectDockContents = new QWidget(projectDock_);
    projectDockContents->setObjectName(QStringLiteral("projectExplorerSurface"));
    auto* projectDockLayout = new QVBoxLayout(projectDockContents);
    projectDockLayout->setContentsMargins(12, 12, 12, 12);
    projectDockLayout->setSpacing(10);

    projectSearchEdit_ = new QLineEdit(projectDockContents);
    projectSearchEdit_->setObjectName(QStringLiteral("projectExplorerSearch"));
    projectSearchEdit_->setClearButtonEnabled(true);

    projectToolBar_ = new QToolBar(projectDockContents);
    projectToolBar_->setObjectName(QStringLiteral("projectExplorerToolBar"));
    projectToolBar_->setIconSize(QSize(16, 16));
    projectToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    projectToolBar_->setMovable(false);
    projectToolBar_->setFloatable(false);
    projectToolBar_->addAction(openAction_);
    projectToolBar_->addAction(addPointCloudAction_);
    projectToolBar_->addAction(removeDatasetAction_);
    projectToolBar_->addSeparator();
    projectToolBar_->addAction(locateDatasetAction_);
    projectToolBar_->addAction(copyDatasetPathAction_);
    projectToolBar_->addSeparator();
    projectToolBar_->addAction(expandProjectTreeAction_);
    projectToolBar_->addAction(collapseProjectTreeAction_);

    projectTreeWidget_ = new QTreeWidget(projectDockContents);
    projectTreeWidget_->setObjectName(QStringLiteral("projectExplorerTree"));
    projectTreeWidget_->setColumnCount(1);
    projectTreeWidget_->setHeaderHidden(true);
    projectTreeWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    projectTreeWidget_->setAlternatingRowColors(false);
    projectTreeWidget_->setRootIsDecorated(true);
    projectTreeWidget_->setUniformRowHeights(true);
    projectTreeWidget_->setAnimated(true);
    projectTreeWidget_->setIndentation(18);
    projectTreeWidget_->setFrameShape(QFrame::NoFrame);
    projectTreeWidget_->setContextMenuPolicy(Qt::CustomContextMenu);

    projectDockLayout->addWidget(projectSearchEdit_);
    projectDockLayout->addWidget(projectToolBar_);
    projectDockLayout->addWidget(projectTreeWidget_, 1);

    projectDockContents->setStyleSheet(QStringLiteral(
        "QWidget#projectExplorerSurface {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f8fafc, stop:1 #eef4fb);"
        "border: none;"
        "}"
        "QLineEdit#projectExplorerSearch {"
        "background: rgba(255, 255, 255, 0.92);"
        "border: 1px solid #d6dee9;"
        "border-radius: 10px;"
        "padding: 8px 12px;"
        "color: #0f172a;"
        "selection-background-color: #bfdbfe;"
        "}"
        "QLineEdit#projectExplorerSearch:focus {"
        "border-color: #60a5fa;"
        "}"
        "QToolBar#projectExplorerToolBar {"
        "background: rgba(255, 255, 255, 0.7);"
        "border: 1px solid #d8e0ea;"
        "border-radius: 10px;"
        "padding: 4px;"
        "spacing: 4px;"
        "}"
        "QToolBar#projectExplorerToolBar QToolButton {"
        "background: rgba(255, 255, 255, 0.92);"
        "border: 1px solid #d6dde8;"
        "border-radius: 8px;"
        "padding: 6px 10px;"
        "color: #1e293b;"
        "}"
        "QToolBar#projectExplorerToolBar QToolButton:hover {"
        "background: rgba(219, 234, 254, 0.95);"
        "border-color: #93c5fd;"
        "}"
        "QToolBar#projectExplorerToolBar QToolButton:pressed,"
        "QToolBar#projectExplorerToolBar QToolButton:checked {"
        "background: #2563eb;"
        "border-color: #1d4ed8;"
        "color: #eff6ff;"
        "}"
        "QToolBar#projectExplorerToolBar QToolButton:disabled {"
        "background: #f1f5f9;"
        "border-color: #e2e8f0;"
        "color: #94a3b8;"
        "}"
        "QTreeWidget#projectExplorerTree {"
        "background: rgba(255, 255, 255, 0.84);"
        "border: 1px solid #d8e0ea;"
        "border-radius: 12px;"
        "padding: 8px 6px;"
        "color: #0f172a;"
        "outline: none;"
        "}"
        "QTreeWidget#projectExplorerTree::item {"
        "min-height: 28px;"
        "padding: 4px 8px;"
        "border-radius: 8px;"
        "}"
        "QTreeWidget#projectExplorerTree::item:hover {"
        "background: rgba(219, 234, 254, 0.85);"
        "}"
        "QTreeWidget#projectExplorerTree::item:selected {"
        "background: #2563eb;"
        "color: #eff6ff;"
        "}"));

    projectDock_->setWidget(projectDockContents);
    addDockWidget(Qt::LeftDockWidgetArea, projectDock_);
}

void MainWindow::createInspectorPanel()
{
    inspectorDock_ = new QDockWidget(tr("Scene Inspector"), this);
    inspectorDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    inspectorDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    inspectorTabWidget_ = new QTabWidget(inspectorDock_);
    inspectorTabWidget_->setObjectName(QStringLiteral("sceneInspectorTabs"));
    inspectorTabWidget_->setDocumentMode(true);
    inspectorTabWidget_->setMovable(false);

    auto createTabPage = [this](const QString& objectName) {
        auto* scrollArea = new QScrollArea(inspectorTabWidget_);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        auto* page = new QWidget(scrollArea);
        page->setObjectName(objectName);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(14, 14, 14, 14);
        pageLayout->setSpacing(12);
        pageLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
        scrollArea->setWidget(page);
        return qMakePair(scrollArea, pageLayout);
    };

    auto overviewTab = createTabPage(QStringLiteral("sceneInspectorOverviewPage"));
    auto towerTab = createTabPage(QStringLiteral("sceneInspectorTowerPage"));
    auto issueTab = createTabPage(QStringLiteral("sceneInspectorIssuePage"));
    auto renderingTab = createTabPage(QStringLiteral("sceneInspectorRenderingPage"));
    auto measurementTab = createTabPage(QStringLiteral("sceneInspectorMeasurementPage"));
    auto analysisTab = createTabPage(QStringLiteral("sceneInspectorAnalysisPage"));
    auto navigationTab = createTabPage(QStringLiteral("sceneInspectorNavigationPage"));

    datasetGroupBox_ = new QGroupBox(tr("Dataset Summary"), overviewTab.first);
    datasetLayout_ = new QFormLayout(datasetGroupBox_);
    datasetLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    datasetLayout_->setFormAlignment(Qt::AlignTop);

    datasetNameValueLabel_ = new QLabel(datasetGroupBox_);
    datasetPathValueLabel_ = new QLabel(datasetGroupBox_);
    datasetPointsValueLabel_ = new QLabel(datasetGroupBox_);
    datasetBoundsValueLabel_ = new QLabel(datasetGroupBox_);
    datasetExtentValueLabel_ = new QLabel(datasetGroupBox_);
    datasetColorValueLabel_ = new QLabel(datasetGroupBox_);

    const QList<QLabel*> datasetLabels = {
        datasetNameValueLabel_,
        datasetPathValueLabel_,
        datasetPointsValueLabel_,
        datasetBoundsValueLabel_,
        datasetExtentValueLabel_,
        datasetColorValueLabel_
    };
    for (QLabel* label : datasetLabels) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    datasetLayout_->addRow(tr("Name"), datasetNameValueLabel_);
    datasetLayout_->addRow(tr("Path"), datasetPathValueLabel_);
    datasetLayout_->addRow(tr("Points"), datasetPointsValueLabel_);
    datasetLayout_->addRow(tr("Bounds"), datasetBoundsValueLabel_);
    datasetLayout_->addRow(tr("Extent"), datasetExtentValueLabel_);
    datasetLayout_->addRow(tr("Color Source"), datasetColorValueLabel_);

    auto* towerToolbarHost = new QWidget(towerTab.first);
    auto* towerToolbarHostLayout = new QHBoxLayout(towerToolbarHost);
    towerToolbarHostLayout->setContentsMargins(0, 0, 0, 0);
    const double towerUiScale = std::clamp(static_cast<double>(towerToolbarHost->logicalDpiX()) / 96.0, 1.0, 2.0);
    const int towerIconSize = static_cast<int>(std::lround(26.0 * towerUiScale));
    const int towerButtonSize = static_cast<int>(std::lround(44.0 * towerUiScale));
    const int towerButtonPadding = static_cast<int>(std::lround(7.0 * towerUiScale));
    const int towerButtonRadius = static_cast<int>(std::lround(9.0 * towerUiScale));
    const int towerToolSpacing = static_cast<int>(std::lround(8.0 * towerUiScale));
    towerToolbarHostLayout->setSpacing(towerToolSpacing + 2);

    towerToolBar_ = new QToolBar(towerToolbarHost);
    towerToolBar_->setIconSize(QSize(towerIconSize, towerIconSize));
    towerToolBar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    towerToolBar_->setMovable(false);
    towerToolBar_->setFloatable(false);
    towerToolBar_->setStyleSheet(QStringLiteral(
        "QToolBar { spacing: %1px; }"
        "QToolButton {"
        "min-width: %2px;"
        "min-height: %2px;"
        "padding: %3px;"
        "border: 1px solid #cbd5e1;"
        "border-radius: %4px;"
        "background-color: #ffffff;"
        "}"
        "QToolButton:hover {"
        "border-color: #94a3b8;"
        "background-color: #f8fafc;"
        "}"
        "QToolButton:pressed {"
        "background-color: #e2e8f0;"
        "}"
        "QToolButton:disabled {"
        "border-color: #e2e8f0;"
        "background-color: #f8fafc;"
        "color: #94a3b8;"
        "}"
    ).arg(towerToolSpacing).arg(towerButtonSize).arg(towerButtonPadding).arg(towerButtonRadius));
    towerToolBar_->addAction(addTowerAction_);
    towerToolBar_->addAction(insertTowerAction_);
    towerToolBar_->addAction(moveTowerAction_);
    towerToolBar_->addAction(editCurrentTowerAction_);
    towerToolBar_->addAction(focusTowerAction_);
    towerToolBar_->addAction(removeTowerAction_);

    towerToolbarHostLayout->addWidget(towerToolBar_, 1);

    auto* towerPanel = new QWidget(towerTab.first);
    auto* towerLayout = new QVBoxLayout(towerPanel);
    towerLayout->setContentsMargins(0, 0, 0, 0);
    towerLayout->setSpacing(8);

    towerCountValueLabel_ = new QLabel(towerPanel);
    towerCountValueLabel_->setWordWrap(true);
    towerToolStatusLabel_ = new QLabel(towerPanel);
    towerToolStatusLabel_->setWordWrap(true);

    towerTableWidget_ = new QTableWidget(towerPanel);
    towerTableWidget_->setColumnCount(5);
    towerTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    towerTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    towerTableWidget_->setAlternatingRowColors(true);
    towerTableWidget_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    towerTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Name"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z") });
    towerTableWidget_->verticalHeader()->setVisible(false);
    towerTableWidget_->horizontalHeader()->setStretchLastSection(false);
    towerTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    towerTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    towerTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    towerTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    towerTableWidget_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    towerTableWidget_->setColumnHidden(2, true);
    towerTableWidget_->setColumnHidden(3, true);
    towerTableWidget_->setColumnHidden(4, true);
    towerTableWidget_->setStyleSheet(QStringLiteral(
        "QTableWidget {"
        "background-color: #ffffff;"
        "alternate-background-color: #f8fafc;"
        "gridline-color: #e2e8f0;"
        "color: #0f172a;"
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
        "}"
    ));

    towerLayout->addWidget(towerCountValueLabel_);
    towerLayout->addWidget(towerToolStatusLabel_);
    towerLayout->addWidget(towerTableWidget_, 1);

    towerDetailsGroupBox_ = new QGroupBox(tr("Selected Tower Details"), towerPanel);
    towerDetailsLayout_ = new QFormLayout(towerDetailsGroupBox_);
    towerDetailsLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    towerDetailsLayout_->setFormAlignment(Qt::AlignTop);
    towerCodeEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerLineNameEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerVoltageLevelEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerStructureTypeEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerInspectionDateEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerStatusEdit_ = new QLineEdit(towerDetailsGroupBox_);
    towerNotesEdit_ = new QPlainTextEdit(towerDetailsGroupBox_);
    towerNotesEdit_->setMaximumHeight(96);
    towerDetailsLayout_->addRow(tr("Code"), towerCodeEdit_);
    towerDetailsLayout_->addRow(tr("Line"), towerLineNameEdit_);
    towerDetailsLayout_->addRow(tr("Voltage"), towerVoltageLevelEdit_);
    towerDetailsLayout_->addRow(tr("Tower Type"), towerStructureTypeEdit_);
    towerDetailsLayout_->addRow(tr("Inspection Date"), towerInspectionDateEdit_);
    towerDetailsLayout_->addRow(tr("Tower Status"), towerStatusEdit_);
    towerDetailsLayout_->addRow(tr("Notes"), towerNotesEdit_);
    towerLayout->addWidget(towerDetailsGroupBox_);

    auto* issueToolbarHost = new QWidget(issueTab.first);
    auto* issueToolbarHostLayout = new QHBoxLayout(issueToolbarHost);
    issueToolbarHostLayout->setContentsMargins(0, 0, 0, 0);
    issueToolbarHostLayout->setSpacing(8);

    issueToolBar_ = new QToolBar(issueToolbarHost);
    issueToolBar_->setIconSize(QSize(16, 16));
    issueToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    issueToolBar_->setMovable(false);
    issueToolBar_->setFloatable(false);
    issueToolBar_->addAction(startIssueMarkAction_);
    issueToolBar_->addAction(cancelIssueToolAction_);
    issueToolBar_->addSeparator();
    issueToolBar_->addAction(focusIssueAction_);
    issueToolBar_->addAction(removeIssueAction_);
    issueToolBar_->addAction(clearIssuesAction_);
    issueToolBar_->addSeparator();
    issueToolBar_->addAction(exportIssuesCsvAction_);
    issueToolBar_->addAction(exportInspectionReportAction_);

    issueActionsMenu_ = new QMenu(issueToolbarHost);
    issueActionsMenu_->addAction(startIssueMarkAction_);
    issueActionsMenu_->addAction(cancelIssueToolAction_);
    issueActionsMenu_->addSeparator();
    issueActionsMenu_->addAction(focusIssueAction_);
    issueActionsMenu_->addAction(removeIssueAction_);
    issueActionsMenu_->addAction(clearIssuesAction_);
    issueActionsMenu_->addSeparator();
    issueActionsMenu_->addAction(exportIssuesCsvAction_);
    issueActionsMenu_->addAction(exportInspectionReportAction_);

    issueMenuButton_ = new QToolButton(issueToolbarHost);
    issueMenuButton_->setPopupMode(QToolButton::InstantPopup);
    issueMenuButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    issueMenuButton_->setMenu(issueActionsMenu_);

    issueToolbarHostLayout->addWidget(issueToolBar_, 1);
    issueToolbarHostLayout->addWidget(issueMenuButton_, 0);

    auto* issuePanel = new QWidget(issueTab.first);
    auto* issueLayout = new QVBoxLayout(issuePanel);
    issueLayout->setContentsMargins(0, 0, 0, 0);
    issueLayout->setSpacing(8);

    issueCountValueLabel_ = new QLabel(issuePanel);
    issueCountValueLabel_->setWordWrap(true);
    issueToolStatusLabel_ = new QLabel(issuePanel);
    issueToolStatusLabel_->setWordWrap(true);

    issueTableWidget_ = new QTableWidget(issuePanel);
    issueTableWidget_->setColumnCount(6);
    issueTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    issueTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    issueTableWidget_->setAlternatingRowColors(true);
    issueTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    issueTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Title"), tr("Severity"), tr("Status"), tr("Tower"), tr("Category") });
    issueTableWidget_->verticalHeader()->setVisible(false);
    issueTableWidget_->horizontalHeader()->setStretchLastSection(false);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    issueTableWidget_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    issueDetailsGroupBox_ = new QGroupBox(tr("Selected Issue Details"), issuePanel);
    issueDetailsLayout_ = new QFormLayout(issueDetailsGroupBox_);
    issueDetailsLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    issueDetailsLayout_->setFormAlignment(Qt::AlignTop);
    issueTitleEdit_ = new QLineEdit(issueDetailsGroupBox_);
    issueCategoryComboBox_ = new QComboBox(issueDetailsGroupBox_);
    issueCategoryComboBox_->setEditable(true);
    issueCategoryComboBox_->addItems({ tr("Vegetation"), tr("Insulator"), tr("Tower Body"), tr("Channel Risk"), tr("Other") });
    issueSeverityComboBox_ = new QComboBox(issueDetailsGroupBox_);
    issueStatusComboBox_ = new QComboBox(issueDetailsGroupBox_);
    issueRelatedTowerComboBox_ = new QComboBox(issueDetailsGroupBox_);
    issueImagePathEdit_ = new QLineEdit(issueDetailsGroupBox_);
    issueDescriptionEdit_ = new QPlainTextEdit(issueDetailsGroupBox_);
    issueDescriptionEdit_->setMaximumHeight(110);
    issueLocationValueLabel_ = new QLabel(issueDetailsGroupBox_);
    issueLocationValueLabel_->setWordWrap(true);
    issueCreatedAtValueLabel_ = new QLabel(issueDetailsGroupBox_);
    issueCreatedAtValueLabel_->setWordWrap(true);
    issueSeverityComboBox_->addItems({
        issueSeverityDisplayName(IssueSeverity::Info),
        issueSeverityDisplayName(IssueSeverity::Minor),
        issueSeverityDisplayName(IssueSeverity::Major),
        issueSeverityDisplayName(IssueSeverity::Critical)
    });
    issueStatusComboBox_->addItems({
        issueStatusDisplayName(IssueStatus::Open),
        issueStatusDisplayName(IssueStatus::Monitoring),
        issueStatusDisplayName(IssueStatus::Resolved)
    });
    issueDetailsLayout_->addRow(tr("Title"), issueTitleEdit_);
    issueDetailsLayout_->addRow(tr("Category"), issueCategoryComboBox_);
    issueDetailsLayout_->addRow(tr("Severity"), issueSeverityComboBox_);
    issueDetailsLayout_->addRow(tr("Issue Status"), issueStatusComboBox_);
    issueDetailsLayout_->addRow(tr("Related Tower"), issueRelatedTowerComboBox_);
    issueDetailsLayout_->addRow(tr("Image Path"), issueImagePathEdit_);
    issueDetailsLayout_->addRow(tr("Location"), issueLocationValueLabel_);
    issueDetailsLayout_->addRow(tr("Created At"), issueCreatedAtValueLabel_);
    issueDetailsLayout_->addRow(tr("Description"), issueDescriptionEdit_);

    issueLayout->addWidget(issueCountValueLabel_);
    issueLayout->addWidget(issueToolStatusLabel_);
    issueLayout->addWidget(issueTableWidget_, 1);
    issueLayout->addWidget(issueDetailsGroupBox_);

    renderingGroupBox_ = new QGroupBox(tr("Rendering Controls"), renderingTab.first);
    renderingLayout_ = new QFormLayout(renderingGroupBox_);
    renderingLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    renderingLayout_->setFormAlignment(Qt::AlignTop);

    pointSizeControl_ = createSliderControl(pointSizeSlider_, pointSizeValueLabel_, 1, 20, 1);
    pointOpacityControl_ = createSliderControl(pointOpacitySlider_, pointOpacityValueLabel_, 10, 100, 5);
    depthCueControl_ = createSliderControl(depthCueSlider_, depthCueValueLabel_, 0, 100, 5);
    edlStrengthControl_ = createSliderControl(edlStrengthSlider_, edlStrengthValueLabel_, 0, 100, 5);

    colorModeComboBox_ = new QComboBox(renderingGroupBox_);
    colorModeComboBox_->addItem(tr("RGB"));
    colorModeComboBox_->addItem(tr("Elevation Ramp"));
    colorModeComboBox_->addItem(tr("Single Color"));
    colorModeComboBox_->addItem(tr("Classification"));

    pointColorButton_ = new QPushButton(tr("Pick Color"), renderingGroupBox_);
    backgroundColorButton_ = new QPushButton(tr("Pick Background"), renderingGroupBox_);

    roundSplatsCheckBox_ = new QCheckBox(tr("Round splats (survey style)"), renderingGroupBox_);
    axesCheckBox_ = new QCheckBox(tr("Show XYZ axes"), renderingGroupBox_);
    boundingBoxCheckBox_ = new QCheckBox(tr("Show bounding box"), renderingGroupBox_);

    renderingLayout_->addRow(tr("Point Size"), pointSizeControl_);
    renderingLayout_->addRow(tr("Point Opacity"), pointOpacityControl_);
    renderingLayout_->addRow(tr("Depth Cue"), depthCueControl_);
    renderingLayout_->addRow(tr("EDL-style Shading"), edlStrengthControl_);
    renderingLayout_->addRow(tr("Color Mode"), colorModeComboBox_);
    renderingLayout_->addRow(tr("Single Color"), pointColorButton_);
    renderingLayout_->addRow(tr("Background"), backgroundColorButton_);
    renderingLayout_->addRow(QString(), roundSplatsCheckBox_);
    renderingLayout_->addRow(QString(), axesCheckBox_);
    renderingLayout_->addRow(QString(), boundingBoxCheckBox_);

    classificationColorsGroupBox_ = new QGroupBox(tr("Classification Mapping"), renderingTab.first);
    auto* classificationColorsLayout = new QVBoxLayout(classificationColorsGroupBox_);
    classificationColorsLayout->setContentsMargins(12, 12, 12, 12);
    classificationColorsLayout->setSpacing(8);

    classificationColorsTableWidget_ = new QTableWidget(classificationColorsGroupBox_);
    classificationColorsTableWidget_->setColumnCount(4);
    classificationColorsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    classificationColorsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    classificationColorsTableWidget_->setAlternatingRowColors(true);
    classificationColorsTableWidget_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    classificationColorsTableWidget_->setHorizontalHeaderLabels({ tr("Show"), tr("Class ID"), tr("Class Name"), tr("Color") });
    classificationColorsTableWidget_->verticalHeader()->setVisible(false);
    classificationColorsTableWidget_->horizontalHeader()->setStretchLastSection(false);
    classificationColorsTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    classificationColorsTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    classificationColorsTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    classificationColorsTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    classificationColorsTableWidget_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    classificationColorsTableWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    resetClassificationColorsButton_ = new QPushButton(tr("Reset Defaults"), classificationColorsGroupBox_);
    resetClassificationColorsButton_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    auto* classificationButtonRow = new QHBoxLayout();
    classificationButtonRow->addStretch(1);
    classificationButtonRow->addWidget(resetClassificationColorsButton_);

    classificationColorsLayout->addWidget(classificationColorsTableWidget_);
    classificationColorsLayout->addLayout(classificationButtonRow);

    profileClassificationGroupBox_ = new QGroupBox(tr("3D Profile Classification"), renderingTab.first);
    auto* profileClassificationLayout = new QVBoxLayout(profileClassificationGroupBox_);
    profileClassificationLayout->setContentsMargins(12, 12, 12, 12);
    profileClassificationLayout->setSpacing(8);

    auto* profileClassificationButtonRow = new QHBoxLayout();
    profileClassificationButtonRow->setContentsMargins(0, 0, 0, 0);
    profileClassificationButtonRow->setSpacing(6);
    profileClassificationToggleButton_ = new QPushButton(tr("Start Tool"), profileClassificationGroupBox_);
    profileClassificationSelectAllButton_ = new QPushButton(tr("Select All"), profileClassificationGroupBox_);
    profileClassificationClearSelectionButton_ = new QPushButton(tr("Clear Sources"), profileClassificationGroupBox_);
    profileClassificationButtonRow->addWidget(profileClassificationToggleButton_);
    profileClassificationButtonRow->addWidget(profileClassificationSelectAllButton_);
    profileClassificationButtonRow->addWidget(profileClassificationClearSelectionButton_);
    profileClassificationButtonRow->addStretch(1);

    auto* profileClassificationHistoryRow = new QHBoxLayout();
    profileClassificationHistoryRow->setContentsMargins(0, 0, 0, 0);
    profileClassificationHistoryRow->setSpacing(6);
    profileClassificationUndoButton_ = new QPushButton(tr("Undo"), profileClassificationGroupBox_);
    profileClassificationRedoButton_ = new QPushButton(tr("Redo"), profileClassificationGroupBox_);
    profileClassificationClearEditsButton_ = new QPushButton(tr("Clear Edits"), profileClassificationGroupBox_);
    profileClassificationSaveButton_ = new QPushButton(tr("Save Result"), profileClassificationGroupBox_);
    profileClassificationHistoryRow->addWidget(profileClassificationUndoButton_);
    profileClassificationHistoryRow->addWidget(profileClassificationRedoButton_);
    profileClassificationHistoryRow->addWidget(profileClassificationClearEditsButton_);
    profileClassificationHistoryRow->addWidget(profileClassificationSaveButton_);
    profileClassificationHistoryRow->addStretch(1);

    profileClassificationStatusLabel_ = new QLabel(profileClassificationGroupBox_);
    profileClassificationStatusLabel_->setWordWrap(true);
    profileClassificationStatusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* sourceTitleLabel = new QLabel(tr("Source Classes"), profileClassificationGroupBox_);
    profileClassificationSourceListWidget_ = new QListWidget(profileClassificationGroupBox_);
    profileClassificationSourceListWidget_->setSelectionMode(QAbstractItemView::NoSelection);
    profileClassificationSourceListWidget_->setAlternatingRowColors(true);
    profileClassificationSourceListWidget_->setMinimumHeight(220);

    auto* targetTitleLabel = new QLabel(tr("Target Class"), profileClassificationGroupBox_);
    profileClassificationTargetListWidget_ = new QListWidget(profileClassificationGroupBox_);
    profileClassificationTargetListWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    profileClassificationTargetListWidget_->setAlternatingRowColors(true);
    profileClassificationTargetListWidget_->setMinimumHeight(180);

    for (const ClassificationDisplayItem& item : kClassificationDisplayItems) {
        if (item.code < 0) {
            continue;
        }

        auto* sourceItem = new QListWidgetItem(profileClassificationSourceListWidget_);
        sourceItem->setData(Qt::UserRole, item.code);
        sourceItem->setFlags(sourceItem->flags() | Qt::ItemIsUserCheckable);
        sourceItem->setCheckState(Qt::Unchecked);

        auto* targetItem = new QListWidgetItem(profileClassificationTargetListWidget_);
        targetItem->setData(Qt::UserRole, item.code);
    }

    profileClassificationLayout->addLayout(profileClassificationButtonRow);
    profileClassificationLayout->addLayout(profileClassificationHistoryRow);
    profileClassificationLayout->addWidget(profileClassificationStatusLabel_);
    profileClassificationLayout->addWidget(sourceTitleLabel);
    profileClassificationLayout->addWidget(profileClassificationSourceListWidget_);
    profileClassificationLayout->addWidget(targetTitleLabel);
    profileClassificationLayout->addWidget(profileClassificationTargetListWidget_);

    auto* measurementToolbarHost = new QWidget(measurementTab.first);
    auto* measurementToolbarHostLayout = new QHBoxLayout(measurementToolbarHost);
    measurementToolbarHostLayout->setContentsMargins(0, 0, 0, 0);
    measurementToolbarHostLayout->setSpacing(8);

    measurementToolBar_ = new QToolBar(measurementToolbarHost);
    measurementToolBar_->setIconSize(QSize(16, 16));
    measurementToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    measurementToolBar_->setMovable(false);
    measurementToolBar_->setFloatable(false);
    measurementToolBar_->addAction(measureAction_);
    measurementToolBar_->addAction(showProfileDockAction_);
    measurementToolBar_->addAction(clearMeasurementAction_);
    measurementToolBar_->addSeparator();
    measurementToolBar_->addAction(exportClearanceCsvAction_);
    measurementToolbarHostLayout->addWidget(measurementToolBar_, 1);

    measurementGroupBox_ = new QGroupBox(tr("Measurement"), measurementTab.first);
    measurementLayout_ = new QFormLayout(measurementGroupBox_);
    measurementLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    measurementLayout_->setFormAlignment(Qt::AlignTop);

    measurementToggleButton_ = new QPushButton(tr("Start Measurement"), measurementGroupBox_);
    measurementClearButton_ = new QPushButton(tr("Clear Measurement"), measurementGroupBox_);
    measurementStartValueLabel_ = new QLabel(measurementGroupBox_);
    measurementEndValueLabel_ = new QLabel(measurementGroupBox_);
    measurementDistanceValueLabel_ = new QLabel(measurementGroupBox_);
    measurementHorizontalDistanceValueLabel_ = new QLabel(measurementGroupBox_);
    measurementDeltaZValueLabel_ = new QLabel(measurementGroupBox_);
    measurementSegmentsValueLabel_ = new QLabel(measurementGroupBox_);

    const QList<QLabel*> measurementLabels = {
        measurementStartValueLabel_,
        measurementEndValueLabel_,
        measurementDistanceValueLabel_,
        measurementHorizontalDistanceValueLabel_,
        measurementDeltaZValueLabel_,
        measurementSegmentsValueLabel_
    };
    for (QLabel* label : measurementLabels) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    measurementLayout_->addRow(QString(), measurementToggleButton_);
    measurementLayout_->addRow(QString(), measurementClearButton_);
    measurementLayout_->addRow(tr("Start Point"), measurementStartValueLabel_);
    measurementLayout_->addRow(tr("End Point"), measurementEndValueLabel_);
    measurementLayout_->addRow(tr("3D Distance"), measurementDistanceValueLabel_);
    measurementLayout_->addRow(tr("Horizontal Distance"), measurementHorizontalDistanceValueLabel_);
    measurementLayout_->addRow(tr("Height Delta"), measurementDeltaZValueLabel_);
    measurementLayout_->addRow(tr("Path Segments"), measurementSegmentsValueLabel_);

    clearanceGroupBox_ = new QGroupBox(tr("Clearance Analysis"), measurementTab.first);
    clearanceLayout_ = new QFormLayout(clearanceGroupBox_);
    clearanceLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    clearanceLayout_->setFormAlignment(Qt::AlignTop);

    clearanceThresholdSpinBox_ = new QDoubleSpinBox(clearanceGroupBox_);
    clearanceThresholdSpinBox_->setRange(0.0, 1000000.0);
    clearanceThresholdSpinBox_->setDecimals(2);
    clearanceThresholdSpinBox_->setSingleStep(0.5);
    clearanceThresholdSpinBox_->setKeyboardTracking(false);

    clearanceShortestValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceWarningCountValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceStatusValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceStatusValueLabel_->setWordWrap(true);

    for (QLabel* label : { clearanceShortestValueLabel_, clearanceWarningCountValueLabel_, clearanceStatusValueLabel_ }) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    clearanceRulePresetComboBox_ = new QComboBox(clearanceGroupBox_);
    clearanceRulePresetComboBox_->addItem(clearanceRulePresetDisplayName(ClearanceRulePreset::TransmissionCorridor), static_cast<int>(ClearanceRulePreset::TransmissionCorridor));
    clearanceRulePresetComboBox_->addItem(clearanceRulePresetDisplayName(ClearanceRulePreset::DistributionCorridor), static_cast<int>(ClearanceRulePreset::DistributionCorridor));
    clearanceRulePresetComboBox_->addItem(clearanceRulePresetDisplayName(ClearanceRulePreset::StructureApproach), static_cast<int>(ClearanceRulePreset::StructureApproach));
    clearanceRulePresetComboBox_->addItem(clearanceRulePresetDisplayName(ClearanceRulePreset::Custom), static_cast<int>(ClearanceRulePreset::Custom));
    clearanceRuleBandsValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceRuleBandsValueLabel_->setWordWrap(true);
    clearanceRuleBandsValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    clearanceLayout_->addRow(tr("Rule Preset"), clearanceRulePresetComboBox_);
    clearanceLayout_->addRow(tr("Critical Threshold"), clearanceThresholdSpinBox_);
    clearanceLayout_->addRow(tr("Risk Bands"), clearanceRuleBandsValueLabel_);
    clearanceLayout_->addRow(tr("Shortest Segment"), clearanceShortestValueLabel_);
    clearanceLayout_->addRow(tr("Risk Segments"), clearanceWarningCountValueLabel_);
    clearanceLayout_->addRow(tr("Status"), clearanceStatusValueLabel_);

    clearanceSegmentsGroupBox_ = new QGroupBox(tr("Path Segment Details"), measurementTab.first);
    auto* clearanceSegmentsLayout = new QVBoxLayout(clearanceSegmentsGroupBox_);
    clearanceSegmentsLayout->setContentsMargins(12, 12, 12, 12);
    clearanceSegmentsLayout->setSpacing(8);

    clearanceSegmentsSummaryLabel_ = new QLabel(clearanceSegmentsGroupBox_);
    clearanceSegmentsSummaryLabel_->setWordWrap(true);
    clearanceSegmentsSummaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    clearanceSegmentsTableWidget_ = new QTableWidget(clearanceSegmentsGroupBox_);
    clearanceSegmentsTableWidget_->setColumnCount(8);
    clearanceSegmentsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    clearanceSegmentsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    clearanceSegmentsTableWidget_->setAlternatingRowColors(true);
    clearanceSegmentsTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    clearanceSegmentsTableWidget_->setHorizontalHeaderLabels({
        tr("Segment"),
        tr("From"),
        tr("To"),
        tr("Chainage"),
        tr("Horizontal"),
        tr("3D"),
        tr("dZ"),
        tr("Status")
    });
    clearanceSegmentsTableWidget_->verticalHeader()->setVisible(false);
    clearanceSegmentsTableWidget_->horizontalHeader()->setStretchLastSection(false);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->setMinimumHeight(220);

    clearanceSegmentsLayout->addWidget(clearanceSegmentsSummaryLabel_);
    clearanceSegmentsLayout->addWidget(clearanceSegmentsTableWidget_, 1);

    auto* analysisToolbarHost = new QWidget(analysisTab.first);
    auto* analysisToolbarHostLayout = new QHBoxLayout(analysisToolbarHost);
    analysisToolbarHostLayout->setContentsMargins(0, 0, 0, 0);
    analysisToolbarHostLayout->setSpacing(8);

    analysisToolBar_ = new QToolBar(analysisToolbarHost);
    analysisToolBar_->setIconSize(QSize(16, 16));
    analysisToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    analysisToolBar_->setMovable(false);
    analysisToolBar_->setFloatable(false);
    analysisToolBar_->addAction(analyzeVegetationRisksAction_);
    analysisToolBar_->addAction(focusVegetationRiskAction_);
    analysisToolBar_->addSeparator();
    analysisToolBar_->addAction(createIssueFromRiskAction_);
    analysisToolBar_->addAction(createIssuesFromRisksAction_);
    analysisToolBar_->addAction(clearVegetationRisksAction_);
    analysisToolbarHostLayout->addWidget(analysisToolBar_, 1);

    analysisParametersGroupBox_ = new QGroupBox(tr("Vegetation Risk Analysis"), analysisTab.first);
    analysisParametersLayout_ = new QFormLayout(analysisParametersGroupBox_);
    analysisParametersLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    analysisParametersLayout_->setFormAlignment(Qt::AlignTop);

    vegetationSearchRadiusSpinBox_ = new QDoubleSpinBox(analysisParametersGroupBox_);
    vegetationSearchRadiusSpinBox_->setRange(1.0, 1000000.0);
    vegetationSearchRadiusSpinBox_->setDecimals(2);
    vegetationSearchRadiusSpinBox_->setSingleStep(1.0);
    vegetationSearchRadiusSpinBox_->setKeyboardTracking(false);

    vegetationClusterGapSpinBox_ = new QDoubleSpinBox(analysisParametersGroupBox_);
    vegetationClusterGapSpinBox_->setRange(0.5, 1000000.0);
    vegetationClusterGapSpinBox_->setDecimals(2);
    vegetationClusterGapSpinBox_->setSingleStep(0.5);
    vegetationClusterGapSpinBox_->setKeyboardTracking(false);

    vegetationClusterPointCountSpinBox_ = new QSpinBox(analysisParametersGroupBox_);
    vegetationClusterPointCountSpinBox_->setRange(1, 999999);

    preferVegetationClassificationCheckBox_ = new QCheckBox(tr("Prefer LAS vegetation classifications when available"), analysisParametersGroupBox_);

    vegetationRiskCountValueLabel_ = new QLabel(analysisParametersGroupBox_);
    vegetationRiskCountValueLabel_->setWordWrap(true);
    vegetationRiskStatusValueLabel_ = new QLabel(analysisParametersGroupBox_);
    vegetationRiskStatusValueLabel_->setWordWrap(true);
    vegetationRiskSummaryLabel_ = new QLabel(analysisParametersGroupBox_);
    vegetationRiskSummaryLabel_->setWordWrap(true);
    vegetationRiskSummaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    analysisParametersLayout_->addRow(tr("Search Radius"), vegetationSearchRadiusSpinBox_);
    analysisParametersLayout_->addRow(tr("Cluster Gap"), vegetationClusterGapSpinBox_);
    analysisParametersLayout_->addRow(tr("Min Cluster Points"), vegetationClusterPointCountSpinBox_);
    analysisParametersLayout_->addRow(QString(), preferVegetationClassificationCheckBox_);
    analysisParametersLayout_->addRow(tr("Risk Count"), vegetationRiskCountValueLabel_);
    analysisParametersLayout_->addRow(tr("Status"), vegetationRiskStatusValueLabel_);
    analysisParametersLayout_->addRow(tr("Summary"), vegetationRiskSummaryLabel_);

    vegetationRisksGroupBox_ = new QGroupBox(tr("Detected Risk Clusters"), analysisTab.first);
    auto* vegetationRisksLayout = new QVBoxLayout(vegetationRisksGroupBox_);
    vegetationRisksLayout->setContentsMargins(12, 12, 12, 12);
    vegetationRisksLayout->setSpacing(8);

    vegetationRisksTableWidget_ = new QTableWidget(vegetationRisksGroupBox_);
    vegetationRisksTableWidget_->setColumnCount(7);
    vegetationRisksTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    vegetationRisksTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    vegetationRisksTableWidget_->setAlternatingRowColors(true);
    vegetationRisksTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    vegetationRisksTableWidget_->setHorizontalHeaderLabels({
        tr("Index"),
        tr("Title"),
        tr("Severity"),
        tr("Min Distance"),
        tr("Chainage"),
        tr("Tower"),
        tr("Points")
    });
    vegetationRisksTableWidget_->verticalHeader()->setVisible(false);
    vegetationRisksTableWidget_->horizontalHeader()->setStretchLastSection(false);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->setMinimumHeight(220);
    vegetationRisksLayout->addWidget(vegetationRisksTableWidget_, 1);

    routePlanningGroupBox_ = new QGroupBox(tr("Inspection Route Planning"), analysisTab.first);
    auto* routePlanningLayout = new QFormLayout(routePlanningGroupBox_);
    routePlanningLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    routePlanningLayout->setFormAlignment(Qt::AlignTop);

    aircraftProfileComboBox_ = new QComboBox(routePlanningGroupBox_);
    for (const DjiAircraftProfile profile : supportedDjiAircraftProfiles()) {
        aircraftProfileComboBox_->addItem(djiAircraftProfileDisplayName(profile), static_cast<int>(profile));
    }
    {
        const int profileIndex = aircraftProfileComboBox_->findData(static_cast<int>(routePlanningOptions_.aircraftProfile));
        aircraftProfileComboBox_->setCurrentIndex(profileIndex >= 0 ? profileIndex : 0);
    }

    routeSafetyHeightSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeSafetyHeightSpinBox_->setRange(1.0, 1500.0);
    routeSafetyHeightSpinBox_->setDecimals(2);
    routeSafetyHeightSpinBox_->setValue(routePlanningOptions_.safety.safetyHeightMeters);

    routeWaypointSpeedSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeWaypointSpeedSpinBox_->setRange(0.5, 25.0);
    routeWaypointSpeedSpinBox_->setDecimals(2);
    routeWaypointSpeedSpinBox_->setValue(routePlanningOptions_.safety.defaultWaypointSpeedMps);

    routeWaypointSpacingSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeWaypointSpacingSpinBox_->setRange(1.0, 1000.0);
    routeWaypointSpacingSpinBox_->setDecimals(2);
    routeWaypointSpacingSpinBox_->setValue(routePlanningOptions_.generation.waypointSpacingMeters);

    routeSmoothingStrengthSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeSmoothingStrengthSpinBox_->setRange(0.0, 100.0);
    routeSmoothingStrengthSpinBox_->setDecimals(1);
    routeSmoothingStrengthSpinBox_->setValue(routePlanningOptions_.generation.smoothingStrengthPercent);

    routeHeightOffsetSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeHeightOffsetSpinBox_->setRange(0.0, 500.0);
    routeHeightOffsetSpinBox_->setDecimals(2);
    routeHeightOffsetSpinBox_->setValue(routePlanningOptions_.safety.heightOffsetMeters);

    routeStatusValueLabel_ = new QLabel(routePlanningGroupBox_);
    routeStatusValueLabel_->setWordWrap(true);
    routeSummaryValueLabel_ = new QLabel(routePlanningGroupBox_);
    routeSummaryValueLabel_->setWordWrap(true);
    routeSummaryValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    routePlanningLayout->addRow(tr("DJI Profile"), aircraftProfileComboBox_);
    routePlanningLayout->addRow(tr("Safety Height"), routeSafetyHeightSpinBox_);
    routePlanningLayout->addRow(tr("Waypoint Speed"), routeWaypointSpeedSpinBox_);
    routePlanningLayout->addRow(tr("Waypoint Spacing"), routeWaypointSpacingSpinBox_);
    routePlanningLayout->addRow(tr("Smoothing"), routeSmoothingStrengthSpinBox_);
    routePlanningLayout->addRow(tr("Height Offset"), routeHeightOffsetSpinBox_);
    routePlanningLayout->addRow(tr("Status"), routeStatusValueLabel_);
    routePlanningLayout->addRow(tr("Summary"), routeSummaryValueLabel_);

    routeWaypointsGroupBox_ = new QGroupBox(tr("Route Waypoints"), analysisTab.first);
    auto* routeWaypointsLayout = new QVBoxLayout(routeWaypointsGroupBox_);
    routeWaypointsLayout->setContentsMargins(12, 12, 12, 12);
    routeWaypointsLayout->setSpacing(8);

    auto* routeToolbarHost = new QWidget(routeWaypointsGroupBox_);
    auto* routeToolbarLayout = new QHBoxLayout(routeToolbarHost);
    routeToolbarLayout->setContentsMargins(0, 0, 0, 0);
    routeToolbarLayout->setSpacing(8);
    auto* routeToolBar = new QToolBar(routeToolbarHost);
    routeToolBar->setIconSize(QSize(16, 16));
    routeToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    routeToolBar->setMovable(false);
    routeToolBar->setFloatable(false);
    routeToolBar->addAction(generateInspectionRouteAction_);
    routeToolBar->addAction(regenerateInspectionRouteAction_);
    routeToolBar->addAction(clearInspectionRouteAction_);
    routeToolBar->addSeparator();
    routeToolBar->addAction(focusRouteWaypointAction_);
    routeToolBar->addAction(importRouteKmlAction_);
    routeToolBar->addAction(exportRouteKmlAction_);
    routeToolBar->addAction(exportRouteDjiKmzAction_);
    routeToolbarLayout->addWidget(routeToolBar, 1);

    routeWaypointsTableWidget_ = new QTableWidget(routeWaypointsGroupBox_);
    routeWaypointsTableWidget_->setColumnCount(6);
    routeWaypointsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    routeWaypointsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routeWaypointsTableWidget_->setAlternatingRowColors(true);
    routeWaypointsTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    routeWaypointsTableWidget_->setHorizontalHeaderLabels({
        tr("Index"),
        tr("X"),
        tr("Y"),
        tr("Z"),
        tr("Speed"),
        tr("Chainage")
    });
    routeWaypointsTableWidget_->verticalHeader()->setVisible(false);
    routeWaypointsTableWidget_->horizontalHeader()->setStretchLastSection(false);
    routeWaypointsTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    routeWaypointsTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    routeWaypointsTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    routeWaypointsTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    routeWaypointsTableWidget_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    routeWaypointsTableWidget_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    routeWaypointsTableWidget_->setMinimumHeight(220);

    routeWaypointsLayout->addWidget(routeToolbarHost);
    routeWaypointsLayout->addWidget(routeWaypointsTableWidget_, 1);

    navigationGroupBox_ = new QGroupBox(tr("Navigation"), navigationTab.first);
    auto* tipsLayout = new QVBoxLayout(navigationGroupBox_);
    navigationTipsLabel_ = new QLabel(navigationGroupBox_);
    navigationTipsLabel_->setWordWrap(true);
    tipsLayout->addWidget(navigationTipsLabel_);

    auto* navigationToggleContainer = new QWidget(navigationGroupBox_);
    navigationToggleLayout_ = new QFormLayout(navigationToggleContainer);
    navigationToggleLayout_->setContentsMargins(0, 0, 0, 0);
    navigationToggleLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    navigationToggleLayout_->setFormAlignment(Qt::AlignTop);

    invertOrbitCheckBox_ = new QCheckBox(tr("Invert orbit drag"), navigationToggleContainer);
    invertPanCheckBox_ = new QCheckBox(tr("Invert pan drag"), navigationToggleContainer);
    invertWheelCheckBox_ = new QCheckBox(tr("Invert wheel zoom"), navigationToggleContainer);
    navigationToggleLayout_->addRow(QString(), invertOrbitCheckBox_);
    navigationToggleLayout_->addRow(QString(), invertPanCheckBox_);
    navigationToggleLayout_->addRow(QString(), invertWheelCheckBox_);
    tipsLayout->addWidget(navigationToggleContainer);

    overviewTab.second->addWidget(datasetGroupBox_);
    overviewTab.second->addStretch(1);
    towerTab.second->addWidget(towerToolbarHost);
    towerTab.second->addWidget(towerPanel, 1);
    issueTab.second->addWidget(issueToolbarHost);
    issueTab.second->addWidget(issuePanel, 1);
    renderingTab.second->addWidget(renderingGroupBox_);
    renderingTab.second->addWidget(classificationColorsGroupBox_);
    renderingTab.second->addStretch(1);
    measurementTab.second->addWidget(measurementToolbarHost);
    measurementTab.second->addWidget(measurementGroupBox_);
    measurementTab.second->addWidget(clearanceGroupBox_);
    measurementTab.second->addWidget(clearanceSegmentsGroupBox_);
    measurementTab.second->addStretch(1);
    analysisTab.second->addWidget(analysisToolbarHost);
    analysisTab.second->addWidget(analysisParametersGroupBox_);
    analysisTab.second->addWidget(vegetationRisksGroupBox_, 1);
    analysisTab.second->addWidget(routePlanningGroupBox_);
    analysisTab.second->addWidget(routeWaypointsGroupBox_, 1);
    analysisTab.second->addStretch(1);
    navigationTab.second->addWidget(navigationGroupBox_);
    navigationTab.second->addStretch(1);

    inspectorTabWidget_->addTab(overviewTab.first, QString());
    inspectorTabWidget_->addTab(towerTab.first, QString());
    inspectorTabWidget_->addTab(issueTab.first, QString());
    inspectorTabWidget_->addTab(renderingTab.first, QString());
    inspectorTabWidget_->addTab(measurementTab.first, QString());
    inspectorTabWidget_->addTab(analysisTab.first, QString());
    inspectorTabWidget_->addTab(navigationTab.first, QString());

    inspectorTabWidget_->setStyleSheet(
        "QWidget {"
        "background-color: #f6f8fb;"
        "color: #1f2937;"
        "}"
        "QTabWidget::pane {"
        "border: 1px solid #d6dde8;"
        "border-radius: 10px;"
        "top: -1px;"
        "}"
        "QTabBar::tab {"
        "background-color: #eef2f7;"
        "border: 1px solid #d6dde8;"
        "border-bottom: none;"
        "border-top-left-radius: 8px;"
        "border-top-right-radius: 8px;"
        "padding: 8px 14px;"
        "margin-right: 4px;"
        "color: #475569;"
        "font-weight: 600;"
        "}"
        "QTabBar::tab:selected {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "background-color: #e2e8f0;"
        "}"
        "QToolBar {"
        "background: transparent;"
        "border: none;"
        "spacing: 6px;"
        "padding: 0;"
        "}"
        "QToolButton {"
        "background-color: #ffffff;"
        "border: 1px solid #d6dde8;"
        "border-radius: 6px;"
        "padding: 6px 10px;"
        "color: #1f2937;"
        "font-weight: 600;"
        "}"
        "QToolButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QToolButton:pressed {"
        "background-color: #dbeafe;"
        "border-color: #60a5fa;"
        "color: #0f172a;"
        "}"
        "QToolButton:checked {"
        "background-color: #2563eb;"
        "border-color: #1d4ed8;"
        "color: #eff6ff;"
        "}"
        "QToolButton:disabled {"
        "background-color: #f1f5f9;"
        "border-color: #e2e8f0;"
        "color: #94a3b8;"
        "}"
        "QToolButton::menu-indicator {"
        "subcontrol-origin: padding;"
        "subcontrol-position: right center;"
        "right: 8px;"
        "}"
        "QLabel {"
        "color: #1f2937;"
        "}"
        "QTreeWidget {"
        "background-color: #ffffff;"
        "border: 1px solid #d6dde8;"
        "border-radius: 8px;"
        "alternate-background-color: #f8fbff;"
        "padding: 4px;"
        "}"
        "QTreeWidget::item {"
        "padding: 4px 6px;"
        "}"
        "QTreeWidget::item:selected {"
        "background-color: #dbeafe;"
        "color: #0f172a;"
        "}"
        "QTableWidget {"
        "background-color: #ffffff;"
        "alternate-background-color: #f8fafc;"
        "gridline-color: #e2e8f0;"
        "color: #0f172a;"
        "}"
        "QTableWidget::item:selected {"
        "background-color: #dbeafe;"
        "color: #0f172a;"
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
        "color: #0f172a;"
        "}"
        "QHeaderView::section:pressed {"
        "background-color: #bfdbfe;"
        "color: #0f172a;"
        "}"
        "QTableCornerButton::section {"
        "background-color: #e2e8f0;"
        "border: 1px solid #cbd5e1;"
        "}"
        "QGroupBox {"
        "font-weight: 600;"
        "background-color: #ffffff;"
        "border: 1px solid #d6dde8;"
        "border-radius: 8px;"
        "margin-top: 8px;"
        "padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 8px;"
        "padding: 0 4px;"
        "color: #334155;"
        "background-color: #f6f8fb;"
        "}"
        "QPushButton, QComboBox, QSpinBox, QDoubleSpinBox {"
        "background-color: #ffffff;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "min-height: 32px;"
        "padding: 4px 10px;"
        "color: #111827;"
        "}"
        "QComboBox {"
        "padding-right: 30px;"
        "}"
        "QSpinBox, QDoubleSpinBox {"
        "padding-right: 20px;"
        "}"
        "QComboBox::drop-down {"
        "subcontrol-origin: padding;"
        "subcontrol-position: top right;"
        "width: 24px;"
        "border: none;"
        "}"
        "QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "width: 18px;"
        "border: none;"
        "}"
        "QComboBox QAbstractItemView {"
        "padding: 4px 0;"
        "selection-background-color: #dbeafe;"
        "selection-color: #111827;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "min-height: 24px;"
        "padding: 4px 10px;"
        "}"
        "QPushButton:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover {"
        "border-color: #94a3b8;"
        "}"
        "QCheckBox {"
        "color: #1f2937;"
        "}"
        "QToolTip {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "border: 1px solid #94a3b8;"
        "padding: 4px 8px;"
        "border-radius: 4px;"
        "}");

    QPalette toolTipPalette = QToolTip::palette();
    toolTipPalette.setColor(QPalette::ToolTipBase, QColor(248, 250, 252));
    toolTipPalette.setColor(QPalette::ToolTipText, QColor(15, 23, 42));
    QToolTip::setPalette(toolTipPalette);

    inspectorDock_->setWidget(inspectorTabWidget_);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);
}

void MainWindow::createProfileClassificationDock()
{
    profileClassificationDock_ = new QDockWidget(tr("Profile Classification"), this);
    profileClassificationDock_->setObjectName(QStringLiteral("profileClassificationDock"));
    profileClassificationDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    profileClassificationDock_->setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    profileClassificationDock_->setMinimumWidth(320);

    auto* profileClassificationSurface = new QWidget(profileClassificationDock_);
    profileClassificationSurface->setObjectName(QStringLiteral("profileClassificationSurface"));
    auto* profileClassificationSurfaceLayout = new QVBoxLayout(profileClassificationSurface);
    profileClassificationSurfaceLayout->setContentsMargins(10, 10, 10, 10);
    profileClassificationSurfaceLayout->setSpacing(8);
    if (profileClassificationGroupBox_ != nullptr) {
        profileClassificationSurfaceLayout->addWidget(profileClassificationGroupBox_);
    }
    profileClassificationSurfaceLayout->addStretch(1);
    profileClassificationSurface->setStyleSheet(QStringLiteral(
        "QWidget#profileClassificationSurface {"
        "background-color: #f3f7fc;"
        "border: 1px solid #d6e0eb;"
        "border-radius: 10px;"
        "}"
        "QGroupBox {"
        "font-weight: 600;"
        "background-color: #ffffff;"
        "color: #1f2937;"
        "border: 1px solid #d6dde8;"
        "border-radius: 8px;"
        "margin-top: 8px;"
        "padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 8px;"
        "padding: 0 4px;"
        "background-color: #f3f7fc;"
        "color: #334155;"
        "}"
        "QLabel {"
        "color: #1f2937;"
        "}"
        "QListWidget {"
        "background-color: #ffffff;"
        "border: 1px solid #d6dde8;"
        "border-radius: 6px;"
        "alternate-background-color: #f8fafc;"
        "color: #0f172a;"
        "}"
        "QListWidget::item:selected {"
        "background-color: #dbeafe;"
        "color: #0f172a;"
        "}"
        "QScrollBar:vertical {"
        "background: #edf2f7;"
        "width: 12px;"
        "margin: 2px;"
        "border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical {"
        "background: #94a3b8;"
        "min-height: 28px;"
        "border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "background: #64748b;"
        "}"
        "QScrollBar:horizontal {"
        "background: #edf2f7;"
        "height: 12px;"
        "margin: 2px;"
        "border-radius: 6px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "background: #94a3b8;"
        "min-width: 28px;"
        "border-radius: 6px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "background: #64748b;"
        "}"
        "QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page {"
        "background: transparent;"
        "border: none;"
        "}"
        "QPushButton {"
        "background-color: #ffffff;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "padding: 6px 10px;"
        "color: #0f172a;"
        "}"
        "QPushButton:hover {"
        "border-color: #94a3b8;"
        "background-color: #f8fafc;"
        "}"
        "QPushButton:disabled {"
        "background-color: #f1f5f9;"
        "border-color: #e2e8f0;"
        "color: #94a3b8;"
        "}"));
    profileClassificationDock_->setWidget(profileClassificationSurface);

    addDockWidget(Qt::LeftDockWidgetArea, profileClassificationDock_);
    if (projectDock_ != nullptr) {
        tabifyDockWidget(projectDock_, profileClassificationDock_);
    }
    profileClassificationDock_->hide();
}

void MainWindow::createProfileDock()
{
    profileDock_ = new QDockWidget(tr("Span Profile"), this);
    profileDock_->setObjectName(QStringLiteral("spanProfileDock"));
    profileDock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    profileDock_->setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    profileDock_->setMinimumHeight(220);

    auto* profileSurface = new QWidget(profileDock_);
    profileSurface->setObjectName(QStringLiteral("spanProfileSurface"));
    auto* profileLayout = new QVBoxLayout(profileSurface);
    profileLayout->setContentsMargins(12, 12, 12, 12);
    profileLayout->setSpacing(8);

    auto* titleLabel = new QLabel(tr("Measured corridor profile"), profileSurface);
    titleLabel->setObjectName(QStringLiteral("spanProfileTitleLabel"));
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel#spanProfileTitleLabel {"
        "font-size: 14px;"
        "font-weight: 600;"
        "color: #0f172a;"
        "}"));

    auto* subtitleLabel = new QLabel(
        tr("The profile updates from the current measurement path, highlights clearance segments below the threshold, and overlays nearby towers and issues."),
        profileSurface);
    subtitleLabel->setObjectName(QStringLiteral("spanProfileSubtitleLabel"));
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setStyleSheet(QStringLiteral("color: #64748b;"));

    profilePlotWidget_ = new ProfilePlotWidget(profileSurface);
    profilePlotWidget_->setObjectName(QStringLiteral("spanProfilePlotWidget"));
    profilePlotWidget_->setStyleSheet(QStringLiteral(
        "ProfilePlotWidget#spanProfilePlotWidget {"
        "background: transparent;"
        "}"));

    profileLayout->addWidget(titleLabel);
    profileLayout->addWidget(subtitleLabel);
    profileLayout->addWidget(profilePlotWidget_, 1);

    profileSurface->setStyleSheet(QStringLiteral(
        "QWidget#spanProfileSurface {"
        "background-color: #eef4fb;"
        "border-top: 1px solid #d7e2f0;"
        "}"));

    profileDock_->setWidget(profileSurface);
    addDockWidget(Qt::BottomDockWidgetArea, profileDock_);
    profileDock_->hide();
}

void MainWindow::createLogDock()
{
    logDock_ = new QDockWidget(tr("Application Log"), this);
    logDock_->setObjectName(QStringLiteral("applicationLogDock"));
    logDock_->setAllowedAreas(
        Qt::BottomDockWidgetArea
        | Qt::TopDockWidgetArea);
    logDock_->setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    logDock_->setMinimumHeight(140);
    logDock_->setMaximumHeight(260);

    logTextEdit_ = new QTextEdit(logDock_);
    logTextEdit_->setReadOnly(true);
    logTextEdit_->setUndoRedoEnabled(false);
    logTextEdit_->setAcceptRichText(true);
    logTextEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
    logTextEdit_->document()->setMaximumBlockCount(500);
    logTextEdit_->document()->setDocumentMargin(12);
    logTextEdit_->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "background-color: #08111d;"
        "color: #dbe4f0;"
        "border: none;"
        "selection-background-color: #1d4ed8;"
        "selection-color: #f8fafc;"
        "font-family: 'Segoe UI', 'Microsoft YaHei UI';"
        "font-size: 11px;"
        "}"));

    logDock_->setWidget(logTextEdit_);
    addDockWidget(Qt::BottomDockWidgetArea, logDock_);
    logDock_->hide();
}

void MainWindow::createStatusBar()
{
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet(QStringLiteral(
        "QStatusBar {"
        "background-color: #eef2f7;"
        "color: #334155;"
        "border-top: 1px solid #d6dde8;"
        "}"));

    globalProgressBar_ = new QProgressBar(this);
    globalProgressBar_->setObjectName(QStringLiteral("globalOperationProgress"));
    globalProgressBar_->setRange(0, 1000);
    globalProgressBar_->setValue(0);
    globalProgressBar_->setTextVisible(true);
    globalProgressBar_->setFormat(QStringLiteral("%p%"));
    globalProgressBar_->setFixedWidth(220);
    globalProgressBar_->setVisible(false);
    globalProgressBar_->setStyleSheet(QStringLiteral(
        "QProgressBar#globalOperationProgress {"
        "background: #dbe4ef;"
        "color: #0f172a;"
        "border: 1px solid #c7d2e2;"
        "border-radius: 7px;"
        "text-align: center;"
        "padding: 1px;"
        "font-size: 11px;"
        "font-weight: 600;"
        "}"
        "QProgressBar#globalOperationProgress::chunk {"
        "background: #2563eb;"
        "border-radius: 6px;"
        "}"));
    statusBar()->addPermanentWidget(globalProgressBar_);
}

QWidget* MainWindow::createSliderControl(QSlider*& slider, QLabel*& valueLabel, int minimum, int maximum, int step)
{
    auto* container = new QWidget(renderingGroupBox_);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(minimum, maximum);
    slider->setSingleStep(step);
    slider->setPageStep(std::max(step, (maximum - minimum) / 10));
    slider->setTracking(true);
    slider->setTickInterval(step);

    valueLabel = new QLabel(container);
    valueLabel->setMinimumWidth(56);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "color: #475569;"
        "font-weight: 600;"
        "}"));

    layout->addWidget(slider, 1);
    layout->addWidget(valueLabel, 0);
    return container;
}

void MainWindow::updateSliderValueLabel(QSlider* slider, QLabel* valueLabel, const QString& formatText) const
{
    if (slider == nullptr || valueLabel == nullptr) {
        return;
    }

    valueLabel->setText(formatText.arg(QLocale().toString(slider->value())));
}

void MainWindow::updateVisualizationTooltips()
{
    const auto applyTooltip = [](QWidget* primary, QWidget* secondary, QWidget* tertiary, const QString& tooltip) {
        if (primary != nullptr) {
            primary->setToolTip(tooltip);
        }
        if (secondary != nullptr) {
            secondary->setToolTip(tooltip);
        }
        if (tertiary != nullptr) {
            tertiary->setToolTip(tooltip);
        }
    };

    if (pointSizeSlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>Point Size</b><br/>"
            "Controls the screen size of each rendered point.");
        applyTooltip(pointSizeSlider_, pointSizeValueLabel_, pointSizeControl_, tooltip);
    }
    if (pointOpacitySlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>Point Opacity</b><br/>"
            "Controls how solid each point appears.<br/>"
            "Lower values reveal deeper layers; higher values make the cloud denser and stronger.");
        applyTooltip(pointOpacitySlider_, pointOpacityValueLabel_, pointOpacityControl_, tooltip);
    }
    if (depthCueSlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>Depth Cue</b><br/>"
            "Adds distance-based fading for better front/back separation.<br/>"
            "Higher values make distant points fade more strongly.");
        applyTooltip(depthCueSlider_, depthCueValueLabel_, depthCueControl_, tooltip);
    }
    if (edlStrengthSlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>EDL-style Shading</b><br/>"
            "Enhances point edges and local depth contrast, similar to survey software display enhancement.<br/>"
            "Higher values produce stronger contour darkening and a more layered look.");
        applyTooltip(edlStrengthSlider_, edlStrengthValueLabel_, edlStrengthControl_, tooltip);
    }
    if (roundSplatsCheckBox_ != nullptr) {
        roundSplatsCheckBox_->setToolTip(tr(
            "<b>Round splats</b><br/>"
            "Draw points as circular splats instead of square pixels for a more natural survey-style point cloud look."));
    }
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("LAS Point Cloud Viewer"));

    openAction_->setText(tr("Open"));
    openAction_->setToolTip(tr("Open one or more LAS or LAZ datasets"));
    addPointCloudAction_->setText(tr("Add LAS Files"));
    addPointCloudAction_->setToolTip(tr("Add one or more LAS or LAZ datasets to the current project"));
    removeDatasetAction_->setText(tr("Remove Selected Dataset"));
    removeDatasetAction_->setToolTip(tr("Remove the selected LAS or LAZ dataset from the project"));
    locateDatasetAction_->setText(tr("Open Folder"));
    locateDatasetAction_->setToolTip(tr("Open the folder that contains the selected dataset"));
    copyDatasetPathAction_->setText(tr("Copy Path"));
    copyDatasetPathAction_->setToolTip(tr("Copy the full path of the selected dataset"));
    expandProjectTreeAction_->setText(tr("Expand All"));
    expandProjectTreeAction_->setToolTip(tr("Expand the project explorer tree"));
    collapseProjectTreeAction_->setText(tr("Collapse All"));
    collapseProjectTreeAction_->setToolTip(tr("Collapse the project explorer tree"));
    openProjectAction_->setText(tr("Open Project"));
    saveProjectAction_->setText(tr("Save Project"));
    saveProjectAsAction_->setText(tr("Save Project As"));
    projectCoordinateSystemsAction_->setText(tr("Project Properties"));
    projectCoordinateSystemsAction_->setToolTip(tr("Open project coordinate system settings"));
    clearAction_->setText(tr("Clear"));
    clearAction_->setToolTip(tr("Clear the current scene"));
    exitAction_->setText(tr("Exit"));
    fitSceneAction_->setText(tr("Fit Scene"));
    fitSceneAction_->setToolTip(tr("Reset to a fitted isometric view"));
    topViewAction_->setText(tr("Top"));
    frontViewAction_->setText(tr("Front"));
    rightViewAction_->setText(tr("Right"));
    showAxesAction_->setText(tr("Axes"));
    showBoundingBoxAction_->setText(tr("Bounds"));
    darkBackgroundAction_->setText(tr("Dark"));
    lightBackgroundAction_->setText(tr("Light"));
    rgbColorAction_->setText(tr("RGB"));
    elevationColorAction_->setText(tr("Elevation"));
    singleColorAction_->setText(tr("Single"));
    classificationColorAction_->setText(tr("Classification"));
    themeColorfulAction_->setText(tr("Colorful"));
    themeWhiteAction_->setText(tr("White"));
    themeDarkGrayAction_->setText(tr("Dark Gray"));
    measureAction_->setText(tr("Measure"));
    profileClassificationAction_->setText(tr("Profile Classify"));
    profileClassificationAction_->setToolTip(tr("Drag a rectangle to reclassify points. Hold Alt and drag left mouse to adjust view while the tool is active"));
    showProfileClassificationDockAction_->setText(tr("Classify Panel"));
    showProfileClassificationDockAction_->setToolTip(tr("Show or hide the profile classification dock"));
    saveProfileClassificationEditsAction_->setText(tr("Save Classify Result"));
    saveProfileClassificationEditsAction_->setToolTip(tr("Write current profile classification edits back to LAS files"));
    clearMeasurementAction_->setText(tr("Clear Measure"));
    undoProfileClassificationAction_->setText(tr("Undo Classify"));
    redoProfileClassificationAction_->setText(tr("Redo Classify"));
    clearProfileClassificationEditsAction_->setText(tr("Clear Classify Edits"));
    exportClearanceCsvAction_->setText(tr("Export Clearance CSV"));
    showProfileDockAction_->setText(tr("Profile View"));
    showProfileDockAction_->setToolTip(tr("Show or hide the span profile dock"));
    analyzeVegetationRisksAction_->setText(tr("Analyze Risks"));
    analyzeVegetationRisksAction_->setToolTip(tr("Analyze vegetation risks around the current measured corridor"));
    focusVegetationRiskAction_->setText(tr("Focus Current Risk"));
    createIssueFromRiskAction_->setText(tr("Create Issue"));
    createIssuesFromRisksAction_->setText(tr("Create All Issues"));
    clearVegetationRisksAction_->setText(tr("Clear Risks"));
    generateInspectionRouteAction_->setText(tr("Generate Route"));
    regenerateInspectionRouteAction_->setText(tr("Regenerate Route"));
    clearInspectionRouteAction_->setText(tr("Clear Route"));
    focusRouteWaypointAction_->setText(tr("Focus Route Point"));
    importRouteKmlAction_->setText(tr("Import Route KML"));
    exportRouteKmlAction_->setText(tr("Export Route KML"));
    exportRouteDjiKmzAction_->setText(tr("Export DJI KMZ"));
    startTowerEditAction_->setText(tr("Start Editing"));
    finishTowerEditAction_->setText(tr("Finish Editing"));
    addTowerAction_->setText(tr("Click To Add Tower"));
    insertTowerAction_->setText(tr("Insert Before Current"));
    moveTowerAction_->setText(tr("Move Current Tower"));
    editCurrentTowerAction_->setText(tr("Edit Current Tower"));
    focusTowerAction_->setText(tr("Focus Current Tower"));
    removeTowerAction_->setText(tr("Remove Current Tower"));
    clearTowersAction_->setText(tr("Clear Tower Markers"));
    cancelTowerToolAction_->setText(tr("Cancel Tower Tool"));
    showTowerXAction_->setText(tr("Show X"));
    showTowerYAction_->setText(tr("Show Y"));
    showTowerZAction_->setText(tr("Show Z"));
    startIssueMarkAction_->setText(tr("Mark Issue"));
    startIssueMarkAction_->setToolTip(tr("Click a point in the view to add an inspection issue"));
    cancelIssueToolAction_->setText(tr("Cancel Issue Tool"));
    focusIssueAction_->setText(tr("Focus Current Issue"));
    removeIssueAction_->setText(tr("Remove Current Issue"));
    clearIssuesAction_->setText(tr("Clear Issues"));
    exportIssuesCsvAction_->setText(tr("Export Issues CSV"));
    exportInspectionReportAction_->setText(tr("Export Inspection Report"));
    showLogAction_->setText(tr("Log"));
    showLogAction_->setToolTip(tr("Show or hide the log panel"));
    languageEnglishAction_->setText(QStringLiteral("English"));
    languageChineseAction_->setText(QStringLiteral("\u4e2d\u6587"));

    if (homePage_ != nullptr) {
        homePage_->setTitle(tr("Home"));
    }
    if (towerPage_ != nullptr) {
        towerPage_->setTitle(tr("Tower"));
    }
    if (appearancePage_ != nullptr) {
        appearancePage_->setTitle(tr("Appearance"));
    }
    if (datasetRibbonGroup_ != nullptr) {
        datasetRibbonGroup_->setTitle(tr("Dataset"));
    }
    if (cameraRibbonGroup_ != nullptr) {
        cameraRibbonGroup_->setTitle(tr("Camera"));
    }
    if (sceneRibbonGroup_ != nullptr) {
        sceneRibbonGroup_->setTitle(tr("Scene Guides"));
    }
    if (measureRibbonGroup_ != nullptr) {
        measureRibbonGroup_->setTitle(tr("Measure"));
    }
    if (workspaceRibbonGroup_ != nullptr) {
        workspaceRibbonGroup_->setTitle(tr("Workspace"));
    }
    if (towerRibbonGroup_ != nullptr) {
        towerRibbonGroup_->setTitle(tr("Tower Editing"));
    }
    if (colorRibbonGroup_ != nullptr) {
        colorRibbonGroup_->setTitle(tr("Point Colors"));
    }
    if (themeRibbonGroup_ != nullptr) {
        themeRibbonGroup_->setTitle(tr("Office Theme"));
    }
    if (languageRibbonGroup_ != nullptr) {
        languageRibbonGroup_->setTitle(tr("Language"));
    }

    if (projectDock_ != nullptr) {
        projectDock_->setWindowTitle(tr("Project Explorer"));
    }
    if (inspectorDock_ != nullptr) {
        inspectorDock_->setWindowTitle(tr("Scene Inspector"));
    }
    if (profileDock_ != nullptr) {
        profileDock_->setWindowTitle(tr("Span Profile"));
    }
    if (profileClassificationDock_ != nullptr) {
        profileClassificationDock_->setWindowTitle(tr("Profile Classification"));
    }
    if (logDock_ != nullptr) {
        logDock_->setWindowTitle(tr("Application Log"));
    }
    if (inspectorTabWidget_ != nullptr) {
        inspectorTabWidget_->setTabText(0, tr("Overview"));
        inspectorTabWidget_->setTabText(1, tr("Tower"));
        inspectorTabWidget_->setTabText(2, tr("Issues"));
        inspectorTabWidget_->setTabText(3, tr("Rendering"));
        inspectorTabWidget_->setTabText(4, tr("Measurement"));
        inspectorTabWidget_->setTabText(5, tr("Analysis"));
        inspectorTabWidget_->setTabText(6, tr("Navigation"));
    }
    if (datasetGroupBox_ != nullptr) {
        datasetGroupBox_->setTitle(tr("Dataset Summary"));
    }
    if (projectSearchEdit_ != nullptr) {
    projectSearchEdit_->setPlaceholderText(tr("Filter point clouds, images, or trajectories"));
    }
    if (towerTableWidget_ != nullptr) {
        towerTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Name"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z") });
    }
    if (issueTableWidget_ != nullptr) {
        issueTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Title"), tr("Severity"), tr("Status"), tr("Tower"), tr("Category") });
    }
    if (issueMenuButton_ != nullptr) {
        issueMenuButton_->setText(tr("Menu"));
    }
    if (renderingGroupBox_ != nullptr) {
        renderingGroupBox_->setTitle(tr("Rendering Controls"));
    }
    if (classificationColorsGroupBox_ != nullptr) {
        classificationColorsGroupBox_->setTitle(tr("Classification Mapping"));
    }
    if (classificationColorsTableWidget_ != nullptr) {
        classificationColorsTableWidget_->setHorizontalHeaderLabels({ tr("Show"), tr("Class ID"), tr("Class Name"), tr("Color") });
    }
    if (resetClassificationColorsButton_ != nullptr) {
        resetClassificationColorsButton_->setText(tr("Reset Defaults"));
    }
    updateProfileClassificationPanel();
    if (measurementGroupBox_ != nullptr) {
        measurementGroupBox_->setTitle(tr("Measurement"));
    }
    if (clearanceGroupBox_ != nullptr) {
        clearanceGroupBox_->setTitle(tr("Clearance Analysis"));
    }
    if (clearanceSegmentsGroupBox_ != nullptr) {
        clearanceSegmentsGroupBox_->setTitle(tr("Path Segment Details"));
    }
    if (analysisParametersGroupBox_ != nullptr) {
        analysisParametersGroupBox_->setTitle(tr("Vegetation Risk Analysis"));
    }
    if (vegetationRisksGroupBox_ != nullptr) {
        vegetationRisksGroupBox_->setTitle(tr("Detected Risk Clusters"));
    }
    if (routePlanningGroupBox_ != nullptr) {
        routePlanningGroupBox_->setTitle(tr("Inspection Route Planning"));
    }
    if (routeWaypointsGroupBox_ != nullptr) {
        routeWaypointsGroupBox_->setTitle(tr("Route Waypoints"));
    }
    if (navigationGroupBox_ != nullptr) {
        navigationGroupBox_->setTitle(tr("Navigation"));
    }
    if (towerDetailsGroupBox_ != nullptr) {
        towerDetailsGroupBox_->setTitle(tr("Selected Tower Details"));
    }
    if (issueDetailsGroupBox_ != nullptr) {
        issueDetailsGroupBox_->setTitle(tr("Selected Issue Details"));
    }
    if (projectSearchEdit_ != nullptr) {
    projectSearchEdit_->setPlaceholderText(tr("Filter point clouds, images, or trajectories"));
    }

    auto setFieldLabel = [](QFormLayout* layout, QWidget* field, const QString& text) {
        if (layout == nullptr || field == nullptr) {
            return;
        }
        if (auto* label = qobject_cast<QLabel*>(layout->labelForField(field))) {
            label->setText(text);
        }
    };

    setFieldLabel(datasetLayout_, datasetNameValueLabel_, tr("Name"));
    setFieldLabel(datasetLayout_, datasetPathValueLabel_, tr("Path"));
    setFieldLabel(datasetLayout_, datasetPointsValueLabel_, tr("Points"));
    setFieldLabel(datasetLayout_, datasetBoundsValueLabel_, tr("Bounds"));
    setFieldLabel(datasetLayout_, datasetExtentValueLabel_, tr("Extent"));
    setFieldLabel(datasetLayout_, datasetColorValueLabel_, tr("Color Source"));
    setFieldLabel(towerDetailsLayout_, towerCodeEdit_, tr("Code"));
    setFieldLabel(towerDetailsLayout_, towerLineNameEdit_, tr("Line"));
    setFieldLabel(towerDetailsLayout_, towerVoltageLevelEdit_, tr("Voltage"));
    setFieldLabel(towerDetailsLayout_, towerStructureTypeEdit_, tr("Tower Type"));
    setFieldLabel(towerDetailsLayout_, towerInspectionDateEdit_, tr("Inspection Date"));
    setFieldLabel(towerDetailsLayout_, towerStatusEdit_, tr("Tower Status"));
    setFieldLabel(towerDetailsLayout_, towerNotesEdit_, tr("Notes"));
    setFieldLabel(issueDetailsLayout_, issueTitleEdit_, tr("Title"));
    setFieldLabel(issueDetailsLayout_, issueCategoryComboBox_, tr("Category"));
    setFieldLabel(issueDetailsLayout_, issueSeverityComboBox_, tr("Severity"));
    setFieldLabel(issueDetailsLayout_, issueStatusComboBox_, tr("Issue Status"));
    setFieldLabel(issueDetailsLayout_, issueRelatedTowerComboBox_, tr("Related Tower"));
    setFieldLabel(issueDetailsLayout_, issueImagePathEdit_, tr("Image Path"));
    setFieldLabel(issueDetailsLayout_, issueLocationValueLabel_, tr("Location"));
    setFieldLabel(issueDetailsLayout_, issueCreatedAtValueLabel_, tr("Created At"));
    setFieldLabel(issueDetailsLayout_, issueDescriptionEdit_, tr("Description"));
    if (issueCategoryComboBox_ != nullptr && issueCategoryComboBox_->count() >= 5) {
        issueCategoryComboBox_->setItemText(0, tr("Vegetation"));
        issueCategoryComboBox_->setItemText(1, tr("Insulator"));
        issueCategoryComboBox_->setItemText(2, tr("Tower Body"));
        issueCategoryComboBox_->setItemText(3, tr("Channel Risk"));
        issueCategoryComboBox_->setItemText(4, tr("Other"));
    }
    if (issueSeverityComboBox_ != nullptr && issueSeverityComboBox_->count() >= 4) {
        issueSeverityComboBox_->setItemText(0, issueSeverityDisplayName(IssueSeverity::Info));
        issueSeverityComboBox_->setItemText(1, issueSeverityDisplayName(IssueSeverity::Minor));
        issueSeverityComboBox_->setItemText(2, issueSeverityDisplayName(IssueSeverity::Major));
        issueSeverityComboBox_->setItemText(3, issueSeverityDisplayName(IssueSeverity::Critical));
    }
    if (issueStatusComboBox_ != nullptr && issueStatusComboBox_->count() >= 3) {
        issueStatusComboBox_->setItemText(0, issueStatusDisplayName(IssueStatus::Open));
        issueStatusComboBox_->setItemText(1, issueStatusDisplayName(IssueStatus::Monitoring));
        issueStatusComboBox_->setItemText(2, issueStatusDisplayName(IssueStatus::Resolved));
    }

    setFieldLabel(renderingLayout_, pointSizeControl_, tr("Point Size"));
    setFieldLabel(renderingLayout_, pointOpacityControl_, tr("Point Opacity"));
    setFieldLabel(renderingLayout_, depthCueControl_, tr("Depth Cue"));
    setFieldLabel(renderingLayout_, edlStrengthControl_, tr("EDL-style Shading"));
    setFieldLabel(renderingLayout_, colorModeComboBox_, tr("Color Mode"));
    setFieldLabel(renderingLayout_, pointColorButton_, tr("Single Color"));
    setFieldLabel(renderingLayout_, backgroundColorButton_, tr("Background"));
    colorModeComboBox_->setItemText(0, tr("RGB"));
    colorModeComboBox_->setItemText(1, tr("Elevation Ramp"));
    colorModeComboBox_->setItemText(2, tr("Single Color"));
    if (colorModeComboBox_->count() >= 4) {
        colorModeComboBox_->setItemText(3, tr("Classification"));
    }
    roundSplatsCheckBox_->setText(tr("Round splats (survey style)"));
    axesCheckBox_->setText(tr("Show XYZ axes"));
    boundingBoxCheckBox_->setText(tr("Show bounding box"));
    updateSliderValueLabel(pointSizeSlider_, pointSizeValueLabel_, tr("%1 px"));
    updateSliderValueLabel(pointOpacitySlider_, pointOpacityValueLabel_, tr("%1%"));
    updateSliderValueLabel(depthCueSlider_, depthCueValueLabel_, tr("%1%"));
    updateSliderValueLabel(edlStrengthSlider_, edlStrengthValueLabel_, tr("%1%"));
    updateVisualizationTooltips();

    setFieldLabel(measurementLayout_, measurementStartValueLabel_, tr("Start Point"));
    setFieldLabel(measurementLayout_, measurementEndValueLabel_, tr("End Point"));
    setFieldLabel(measurementLayout_, measurementDistanceValueLabel_, tr("3D Distance"));
    setFieldLabel(measurementLayout_, measurementHorizontalDistanceValueLabel_, tr("Horizontal Distance"));
    setFieldLabel(measurementLayout_, measurementDeltaZValueLabel_, tr("Height Delta"));
    setFieldLabel(measurementLayout_, measurementSegmentsValueLabel_, tr("Path Segments"));
    setFieldLabel(clearanceLayout_, clearanceRulePresetComboBox_, tr("Rule Preset"));
    setFieldLabel(clearanceLayout_, clearanceThresholdSpinBox_, tr("Critical Threshold"));
    setFieldLabel(clearanceLayout_, clearanceRuleBandsValueLabel_, tr("Risk Bands"));
    setFieldLabel(clearanceLayout_, clearanceShortestValueLabel_, tr("Shortest Segment"));
    setFieldLabel(clearanceLayout_, clearanceWarningCountValueLabel_, tr("Risk Segments"));
    setFieldLabel(clearanceLayout_, clearanceStatusValueLabel_, tr("Status"));
    if (clearanceSegmentsSummaryLabel_ != nullptr) {
        clearanceSegmentsSummaryLabel_->setText(tr("Add at least two measured points to list corridor segments and export clearance details."));
    }
    if (clearanceSegmentsTableWidget_ != nullptr) {
        clearanceSegmentsTableWidget_->setHorizontalHeaderLabels({
            tr("Segment"),
            tr("From"),
            tr("To"),
            tr("Chainage"),
            tr("Horizontal"),
            tr("3D"),
            tr("dZ"),
            tr("Status")
        });
    }
    if (clearanceRulePresetComboBox_ != nullptr && clearanceRulePresetComboBox_->count() >= 4) {
        clearanceRulePresetComboBox_->setItemText(0, clearanceRulePresetDisplayName(ClearanceRulePreset::TransmissionCorridor));
        clearanceRulePresetComboBox_->setItemText(1, clearanceRulePresetDisplayName(ClearanceRulePreset::DistributionCorridor));
        clearanceRulePresetComboBox_->setItemText(2, clearanceRulePresetDisplayName(ClearanceRulePreset::StructureApproach));
        clearanceRulePresetComboBox_->setItemText(3, clearanceRulePresetDisplayName(ClearanceRulePreset::Custom));
    }
    measurementToggleButton_->setText(
        viewer_ != nullptr && viewer_->measurementEnabled() ? tr("Stop Measurement") : tr("Start Measurement"));
    measurementClearButton_->setText(tr("Clear Measurement"));
    if (clearanceThresholdSpinBox_ != nullptr) {
        clearanceThresholdSpinBox_->setSuffix(tr(" m"));
        clearanceThresholdSpinBox_->setSpecialValueText(tr("Disabled"));
    }
    if (vegetationSearchRadiusSpinBox_ != nullptr) {
        vegetationSearchRadiusSpinBox_->setSuffix(tr(" m"));
    }
    if (vegetationClusterGapSpinBox_ != nullptr) {
        vegetationClusterGapSpinBox_->setSuffix(tr(" m"));
    }
    if (analysisParametersLayout_ != nullptr) {
        setFieldLabel(analysisParametersLayout_, vegetationSearchRadiusSpinBox_, tr("Search Radius"));
        setFieldLabel(analysisParametersLayout_, vegetationClusterGapSpinBox_, tr("Cluster Gap"));
        setFieldLabel(analysisParametersLayout_, vegetationClusterPointCountSpinBox_, tr("Min Cluster Points"));
        setFieldLabel(analysisParametersLayout_, vegetationRiskCountValueLabel_, tr("Risk Count"));
        setFieldLabel(analysisParametersLayout_, vegetationRiskStatusValueLabel_, tr("Status"));
        setFieldLabel(analysisParametersLayout_, vegetationRiskSummaryLabel_, tr("Summary"));
    }
    if (routePlanningGroupBox_ != nullptr) {
        if (auto* routePlanningLayout = qobject_cast<QFormLayout*>(routePlanningGroupBox_->layout())) {
            setFieldLabel(routePlanningLayout, aircraftProfileComboBox_, tr("DJI Profile"));
            setFieldLabel(routePlanningLayout, routeSafetyHeightSpinBox_, tr("Safety Height"));
            setFieldLabel(routePlanningLayout, routeWaypointSpeedSpinBox_, tr("Waypoint Speed"));
            setFieldLabel(routePlanningLayout, routeWaypointSpacingSpinBox_, tr("Waypoint Spacing"));
            setFieldLabel(routePlanningLayout, routeSmoothingStrengthSpinBox_, tr("Smoothing"));
            setFieldLabel(routePlanningLayout, routeHeightOffsetSpinBox_, tr("Height Offset"));
            setFieldLabel(routePlanningLayout, routeStatusValueLabel_, tr("Status"));
            setFieldLabel(routePlanningLayout, routeSummaryValueLabel_, tr("Summary"));
        }
    }
    if (aircraftProfileComboBox_ != nullptr) {
        const QList<DjiAircraftProfile> profiles = supportedDjiAircraftProfiles();
        for (int index = 0; index < profiles.size() && index < aircraftProfileComboBox_->count(); ++index) {
            aircraftProfileComboBox_->setItemText(index, djiAircraftProfileDisplayName(profiles.at(index)));
        }
    }
    if (preferVegetationClassificationCheckBox_ != nullptr) {
        preferVegetationClassificationCheckBox_->setText(tr("Prefer LAS vegetation classifications when available"));
    }
    if (vegetationRisksTableWidget_ != nullptr) {
        vegetationRisksTableWidget_->setHorizontalHeaderLabels({
            tr("Index"),
            tr("Title"),
            tr("Severity"),
            tr("Min Distance"),
            tr("Chainage"),
            tr("Tower"),
            tr("Points")
        });
    }
    if (routeWaypointsTableWidget_ != nullptr) {
        routeWaypointsTableWidget_->setHorizontalHeaderLabels({
            tr("Index"),
            tr("X"),
            tr("Y"),
            tr("Z"),
            tr("Speed"),
            tr("Chainage")
        });
    }

    invertOrbitCheckBox_->setText(tr("Invert orbit drag"));
    invertPanCheckBox_->setText(tr("Invert pan drag"));
    invertWheelCheckBox_->setText(tr("Invert wheel zoom"));

    if (profileDock_ != nullptr) {
        if (auto* titleLabel = profileDock_->findChild<QLabel*>(QStringLiteral("spanProfileTitleLabel"))) {
            titleLabel->setText(tr("Measured corridor profile"));
        }
        if (auto* subtitleLabel = profileDock_->findChild<QLabel*>(QStringLiteral("spanProfileSubtitleLabel"))) {
            subtitleLabel->setText(tr("The profile updates from the current measurement path, highlights clearance segments below the threshold, and overlays nearby towers and issues."));
        }
    }

    if (viewer_ != nullptr) {
        setColorButtonAppearance(pointColorButton_, viewer_->visualizationOptions().singleColor, tr("Pick Color"));
        setColorButtonAppearance(backgroundColorButton_, viewer_->visualizationOptions().backgroundColor, tr("Pick Background"));
    }
    updateClassificationColorTable();
    updateProfileClassificationPanel();
    updateWindowControlButtons();
    rebuildProjectTree();
    updateDatasetPanel();
    updateNavigationHelpText();
    updateMeasurementPanel();
    updateTowerPanel();
    updateActionState();
    if (viewer_ != nullptr) {
        viewer_->update();
    }
}

void MainWindow::updateClassificationColorTable()
{
    if (classificationColorsTableWidget_ == nullptr || viewer_ == nullptr) {
        return;
    }

    const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();
    const QSignalBlocker blocker(classificationColorsTableWidget_);
    updatingClassificationColorTable_ = true;

    QList<int> classificationCodes;
    classificationCodes.reserve(static_cast<int>(kClassificationDisplayItems.size()) + options.classificationColors.size());
    std::set<int> seenCodes;
    for (const ClassificationDisplayItem& item : kClassificationDisplayItems) {
        if (item.code >= 0) {
            classificationCodes.append(item.code);
            seenCodes.insert(item.code);
        }
    }

    const auto appendConfiguredCode = [&classificationCodes, &seenCodes](int code) {
        if (code < 0 || code > 255 || seenCodes.count(code) > 0) {
            return;
        }
        classificationCodes.append(code);
        seenCodes.insert(code);
    };
    for (auto it = options.classificationColors.constBegin(); it != options.classificationColors.constEnd(); ++it) {
        appendConfiguredCode(it.key());
    }
    for (auto it = options.classificationVisibility.constBegin(); it != options.classificationVisibility.constEnd(); ++it) {
        appendConfiguredCode(it.key());
    }
    for (auto it = classificationNameOverrides_.constBegin(); it != classificationNameOverrides_.constEnd(); ++it) {
        appendConfiguredCode(it.key());
    }
    classificationCodes.append(-1);

    classificationColorsTableWidget_->setRowCount(classificationCodes.size());
    for (int row = 0; row < classificationCodes.size(); ++row) {
        const int classificationCode = classificationCodes.at(row);
        const bool visible = options.classificationVisibility.value(
            classificationCode,
            options.classificationVisibility.value(-1, true));
        const QColor color = classificationCode < 0
            ? options.classificationFallbackColor
            : options.classificationColors.value(classificationCode, options.classificationFallbackColor);

        auto* visibleItem = classificationColorsTableWidget_->item(row, 0);
        if (visibleItem == nullptr) {
            visibleItem = new QTableWidgetItem();
            classificationColorsTableWidget_->setItem(row, 0, visibleItem);
        }
        visibleItem->setFlags((visibleItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable) & ~Qt::ItemIsEditable);
        visibleItem->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
        visibleItem->setText(QString());
        visibleItem->setTextAlignment(Qt::AlignCenter);
        visibleItem->setData(Qt::UserRole, classificationCode);

        auto* classItem = classificationColorsTableWidget_->item(row, 1);
        if (classItem == nullptr) {
            classItem = new QTableWidgetItem();
            classificationColorsTableWidget_->setItem(row, 1, classItem);
        }
        classItem->setFlags((classItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        classItem->setText(classificationCode < 0 ? tr("Other") : QLocale().toString(classificationCode));
        classItem->setTextAlignment(Qt::AlignCenter);
        classItem->setData(Qt::UserRole, classificationCode);

        auto* nameItem = classificationColorsTableWidget_->item(row, 2);
        if (nameItem == nullptr) {
            nameItem = new QTableWidgetItem();
            classificationColorsTableWidget_->setItem(row, 2, nameItem);
        }
        nameItem->setFlags(nameItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
        nameItem->setText(classificationDisplayName(classificationCode, classificationNameOverrides_));
        nameItem->setData(Qt::UserRole, classificationCode);

        auto* colorItem = classificationColorsTableWidget_->item(row, 3);
        if (colorItem == nullptr) {
            colorItem = new QTableWidgetItem();
            classificationColorsTableWidget_->setItem(row, 3, colorItem);
        }
        colorItem->setFlags((colorItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        colorItem->setText(color.name(QColor::HexRgb).toUpper());
        colorItem->setTextAlignment(Qt::AlignCenter);
        colorItem->setData(Qt::UserRole, classificationCode);
        colorItem->setBackground(color);
        const int luminance = static_cast<int>(std::lround(0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()));
        colorItem->setForeground(luminance < 140 ? QColor(248, 250, 252) : QColor(15, 23, 42));
        colorItem->setToolTip(tr("Double-click to change this class color."));
    }

    classificationColorsTableWidget_->resizeRowsToContents();
    adjustClassificationColorTableHeight();
    if (classificationColorsGroupBox_ != nullptr) {
        classificationColorsGroupBox_->setEnabled(viewer_->hasPointCloud());
    }
    if (resetClassificationColorsButton_ != nullptr) {
        resetClassificationColorsButton_->setEnabled(viewer_->hasPointCloud());
    }

    updatingClassificationColorTable_ = false;
}

void MainWindow::adjustClassificationColorTableHeight()
{
    if (classificationColorsTableWidget_ == nullptr) {
        return;
    }

    int contentHeight = classificationColorsTableWidget_->frameWidth() * 2;
    if (classificationColorsTableWidget_->horizontalHeader() != nullptr
        && !classificationColorsTableWidget_->horizontalHeader()->isHidden()) {
        contentHeight += classificationColorsTableWidget_->horizontalHeader()->height();
    }

    for (int row = 0; row < classificationColorsTableWidget_->rowCount(); ++row) {
        contentHeight += classificationColorsTableWidget_->rowHeight(row);
    }

    contentHeight += 4;
    classificationColorsTableWidget_->setMinimumHeight(contentHeight);
    classificationColorsTableWidget_->setMaximumHeight(contentHeight);
}

void MainWindow::updateProfileClassificationPanel()
{
    if (viewer_ == nullptr || profileClassificationGroupBox_ == nullptr) {
        return;
    }

    profileClassificationGroupBox_->setTitle(tr("3D Profile Classification"));
    if (profileClassificationToggleButton_ != nullptr) {
        profileClassificationToggleButton_->setText(
            viewer_->profileClassificationModeEnabled() ? tr("Exit Tool") : tr("Start Tool"));
    }
    if (profileClassificationSelectAllButton_ != nullptr) {
        profileClassificationSelectAllButton_->setText(tr("Select All"));
    }
    if (profileClassificationClearSelectionButton_ != nullptr) {
        profileClassificationClearSelectionButton_->setText(tr("Clear Sources"));
    }
    if (profileClassificationUndoButton_ != nullptr) {
        profileClassificationUndoButton_->setText(tr("Undo"));
    }
    if (profileClassificationRedoButton_ != nullptr) {
        profileClassificationRedoButton_->setText(tr("Redo"));
    }
    if (profileClassificationClearEditsButton_ != nullptr) {
        profileClassificationClearEditsButton_->setText(tr("Clear Edits"));
    }

    const bool hasPointCloud = viewer_->hasPointCloud();
    const bool sceneReady = hasPointCloud;
    const bool toolBusy = viewer_->profileClassificationTaskActive();
    profileClassificationGroupBox_->setEnabled(hasPointCloud);
    if (profileClassificationToggleButton_ != nullptr) {
        profileClassificationToggleButton_->setEnabled(sceneReady && !toolBusy);
    }
    if (profileClassificationSelectAllButton_ != nullptr) {
        profileClassificationSelectAllButton_->setEnabled(sceneReady && !toolBusy);
    }
    if (profileClassificationClearSelectionButton_ != nullptr) {
        profileClassificationClearSelectionButton_->setEnabled(sceneReady && !toolBusy);
    }
    if (profileClassificationUndoButton_ != nullptr) {
        profileClassificationUndoButton_->setEnabled(sceneReady && viewer_->canUndoClassificationEdits() && !toolBusy);
    }
    if (profileClassificationRedoButton_ != nullptr) {
        profileClassificationRedoButton_->setEnabled(sceneReady && viewer_->canRedoClassificationEdits() && !toolBusy);
    }
    if (profileClassificationClearEditsButton_ != nullptr) {
        profileClassificationClearEditsButton_->setEnabled(sceneReady && viewer_->classificationEditedPointCount() > 0 && !toolBusy);
    }
    if (profileClassificationSaveButton_ != nullptr) {
        profileClassificationSaveButton_->setText(tr("Save Result"));
        profileClassificationSaveButton_->setEnabled(hasPointCloud && viewer_->classificationEditedPointCount() > 0 && !toolBusy);
    }

    if (profileClassificationSourceListWidget_ != nullptr) {
        const QSignalBlocker blocker(profileClassificationSourceListWidget_);
        for (int row = 0; row < profileClassificationSourceListWidget_->count(); ++row) {
            QListWidgetItem* item = profileClassificationSourceListWidget_->item(row);
            if (item == nullptr) {
                continue;
            }

            const int classificationCode = item->data(Qt::UserRole).toInt();
            item->setText(QStringLiteral("%1 - %2")
                .arg(QLocale().toString(classificationCode))
                .arg(classificationDisplayName(classificationCode, classificationNameOverrides_)));
            item->setCheckState(viewer_->profileClassificationSourceClasses().contains(classificationCode)
                ? Qt::Checked
                : Qt::Unchecked);
        }
        profileClassificationSourceListWidget_->setEnabled(sceneReady && !toolBusy);
    }

    if (profileClassificationTargetListWidget_ != nullptr) {
        const QSignalBlocker blocker(profileClassificationTargetListWidget_);
        int targetRow = -1;
        for (int row = 0; row < profileClassificationTargetListWidget_->count(); ++row) {
            QListWidgetItem* item = profileClassificationTargetListWidget_->item(row);
            if (item == nullptr) {
                continue;
            }

            const int classificationCode = item->data(Qt::UserRole).toInt();
            item->setText(QStringLiteral("%1 - %2")
                .arg(QLocale().toString(classificationCode))
                .arg(classificationDisplayName(classificationCode, classificationNameOverrides_)));
            if (classificationCode == viewer_->profileClassificationTargetClass()) {
                targetRow = row;
            }
        }
        profileClassificationTargetListWidget_->setCurrentRow(targetRow >= 0 ? targetRow : 0);
        profileClassificationTargetListWidget_->setEnabled(sceneReady && !toolBusy);
    }

    if (profileClassificationStatusLabel_ != nullptr) {
        if (!hasPointCloud) {
            profileClassificationStatusLabel_->setText(tr("Load a point cloud and switch to a stable scene before using profile classification."));
        } else if (toolBusy) {
            profileClassificationStatusLabel_->setText(tr("Profile classification is processing the current rectangular selection."));
        } else {
            profileClassificationStatusLabel_->setText(
                tr("Source classes %1 | Target class %2 | Edited points %3 | Save state %4")
                    .arg(QLocale().toString(viewer_->profileClassificationSourceClasses().size()))
                    .arg(QLocale().toString(viewer_->profileClassificationTargetClass()))
                    .arg(QLocale().toString(viewer_->classificationEditedPointCount()))
                    .arg(classificationEditsDirty_ ? tr("unsaved") : tr("saved")));
        }
    }
}

bool MainWindow::saveProfileClassificationEditsToLas()
{
    if (savingProfileClassificationEdits_) {
        return false;
    }

    savingProfileClassificationEdits_ = true;
    struct SaveFlagResetGuard
    {
        bool* flag = nullptr;
        ~SaveFlagResetGuard()
        {
            if (flag != nullptr) {
                *flag = false;
            }
        }
    } saveFlagResetGuard { &savingProfileClassificationEdits_ };

    if (viewer_ == nullptr || viewer_->classificationEditedPointCount() <= 0) {
        classificationEditsDirty_ = false;
        updateProfileClassificationPanel();
        updateActionState();
        return true;
    }

#ifndef LAS_VIEWER_HAS_LASLIB
    const QString errorMessage = tr("This build does not support writing LAS/LAZ files.");
    showUserMessage(LogLevel::Error, errorMessage, 5000);
    showLightStyledMessageBox(
        this,
        QMessageBox::Warning,
        tr("Save Classification Results"),
        errorMessage,
        QMessageBox::Ok);
    return false;
#else
    const ClassificationEditStore::StoreMap editsByDataset = viewer_->classificationEditStore().editsByDataset();
    if (editsByDataset.isEmpty()) {
        classificationEditsDirty_ = false;
        updateProfileClassificationPanel();
        updateActionState();
        return true;
    }

    QHash<QString, qint64> datasetPointCounts;
    for (const PointCloudDatasetInfo& datasetInfo : viewer_->pointCloudDatasets()) {
        datasetPointCounts.insert(datasetInfo.filePath.toLower(), static_cast<qint64>(datasetInfo.pointCount));
    }

    int datasetCountToWrite = 0;
    qint64 totalPointsToWrite = 0;
    for (auto it = editsByDataset.constBegin(); it != editsByDataset.constEnd(); ++it) {
        if (it->isEmpty()) {
            continue;
        }

        ++datasetCountToWrite;
        const qint64 estimatedPointCount = datasetPointCounts.value(
            it.key().toLower(),
            static_cast<qint64>(it->size()));
        totalPointsToWrite += std::max<qint64>(estimatedPointCount, 1);
    }

    if (datasetCountToWrite <= 0) {
        classificationEditsDirty_ = false;
        updateProfileClassificationPanel();
        updateActionState();
        return true;
    }

    const int progressMaximum = static_cast<int>(std::min<qint64>(
        std::max<qint64>(totalPointsToWrite, 1),
        2000000000LL));
    qint64 processedPoints = 0;
    QStringList writtenDatasetNames;

    beginOperationProgress(tr("Saving classification results to LAS files..."));
    updateOperationProgress(tr("Preparing LAS write tasks..."), 0, progressMaximum);

    const auto failWithMessage = [this](const QString& message) -> bool {
        endOperationProgress();
        showUserMessage(LogLevel::Error, message, 7000);
        showLightStyledMessageBox(
            this,
            QMessageBox::Warning,
            tr("Save Classification Results"),
            message,
            QMessageBox::Ok);
        return false;
    };

    for (auto datasetIt = editsByDataset.constBegin(); datasetIt != editsByDataset.constEnd(); ++datasetIt) {
        const QString datasetPath = datasetIt.key();
        const ClassificationEditStore::DatasetEditMap& edits = datasetIt.value();
        if (edits.isEmpty()) {
            continue;
        }

        const QFileInfo datasetInfo(datasetPath);
        if (!datasetInfo.exists() || !datasetInfo.isFile()) {
            return failWithMessage(tr("Failed to save classification result: dataset file not found (%1).")
                .arg(datasetPath));
        }

        const QString suffix = datasetInfo.suffix().isEmpty() ? QStringLiteral("las") : datasetInfo.suffix();
        const QString tempFilePath = datasetInfo.absolutePath()
            + QDir::separator()
            + QStringLiteral("%1.__classify_tmp_%2.%3")
                .arg(datasetInfo.completeBaseName())
                .arg(QString::number(QDateTime::currentMSecsSinceEpoch()))
                .arg(suffix);
        const QString backupFilePath = datasetPath + QStringLiteral(".__classify_backup");
        QFile::remove(tempFilePath);
        QFile::remove(backupFilePath);

        LASreadOpener readOpener;
        const QByteArray datasetPathNative = QDir::toNativeSeparators(datasetPath).toLocal8Bit();
        readOpener.set_file_name(datasetPathNative.constData());
        std::unique_ptr<LASreader> reader(readOpener.open());
        if (!reader) {
            QFile::remove(tempFilePath);
            return failWithMessage(tr("Failed to open dataset for write-back (%1).")
                .arg(datasetInfo.fileName()));
        }

        LASwriteOpener writeOpener;
        const QByteArray tempFilePathNative = QDir::toNativeSeparators(tempFilePath).toLocal8Bit();
        writeOpener.set_file_name(tempFilePathNative.constData());
        std::unique_ptr<LASwriter> writer(writeOpener.open(&reader->header));
        const auto closeLasHandles = [&reader, &writer]() {
            if (writer) {
                writer->close();
                writer.reset();
            }
            if (reader) {
                reader->close();
                reader.reset();
            }
        };
        if (!writer) {
            closeLasHandles();
            QFile::remove(tempFilePath);
            return failWithMessage(tr("Failed to create output LAS file (%1).")
                .arg(datasetInfo.fileName()));
        }

        quint32 pointIndex = 0;
        const qint64 datasetTotalPoints = reader->npoints > 0
            ? static_cast<qint64>(reader->npoints)
            : std::max<qint64>(static_cast<qint64>(edits.size()), 1);
        while (reader->read_point()) {
            const auto editIt = edits.constFind(pointIndex);
            if (editIt != edits.constEnd()) {
                reader->point.set_classification(static_cast<U8>(std::clamp(editIt.value(), 0, 255)));
            }

            if (!writer->write_point(&reader->point)) {
                closeLasHandles();
                QFile::remove(tempFilePath);
                return failWithMessage(tr("Failed while writing classification result (%1).")
                    .arg(datasetInfo.fileName()));
            }

            ++pointIndex;
            ++processedPoints;
            if ((pointIndex & 0x1FFFu) == 0u) {
                const int progressValue = static_cast<int>(std::min<qint64>(processedPoints, progressMaximum));
                updateOperationProgress(
                    tr("Writing %1 (%2/%3 points)")
                        .arg(datasetInfo.fileName())
                        .arg(QLocale().toString(static_cast<qlonglong>(pointIndex)))
                        .arg(QLocale().toString(static_cast<qlonglong>(datasetTotalPoints))),
                    progressValue,
                    progressMaximum);
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }

        closeLasHandles();

        if (!QFile::rename(datasetPath, backupFilePath)) {
            QFile::remove(tempFilePath);
            return failWithMessage(tr("Failed to replace dataset while saving (%1).")
                .arg(datasetInfo.fileName()));
        }

        if (!QFile::rename(tempFilePath, datasetPath)) {
            QFile::rename(backupFilePath, datasetPath);
            QFile::remove(tempFilePath);
            return failWithMessage(tr("Failed to finalize LAS save (%1).")
                .arg(datasetInfo.fileName()));
        }
        QFile::remove(backupFilePath);

        writtenDatasetNames.append(datasetInfo.fileName());
        const int progressValue = static_cast<int>(std::min<qint64>(processedPoints, progressMaximum));
        updateOperationProgress(
            tr("Saved %1 (%2/%3 files)")
                .arg(datasetInfo.fileName())
                .arg(QLocale().toString(writtenDatasetNames.size()))
                .arg(QLocale().toString(datasetCountToWrite)),
            progressValue,
            progressMaximum);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    endOperationProgress();
    viewer_->commitClassificationEditsToPointCloudData();
    classificationEditsDirty_ = false;
    updateProfileClassificationPanel();
    updateActionState();

    const QString completionMessage = tr("Classification results were written to %1 LAS file(s).")
        .arg(QLocale().toString(writtenDatasetNames.size()));
    showUserMessage(LogLevel::Info, completionMessage, 5000);
    showLightStyledMessageBox(
        this,
        QMessageBox::Information,
        tr("Save Classification Results"),
        writtenDatasetNames.isEmpty()
            ? completionMessage
            : tr("%1\n\nSaved files: %2")
                .arg(completionMessage)
                .arg(writtenDatasetNames.join(QStringLiteral(", "))),
        QMessageBox::Ok);
    return true;
#endif
}

void MainWindow::promptSaveProfileClassificationEditsIfNeeded()
{
    if (handlingProfileClassificationExitPrompt_ || viewer_ == nullptr) {
        return;
    }

    if (!classificationEditsDirty_ || viewer_->classificationEditedPointCount() <= 0) {
        return;
    }

    handlingProfileClassificationExitPrompt_ = true;
    const QMessageBox::StandardButton choice = showLightStyledMessageBox(
        this,
        QMessageBox::Question,
        tr("Save Classification Results"),
        tr("Profile classification results are not saved. Write them to LAS files now?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    handlingProfileClassificationExitPrompt_ = false;

    if (choice == QMessageBox::Yes) {
        saveProfileClassificationEditsToLas();
    }
}

void MainWindow::createConnections()
{
    connect(openAction_, &QAction::triggered, this, [this]() { openPointCloud(); });
    connect(addPointCloudAction_, &QAction::triggered, this, [this]() { addPointCloudFiles(); });
    connect(removeDatasetAction_, &QAction::triggered, this, [this]() { removeSelectedDataset(); });
    connect(locateDatasetAction_, &QAction::triggered, this, [this]() {
        const QTreeWidgetItem* currentItem = projectTreeWidget_ != nullptr ? projectTreeWidget_->currentItem() : nullptr;
        const QString filePath = projectTreeItemFilePath(currentItem);
        if (filePath.isEmpty()) {
            return;
        }

        const QString folderPath = QFileInfo(filePath).absolutePath();
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath))) {
            showUserMessage(LogLevel::Warning, tr("Unable to open the selected file folder."), 3000);
        }
    });
    connect(copyDatasetPathAction_, &QAction::triggered, this, [this]() {
        const QTreeWidgetItem* currentItem = projectTreeWidget_ != nullptr ? projectTreeWidget_->currentItem() : nullptr;
        const QString filePath = projectTreeItemFilePath(currentItem);
        if (filePath.isEmpty()) {
            return;
        }

        if (QGuiApplication::clipboard() != nullptr) {
            QGuiApplication::clipboard()->setText(filePath);
            showUserMessage(LogLevel::Info, tr("Selected path copied."), 2000);
        }
    });
    connect(expandProjectTreeAction_, &QAction::triggered, this, [this]() {
        if (projectTreeWidget_ != nullptr) {
            projectTreeWidget_->expandAll();
        }
    });
    connect(collapseProjectTreeAction_, &QAction::triggered, this, [this]() {
        if (projectTreeWidget_ != nullptr) {
            projectTreeWidget_->collapseAll();
            if (projectTreeWidget_->topLevelItemCount() > 0) {
                projectTreeWidget_->topLevelItem(0)->setExpanded(true);
            }
        }
    });
    connect(openProjectAction_, &QAction::triggered, this, [this]() { openProject(); });
    connect(saveProjectAction_, &QAction::triggered, this, [this]() { saveProject(); });
    connect(saveProjectAsAction_, &QAction::triggered, this, [this]() { saveProjectAs(); });
    connect(projectCoordinateSystemsAction_, &QAction::triggered, this, [this]() { openProjectCoordinateSystems(); });
    connect(clearAction_, &QAction::triggered, this, [this]() { clearPointCloud(); });
    connect(exitAction_, &QAction::triggered, this, &QWidget::close);

    connect(fitSceneAction_, &QAction::triggered, viewer_, &PointCloudViewer::resetView);
    connect(topViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Top); });
    connect(frontViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Front); });
    connect(rightViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Right); });

    connect(showAxesAction_, &QAction::toggled, viewer_, &PointCloudViewer::setShowAxes);
    connect(showBoundingBoxAction_, &QAction::toggled, viewer_, &PointCloudViewer::setShowBoundingBox);
    connect(darkBackgroundAction_, &QAction::triggered, this, [this]() { viewer_->setBackgroundColor(kDarkBackground); });
    connect(lightBackgroundAction_, &QAction::triggered, this, [this]() { viewer_->setBackgroundColor(kLightBackground); });

    connect(rgbColorAction_, &QAction::triggered, this, [this]() { viewer_->setColorMode(PointCloudColorMode::Rgb); });
    connect(elevationColorAction_, &QAction::triggered, this, [this]() { viewer_->setColorMode(PointCloudColorMode::Elevation); });
    connect(singleColorAction_, &QAction::triggered, this, [this]() { viewer_->setColorMode(PointCloudColorMode::SingleColor); });
    connect(classificationColorAction_, &QAction::triggered, this, [this]() { viewer_->setColorMode(PointCloudColorMode::Classification); });

    connect(themeColorfulAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016Colorful); });
    connect(themeWhiteAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016White); });
    connect(themeDarkGrayAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016DarkGray); });
    connect(measureAction_, &QAction::toggled, viewer_, &PointCloudViewer::setMeasurementEnabled);
    connect(profileClassificationAction_, &QAction::toggled, viewer_, &PointCloudViewer::setProfileClassificationModeEnabled);
    connect(showProfileClassificationDockAction_, &QAction::toggled, this, [this](bool visible) {
        if (profileClassificationDock_ != nullptr && profileClassificationDock_->isVisible() != visible) {
            profileClassificationDock_->setVisible(visible);
        }
    });
    connect(saveProfileClassificationEditsAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr || viewer_->classificationEditedPointCount() <= 0) {
            return;
        }
        saveProfileClassificationEditsToLas();
    });
    connect(undoProfileClassificationAction_, &QAction::triggered, viewer_, &PointCloudViewer::undoClassificationEdit);
    connect(redoProfileClassificationAction_, &QAction::triggered, viewer_, &PointCloudViewer::redoClassificationEdit);
    connect(clearProfileClassificationEditsAction_, &QAction::triggered, viewer_, &PointCloudViewer::clearClassificationEdits);
    connect(measureAction_, &QAction::toggled, this, [this](bool enabled) {
        syncProfileDockForMeasurementMode(enabled);
    });
    connect(clearMeasurementAction_, &QAction::triggered, viewer_, &PointCloudViewer::clearMeasurement);
    connect(exportClearanceCsvAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        const ClearanceAnalysisResult analysisResult = analyzeClearancePath(
            viewer_->measurementResult().points,
            static_cast<float>(clearanceWarningThresholdMeters_));
        if (!analysisResult.isValid()) {
            showUserMessage(LogLevel::Warning, tr("Add at least two measured points before exporting clearance details."), 3000);
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Clearance CSV"),
            QStringLiteral("clearance_segments.csv"),
            tr("CSV Files (*.csv)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        const ClearanceRuleEvaluationResult ruleEvaluation = evaluateClearanceRules(
            analysisResult,
            { clearanceRulePreset_, static_cast<float>(clearanceWarningThresholdMeters_) });
        if (!ClearanceReportExporter::exportSegmentsCsv(filePath, analysisResult, &ruleEvaluation, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        showUserMessage(LogLevel::Info, tr("Clearance CSV exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    });
    connect(showProfileDockAction_, &QAction::toggled, this, [this](bool visible) {
        if (profileDock_ != nullptr && profileDock_->isVisible() != visible) {
            profileDock_->setVisible(visible);
        }
    });
    const auto analyzeCurrentVegetationRisks = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before running vegetation risk analysis."), 3000);
            return;
        }

        const ClearanceAnalysisResult pathAnalysis = analyzeClearancePath(
            viewer_->measurementResult().points,
            static_cast<float>(clearanceWarningThresholdMeters_));
        if (!pathAnalysis.isValid()) {
            showUserMessage(LogLevel::Warning, tr("Add at least two measured points before analyzing corridor risks."), 3500);
            return;
        }

        VegetationRiskAnalysisParameters parameters;
        parameters.searchRadius = static_cast<float>(vegetationSearchRadiusMeters_);
        parameters.criticalThreshold = static_cast<float>(clearanceWarningThresholdMeters_);
        parameters.clusterGap = static_cast<float>(vegetationClusterGapMeters_);
        parameters.minimumClusterPoints = vegetationClusterPointCount_;
        parameters.preferVegetationClassification = preferVegetationClassification_;
        parameters.preset = clearanceRulePreset_;

        vegetationRiskResults_ = analyzeVegetationRisks(
            *viewer_->pointCloudData(),
            pathAnalysis,
            viewer_->towerMarkers(),
            parameters).records;
        selectedVegetationRiskIndex_ = vegetationRiskResults_.isEmpty() ? -1 : 0;
        updateVegetationRiskPanel();
        rebuildProjectTree();
        updateActionState();
        if (inspectorTabWidget_ != nullptr) {
            inspectorTabWidget_->setCurrentIndex(5);
        }
        showUserMessage(
            LogLevel::Info,
            vegetationRiskResults_.isEmpty()
                ? tr("Vegetation risk analysis completed. No clusters were found near the current corridor.")
                : tr("Vegetation risk analysis completed. %1 cluster(s) detected.")
                    .arg(QLocale().toString(vegetationRiskResults_.size())),
            4000);
    };
    const auto createIssueFromRisk = [this](int riskIndex) -> bool {
        if (viewer_ == nullptr || riskIndex < 0 || riskIndex >= vegetationRiskResults_.size()) {
            return false;
        }

        const VegetationRiskRecord& risk = vegetationRiskResults_.at(riskIndex);
        InspectionIssue issue;
        issue.id = issueDefaultId();
        issue.title = risk.title.trimmed().isEmpty() ? nextDefaultIssueTitle() : risk.title;
        issue.category = tr("Vegetation");
        issue.severity = issueSeverityFromAnalysisSeverity(risk.severity);
        issue.status = IssueStatus::Open;
        issue.point = risk.point;
        issue.relatedTowerIndex = risk.nearestTowerIndex;
        issue.relatedTowerName = risk.nearestTowerName;
        issue.description = tr("%1\nRule: %2\nMin distance: %3 m\nChainage: %4 - %5 m")
            .arg(risk.notes)
            .arg(risk.sourceRule)
            .arg(formatCoordinate(risk.minimumDistance))
            .arg(formatCoordinate(risk.chainageStart))
            .arg(formatCoordinate(risk.chainageEnd));
        issue.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
        return viewer_->addInspectionIssue(issue);
    };
    connect(analyzeVegetationRisksAction_, &QAction::triggered, this, analyzeCurrentVegetationRisks);
    connect(focusVegetationRiskAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr || selectedVegetationRiskIndex_ < 0 || selectedVegetationRiskIndex_ >= vegetationRiskResults_.size()) {
            return;
        }
        viewer_->focusOnPoint(vegetationRiskResults_.at(selectedVegetationRiskIndex_).point);
    });
    connect(createIssueFromRiskAction_, &QAction::triggered, this, [this, createIssueFromRisk]() {
        if (createIssueFromRisk(selectedVegetationRiskIndex_)) {
            if (inspectorTabWidget_ != nullptr) {
                inspectorTabWidget_->setCurrentIndex(2);
            }
            updateIssuePanel();
            showUserMessage(LogLevel::Info, tr("Created an inspection issue from the selected vegetation risk."), 3000);
        }
    });
    connect(createIssuesFromRisksAction_, &QAction::triggered, this, [this, createIssueFromRisk]() {
        int createdCount = 0;
        for (int riskIndex = 0; riskIndex < vegetationRiskResults_.size(); ++riskIndex) {
            if (createIssueFromRisk(riskIndex)) {
                ++createdCount;
            }
        }
        updateIssuePanel();
        showUserMessage(
            LogLevel::Info,
            createdCount <= 0
                ? tr("No vegetation risks were converted into issues.")
                : tr("Created %1 inspection issue(s) from vegetation risks.").arg(QLocale().toString(createdCount)),
            3500);
    });
    connect(clearVegetationRisksAction_, &QAction::triggered, this, [this]() {
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        updateVegetationRiskPanel();
        rebuildProjectTree();
        updateActionState();
        showUserMessage(LogLevel::Info, tr("Vegetation risk results cleared."), 2500);
    });

    const auto syncRoutePlanningOptionsFromUi = [this]() {
        syncRoutePlanningFromProjectCoordinateSystems();
        if (aircraftProfileComboBox_ != nullptr) {
            const QVariant profileValue = aircraftProfileComboBox_->currentData();
            const int profileIndex = profileValue.isValid() ? profileValue.toInt() : static_cast<int>(routePlanningOptions_.aircraftProfile);
            routePlanningOptions_.aircraftProfile = static_cast<DjiAircraftProfile>(profileIndex);
        }
        if (routeSafetyHeightSpinBox_ != nullptr) {
            routePlanningOptions_.safety.safetyHeightMeters = static_cast<float>(routeSafetyHeightSpinBox_->value());
        }
        routePlanningOptions_.safety.globalTransitionalSpeedMps = 8.0f;
        routePlanningOptions_.safety.globalRthHeightMeters = 60.0f;
        routePlanningOptions_.safety.defaultGimbalPitchDeg = -45.0f;
        if (routeWaypointSpeedSpinBox_ != nullptr) {
            routePlanningOptions_.safety.defaultWaypointSpeedMps = static_cast<float>(routeWaypointSpeedSpinBox_->value());
        }
        if (routeWaypointSpacingSpinBox_ != nullptr) {
            routePlanningOptions_.generation.waypointSpacingMeters = static_cast<float>(routeWaypointSpacingSpinBox_->value());
        }
        if (routeSmoothingStrengthSpinBox_ != nullptr) {
            routePlanningOptions_.generation.smoothingStrengthPercent = static_cast<float>(routeSmoothingStrengthSpinBox_->value());
        }
        if (routeHeightOffsetSpinBox_ != nullptr) {
            routePlanningOptions_.safety.heightOffsetMeters = static_cast<float>(routeHeightOffsetSpinBox_->value());
        }
    };

    const auto applyRouteToViewer = [this]() {
        QList<PointRecord> waypointPoints;
        QStringList waypointLabels;
        waypointPoints.reserve(inspectionRoute_.waypoints.size());
        waypointLabels.reserve(inspectionRoute_.waypoints.size());
        for (int waypointIndex = 0; waypointIndex < inspectionRoute_.waypoints.size(); ++waypointIndex) {
            const InspectionWaypoint& waypoint = inspectionRoute_.waypoints.at(waypointIndex);
            waypointPoints.append(waypoint.localPoint);
            waypointLabels.append(waypoint.id.isEmpty() ? QString::number(waypointIndex + 1) : waypoint.id);
        }
        viewer_->setInspectionRouteWaypoints(waypointPoints, waypointLabels);
    };

    const auto regenerateInspectionRoute = [this, syncRoutePlanningOptionsFromUi, applyRouteToViewer]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before generating an inspection route."), 3000);
            return;
        }
        if (vegetationRiskResults_.isEmpty()) {
            showUserMessage(LogLevel::Warning, tr("Run vegetation risk analysis before generating an inspection route."), 3500);
            return;
        }

        syncRoutePlanningOptionsFromUi();
        inspectionRoute_ = generateInspectionRouteFromRisks(
            vegetationRiskResults_,
            viewer_->towerMarkers(),
            routePlanningOptions_.generation,
            routePlanningOptions_.safety);
        selectedRouteWaypointIndex_ = inspectionRoute_.waypoints.isEmpty() ? -1 : 0;
        applyRouteToViewer();
        viewer_->setSelectedInspectionRouteWaypointIndex(selectedRouteWaypointIndex_);
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();
        showUserMessage(
            LogLevel::Info,
            inspectionRoute_.waypoints.isEmpty()
                ? tr("No route waypoints were generated.")
                : tr("Generated inspection route with %1 waypoint(s).")
                    .arg(QLocale().toString(inspectionRoute_.waypoints.size())),
            3500);
    };

    connect(generateInspectionRouteAction_, &QAction::triggered, this, regenerateInspectionRoute);
    connect(regenerateInspectionRouteAction_, &QAction::triggered, this, regenerateInspectionRoute);

    connect(clearInspectionRouteAction_, &QAction::triggered, this, [this]() {
        inspectionRoute_ = InspectionRoute();
        selectedRouteWaypointIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();
        showUserMessage(LogLevel::Info, tr("Inspection route cleared."), 2500);
    });

    connect(focusRouteWaypointAction_, &QAction::triggered, this, [this]() {
        if (viewer_ == nullptr || selectedRouteWaypointIndex_ < 0 || selectedRouteWaypointIndex_ >= inspectionRoute_.waypoints.size()) {
            return;
        }
        viewer_->focusOnPoint(inspectionRoute_.waypoints.at(selectedRouteWaypointIndex_).localPoint, 0.22);
    });

    connect(importRouteKmlAction_, &QAction::triggered, this, [this, syncRoutePlanningOptionsFromUi, applyRouteToViewer]() {
        syncRoutePlanningOptionsFromUi();
        if (projectCoordinateSystems_.pointCloudCrs.code <= 0) {
            showUserMessage(LogLevel::Error, tr("Set the project point cloud CRS before importing route KML."), 4500);
            return;
        }

        const QString filePath = showStyledOpenFileNameDialog(
            this,
            tr("Import Route KML"),
            QString(),
            tr("KML Files (*.kml)"));
        if (filePath.isEmpty()) {
            return;
        }

        InspectionRoute importedWgs84;
        QString errorMessage;
        if (!importRouteKml(filePath, &importedWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        InspectionRoute importedLocal;
        if (!transformRouteFromWgs84(importedWgs84, projectCoordinateSystems_, &importedLocal, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        inspectionRoute_ = importedLocal;
        if (inspectionRoute_.name.trimmed().isEmpty()) {
            inspectionRoute_.name = QFileInfo(filePath).baseName();
        }
        selectedRouteWaypointIndex_ = inspectionRoute_.waypoints.isEmpty() ? -1 : 0;
        applyRouteToViewer();
        viewer_->setSelectedInspectionRouteWaypointIndex(selectedRouteWaypointIndex_);
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();
        showUserMessage(LogLevel::Info, tr("Imported route KML: %1").arg(QFileInfo(filePath).fileName()), 3500);
    });

    connect(exportRouteKmlAction_, &QAction::triggered, this, [this, syncRoutePlanningOptionsFromUi]() {
        if (inspectionRoute_.waypoints.isEmpty()) {
            showUserMessage(LogLevel::Warning, tr("Generate a route before exporting KML."), 3000);
            return;
        }

        syncRoutePlanningOptionsFromUi();
        if (projectCoordinateSystems_.pointCloudCrs.code <= 0) {
            showUserMessage(LogLevel::Error, tr("Set the project point cloud CRS before exporting route KML."), 4500);
            return;
        }

        InspectionRoute routeWgs84;
        QString errorMessage;
        if (!transformRouteToWgs84(inspectionRoute_, projectCoordinateSystems_, &routeWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Route KML"),
            QStringLiteral("inspection_route.kml"),
            tr("KML Files (*.kml)"));
        if (filePath.isEmpty()) {
            return;
        }

        if (!exportRouteKml(filePath, routeWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }
        showUserMessage(LogLevel::Info, tr("Route KML exported: %1").arg(QFileInfo(filePath).fileName()), 3500);
    });

    connect(exportRouteDjiKmzAction_, &QAction::triggered, this, [this, syncRoutePlanningOptionsFromUi]() {
        if (inspectionRoute_.waypoints.size() < 2) {
            showUserMessage(LogLevel::Warning, tr("Route needs at least 2 waypoints for DJI KMZ export."), 3500);
            return;
        }

        syncRoutePlanningOptionsFromUi();
        if (projectCoordinateSystems_.pointCloudCrs.code <= 0) {
            showUserMessage(LogLevel::Error, tr("Set the project point cloud CRS before exporting DJI KMZ."), 4500);
            return;
        }

        InspectionRoute routeWgs84;
        QString errorMessage;
        if (!transformRouteToWgs84(inspectionRoute_, projectCoordinateSystems_, &routeWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export DJI KMZ"),
            QStringLiteral("inspection_route.kmz"),
            tr("DJI Wayline KMZ (*.kmz)"));
        if (filePath.isEmpty()) {
            return;
        }

        if (!exportRouteDjiKmz(filePath, routeWgs84, routePlanningOptions_, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }
        showUserMessage(LogLevel::Info, tr("DJI KMZ exported: %1").arg(QFileInfo(filePath).fileName()), 3500);
    });

    connect(routeWaypointsTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
        selectedRouteWaypointIndex_ =
            (currentRow >= 0 && currentRow < inspectionRoute_.waypoints.size())
                ? currentRow
                : -1;
        if (viewer_ != nullptr) {
            viewer_->setSelectedInspectionRouteWaypointIndex(selectedRouteWaypointIndex_);
        }
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::selectedInspectionRouteWaypointChanged, this, [this](int index) {
        selectedRouteWaypointIndex_ = index;
        updateRoutePlanningPanel();
        updateActionState();
    });
    connect(aircraftProfileComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        const QVariant profileValue = aircraftProfileComboBox_->currentData();
        const int profileIndex = profileValue.isValid() ? profileValue.toInt() : static_cast<int>(routePlanningOptions_.aircraftProfile);
        routePlanningOptions_.aircraftProfile = static_cast<DjiAircraftProfile>(profileIndex);
    });
    connect(routeSafetyHeightSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        routePlanningOptions_.safety.safetyHeightMeters = static_cast<float>(value);
        updateRoutePlanningPanel();
    });
    connect(routeWaypointSpeedSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        routePlanningOptions_.safety.defaultWaypointSpeedMps = static_cast<float>(value);
        updateRoutePlanningPanel();
    });
    connect(routeWaypointSpacingSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        routePlanningOptions_.generation.waypointSpacingMeters = static_cast<float>(value);
        updateRoutePlanningPanel();
    });
    connect(routeSmoothingStrengthSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        routePlanningOptions_.generation.smoothingStrengthPercent = static_cast<float>(value);
        updateRoutePlanningPanel();
    });
    connect(routeHeightOffsetSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        routePlanningOptions_.safety.heightOffsetMeters = static_cast<float>(value);
        updateRoutePlanningPanel();
    });

    connect(pointSizeSlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setPointSize);
    connect(pointOpacitySlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setPointOpacity);
    connect(depthCueSlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setDepthCueStrength);
    connect(edlStrengthSlider_, &QSlider::valueChanged, viewer_, &PointCloudViewer::setEdlStrength);
    connect(pointSizeSlider_, &QSlider::valueChanged, this, [this](int) {
        updateSliderValueLabel(pointSizeSlider_, pointSizeValueLabel_, tr("%1 px"));
    });
    connect(pointOpacitySlider_, &QSlider::valueChanged, this, [this](int) {
        updateSliderValueLabel(pointOpacitySlider_, pointOpacityValueLabel_, tr("%1%"));
    });
    connect(depthCueSlider_, &QSlider::valueChanged, this, [this](int) {
        updateSliderValueLabel(depthCueSlider_, depthCueValueLabel_, tr("%1%"));
    });
    connect(edlStrengthSlider_, &QSlider::valueChanged, this, [this](int) {
        updateSliderValueLabel(edlStrengthSlider_, edlStrengthValueLabel_, tr("%1%"));
    });
    connect(
        colorModeComboBox_,
        qOverload<int>(&QComboBox::currentIndexChanged),
        viewer_,
        static_cast<void (PointCloudViewer::*)(int)>(&PointCloudViewer::setColorMode));
    connect(pointColorButton_, &QPushButton::clicked, this, [this]() { choosePointColor(); });
    connect(backgroundColorButton_, &QPushButton::clicked, this, [this]() { chooseBackgroundColor(); });
    connect(resetClassificationColorsButton_, &QPushButton::clicked, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        classificationNameOverrides_.clear();
        viewer_->resetClassificationColors();
        updateClassificationColorTable();
        updateProfileClassificationPanel();
    });
    connect(profileClassificationToggleButton_, &QPushButton::clicked, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        viewer_->setProfileClassificationModeEnabled(!viewer_->profileClassificationModeEnabled());
    });
    connect(profileClassificationSelectAllButton_, &QPushButton::clicked, this, [this]() {
        if (profileClassificationSourceListWidget_ == nullptr) {
            return;
        }

        for (int row = 0; row < profileClassificationSourceListWidget_->count(); ++row) {
            if (QListWidgetItem* item = profileClassificationSourceListWidget_->item(row)) {
                item->setCheckState(Qt::Checked);
            }
        }
    });
    connect(profileClassificationClearSelectionButton_, &QPushButton::clicked, this, [this]() {
        if (profileClassificationSourceListWidget_ == nullptr) {
            return;
        }

        for (int row = 0; row < profileClassificationSourceListWidget_->count(); ++row) {
            if (QListWidgetItem* item = profileClassificationSourceListWidget_->item(row)) {
                item->setCheckState(Qt::Unchecked);
            }
        }
    });
    connect(profileClassificationUndoButton_, &QPushButton::clicked, viewer_, &PointCloudViewer::undoClassificationEdit);
    connect(profileClassificationRedoButton_, &QPushButton::clicked, viewer_, &PointCloudViewer::redoClassificationEdit);
    connect(profileClassificationClearEditsButton_, &QPushButton::clicked, viewer_, &PointCloudViewer::clearClassificationEdits);
    connect(profileClassificationSaveButton_, &QPushButton::clicked, this, [this]() {
        if (viewer_ == nullptr || viewer_->classificationEditedPointCount() <= 0) {
            return;
        }
        saveProfileClassificationEditsToLas();
    });
    connect(profileClassificationSourceListWidget_, &QListWidget::itemChanged, this, [this](QListWidgetItem*) {
        if (viewer_ == nullptr || profileClassificationSourceListWidget_ == nullptr) {
            return;
        }

        QSet<int> selectedSourceClasses;
        for (int row = 0; row < profileClassificationSourceListWidget_->count(); ++row) {
            QListWidgetItem* item = profileClassificationSourceListWidget_->item(row);
            if (item != nullptr && item->checkState() == Qt::Checked) {
                selectedSourceClasses.insert(item->data(Qt::UserRole).toInt());
            }
        }
        viewer_->setProfileClassificationSourceClasses(selectedSourceClasses);
    });
    connect(profileClassificationTargetListWidget_, &QListWidget::currentRowChanged, this, [this](int currentRow) {
        if (viewer_ == nullptr || profileClassificationTargetListWidget_ == nullptr || currentRow < 0) {
            return;
        }

        QListWidgetItem* item = profileClassificationTargetListWidget_->item(currentRow);
        if (item != nullptr) {
            viewer_->setProfileClassificationTargetClass(item->data(Qt::UserRole).toInt());
        }
    });
    connect(classificationColorsTableWidget_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (viewer_ == nullptr || item == nullptr || updatingClassificationColorTable_) {
            return;
        }

        const int classificationCode = item->data(Qt::UserRole).toInt();
        if (item->column() == 0) {
            viewer_->setClassificationVisible(classificationCode, item->checkState() == Qt::Checked);
            return;
        }

        if (item->column() != 2) {
            return;
        }

        const QString trimmedName = item->text().trimmed();
        const QString defaultName = defaultClassificationDisplayName(classificationCode);
        if (trimmedName.isEmpty() || trimmedName == defaultName) {
            classificationNameOverrides_.remove(classificationCode);
        } else {
            classificationNameOverrides_.insert(classificationCode, trimmedName);
        }
        persistVisualizationSettings();
        updateClassificationColorTable();
        updateProfileClassificationPanel();
    });
    connect(classificationColorsTableWidget_, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (viewer_ == nullptr || classificationColorsTableWidget_ == nullptr || updatingClassificationColorTable_) {
            return;
        }
        if (column != 3) {
            return;
        }

        QTableWidgetItem* colorItem = classificationColorsTableWidget_->item(row, 3);
        if (colorItem == nullptr) {
            return;
        }

        const int classificationCode = colorItem->data(Qt::UserRole).toInt();
        const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();
        const QColor currentColor = classificationCode < 0
            ? options.classificationFallbackColor
            : options.classificationColors.value(classificationCode, options.classificationFallbackColor);
        const QColor selectedColor = showStyledColorDialog(this, currentColor, tr("Choose Classification Color"));
        if (!selectedColor.isValid()) {
            return;
        }

        if (classificationCode < 0) {
            viewer_->setClassificationFallbackColor(selectedColor);
        } else {
            viewer_->setClassificationColor(classificationCode, selectedColor);
        }

        if (viewer_->visualizationOptions().colorMode != PointCloudColorMode::Classification) {
            viewer_->setColorMode(PointCloudColorMode::Classification);
        }
        updateClassificationColorTable();
    });
    connect(roundSplatsCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setUseRoundSplats);
    connect(axesCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setShowAxes);
    connect(boundingBoxCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setShowBoundingBox);
    connect(invertOrbitCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertOrbitDrag);
    connect(invertPanCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertPanDrag);
    connect(invertWheelCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertWheelZoom);
    connect(measurementToggleButton_, &QPushButton::clicked, this, [this]() {
        viewer_->setMeasurementEnabled(!viewer_->measurementEnabled());
    });
    connect(measurementClearButton_, &QPushButton::clicked, viewer_, &PointCloudViewer::clearMeasurement);
    connect(clearanceThresholdSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        clearanceWarningThresholdMeters_ = value;
        persistMeasurementSettings();
        updateMeasurementPanel();
    });
    connect(clearanceRulePresetComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0 || clearanceRulePresetComboBox_ == nullptr) {
            return;
        }
        clearanceRulePreset_ = static_cast<ClearanceRulePreset>(clearanceRulePresetComboBox_->itemData(index).toInt());
        persistMeasurementSettings();
        updateMeasurementPanel();
        updateVegetationRiskPanel();
    });
    connect(vegetationSearchRadiusSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        vegetationSearchRadiusMeters_ = value;
        persistMeasurementSettings();
        updateVegetationRiskPanel();
    });
    connect(vegetationClusterGapSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        vegetationClusterGapMeters_ = value;
        persistMeasurementSettings();
    });
    connect(vegetationClusterPointCountSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        vegetationClusterPointCount_ = value;
        persistMeasurementSettings();
    });
    connect(preferVegetationClassificationCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        preferVegetationClassification_ = checked;
        persistMeasurementSettings();
        updateVegetationRiskPanel();
    });
    connect(clearanceSegmentsTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
        if (profilePlotWidget_ != nullptr) {
            profilePlotWidget_->setSelectedSegmentIndex(currentRow);
        }
    });
    connect(vegetationRisksTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
        selectedVegetationRiskIndex_ = (currentRow >= 0 && currentRow < vegetationRiskResults_.size()) ? currentRow : -1;
        updateVegetationRiskPanel();
    });

    const auto beginAddTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before adding tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }
        viewer_->beginTowerAddMode();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower add mode enabled. Click points continuously to add tower markers, or cancel the tool when finished."), 4500);
    };
    const auto beginInsertTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before inserting tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }

        const int currentRow = viewer_ != nullptr ? viewer_->selectedTowerIndex() : -1;
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            showUserMessage(LogLevel::Warning, tr("Select the current tower before inserting a new one."), 3000);
            return;
        }

        viewer_->setSelectedTowerIndex(currentRow);
        viewer_->beginTowerInsertMode(currentRow);
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Click a point in the view to insert a tower marker before the current one."), 4000);
    };
    const auto beginMoveTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before moving tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }

        const int currentRow = viewer_ != nullptr ? viewer_->selectedTowerIndex() : -1;
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            showUserMessage(LogLevel::Warning, tr("Select the current tower before moving it."), 3000);
            return;
        }

        viewer_->setSelectedTowerIndex(currentRow);
        viewer_->beginTowerMoveMode(currentRow);
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Click a point in the view to move the current tower marker."), 4000);
    };
    const auto editCurrentTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before moving tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }

        const int currentRow = viewer_ != nullptr ? viewer_->selectedTowerIndex() : -1;
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            showUserMessage(LogLevel::Warning, tr("Select the current tower before moving it."), 3000);
            return;
        }

        viewer_->setSelectedTowerIndex(currentRow);
        viewer_->beginTowerMoveMode(currentRow);
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Click a point in the view to move the current tower marker."), 4000);
    };
    const auto focusSelectedTower = [this]() {
        if (viewer_ == nullptr || towerTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = towerTableWidget_->currentRow();
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            return;
        }

        viewer_->focusOnPoint(viewer_->towerMarkers().at(currentRow).point);
    };
    const auto removeSelectedTower = [this]() {
        if (viewer_ == nullptr || towerTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = towerTableWidget_->currentRow();
        if (!viewer_->removeTowerMarker(currentRow)) {
            return;
        }

        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower marker removed."), 2500);
    };
    const auto clearAllTowers = [this]() {
        if (viewer_ == nullptr || viewer_->towerMarkers().isEmpty()) {
            return;
        }

        viewer_->clearTowerMarkers();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower markers cleared."), 2500);
    };
    const auto cancelTowerTool = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        viewer_->cancelTowerEditMode();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower tool cancelled."), 2500);
    };
    const auto startTowerEditing = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before editing tower markers."), 3000);
            return;
        }

        setTowerEditingEnabled(true);
        if (inspectorTabWidget_ != nullptr) {
            inspectorTabWidget_->setCurrentIndex(1);
        }
        showUserMessage(LogLevel::Info, tr("Tower editing started. Use the tools in the right dock to add, insert, move, rename, or remove tower markers."), 4500);
    };
    const auto finishTowerEditing = [this]() {
        setTowerEditingEnabled(false);
        showUserMessage(LogLevel::Info, tr("Tower editing finished."), 2500);
    };

    connect(startTowerEditAction_, &QAction::triggered, this, startTowerEditing);
    connect(finishTowerEditAction_, &QAction::triggered, this, finishTowerEditing);
    connect(addTowerAction_, &QAction::triggered, this, beginAddTower);
    connect(insertTowerAction_, &QAction::triggered, this, beginInsertTower);
    connect(moveTowerAction_, &QAction::triggered, this, beginMoveTower);
    connect(editCurrentTowerAction_, &QAction::triggered, this, editCurrentTower);
    connect(focusTowerAction_, &QAction::triggered, this, focusSelectedTower);
    connect(removeTowerAction_, &QAction::triggered, this, removeSelectedTower);
    connect(clearTowersAction_, &QAction::triggered, this, clearAllTowers);
    connect(cancelTowerToolAction_, &QAction::triggered, this, cancelTowerTool);
    connect(showTowerXAction_, &QAction::toggled, this, [this](bool checked) {
        if (towerTableWidget_ != nullptr) {
            towerTableWidget_->setColumnHidden(2, !checked);
        }
    });
    connect(showTowerYAction_, &QAction::toggled, this, [this](bool checked) {
        if (towerTableWidget_ != nullptr) {
            towerTableWidget_->setColumnHidden(3, !checked);
        }
    });
    connect(showTowerZAction_, &QAction::toggled, this, [this](bool checked) {
        if (towerTableWidget_ != nullptr) {
            towerTableWidget_->setColumnHidden(4, !checked);
        }
    });
    if (towerTableWidget_ != nullptr && towerTableWidget_->horizontalHeader() != nullptr) {
        towerTableWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(towerTableWidget_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            if (towerTableWidget_ == nullptr || viewer_ == nullptr) {
                return;
            }

            const QModelIndex index = towerTableWidget_->indexAt(pos);
            if (index.isValid()) {
                const int row = index.row();
                if (row >= 0 && row < towerTableWidget_->rowCount()) {
                    towerTableWidget_->setCurrentCell(row, 1);
                }
            }

            QMenu rowMenu(towerTableWidget_);
            rowMenu.addAction(focusTowerAction_);
            rowMenu.addAction(removeTowerAction_);
            rowMenu.addSeparator();
            rowMenu.addAction(editCurrentTowerAction_);
            rowMenu.addAction(moveTowerAction_);
            rowMenu.addAction(insertTowerAction_);
            rowMenu.addSeparator();
            rowMenu.addAction(addTowerAction_);
            rowMenu.addAction(clearTowersAction_);
            rowMenu.addAction(cancelTowerToolAction_);
            rowMenu.exec(towerTableWidget_->viewport()->mapToGlobal(pos));
        });

        towerTableWidget_->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(towerTableWidget_->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, [this](const QPoint& pos) {
            if (towerTableWidget_ == nullptr || towerTableWidget_->horizontalHeader() == nullptr) {
                return;
            }

            QMenu columnMenu(towerTableWidget_);
            columnMenu.addAction(showTowerXAction_);
            columnMenu.addAction(showTowerYAction_);
            columnMenu.addAction(showTowerZAction_);
            columnMenu.exec(towerTableWidget_->horizontalHeader()->mapToGlobal(pos));
        });
    }
    connect(towerTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
        if (viewer_ != nullptr) {
            viewer_->setSelectedTowerIndex(currentRow);
        }
        updateActionState();
        updateTowerPanel();
    });
    connect(towerTableWidget_, &QTableWidget::cellChanged, this, [this](int row, int column) {
        if (viewer_ == nullptr || towerTableWidget_ == nullptr || column != 1) {
            return;
        }

        if (!towerEditingEnabled_) {
            updateTowerPanel();
            return;
        }

        QTableWidgetItem* item = towerTableWidget_->item(row, column);
        if (item == nullptr) {
            return;
        }

        if (!viewer_->setTowerMarkerName(row, item->text())) {
            showUserMessage(LogLevel::Warning, tr("Tower marker name cannot be empty."), 3000);
            updateTowerPanel();
            return;
        }

        updateTowerPanel();
    });
    const auto commitTowerDetails = [this]() {
        if (updatingTowerDetails_ || viewer_ == nullptr) {
            return;
        }

        const int selectedTowerIndex = viewer_->selectedTowerIndex();
        if (selectedTowerIndex < 0 || selectedTowerIndex >= viewer_->towerMarkers().size()) {
            return;
        }

        TowerRecord towerRecord = viewer_->towerMarkers().at(selectedTowerIndex);
        towerRecord.code = towerCodeEdit_->text().trimmed();
        towerRecord.lineName = towerLineNameEdit_->text().trimmed();
        towerRecord.voltageLevel = towerVoltageLevelEdit_->text().trimmed();
        towerRecord.structureType = towerStructureTypeEdit_->text().trimmed();
        towerRecord.inspectionDate = towerInspectionDateEdit_->text().trimmed();
        towerRecord.status = towerStatusEdit_->text().trimmed();
        towerRecord.notes = towerNotesEdit_->toPlainText().trimmed();
        if (viewer_->setTowerRecord(selectedTowerIndex, towerRecord)) {
            updateTowerPanel();
        }
    };
    connect(towerCodeEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerLineNameEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerVoltageLevelEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerStructureTypeEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerInspectionDateEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerStatusEdit_, &QLineEdit::editingFinished, this, commitTowerDetails);
    connect(towerNotesEdit_, &QPlainTextEdit::textChanged, this, commitTowerDetails);

    const auto beginIssueMarking = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before marking issues."), 3000);
            return;
        }

        viewer_->beginIssueAddMode();
        if (inspectorTabWidget_ != nullptr) {
            inspectorTabWidget_->setCurrentIndex(2);
        }
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Issue marking enabled. Click a point in the view to add an issue, or right-click to cancel."), 4500);
    };
    const auto cancelIssueTool = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        viewer_->cancelIssueEditMode();
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Issue tool cancelled."), 2500);
    };
    const auto focusSelectedIssue = [this]() {
        if (viewer_ == nullptr || issueTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = issueTableWidget_->currentRow();
        if (currentRow < 0 || currentRow >= viewer_->inspectionIssues().size()) {
            return;
        }

        viewer_->focusOnPoint(viewer_->inspectionIssues().at(currentRow).point, 0.2);
    };
    const auto removeSelectedIssue = [this]() {
        if (viewer_ == nullptr || issueTableWidget_ == nullptr) {
            return;
        }

        if (viewer_->removeInspectionIssue(issueTableWidget_->currentRow())) {
            updateIssuePanel();
            showUserMessage(LogLevel::Info, tr("Inspection issue removed."), 2500);
        }
    };
    const auto clearAllIssues = [this]() {
        if (viewer_ == nullptr || viewer_->inspectionIssues().isEmpty()) {
            return;
        }

        viewer_->clearInspectionIssues();
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Inspection issues cleared."), 2500);
    };
    const auto exportIssuesCsv = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Issues CSV"),
            QStringLiteral("inspection_issues.csv"),
            tr("CSV Files (*.csv)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        if (!InspectionReportExporter::exportIssuesCsv(filePath, viewer_->inspectionIssues(), &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }
        showUserMessage(LogLevel::Info, tr("Issue CSV exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    };
    const auto exportInspectionReport = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Inspection Report"),
            QStringLiteral("inspection_report.html"),
            tr("HTML Files (*.html)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        const QString projectName = currentProjectFilePath_.isEmpty()
            ? tr("Current Project")
            : QFileInfo(currentProjectFilePath_).completeBaseName();
        if (!InspectionReportExporter::exportProjectHtml(
                filePath,
                projectName,
                viewer_->currentFilePaths(),
                viewer_->towerMarkers(),
                viewer_->inspectionIssues(),
                &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        showUserMessage(LogLevel::Info, tr("Inspection report exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    };
    connect(startIssueMarkAction_, &QAction::triggered, this, beginIssueMarking);
    connect(cancelIssueToolAction_, &QAction::triggered, this, cancelIssueTool);
    connect(focusIssueAction_, &QAction::triggered, this, focusSelectedIssue);
    connect(removeIssueAction_, &QAction::triggered, this, removeSelectedIssue);
    connect(clearIssuesAction_, &QAction::triggered, this, clearAllIssues);
    connect(exportIssuesCsvAction_, &QAction::triggered, this, exportIssuesCsv);
    connect(exportInspectionReportAction_, &QAction::triggered, this, exportInspectionReport);
    connect(issueTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
        if (viewer_ != nullptr) {
            viewer_->setSelectedIssueIndex(currentRow);
        }
        updateActionState();
        updateIssuePanel();
    });
    const auto commitIssueDetails = [this]() {
        if (updatingIssueDetails_ || viewer_ == nullptr) {
            return;
        }

        const int selectedIssueIndex = viewer_->selectedIssueIndex();
        if (selectedIssueIndex < 0 || selectedIssueIndex >= viewer_->inspectionIssues().size()) {
            return;
        }

        InspectionIssue issue = viewer_->inspectionIssues().at(selectedIssueIndex);
        issue.title = issueTitleEdit_->text().trimmed();
        issue.category = issueCategoryComboBox_->currentText().trimmed();
        issue.severity = static_cast<IssueSeverity>(issueSeverityComboBox_->currentIndex());
        issue.status = static_cast<IssueStatus>(issueStatusComboBox_->currentIndex());
        issue.relatedTowerIndex = issueRelatedTowerComboBox_->currentData().toInt();
        issue.relatedTowerName = issueRelatedTowerComboBox_->currentText().trimmed();
        issue.imagePath = issueImagePathEdit_->text().trimmed();
        issue.description = issueDescriptionEdit_->toPlainText().trimmed();
        if (viewer_->updateInspectionIssue(selectedIssueIndex, issue)) {
            updateIssuePanel();
        }
    };
    connect(issueTitleEdit_, &QLineEdit::editingFinished, this, commitIssueDetails);
    connect(issueCategoryComboBox_, &QComboBox::editTextChanged, this, [commitIssueDetails](const QString&) { commitIssueDetails(); });
    connect(issueSeverityComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [commitIssueDetails](int) { commitIssueDetails(); });
    connect(issueStatusComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [commitIssueDetails](int) { commitIssueDetails(); });
    connect(issueRelatedTowerComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [commitIssueDetails](int) { commitIssueDetails(); });
    connect(issueImagePathEdit_, &QLineEdit::editingFinished, this, commitIssueDetails);
    connect(issueDescriptionEdit_, &QPlainTextEdit::textChanged, this, commitIssueDetails);
    connect(projectSearchEdit_, &QLineEdit::textChanged, this, [this](const QString&) {
        refreshProjectTreeFilter();
    });
    connect(projectTreeWidget_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        Q_UNUSED(column);
        applyProjectTreeItemCheckState(item);
    });
    connect(projectTreeWidget_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* currentItem, QTreeWidgetItem*) {
        if (viewer_ == nullptr || currentItem == nullptr) {
            updateActionState();
            return;
        }

        const QString itemType = currentItem->data(0, kProjectTreeItemTypeRole).toString();
        if (itemType == QStringLiteral("imageItem")) {
            const int issueIndex = currentItem->data(0, kProjectTreeIssueIndexRole).toInt();
            viewer_->setSelectedIssueIndex(issueIndex);
            updateIssuePanel();
        } else if (itemType == QStringLiteral("trajectoryItem")) {
            selectedRouteWaypointIndex_ = inspectionRoute_.waypoints.isEmpty() ? -1 : 0;
            viewer_->setSelectedInspectionRouteWaypointIndex(selectedRouteWaypointIndex_);
            updateRoutePlanningPanel();
        } else if (itemType == QStringLiteral("pointCloudItem")) {
            viewer_->setSelectedIssueIndex(-1);
            updateIssuePanel();
        } else {
            viewer_->setSelectedIssueIndex(-1);
            updateIssuePanel();
        }
        updateActionState();
    });
    connect(projectTreeWidget_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        focusProjectTreeItem(item);
    });
    connect(projectTreeWidget_, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        showProjectTreeContextMenu(pos);
    });
    connect(languageEnglishAction_, &QAction::triggered, this, [this]() { applyLanguage(UiLanguage::English); });
    connect(languageChineseAction_, &QAction::triggered, this, [this]() { applyLanguage(UiLanguage::Chinese); });

    connect(showLogAction_, &QAction::toggled, this, [this](bool visible) {
        if (logDock_ != nullptr) {
            if (visible) {
                logDock_->show();
                resizeDocks({logDock_}, {190}, Qt::Vertical);
            } else {
                logDock_->hide();
            }
            persistWindowSettings();
        }
    });
    connect(logDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (showLogAction_ != nullptr && showLogAction_->isChecked() != visible) {
            showLogAction_->setChecked(visible);
        }
        persistWindowSettings();
    });
    connect(profileDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (showProfileDockAction_ != nullptr && showProfileDockAction_->isChecked() != visible) {
            showProfileDockAction_->setChecked(visible);
        }
        persistWindowSettings();
    });
    connect(profileClassificationDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (showProfileClassificationDockAction_ != nullptr && showProfileClassificationDockAction_->isChecked() != visible) {
            const QSignalBlocker blocker(showProfileClassificationDockAction_);
            showProfileClassificationDockAction_->setChecked(visible);
        }
        persistWindowSettings();
    });
    const auto persistDockState = [this]() {
        persistWindowSettings();
    };
    connect(projectDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(inspectorDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(profileClassificationDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(profileDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(logDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(projectDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(inspectorDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(profileClassificationDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(profileDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(logDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    if (inspectorTabWidget_ != nullptr) {
        connect(inspectorTabWidget_, &QTabWidget::currentChanged, this, [this](int) {
            persistWindowSettings();
        });
    }

    connect(viewer_, &PointCloudViewer::pointCloudLoadingStarted, this, [this](const QString& message) {
        beginOperationProgress(message);
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoadingProgress, this, [this](const QString& message, int value, int maximum) {
        updateOperationProgress(message, value, maximum);
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoadingFinished, this, [this]() {
        endOperationProgress();
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoaded, this, [this]() {
        endOperationProgress();
        classificationEditsDirty_ = false;
        rebuildProjectTree();
        syncUiFromViewer();
    });
    connect(viewer_, &PointCloudViewer::pointCloudCleared, this, [this]() {
        endOperationProgress();
        classificationEditsDirty_ = false;
        setTowerEditingEnabled(false);
        inspectionRoute_ = InspectionRoute();
        selectedRouteWaypointIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        showUserMessage(LogLevel::Info, tr("Scene cleared."), 3000);
    });
    connect(viewer_, &PointCloudViewer::visualizationOptionsChanged, this, [this]() { syncUiFromViewer(); });
    connect(viewer_, &PointCloudViewer::visualizationOptionsChanged, this, [this]() { persistVisualizationSettings(); });
    connect(viewer_, &PointCloudViewer::profileClassificationModeChanged, this, [this](bool enabled) {
        const QSignalBlocker blocker(profileClassificationAction_);
        profileClassificationAction_->setChecked(enabled);
        if (enabled && profileClassificationDock_ != nullptr) {
            profileClassificationDock_->show();
            profileClassificationDock_->raise();
        }
        if (!enabled) {
            promptSaveProfileClassificationEditsIfNeeded();
        }
        updateProfileClassificationPanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::classificationEditsChanged, this, [this]() {
        classificationEditsDirty_ = viewer_ != nullptr && viewer_->classificationEditedPointCount() > 0;
        updateProfileClassificationPanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::profileClassificationStateChanged, this, [this]() {
        updateProfileClassificationPanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::interactionOptionsChanged, this, [this]() {
        persistInteractionSettings();
        syncUiFromViewer();
        updateNavigationHelpText();
        showUserMessage(LogLevel::Info, tr("Navigation preferences updated."), 2500);
    });
    connect(viewer_, &PointCloudViewer::measurementChanged, this, [this]() {
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        inspectionRoute_ = InspectionRoute();
        selectedRouteWaypointIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        updateMeasurementPanel();
    });
    connect(viewer_, &PointCloudViewer::measurementModeChanged, this, [this]() {
        if (!viewer_->measurementEnabled()) {
            vegetationRiskResults_.clear();
            selectedVegetationRiskIndex_ = -1;
            inspectionRoute_ = InspectionRoute();
            selectedRouteWaypointIndex_ = -1;
            viewer_->clearInspectionRouteWaypoints();
        }
        syncProfileDockForMeasurementMode(viewer_->measurementEnabled());
        syncUiFromViewer();
        updateMeasurementPanel();
    });
    connect(viewer_, &PointCloudViewer::towerMarkersChanged, this, [this]() {
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        inspectionRoute_ = InspectionRoute();
        selectedRouteWaypointIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        updateMeasurementPanel();
        updateTowerPanel();
        updateVegetationRiskPanel();
    });
    connect(viewer_, &PointCloudViewer::selectedTowerChanged, this, [this](int index) {
        if (towerTableWidget_ != nullptr && towerTableWidget_->currentRow() != index) {
            const QSignalBlocker blocker(towerTableWidget_);
            if (index >= 0) {
                towerTableWidget_->setCurrentCell(index, 1);
            } else {
                towerTableWidget_->clearSelection();
                towerTableWidget_->setCurrentItem(nullptr);
            }
        }
        updateActionState();
        updateMeasurementPanel();
        updateTowerPanel();
    });
    connect(viewer_, &PointCloudViewer::towerEditModeChanged, this, [this]() {
        updateTowerPanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::towerEditRequested, this, [this](const PointRecord& point, int modeValue, int targetIndex) {
        const TowerEditMode mode = static_cast<TowerEditMode>(modeValue);
        if (viewer_ == nullptr) {
            return;
        }

        if (mode == TowerEditMode::MoveSelected) {
            if (viewer_->moveTowerMarker(targetIndex, point)) {
                updateTowerPanel();
                showUserMessage(LogLevel::Info, tr("Tower marker moved."), 2500);
            }
            return;
        }

        const QString towerName = nextDefaultTowerName();

        const bool inserted = mode == TowerEditMode::InsertBeforeSelected
            ? viewer_->insertTowerMarker(targetIndex, towerName, point)
            : viewer_->addTowerMarker(towerName, point);
        if (!inserted) {
            showUserMessage(LogLevel::Warning, tr("Tower marker name cannot be empty."), 3000);
            return;
        }

        updateTowerPanel();
        showUserMessage(
            LogLevel::Info,
            mode == TowerEditMode::AddAfterLast
                ? tr("Tower marker added. Continue clicking points to add more, or cancel the tool when finished.")
                : tr("Tower marker added."),
            mode == TowerEditMode::AddAfterLast ? 3500 : 2500);
    });
    connect(viewer_, &PointCloudViewer::inspectionIssuesChanged, this, [this]() {
        rebuildProjectTree();
        syncUiFromViewer();
        updateMeasurementPanel();
        updateIssuePanel();
        updateVegetationRiskPanel();
    });
    connect(viewer_, &PointCloudViewer::selectedIssueChanged, this, [this](int index) {
        if (issueTableWidget_ != nullptr && issueTableWidget_->currentRow() != index) {
            const QSignalBlocker blocker(issueTableWidget_);
            if (index >= 0) {
                issueTableWidget_->setCurrentCell(index, 1);
            } else {
                issueTableWidget_->clearSelection();
                issueTableWidget_->setCurrentItem(nullptr);
            }
        }
        updateActionState();
        updateMeasurementPanel();
        updateIssuePanel();
    });
    connect(viewer_, &PointCloudViewer::issueEditModeChanged, this, [this]() {
        updateIssuePanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::issueEditRequested, this, [this](const PointRecord& point) {
        if (viewer_ == nullptr) {
            return;
        }

        InspectionIssue issue;
        issue.id = issueDefaultId();
        issue.title = nextDefaultIssueTitle();
        issue.category = tr("Other");
        issue.severity = IssueSeverity::Major;
        issue.status = IssueStatus::Open;
        issue.point = point;
        issue.relatedTowerIndex = viewer_->selectedTowerIndex();
        if (issue.relatedTowerIndex >= 0 && issue.relatedTowerIndex < viewer_->towerMarkers().size()) {
            issue.relatedTowerName = viewer_->towerMarkers().at(issue.relatedTowerIndex).name;
        }
        issue.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
        if (viewer_->addInspectionIssue(issue)) {
            if (inspectorTabWidget_ != nullptr) {
                inspectorTabWidget_->setCurrentIndex(2);
            }
            updateIssuePanel();
            showUserMessage(LogLevel::Info, tr("Inspection issue added. Continue clicking points to add more, or right-click to cancel."), 3500);
        }
    });
    connect(viewer_, &PointCloudViewer::measurementMessage, this, [this](const QString& message, bool error) {
        showUserMessage(error ? LogLevel::Error : LogLevel::Info, message, error ? 4000 : 3000);
    });
}

void MainWindow::openProject()
{
    const QString filePath = showStyledOpenFileNameDialog(
        this,
        tr("Open Project"),
        QString(),
        tr("LiDAR Power Projects (*.json *.lpproj);;JSON Files (*.json);;All Files (*.*)"));

    if (filePath.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Open project cancelled."), 2000);
        return;
    }

    loadProjectFile(filePath);
}

void MainWindow::saveProject()
{
    if (currentProjectFilePath_.isEmpty()) {
        saveProjectAs();
        return;
    }

    saveProjectFile(currentProjectFilePath_);
}

void MainWindow::saveProjectAs()
{
    const QString initialPath = currentProjectFilePath_.isEmpty()
        ? QStringLiteral("project.lpproj")
        : currentProjectFilePath_;
    const QString filePath = showStyledSaveFileNameDialog(
        this,
        tr("Save Project"),
        initialPath,
        tr("LiDAR Power Projects (*.lpproj);;JSON Files (*.json)"));

    if (filePath.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Save project cancelled."), 2000);
        return;
    }

    QString normalizedPath = filePath;
    if (QFileInfo(normalizedPath).suffix().isEmpty()) {
        normalizedPath += QStringLiteral(".lpproj");
    }

    saveProjectFile(normalizedPath);
}

void MainWindow::openProjectCoordinateSystems()
{
    ProjectCoordinateSystemsDialog dialog(this);
    dialog.setCoordinateSystems(projectCoordinateSystems_);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    projectCoordinateSystems_ = dialog.coordinateSystems();
    syncRoutePlanningFromProjectCoordinateSystems();
    updateRoutePlanningPanel();
    rebuildProjectTree();
    updateActionState();
    showUserMessage(LogLevel::Info, tr("Project coordinate systems updated."), 3000);
}

void MainWindow::syncProjectCoordinateSystemsFromRoutePlanning()
{
    if (projectCoordinateSystems_.pointCloudCrs.authName.trimmed().isEmpty()) {
        projectCoordinateSystems_.pointCloudCrs.authName = QStringLiteral("EPSG");
    }

    projectCoordinateSystems_.pointCloudCrs.code = routePlanningOptions_.crs.sourceEpsg;
    if (projectCoordinateSystems_.pointCloudCrs.code <= 0) {
        projectCoordinateSystems_.pointCloudCrs.displayName.clear();
        projectCoordinateSystems_.pointCloudCrs.wkt.clear();
    } else {
        CoordinateSystemRef normalized;
        if (CrsAuthorityService::normalizeCoordinateSystem(projectCoordinateSystems_.pointCloudCrs, &normalized, nullptr)) {
            projectCoordinateSystems_.pointCloudCrs = normalized;
        }
    }
    projectCoordinateSystems_.pointCloudCrs.kind = CoordinateSystemKind::Projected;

    if (projectCoordinateSystems_.geographicCrs.code <= 0) {
        projectCoordinateSystems_.geographicCrs = defaultGeographicCoordinateSystem();
    } else {
        CoordinateSystemRef normalized;
        if (CrsAuthorityService::normalizeCoordinateSystem(projectCoordinateSystems_.geographicCrs, &normalized, nullptr)) {
            projectCoordinateSystems_.geographicCrs = normalized;
        }
    }
    projectCoordinateSystems_.geographicCrs.kind = CoordinateSystemKind::Geographic;
}

void MainWindow::syncRoutePlanningFromProjectCoordinateSystems()
{
    if (projectCoordinateSystems_.pointCloudCrs.authName.trimmed().isEmpty()) {
        projectCoordinateSystems_.pointCloudCrs.authName = QStringLiteral("EPSG");
    }
    projectCoordinateSystems_.pointCloudCrs.kind = CoordinateSystemKind::Projected;

    if (projectCoordinateSystems_.geographicCrs.authName.trimmed().isEmpty()) {
        projectCoordinateSystems_.geographicCrs.authName = QStringLiteral("EPSG");
    }

    if (projectCoordinateSystems_.geographicCrs.code <= 0) {
        projectCoordinateSystems_.geographicCrs = defaultGeographicCoordinateSystem();
    }
    if (projectCoordinateSystems_.pointCloudCrs.code > 0) {
        CoordinateSystemRef normalized;
        if (CrsAuthorityService::normalizeCoordinateSystem(projectCoordinateSystems_.pointCloudCrs, &normalized, nullptr)) {
            projectCoordinateSystems_.pointCloudCrs = normalized;
        }
    }
    if (projectCoordinateSystems_.geographicCrs.code > 0) {
        CoordinateSystemRef normalized;
        if (CrsAuthorityService::normalizeCoordinateSystem(projectCoordinateSystems_.geographicCrs, &normalized, nullptr)) {
            projectCoordinateSystems_.geographicCrs = normalized;
        }
    }
    projectCoordinateSystems_.pointCloudCrs.kind = CoordinateSystemKind::Projected;
    projectCoordinateSystems_.geographicCrs.kind = CoordinateSystemKind::Geographic;

    routePlanningOptions_.crs.sourceEpsg = projectCoordinateSystems_.pointCloudCrs.code;
    routePlanningOptions_.crs.targetEpsg = projectCoordinateSystems_.geographicCrs.code;
}

bool MainWindow::loadProjectFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        showUserMessage(LogLevel::Error, tr("Failed to open project file."), 5000);
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) {
        showUserMessage(LogLevel::Error, tr("Project file format is invalid."), 5000);
        return false;
    }

    const QJsonObject projectObject = document.object();
    QStringList pointCloudFilePaths;
    const QJsonArray pointCloudFilesArray = projectObject.value(QStringLiteral("pointCloudFilePaths")).toArray();
    for (const QJsonValue& pathValue : pointCloudFilesArray) {
        const QString resolvedPath = resolveProjectPath(filePath, pathValue.toString());
        if (!resolvedPath.isEmpty()) {
            pointCloudFilePaths.append(resolvedPath);
        }
    }
    if (pointCloudFilePaths.isEmpty()) {
        const QString legacyPointCloudPath = resolveProjectPath(
            filePath,
            projectObject.value(QStringLiteral("pointCloudFilePath")).toString());
        if (!legacyPointCloudPath.isEmpty()) {
            pointCloudFilePaths.append(legacyPointCloudPath);
        }
    }
    if (pointCloudFilePaths.isEmpty()) {
        showUserMessage(LogLevel::Error, tr("Project file does not contain any point cloud paths."), 5000);
        return false;
    }

    QString errorMessage;
    if (!viewer_->loadPointCloudFiles(pointCloudFilePaths, &errorMessage)) {
        syncUiFromViewer();
        showUserMessage(LogLevel::Error, errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage, 6000);
        return false;
    }

    const QJsonObject visualizationObject = projectObject.value(QStringLiteral("visualization")).toObject();
    const PointCloudVisualizationOptions defaults = viewer_->visualizationOptions();
    viewer_->setPointSize(visualizationObject.value(QStringLiteral("pointSize")).toInt(static_cast<int>(defaults.pointSize)));
    viewer_->setPointOpacity(visualizationObject.value(QStringLiteral("pointOpacity")).toInt(static_cast<int>(defaults.pointOpacity * 100.0f)));
    viewer_->setDepthCueStrength(visualizationObject.value(QStringLiteral("depthCueStrength")).toInt(static_cast<int>(defaults.depthCueStrength * 100.0f)));
    viewer_->setEdlStrength(visualizationObject.value(QStringLiteral("edlStrength")).toInt(static_cast<int>(defaults.edlStrength * 100.0f)));
    viewer_->setClassificationColorMap(classificationColorMapFromJson(
        visualizationObject.value(QStringLiteral("classificationColors")).toObject(),
        defaults.classificationColors));
    viewer_->setClassificationVisibilityMap(classificationVisibilityMapFromJson(
        visualizationObject.value(QStringLiteral("classificationVisibility")).toObject(),
        defaults.classificationVisibility));
    viewer_->setClassificationFallbackColor(colorFromJson(
        visualizationObject.value(QStringLiteral("classificationFallbackColor")).toObject(),
        defaults.classificationFallbackColor));
    classificationNameOverrides_ = classificationNameMapFromJson(
        visualizationObject.value(QStringLiteral("classificationNameOverrides")).toObject());
    viewer_->setColorMode(visualizationObject.value(QStringLiteral("colorMode")).toInt(static_cast<int>(defaults.colorMode)));
    viewer_->setSingleColor(colorFromJson(visualizationObject.value(QStringLiteral("singleColor")).toObject(), defaults.singleColor));
    viewer_->setBackgroundColor(colorFromJson(visualizationObject.value(QStringLiteral("backgroundColor")).toObject(), defaults.backgroundColor));
    viewer_->setUseRoundSplats(visualizationObject.value(QStringLiteral("useRoundSplats")).toBool(defaults.useRoundSplats));
    viewer_->setShowAxes(visualizationObject.value(QStringLiteral("showAxes")).toBool(defaults.showAxes));
    viewer_->setShowBoundingBox(visualizationObject.value(QStringLiteral("showBoundingBox")).toBool(defaults.showBoundingBox));

    InteractionOptions interactionOptions = viewer_->interactionOptions();
    const QJsonObject interactionObject = projectObject.value(QStringLiteral("interaction")).toObject();
    interactionOptions.invertOrbitDrag = interactionObject.value(QStringLiteral("invertOrbitDrag")).toBool(interactionOptions.invertOrbitDrag);
    interactionOptions.invertPanDrag = interactionObject.value(QStringLiteral("invertPanDrag")).toBool(interactionOptions.invertPanDrag);
    interactionOptions.invertWheelZoom = interactionObject.value(QStringLiteral("invertWheelZoom")).toBool(interactionOptions.invertWheelZoom);
    viewer_->setInteractionOptions(interactionOptions);

    const QJsonObject measurementObject = projectObject.value(QStringLiteral("measurement")).toObject();
    clearanceWarningThresholdMeters_ = measurementObject.value(QStringLiteral("clearanceThresholdMeters")).toDouble(clearanceWarningThresholdMeters_);
    if (clearanceThresholdSpinBox_ != nullptr) {
        const QSignalBlocker blocker(clearanceThresholdSpinBox_);
        clearanceThresholdSpinBox_->setValue(clearanceWarningThresholdMeters_);
    }
    const QJsonObject analysisObject = projectObject.value(QStringLiteral("analysis")).toObject();
    clearanceRulePreset_ = static_cast<ClearanceRulePreset>(analysisObject.value(QStringLiteral("clearanceRulePreset")).toInt(static_cast<int>(clearanceRulePreset_)));
    vegetationSearchRadiusMeters_ = analysisObject.value(QStringLiteral("vegetationSearchRadiusMeters")).toDouble(vegetationSearchRadiusMeters_);
    vegetationClusterGapMeters_ = analysisObject.value(QStringLiteral("vegetationClusterGapMeters")).toDouble(vegetationClusterGapMeters_);
    vegetationClusterPointCount_ = analysisObject.value(QStringLiteral("vegetationClusterPointCount")).toInt(vegetationClusterPointCount_);
    preferVegetationClassification_ = analysisObject.value(QStringLiteral("preferVegetationClassification")).toBool(preferVegetationClassification_);
    if (clearanceRulePresetComboBox_ != nullptr) {
        const QSignalBlocker blocker(clearanceRulePresetComboBox_);
        const int presetIndex = clearanceRulePresetComboBox_->findData(static_cast<int>(clearanceRulePreset_));
        clearanceRulePresetComboBox_->setCurrentIndex(presetIndex >= 0 ? presetIndex : 0);
    }
    if (vegetationSearchRadiusSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationSearchRadiusSpinBox_);
        vegetationSearchRadiusSpinBox_->setValue(vegetationSearchRadiusMeters_);
    }
    if (vegetationClusterGapSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationClusterGapSpinBox_);
        vegetationClusterGapSpinBox_->setValue(vegetationClusterGapMeters_);
    }
    if (vegetationClusterPointCountSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationClusterPointCountSpinBox_);
        vegetationClusterPointCountSpinBox_->setValue(vegetationClusterPointCount_);
    }
    if (preferVegetationClassificationCheckBox_ != nullptr) {
        const QSignalBlocker blocker(preferVegetationClassificationCheckBox_);
        preferVegetationClassificationCheckBox_->setChecked(preferVegetationClassification_);
    }

    routePlanningOptions_ = routePlanningOptionsFromJson(projectObject.value(QStringLiteral("routePlanning")).toObject());
    syncProjectCoordinateSystemsFromRoutePlanning();

    const QJsonObject projectPropertiesObject = projectObject.value(QStringLiteral("projectProperties")).toObject();
    const QJsonObject coordinateSystemsObject = projectPropertiesObject.value(QStringLiteral("coordinateSystems")).toObject();
    if (!coordinateSystemsObject.isEmpty()) {
        projectCoordinateSystems_.pointCloudCrs = coordinateSystemRefFromJson(
            coordinateSystemsObject.value(QStringLiteral("pointCloudCrs")).toObject(),
            projectCoordinateSystems_.pointCloudCrs);
        projectCoordinateSystems_.geographicCrs = coordinateSystemRefFromJson(
            coordinateSystemsObject.value(QStringLiteral("geographicCrs")).toObject(),
            projectCoordinateSystems_.geographicCrs);
        if (projectCoordinateSystems_.geographicCrs.code <= 0) {
            projectCoordinateSystems_.geographicCrs = defaultGeographicCoordinateSystem();
        }
    }
    syncRoutePlanningFromProjectCoordinateSystems();

    const QJsonObject classificationEditsObject =
        projectObject.value(QStringLiteral("classificationEdits")).toObject();
    viewer_->setClassificationEditStore(classificationEditsFromJson(
        classificationEditsObject,
        [&filePath](const QString& storedDatasetPath) {
            return resolveProjectPath(filePath, storedDatasetPath);
        }));

    QList<TowerMarker> towerMarkers;
    const QJsonArray towersArray = projectObject.value(QStringLiteral("towerMarkers")).toArray();
    for (const QJsonValue& towerValue : towersArray) {
        const TowerRecord towerRecord = towerRecordFromJson(towerValue.toObject());
        if (towerRecord.name.isEmpty()) {
            continue;
        }
        towerMarkers.append(towerRecord);
    }
    viewer_->setTowerMarkers(towerMarkers);

    QList<InspectionIssue> inspectionIssues;
    const QJsonArray issuesArray = projectObject.value(QStringLiteral("inspectionIssues")).toArray();
    for (const QJsonValue& issueValue : issuesArray) {
        const InspectionIssue issue = inspectionIssueFromJson(issueValue.toObject());
        if (issue.title.trimmed().isEmpty()) {
            continue;
        }
        inspectionIssues.append(issue);
    }
    viewer_->setInspectionIssues(inspectionIssues);

    vegetationRiskResults_.clear();
    const QJsonArray vegetationRisksArray = analysisObject.value(QStringLiteral("vegetationRisks")).toArray();
    for (const QJsonValue& riskValue : vegetationRisksArray) {
        const VegetationRiskRecord riskRecord = vegetationRiskRecordFromJson(riskValue.toObject());
        if (riskRecord.title.trimmed().isEmpty()) {
            continue;
        }
        vegetationRiskResults_.append(riskRecord);
    }
    selectedVegetationRiskIndex_ = vegetationRiskResults_.isEmpty() ? -1 : 0;

    inspectionRoute_ = InspectionRoute();
    const QJsonArray routesArray = projectObject.value(QStringLiteral("routes")).toArray();
    if (!routesArray.isEmpty()) {
        const InspectionRoute route = inspectionRouteFromJson(routesArray.first().toObject());
        if (!route.waypoints.isEmpty()) {
            inspectionRoute_ = route;
        }
    }
    selectedRouteWaypointIndex_ = inspectionRoute_.waypoints.isEmpty() ? -1 : 0;
    if (inspectionRoute_.waypoints.isEmpty()) {
        viewer_->clearInspectionRouteWaypoints();
    } else {
        QList<PointRecord> routePoints;
        QStringList routeLabels;
        routePoints.reserve(inspectionRoute_.waypoints.size());
        routeLabels.reserve(inspectionRoute_.waypoints.size());
        for (int waypointIndex = 0; waypointIndex < inspectionRoute_.waypoints.size(); ++waypointIndex) {
            const InspectionWaypoint& waypoint = inspectionRoute_.waypoints.at(waypointIndex);
            routePoints.append(waypoint.localPoint);
            routeLabels.append(waypoint.id.isEmpty() ? QString::number(waypointIndex + 1) : waypoint.id);
        }
        viewer_->setInspectionRouteWaypoints(routePoints, routeLabels);
        viewer_->setSelectedInspectionRouteWaypointIndex(selectedRouteWaypointIndex_);
    }

    currentProjectFilePath_ = filePath;
    classificationEditsDirty_ = false;
    setTowerEditingEnabled(false);
    const QString languageCode = projectObject.value(QStringLiteral("language")).toString();
    if (languageCode == QStringLiteral("zh_CN")) {
        applyLanguage(UiLanguage::Chinese);
    } else if (languageCode == QStringLiteral("en")) {
        applyLanguage(UiLanguage::English);
    }
    syncUiFromViewer();
    updateTowerPanel();
    updateRoutePlanningPanel();
    showUserMessage(LogLevel::Info, tr("Project loaded: %1").arg(QFileInfo(filePath).fileName()), 4000);
    return true;
}

bool MainWindow::saveProjectFile(const QString& filePath)
{
    if (viewer_ == nullptr || viewer_->currentFilePaths().isEmpty()) {
        showUserMessage(LogLevel::Warning, tr("Load a point cloud before saving a project."), 4000);
        return false;
    }

    syncRoutePlanningFromProjectCoordinateSystems();
    if (aircraftProfileComboBox_ != nullptr) {
        const QVariant profileValue = aircraftProfileComboBox_->currentData();
        const int profileIndex = profileValue.isValid() ? profileValue.toInt() : static_cast<int>(routePlanningOptions_.aircraftProfile);
        routePlanningOptions_.aircraftProfile = static_cast<DjiAircraftProfile>(profileIndex);
    }
    if (routeSafetyHeightSpinBox_ != nullptr) {
        routePlanningOptions_.safety.safetyHeightMeters = static_cast<float>(routeSafetyHeightSpinBox_->value());
    }
    if (routeWaypointSpeedSpinBox_ != nullptr) {
        routePlanningOptions_.safety.defaultWaypointSpeedMps = static_cast<float>(routeWaypointSpeedSpinBox_->value());
    }
    if (routeWaypointSpacingSpinBox_ != nullptr) {
        routePlanningOptions_.generation.waypointSpacingMeters = static_cast<float>(routeWaypointSpacingSpinBox_->value());
    }
    if (routeSmoothingStrengthSpinBox_ != nullptr) {
        routePlanningOptions_.generation.smoothingStrengthPercent = static_cast<float>(routeSmoothingStrengthSpinBox_->value());
    }
    if (routeHeightOffsetSpinBox_ != nullptr) {
        routePlanningOptions_.safety.heightOffsetMeters = static_cast<float>(routeHeightOffsetSpinBox_->value());
    }

    QJsonObject visualizationObject {
        { QStringLiteral("pointSize"), static_cast<int>(std::lround(viewer_->visualizationOptions().pointSize)) },
        { QStringLiteral("pointOpacity"), static_cast<int>(std::lround(viewer_->visualizationOptions().pointOpacity * 100.0f)) },
        { QStringLiteral("depthCueStrength"), static_cast<int>(std::lround(viewer_->visualizationOptions().depthCueStrength * 100.0f)) },
        { QStringLiteral("edlStrength"), static_cast<int>(std::lround(viewer_->visualizationOptions().edlStrength * 100.0f)) },
        { QStringLiteral("colorMode"), static_cast<int>(viewer_->visualizationOptions().colorMode) },
        { QStringLiteral("singleColor"), colorToJson(viewer_->visualizationOptions().singleColor) },
        { QStringLiteral("classificationColors"), classificationColorMapToJson(viewer_->visualizationOptions().classificationColors) },
        { QStringLiteral("classificationVisibility"), classificationVisibilityMapToJson(viewer_->visualizationOptions().classificationVisibility) },
        { QStringLiteral("classificationNameOverrides"), classificationNameMapToJson(classificationNameOverrides_) },
        { QStringLiteral("classificationFallbackColor"), colorToJson(viewer_->visualizationOptions().classificationFallbackColor) },
        { QStringLiteral("backgroundColor"), colorToJson(viewer_->visualizationOptions().backgroundColor) },
        { QStringLiteral("useRoundSplats"), viewer_->visualizationOptions().useRoundSplats },
        { QStringLiteral("showAxes"), viewer_->visualizationOptions().showAxes },
        { QStringLiteral("showBoundingBox"), viewer_->visualizationOptions().showBoundingBox }
    };

    QJsonObject interactionObject {
        { QStringLiteral("invertOrbitDrag"), viewer_->interactionOptions().invertOrbitDrag },
        { QStringLiteral("invertPanDrag"), viewer_->interactionOptions().invertPanDrag },
        { QStringLiteral("invertWheelZoom"), viewer_->interactionOptions().invertWheelZoom }
    };

    QJsonObject measurementObject {
        { QStringLiteral("clearanceThresholdMeters"), clearanceWarningThresholdMeters_ }
    };
    QJsonObject analysisObject {
        { QStringLiteral("clearanceRulePreset"), static_cast<int>(clearanceRulePreset_) },
        { QStringLiteral("vegetationSearchRadiusMeters"), vegetationSearchRadiusMeters_ },
        { QStringLiteral("vegetationClusterGapMeters"), vegetationClusterGapMeters_ },
        { QStringLiteral("vegetationClusterPointCount"), vegetationClusterPointCount_ },
        { QStringLiteral("preferVegetationClassification"), preferVegetationClassification_ }
    };
    QJsonObject routePlanningObject = routePlanningOptionsToJson(routePlanningOptions_);
    QJsonObject classificationEditsObject = classificationEditsToJson(
        viewer_->classificationEditStore(),
        [&filePath](const QString& datasetPath) {
            return projectRelativePathFor(filePath, datasetPath);
        });
    QJsonObject projectPropertiesObject {
        { QStringLiteral("coordinateSystems"), projectCoordinateSystemsToJson(projectCoordinateSystems_) }
    };

    QJsonArray towersArray;
    for (const TowerMarker& towerMarker : viewer_->towerMarkers()) {
        towersArray.append(towerRecordToJson(towerMarker));
    }

    QJsonArray inspectionIssuesArray;
    for (const InspectionIssue& issue : viewer_->inspectionIssues()) {
        inspectionIssuesArray.append(inspectionIssueToJson(issue));
    }
    QJsonArray vegetationRisksArray;
    for (const VegetationRiskRecord& riskRecord : vegetationRiskResults_) {
        vegetationRisksArray.append(vegetationRiskRecordToJson(riskRecord));
    }
    analysisObject.insert(QStringLiteral("vegetationRisks"), vegetationRisksArray);

    QJsonArray routesArray;
    if (!inspectionRoute_.waypoints.isEmpty()) {
        routesArray.append(inspectionRouteToJson(inspectionRoute_));
    }

    QJsonArray pointCloudFilesArray;
    for (const QString& pointCloudFilePath : viewer_->currentFilePaths()) {
        pointCloudFilesArray.append(projectRelativePathFor(filePath, pointCloudFilePath));
    }

    QJsonObject projectObject {
        { QStringLiteral("version"), 7 },
        { QStringLiteral("pointCloudFilePaths"), pointCloudFilesArray },
        { QStringLiteral("pointCloudFilePath"), viewer_->currentFilePath().isEmpty() ? QString() : projectRelativePathFor(filePath, viewer_->currentFilePath()) },
        { QStringLiteral("language"), languageCodeFor(currentLanguage_) },
        { QStringLiteral("projectProperties"), projectPropertiesObject },
        { QStringLiteral("visualization"), visualizationObject },
        { QStringLiteral("interaction"), interactionObject },
        { QStringLiteral("measurement"), measurementObject },
        { QStringLiteral("analysis"), analysisObject },
        { QStringLiteral("routePlanning"), routePlanningObject },
        { QStringLiteral("classificationEdits"), classificationEditsObject },
        { QStringLiteral("routes"), routesArray },
        { QStringLiteral("towerMarkers"), towersArray },
        { QStringLiteral("inspectionIssues"), inspectionIssuesArray }
    };

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        showUserMessage(LogLevel::Error, tr("Failed to save project file."), 5000);
        return false;
    }

    file.write(QJsonDocument(projectObject).toJson(QJsonDocument::Indented));
    file.close();
    currentProjectFilePath_ = filePath;
    classificationEditsDirty_ = false;
    rebuildProjectTree();
    showUserMessage(LogLevel::Info, tr("Project saved: %1").arg(QFileInfo(filePath).fileName()), 3000);
    return true;
}

void MainWindow::openPointCloud()
{
    const QStringList filePaths = showStyledOpenFileNamesDialog(
        this,
        tr("Open LAS Point Clouds"),
        QString(),
        tr("LAS Files (*.las *.laz);;All Files (*.*)"));

    if (filePaths.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Open cancelled."), 2000);
        return;
    }

    loadPointCloudFiles(filePaths);
}

void MainWindow::addPointCloudFiles()
{
    const QStringList filePaths = showStyledOpenFileNamesDialog(
        this,
        tr("Add LAS Point Clouds"),
        QString(),
        tr("LAS Files (*.las *.laz);;All Files (*.*)"));

    if (filePaths.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Add datasets cancelled."), 2000);
        return;
    }

    appendPointCloudFiles(filePaths);
}

bool MainWindow::loadPointCloudFile(const QString& filePath)
{
    return loadPointCloudFiles(QStringList { filePath });
}

bool MainWindow::loadPointCloudFiles(const QStringList& filePaths)
{
    if (viewer_ == nullptr) {
        return false;
    }

    QString errorMessage;
    if (viewer_->loadPointCloudFiles(filePaths, &errorMessage)) {
        currentProjectFilePath_.clear();
        setTowerEditingEnabled(false);
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        const QString successMessage = filePaths.size() == 1
            ? tr("Loaded %1. %2").arg(QFileInfo(filePaths.constFirst()).fileName(), errorMessage)
            : tr("Loaded %1 datasets. %2")
                  .arg(QLocale().toString(filePaths.size()))
                  .arg(errorMessage);
        showUserMessage(LogLevel::Info, successMessage, 4500);
        return true;
    }

    syncUiFromViewer();
    showUserMessage(
        LogLevel::Error,
        errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
        6000);
    return false;
}

bool MainWindow::appendPointCloudFiles(const QStringList& filePaths)
{
    if (viewer_ == nullptr) {
        return false;
    }
    QString errorMessage;
    if (!viewer_->appendPointCloudFiles(filePaths, &errorMessage)) {
        syncUiFromViewer();
        showUserMessage(
            LogLevel::Error,
            errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
            6000);
        return false;
    }

    currentProjectFilePath_.clear();
    setTowerEditingEnabled(false);
    vegetationRiskResults_.clear();
    selectedVegetationRiskIndex_ = -1;
    syncUiFromViewer();
    showUserMessage(
        LogLevel::Info,
        errorMessage.isEmpty() ? tr("Datasets added.") : errorMessage,
        4500);
    return true;
}

void MainWindow::clearPointCloud()
{
    currentProjectFilePath_.clear();
    classificationEditsDirty_ = false;
    setTowerEditingEnabled(false);
    vegetationRiskResults_.clear();
    selectedVegetationRiskIndex_ = -1;
    inspectionRoute_ = InspectionRoute();
    selectedRouteWaypointIndex_ = -1;
    viewer_->clearPointCloud();
}

void MainWindow::removeSelectedDataset()
{
    if (viewer_ == nullptr || projectTreeWidget_ == nullptr) {
        return;
    }

    const QString datasetPath = selectedDatasetPath();
    if (datasetPath.isEmpty()) {
        showUserMessage(LogLevel::Warning, tr("Select a dataset in the project tree before removing it."), 3000);
        return;
    }
    QStringList remainingFilePaths = viewer_->currentFilePaths();
    remainingFilePaths.removeAll(datasetPath);

    if (remainingFilePaths.isEmpty()) {
        clearPointCloud();
        showUserMessage(LogLevel::Info, tr("Dataset removed. The project is now empty."), 3000);
        return;
    }

    const QList<TowerMarker> towerMarkers = viewer_->towerMarkers();
    const QList<InspectionIssue> inspectionIssues = viewer_->inspectionIssues();
    const int selectedTowerIndex = viewer_->selectedTowerIndex();
    const int selectedIssueIndex = viewer_->selectedIssueIndex();
    QHash<QString, bool> datasetVisibility;
    for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
        datasetVisibility.insert(datasetInfo.filePath.toLower(), datasetInfo.visible);
    }
    QString errorMessage;
    if (viewer_->loadPointCloudFiles(remainingFilePaths, &errorMessage)) {
        currentProjectFilePath_.clear();
        setTowerEditingEnabled(false);
        for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
            const auto visibilityIt = datasetVisibility.constFind(datasetInfo.filePath.toLower());
            if (visibilityIt != datasetVisibility.constEnd() && !visibilityIt.value()) {
                viewer_->setPointCloudDatasetVisible(datasetInfo.filePath, false);
            }
        }
        viewer_->setTowerMarkers(towerMarkers);
        viewer_->setInspectionIssues(inspectionIssues);
        viewer_->setSelectedTowerIndex(selectedTowerIndex);
        viewer_->setSelectedIssueIndex(selectedIssueIndex);
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        inspectionRoute_ = InspectionRoute();
        selectedRouteWaypointIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        showUserMessage(LogLevel::Info, tr("Dataset removed from the project."), 3000);
    } else {
        syncUiFromViewer();
        showUserMessage(
            LogLevel::Error,
            errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
            6000);
    }
}

void MainWindow::choosePointColor()
{
    const QColor initialColor = viewer_->visualizationOptions().singleColor;
    const QColor chosenColor = showStyledColorDialog(this, initialColor, tr("Choose Single Point Color"));
    if (chosenColor.isValid()) {
        viewer_->setSingleColor(chosenColor);
        if (viewer_->visualizationOptions().colorMode != PointCloudColorMode::SingleColor) {
            viewer_->setColorMode(PointCloudColorMode::SingleColor);
        }
    }
}

void MainWindow::chooseBackgroundColor()
{
    const QColor initialColor = viewer_->visualizationOptions().backgroundColor;
    const QColor chosenColor = showStyledColorDialog(this, initialColor, tr("Choose Background Color"));
    if (chosenColor.isValid()) {
        viewer_->setBackgroundColor(chosenColor);
    }
}

void MainWindow::applyOfficeTheme(Qtitan::RibbonStyle::Theme theme)
{
    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        ribbonStyle->setTheme(theme);
        updateWindowChromePalette(theme);
        persistThemeSettings();
        syncUiFromViewer();
        showUserMessage(LogLevel::Info, tr("Theme updated."), 2500);
    }
}

void MainWindow::updateWindowChromePalette(Qtitan::RibbonStyle::Theme theme)
{
    const bool useDarkChrome = theme == Qtitan::RibbonStyle::Office2016DarkGray;
    const QColor frameColor = useDarkChrome ? kWindowChromeDark : kWindowChromeLight;
    const QColor textColor = useDarkChrome ? QColor(241, 245, 249) : QColor(31, 41, 55);
    const QString dockTitleBackground = useDarkChrome ? QStringLiteral("#1f2937") : QStringLiteral("#e2e8f0");
    const QString dockTitleText = useDarkChrome ? QStringLiteral("#f1f5f9") : QStringLiteral("#0f172a");
    const QString dockBorder = useDarkChrome ? QStringLiteral("#475569") : QStringLiteral("#cbd5e1");
    const QString dockTabBackground = QStringLiteral("#e2e8f0");
    const QString dockTabText = QStringLiteral("#334155");
    const QString dockTabSelectedBackground = QStringLiteral("#ffffff");
    const QString dockTabSelectedText = QStringLiteral("#0f172a");
    const QString dockTabHoverBackground = QStringLiteral("#dbeafe");

    QPalette windowPalette = palette();
    windowPalette.setColor(QPalette::Window, frameColor);
    windowPalette.setColor(QPalette::WindowText, textColor);
    setPalette(windowPalette);
    setBackgroundRole(QPalette::Window);

    if (ribbonBar_ != nullptr) {
        QPalette ribbonPalette = ribbonBar_->palette();
        ribbonPalette.setColor(QPalette::Window, frameColor);
        ribbonPalette.setColor(QPalette::WindowText, textColor);
        ribbonBar_->setPalette(ribbonPalette);
        ribbonBar_->setBackgroundRole(QPalette::Window);
        ribbonBar_->setAutoFillBackground(true);

        const QList<Qtitan::RibbonPage*> pages = ribbonBar_->pages();
        for (Qtitan::RibbonPage* page : pages) {
            if (page == nullptr) {
                continue;
            }

            QPalette pagePalette = page->palette();
            pagePalette.setColor(QPalette::Window, frameColor);
            pagePalette.setColor(QPalette::WindowText, textColor);
            page->setPalette(pagePalette);
            page->setBackgroundRole(QPalette::Window);
            page->setAutoFillBackground(true);
            page->update();
        }

        const QString normalText = useDarkChrome ? QStringLiteral("#e2e8f0") : QStringLiteral("#0f172a");
        const QString hoverBg = useDarkChrome ? QStringLiteral("rgba(148, 163, 184, 0.22)") : QStringLiteral("rgba(37, 99, 235, 0.16)");
        const QString hoverText = useDarkChrome ? QStringLiteral("#f8fafc") : QStringLiteral("#0b1220");
        const QString checkedBg = useDarkChrome ? QStringLiteral("#2563eb") : QStringLiteral("#1d4ed8");
        const QString disabledText = useDarkChrome ? QStringLiteral("#94a3b8") : QStringLiteral("#475569");
        ribbonBar_->setStyleSheet(QStringLiteral(
            "QAbstractButton {"
            "background-color: transparent;"
            "border: none;"
            "color: %1;"
            "}"
            "QAbstractButton:hover {"
            "background-color: %2;"
            "color: %3;"
            "}"
            "QAbstractButton:checked, QAbstractButton:pressed {"
            "background-color: %4;"
            "color: #ffffff;"
            "}"
            "QAbstractButton:disabled {"
            "background-color: transparent;"
            "color: %5;"
            "}")
                .arg(normalText, hoverBg, hoverText, checkedBg, disabledText));

        ribbonBar_->update();
    }

    setStyleSheet(QStringLiteral(
        "QDockWidget {"
        "border: 1px solid %1;"
        "}"
        "QMainWindow::tab-bar {"
        "alignment: left;"
        "}"
        "QMainWindow QTabBar::tab {"
        "background-color: %4;"
        "color: %5;"
        "border: 1px solid %1;"
        "border-bottom: none;"
        "border-top-left-radius: 6px;"
        "border-top-right-radius: 6px;"
        "padding: 6px 12px;"
        "margin-right: 2px;"
        "font-weight: 600;"
        "}"
        "QMainWindow QTabBar::tab:selected {"
        "background-color: %6;"
        "color: %7;"
        "}"
        "QMainWindow QTabBar::tab:hover:!selected {"
        "background-color: %8;"
        "color: %7;"
        "}"
        "QDockWidget::title {"
        "background-color: %2;"
        "color: %3;"
        "text-align: left;"
        "padding: 6px 10px;"
        "border-bottom: 1px solid %1;"
        "font-weight: 600;"
        "}")
            .arg(dockBorder, dockTitleBackground, dockTitleText,
                dockTabBackground, dockTabText, dockTabSelectedBackground,
                dockTabSelectedText, dockTabHoverBackground));

    updateWindowControlAppearance(theme);
    updateWindowControlButtons();
    update();
}

void MainWindow::updateWindowControlButtons()
{
    if (minimizeButton_ == nullptr || maximizeButton_ == nullptr || closeButton_ == nullptr) {
        return;
    }

    Qtitan::RibbonStyle::Theme theme = Qtitan::RibbonStyle::Office2016White;
    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        theme = ribbonStyle->getTheme();
    }

    const bool useDarkChrome = theme == Qtitan::RibbonStyle::Office2016DarkGray;
    const QColor iconColor = useDarkChrome ? QColor(241, 245, 249) : QColor(31, 41, 55);

    minimizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Minimize, iconColor));
    closeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Close, iconColor));

    if (isMaximized()) {
        maximizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Restore, iconColor));
        maximizeButton_->setToolTip(tr("Restore Down"));
    } else {
        maximizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Maximize, iconColor));
        maximizeButton_->setToolTip(tr("Maximize"));
    }

    minimizeButton_->setToolTip(tr("Minimize"));
    closeButton_->setToolTip(tr("Close"));
}

void MainWindow::updateWindowControlAppearance(Qtitan::RibbonStyle::Theme theme)
{
    if (windowControlsWidget_ == nullptr) {
        return;
    }

    const bool useDarkChrome = theme == Qtitan::RibbonStyle::Office2016DarkGray;
    const QString hoverColor = useDarkChrome ? QStringLiteral("rgba(255, 255, 255, 0.12)") : QStringLiteral("rgba(15, 23, 42, 0.08)");
    const QString pressedColor = useDarkChrome ? QStringLiteral("rgba(255, 255, 255, 0.18)") : QStringLiteral("rgba(15, 23, 42, 0.14)");
    windowControlsWidget_->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "border: none;"
        "background: transparent;"
        "padding: 0;"
        "}"
        "QToolButton:hover {"
        "background: %1;"
        "}"
        "QToolButton:pressed {"
        "background: %2;"
        "}"
        "QToolButton#windowCloseButton:hover {"
        "background: #e11d48;"
        "}"
        "QToolButton#windowCloseButton:pressed {"
        "background: #be123c;"
        "}").arg(hoverColor, pressedColor));
}

void MainWindow::toggleMaximizedWindow()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
    updateWindowControlButtons();
}

bool MainWindow::isDraggableRibbonArea(const QPoint& position) const
{
    if (ribbonBar_ == nullptr || !ribbonBar_->rect().contains(position)) {
        return false;
    }

    QWidget* child = ribbonBar_->childAt(position);
    return !isInteractiveRibbonWidget(child);
}

bool MainWindow::isInteractiveRibbonWidget(const QWidget* widget) const
{
    const QWidget* current = widget;
    while (current != nullptr) {
        if (current == ribbonBar_) {
            return false;
        }
        if (isWindowControlWidget(current)) {
            return true;
        }
        const QString className = QString::fromLatin1(current->metaObject()->className());
        if (qobject_cast<const QAbstractButton*>(current) != nullptr
            || qobject_cast<const QComboBox*>(current) != nullptr
            || qobject_cast<const QAbstractSpinBox*>(current) != nullptr
            || qobject_cast<const QTabBar*>(current) != nullptr
            || className.contains(QStringLiteral("RibbonTab"), Qt::CaseInsensitive)
            || className.contains(QStringLiteral("SystemButton"), Qt::CaseInsensitive)) {
            return true;
        }
        current = current->parentWidget();
    }

    return false;
}

bool MainWindow::isWindowControlWidget(const QWidget* widget) const
{
    const QWidget* current = widget;
    while (current != nullptr) {
        if (current == windowControlsWidget_) {
            return true;
        }
        current = current->parentWidget();
    }
    return false;
}

void MainWindow::syncUiFromViewer()
{
    const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();
    const InteractionOptions& interactionOptions = viewer_->interactionOptions();

    {
        const QSignalBlocker pointSizeBlocker(pointSizeSlider_);
        const QSignalBlocker pointOpacityBlocker(pointOpacitySlider_);
        const QSignalBlocker depthCueBlocker(depthCueSlider_);
        const QSignalBlocker edlStrengthBlocker(edlStrengthSlider_);
        const QSignalBlocker colorModeBlocker(colorModeComboBox_);
        const QSignalBlocker roundSplatsBlocker(roundSplatsCheckBox_);
        const QSignalBlocker axesBlocker(axesCheckBox_);
        const QSignalBlocker boundsBlocker(boundingBoxCheckBox_);
        const QSignalBlocker orbitBlocker(invertOrbitCheckBox_);
        const QSignalBlocker panBlocker(invertPanCheckBox_);
        const QSignalBlocker wheelBlocker(invertWheelCheckBox_);
        const QSignalBlocker axesActionBlocker(showAxesAction_);
        const QSignalBlocker boundsActionBlocker(showBoundingBoxAction_);
        const QSignalBlocker rgbActionBlocker(rgbColorAction_);
        const QSignalBlocker elevationActionBlocker(elevationColorAction_);
        const QSignalBlocker singleActionBlocker(singleColorAction_);
        const QSignalBlocker classificationActionBlocker(classificationColorAction_);
        const QSignalBlocker colorfulThemeBlocker(themeColorfulAction_);
        const QSignalBlocker whiteThemeBlocker(themeWhiteAction_);
        const QSignalBlocker darkThemeBlocker(themeDarkGrayAction_);
        const QSignalBlocker measurementActionBlocker(measureAction_);
        const QSignalBlocker englishLanguageBlocker(languageEnglishAction_);
        const QSignalBlocker chineseLanguageBlocker(languageChineseAction_);
        const QSignalBlocker clearanceRulePresetBlocker(clearanceRulePresetComboBox_);
        const QSignalBlocker vegetationSearchRadiusBlocker(vegetationSearchRadiusSpinBox_);
        const QSignalBlocker vegetationClusterGapBlocker(vegetationClusterGapSpinBox_);
        const QSignalBlocker vegetationClusterPointCountBlocker(vegetationClusterPointCountSpinBox_);
        const QSignalBlocker preferVegetationClassificationBlocker(preferVegetationClassificationCheckBox_);
        const QSignalBlocker aircraftProfileBlocker(aircraftProfileComboBox_);
        const QSignalBlocker routeSafetyHeightBlocker(routeSafetyHeightSpinBox_);
        const QSignalBlocker routeWaypointSpeedBlocker(routeWaypointSpeedSpinBox_);
        const QSignalBlocker routeWaypointSpacingBlocker(routeWaypointSpacingSpinBox_);
        const QSignalBlocker routeSmoothingStrengthBlocker(routeSmoothingStrengthSpinBox_);
        const QSignalBlocker routeHeightOffsetBlocker(routeHeightOffsetSpinBox_);

        pointSizeSlider_->setValue(static_cast<int>(options.pointSize));
        pointOpacitySlider_->setValue(static_cast<int>(std::lround(options.pointOpacity * 100.0f)));
        depthCueSlider_->setValue(static_cast<int>(std::lround(options.depthCueStrength * 100.0f)));
        edlStrengthSlider_->setValue(static_cast<int>(std::lround(options.edlStrength * 100.0f)));
        colorModeComboBox_->setCurrentIndex(static_cast<int>(options.colorMode));
        roundSplatsCheckBox_->setChecked(options.useRoundSplats);
        axesCheckBox_->setChecked(options.showAxes);
        boundingBoxCheckBox_->setChecked(options.showBoundingBox);
        invertOrbitCheckBox_->setChecked(interactionOptions.invertOrbitDrag);
        invertPanCheckBox_->setChecked(interactionOptions.invertPanDrag);
        invertWheelCheckBox_->setChecked(interactionOptions.invertWheelZoom);

        showAxesAction_->setChecked(options.showAxes);
        showBoundingBoxAction_->setChecked(options.showBoundingBox);
        rgbColorAction_->setChecked(options.colorMode == PointCloudColorMode::Rgb);
        elevationColorAction_->setChecked(options.colorMode == PointCloudColorMode::Elevation);
        singleColorAction_->setChecked(options.colorMode == PointCloudColorMode::SingleColor);
        classificationColorAction_->setChecked(options.colorMode == PointCloudColorMode::Classification);
        measureAction_->setChecked(viewer_->measurementEnabled());
        languageEnglishAction_->setChecked(currentLanguage_ == UiLanguage::English);
        languageChineseAction_->setChecked(currentLanguage_ == UiLanguage::Chinese);
        if (clearanceRulePresetComboBox_ != nullptr) {
            const int presetIndex = clearanceRulePresetComboBox_->findData(static_cast<int>(clearanceRulePreset_));
            clearanceRulePresetComboBox_->setCurrentIndex(presetIndex >= 0 ? presetIndex : 0);
        }
        if (vegetationSearchRadiusSpinBox_ != nullptr) {
            vegetationSearchRadiusSpinBox_->setValue(vegetationSearchRadiusMeters_);
        }
        if (vegetationClusterGapSpinBox_ != nullptr) {
            vegetationClusterGapSpinBox_->setValue(vegetationClusterGapMeters_);
        }
        if (vegetationClusterPointCountSpinBox_ != nullptr) {
            vegetationClusterPointCountSpinBox_->setValue(vegetationClusterPointCount_);
        }
        if (preferVegetationClassificationCheckBox_ != nullptr) {
            preferVegetationClassificationCheckBox_->setChecked(preferVegetationClassification_);
        }
        if (aircraftProfileComboBox_ != nullptr) {
            const int profileIndex = aircraftProfileComboBox_->findData(static_cast<int>(routePlanningOptions_.aircraftProfile));
            aircraftProfileComboBox_->setCurrentIndex(profileIndex >= 0 ? profileIndex : 0);
        }
        if (routeSafetyHeightSpinBox_ != nullptr) {
            routeSafetyHeightSpinBox_->setValue(routePlanningOptions_.safety.safetyHeightMeters);
        }
        if (routeWaypointSpeedSpinBox_ != nullptr) {
            routeWaypointSpeedSpinBox_->setValue(routePlanningOptions_.safety.defaultWaypointSpeedMps);
        }
        if (routeWaypointSpacingSpinBox_ != nullptr) {
            routeWaypointSpacingSpinBox_->setValue(routePlanningOptions_.generation.waypointSpacingMeters);
        }
        if (routeSmoothingStrengthSpinBox_ != nullptr) {
            routeSmoothingStrengthSpinBox_->setValue(routePlanningOptions_.generation.smoothingStrengthPercent);
        }
        if (routeHeightOffsetSpinBox_ != nullptr) {
            routeHeightOffsetSpinBox_->setValue(routePlanningOptions_.safety.heightOffsetMeters);
        }

        if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
            themeColorfulAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016Colorful);
            themeWhiteAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016White);
            themeDarkGrayAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016DarkGray);
        }
    }

    updateSliderValueLabel(pointSizeSlider_, pointSizeValueLabel_, tr("%1 px"));
    updateSliderValueLabel(pointOpacitySlider_, pointOpacityValueLabel_, tr("%1%"));
    updateSliderValueLabel(depthCueSlider_, depthCueValueLabel_, tr("%1%"));
    updateSliderValueLabel(edlStrengthSlider_, edlStrengthValueLabel_, tr("%1%"));

    setColorButtonAppearance(pointColorButton_, options.singleColor, tr("Pick Color"));
    setColorButtonAppearance(backgroundColorButton_, options.backgroundColor, tr("Pick Background"));
    updateClassificationColorTable();
    updateNavigationHelpText();
    updateDatasetPanel();
    updateMeasurementPanel();
    updateTowerPanel();
    updateIssuePanel();
    updateVegetationRiskPanel();
    updateRoutePlanningPanel();
    updateActionState();
}

void MainWindow::updateDatasetPanel()
{
    const PointCloudData* pointCloudData = viewer_->pointCloudData();
    if (pointCloudData == nullptr && viewer_->currentFilePaths().isEmpty()) {
        datasetNameValueLabel_->setText(tr("No dataset loaded"));
        datasetPathValueLabel_->setText(tr("Open, add, or drag LAS/LAZ files into the window."));
        datasetPointsValueLabel_->setText(QStringLiteral("0"));
        datasetBoundsValueLabel_->setText(tr("N/A"));
        datasetExtentValueLabel_->setText(tr("N/A"));
        datasetColorValueLabel_->setText(colorModeName(viewer_->visualizationOptions().colorMode));
        return;
    }

    if (pointCloudData == nullptr) {
        datasetNameValueLabel_->setText(tr("All datasets hidden"));
        datasetPathValueLabel_->setText(datasetPathSummary(viewer_->currentFilePaths()));
        datasetPointsValueLabel_->setText(QStringLiteral("0"));
        datasetBoundsValueLabel_->setText(tr("N/A"));
        datasetExtentValueLabel_->setText(tr("N/A"));
        datasetColorValueLabel_->setText(colorModeName(viewer_->visualizationOptions().colorMode));
        return;
    }

    const QStringList filePaths = viewer_->currentFilePaths();
    const PointRecord& minBounds = pointCloudData->minBounds();
    const PointRecord& maxBounds = pointCloudData->maxBounds();

    datasetNameValueLabel_->setText(
        filePaths.size() == 1
            ? QFileInfo(filePaths.constFirst()).fileName()
            : tr("%1 datasets").arg(QLocale().toString(filePaths.size())));
    datasetPathValueLabel_->setText(datasetPathSummary(filePaths));
    datasetPointsValueLabel_->setText(QLocale().toString(static_cast<qlonglong>(pointCloudData->size())));
    datasetBoundsValueLabel_->setText(
        tr("Min (%1)\nMax (%2)")
            .arg(formatTriplet(minBounds.x, minBounds.y, minBounds.z))
            .arg(formatTriplet(maxBounds.x, maxBounds.y, maxBounds.z)));
    datasetExtentValueLabel_->setText(formatTriplet(
        maxBounds.x - minBounds.x,
        maxBounds.y - minBounds.y,
        maxBounds.z - minBounds.z));
    datasetColorValueLabel_->setText(
        tr("%1 | Native RGB: %2")
            .arg(colorModeName(viewer_->visualizationOptions().colorMode))
            .arg(pointCloudData->hasColor() ? tr("yes") : tr("no")));
}

void MainWindow::updateActionState()
{
    const bool hasPointCloud = viewer_->hasPointCloud();
    const bool profileClassificationReady = hasPointCloud;
    const bool hasTowerMarkers = viewer_ != nullptr && !viewer_->towerMarkers().isEmpty();
    const bool hasTowerSelection = viewer_ != nullptr && viewer_->selectedTowerIndex() >= 0;
    const bool towerToolActive = viewer_ != nullptr && viewer_->towerEditMode() != TowerEditMode::None;
    const bool hasIssues = viewer_ != nullptr && !viewer_->inspectionIssues().isEmpty();
    const bool hasIssueSelection = viewer_ != nullptr && viewer_->selectedIssueIndex() >= 0;
    const bool issueToolActive = viewer_ != nullptr && viewer_->issueEditMode() != IssueEditMode::None;
    const bool hasVegetationRisks = !vegetationRiskResults_.isEmpty();
    const bool hasVegetationRiskSelection = selectedVegetationRiskIndex_ >= 0 && selectedVegetationRiskIndex_ < vegetationRiskResults_.size();
    const bool hasInspectionRoute = !inspectionRoute_.waypoints.isEmpty();
    const bool hasRouteWaypointSelection = selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < inspectionRoute_.waypoints.size();
    const bool hasMeasuredCorridor = viewer_ != nullptr && viewer_->measurementResult().isComplete();
    const QTreeWidgetItem* currentProjectItem = projectTreeWidget_ != nullptr ? projectTreeWidget_->currentItem() : nullptr;
    const bool hasDatasetSelection = !selectedDatasetPath().isEmpty();
    const bool hasPathSelection = !projectTreeItemFilePath(currentProjectItem).isEmpty();
    openProjectAction_->setEnabled(true);
    saveProjectAction_->setEnabled(viewer_ != nullptr && !viewer_->currentFilePaths().isEmpty());
    saveProjectAsAction_->setEnabled(viewer_ != nullptr && !viewer_->currentFilePaths().isEmpty());
    projectCoordinateSystemsAction_->setEnabled(viewer_ != nullptr);
    addPointCloudAction_->setEnabled(true);
    removeDatasetAction_->setEnabled(hasDatasetSelection);
    locateDatasetAction_->setEnabled(hasPathSelection);
    copyDatasetPathAction_->setEnabled(hasPathSelection);
    expandProjectTreeAction_->setEnabled(projectTreeWidget_ != nullptr && projectTreeWidget_->topLevelItemCount() > 0);
    collapseProjectTreeAction_->setEnabled(projectTreeWidget_ != nullptr && projectTreeWidget_->topLevelItemCount() > 0);
    clearAction_->setEnabled(viewer_ != nullptr && !viewer_->currentFilePaths().isEmpty());
    fitSceneAction_->setEnabled(hasPointCloud);
    topViewAction_->setEnabled(hasPointCloud);
    frontViewAction_->setEnabled(hasPointCloud);
    rightViewAction_->setEnabled(hasPointCloud);
    measureAction_->setEnabled(hasPointCloud);
    profileClassificationAction_->setEnabled(profileClassificationReady);
    showProfileClassificationDockAction_->setEnabled(true);
    showProfileClassificationDockAction_->setChecked(profileClassificationDock_ != nullptr && profileClassificationDock_->isVisible());
    saveProfileClassificationEditsAction_->setEnabled(hasPointCloud && viewer_->classificationEditedPointCount() > 0);
    clearMeasurementAction_->setEnabled(hasPointCloud && viewer_->measurementResult().hasStartPoint);
    analyzeVegetationRisksAction_->setEnabled(hasPointCloud && hasMeasuredCorridor);
    exportClearanceCsvAction_->setEnabled(hasPointCloud && viewer_->measurementResult().isComplete());
    showProfileDockAction_->setEnabled(true);
    showProfileDockAction_->setChecked(profileDock_ != nullptr && profileDock_->isVisible());
    measurementToggleButton_->setEnabled(hasPointCloud);
    measurementClearButton_->setEnabled(hasPointCloud && viewer_->measurementResult().hasStartPoint);
    clearanceThresholdSpinBox_->setEnabled(hasPointCloud);
    if (classificationColorsGroupBox_ != nullptr) {
        classificationColorsGroupBox_->setEnabled(hasPointCloud);
    }
    if (resetClassificationColorsButton_ != nullptr) {
        resetClassificationColorsButton_->setEnabled(hasPointCloud);
    }
    if (clearanceSegmentsTableWidget_ != nullptr) {
        clearanceSegmentsTableWidget_->setEnabled(hasPointCloud && viewer_->measurementResult().isComplete());
    }
    startTowerEditAction_->setEnabled(hasPointCloud && !towerEditingEnabled_);
    finishTowerEditAction_->setEnabled(towerEditingEnabled_);
    addTowerAction_->setEnabled(hasPointCloud);
    insertTowerAction_->setEnabled(hasPointCloud && hasTowerSelection);
    moveTowerAction_->setEnabled(hasPointCloud && hasTowerSelection);
    editCurrentTowerAction_->setEnabled(hasPointCloud && hasTowerSelection);
    focusTowerAction_->setEnabled(hasTowerSelection);
    removeTowerAction_->setEnabled(hasTowerSelection);
    clearTowersAction_->setEnabled(hasTowerMarkers && !towerToolActive);
    cancelTowerToolAction_->setEnabled(towerToolActive);
    startIssueMarkAction_->setEnabled(hasPointCloud && !issueToolActive);
    cancelIssueToolAction_->setEnabled(issueToolActive);
    focusIssueAction_->setEnabled(hasIssueSelection);
    removeIssueAction_->setEnabled(hasIssueSelection);
    clearIssuesAction_->setEnabled(hasIssues);
    exportIssuesCsvAction_->setEnabled(hasIssues);
    exportInspectionReportAction_->setEnabled(hasPointCloud && (hasTowerMarkers || hasIssues));
    focusVegetationRiskAction_->setEnabled(hasVegetationRiskSelection);
    createIssueFromRiskAction_->setEnabled(hasVegetationRiskSelection);
    createIssuesFromRisksAction_->setEnabled(hasVegetationRisks);
    clearVegetationRisksAction_->setEnabled(hasVegetationRisks);
    generateInspectionRouteAction_->setEnabled(hasVegetationRisks && hasPointCloud);
    regenerateInspectionRouteAction_->setEnabled(hasVegetationRisks && hasPointCloud);
    clearInspectionRouteAction_->setEnabled(hasInspectionRoute);
    focusRouteWaypointAction_->setEnabled(hasRouteWaypointSelection);
    importRouteKmlAction_->setEnabled(hasPointCloud);
    exportRouteKmlAction_->setEnabled(hasInspectionRoute);
    exportRouteDjiKmzAction_->setEnabled(hasInspectionRoute && inspectionRoute_.waypoints.size() >= 2);
}

void MainWindow::setColorButtonAppearance(QPushButton* button, const QColor& color, const QString& fallbackText) const
{
    if (button == nullptr) {
        return;
    }

    const int luminance = (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
    const QString foreground = luminance >= 150 ? QStringLiteral("#111827") : QStringLiteral("#f8fafc");

    button->setText(fallbackText);
    button->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "background-color: %1;"
        "color: %2;"
        "border: 1px solid rgba(15, 23, 42, 0.14);"
        "border-radius: 4px;"
        "padding: 6px 10px;"
        "}").arg(color.name(), foreground));
}

void MainWindow::appendLog(LogLevel level, const QString& message)
{
    if (logTextEdit_ == nullptr || message.trimmed().isEmpty()) {
        return;
    }

    QString levelText;
    QString accentColor;
    QString badgeBackground;
    QString badgeForeground;
    QString messageColor;
    switch (level) {
    case LogLevel::Warning:
        levelText = QStringLiteral("WARN");
        accentColor = QStringLiteral("#f59e0b");
        badgeBackground = QStringLiteral("#3b2a06");
        badgeForeground = QStringLiteral("#fde68a");
        messageColor = QStringLiteral("#fef3c7");
        break;
    case LogLevel::Error:
        levelText = QStringLiteral("ERROR");
        accentColor = QStringLiteral("#ef4444");
        badgeBackground = QStringLiteral("#3f1319");
        badgeForeground = QStringLiteral("#fecaca");
        messageColor = QStringLiteral("#fee2e2");
        break;
    case LogLevel::Info:
    default:
        levelText = QStringLiteral("INFO");
        accentColor = QStringLiteral("#38bdf8");
        badgeBackground = QStringLiteral("#0f2c3d");
        badgeForeground = QStringLiteral("#d7f0ff");
        messageColor = QStringLiteral("#e2e8f0");
        break;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    const QString entryHtml = QStringLiteral(
        "<div style='margin:0 0 8px 0; padding:8px 10px; "
        "border-left:4px solid %1; background-color:#0f172a; border-radius:8px;'>"
        "<span style='color:#94a3b8; font-family:Consolas, \"Courier New\", monospace; font-size:10px;'>%2</span>"
        "<span style='display:inline-block; margin-left:8px; padding:2px 7px; border-radius:999px; "
        "background-color:%3; color:%4; font-family:Consolas, \"Courier New\", monospace; font-size:10px; font-weight:700;'>%5</span>"
        "<div style='margin-top:6px; color:%6; line-height:1.45;'>%7</div>"
        "</div>")
            .arg(accentColor)
            .arg(timestamp)
            .arg(badgeBackground)
            .arg(badgeForeground)
            .arg(levelText)
            .arg(messageColor)
            .arg(message.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br/>")));
    QTextCursor cursor = logTextEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    logTextEdit_->setTextCursor(cursor);
    logTextEdit_->insertHtml(entryHtml);
    logTextEdit_->insertHtml(QStringLiteral("<div style='height:2px;'></div>"));
    logTextEdit_->verticalScrollBar()->setValue(logTextEdit_->verticalScrollBar()->maximum());
}

void MainWindow::showUserMessage(LogLevel level, const QString& message, int timeoutMs)
{
    if (statusBar() != nullptr) {
        statusBar()->showMessage(message, timeoutMs);
    }
    appendLog(level, message);
}

void MainWindow::loadInteractionSettings()
{
    QSettings settings;
    InteractionOptions options;
    options.invertOrbitDrag = settings.value(QStringLiteral("interaction/invertOrbitDrag"), false).toBool();
    options.invertPanDrag = settings.value(QStringLiteral("interaction/invertPanDrag"), false).toBool();
    options.invertWheelZoom = settings.value(QStringLiteral("interaction/invertWheelZoom"), false).toBool();
    viewer_->setInteractionOptions(options);
}

void MainWindow::persistInteractionSettings() const
{
    QSettings settings;
    const InteractionOptions& options = viewer_->interactionOptions();
    settings.setValue(QStringLiteral("interaction/invertOrbitDrag"), options.invertOrbitDrag);
    settings.setValue(QStringLiteral("interaction/invertPanDrag"), options.invertPanDrag);
    settings.setValue(QStringLiteral("interaction/invertWheelZoom"), options.invertWheelZoom);
}

void MainWindow::loadMeasurementSettings()
{
    QSettings settings;
    clearanceWarningThresholdMeters_ = settings.value(
        QStringLiteral("measurement/clearanceThresholdMeters"),
        clearanceWarningThresholdMeters_).toDouble();
    clearanceRulePreset_ = static_cast<ClearanceRulePreset>(settings.value(
        QStringLiteral("measurement/clearanceRulePreset"),
        static_cast<int>(clearanceRulePreset_)).toInt());
    vegetationSearchRadiusMeters_ = settings.value(
        QStringLiteral("measurement/vegetationSearchRadiusMeters"),
        vegetationSearchRadiusMeters_).toDouble();
    vegetationClusterGapMeters_ = settings.value(
        QStringLiteral("measurement/vegetationClusterGapMeters"),
        vegetationClusterGapMeters_).toDouble();
    vegetationClusterPointCount_ = settings.value(
        QStringLiteral("measurement/vegetationClusterPointCount"),
        vegetationClusterPointCount_).toInt();
    preferVegetationClassification_ = settings.value(
        QStringLiteral("measurement/preferVegetationClassification"),
        preferVegetationClassification_).toBool();

    if (clearanceThresholdSpinBox_ != nullptr) {
        const QSignalBlocker blocker(clearanceThresholdSpinBox_);
        clearanceThresholdSpinBox_->setValue(clearanceWarningThresholdMeters_);
    }
    if (clearanceRulePresetComboBox_ != nullptr) {
        const QSignalBlocker blocker(clearanceRulePresetComboBox_);
        const int presetIndex = clearanceRulePresetComboBox_->findData(static_cast<int>(clearanceRulePreset_));
        clearanceRulePresetComboBox_->setCurrentIndex(presetIndex >= 0 ? presetIndex : 0);
    }
    if (vegetationSearchRadiusSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationSearchRadiusSpinBox_);
        vegetationSearchRadiusSpinBox_->setValue(vegetationSearchRadiusMeters_);
    }
    if (vegetationClusterGapSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationClusterGapSpinBox_);
        vegetationClusterGapSpinBox_->setValue(vegetationClusterGapMeters_);
    }
    if (vegetationClusterPointCountSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationClusterPointCountSpinBox_);
        vegetationClusterPointCountSpinBox_->setValue(vegetationClusterPointCount_);
    }
    if (preferVegetationClassificationCheckBox_ != nullptr) {
        const QSignalBlocker blocker(preferVegetationClassificationCheckBox_);
        preferVegetationClassificationCheckBox_->setChecked(preferVegetationClassification_);
    }
}

void MainWindow::persistMeasurementSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("measurement/clearanceThresholdMeters"), clearanceWarningThresholdMeters_);
    settings.setValue(QStringLiteral("measurement/clearanceRulePreset"), static_cast<int>(clearanceRulePreset_));
    settings.setValue(QStringLiteral("measurement/vegetationSearchRadiusMeters"), vegetationSearchRadiusMeters_);
    settings.setValue(QStringLiteral("measurement/vegetationClusterGapMeters"), vegetationClusterGapMeters_);
    settings.setValue(QStringLiteral("measurement/vegetationClusterPointCount"), vegetationClusterPointCount_);
    settings.setValue(QStringLiteral("measurement/preferVegetationClassification"), preferVegetationClassification_);
}

void MainWindow::loadVisualizationSettings()
{
    if (viewer_ == nullptr) {
        return;
    }

    QSettings settings;
    const PointCloudVisualizationOptions defaults = viewer_->visualizationOptions();
    viewer_->setPointSize(settings.value(QStringLiteral("visualization/pointSize"), defaults.pointSize).toInt());
    viewer_->setPointOpacity(settings.value(QStringLiteral("visualization/pointOpacity"), defaults.pointOpacity * 100.0f).toInt());
    viewer_->setDepthCueStrength(settings.value(QStringLiteral("visualization/depthCueStrength"), defaults.depthCueStrength * 100.0f).toInt());
    viewer_->setEdlStrength(settings.value(QStringLiteral("visualization/edlStrength"), defaults.edlStrength * 100.0f).toInt());
    viewer_->setColorMode(settings.value(QStringLiteral("visualization/colorMode"), static_cast<int>(defaults.colorMode)).toInt());
    viewer_->setSingleColor(settings.value(QStringLiteral("visualization/singleColor"), defaults.singleColor).value<QColor>());
    const QJsonDocument classificationColorDocument = QJsonDocument::fromJson(
        settings.value(QStringLiteral("visualization/classificationColorsJson")).toByteArray());
    if (classificationColorDocument.isObject()) {
        viewer_->setClassificationColorMap(
            classificationColorMapFromJson(classificationColorDocument.object(), defaults.classificationColors));
    }
    const QJsonDocument classificationVisibilityDocument = QJsonDocument::fromJson(
        settings.value(QStringLiteral("visualization/classificationVisibilityJson")).toByteArray());
    if (classificationVisibilityDocument.isObject()) {
        viewer_->setClassificationVisibilityMap(
            classificationVisibilityMapFromJson(classificationVisibilityDocument.object(), defaults.classificationVisibility));
    }
    const QJsonDocument classificationNameDocument = QJsonDocument::fromJson(
        settings.value(QStringLiteral("visualization/classificationNameOverridesJson")).toByteArray());
    classificationNameOverrides_ = classificationNameDocument.isObject()
        ? classificationNameMapFromJson(classificationNameDocument.object())
        : QMap<int, QString>();
    viewer_->setClassificationFallbackColor(
        settings.value(QStringLiteral("visualization/classificationFallbackColor"), defaults.classificationFallbackColor).value<QColor>());
    viewer_->setBackgroundColor(settings.value(QStringLiteral("visualization/backgroundColor"), defaults.backgroundColor).value<QColor>());
    viewer_->setUseRoundSplats(settings.value(QStringLiteral("visualization/useRoundSplats"), defaults.useRoundSplats).toBool());
    viewer_->setShowAxes(settings.value(QStringLiteral("visualization/showAxes"), defaults.showAxes).toBool());
    viewer_->setShowBoundingBox(settings.value(QStringLiteral("visualization/showBoundingBox"), defaults.showBoundingBox).toBool());
}

void MainWindow::persistVisualizationSettings() const
{
    if (viewer_ == nullptr) {
        return;
    }

    QSettings settings;
    const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();
    settings.setValue(QStringLiteral("visualization/pointSize"), options.pointSize);
    settings.setValue(QStringLiteral("visualization/pointOpacity"), options.pointOpacity * 100.0f);
    settings.setValue(QStringLiteral("visualization/depthCueStrength"), options.depthCueStrength * 100.0f);
    settings.setValue(QStringLiteral("visualization/edlStrength"), options.edlStrength * 100.0f);
    settings.setValue(QStringLiteral("visualization/colorMode"), static_cast<int>(options.colorMode));
    settings.setValue(QStringLiteral("visualization/singleColor"), options.singleColor);
    settings.setValue(
        QStringLiteral("visualization/classificationColorsJson"),
        QJsonDocument(classificationColorMapToJson(options.classificationColors)).toJson(QJsonDocument::Compact));
    settings.setValue(
        QStringLiteral("visualization/classificationVisibilityJson"),
        QJsonDocument(classificationVisibilityMapToJson(options.classificationVisibility)).toJson(QJsonDocument::Compact));
    settings.setValue(
        QStringLiteral("visualization/classificationNameOverridesJson"),
        QJsonDocument(classificationNameMapToJson(classificationNameOverrides_)).toJson(QJsonDocument::Compact));
    settings.setValue(QStringLiteral("visualization/classificationFallbackColor"), options.classificationFallbackColor);
    settings.setValue(QStringLiteral("visualization/backgroundColor"), options.backgroundColor);
    settings.setValue(QStringLiteral("visualization/useRoundSplats"), options.useRoundSplats);
    settings.setValue(QStringLiteral("visualization/showAxes"), options.showAxes);
    settings.setValue(QStringLiteral("visualization/showBoundingBox"), options.showBoundingBox);
}

void MainWindow::loadLanguageSettings()
{
    QSettings settings;
    const QString storedLanguage = settings.value(QStringLiteral("ui/language")).toString();
    if (storedLanguage == QStringLiteral("zh_CN")) {
        currentLanguage_ = UiLanguage::Chinese;
    } else if (storedLanguage == QStringLiteral("en")) {
        currentLanguage_ = UiLanguage::English;
    } else {
        currentLanguage_ = defaultLanguageFromLocale();
    }
}

void MainWindow::persistLanguageSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/language"), languageCodeFor(currentLanguage_));
}

void MainWindow::loadWindowSettings()
{
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    const QByteArray state = settings.value(QStringLiteral("window/state")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const bool restoredState = !state.isEmpty() && restoreState(state, kMainWindowStateVersion);
    if (settings.value(QStringLiteral("window/maximized"), false).toBool()) {
        showMaximized();
    }

    if (inspectorTabWidget_ != nullptr) {
        inspectorTabWidget_->setCurrentIndex(settings.value(QStringLiteral("window/inspectorTab"), 0).toInt());
    }

    if (!restoredState) {
        const bool showLog = settings.value(QStringLiteral("window/showLog"), false).toBool();
        const bool showProfileClassification = settings.value(QStringLiteral("window/showProfileClassification"), false).toBool();
        if (logDock_ != nullptr) {
            logDock_->setVisible(showLog);
        }
        if (profileClassificationDock_ != nullptr) {
            profileClassificationDock_->setVisible(showProfileClassification);
        }
    }

    syncProfileDockForMeasurementMode(viewer_ != nullptr && viewer_->measurementEnabled());

    if (showLogAction_ != nullptr) {
        const QSignalBlocker blocker(showLogAction_);
        showLogAction_->setChecked(logDock_ != nullptr && logDock_->isVisible());
    }
    if (showProfileDockAction_ != nullptr) {
        const QSignalBlocker blocker(showProfileDockAction_);
        showProfileDockAction_->setChecked(profileDock_ != nullptr && profileDock_->isVisible());
    }
    if (showProfileClassificationDockAction_ != nullptr) {
        const QSignalBlocker blocker(showProfileClassificationDockAction_);
        showProfileClassificationDockAction_->setChecked(profileClassificationDock_ != nullptr && profileClassificationDock_->isVisible());
    }

    if (projectDock_ != nullptr) {
        projectDock_->show();
        projectDock_->raise();
    }
}

void MainWindow::persistWindowSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState(kMainWindowStateVersion));
    settings.setValue(QStringLiteral("window/maximized"), isMaximized());
    settings.setValue(QStringLiteral("window/showLog"), logDock_ != nullptr && logDock_->isVisible());
    settings.setValue(QStringLiteral("window/showProfile"), profileDock_ != nullptr && profileDock_->isVisible());
    settings.setValue(
        QStringLiteral("window/showProfileClassification"),
        profileClassificationDock_ != nullptr && profileClassificationDock_->isVisible());
    settings.setValue(
        QStringLiteral("window/inspectorTab"),
        inspectorTabWidget_ != nullptr ? inspectorTabWidget_->currentIndex() : 0);
}

void MainWindow::loadThemeSettings()
{
    const int storedTheme = QSettings().value(
        QStringLiteral("ui/theme"),
        static_cast<int>(Qtitan::RibbonStyle::Office2016Colorful)).toInt();

    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        const auto theme = static_cast<Qtitan::RibbonStyle::Theme>(storedTheme);
        ribbonStyle->setTheme(theme);
        updateWindowChromePalette(theme);
    }
}

void MainWindow::persistThemeSettings() const
{
    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        QSettings().setValue(QStringLiteral("ui/theme"), static_cast<int>(ribbonStyle->getTheme()));
    }
}

void MainWindow::syncProfileDockForMeasurementMode(bool measurementEnabled)
{
    if (profileDock_ == nullptr) {
        return;
    }

    const bool targetVisible = measurementEnabled;
    if (profileDock_->isVisible() != targetVisible) {
        profileDock_->setVisible(targetVisible);
    }

    if (showProfileDockAction_ != nullptr && showProfileDockAction_->isChecked() != targetVisible) {
        const QSignalBlocker blocker(showProfileDockAction_);
        showProfileDockAction_->setChecked(targetVisible);
    }
}

void MainWindow::beginOperationProgress(const QString& message)
{
    if (globalProgressBar_ != nullptr) {
        globalProgressBar_->setVisible(true);
        globalProgressBar_->setRange(0, 0);
    }
    if (statusBar() != nullptr && !message.trimmed().isEmpty()) {
        statusBar()->showMessage(message);
    }
}

void MainWindow::updateOperationProgress(const QString& message, int value, int maximum)
{
    if (globalProgressBar_ != nullptr) {
        if (!globalProgressBar_->isVisible()) {
            globalProgressBar_->setVisible(true);
        }
        if (maximum <= 0) {
            globalProgressBar_->setRange(0, 0);
        } else {
            globalProgressBar_->setRange(0, maximum);
            globalProgressBar_->setValue(std::clamp(value, 0, maximum));
        }
    }

    if (statusBar() != nullptr && !message.trimmed().isEmpty()) {
        statusBar()->showMessage(message);
    }
}

void MainWindow::endOperationProgress()
{
    if (globalProgressBar_ != nullptr) {
        globalProgressBar_->hide();
        globalProgressBar_->setRange(0, 1000);
        globalProgressBar_->setValue(0);
    }
}

void MainWindow::updateNavigationHelpText()
{
    if (navigationTipsLabel_ == nullptr || viewer_ == nullptr) {
        return;
    }

    const InteractionOptions& options = viewer_->interactionOptions();
    navigationTipsLabel_->setText(tr(
        "Left drag orbits (%1), middle or right drag pans (%2), and the mouse wheel zooms (%3). "
        "Use the toggles below to match your preferred interaction direction.")
        .arg(options.invertOrbitDrag ? tr("inverted") : tr("normal"))
        .arg(options.invertPanDrag ? tr("inverted") : tr("normal"))
        .arg(options.invertWheelZoom ? tr("inverted") : tr("normal")));
}

void MainWindow::updateMeasurementPanel()
{
    if (viewer_ == nullptr || measurementToggleButton_ == nullptr || measurementClearButton_ == nullptr) {
        return;
    }

    const MeasurementResult& measurementResult = viewer_->measurementResult();
    const ClearanceAnalysisResult clearanceAnalysis = analyzeClearancePath(
        measurementResult.points,
        static_cast<float>(clearanceWarningThresholdMeters_));
    const ClearanceRuleEvaluationResult ruleEvaluation = evaluateClearanceRules(
        clearanceAnalysis,
        { clearanceRulePreset_, static_cast<float>(clearanceWarningThresholdMeters_) });
    measurementToggleButton_->setText(
        viewer_->measurementEnabled() ? tr("Stop Measurement") : tr("Start Measurement"));
    measurementClearButton_->setText(tr("Clear Measurement"));

    measurementStartValueLabel_->setText(measurementPointText(measurementResult, true));
    measurementEndValueLabel_->setText(measurementPointText(measurementResult, false));
    measurementDistanceValueLabel_->setText(
        measurementResult.isComplete() ? formatCoordinate(measurementResult.distance3d) : tr("N/A"));
    measurementHorizontalDistanceValueLabel_->setText(
        clearanceAnalysis.isValid() ? formatCoordinate(clearanceAnalysis.totalHorizontalDistance) : tr("N/A"));
    measurementDeltaZValueLabel_->setText(
        measurementResult.isComplete() ? formatCoordinate(measurementResult.deltaZ) : tr("N/A"));
    measurementSegmentsValueLabel_->setText(
        measurementResult.isComplete()
            ? QLocale().toString(measurementResult.pointCount() - 1)
            : QStringLiteral("0"));

    QString clearanceStatusText = tr("Add at least two measured points to analyze corridor clearance.");
    QString clearanceStatusStyle = QStringLiteral("color: #475569;");
    if (clearanceRuleBandsValueLabel_ != nullptr) {
        clearanceRuleBandsValueLabel_->setText(
            ruleEvaluation.enabled()
                ? tr("Advisory %1 m | Warning %2 m | Critical %3 m")
                    .arg(formatCoordinate(ruleEvaluation.advisoryThreshold))
                    .arg(formatCoordinate(ruleEvaluation.warningThreshold))
                    .arg(formatCoordinate(ruleEvaluation.criticalThreshold))
                : tr("Disabled"));
    }
    if (!clearanceAnalysis.isValid()) {
        clearanceShortestValueLabel_->setText(tr("N/A"));
        clearanceWarningCountValueLabel_->setText(tr("0 / 0 / 0"));
    } else {
        clearanceShortestValueLabel_->setText(formatCoordinate(clearanceAnalysis.minimumSegmentDistance));
        clearanceWarningCountValueLabel_->setText(
            tr("%1 / %2 / %3")
                .arg(QLocale().toString(ruleEvaluation.advisoryCount))
                .arg(QLocale().toString(ruleEvaluation.warningCount))
                .arg(QLocale().toString(ruleEvaluation.criticalCount)));

        if (!ruleEvaluation.enabled()) {
            clearanceStatusText = tr("Clearance threshold is disabled. Set a value above 0 m to enable risk bands.");
            clearanceStatusStyle = QStringLiteral("color: #475569;");
        } else if (ruleEvaluation.criticalCount > 0) {
            clearanceStatusText = tr("%1 critical segment(s), %2 warning segment(s), and %3 advisory segment(s) were detected under %4.")
                .arg(QLocale().toString(ruleEvaluation.criticalCount))
                .arg(QLocale().toString(ruleEvaluation.warningCount))
                .arg(QLocale().toString(ruleEvaluation.advisoryCount))
                .arg(ruleEvaluation.presetLabel);
            clearanceStatusStyle = QStringLiteral("color: #b91c1c; font-weight: 600;");
        } else if (ruleEvaluation.warningCount > 0 || ruleEvaluation.advisoryCount > 0) {
            clearanceStatusText = tr("%1 warning segment(s) and %2 advisory segment(s) were detected under %3.")
                .arg(QLocale().toString(ruleEvaluation.warningCount))
                .arg(QLocale().toString(ruleEvaluation.advisoryCount))
                .arg(ruleEvaluation.presetLabel);
            clearanceStatusStyle = QStringLiteral("color: #b45309; font-weight: 600;");
        } else {
            clearanceStatusText = tr("All measured segments stay outside the active %1 risk bands.")
                .arg(ruleEvaluation.presetLabel);
            clearanceStatusStyle = QStringLiteral("color: #15803d; font-weight: 600;");
        }
    }

    clearanceStatusValueLabel_->setText(clearanceStatusText);
    clearanceStatusValueLabel_->setStyleSheet(clearanceStatusStyle);
    updateClearanceSegmentsTable(clearanceAnalysis);
    if (profilePlotWidget_ != nullptr) {
        profilePlotWidget_->setAnalysisResult(clearanceAnalysis);
        profilePlotWidget_->setRuleEvaluation(ruleEvaluation);
        profilePlotWidget_->setProfileMarkers(projectProfileMarkers(
            clearanceAnalysis,
            viewer_->towerMarkers(),
            viewer_->selectedTowerIndex(),
            viewer_->inspectionIssues(),
            viewer_->selectedIssueIndex()));
        profilePlotWidget_->setSelectedSegmentIndex(
            clearanceSegmentsTableWidget_ != nullptr ? clearanceSegmentsTableWidget_->currentRow() : -1);
    }
}

void MainWindow::updateClearanceSegmentsTable(const ClearanceAnalysisResult& clearanceAnalysis)
{
    if (clearanceSegmentsSummaryLabel_ == nullptr || clearanceSegmentsTableWidget_ == nullptr) {
        return;
    }

    const ClearanceRuleEvaluationResult ruleEvaluation = evaluateClearanceRules(
        clearanceAnalysis,
        { clearanceRulePreset_, static_cast<float>(clearanceWarningThresholdMeters_) });

    const int previousRow = clearanceSegmentsTableWidget_->currentRow();
    const QSignalBlocker blocker(clearanceSegmentsTableWidget_);
    clearanceSegmentsTableWidget_->setRowCount(0);

    if (!clearanceAnalysis.isValid()) {
        clearanceSegmentsSummaryLabel_->setText(
            tr("Add at least two measured points to list corridor segments and export clearance details."));
        clearanceSegmentsTableWidget_->clearSelection();
        return;
    }

    if (!ruleEvaluation.enabled()) {
        clearanceSegmentsSummaryLabel_->setText(
            tr("Listed %1 path segment(s). Set a threshold above 0 m to enable electric-scene risk bands.")
                .arg(QLocale().toString(clearanceAnalysis.segments.size())));
    } else if (ruleEvaluation.criticalCount > 0 || ruleEvaluation.warningCount > 0 || ruleEvaluation.advisoryCount > 0) {
        clearanceSegmentsSummaryLabel_->setText(
            tr("%1 critical, %2 warning, %3 advisory segment(s) under %4. Select a row to highlight it in the profile or export the full list.")
                .arg(QLocale().toString(ruleEvaluation.criticalCount))
                .arg(QLocale().toString(ruleEvaluation.warningCount))
                .arg(QLocale().toString(ruleEvaluation.advisoryCount))
                .arg(ruleEvaluation.presetLabel));
    } else {
        clearanceSegmentsSummaryLabel_->setText(
            tr("All %1 segment(s) stay outside the current %2 risk bands.")
                .arg(QLocale().toString(clearanceAnalysis.segments.size()))
                .arg(ruleEvaluation.presetLabel));
    }

    int preferredRow = previousRow;
    if (preferredRow < 0 || preferredRow >= clearanceAnalysis.segments.size()) {
        preferredRow = 0;
        for (int segmentIndex = 0; segmentIndex < clearanceAnalysis.segments.size(); ++segmentIndex) {
            if (segmentIndex < ruleEvaluation.segmentEvaluations.size()
                && ruleEvaluation.segmentEvaluations.at(segmentIndex).severity != AnalysisSeverity::None) {
                preferredRow = segmentIndex;
                break;
            }
        }
    }

    for (int segmentIndex = 0; segmentIndex < clearanceAnalysis.segments.size(); ++segmentIndex) {
        const ClearanceSegment& segment = clearanceAnalysis.segments.at(segmentIndex);
        const ClearanceSegmentEvaluation evaluation = segmentIndex < ruleEvaluation.segmentEvaluations.size()
            ? ruleEvaluation.segmentEvaluations.at(segmentIndex)
            : ClearanceSegmentEvaluation();
        clearanceSegmentsTableWidget_->insertRow(segmentIndex);

        auto createReadOnlyItem = [](const QString& text, const QColor& color = QColor(), Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags((item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            item->setTextAlignment(alignment);
            if (color.isValid()) {
                item->setForeground(color);
            }
            return item;
        };

        const QColor statusColor = analysisSeverityColor(evaluation.severity);
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            0,
            createReadOnlyItem(QLocale().toString(segmentIndex + 1), QColor(), Qt::AlignCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            1,
            createReadOnlyItem(QLocale().toString(segment.startPointIndex + 1), QColor(), Qt::AlignCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            2,
            createReadOnlyItem(QLocale().toString(segment.endPointIndex + 1), QColor(), Qt::AlignCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            3,
            createReadOnlyItem(
                tr("%1 - %2 m")
                    .arg(formatCoordinate(segment.chainageStart))
                    .arg(formatCoordinate(segment.chainageEnd))));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            4,
            createReadOnlyItem(formatCoordinate(segment.horizontalDistance), QColor(), Qt::AlignRight | Qt::AlignVCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            5,
            createReadOnlyItem(formatCoordinate(segment.distance3d), QColor(), Qt::AlignRight | Qt::AlignVCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            6,
            createReadOnlyItem(formatCoordinate(segment.deltaZ), QColor(), Qt::AlignRight | Qt::AlignVCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            7,
            createReadOnlyItem(evaluation.severityLabel, statusColor, Qt::AlignCenter));
    }

    if (clearanceSegmentsTableWidget_->rowCount() > 0) {
        const int normalizedRow = std::max(0, std::min(preferredRow, clearanceSegmentsTableWidget_->rowCount() - 1));
        clearanceSegmentsTableWidget_->setCurrentCell(normalizedRow, 0);
    } else {
        clearanceSegmentsTableWidget_->clearSelection();
    }
}

void MainWindow::updateVegetationRiskPanel()
{
    if (vegetationRiskCountValueLabel_ == nullptr || vegetationRiskStatusValueLabel_ == nullptr || vegetationRisksTableWidget_ == nullptr) {
        return;
    }

    const ClearanceAnalysisResult pathAnalysis = (viewer_ != nullptr)
        ? analyzeClearancePath(viewer_->measurementResult().points, static_cast<float>(clearanceWarningThresholdMeters_))
        : ClearanceAnalysisResult();
    const bool pathReady = pathAnalysis.isValid();

    vegetationRiskCountValueLabel_->setText(
        vegetationRiskResults_.isEmpty()
            ? tr("No vegetation risk clusters available.")
            : tr("%1 vegetation risk cluster(s)").arg(QLocale().toString(vegetationRiskResults_.size())));
    vegetationRiskStatusValueLabel_->setText(
        !pathReady
            ? tr("Measure a corridor path first, then run the analysis.")
            : vegetationRiskResults_.isEmpty()
                ? tr("Run analysis to scan points near the measured corridor and propose vegetation issues.")
                : tr("Select a cluster to focus it in the scene or convert it into inspection issues."));
    vegetationRiskSummaryLabel_->setText(
        tr("Search radius %1 m | Cluster gap %2 m | Min cluster points %3 | Classification preference %4")
            .arg(formatCoordinate(static_cast<float>(vegetationSearchRadiusMeters_)))
            .arg(formatCoordinate(static_cast<float>(vegetationClusterGapMeters_)))
            .arg(QLocale().toString(vegetationClusterPointCount_))
            .arg(preferVegetationClassification_ ? tr("on") : tr("off")));

    const QSignalBlocker blocker(vegetationRisksTableWidget_);
    vegetationRisksTableWidget_->setRowCount(0);
    for (int riskIndex = 0; riskIndex < vegetationRiskResults_.size(); ++riskIndex) {
        const VegetationRiskRecord& risk = vegetationRiskResults_.at(riskIndex);
        vegetationRisksTableWidget_->insertRow(riskIndex);

        auto createReadOnlyItem = [](const QString& text, const QColor& color = QColor(), Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags((item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            item->setTextAlignment(alignment);
            if (color.isValid()) {
                item->setForeground(color);
            }
            return item;
        };

        const QColor severityColor = analysisSeverityColor(risk.severity);
        vegetationRisksTableWidget_->setItem(riskIndex, 0, createReadOnlyItem(QLocale().toString(riskIndex + 1), QColor(), Qt::AlignCenter));
        vegetationRisksTableWidget_->setItem(riskIndex, 1, createReadOnlyItem(risk.title));
        vegetationRisksTableWidget_->setItem(riskIndex, 2, createReadOnlyItem(analysisSeverityDisplayName(risk.severity), severityColor, Qt::AlignCenter));
        vegetationRisksTableWidget_->setItem(riskIndex, 3, createReadOnlyItem(formatCoordinate(risk.minimumDistance), QColor(), Qt::AlignRight | Qt::AlignVCenter));
        vegetationRisksTableWidget_->setItem(riskIndex, 4, createReadOnlyItem(
            tr("%1 - %2 m").arg(formatCoordinate(risk.chainageStart)).arg(formatCoordinate(risk.chainageEnd)),
            QColor(),
            Qt::AlignRight | Qt::AlignVCenter));
        vegetationRisksTableWidget_->setItem(riskIndex, 5, createReadOnlyItem(risk.nearestTowerName.isEmpty() ? tr("N/A") : risk.nearestTowerName));
        vegetationRisksTableWidget_->setItem(riskIndex, 6, createReadOnlyItem(QLocale().toString(risk.supportPointCount), QColor(), Qt::AlignCenter));
    }

    if (selectedVegetationRiskIndex_ >= 0 && selectedVegetationRiskIndex_ < vegetationRisksTableWidget_->rowCount()) {
        vegetationRisksTableWidget_->setCurrentCell(selectedVegetationRiskIndex_, 1);
    } else {
        vegetationRisksTableWidget_->clearSelection();
    }
}

void MainWindow::updateRoutePlanningPanel()
{
    if (routeStatusValueLabel_ == nullptr || routeSummaryValueLabel_ == nullptr || routeWaypointsTableWidget_ == nullptr) {
        return;
    }

    routeStatusValueLabel_->setText(
        inspectionRoute_.waypoints.isEmpty()
            ? tr("No route generated. Analyze vegetation risks first, then generate inspection route.")
            : tr("%1 waypoint(s) ready for KML/KMZ interoperability.")
                .arg(QLocale().toString(inspectionRoute_.waypoints.size())));
    routeSummaryValueLabel_->setText(
        tr("%1 -> %2 | DJI profile: %3 | Safety %4 m | Speed %5 m/s | Spacing %6 m | Smoothing %7%% | Height offset %8 m")
            .arg(formatCoordinateSystemCode(projectCoordinateSystems_.pointCloudCrs, tr("Unset")))
            .arg(formatCoordinateSystemCode(projectCoordinateSystems_.geographicCrs, QStringLiteral("EPSG:4326")))
            .arg(djiAircraftProfileDisplayName(routePlanningOptions_.aircraftProfile))
            .arg(formatCoordinate(routePlanningOptions_.safety.safetyHeightMeters))
            .arg(formatCoordinate(routePlanningOptions_.safety.defaultWaypointSpeedMps))
            .arg(formatCoordinate(routePlanningOptions_.generation.waypointSpacingMeters))
            .arg(formatCoordinate(routePlanningOptions_.generation.smoothingStrengthPercent))
            .arg(formatCoordinate(routePlanningOptions_.safety.heightOffsetMeters)));

    const QSignalBlocker blocker(routeWaypointsTableWidget_);
    routeWaypointsTableWidget_->setRowCount(0);

    auto createReadOnlyItem = [](const QString& text, Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
        auto* item = new QTableWidgetItem(text);
        item->setFlags((item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        item->setTextAlignment(alignment);
        return item;
    };

    for (int waypointIndex = 0; waypointIndex < inspectionRoute_.waypoints.size(); ++waypointIndex) {
        const InspectionWaypoint& waypoint = inspectionRoute_.waypoints.at(waypointIndex);
        routeWaypointsTableWidget_->insertRow(waypointIndex);
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            0,
            createReadOnlyItem(QLocale().toString(waypointIndex + 1), Qt::AlignCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            1,
            createReadOnlyItem(formatCoordinate(waypoint.localPoint.x), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            2,
            createReadOnlyItem(formatCoordinate(waypoint.localPoint.y), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            3,
            createReadOnlyItem(formatCoordinate(waypoint.localPoint.z), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            4,
            createReadOnlyItem(formatCoordinate(waypoint.speedMps), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            5,
            createReadOnlyItem(formatCoordinate(waypoint.chainage), Qt::AlignRight | Qt::AlignVCenter));
    }

    if (selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < routeWaypointsTableWidget_->rowCount()) {
        routeWaypointsTableWidget_->setCurrentCell(selectedRouteWaypointIndex_, 1);
    } else {
        routeWaypointsTableWidget_->clearSelection();
    }
}

void MainWindow::rebuildProjectTree()
{
    if (projectTreeWidget_ == nullptr || viewer_ == nullptr) {
        return;
    }

    syncDataManagerTrajectory();
    const DataManager& dataManager = DataManager::instance();

    const QTreeWidgetItem* previousItem = projectTreeWidget_->currentItem();
    const QString previousItemType = projectTreeItemType(previousItem);
    const QString previousFilePath = projectTreeItemFilePath(previousItem);
    const int previousIssueIndex = previousItem != nullptr ? previousItem->data(0, kProjectTreeIssueIndexRole).toInt() : -1;
    const QSignalBlocker blocker(projectTreeWidget_);
    updatingProjectTree_ = true;
    projectTreeWidget_->clear();

    const QString projectName = currentProjectFilePath_.trimmed().isEmpty()
        ? tr("Unsaved Project")
        : QFileInfo(currentProjectFilePath_).completeBaseName();
    auto* coordinateSystemsItem = new QTreeWidgetItem(projectTreeWidget_, QStringList { tr("Project Properties") });
    coordinateSystemsItem->setData(0, kProjectTreeItemTypeRole, QStringLiteral("coordinateSystemsItem"));
    coordinateSystemsItem->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogContentsView));
    coordinateSystemsItem->setToolTip(
        0,
        tr("Project: %1\nCurrent project CRS: %2")
            .arg(projectName, formatProjectCoordinateSystemsSummary(projectCoordinateSystems_)));

    auto* pointCloudGroup = new QTreeWidgetItem(projectTreeWidget_, QStringList {
        tr("Point Clouds (%1)").arg(QLocale().toString(dataManager.pointCloudDatasets().size()))
    });
    pointCloudGroup->setData(0, kProjectTreeItemTypeRole, QStringLiteral("pointCloudGroup"));
    pointCloudGroup->setIcon(0, style()->standardIcon(QStyle::SP_DriveHDIcon));

    auto* imageGroup = new QTreeWidgetItem(projectTreeWidget_, QStringList {
        tr("Images (%1)").arg(QLocale().toString(dataManager.imageItems().size()))
    });
    imageGroup->setData(0, kProjectTreeItemTypeRole, QStringLiteral("imageGroup"));
    imageGroup->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));

    auto* trajectoryGroup = new QTreeWidgetItem(projectTreeWidget_, QStringList {
        tr("Trajectories (%1)").arg(QLocale().toString(dataManager.hasTrajectory() ? 1 : 0))
    });
    trajectoryGroup->setData(0, kProjectTreeItemTypeRole, QStringLiteral("trajectoryGroup"));
    trajectoryGroup->setIcon(0, style()->standardIcon(QStyle::SP_ArrowRight));

    QTreeWidgetItem* selectedItem = nullptr;
    if (previousItemType == QStringLiteral("coordinateSystemsItem")) {
        selectedItem = coordinateSystemsItem;
    } else if (previousItemType == QStringLiteral("pointCloudGroup")) {
        selectedItem = pointCloudGroup;
    } else if (previousItemType == QStringLiteral("imageGroup")) {
        selectedItem = imageGroup;
    } else if (previousItemType == QStringLiteral("trajectoryGroup")) {
        selectedItem = trajectoryGroup;
    }

    for (const PointCloudDatasetInfo& datasetInfo : dataManager.pointCloudDatasets()) {
        const QFileInfo fileInfo(datasetInfo.filePath);
        const QString absoluteFilePath = fileInfo.absoluteFilePath();
        auto* datasetItem = new QTreeWidgetItem(pointCloudGroup, QStringList {
            fileInfo.fileName().isEmpty() ? absoluteFilePath : fileInfo.fileName()
        });
        datasetItem->setData(0, kProjectTreeItemTypeRole, QStringLiteral("pointCloudItem"));
        datasetItem->setData(0, kProjectTreeFilePathRole, absoluteFilePath);
        datasetItem->setToolTip(0, absoluteFilePath);
        datasetItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        datasetItem->setFlags(datasetItem->flags() | Qt::ItemIsUserCheckable);
        datasetItem->setCheckState(0, datasetInfo.visible ? Qt::Checked : Qt::Unchecked);

        if (previousItemType == QStringLiteral("pointCloudItem")
            && previousFilePath.compare(absoluteFilePath, Qt::CaseInsensitive) == 0) {
            selectedItem = datasetItem;
        }
    }

    for (const DataImageItem& imageInfoRecord : dataManager.imageItems()) {
        const QFileInfo imageInfo(imageInfoRecord.filePath);
        const QString absoluteImagePath = imageInfo.absoluteFilePath();
        QString itemText = imageInfo.fileName();
        if (itemText.isEmpty()) {
            itemText = imageInfoRecord.title.trimmed().isEmpty()
                ? tr("Image %1").arg(QLocale().toString(imageInfoRecord.issueIndex + 1))
                : imageInfoRecord.title;
        }

        auto* imageItem = new QTreeWidgetItem(imageGroup, QStringList { itemText });
        imageItem->setData(0, kProjectTreeItemTypeRole, QStringLiteral("imageItem"));
        imageItem->setData(0, kProjectTreeFilePathRole, absoluteImagePath);
        imageItem->setData(0, kProjectTreeIssueIndexRole, imageInfoRecord.issueIndex);
        imageItem->setToolTip(0, tr("%1\n%2").arg(imageInfoRecord.title, absoluteImagePath));
        imageItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        imageItem->setFlags(imageItem->flags() | Qt::ItemIsUserCheckable);
        imageItem->setCheckState(0, imageInfoRecord.visible ? Qt::Checked : Qt::Unchecked);

        if (previousItemType == QStringLiteral("imageItem")
            && previousIssueIndex == imageInfoRecord.issueIndex
            && previousFilePath.compare(absoluteImagePath, Qt::CaseInsensitive) == 0) {
            selectedItem = imageItem;
        }
    }

    if (dataManager.hasTrajectory()) {
        const DataTrajectoryItem& trajectoryItem = dataManager.trajectoryItem();
        const QString routeName = trajectoryItem.name.trimmed().isEmpty()
            ? tr("Inspection Route")
            : trajectoryItem.name.trimmed();
        auto* routeItem = new QTreeWidgetItem(trajectoryGroup, QStringList {
            tr("%1 (%2 WP)").arg(routeName, QLocale().toString(trajectoryItem.points.size()))
        });
        routeItem->setData(0, kProjectTreeItemTypeRole, QStringLiteral("trajectoryItem"));
        routeItem->setToolTip(0, tr("%1 waypoint(s)").arg(QLocale().toString(trajectoryItem.points.size())));
        routeItem->setIcon(0, style()->standardIcon(QStyle::SP_ArrowForward));
        routeItem->setFlags(routeItem->flags() | Qt::ItemIsUserCheckable);
        routeItem->setCheckState(0, trajectoryItem.visible ? Qt::Checked : Qt::Unchecked);

        if (previousItemType == QStringLiteral("trajectoryItem")) {
            selectedItem = routeItem;
        }
    }

    pointCloudGroup->setExpanded(true);
    imageGroup->setExpanded(true);
    trajectoryGroup->setExpanded(true);

    if (selectedItem == nullptr) {
        QTreeWidgetItemIterator it(projectTreeWidget_);
        while (*it != nullptr) {
            const QString itemType = (*it)->data(0, kProjectTreeItemTypeRole).toString();
            if (itemType == QStringLiteral("coordinateSystemsItem")
                || itemType == QStringLiteral("pointCloudItem")
                || itemType == QStringLiteral("imageItem")
                || itemType == QStringLiteral("trajectoryItem")) {
                selectedItem = *it;
                break;
            }
            ++it;
        }
    }

    projectTreeWidget_->setCurrentItem(selectedItem != nullptr ? selectedItem : coordinateSystemsItem);
    updatingProjectTree_ = false;
    refreshProjectTreeFilter();
}

void MainWindow::refreshProjectTreeFilter()
{
    if (projectTreeWidget_ == nullptr) {
        return;
    }

    const QString filterText = projectSearchEdit_ != nullptr ? projectSearchEdit_->text().trimmed() : QString();
    const auto updateItemVisibility = [&filterText](auto&& self, QTreeWidgetItem* item) -> bool {
        if (item == nullptr) {
            return false;
        }

        bool hasVisibleChild = false;
        for (int childIndex = 0; childIndex < item->childCount(); ++childIndex) {
            hasVisibleChild = self(self, item->child(childIndex)) || hasVisibleChild;
        }

        const QString itemText = item->text(0) + QLatin1Char('\n') + item->toolTip(0);
        const bool matchesSelf =
            filterText.isEmpty()
            || itemText.contains(filterText, Qt::CaseInsensitive);
        const QString itemType = item->data(0, kProjectTreeItemTypeRole).toString();
        const bool forceVisible = itemType == QStringLiteral("pointCloudGroup")
            || itemType == QStringLiteral("imageGroup")
            || itemType == QStringLiteral("trajectoryGroup");
        const bool visible = forceVisible || matchesSelf || hasVisibleChild;
        item->setHidden(!visible);
        if (!filterText.isEmpty() && visible && item->childCount() > 0) {
            item->setExpanded(true);
        }
        return visible;
    };

    for (int rootIndex = 0; rootIndex < projectTreeWidget_->topLevelItemCount(); ++rootIndex) {
        updateItemVisibility(updateItemVisibility, projectTreeWidget_->topLevelItem(rootIndex));
    }

    if (QTreeWidgetItem* currentItem = projectTreeWidget_->currentItem();
        currentItem != nullptr && currentItem->isHidden()) {
        projectTreeWidget_->setCurrentItem(nullptr);
    }

    updateActionState();
}

QString MainWindow::selectedDatasetPath() const
{
    if (projectTreeWidget_ == nullptr || projectTreeWidget_->currentItem() == nullptr) {
        return QString();
    }

    const QTreeWidgetItem* currentItem = projectTreeWidget_->currentItem();
    if (currentItem->data(0, kProjectTreeItemTypeRole).toString() != QStringLiteral("pointCloudItem")) {
        return QString();
    }

    return currentItem->data(0, kProjectTreeFilePathRole).toString();
}

void MainWindow::focusProjectTreeItem(QTreeWidgetItem* item)
{
    if (viewer_ == nullptr || item == nullptr) {
        return;
    }

    const QString itemType = projectTreeItemType(item);
    if (itemType == QStringLiteral("coordinateSystemsItem")) {
        openProjectCoordinateSystems();
        return;
    }

    if (itemType == QStringLiteral("pointCloudItem")) {
        const QString filePath = projectTreeItemFilePath(item);
        for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
            if (datasetInfo.filePath.compare(filePath, Qt::CaseInsensitive) == 0) {
                viewer_->focusOnBounds(datasetInfo.minBounds, datasetInfo.maxBounds, 0.9);
                return;
            }
        }
        return;
    }

    if (itemType == QStringLiteral("imageItem")) {
        const int issueIndex = item->data(0, kProjectTreeIssueIndexRole).toInt();
        for (const DataImageItem& imageItem : DataManager::instance().imageItems()) {
            if (imageItem.issueIndex != issueIndex) {
                continue;
            }
            viewer_->setSelectedIssueIndex(issueIndex);
            viewer_->focusOnPoint(imageItem.point, 0.2);
            updateIssuePanel();
            break;
        }
        return;
    }

    if (itemType == QStringLiteral("trajectoryItem")) {
        PointRecord minBounds;
        PointRecord maxBounds;
        QList<PointRecord> routePoints;
        routePoints.reserve(inspectionRoute_.waypoints.size());
        for (const InspectionWaypoint& waypoint : inspectionRoute_.waypoints) {
            routePoints.append(waypoint.localPoint);
        }
        if (boundsFromPoints(routePoints, &minBounds, &maxBounds)) {
            viewer_->focusOnBounds(minBounds, maxBounds, 1.15);
        }
    }
}

void MainWindow::applyProjectTreeItemCheckState(QTreeWidgetItem* item)
{
    if (updatingProjectTree_ || viewer_ == nullptr || item == nullptr) {
        return;
    }

    const QString itemType = projectTreeItemType(item);
    const bool visible = item->checkState(0) != Qt::Unchecked;
    if (itemType == QStringLiteral("pointCloudItem")) {
        viewer_->setPointCloudDatasetVisible(projectTreeItemFilePath(item), visible);
        updateDatasetPanel();
        updateActionState();
        return;
    }

    if (itemType == QStringLiteral("imageItem")) {
        viewer_->setInspectionIssueVisible(item->data(0, kProjectTreeIssueIndexRole).toInt(), visible);
        updateIssuePanel();
        updateActionState();
        return;
    }

    if (itemType == QStringLiteral("trajectoryItem")) {
        viewer_->setInspectionRouteVisible(visible);
        updateRoutePlanningPanel();
        updateActionState();
    }
}

void MainWindow::setProjectTreeGroupVisibility(const QString& groupType, bool visible)
{
    if (viewer_ == nullptr) {
        return;
    }

    if (groupType == QStringLiteral("pointCloudGroup")) {
        for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
            viewer_->setPointCloudDatasetVisible(datasetInfo.filePath, visible);
        }
        updateDatasetPanel();
    } else if (groupType == QStringLiteral("imageGroup")) {
        for (const DataImageItem& imageItem : DataManager::instance().imageItems()) {
            viewer_->setInspectionIssueVisible(imageItem.issueIndex, visible);
        }
        updateIssuePanel();
    } else if (groupType == QStringLiteral("trajectoryGroup")) {
        viewer_->setInspectionRouteVisible(visible);
        updateRoutePlanningPanel();
    }

    rebuildProjectTree();
    updateActionState();
}

void MainWindow::clearAllProjectImages()
{
    if (viewer_ == nullptr) {
        return;
    }

    QList<InspectionIssue> issues = viewer_->inspectionIssues();
    if (issues.isEmpty()) {
        return;
    }

    bool changed = false;
    for (InspectionIssue& issue : issues) {
        if (issue.imagePath.trimmed().isEmpty()) {
            continue;
        }

        issue.imagePath.clear();
        changed = true;
    }

    if (!changed) {
        return;
    }

    const int selectedIssueIndex = viewer_->selectedIssueIndex();
    viewer_->setInspectionIssues(issues);
    viewer_->setSelectedIssueIndex(selectedIssueIndex);
    updateIssuePanel();
    rebuildProjectTree();
    updateActionState();
    showUserMessage(LogLevel::Info, tr("All image attachments were removed from the project."), 3000);
}

bool MainWindow::attachImageToIssue(int issueIndex)
{
    if (viewer_ == nullptr) {
        return false;
    }

    const QList<InspectionIssue> issues = viewer_->inspectionIssues();
    if (issueIndex < 0 || issueIndex >= issues.size()) {
        showUserMessage(LogLevel::Warning, tr("Select an inspection issue before attaching an image."), 3000);
        return false;
    }

    const InspectionIssue& selectedIssue = issues.at(issueIndex);
    const QString initialPath = selectedIssue.imagePath.trimmed();
    const QString filePath = showStyledOpenFileNameDialog(
        this,
        tr("Attach Image"),
        initialPath.isEmpty() ? QDir::homePath() : QFileInfo(initialPath).absolutePath(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;All Files (*.*)"));
    if (filePath.isEmpty()) {
        return false;
    }

    InspectionIssue updatedIssue = selectedIssue;
    updatedIssue.imagePath = QFileInfo(filePath).absoluteFilePath();
    if (!viewer_->updateInspectionIssue(issueIndex, updatedIssue)) {
        showUserMessage(LogLevel::Warning, tr("Unable to attach the selected image."), 3000);
        return false;
    }

    viewer_->setSelectedIssueIndex(issueIndex);
    updateIssuePanel();
    rebuildProjectTree();
    updateActionState();
    showUserMessage(LogLevel::Info, tr("Image attached to the selected inspection issue."), 3000);
    return true;
}

void MainWindow::showProjectTreeContextMenu(const QPoint& pos)
{
    if (projectTreeWidget_ == nullptr) {
        return;
    }

    if (QTreeWidgetItem* item = projectTreeWidget_->itemAt(pos)) {
        projectTreeWidget_->setCurrentItem(item);
    }
    updateActionState();

    QTreeWidgetItem* currentItem = projectTreeWidget_->currentItem();
    const QString itemType = projectTreeItemType(currentItem);
    QMenu menu(this);
    menu.setAttribute(Qt::WA_TranslucentBackground, false);
    menu.setWindowOpacity(1.0);
    menu.setStyleSheet(QStringLiteral(
        "QMenu {"
        "background-color: #f8fbff;"
        "border: 1px solid #cfd9e6;"
        "border-radius: 10px;"
        "padding: 6px;"
        "color: #0f172a;"
        "}"
        "QMenu::item {"
        "padding: 8px 16px 8px 12px;"
        "border-radius: 7px;"
        "background-color: transparent;"
        "}"
        "QMenu::item:selected {"
        "background-color: #dbeafe;"
        "color: #0f172a;"
        "}"
        "QMenu::separator {"
        "height: 1px;"
        "margin: 6px 8px;"
        "background: #d7e1ee;"
        "}"));
    const QPoint globalPos = projectTreeWidget_->viewport()->mapToGlobal(pos);

    if (itemType == QStringLiteral("pointCloudItem")) {
        QAction* detailsAction = menu.addAction(tr("Point Cloud Details"));
        QAction* focusAction = menu.addAction(tr("Focus in View"));
        menu.addSeparator();
        QAction* openFolderAction = menu.addAction(tr("Open Folder"));
        QAction* copyPathAction = menu.addAction(tr("Copy Path"));
        menu.addSeparator();
        QAction* removeAction = menu.addAction(tr("Remove Selected Dataset"));

        QAction* chosenAction = menu.exec(globalPos);
        const QString filePath = projectTreeItemFilePath(currentItem);
        if (chosenAction == focusAction) {
            focusProjectTreeItem(currentItem);
        } else if (chosenAction == detailsAction) {
            showPointCloudDatasetDetails(filePath);
        } else if (chosenAction == openFolderAction) {
            const QString folderPath = QFileInfo(filePath).absolutePath();
            if (!folderPath.isEmpty() && !QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath))) {
                showUserMessage(LogLevel::Warning, tr("Unable to open the selected file folder."), 3000);
            }
        } else if (chosenAction == copyPathAction) {
            if (!filePath.isEmpty() && QGuiApplication::clipboard() != nullptr) {
                QGuiApplication::clipboard()->setText(filePath);
                showUserMessage(LogLevel::Info, tr("Selected path copied."), 2000);
            }
        } else if (chosenAction == removeAction) {
            removeSelectedDataset();
        }
        return;
    }

    if (itemType == QStringLiteral("imageItem")) {
        QAction* focusAction = menu.addAction(tr("Focus in View"));
        QAction* openImageAction = menu.addAction(tr("Open Image"));
        QAction* attachImageAction = menu.addAction(tr("Replace Image"));
        menu.addSeparator();
        QAction* openFolderAction = menu.addAction(tr("Open Folder"));
        QAction* copyPathAction = menu.addAction(tr("Copy Path"));
        QAction* removeImageAction = menu.addAction(tr("Remove Image"));

        QAction* chosenAction = menu.exec(globalPos);
        const QString imagePath = projectTreeItemFilePath(currentItem);
        const int issueIndex = currentItem != nullptr ? currentItem->data(0, kProjectTreeIssueIndexRole).toInt() : -1;
        if (chosenAction == focusAction) {
            focusProjectTreeItem(currentItem);
        } else if (chosenAction == openImageAction) {
            if (!imagePath.isEmpty() && !QDesktopServices::openUrl(QUrl::fromLocalFile(imagePath))) {
                showUserMessage(LogLevel::Warning, tr("Unable to open the image file."), 3000);
            }
        } else if (chosenAction == attachImageAction) {
            attachImageToIssue(issueIndex);
        } else if (chosenAction == openFolderAction) {
            const QString folderPath = QFileInfo(imagePath).absolutePath();
            if (!folderPath.isEmpty() && !QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath))) {
                showUserMessage(LogLevel::Warning, tr("Unable to open the selected file folder."), 3000);
            }
        } else if (chosenAction == copyPathAction) {
            if (!imagePath.isEmpty() && QGuiApplication::clipboard() != nullptr) {
                QGuiApplication::clipboard()->setText(imagePath);
                showUserMessage(LogLevel::Info, tr("Selected path copied."), 2000);
            }
        } else if (chosenAction == removeImageAction) {
            QList<InspectionIssue> issues = viewer_->inspectionIssues();
            if (issueIndex >= 0 && issueIndex < issues.size()) {
                issues[issueIndex].imagePath.clear();
                viewer_->setInspectionIssues(issues);
                viewer_->setSelectedIssueIndex(issueIndex);
                updateIssuePanel();
                rebuildProjectTree();
                updateActionState();
                showUserMessage(LogLevel::Info, tr("Image attachment removed."), 2500);
            }
        }
        return;
    }

    if (itemType == QStringLiteral("trajectoryItem")) {
        QAction* focusAction = menu.addAction(tr("Focus in View"));
        QAction* detailsAction = menu.addAction(tr("Trajectory Details"));
        menu.addSeparator();
        QAction* hideAction = menu.addAction(tr("Hide"));
        QAction* clearAction = menu.addAction(tr("Remove Trajectory"));

        QAction* chosenAction = menu.exec(globalPos);
        if (chosenAction == focusAction) {
            focusProjectTreeItem(currentItem);
        } else if (chosenAction == detailsAction) {
            showInspectionRouteDetails();
        } else if (chosenAction == hideAction) {
            viewer_->setInspectionRouteVisible(false);
            updateRoutePlanningPanel();
            rebuildProjectTree();
            updateActionState();
        } else if (chosenAction == clearAction) {
            inspectionRoute_ = InspectionRoute();
            selectedRouteWaypointIndex_ = -1;
            viewer_->clearInspectionRouteWaypoints();
            updateRoutePlanningPanel();
            rebuildProjectTree();
            updateActionState();
        }
        return;
    }

    if (itemType == QStringLiteral("pointCloudGroup")) {
        QAction* addAction = menu.addAction(tr("Add LAS/LAZ Files"));
        menu.addSeparator();
        QAction* showAllAction = menu.addAction(tr("Show All"));
        QAction* hideAllAction = menu.addAction(tr("Hide All"));
        menu.addSeparator();
        QAction* removeAllAction = menu.addAction(tr("Remove All Point Clouds"));
        removeAllAction->setEnabled(viewer_ != nullptr && viewer_->hasLoadedPointClouds());

        QAction* chosenAction = menu.exec(globalPos);
        if (chosenAction == addAction) {
            addPointCloudFiles();
        } else if (chosenAction == showAllAction) {
            setProjectTreeGroupVisibility(itemType, true);
        } else if (chosenAction == hideAllAction) {
            setProjectTreeGroupVisibility(itemType, false);
        } else if (chosenAction == removeAllAction) {
            clearPointCloud();
        }
        return;
    }

    if (itemType == QStringLiteral("imageGroup")) {
        QAction* markIssueAction = menu.addAction(tr("Mark Issue"));
        QAction* attachImageAction = menu.addAction(tr("Attach Image To Selected Issue"));
        attachImageAction->setEnabled(viewer_ != nullptr && viewer_->selectedIssueIndex() >= 0);
        menu.addSeparator();
        QAction* showAllAction = menu.addAction(tr("Show All"));
        QAction* hideAllAction = menu.addAction(tr("Hide All"));
        menu.addSeparator();
        QAction* removeAllAction = menu.addAction(tr("Remove All Images"));
        removeAllAction->setEnabled(!DataManager::instance().imageItems().isEmpty());

        QAction* chosenAction = menu.exec(globalPos);
        if (chosenAction == markIssueAction) {
            startIssueMarkAction_->trigger();
        } else if (chosenAction == attachImageAction) {
            attachImageToIssue(viewer_ != nullptr ? viewer_->selectedIssueIndex() : -1);
        } else if (chosenAction == showAllAction) {
            setProjectTreeGroupVisibility(itemType, true);
        } else if (chosenAction == hideAllAction) {
            setProjectTreeGroupVisibility(itemType, false);
        } else if (chosenAction == removeAllAction) {
            clearAllProjectImages();
        }
        return;
    }

    if (itemType == QStringLiteral("trajectoryGroup")) {
        QAction* importAction = menu.addAction(tr("Import Route KML"));
        menu.addSeparator();
        QAction* showAllAction = menu.addAction(tr("Show All"));
        QAction* hideAllAction = menu.addAction(tr("Hide All"));
        showAllAction->setEnabled(DataManager::instance().hasTrajectory());
        hideAllAction->setEnabled(DataManager::instance().hasTrajectory());
        menu.addSeparator();
        QAction* removeAction = menu.addAction(tr("Remove Trajectory"));
        removeAction->setEnabled(DataManager::instance().hasTrajectory());

        QAction* chosenAction = menu.exec(globalPos);
        if (chosenAction == importAction) {
            importRouteKmlAction_->trigger();
        } else if (chosenAction == showAllAction) {
            setProjectTreeGroupVisibility(itemType, true);
        } else if (chosenAction == hideAllAction) {
            setProjectTreeGroupVisibility(itemType, false);
        } else if (chosenAction == removeAction) {
            inspectionRoute_ = InspectionRoute();
            selectedRouteWaypointIndex_ = -1;
            viewer_->clearInspectionRouteWaypoints();
            updateRoutePlanningPanel();
            rebuildProjectTree();
            updateActionState();
        }
        return;
    }

    if (itemType == QStringLiteral("projectGroup") || itemType == QStringLiteral("coordinateSystemsItem")) {
        QAction* propertiesAction = menu.addAction(tr("Project Properties"));
        QAction* chosenAction = menu.exec(globalPos);
        if (chosenAction == propertiesAction) {
            openProjectCoordinateSystems();
        }
        return;
    }

    menu.addAction(openAction_);
    menu.addAction(addPointCloudAction_);
    menu.addSeparator();
    menu.addAction(expandProjectTreeAction_);
    menu.addAction(collapseProjectTreeAction_);
    menu.exec(globalPos);
}

void MainWindow::showPointCloudDatasetDetails(const QString& filePath)
{
    if (viewer_ == nullptr || filePath.trimmed().isEmpty()) {
        return;
    }

    for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
        if (datasetInfo.filePath.compare(filePath, Qt::CaseInsensitive) != 0) {
            continue;
        }

        const QFileInfo fileInfo(datasetInfo.filePath);
        const QString extentText = formatTriplet(
            datasetInfo.maxBounds.x - datasetInfo.minBounds.x,
            datasetInfo.maxBounds.y - datasetInfo.minBounds.y,
            datasetInfo.maxBounds.z - datasetInfo.minBounds.z);
        const QString subtitle = tr("Review the active dataset metadata, spatial bounds, and attribute availability before further analysis.");
        const QList<QPair<QString, QString>> statRows = {
            qMakePair(tr("Point Count"), QLocale().toString(static_cast<qlonglong>(datasetInfo.pointCount))),
            qMakePair(tr("Extent"), extentText),
            qMakePair(tr("Projection"), datasetInfo.projectionText.trimmed().isEmpty() ? tr("Unknown") : datasetInfo.projectionText),
            qMakePair(tr("Native RGB"), datasetInfo.hasColor ? tr("Available") : tr("Unavailable"))
        };
        const QList<QPair<QString, QString>> detailRows = {
            qMakePair(tr("Dataset Name"), fileInfo.fileName().isEmpty() ? datasetInfo.filePath : fileInfo.fileName()),
            qMakePair(tr("Full Path"), QDir::toNativeSeparators(datasetInfo.filePath)),
            qMakePair(tr("File Size"), fileInfo.exists() ? QLocale().formattedDataSize(fileInfo.size()) : tr("N/A")),
            qMakePair(tr("Visibility"), datasetInfo.visible ? tr("Visible") : tr("Hidden")),
            qMakePair(tr("Min Bounds"), formatTriplet(datasetInfo.minBounds.x, datasetInfo.minBounds.y, datasetInfo.minBounds.z)),
            qMakePair(tr("Max Bounds"), formatTriplet(datasetInfo.maxBounds.x, datasetInfo.maxBounds.y, datasetInfo.maxBounds.z)),
            qMakePair(tr("Extent"), extentText),
            qMakePair(tr("Projection Text"), datasetInfo.projectionText.trimmed().isEmpty() ? tr("Unknown") : datasetInfo.projectionText),
            qMakePair(tr("RGB Attribute"), datasetInfo.hasColor ? tr("Available") : tr("Unavailable")),
            qMakePair(tr("Intensity"), datasetInfo.hasIntensity ? tr("Available") : tr("Unavailable")),
            qMakePair(tr("Classification"), datasetInfo.hasClassification ? tr("Available") : tr("Unavailable")),
            qMakePair(tr("Return Info"), datasetInfo.hasReturnInfo ? tr("Available") : tr("Unavailable")),
            qMakePair(tr("GPS Time"), datasetInfo.hasGpsTime ? tr("Available") : tr("Unavailable"))
        };
        showStyledDetailsDialog(this, tr("Point Cloud Details"), subtitle, detailRows, statRows);
        return;
    }
}

void MainWindow::showInspectionRouteDetails() const
{
    const DataTrajectoryItem& trajectoryItem = DataManager::instance().trajectoryItem();
    const QList<PointRecord>& routePoints = trajectoryItem.points;

    PointRecord minBounds;
    PointRecord maxBounds;
    const bool hasBounds = boundsFromPoints(routePoints, &minBounds, &maxBounds);
    const QString routeName = trajectoryItem.name.trimmed().isEmpty()
        ? tr("Inspection Route")
        : trajectoryItem.name.trimmed();
    const QString extentText = hasBounds
        ? formatTriplet(maxBounds.x - minBounds.x, maxBounds.y - minBounds.y, maxBounds.z - minBounds.z)
        : tr("N/A");
    const QList<QPair<QString, QString>> statRows = {
        qMakePair(tr("Waypoints"), QLocale().toString(routePoints.size())),
        qMakePair(tr("Visibility"), trajectoryItem.visible ? tr("Visible") : tr("Hidden")),
        qMakePair(tr("Extent"), extentText)
    };
    const QList<QPair<QString, QString>> detailRows = {
        qMakePair(tr("Route Name"), routeName),
        qMakePair(tr("Waypoints"), QLocale().toString(routePoints.size())),
        qMakePair(tr("Min Bounds"), hasBounds ? formatTriplet(minBounds.x, minBounds.y, minBounds.z) : tr("N/A")),
        qMakePair(tr("Max Bounds"), hasBounds ? formatTriplet(maxBounds.x, maxBounds.y, maxBounds.z) : tr("N/A")),
        qMakePair(tr("Extent"), extentText)
    };
    showStyledDetailsDialog(
        const_cast<MainWindow*>(this),
        tr("Trajectory Details"),
        tr("Inspect route bounds, waypoint count, and visibility before exporting or editing."),
        detailRows,
        statRows);
}

void MainWindow::syncDataManagerTrajectory() const
{
    QList<PointRecord> routePoints;
    routePoints.reserve(inspectionRoute_.waypoints.size());
    for (const InspectionWaypoint& waypoint : inspectionRoute_.waypoints) {
        routePoints.append(waypoint.localPoint);
    }

    DataManager::instance().setTrajectory(
        inspectionRoute_.name.trimmed().isEmpty() ? tr("Inspection Route") : inspectionRoute_.name.trimmed(),
        routePoints,
        viewer_ != nullptr ? viewer_->inspectionRouteVisible() : true);
}

void MainWindow::updateTowerPanel()
{
    if (viewer_ == nullptr || towerTableWidget_ == nullptr || towerCountValueLabel_ == nullptr || towerToolStatusLabel_ == nullptr) {
        return;
    }

    const QList<TowerMarker>& towerMarkers = viewer_->towerMarkers();
    towerCountValueLabel_->setText(
        towerMarkers.isEmpty()
            ? tr("No tower markers yet. Use the icon tools above to add one from the point cloud.")
            : tr("%1 tower marker(s)").arg(QLocale().toString(towerMarkers.size())));

    switch (viewer_->towerEditMode()) {
    case TowerEditMode::AddAfterLast:
        towerToolStatusLabel_->setText(tr("Tower tool: click points in the view continuously to add tower markers. Cancel the tool when finished."));
        break;
    case TowerEditMode::InsertBeforeSelected:
        towerToolStatusLabel_->setText(
            tr("Tower tool: click a point in the view to insert a tower marker before current tower #%1.")
                .arg(QLocale().toString(viewer_->towerEditTargetIndex() + 1)));
        break;
    case TowerEditMode::MoveSelected:
        towerToolStatusLabel_->setText(
            tr("Tower tool: click a point in the view to move current tower #%1.")
                .arg(QLocale().toString(viewer_->towerEditTargetIndex() + 1)));
        break;
    case TowerEditMode::None:
    default:
        towerToolStatusLabel_->setText(
            towerEditingEnabled_
                ? tr("Tower editing active. Select the current tower, then use the toolbar above to insert, move, rename, focus, or remove it.")
                : tr("Tower editing is off. Use the Ribbon to start editing before changing tower markers."));
        break;
    }

    const int selectedRow = viewer_->selectedTowerIndex();
    const QSignalBlocker blocker(towerTableWidget_);
    towerTableWidget_->setEditTriggers(static_cast<QAbstractItemView::EditTriggers>(
        towerEditingEnabled_
            ? (QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed)
            : QAbstractItemView::NoEditTriggers));
    towerTableWidget_->setRowCount(0);
    for (int towerIndex = 0; towerIndex < towerMarkers.size(); ++towerIndex) {
        const TowerMarker& towerMarker = towerMarkers.at(towerIndex);
        towerTableWidget_->insertRow(towerIndex);

        auto* indexItem = new QTableWidgetItem(QLocale().toString(towerIndex + 1));
        indexItem->setFlags((indexItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        indexItem->setTextAlignment(Qt::AlignCenter);
        towerTableWidget_->setItem(towerIndex, 0, indexItem);

        auto* nameItem = new QTableWidgetItem(towerMarker.name);
        if (towerEditingEnabled_) {
            nameItem->setFlags(nameItem->flags() | Qt::ItemIsEditable);
        } else {
            nameItem->setFlags((nameItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        }
        towerTableWidget_->setItem(towerIndex, 1, nameItem);

        const QStringList coordinateTexts = {
            formatCoordinate(towerMarker.point.x),
            formatCoordinate(towerMarker.point.y),
            formatCoordinate(towerMarker.point.z)
        };
        for (int coordinateColumn = 0; coordinateColumn < coordinateTexts.size(); ++coordinateColumn) {
            auto* coordinateItem = new QTableWidgetItem(coordinateTexts.at(coordinateColumn));
            coordinateItem->setFlags((coordinateItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            coordinateItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            towerTableWidget_->setItem(towerIndex, coordinateColumn + 2, coordinateItem);
        }
    }

    if (towerTableWidget_->rowCount() > 0) {
        towerTableWidget_->setCurrentCell(std::clamp(selectedRow, 0, towerTableWidget_->rowCount() - 1), 1);
    } else {
        towerTableWidget_->clearSelection();
    }

    updateTowerDetailEditor();
}

QString MainWindow::nextDefaultTowerName() const
{
    if (viewer_ == nullptr) {
        return QString();
    }

    std::set<QString> existingNames;
    for (const TowerMarker& towerMarker : viewer_->towerMarkers()) {
        existingNames.insert(towerMarker.name.trimmed());
    }

    int towerIndex = 1;
    while (true) {
        const QString candidate = tr("Tower %1").arg(QLocale().toString(towerIndex));
        if (existingNames.find(candidate) == existingNames.end()) {
            return candidate;
        }
        ++towerIndex;
    }
}

QString MainWindow::nextDefaultIssueTitle() const
{
    if (viewer_ == nullptr) {
        return QString();
    }

    std::set<QString> existingTitles;
    for (const InspectionIssue& issue : viewer_->inspectionIssues()) {
        existingTitles.insert(issue.title.trimmed());
    }

    int issueIndex = 1;
    while (true) {
        const QString candidate = tr("Issue %1").arg(QLocale().toString(issueIndex));
        if (existingTitles.find(candidate) == existingTitles.end()) {
            return candidate;
        }
        ++issueIndex;
    }
}

void MainWindow::updateTowerDetailEditor()
{
    if (viewer_ == nullptr || towerCodeEdit_ == nullptr) {
        return;
    }

    updatingTowerDetails_ = true;
    const QList<TowerRecord>& towers = viewer_->towerMarkers();
    const int selectedTowerIndex = viewer_->selectedTowerIndex();
    const bool hasSelection = selectedTowerIndex >= 0 && selectedTowerIndex < towers.size();

    const TowerRecord towerRecord = hasSelection ? towers.at(selectedTowerIndex) : TowerRecord();
    towerCodeEdit_->setText(towerRecord.code);
    towerLineNameEdit_->setText(towerRecord.lineName);
    towerVoltageLevelEdit_->setText(towerRecord.voltageLevel);
    towerStructureTypeEdit_->setText(towerRecord.structureType);
    towerInspectionDateEdit_->setText(towerRecord.inspectionDate);
    towerStatusEdit_->setText(towerRecord.status);
    towerNotesEdit_->setPlainText(towerRecord.notes);

    const QList<QWidget*> editors = {
        towerCodeEdit_,
        towerLineNameEdit_,
        towerVoltageLevelEdit_,
        towerStructureTypeEdit_,
        towerInspectionDateEdit_,
        towerStatusEdit_,
        towerNotesEdit_
    };
    for (QWidget* editor : editors) {
        if (editor != nullptr) {
            editor->setEnabled(hasSelection && towerEditingEnabled_);
        }
    }
    updatingTowerDetails_ = false;
}

void MainWindow::updateIssuePanel()
{
    if (viewer_ == nullptr || issueTableWidget_ == nullptr || issueCountValueLabel_ == nullptr || issueToolStatusLabel_ == nullptr) {
        return;
    }

    const QList<InspectionIssue>& issues = viewer_->inspectionIssues();
    issueCountValueLabel_->setText(
        issues.isEmpty()
            ? tr("No inspection issues yet. Use the toolbar above to mark one from the point cloud.")
            : tr("%1 inspection issue(s)").arg(QLocale().toString(issues.size())));
    issueToolStatusLabel_->setText(
        viewer_->issueEditMode() == IssueEditMode::Add
            ? tr("Issue tool: click points in the view continuously to add inspection issues. Right-click to cancel the tool.")
            : tr("Select an issue to edit its business details, focus the scene, or export reports."));

    const int selectedRow = viewer_->selectedIssueIndex();
    const QSignalBlocker blocker(issueTableWidget_);
    issueTableWidget_->setRowCount(0);
    for (int issueIndex = 0; issueIndex < issues.size(); ++issueIndex) {
        const InspectionIssue& issue = issues.at(issueIndex);
        issueTableWidget_->insertRow(issueIndex);

        auto* indexItem = new QTableWidgetItem(QLocale().toString(issueIndex + 1));
        indexItem->setFlags((indexItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        indexItem->setTextAlignment(Qt::AlignCenter);
        issueTableWidget_->setItem(issueIndex, 0, indexItem);

        auto* titleItem = new QTableWidgetItem(issue.title);
        titleItem->setFlags((titleItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        issueTableWidget_->setItem(issueIndex, 1, titleItem);

        const QStringList rowValues = {
            issueSeverityDisplayName(issue.severity),
            issueStatusDisplayName(issue.status),
            issue.relatedTowerName,
            issue.category
        };
        for (int valueIndex = 0; valueIndex < rowValues.size(); ++valueIndex) {
            auto* valueItem = new QTableWidgetItem(rowValues.at(valueIndex));
            valueItem->setFlags((valueItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            issueTableWidget_->setItem(issueIndex, valueIndex + 2, valueItem);
        }
    }

    if (issueTableWidget_->rowCount() > 0 && selectedRow >= 0) {
        issueTableWidget_->setCurrentCell(std::clamp(selectedRow, 0, issueTableWidget_->rowCount() - 1), 1);
    } else {
        issueTableWidget_->clearSelection();
    }

    updateIssueDetailEditor();
}

void MainWindow::updateIssueDetailEditor()
{
    if (viewer_ == nullptr || issueTitleEdit_ == nullptr) {
        return;
    }

    updatingIssueDetails_ = true;
    const QList<InspectionIssue>& issues = viewer_->inspectionIssues();
    const QList<TowerRecord>& towers = viewer_->towerMarkers();
    const int selectedIssueIndex = viewer_->selectedIssueIndex();
    const bool hasSelection = selectedIssueIndex >= 0 && selectedIssueIndex < issues.size();
    const InspectionIssue issue = hasSelection ? issues.at(selectedIssueIndex) : InspectionIssue();

    issueTitleEdit_->setText(issue.title);
    issueCategoryComboBox_->setEditText(issue.category);
    issueSeverityComboBox_->setCurrentIndex(static_cast<int>(issue.severity));
    issueStatusComboBox_->setCurrentIndex(static_cast<int>(issue.status));
    issueImagePathEdit_->setText(issue.imagePath);
    issueDescriptionEdit_->setPlainText(issue.description);
    issueLocationValueLabel_->setText(
        hasSelection ? formatTriplet(issue.point.x, issue.point.y, issue.point.z) : tr("N/A"));
    issueCreatedAtValueLabel_->setText(hasSelection ? issue.createdAt : tr("N/A"));

    issueRelatedTowerComboBox_->clear();
    issueRelatedTowerComboBox_->addItem(tr("None"), -1);
    for (int towerIndex = 0; towerIndex < towers.size(); ++towerIndex) {
        issueRelatedTowerComboBox_->addItem(towers.at(towerIndex).name, towerIndex);
    }
    const int relatedTowerOption = issueRelatedTowerComboBox_->findData(issue.relatedTowerIndex);
    issueRelatedTowerComboBox_->setCurrentIndex(relatedTowerOption >= 0 ? relatedTowerOption : 0);

    const QList<QWidget*> editors = {
        issueTitleEdit_,
        issueCategoryComboBox_,
        issueSeverityComboBox_,
        issueStatusComboBox_,
        issueRelatedTowerComboBox_,
        issueImagePathEdit_,
        issueDescriptionEdit_
    };
    for (QWidget* editor : editors) {
        if (editor != nullptr) {
            editor->setEnabled(hasSelection);
        }
    }
    updatingIssueDetails_ = false;
}

void MainWindow::setTowerEditingEnabled(bool enabled)
{
    if (towerEditingEnabled_ == enabled) {
        return;
    }

    towerEditingEnabled_ = enabled;
    if (!towerEditingEnabled_ && viewer_ != nullptr) {
        viewer_->cancelTowerEditMode();
    }

    updateActionState();
    updateTowerPanel();
}

void MainWindow::applyLanguage(UiLanguage language)
{
    currentLanguage_ = language;

    if (appTranslator_ != nullptr) {
        qApp->removeTranslator(appTranslator_);
        if (language == UiLanguage::Chinese) {
            const QString translationDir = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
            appTranslator_->load(QStringLiteral("lasviewer_zh_CN"), translationDir);
            qApp->installTranslator(appTranslator_);
        }
    }

    if (qtTranslator_ != nullptr) {
        qApp->removeTranslator(qtTranslator_);
        if (language == UiLanguage::Chinese) {
            const QString deployedTranslationDir = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
            const QString qtTranslationName = QStringLiteral("qt_zh_CN");
            if (qtTranslator_->load(qtTranslationName, deployedTranslationDir)
                || qtTranslator_->load(qtTranslationName, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
                qApp->installTranslator(qtTranslator_);
            }
        }
    }

    persistLanguageSettings();
    retranslateUi();
}
