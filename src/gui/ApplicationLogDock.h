#pragma once

#include <QDockWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QTextEdit;

class ApplicationLogDock final : public QDockWidget
{
    Q_OBJECT

public:
    explicit ApplicationLogDock(QWidget* parent = nullptr);

    void retranslateUi();
    void refreshEntries();

    int totalEntryCount() const;
    int visibleEntryCount() const;
    int selectedFilterLevel() const;
    void setSelectedFilterLevel(int level);
    QString searchKeyword() const;
    void setSearchKeyword(const QString& keyword);
    bool autoScrollEnabled() const;
    void setAutoScrollEnabled(bool enabled);
    QLineEdit* searchLineEdit() const;
    QTextEdit* logTextEdit() const;

signals:
    void filterStateChanged();
    void autoScrollToggled(bool enabled);
    void exportRequested();
    void entriesClearedByUser();

private:
    QComboBox* logLevelFilterComboBox_ = nullptr;
    QLineEdit* logSearchLineEdit_ = nullptr;
    QCheckBox* logAutoScrollCheckBox_ = nullptr;
    QPushButton* logClearButton_ = nullptr;
    QPushButton* logExportButton_ = nullptr;
    QLabel* logStatsLabel_ = nullptr;
    QTextEdit* logTextEdit_ = nullptr;
    int totalEntryCount_ = 0;
    int visibleEntryCount_ = 0;
};
