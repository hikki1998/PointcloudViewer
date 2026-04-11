#pragma once

#include <QDockWidget>

class QLineEdit;
class QToolBar;
class QTreeWidget;

class ProjectExplorerDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit ProjectExplorerDock(QWidget* parent = nullptr);

    void retranslateUi();
    QLineEdit* searchEdit() const;
    QToolBar* toolBar() const;
    QTreeWidget* treeWidget() const;

private:
    QLineEdit* searchEdit_ = nullptr;
    QToolBar* toolBar_ = nullptr;
    QTreeWidget* treeWidget_ = nullptr;
};
