#pragma once

#include <QWidget>

namespace Ui
{
class TowerEditorWidget;
}

class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QToolBar;

class TowerEditorWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TowerEditorWidget(QWidget* parent = nullptr);
    ~TowerEditorWidget() override;

    void retranslateUi();

    QToolBar* toolBar() const;
    QLabel* towerCountLabel() const;
    QLabel* towerToolStatusLabel() const;
    QTableWidget* towerTable() const;
    QGroupBox* towerDetailsGroupBox() const;
    QFormLayout* towerDetailsLayout() const;
    QLineEdit* towerCodeEdit() const;
    QLineEdit* towerLineNameEdit() const;
    QLineEdit* towerVoltageLevelEdit() const;
    QComboBox* towerTypeComboBox() const;
    QLineEdit* towerStructureTypeEdit() const;
    QLineEdit* towerInspectionDateEdit() const;
    QLineEdit* towerStatusEdit() const;
    QPlainTextEdit* towerNotesEdit() const;

private:
    Ui::TowerEditorWidget* ui_ = nullptr;
};
