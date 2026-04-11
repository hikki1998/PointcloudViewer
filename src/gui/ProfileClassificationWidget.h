#pragma once

#include <QGroupBox>

namespace Ui
{
class ProfileClassificationWidget;
}

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

class ProfileClassificationWidget final : public QGroupBox
{
    Q_OBJECT

public:
    explicit ProfileClassificationWidget(QWidget* parent = nullptr);
    ~ProfileClassificationWidget() override;

    void retranslateUi();

    QPushButton* toggleButton() const;
    QPushButton* selectAllButton() const;
    QPushButton* clearSelectionButton() const;
    QPushButton* undoButton() const;
    QPushButton* redoButton() const;
    QPushButton* clearEditsButton() const;
    QPushButton* saveButton() const;
    QLabel* modeLabel() const;
    QLabel* statusLabel() const;
    QComboBox* modeComboBox() const;
    QListWidget* sourceListWidget() const;
    QListWidget* targetListWidget() const;

private:
    void rebuildModeComboItems();

    Ui::ProfileClassificationWidget* ui_ = nullptr;
};
