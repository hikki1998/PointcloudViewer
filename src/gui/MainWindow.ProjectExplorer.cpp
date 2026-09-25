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
#include <QProcess>
#include <QPushButton>
#include <QDropEvent>
#include <QFile>
#include <QPalette>
#include <QPlainTextEdit>
#include <QGuiApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSet>
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
#include "QtnRibbonBackstageView.h"
#include "QtnRibbonGroup.h"
#include "QtnRibbonPage.h"
#include "QtnRibbonQuickAccessBar.h"
#include "QtnRibbonSystemPopupBar.h"
#include "QtnRibbonToolTip.h"

#include "crs/CrsAuthorityService.h"
#include "crs/CrsTransformService.h"
#include "crs/ProjectCoordinateSystemsDialog.h"
#include "crs/CrsTypes.h"
#include "domain/ClearanceAnalysis.h"
#include "domain/ClearanceReportExporter.h"
#include "domain/ClassificationEditStore.h"
#include "domain/DataManager.h"
#include "domain/InspectionData.h"
#include "domain/InspectionReportExporter.h"
#include "domain/ProfileMarkerProjection.h"
#include "domain/RuleBasedClearanceEngine.h"
#include "domain/TowerFileInterop.h"
#include "domain/VegetationRiskAnalysis.h"
#include "gui/ApplicationLogDock.h"
#include "gui/BackstageAboutWidget.h"
#include "gui/BackstageApplicationSettingsWidget.h"
#include "gui/BackstageOpenActionsWidget.h"
#include "gui/BackstageOpenProjectWidget.h"
#include "gui/BackstagePageHeaderWidget.h"
#include "gui/BackstageProjectPropertiesWidget.h"
#include "gui/DatasetSummaryWidget.h"
#include "gui/IssueController.h"
#include "gui/IssueEditorWidget.h"
#include "gui/MainWindowInternal.h"
#include "gui/MeasurementAnalysisController.h"
#include "gui/MeasurementPanelWidget.h"
#include "gui/NavigationSettingsWidget.h"
#include "gui/PointCloudViewer.h"
#include "gui/ProfileClassificationController.h"
#include "gui/ProfileClassificationDock.h"
#include "gui/ProfileClassificationWidget.h"
#include "gui/ProfilePlotWidget.h"
#include "gui/ProjectExplorerController.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/RouteController.h"
#include "gui/RouteDetailsDock.h"
#include "gui/SceneInspectorDock.h"
#include "gui/SpanProfileDock.h"
#include "gui/TowerController.h"
#include "gui/TowerEditorWidget.h"
#include "gui/UiHistoryStore.h"
#include "gui/VisualizationPanelController.h"
#include "gui/WebPageDock.h"
#include "gui/support/RibbonIconFactory.h"
#include "gui/support/SettingsKeys.h"
#include "gui/support/UiHelpers.h"
#include "logging/ApplicationLogger.h"
#include "osg/PointCloudVisualization.h"
#include "pointcloud/LasReader.h"
#include "pointcloud/PointCloudData.h"
#include "route/InspectionRoutePlanning.h"
#include "route/PowerlineRouteBridge.h"
#include "route/PowerlineRouteJson.h"
#include "route/RouteInterop.h"

#ifdef LAS_VIEWER_HAS_LASLIB
#include "lasreader.hpp"
#include "laswriter.hpp"
#endif

using lasviewer::crs::CoordinateSystemKind;
using lasviewer::crs::CoordinateSystemRef;
using lasviewer::crs::CrsAuthorityService;
using lasviewer::crs::CrsTransformService;
using lasviewer::crs::ProjectCoordinateSystemsDialog;
using lasviewer::gui::RibbonGlyph;
using lasviewer::gui::WindowControlGlyph;
using lasviewer::gui::applyStyledDialogPalette;
using lasviewer::gui::createResourceIconOrFallback;
using lasviewer::gui::createRibbonIcon;
using lasviewer::gui::createWindowControlIcon;
using lasviewer::gui::enforceLightDialogButtonStyles;
using lasviewer::gui::setFormFieldLabel;
using lasviewer::gui::showLightStyledMessageBox;
using lasviewer::gui::showStyledOpenFileNameDialog;
using lasviewer::gui::showStyledOpenFileNamesDialog;
using lasviewer::gui::showStyledSaveFileNameDialog;
namespace settingskeys = lasviewer::gui::settingskeys;
using namespace mainwindow_internal;

namespace
{
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
    auto* coordinateSystemsItem = new QTreeWidgetItem(projectTreeWidget_, QStringList { tr("Project Management") });
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
    if (projectExplorerController_ != nullptr) {
        projectExplorerController_->refreshFilter();
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
        const QList<PointRecord> routePoints = toRouteDisplayPoints(currentPowerlineRoute_);
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
            currentPowerlineRoute_ = PowerlineRouteDocument();
            linkedRouteFilePath_.clear();
            selectedRouteWaypointIndex_ = -1;
            viewer_->clearInspectionRouteWaypoints();
            updateRoutePlanningPanel();
            rebuildProjectTree();
            updateActionState();
        }
        return;
    }

    if (itemType == QStringLiteral("pointCloudGroup")) {
        QAction* addAction = menu.addAction(tr("Add Data Files"));
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
        QAction* importJsonAction = menu.addAction(tr("Import Route File"));
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
        if (chosenAction == importJsonAction) {
            importRouteFileAction_->trigger();
        } else if (chosenAction == importAction) {
            importRouteKmlAction_->trigger();
        } else if (chosenAction == showAllAction) {
            setProjectTreeGroupVisibility(itemType, true);
        } else if (chosenAction == hideAllAction) {
            setProjectTreeGroupVisibility(itemType, false);
        } else if (chosenAction == removeAction) {
            currentPowerlineRoute_ = PowerlineRouteDocument();
            linkedRouteFilePath_.clear();
            selectedRouteWaypointIndex_ = -1;
            viewer_->clearInspectionRouteWaypoints();
            updateRoutePlanningPanel();
            rebuildProjectTree();
            updateActionState();
        }
        return;
    }

    if (itemType == QStringLiteral("projectGroup") || itemType == QStringLiteral("coordinateSystemsItem")) {
        QAction* propertiesAction = menu.addAction(tr("Project Management"));
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
