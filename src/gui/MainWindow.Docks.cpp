#include "gui/MainWindow.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolButton>
#include <QToolBar>
#include <QVBoxLayout>

#include "domain/InspectionData.h"
#include "domain/RuleBasedClearanceEngine.h"
#include "gui/ApplicationLogDock.h"
#include "gui/DatasetSummaryWidget.h"
#include "gui/IssueEditorWidget.h"
#include "gui/MainWindowInternal.h"
#include "gui/MeasurementPanelWidget.h"
#include "gui/NavigationSettingsWidget.h"
#include "gui/PointCloudViewer.h"
#include "gui/ProfileClassificationDock.h"
#include "gui/ProfileClassificationWidget.h"
#include "gui/ProjectExplorerController.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/RouteDetailsDock.h"
#include "gui/SceneInspectorDock.h"
#include "gui/SpanProfileDock.h"
#include "gui/TowerEditorWidget.h"
#include "route/InspectionRoutePlanning.h"

using namespace mainwindow_internal;

void MainWindow::createProjectDock()
{
    projectDock_ = new ProjectExplorerDock(this);
    projectDock_->setMinimumWidth(adaptiveDockWidth(this, 0.14, 220, 280));
    projectExplorerController_ = new ProjectExplorerController(
        projectDock_,
        openAction_,
        addPointCloudAction_,
        removeDatasetAction_,
        locateDatasetAction_,
        copyDatasetPathAction_,
        expandProjectTreeAction_,
        collapseProjectTreeAction_,
        this);
    projectSearchEdit_ = projectExplorerController_->searchEdit();
    projectToolBar_ = projectExplorerController_->toolBar();
    projectTreeWidget_ = projectExplorerController_->treeWidget();
    addDockWidget(Qt::LeftDockWidgetArea, projectDock_);
}

