#include "gui/SceneInspectorDock.h"

#include <QColor>
#include <QFrame>
#include <QLayout>
#include <QPalette>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTabBar>
#include <QTabWidget>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

SceneInspectorDock::SceneInspectorDock(QWidget* parent)
    : QDockWidget(parent)
{
    setObjectName(QStringLiteral("sceneInspectorDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    tabWidget_ = new QTabWidget(this);
    tabWidget_->setObjectName(QStringLiteral("sceneInspectorTabs"));
    tabWidget_->setDocumentMode(true);
    tabWidget_->setMovable(false);
    tabWidget_->setUsesScrollButtons(true);
    tabWidget_->setElideMode(Qt::ElideRight);
    tabWidget_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    if (QTabBar* tabBar = tabWidget_->tabBar()) {
        tabBar->setExpanding(false);
        tabBar->setElideMode(Qt::ElideRight);
    }

    overviewTab_ = createTabPage(QStringLiteral("sceneInspectorOverviewPage"));
    towerTab_ = createTabPage(QStringLiteral("sceneInspectorTowerPage"));
    issueTab_ = createTabPage(QStringLiteral("sceneInspectorIssuePage"));
    renderingTab_ = createTabPage(QStringLiteral("sceneInspectorRenderingPage"));
    measurementTab_ = createTabPage(QStringLiteral("sceneInspectorMeasurementPage"));
    analysisTab_ = createTabPage(QStringLiteral("sceneInspectorAnalysisPage"));
    navigationTab_ = createTabPage(QStringLiteral("sceneInspectorNavigationPage"));

    tabWidget_->addTab(overviewTab_.scrollArea, QString());
    tabWidget_->addTab(towerTab_.scrollArea, QString());
    tabWidget_->addTab(issueTab_.scrollArea, QString());
    tabWidget_->addTab(renderingTab_.scrollArea, QString());
    tabWidget_->addTab(measurementTab_.scrollArea, QString());
    tabWidget_->addTab(analysisTab_.scrollArea, QString());
    tabWidget_->addTab(navigationTab_.scrollArea, QString());

    tabWidget_->setStyleSheet(
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
        "padding: 8px 10px;"
        "margin-right: 2px;"
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

    setWidget(tabWidget_);
    retranslateUi();
}

QSize SceneInspectorDock::sizeHint() const
{
    QSize hint = QDockWidget::sizeHint();
    hint.setWidth(std::max(minimumWidth(), 0));
    return hint;
}

QSize SceneInspectorDock::minimumSizeHint() const
{
    QSize hint = QDockWidget::minimumSizeHint();
    hint.setWidth(std::max(minimumWidth(), hint.width()));
    return hint;
}

void SceneInspectorDock::retranslateUi()
{
    setWindowTitle(tr("Scene Inspector"));
}

QTabWidget* SceneInspectorDock::tabWidget() const
{
    return tabWidget_;
}

QScrollArea* SceneInspectorDock::overviewScrollArea() const { return overviewTab_.scrollArea; }
QVBoxLayout* SceneInspectorDock::overviewLayout() const { return overviewTab_.layout; }
QScrollArea* SceneInspectorDock::towerScrollArea() const { return towerTab_.scrollArea; }
QVBoxLayout* SceneInspectorDock::towerLayout() const { return towerTab_.layout; }
QScrollArea* SceneInspectorDock::issueScrollArea() const { return issueTab_.scrollArea; }
QVBoxLayout* SceneInspectorDock::issueLayout() const { return issueTab_.layout; }
QScrollArea* SceneInspectorDock::renderingScrollArea() const { return renderingTab_.scrollArea; }
QVBoxLayout* SceneInspectorDock::renderingLayout() const { return renderingTab_.layout; }
QScrollArea* SceneInspectorDock::measurementScrollArea() const { return measurementTab_.scrollArea; }
QVBoxLayout* SceneInspectorDock::measurementLayout() const { return measurementTab_.layout; }
QScrollArea* SceneInspectorDock::analysisScrollArea() const { return analysisTab_.scrollArea; }
QVBoxLayout* SceneInspectorDock::analysisLayout() const { return analysisTab_.layout; }
QScrollArea* SceneInspectorDock::navigationScrollArea() const { return navigationTab_.scrollArea; }
QVBoxLayout* SceneInspectorDock::navigationLayout() const { return navigationTab_.layout; }

SceneInspectorDock::TabPage SceneInspectorDock::createTabPage(const QString& objectName)
{
    TabPage tabPage;
    tabPage.scrollArea = new QScrollArea(tabWidget_);
    tabPage.scrollArea->setWidgetResizable(true);
    tabPage.scrollArea->setFrameShape(QFrame::NoFrame);
    tabPage.scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tabPage.scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* page = new QWidget(tabPage.scrollArea);
    page->setObjectName(objectName);
    page->setMinimumWidth(0);
    tabPage.layout = new QVBoxLayout(page);
    tabPage.layout->setContentsMargins(14, 14, 14, 14);
    tabPage.layout->setSpacing(12);
    tabPage.layout->setSizeConstraint(QLayout::SetDefaultConstraint);
    tabPage.scrollArea->setWidget(page);
    return tabPage;
}
