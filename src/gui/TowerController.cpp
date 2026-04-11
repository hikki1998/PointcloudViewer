#include "gui/TowerController.h"

#include <QAction>
#include <QComboBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QTableWidgetItem>

TowerController::TowerController(
    QAction* startTowerEditAction,
    QAction* finishTowerEditAction,
    QAction* addTowerAction,
    QAction* insertTowerAction,
    QAction* moveTowerAction,
    QAction* editCurrentTowerAction,
    QAction* focusTowerAction,
    QAction* removeTowerAction,
    QAction* clearTowersAction,
    QAction* cancelTowerToolAction,
    QAction* importTowerFileAction,
    QAction* saveTowerFileAction,
    QAction* saveTowerFileAsAction,
    QAction* reloadTowerFileAction,
    QAction* showTowerXAction,
    QAction* showTowerYAction,
    QAction* showTowerZAction,
    QTableWidget* towerTableWidget,
    QLineEdit* towerCodeEdit,
    QLineEdit* towerLineNameEdit,
    QLineEdit* towerVoltageLevelEdit,
    QComboBox* towerTypeComboBox,
    QLineEdit* towerStructureTypeEdit,
    QLineEdit* towerInspectionDateEdit,
    QLineEdit* towerStatusEdit,
    QPlainTextEdit* towerNotesEdit,
    VoidCallback startTowerEditing,
    VoidCallback finishTowerEditing,
    VoidCallback beginAddTower,
    VoidCallback beginInsertTower,
    VoidCallback beginMoveTower,
    VoidCallback editCurrentTower,
    VoidCallback focusSelectedTower,
    VoidCallback removeSelectedTower,
    VoidCallback clearAllTowers,
    VoidCallback cancelTowerTool,
    VoidCallback importTowerFileFromDialog,
    VoidCallback saveTowerFileToLinkedPath,
    VoidCallback saveTowerFileAs,
    VoidCallback reloadTowerFileFromLinkedPath,
    BoolCallback showTowerXChanged,
    BoolCallback showTowerYChanged,
    BoolCallback showTowerZChanged,
    IntCallback selectionChanged,
    TowerNameEditedCallback towerNameEdited,
    VoidCallback commitTowerDetails,
    QObject* parent)
    : QObject(parent)
{
    auto connectAction = [this](QAction* action, const VoidCallback& callback) {
        if (action == nullptr) {
            return;
        }
        connect(action, &QAction::triggered, this, [callback]() {
            if (callback) {
                callback();
            }
        });
    };

    connectAction(startTowerEditAction, startTowerEditing);
    connectAction(finishTowerEditAction, finishTowerEditing);
    connectAction(addTowerAction, beginAddTower);
    connectAction(insertTowerAction, beginInsertTower);
    connectAction(moveTowerAction, beginMoveTower);
    connectAction(editCurrentTowerAction, editCurrentTower);
    connectAction(focusTowerAction, focusSelectedTower);
    connectAction(removeTowerAction, removeSelectedTower);
    connectAction(clearTowersAction, clearAllTowers);
    connectAction(cancelTowerToolAction, cancelTowerTool);
    connectAction(importTowerFileAction, importTowerFileFromDialog);
    connectAction(saveTowerFileAction, saveTowerFileToLinkedPath);
    connectAction(saveTowerFileAsAction, saveTowerFileAs);
    connectAction(reloadTowerFileAction, reloadTowerFileFromLinkedPath);

    if (showTowerXAction != nullptr) {
        connect(showTowerXAction, &QAction::toggled, this, [towerTableWidget, showTowerXChanged](bool checked) {
            if (towerTableWidget != nullptr) {
                towerTableWidget->setColumnHidden(2, !checked);
            }
            if (showTowerXChanged) {
                showTowerXChanged(checked);
            }
        });
    }
    if (showTowerYAction != nullptr) {
        connect(showTowerYAction, &QAction::toggled, this, [towerTableWidget, showTowerYChanged](bool checked) {
            if (towerTableWidget != nullptr) {
                towerTableWidget->setColumnHidden(3, !checked);
            }
            if (showTowerYChanged) {
                showTowerYChanged(checked);
            }
        });
    }
    if (showTowerZAction != nullptr) {
        connect(showTowerZAction, &QAction::toggled, this, [towerTableWidget, showTowerZChanged](bool checked) {
            if (towerTableWidget != nullptr) {
                towerTableWidget->setColumnHidden(4, !checked);
            }
            if (showTowerZChanged) {
                showTowerZChanged(checked);
            }
        });
    }

    if (towerTableWidget != nullptr && towerTableWidget->horizontalHeader() != nullptr) {
        towerTableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(towerTableWidget, &QTableWidget::customContextMenuRequested, this, [
            towerTableWidget,
            focusTowerAction,
            removeTowerAction,
            editCurrentTowerAction,
            moveTowerAction,
            insertTowerAction,
            addTowerAction,
            clearTowersAction,
            cancelTowerToolAction,
            importTowerFileAction,
            saveTowerFileAction,
            saveTowerFileAsAction,
            reloadTowerFileAction
        ](const QPoint& pos) {
            if (towerTableWidget == nullptr) {
                return;
            }

            const QModelIndex index = towerTableWidget->indexAt(pos);
            if (index.isValid()) {
                const int row = index.row();
                if (row >= 0 && row < towerTableWidget->rowCount()) {
                    towerTableWidget->setCurrentCell(row, 1);
                }
            }

            QMenu rowMenu(towerTableWidget);
            rowMenu.addAction(focusTowerAction);
            rowMenu.addAction(removeTowerAction);
            rowMenu.addSeparator();
            rowMenu.addAction(editCurrentTowerAction);
            rowMenu.addAction(moveTowerAction);
            rowMenu.addAction(insertTowerAction);
            rowMenu.addSeparator();
            rowMenu.addAction(addTowerAction);
            rowMenu.addAction(clearTowersAction);
            rowMenu.addAction(cancelTowerToolAction);
            rowMenu.addSeparator();
            rowMenu.addAction(importTowerFileAction);
            rowMenu.addAction(saveTowerFileAction);
            rowMenu.addAction(saveTowerFileAsAction);
            rowMenu.addAction(reloadTowerFileAction);
            rowMenu.exec(towerTableWidget->viewport()->mapToGlobal(pos));
        });

        towerTableWidget->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(towerTableWidget->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, [
            towerTableWidget,
            showTowerXAction,
            showTowerYAction,
            showTowerZAction
        ](const QPoint& pos) {
            if (towerTableWidget == nullptr || towerTableWidget->horizontalHeader() == nullptr) {
                return;
            }

            QMenu columnMenu(towerTableWidget);
            columnMenu.addAction(showTowerXAction);
            columnMenu.addAction(showTowerYAction);
            columnMenu.addAction(showTowerZAction);
            columnMenu.exec(towerTableWidget->horizontalHeader()->mapToGlobal(pos));
        });
    }

    if (towerTableWidget != nullptr) {
        connect(towerTableWidget, &QTableWidget::currentCellChanged, this, [selectionChanged](int currentRow, int, int, int) {
            if (selectionChanged) {
                selectionChanged(currentRow);
            }
        });

        connect(towerTableWidget, &QTableWidget::cellChanged, this, [towerTableWidget, towerNameEdited](int row, int column) {
            if (towerTableWidget == nullptr || towerNameEdited == nullptr || column != 1) {
                return;
            }

            QTableWidgetItem* item = towerTableWidget->item(row, column);
            if (item == nullptr) {
                return;
            }

            towerNameEdited(row, item->text());
        });
    }

    if (towerCodeEdit != nullptr) {
        connect(towerCodeEdit, &QLineEdit::editingFinished, this, [commitTowerDetails]() {
            if (commitTowerDetails) {
                commitTowerDetails();
            }
        });
    }
    if (towerLineNameEdit != nullptr) {
        connect(towerLineNameEdit, &QLineEdit::editingFinished, this, [commitTowerDetails]() {
            if (commitTowerDetails) {
                commitTowerDetails();
            }
        });
    }
    if (towerVoltageLevelEdit != nullptr) {
        connect(towerVoltageLevelEdit, &QLineEdit::editingFinished, this, [commitTowerDetails]() {
            if (commitTowerDetails) {
                commitTowerDetails();
            }
        });
    }
    if (towerTypeComboBox != nullptr) {
        connect(towerTypeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [commitTowerDetails](int) {
            if (commitTowerDetails) {
                commitTowerDetails();
            }
        });
    }
    if (towerStructureTypeEdit != nullptr) {
        connect(towerStructureTypeEdit, &QLineEdit::editingFinished, this, [commitTowerDetails]() {
            if (commitTowerDetails) {
                commitTowerDetails();
            }
        });
    }
    if (towerInspectionDateEdit != nullptr) {
        connect(towerInspectionDateEdit, &QLineEdit::editingFinished, this, [commitTowerDetails]() {
            if (commitTowerDetails) {
                commitTowerDetails();
            }
        });
    }
    if (towerStatusEdit != nullptr) {
        connect(towerStatusEdit, &QLineEdit::editingFinished, this, [commitTowerDetails]() {
            if (commitTowerDetails) {
                commitTowerDetails();
            }
        });
    }
    if (towerNotesEdit != nullptr) {
        connect(towerNotesEdit, &QPlainTextEdit::textChanged, this, [commitTowerDetails]() {
            if (commitTowerDetails) {
                commitTowerDetails();
            }
        });
    }
}