void MainWindow::createInspectorPanel()
{
    inspectorDock_ = new SceneInspectorDock(this);
    inspectorDock_->setMinimumWidth(adaptiveDockWidth(this, 0.14, 240, 300));
    inspectorTabWidget_ = inspectorDock_->tabWidget();

    auto overviewTab = qMakePair(inspectorDock_->overviewScrollArea(), inspectorDock_->overviewLayout());
    auto towerTab = qMakePair(inspectorDock_->towerScrollArea(), inspectorDock_->towerLayout());
    auto issueTab = qMakePair(inspectorDock_->issueScrollArea(), inspectorDock_->issueLayout());
    auto renderingTab = qMakePair(inspectorDock_->renderingScrollArea(), inspectorDock_->renderingLayout());
    auto measurementTab = qMakePair(inspectorDock_->measurementScrollArea(), inspectorDock_->measurementLayout());
    auto analysisTab = qMakePair(inspectorDock_->analysisScrollArea(), inspectorDock_->analysisLayout());
    auto navigationTab = qMakePair(inspectorDock_->navigationScrollArea(), inspectorDock_->navigationLayout());

    datasetSummaryWidget_ = new DatasetSummaryWidget(overviewTab.first);
    datasetGroupBox_ = datasetSummaryWidget_;
    datasetLayout_ = datasetSummaryWidget_->datasetLayout();
    datasetNameValueLabel_ = datasetSummaryWidget_->datasetNameValueLabel();
    datasetPathValueLabel_ = datasetSummaryWidget_->datasetPathValueLabel();
    datasetPointsValueLabel_ = datasetSummaryWidget_->datasetPointsValueLabel();
    datasetBoundsValueLabel_ = datasetSummaryWidget_->datasetBoundsValueLabel();
    datasetExtentValueLabel_ = datasetSummaryWidget_->datasetExtentValueLabel();
    datasetColorValueLabel_ = datasetSummaryWidget_->datasetColorValueLabel();

    towerEditorWidget_ = new TowerEditorWidget(towerTab.first);
    towerToolBar_ = towerEditorWidget_->toolBar();
    towerCountValueLabel_ = towerEditorWidget_->towerCountLabel();
    towerToolStatusLabel_ = towerEditorWidget_->towerToolStatusLabel();
    towerTableWidget_ = towerEditorWidget_->towerTable();
    towerDetailsGroupBox_ = towerEditorWidget_->towerDetailsGroupBox();
    towerDetailsLayout_ = towerEditorWidget_->towerDetailsLayout();
    towerCodeEdit_ = towerEditorWidget_->towerCodeEdit();
    towerLineNameEdit_ = towerEditorWidget_->towerLineNameEdit();
    towerVoltageLevelEdit_ = towerEditorWidget_->towerVoltageLevelEdit();
    towerTypeComboBox_ = towerEditorWidget_->towerTypeComboBox();
    towerStructureTypeEdit_ = towerEditorWidget_->towerStructureTypeEdit();
    towerInspectionDateEdit_ = towerEditorWidget_->towerInspectionDateEdit();
    towerStatusEdit_ = towerEditorWidget_->towerStatusEdit();
    towerNotesEdit_ = towerEditorWidget_->towerNotesEdit();

    if (towerToolBar_ != nullptr) {
        towerToolBar_->addAction(addTowerAction_);
        towerToolBar_->addAction(insertTowerAction_);
        towerToolBar_->addAction(moveTowerAction_);
        towerToolBar_->addAction(editCurrentTowerAction_);
        towerToolBar_->addAction(focusTowerAction_);
        towerToolBar_->addAction(removeTowerAction_);
        towerToolBar_->addSeparator();
        towerToolBar_->addAction(importTowerFileAction_);
        towerToolBar_->addAction(saveTowerFileAction_);
        towerToolBar_->addAction(saveTowerFileAsAction_);
        towerToolBar_->addAction(reloadTowerFileAction_);
    }

    if (towerTableWidget_ != nullptr) {
        towerTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Name"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z") });
    }

    if (towerTypeComboBox_ != nullptr) {
        towerTypeComboBox_->addItem(QString(), static_cast<int>(TowerType::Unknown));
        towerTypeComboBox_->addItem(QString(), static_cast<int>(TowerType::Tangent));
        towerTypeComboBox_->addItem(QString(), static_cast<int>(TowerType::Strain));
    }

    issueEditorWidget_ = new IssueEditorWidget(issueTab.first);
    issueToolBar_ = issueEditorWidget_->toolBar();
    issueMenuButton_ = issueEditorWidget_->menuButton();
    issueCountValueLabel_ = issueEditorWidget_->issueCountLabel();
    issueToolStatusLabel_ = issueEditorWidget_->issueToolStatusLabel();
    issueTableWidget_ = issueEditorWidget_->issueTable();
    issueDetailsGroupBox_ = issueEditorWidget_->issueDetailsGroupBox();
    issueDetailsLayout_ = issueEditorWidget_->issueDetailsLayout();
    issueTitleEdit_ = issueEditorWidget_->issueTitleEdit();
    issueCategoryComboBox_ = issueEditorWidget_->issueCategoryComboBox();
    issueSeverityComboBox_ = issueEditorWidget_->issueSeverityComboBox();
    issueStatusComboBox_ = issueEditorWidget_->issueStatusComboBox();
    issueRelatedTowerComboBox_ = issueEditorWidget_->issueRelatedTowerComboBox();
    issueImagePathEdit_ = issueEditorWidget_->issueImagePathEdit();
    issueLocationValueLabel_ = issueEditorWidget_->issueLocationValueLabel();
    issueCreatedAtValueLabel_ = issueEditorWidget_->issueCreatedAtValueLabel();
    issueDescriptionEdit_ = issueEditorWidget_->issueDescriptionEdit();

    if (issueToolBar_ != nullptr) {
        issueToolBar_->addAction(startIssueMarkAction_);
        issueToolBar_->addAction(cancelIssueToolAction_);
        issueToolBar_->addSeparator();
        issueToolBar_->addAction(focusIssueAction_);
        issueToolBar_->addAction(removeIssueAction_);
        issueToolBar_->addAction(clearIssuesAction_);
        issueToolBar_->addSeparator();
        issueToolBar_->addAction(exportIssuesCsvAction_);
        issueToolBar_->addAction(exportInspectionReportAction_);
    }

    issueActionsMenu_ = new QMenu(issueEditorWidget_);
    issueActionsMenu_->addAction(startIssueMarkAction_);
    issueActionsMenu_->addAction(cancelIssueToolAction_);
    issueActionsMenu_->addSeparator();
    issueActionsMenu_->addAction(focusIssueAction_);
    issueActionsMenu_->addAction(removeIssueAction_);
    issueActionsMenu_->addAction(clearIssuesAction_);
    issueActionsMenu_->addSeparator();
    issueActionsMenu_->addAction(exportIssuesCsvAction_);
    issueActionsMenu_->addAction(exportInspectionReportAction_);
    if (issueMenuButton_ != nullptr) {
        issueMenuButton_->setMenu(issueActionsMenu_);
    }

    if (issueTableWidget_ != nullptr) {
        issueTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Title"), tr("Severity"), tr("Status"), tr("Tower"), tr("Category") });
    }

    if (issueCategoryComboBox_ != nullptr) {
        issueCategoryComboBox_->addItems({ tr("Vegetation"), tr("Insulator"), tr("Tower Body"), tr("Channel Risk"), tr("Other") });
    }
    if (issueSeverityComboBox_ != nullptr) {
        issueSeverityComboBox_->addItems({
            issueSeverityDisplayName(IssueSeverity::Info),
            issueSeverityDisplayName(IssueSeverity::Minor),
            issueSeverityDisplayName(IssueSeverity::Major),
            issueSeverityDisplayName(IssueSeverity::Critical)
        });
    }
    if (issueStatusComboBox_ != nullptr) {
        issueStatusComboBox_->addItems({
            issueStatusDisplayName(IssueStatus::Open),
            issueStatusDisplayName(IssueStatus::Monitoring),
            issueStatusDisplayName(IssueStatus::Resolved)
        });
    }

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

    profileClassificationGroupBox_ = new ProfileClassificationWidget(renderingTab.first);

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

    routeRoamSpeedSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeRoamSpeedSpinBox_->setRange(0.1, 80.0);
    routeRoamSpeedSpinBox_->setDecimals(1);
    routeRoamSpeedSpinBox_->setValue(viewer_ != nullptr ? viewer_->inspectionRouteRoamSpeedMetersPerSecond() : 2.0);

    routeRoamViewModeComboBox_ = new QComboBox(routePlanningGroupBox_);
    routeRoamViewModeComboBox_->addItem(tr("Third Person"), static_cast<int>(RouteRoamViewMode::ThirdPerson));
    routeRoamViewModeComboBox_->addItem(tr("First Person"), static_cast<int>(RouteRoamViewMode::FirstPerson));

    routeRoamControlsRow_ = new QWidget(routePlanningGroupBox_);
    auto* routeRoamButtonLayout = new QHBoxLayout(routeRoamControlsRow_);
    routeRoamButtonLayout->setContentsMargins(0, 0, 0, 0);
    routeRoamButtonLayout->setSpacing(6);
    routeRoamStartButton_ = new QPushButton(tr("Start Roam"), routeRoamControlsRow_);
    routeRoamPauseResumeButton_ = new QPushButton(tr("Pause Roam"), routeRoamControlsRow_);
    routeRoamStopButton_ = new QPushButton(tr("Stop Roam"), routeRoamControlsRow_);
    routeRoamButtonLayout->addWidget(routeRoamStartButton_);
    routeRoamButtonLayout->addWidget(routeRoamPauseResumeButton_);
    routeRoamButtonLayout->addWidget(routeRoamStopButton_);

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
    routePlanningLayout->addRow(tr("Roam Speed"), routeRoamSpeedSpinBox_);
    routePlanningLayout->addRow(tr("Roam View Mode"), routeRoamViewModeComboBox_);
    routePlanningLayout->addRow(tr("Roam Controls"), routeRoamControlsRow_);
    routePlanningLayout->addRow(tr("Status"), routeStatusValueLabel_);
    routePlanningLayout->addRow(tr("Summary"), routeSummaryValueLabel_);

    navigationSettingsWidget_ = new NavigationSettingsWidget(navigationTab.first);
    navigationGroupBox_ = navigationSettingsWidget_;
    navigationTipsLabel_ = navigationSettingsWidget_->tipsLabel();
    navigationToggleLayout_ = navigationSettingsWidget_->toggleLayout();
    invertOrbitCheckBox_ = navigationSettingsWidget_->invertOrbitCheckBox();
    invertPanCheckBox_ = navigationSettingsWidget_->invertPanCheckBox();
    invertWheelCheckBox_ = navigationSettingsWidget_->invertWheelCheckBox();
    wheelZoomSensitivityControl_ = navigationSettingsWidget_->wheelZoomSensitivityControl();
    wheelZoomSensitivitySlider_ = navigationSettingsWidget_->wheelZoomSensitivitySlider();
    wheelZoomSensitivityValueLabel_ = navigationSettingsWidget_->wheelZoomSensitivityValueLabel();

    overviewTab.second->addWidget(datasetSummaryWidget_);
    overviewTab.second->addStretch(1);
    towerTab.second->addWidget(towerEditorWidget_, 1);
    issueTab.second->addWidget(issueEditorWidget_, 1);
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
    analysisTab.second->addStretch(1);
    navigationTab.second->addWidget(navigationSettingsWidget_);
    navigationTab.second->addStretch(1);

    addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);
}

