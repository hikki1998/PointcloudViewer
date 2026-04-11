#pragma once

#include <QObject>

class QAction;
class QLineEdit;
class QPoint;
class QToolBar;
class QTreeWidget;
class QTreeWidgetItem;
class ProjectExplorerDock;

class ProjectExplorerController final : public QObject
{
    Q_OBJECT

public:
    ProjectExplorerController(
        ProjectExplorerDock* dock,
        QAction* openAction,
        QAction* addPointCloudAction,
        QAction* removeDatasetAction,
        QAction* locateDatasetAction,
        QAction* copyDatasetPathAction,
        QAction* expandProjectTreeAction,
        QAction* collapseProjectTreeAction,
        QObject* parent = nullptr);

    void retranslateUi();
    void refreshFilter();
    ProjectExplorerDock* dock() const;
    QLineEdit* searchEdit() const;
    QToolBar* toolBar() const;
    QTreeWidget* treeWidget() const;

signals:
    void openRequested();
    void addPointCloudRequested();
    void removeDatasetRequested();
    void locateSelectedRequested();
    void copySelectedPathRequested();
    void searchTextChanged(const QString& text);
    void itemChanged(QTreeWidgetItem* item, int column);
    void currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void itemDoubleClicked(QTreeWidgetItem* item, int column);
    void customContextMenuRequested(const QPoint& pos);

private:
    ProjectExplorerDock* dock_ = nullptr;
    QAction* expandProjectTreeAction_ = nullptr;
    QAction* collapseProjectTreeAction_ = nullptr;
};
