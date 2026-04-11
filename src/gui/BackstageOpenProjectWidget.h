#pragma once

#include <QWidget>

namespace Ui
{
class BackstageOpenProjectWidget;
}

class QGroupBox;
class QLineEdit;
class QListWidget;
class QPushButton;

class BackstageOpenProjectWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit BackstageOpenProjectWidget(QWidget* parent = nullptr);
    ~BackstageOpenProjectWidget() override;

    void retranslateUi();

    QGroupBox* recentProjectsGroup() const;
    QGroupBox* projectFileGroup() const;
    QListWidget* recentProjectsListWidget() const;
    QLineEdit* projectPathLineEdit() const;
    QPushButton* browseButton() const;
    QPushButton* openButton() const;

private:
    Ui::BackstageOpenProjectWidget* ui_ = nullptr;
};
