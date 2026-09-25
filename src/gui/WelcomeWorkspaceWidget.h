#pragma once

#include <QImage>
#include <QWidget>

#include <QString>
#include <QList>

class QFrame;
class QGridLayout;
class QLabel;
class QListWidget;
class QResizeEvent;
class QPushButton;

struct WelcomeWorkspaceItem
{
    QString title;
    QString details;
    QString location;
    QString filePath;
    QImage thumbnail;
    bool missing = false;
};

class WelcomeWorkspaceWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeWorkspaceWidget(QWidget* parent = nullptr);

    void setRecentProjects(const QList<WelcomeWorkspaceItem>& items);
    void setRecentDataFiles(const QList<WelcomeWorkspaceItem>& items);
    void retranslateUi();

signals:
    void openProjectRequested();
    void openDataRequested();
    void addDataRequested();
    void recentProjectRequested(const QString& filePath);
    void recentDataRequested(const QString& filePath);
    void locateRecentItemRequested(const QString& filePath);
    void removeRecentProjectRequested(const QString& filePath);
    void removeRecentDataRequested(const QString& filePath);
    void appendRecentDataRequested(const QString& filePath);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void populateList(QListWidget* listWidget, const QList<WelcomeWorkspaceItem>& items, const QString& emptyText);
    void showRecentItemMenu(QListWidget* listWidget, const QPoint& position, bool projectItem);

    QLabel* titleLabel_ = nullptr;
    QLabel* subtitleLabel_ = nullptr;
    QLabel* recentProjectsLabel_ = nullptr;
    QLabel* recentDataLabel_ = nullptr;
    QPushButton* openProjectButton_ = nullptr;
    QPushButton* openDataButton_ = nullptr;
    QPushButton* addDataButton_ = nullptr;
    QGridLayout* sectionsLayout_ = nullptr;
    QFrame* recentProjectsCard_ = nullptr;
    QFrame* recentDataCard_ = nullptr;
    QListWidget* recentProjectsList_ = nullptr;
    QListWidget* recentDataList_ = nullptr;
    QList<WelcomeWorkspaceItem> recentProjects_;
    QList<WelcomeWorkspaceItem> recentDataFiles_;
    bool cardsStacked_ = true;
};
