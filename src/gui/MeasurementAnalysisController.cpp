#include "gui/MeasurementAnalysisController.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>

#include "gui/PointCloudViewer.h"

MeasurementAnalysisController::MeasurementAnalysisController(
    PointCloudViewer* viewer,
    QAction* measureAction,
    QAction* clearMeasurementAction,
    QAction* exportClearanceCsvAction,
    QAction* analyzeVegetationRisksAction,
    QAction* focusVegetationRiskAction,
    QAction* createIssueFromRiskAction,
    QAction* createIssuesFromRisksAction,
    QAction* clearVegetationRisksAction,
    QPushButton* measurementToggleButton,
    QPushButton* measurementClearButton,
    QDoubleSpinBox* clearanceThresholdSpinBox,
    QComboBox* clearanceRulePresetComboBox,
    QDoubleSpinBox* vegetationSearchRadiusSpinBox,
    QDoubleSpinBox* vegetationClusterGapSpinBox,
    QSpinBox* vegetationClusterPointCountSpinBox,
    QCheckBox* preferVegetationClassificationCheckBox,
    QTableWidget* clearanceSegmentsTableWidget,
    QTableWidget* vegetationRisksTableWidget,
    BoolCallback syncProfileDockForMeasurementMode,
    VoidCallback exportClearanceCsv,
    VoidCallback analyzeVegetationRisks,
    VoidCallback focusVegetationRisk,
    VoidCallback createIssueFromSelectedRisk,
    VoidCallback createIssuesFromRisks,
    VoidCallback clearVegetationRisks,
    DoubleCallback clearanceThresholdChanged,
    IntCallback clearanceRulePresetChanged,
    DoubleCallback vegetationSearchRadiusChanged,
    DoubleCallback vegetationClusterGapChanged,
    IntCallback vegetationClusterPointCountChanged,
    BoolCallback preferVegetationClassificationChanged,
    IntCallback clearanceSegmentSelectionChanged,
    IntCallback vegetationRiskSelectionChanged,
    QObject* parent)
    : QObject(parent)
    , viewer_(viewer)
{
    if (measureAction != nullptr) {
        connect(measureAction, &QAction::toggled, this, [this, syncProfileDockForMeasurementMode](bool enabled) {
            if (viewer_ != nullptr) {
                viewer_->setMeasurementEnabled(enabled);
            }
            if (syncProfileDockForMeasurementMode) {
                syncProfileDockForMeasurementMode(enabled);
            }
        });
    }

    if (clearMeasurementAction != nullptr && viewer_ != nullptr) {
        connect(clearMeasurementAction, &QAction::triggered, viewer_, &PointCloudViewer::clearMeasurement);
    }
    if (exportClearanceCsvAction != nullptr) {
        connect(exportClearanceCsvAction, &QAction::triggered, this, [exportClearanceCsv]() {
            if (exportClearanceCsv) {
                exportClearanceCsv();
            }
        });
    }
    if (analyzeVegetationRisksAction != nullptr) {
        connect(analyzeVegetationRisksAction, &QAction::triggered, this, [analyzeVegetationRisks]() {
            if (analyzeVegetationRisks) {
                analyzeVegetationRisks();
            }
        });
    }
    if (focusVegetationRiskAction != nullptr) {
        connect(focusVegetationRiskAction, &QAction::triggered, this, [focusVegetationRisk]() {
            if (focusVegetationRisk) {
                focusVegetationRisk();
            }
        });
    }
    if (createIssueFromRiskAction != nullptr) {
        connect(createIssueFromRiskAction, &QAction::triggered, this, [createIssueFromSelectedRisk]() {
            if (createIssueFromSelectedRisk) {
                createIssueFromSelectedRisk();
            }
        });
    }
    if (createIssuesFromRisksAction != nullptr) {
        connect(createIssuesFromRisksAction, &QAction::triggered, this, [createIssuesFromRisks]() {
            if (createIssuesFromRisks) {
                createIssuesFromRisks();
            }
        });
    }
    if (clearVegetationRisksAction != nullptr) {
        connect(clearVegetationRisksAction, &QAction::triggered, this, [clearVegetationRisks]() {
            if (clearVegetationRisks) {
                clearVegetationRisks();
            }
        });
    }

    if (measurementToggleButton != nullptr) {
        connect(measurementToggleButton, &QPushButton::clicked, this, [this]() {
            if (viewer_ != nullptr) {
                viewer_->setMeasurementEnabled(!viewer_->measurementEnabled());
            }
        });
    }
    if (measurementClearButton != nullptr && viewer_ != nullptr) {
        connect(measurementClearButton, &QPushButton::clicked, viewer_, &PointCloudViewer::clearMeasurement);
    }

    if (clearanceThresholdSpinBox != nullptr) {
        connect(clearanceThresholdSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [clearanceThresholdChanged](double value) {
            if (clearanceThresholdChanged) {
                clearanceThresholdChanged(value);
            }
        });
    }
    if (clearanceRulePresetComboBox != nullptr) {
        connect(clearanceRulePresetComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [clearanceRulePresetChanged](int index) {
            if (clearanceRulePresetChanged) {
                clearanceRulePresetChanged(index);
            }
        });
    }
    if (vegetationSearchRadiusSpinBox != nullptr) {
        connect(vegetationSearchRadiusSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [vegetationSearchRadiusChanged](double value) {
            if (vegetationSearchRadiusChanged) {
                vegetationSearchRadiusChanged(value);
            }
        });
    }
    if (vegetationClusterGapSpinBox != nullptr) {
        connect(vegetationClusterGapSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [vegetationClusterGapChanged](double value) {
            if (vegetationClusterGapChanged) {
                vegetationClusterGapChanged(value);
            }
        });
    }
    if (vegetationClusterPointCountSpinBox != nullptr) {
        connect(vegetationClusterPointCountSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [vegetationClusterPointCountChanged](int value) {
            if (vegetationClusterPointCountChanged) {
                vegetationClusterPointCountChanged(value);
            }
        });
    }
    if (preferVegetationClassificationCheckBox != nullptr) {
        connect(preferVegetationClassificationCheckBox, &QCheckBox::toggled, this, [preferVegetationClassificationChanged](bool checked) {
            if (preferVegetationClassificationChanged) {
                preferVegetationClassificationChanged(checked);
            }
        });
    }

    if (clearanceSegmentsTableWidget != nullptr) {
        connect(clearanceSegmentsTableWidget, &QTableWidget::currentCellChanged, this, [clearanceSegmentSelectionChanged](int currentRow, int, int, int) {
            if (clearanceSegmentSelectionChanged) {
                clearanceSegmentSelectionChanged(currentRow);
            }
        });
    }
    if (vegetationRisksTableWidget != nullptr) {
        connect(vegetationRisksTableWidget, &QTableWidget::currentCellChanged, this, [vegetationRiskSelectionChanged](int currentRow, int, int, int) {
            if (vegetationRiskSelectionChanged) {
                vegetationRiskSelectionChanged(currentRow);
            }
        });
    }
}
