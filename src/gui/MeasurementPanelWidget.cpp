#include "gui/MeasurementPanelWidget.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>

MeasurementPanelWidget::MeasurementPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    auto* measurementToolbarHost = new QWidget(this);
    auto* measurementToolbarHostLayout = new QHBoxLayout(measurementToolbarHost);
    measurementToolbarHostLayout->setContentsMargins(0, 0, 0, 0);
    measurementToolbarHostLayout->setSpacing(8);

    toolBar_ = new QToolBar(measurementToolbarHost);
    toolBar_->setIconSize(QSize(16, 16));
    toolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar_->setMovable(false);
    toolBar_->setFloatable(false);
    measurementToolbarHostLayout->addWidget(toolBar_, 1);

    measurementGroupBox_ = new QGroupBox(measurementToolbarHost);
    measurementLayout_ = new QFormLayout(measurementGroupBox_);
    measurementLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    measurementLayout_->setFormAlignment(Qt::AlignTop);

    measurementToggleButton_ = new QPushButton(measurementGroupBox_);
    measurementClearButton_ = new QPushButton(measurementGroupBox_);
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
    measurementLayout_->addRow(QString(), measurementStartValueLabel_);
    measurementLayout_->addRow(QString(), measurementEndValueLabel_);
    measurementLayout_->addRow(QString(), measurementDistanceValueLabel_);
    measurementLayout_->addRow(QString(), measurementHorizontalDistanceValueLabel_);
    measurementLayout_->addRow(QString(), measurementDeltaZValueLabel_);
    measurementLayout_->addRow(QString(), measurementSegmentsValueLabel_);

    clearanceGroupBox_ = new QGroupBox(this);
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
    clearanceRuleBandsValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceRuleBandsValueLabel_->setWordWrap(true);
    clearanceRuleBandsValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    clearanceLayout_->addRow(QString(), clearanceRulePresetComboBox_);
    clearanceLayout_->addRow(QString(), clearanceThresholdSpinBox_);
    clearanceLayout_->addRow(QString(), clearanceRuleBandsValueLabel_);
    clearanceLayout_->addRow(QString(), clearanceShortestValueLabel_);
    clearanceLayout_->addRow(QString(), clearanceWarningCountValueLabel_);
    clearanceLayout_->addRow(QString(), clearanceStatusValueLabel_);

    clearanceSegmentsGroupBox_ = new QGroupBox(this);
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

    rootLayout->addWidget(measurementToolbarHost);
    rootLayout->addWidget(measurementGroupBox_);
    rootLayout->addWidget(clearanceGroupBox_);
    rootLayout->addWidget(clearanceSegmentsGroupBox_, 1);
}

QToolBar* MeasurementPanelWidget::toolBar() const { return toolBar_; }

QGroupBox* MeasurementPanelWidget::measurementGroupBox() const { return measurementGroupBox_; }
QFormLayout* MeasurementPanelWidget::measurementLayout() const { return measurementLayout_; }
QPushButton* MeasurementPanelWidget::measurementToggleButton() const { return measurementToggleButton_; }
QPushButton* MeasurementPanelWidget::measurementClearButton() const { return measurementClearButton_; }
QLabel* MeasurementPanelWidget::measurementStartValueLabel() const { return measurementStartValueLabel_; }
QLabel* MeasurementPanelWidget::measurementEndValueLabel() const { return measurementEndValueLabel_; }
QLabel* MeasurementPanelWidget::measurementDistanceValueLabel() const { return measurementDistanceValueLabel_; }
QLabel* MeasurementPanelWidget::measurementHorizontalDistanceValueLabel() const { return measurementHorizontalDistanceValueLabel_; }
QLabel* MeasurementPanelWidget::measurementDeltaZValueLabel() const { return measurementDeltaZValueLabel_; }
QLabel* MeasurementPanelWidget::measurementSegmentsValueLabel() const { return measurementSegmentsValueLabel_; }

QGroupBox* MeasurementPanelWidget::clearanceGroupBox() const { return clearanceGroupBox_; }
QFormLayout* MeasurementPanelWidget::clearanceLayout() const { return clearanceLayout_; }
QDoubleSpinBox* MeasurementPanelWidget::clearanceThresholdSpinBox() const { return clearanceThresholdSpinBox_; }
QLabel* MeasurementPanelWidget::clearanceShortestValueLabel() const { return clearanceShortestValueLabel_; }
QLabel* MeasurementPanelWidget::clearanceWarningCountValueLabel() const { return clearanceWarningCountValueLabel_; }
QLabel* MeasurementPanelWidget::clearanceStatusValueLabel() const { return clearanceStatusValueLabel_; }
QComboBox* MeasurementPanelWidget::clearanceRulePresetComboBox() const { return clearanceRulePresetComboBox_; }
QLabel* MeasurementPanelWidget::clearanceRuleBandsValueLabel() const { return clearanceRuleBandsValueLabel_; }

QGroupBox* MeasurementPanelWidget::clearanceSegmentsGroupBox() const { return clearanceSegmentsGroupBox_; }
QLabel* MeasurementPanelWidget::clearanceSegmentsSummaryLabel() const { return clearanceSegmentsSummaryLabel_; }
QTableWidget* MeasurementPanelWidget::clearanceSegmentsTableWidget() const { return clearanceSegmentsTableWidget_; }
