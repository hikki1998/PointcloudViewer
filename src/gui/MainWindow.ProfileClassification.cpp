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
    if (profileClassificationController_ != nullptr) {
        profileClassificationController_->refreshPanel(classificationEditsDirty_);
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
