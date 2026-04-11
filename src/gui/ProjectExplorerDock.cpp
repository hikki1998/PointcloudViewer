#include "gui/ProjectExplorerDock.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QLineEdit>
#include <QSize>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

ProjectExplorerDock::ProjectExplorerDock(QWidget* parent)
    : QDockWidget(parent)
{
    setObjectName(QStringLiteral("projectExplorerDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(280);

    auto* projectDockContents = new QWidget(this);
    projectDockContents->setObjectName(QStringLiteral("projectExplorerSurface"));
    auto* projectDockLayout = new QVBoxLayout(projectDockContents);
    projectDockLayout->setContentsMargins(12, 12, 12, 12);
    projectDockLayout->setSpacing(10);

    searchEdit_ = new QLineEdit(projectDockContents);
    searchEdit_->setObjectName(QStringLiteral("projectExplorerSearch"));
    searchEdit_->setClearButtonEnabled(true);

    toolBar_ = new QToolBar(projectDockContents);
    toolBar_->setObjectName(QStringLiteral("projectExplorerToolBar"));
    toolBar_->setIconSize(QSize(16, 16));
    toolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar_->setMovable(false);
    toolBar_->setFloatable(false);

    treeWidget_ = new QTreeWidget(projectDockContents);
    treeWidget_->setObjectName(QStringLiteral("projectExplorerTree"));
    treeWidget_->setColumnCount(1);
    treeWidget_->setHeaderHidden(true);
    treeWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    treeWidget_->setAlternatingRowColors(false);
    treeWidget_->setRootIsDecorated(true);
    treeWidget_->setUniformRowHeights(true);
    treeWidget_->setAnimated(true);
    treeWidget_->setIndentation(18);
    treeWidget_->setFrameShape(QFrame::NoFrame);
    treeWidget_->setContextMenuPolicy(Qt::CustomContextMenu);

    projectDockLayout->addWidget(searchEdit_);
    projectDockLayout->addWidget(toolBar_);
    projectDockLayout->addWidget(treeWidget_, 1);

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

    setWidget(projectDockContents);
    retranslateUi();
}

void ProjectExplorerDock::retranslateUi()
{
    setWindowTitle(tr("Project Explorer"));
    if (searchEdit_ != nullptr) {
        searchEdit_->setPlaceholderText(tr("Filter point clouds, images, or trajectories"));
    }
}

QLineEdit* ProjectExplorerDock::searchEdit() const
{
    return searchEdit_;
}

QToolBar* ProjectExplorerDock::toolBar() const
{
    return toolBar_;
}

QTreeWidget* ProjectExplorerDock::treeWidget() const
{
    return treeWidget_;
}