void MainWindow::createRouteDetailsDock()
{
    routeDetailsDock_ = new RouteDetailsDock(this);
    routeDetailsDock_->setMinimumWidth(adaptiveDockWidth(this, 0.14, 240, 300));
    routeDetailsTabWidget_ = routeDetailsDock_->tabWidget();
    auto* waypointsTabLayout = routeDetailsDock_->waypointsLayout();

    routeWaypointsGroupBox_ = new QGroupBox(tr("Route Waypoints"), routeDetailsTabWidget_);
    routeWaypointsGroupBox_->setMinimumWidth(0);
    auto* routeWaypointsLayout = new QVBoxLayout(routeWaypointsGroupBox_);
    routeWaypointsLayout->setContentsMargins(10, 10, 10, 10);
    routeWaypointsLayout->setSpacing(8);

    auto* routeWaypointOptionsRow = new QWidget(routeWaypointsGroupBox_);
    routeWaypointOptionsRow->setMinimumWidth(0);
    auto* routeWaypointOptionsLayout = new QVBoxLayout(routeWaypointOptionsRow);
    routeWaypointOptionsLayout->setContentsMargins(0, 0, 0, 0);
    routeWaypointOptionsLayout->setSpacing(6);
    auto* routeWaypointPrimaryRow = new QHBoxLayout();
    routeWaypointPrimaryRow->setContentsMargins(0, 0, 0, 0);
    routeWaypointPrimaryRow->setSpacing(8);
    auto* routeWaypointSecondaryRow = new QHBoxLayout();
    routeWaypointSecondaryRow->setContentsMargins(0, 0, 0, 0);
    routeWaypointSecondaryRow->setSpacing(8);
    routeWaypointLabelModeComboBox_ = new QComboBox(routeWaypointOptionsRow);
    routeWaypointLabelModeComboBox_->setMinimumWidth(0);
    routeWaypointLabelModeComboBox_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    routeWaypointLabelModeComboBox_->addItem(tr("Name"), static_cast<int>(RouteLabelDisplayMode::Name));
    routeWaypointLabelModeComboBox_->addItem(tr("Index"), static_cast<int>(RouteLabelDisplayMode::Sequence));
    routeWaypointLabelModeComboBox_->addItem(tr("Compact Name"), static_cast<int>(RouteLabelDisplayMode::CompactName));
    routeWaypointLabelModeComboBox_->addItem(tr("Compact Index"), static_cast<int>(RouteLabelDisplayMode::CompactSequence));
    routeWaypointLabelModeComboBox_->addItem(tr("Hidden"), static_cast<int>(RouteLabelDisplayMode::Hidden));
    routeWaypointLabelModeComboBox_->setCurrentIndex(0);
    routeWaypointShowCoordinatesCheckBox_ = new QCheckBox(tr("Show Coordinates"), routeWaypointOptionsRow);
    routeWaypointShowCaptureAnglesCheckBox_ = new QCheckBox(tr("Show Capture Angles"), routeWaypointOptionsRow);
    routeWaypointShowCoordinatesCheckBox_->setChecked(true);
    routeWaypointShowCaptureAnglesCheckBox_->setChecked(true);
    routeTrajectoryColorButton_ = new QPushButton(tr("Trajectory Color"), routeWaypointOptionsRow);
    routeWaypointColorButton_ = new QPushButton(tr("Waypoint Color"), routeWaypointOptionsRow);
    routeTrajectoryColorButton_->setMinimumWidth(0);
    routeWaypointColorButton_->setMinimumWidth(0);
    routeTrajectoryColorButton_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    routeWaypointColorButton_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    routeWaypointPrimaryRow->addWidget(routeWaypointLabelModeComboBox_, 1);
    routeWaypointPrimaryRow->addWidget(routeTrajectoryColorButton_);
    routeWaypointPrimaryRow->addWidget(routeWaypointColorButton_);
    routeWaypointSecondaryRow->addWidget(routeWaypointShowCoordinatesCheckBox_);
    routeWaypointSecondaryRow->addWidget(routeWaypointShowCaptureAnglesCheckBox_);
    routeWaypointSecondaryRow->addStretch(1);
    routeWaypointOptionsLayout->addLayout(routeWaypointPrimaryRow);
    routeWaypointOptionsLayout->addLayout(routeWaypointSecondaryRow);

    routeWaypointsTableWidget_ = new QTableWidget(routeWaypointsGroupBox_);
    routeWaypointsTableWidget_->setMinimumWidth(0);
    routeWaypointsTableWidget_->setColumnCount(9);
    routeWaypointsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    routeWaypointsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routeWaypointsTableWidget_->setAlternatingRowColors(true);
    routeWaypointsTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    routeWaypointsTableWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    routeWaypointsTableWidget_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    routeWaypointsTableWidget_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    routeWaypointsTableWidget_->setHorizontalHeaderLabels({
        tr("Index"),
        tr("Part"),
        tr("X"),
        tr("Y"),
        tr("Z"),
        tr("Aircraft Yaw"),
        tr("Gimbal Pitch"),
        tr("Camera Yaw"),
        tr("Camera Pitch")
    });
    routeWaypointsTableWidget_->verticalHeader()->setVisible(false);
    QHeaderView* waypointHeader = routeWaypointsTableWidget_->horizontalHeader();
    waypointHeader->setStretchLastSection(false);
    waypointHeader->setSectionResizeMode(QHeaderView::Interactive);
    routeWaypointsTableWidget_->setColumnWidth(0, 68);
    routeWaypointsTableWidget_->setColumnWidth(1, 220);
    routeWaypointsTableWidget_->setColumnWidth(2, 98);
    routeWaypointsTableWidget_->setColumnWidth(3, 98);
    routeWaypointsTableWidget_->setColumnWidth(4, 98);
    routeWaypointsTableWidget_->setColumnWidth(5, 116);
    routeWaypointsTableWidget_->setColumnWidth(6, 116);
    routeWaypointsTableWidget_->setColumnWidth(7, 116);
    routeWaypointsTableWidget_->setColumnWidth(8, 116);
    routeWaypointsTableWidget_->setStyleSheet(routeTableStyleSheet());
    routeWaypointsTableWidget_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    routeWaypointTargetsGroupBox_ = new QGroupBox(tr("Waypoint Targets"), routeWaypointsGroupBox_);
    routeWaypointTargetsGroupBox_->setMinimumWidth(0);
    auto* routeWaypointTargetsLayout = new QVBoxLayout(routeWaypointTargetsGroupBox_);
    routeWaypointTargetsLayout->setContentsMargins(8, 8, 8, 8);
    routeWaypointTargetsLayout->setSpacing(6);

    routeWaypointTargetsTableWidget_ = new QTableWidget(routeWaypointTargetsGroupBox_);
    routeWaypointTargetsTableWidget_->setMinimumWidth(0);
    routeWaypointTargetsTableWidget_->setColumnCount(6);
    routeWaypointTargetsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    routeWaypointTargetsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routeWaypointTargetsTableWidget_->setAlternatingRowColors(true);
    routeWaypointTargetsTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    routeWaypointTargetsTableWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    routeWaypointTargetsTableWidget_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    routeWaypointTargetsTableWidget_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    routeWaypointTargetsTableWidget_->setHorizontalHeaderLabels({
        tr("Index"),
        tr("Part"),
        tr("Focal Ratio"),
        tr("Camera Yaw"),
        tr("Camera Pitch"),
        tr("Target Point")
    });
    routeWaypointTargetsTableWidget_->verticalHeader()->setVisible(false);
    QHeaderView* targetHeader = routeWaypointTargetsTableWidget_->horizontalHeader();
    targetHeader->setStretchLastSection(false);
    targetHeader->setSectionResizeMode(QHeaderView::Interactive);
    routeWaypointTargetsTableWidget_->setColumnWidth(0, 62);
    routeWaypointTargetsTableWidget_->setColumnWidth(1, 210);
    routeWaypointTargetsTableWidget_->setColumnWidth(2, 96);
    routeWaypointTargetsTableWidget_->setColumnWidth(3, 108);
    routeWaypointTargetsTableWidget_->setColumnWidth(4, 108);
    routeWaypointTargetsTableWidget_->setColumnWidth(5, 220);
    routeWaypointTargetsTableWidget_->setMinimumHeight(170);
    routeWaypointTargetsTableWidget_->setStyleSheet(routeTableStyleSheet());
    routeWaypointTargetsTableWidget_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    routeWaypointTargetsLayout->addWidget(routeWaypointTargetsTableWidget_);

    routeWaypointsLayout->addWidget(routeWaypointOptionsRow);
    routeWaypointsLayout->addWidget(routeWaypointsTableWidget_, 1);
    routeWaypointsLayout->addWidget(routeWaypointTargetsGroupBox_, 0);
    waypointsTabLayout->addWidget(routeWaypointsGroupBox_, 1);

    auto* partPointsTabLayout = routeDetailsDock_->partPointsLayout();

    routePartsGroupBox_ = new QGroupBox(tr("Route Part Points"), routeDetailsTabWidget_);
    routePartsGroupBox_->setMinimumWidth(0);
    auto* routePartsLayout = new QVBoxLayout(routePartsGroupBox_);
    routePartsLayout->setContentsMargins(10, 10, 10, 10);
    routePartsLayout->setSpacing(8);

    auto* routePartOptionsRow = new QWidget(routePartsGroupBox_);
    routePartOptionsRow->setMinimumWidth(0);
    auto* routePartOptionsLayout = new QVBoxLayout(routePartOptionsRow);
    routePartOptionsLayout->setContentsMargins(0, 0, 0, 0);
    routePartOptionsLayout->setSpacing(6);
    auto* routePartPrimaryRow = new QHBoxLayout();
    routePartPrimaryRow->setContentsMargins(0, 0, 0, 0);
    routePartPrimaryRow->setSpacing(8);
    auto* routePartSecondaryRow = new QHBoxLayout();
    routePartSecondaryRow->setContentsMargins(0, 0, 0, 0);
    routePartSecondaryRow->setSpacing(8);
    routePartLabelModeComboBox_ = new QComboBox(routePartOptionsRow);
    routePartLabelModeComboBox_->setMinimumWidth(0);
    routePartLabelModeComboBox_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    routePartLabelModeComboBox_->addItem(tr("Name"), static_cast<int>(RouteLabelDisplayMode::Name));
    routePartLabelModeComboBox_->addItem(tr("Index"), static_cast<int>(RouteLabelDisplayMode::Sequence));
    routePartLabelModeComboBox_->addItem(tr("Compact Name"), static_cast<int>(RouteLabelDisplayMode::CompactName));
    routePartLabelModeComboBox_->addItem(tr("Compact Index"), static_cast<int>(RouteLabelDisplayMode::CompactSequence));
    routePartLabelModeComboBox_->addItem(tr("Hidden"), static_cast<int>(RouteLabelDisplayMode::Hidden));
    routePartLabelModeComboBox_->setCurrentIndex(0);
    routePartShowCoordinatesCheckBox_ = new QCheckBox(tr("Show Coordinates"), routePartOptionsRow);
    routePartShowCaptureAnglesCheckBox_ = new QCheckBox(tr("Show Capture Angles"), routePartOptionsRow);
    routePartShowCoordinatesCheckBox_->setChecked(true);
    routePartShowCaptureAnglesCheckBox_->setChecked(true);
    routePartPointColorButton_ = new QPushButton(tr("Part Point Color"), routePartOptionsRow);
    routePartPointColorButton_->setMinimumWidth(0);
    routePartPointColorButton_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    routePartPrimaryRow->addWidget(routePartLabelModeComboBox_, 1);
    routePartPrimaryRow->addWidget(routePartPointColorButton_);
    routePartSecondaryRow->addWidget(routePartShowCoordinatesCheckBox_);
    routePartSecondaryRow->addWidget(routePartShowCaptureAnglesCheckBox_);
    routePartSecondaryRow->addStretch(1);
    routePartOptionsLayout->addLayout(routePartPrimaryRow);
    routePartOptionsLayout->addLayout(routePartSecondaryRow);

    routePartPointsTableWidget_ = new QTableWidget(routePartsGroupBox_);
    routePartPointsTableWidget_->setMinimumWidth(0);
    routePartPointsTableWidget_->setColumnCount(8);
    routePartPointsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    routePartPointsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routePartPointsTableWidget_->setAlternatingRowColors(true);
    routePartPointsTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    routePartPointsTableWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    routePartPointsTableWidget_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    routePartPointsTableWidget_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    routePartPointsTableWidget_->setHorizontalHeaderLabels({
        tr("Index"),
        tr("Part Name"),
        tr("Hardware"),
        tr("Phase"),
        tr("Camera Angle"),
        tr("X"),
        tr("Y"),
        tr("Z")
    });
    routePartPointsTableWidget_->verticalHeader()->setVisible(false);
    QHeaderView* routePartHeader = routePartPointsTableWidget_->horizontalHeader();
    routePartHeader->setStretchLastSection(false);
    routePartHeader->setSectionResizeMode(QHeaderView::Interactive);
    routePartPointsTableWidget_->setColumnWidth(0, 68);
    routePartPointsTableWidget_->setColumnWidth(1, 180);
    routePartPointsTableWidget_->setColumnWidth(2, 122);
    routePartPointsTableWidget_->setColumnWidth(3, 108);
    routePartPointsTableWidget_->setColumnWidth(4, 108);
    routePartPointsTableWidget_->setColumnWidth(5, 98);
    routePartPointsTableWidget_->setColumnWidth(6, 98);
    routePartPointsTableWidget_->setColumnWidth(7, 98);
    routePartPointsTableWidget_->setStyleSheet(routeTableStyleSheet());
    routePartPointsTableWidget_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    routePartsLayout->addWidget(routePartOptionsRow);
    routePartsLayout->addWidget(routePartPointsTableWidget_, 1);
    partPointsTabLayout->addWidget(routePartsGroupBox_, 1);

    auto* routeQaTabLayout = routeDetailsDock_->routeQaLayout();

    routeQaGroupBox_ = new QGroupBox(tr("Route QA"), routeDetailsTabWidget_);
    routeQaGroupBox_->setMinimumWidth(0);
    auto* routeQaLayout = new QVBoxLayout(routeQaGroupBox_);
    routeQaLayout->setContentsMargins(10, 10, 10, 10);
    routeQaLayout->setSpacing(8);

    routeQaSummaryValueLabel_ = new QLabel(tr("Route QA will run automatically after route updates."), routeQaGroupBox_);
    routeQaSummaryValueLabel_->setWordWrap(true);
    routeQaSummaryValueLabel_->setStyleSheet(QStringLiteral("color: #166534; font-weight: 600;"));

    routeQaIssuesTableWidget_ = new QTableWidget(routeQaGroupBox_);
    routeQaIssuesTableWidget_->setMinimumWidth(0);
    routeQaIssuesTableWidget_->setColumnCount(5);
    routeQaIssuesTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    routeQaIssuesTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routeQaIssuesTableWidget_->setAlternatingRowColors(true);
    routeQaIssuesTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    routeQaIssuesTableWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    routeQaIssuesTableWidget_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    routeQaIssuesTableWidget_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    routeQaIssuesTableWidget_->setHorizontalHeaderLabels({
        tr("Severity"),
        tr("Issue"),
        tr("Location"),
        tr("Part"),
        tr("Description")
    });
    routeQaIssuesTableWidget_->verticalHeader()->setVisible(false);
    QHeaderView* routeQaHeader = routeQaIssuesTableWidget_->horizontalHeader();
    routeQaHeader->setStretchLastSection(false);
    routeQaHeader->setSectionResizeMode(QHeaderView::Interactive);
    routeQaIssuesTableWidget_->setColumnWidth(0, 96);
    routeQaIssuesTableWidget_->setColumnWidth(1, 140);
    routeQaIssuesTableWidget_->setColumnWidth(2, 120);
    routeQaIssuesTableWidget_->setColumnWidth(3, 170);
    routeQaIssuesTableWidget_->setColumnWidth(4, 360);
    routeQaIssuesTableWidget_->setStyleSheet(routeTableStyleSheet());
    routeQaIssuesTableWidget_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    routeQaLayout->addWidget(routeQaSummaryValueLabel_);
    routeQaLayout->addWidget(routeQaIssuesTableWidget_, 1);
    routeQaTabLayout->addWidget(routeQaGroupBox_, 1);

    if (viewer_ != nullptr) {
        setColorButtonAppearance(routeWaypointColorButton_, viewer_->inspectionRouteWaypointColor(), tr("Waypoint Color"));
        setColorButtonAppearance(routePartPointColorButton_, viewer_->inspectionRoutePartPointColor(), tr("Part Point Color"));
        setColorButtonAppearance(routeTrajectoryColorButton_, viewer_->inspectionRouteTrajectoryColor(), tr("Trajectory Color"));
    }
    applyRouteWaypointTableColumnVisibility();
    applyRoutePartTableColumnVisibility();

    addDockWidget(Qt::RightDockWidgetArea, routeDetailsDock_);
    if (inspectorDock_ != nullptr) {
        tabifyDockWidget(inspectorDock_, routeDetailsDock_);
    }
    routeDetailsDock_->hide();
}

void MainWindow::createProfileClassificationDock()
{
    profileClassificationDock_ = new ProfileClassificationDock(this);
    profileClassificationDock_->setMinimumWidth(adaptiveDockWidth(this, 0.16, 240, 300));
    profileClassificationDock_->setContentWidget(profileClassificationGroupBox_);

    addDockWidget(Qt::LeftDockWidgetArea, profileClassificationDock_);
    if (projectDock_ != nullptr) {
        tabifyDockWidget(projectDock_, profileClassificationDock_);
    }
    profileClassificationDock_->hide();
}

void MainWindow::createProfileDock()
{
    profileDock_ = new SpanProfileDock(this);
    profilePlotWidget_ = profileDock_->plotWidget();
    addDockWidget(Qt::BottomDockWidgetArea, profileDock_);
    profileDock_->hide();
}

void MainWindow::createLogDock()
{
    logDock_ = new ApplicationLogDock(this);
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
