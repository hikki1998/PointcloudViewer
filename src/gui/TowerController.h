#pragma once

#include <QObject>

#include <functional>

class QAction;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QString;

class TowerController final : public QObject
{
    Q_OBJECT

public:
    using VoidCallback = std::function<void()>;
    using BoolCallback = std::function<void(bool)>;
    using IntCallback = std::function<void(int)>;
    using TowerNameEditedCallback = std::function<void(int, const QString&)>;

    TowerController(
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
        QObject* parent = nullptr);
};
