#include "gui/RouteDetailsDock.h"

#include <QLayout>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

RouteDetailsDock::RouteDetailsDock(QWidget* parent)
    : QDockWidget(parent)
{
    setObjectName(QStringLiteral("routeDetailsDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(460);

    tabWidget_ = new QTabWidget(this);
    tabWidget_->setObjectName(QStringLiteral("routeDetailsTabs"));
    tabWidget_->setDocumentMode(true);
    tabWidget_->setMovable(false);

    waypointsTab_ = createTabPage(QStringLiteral("routeDetailsWaypointsPage"));
    partPointsTab_ = createTabPage(QStringLiteral("routeDetailsPartPointsPage"));
    routeQaTab_ = createTabPage(QStringLiteral("routeDetailsQaPage"));

    tabWidget_->addTab(waypointsTab_.page, QString());
    tabWidget_->addTab(partPointsTab_.page, QString());
    tabWidget_->addTab(routeQaTab_.page, QString());

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
        "QComboBox {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "padding: 4px 28px 4px 10px;"
        "min-height: 30px;"
        "}"
        "QComboBox:hover {"
        "border-color: #94a3b8;"
        "}"
        "QComboBox::drop-down {"
        "subcontrol-origin: padding;"
        "subcontrol-position: top right;"
        "width: 24px;"
        "border: none;"
        "}"
        "QComboBox QAbstractItemView {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "outline: none;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "min-height: 24px;"
        "padding: 4px 10px;"
        "}");

    setWidget(tabWidget_);
    retranslateUi();
}

void RouteDetailsDock::retranslateUi()
{
    setWindowTitle(tr("Route Waypoints"));
    if (tabWidget_ != nullptr && tabWidget_->count() >= 3) {
        tabWidget_->setTabText(0, tr("Route Waypoints"));
        tabWidget_->setTabText(1, tr("Route Part Points"));
        tabWidget_->setTabText(2, tr("Route QA"));
    }
}

QTabWidget* RouteDetailsDock::tabWidget() const
{
    return tabWidget_;
}

QVBoxLayout* RouteDetailsDock::waypointsLayout() const
{
    return waypointsTab_.layout;
}

QVBoxLayout* RouteDetailsDock::partPointsLayout() const
{
    return partPointsTab_.layout;
}

QVBoxLayout* RouteDetailsDock::routeQaLayout() const
{
    return routeQaTab_.layout;
}

RouteDetailsDock::TabPage RouteDetailsDock::createTabPage(const QString& objectName)
{
    TabPage tabPage;
    tabPage.page = new QWidget(tabWidget_);
    tabPage.page->setObjectName(objectName);
    tabPage.layout = new QVBoxLayout(tabPage.page);
    tabPage.layout->setContentsMargins(10, 10, 10, 10);
    tabPage.layout->setSpacing(8);
    tabPage.layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    return tabPage;
}
