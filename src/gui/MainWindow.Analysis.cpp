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
QString measurementPointText(const MeasurementResult& measurementResult, bool useStartPoint)
{
    const bool hasPoint = useStartPoint ? measurementResult.hasStartPoint : measurementResult.hasEndPoint;
    if (!hasPoint) {
        return QCoreApplication::translate("MainWindow", "Not set");
    }
    const PointRecord& point = useStartPoint ? measurementResult.startPoint : measurementResult.endPoint;
    return formatTriplet(point.x, point.y, point.z);
}

QColor analysisSeverityColor(AnalysisSeverity severity)
{
    switch (severity) {
    case AnalysisSeverity::Advisory: return QColor(217, 119, 6);
    case AnalysisSeverity::Warning: return QColor(220, 38, 38);
    case AnalysisSeverity::Critical: return QColor(127, 29, 29);
    case AnalysisSeverity::None:
    default: return QColor(22, 101, 52);
    }
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
