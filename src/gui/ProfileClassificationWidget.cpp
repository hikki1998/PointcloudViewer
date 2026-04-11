#include "gui/ProfileClassificationWidget.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>

#include "gui/PointCloudViewer.h"

#include "ui_ProfileClassificationWidget.h"

ProfileClassificationWidget::ProfileClassificationWidget(QWidget* parent)
    : QGroupBox(parent)
    , ui_(new Ui::ProfileClassificationWidget())
{
    ui_->setupUi(this);

    if (ui_->sourceListWidget != nullptr) {
        ui_->sourceListWidget->setSelectionMode(QAbstractItemView::NoSelection);
        ui_->sourceListWidget->setAlternatingRowColors(true);
        ui_->sourceListWidget->setMinimumHeight(220);
    }
    if (ui_->targetListWidget != nullptr) {
        ui_->targetListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        ui_->targetListWidget->setAlternatingRowColors(true);
        ui_->targetListWidget->setMinimumHeight(180);
    }
    if (ui_->statusLabel != nullptr) {
        ui_->statusLabel->setWordWrap(true);
        ui_->statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    retranslateUi();
}

ProfileClassificationWidget::~ProfileClassificationWidget()
{
    delete ui_;
}

void ProfileClassificationWidget::retranslateUi()
{
    if (ui_ != nullptr) {
        ui_->retranslateUi(this);
    }
    rebuildModeComboItems();
}

QPushButton* ProfileClassificationWidget::toggleButton() const { return ui_ != nullptr ? ui_->toggleButton : nullptr; }
QPushButton* ProfileClassificationWidget::selectAllButton() const { return ui_ != nullptr ? ui_->selectAllButton : nullptr; }
QPushButton* ProfileClassificationWidget::clearSelectionButton() const { return ui_ != nullptr ? ui_->clearSelectionButton : nullptr; }
QPushButton* ProfileClassificationWidget::undoButton() const { return ui_ != nullptr ? ui_->undoButton : nullptr; }
QPushButton* ProfileClassificationWidget::redoButton() const { return ui_ != nullptr ? ui_->redoButton : nullptr; }
QPushButton* ProfileClassificationWidget::clearEditsButton() const { return ui_ != nullptr ? ui_->clearEditsButton : nullptr; }
QPushButton* ProfileClassificationWidget::saveButton() const { return ui_ != nullptr ? ui_->saveButton : nullptr; }
QLabel* ProfileClassificationWidget::modeLabel() const { return ui_ != nullptr ? ui_->modeLabel : nullptr; }
QLabel* ProfileClassificationWidget::statusLabel() const { return ui_ != nullptr ? ui_->statusLabel : nullptr; }
QComboBox* ProfileClassificationWidget::modeComboBox() const { return ui_ != nullptr ? ui_->modeComboBox : nullptr; }
QListWidget* ProfileClassificationWidget::sourceListWidget() const { return ui_ != nullptr ? ui_->sourceListWidget : nullptr; }
QListWidget* ProfileClassificationWidget::targetListWidget() const { return ui_ != nullptr ? ui_->targetListWidget : nullptr; }

void ProfileClassificationWidget::rebuildModeComboItems()
{
    QComboBox* modeCombo = modeComboBox();
    if (modeCombo == nullptr) {
        return;
    }

    const int selectedMode = modeCombo->currentData().toInt();
    const QSignalBlocker blocker(modeCombo);
    modeCombo->clear();
    modeCombo->addItem(
        tr("Rectangle Selection"),
        static_cast<int>(ProfileClassificationSelectionMode::Rectangle));
    modeCombo->addItem(
        tr("Polygon Selection"),
        static_cast<int>(ProfileClassificationSelectionMode::Polygon));
    const int selectedModeIndex = modeCombo->findData(selectedMode);
    modeCombo->setCurrentIndex(selectedModeIndex >= 0 ? selectedModeIndex : 0);
}
