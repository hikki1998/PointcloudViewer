#include "gui/ProjectExplorerController.h"

#include <QAction>
#include <QLineEdit>
#include <QToolBar>
#include <QTreeWidget>

#include "gui/ProjectExplorerDock.h"

ProjectExplorerController::ProjectExplorerController(
    ProjectExplorerDock* dock,
    QAction* openAction,
    QAction* addPointCloudAction,
    QAction* removeDatasetAction,
    QAction* locateDatasetAction,
    QAction* copyDatasetPathAction,
    QAction* expandProjectTreeAction,
    QAction* collapseProjectTreeAction,
    QObject* parent)
    : QObject(parent)
    , dock_(dock)
    , expandProjectTreeAction_(expandProjectTreeAction)
    , collapseProjectTreeAction_(collapseProjectTreeAction)
{
    if (dock_ == nullptr) {
        return;
    }

    if (QToolBar* toolbar = dock_->toolBar()) {
        toolbar->addAction(openAction);
        toolbar->addAction(addPointCloudAction);
        toolbar->addAction(removeDatasetAction);
        toolbar->addSeparator();
        toolbar->addAction(locateDatasetAction);
        toolbar->addAction(copyDatasetPathAction);
        toolbar->addSeparator();
        toolbar->addAction(expandProjectTreeAction_);
        toolbar->addAction(collapseProjectTreeAction_);
    }

    if (QLineEdit* search = dock_->searchEdit()) {
        connect(search, &QLineEdit::textChanged, this, &ProjectExplorerController::searchTextChanged);
    }
    if (openAction != nullptr) {
        connect(openAction, &QAction::triggered, this, &ProjectExplorerController::openRequested);
    }
    if (addPointCloudAction != nullptr) {
        connect(addPointCloudAction, &QAction::triggered, this, &ProjectExplorerController::addPointCloudRequested);
    }
    if (removeDatasetAction != nullptr) {
        connect(removeDatasetAction, &QAction::triggered, this, &ProjectExplorerController::removeDatasetRequested);
    }
    if (locateDatasetAction != nullptr) {
        connect(locateDatasetAction, &QAction::triggered, this, &ProjectExplorerController::locateSelectedRequested);
    }
    if (copyDatasetPathAction != nullptr) {
        connect(copyDatasetPathAction, &QAction::triggered, this, &ProjectExplorerController::copySelectedPathRequested);
    }
    if (QTreeWidget* tree = dock_->treeWidget()) {
        connect(tree, &QTreeWidget::itemChanged, this, &ProjectExplorerController::itemChanged);
        connect(tree, &QTreeWidget::currentItemChanged, this, &ProjectExplorerController::currentItemChanged);
        connect(tree, &QTreeWidget::itemDoubleClicked, this, &ProjectExplorerController::itemDoubleClicked);
        connect(tree, &QTreeWidget::customContextMenuRequested, this, &ProjectExplorerController::customContextMenuRequested);
    }
    if (expandProjectTreeAction_ != nullptr) {
        connect(expandProjectTreeAction_, &QAction::triggered, this, [this]() {
            if (dock_ != nullptr && dock_->treeWidget() != nullptr) {
                dock_->treeWidget()->expandAll();
            }
        });
    }
    if (collapseProjectTreeAction_ != nullptr) {
        connect(collapseProjectTreeAction_, &QAction::triggered, this, [this]() {
            if (dock_ == nullptr || dock_->treeWidget() == nullptr) {
                return;
            }

            dock_->treeWidget()->collapseAll();
            if (dock_->treeWidget()->topLevelItemCount() > 0) {
                dock_->treeWidget()->topLevelItem(0)->setExpanded(true);
            }
        });
    }
}

void ProjectExplorerController::retranslateUi()
{
    if (dock_ != nullptr) {
        dock_->retranslateUi();
    }
}

void ProjectExplorerController::refreshFilter()
{
    if (dock_ == nullptr || dock_->treeWidget() == nullptr) {
        return;
    }

    const QString filterText = dock_->searchEdit() != nullptr ? dock_->searchEdit()->text().trimmed() : QString();
    const auto updateItemVisibility = [&filterText](auto&& self, QTreeWidgetItem* item) -> bool {
        if (item == nullptr) {
            return false;
        }

        bool hasVisibleChild = false;
        for (int childIndex = 0; childIndex < item->childCount(); ++childIndex) {
            hasVisibleChild = self(self, item->child(childIndex)) || hasVisibleChild;
        }

        const QString itemText = item->text(0) + QLatin1Char('\n') + item->toolTip(0);
        const bool matchesSelf = filterText.isEmpty() || itemText.contains(filterText, Qt::CaseInsensitive);
        const QString itemType = item->data(0, Qt::UserRole).toString();
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

    for (int rootIndex = 0; rootIndex < dock_->treeWidget()->topLevelItemCount(); ++rootIndex) {
        updateItemVisibility(updateItemVisibility, dock_->treeWidget()->topLevelItem(rootIndex));
    }

    if (QTreeWidgetItem* currentItem = dock_->treeWidget()->currentItem();
        currentItem != nullptr && currentItem->isHidden()) {
        dock_->treeWidget()->setCurrentItem(nullptr);
    }
}

ProjectExplorerDock* ProjectExplorerController::dock() const
{
    return dock_;
}

QLineEdit* ProjectExplorerController::searchEdit() const
{
    return dock_ != nullptr ? dock_->searchEdit() : nullptr;
}

QToolBar* ProjectExplorerController::toolBar() const
{
    return dock_ != nullptr ? dock_->toolBar() : nullptr;
}

QTreeWidget* ProjectExplorerController::treeWidget() const
{
    return dock_ != nullptr ? dock_->treeWidget() : nullptr;
}
