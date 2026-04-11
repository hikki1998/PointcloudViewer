#pragma once

#include <QObject>

#include <QList>

#include <functional>

class QAction;
class PointCloudViewer;
class ProfileClassificationWidget;
class QString;

class ProfileClassificationController final : public QObject
{
    Q_OBJECT

public:
    using ClassificationNameResolver = std::function<QString(int)>;

    ProfileClassificationController(
        ProfileClassificationWidget* panel,
        PointCloudViewer* viewer,
        QAction* profileClassificationAction,
        QAction* saveProfileClassificationEditsAction,
        QAction* undoProfileClassificationAction,
        QAction* redoProfileClassificationAction,
        QAction* clearProfileClassificationEditsAction,
        ClassificationNameResolver classificationNameResolver,
        QObject* parent = nullptr);

    void initializeClassificationItems(const QList<int>& classificationCodes);
    void retranslateUi();
    void refreshPanel(bool classificationEditsDirty);

signals:
    void saveRequested();
    void modeChanged(bool enabled);
    void editsDirtyChanged(bool dirty);
    void stateChanged();

private:
    ProfileClassificationWidget* panel_ = nullptr;
    PointCloudViewer* viewer_ = nullptr;
    QAction* profileClassificationAction_ = nullptr;
    QAction* saveProfileClassificationEditsAction_ = nullptr;
    QAction* undoProfileClassificationAction_ = nullptr;
    QAction* redoProfileClassificationAction_ = nullptr;
    QAction* clearProfileClassificationEditsAction_ = nullptr;
    ClassificationNameResolver classificationNameResolver_;
    bool classificationEditsDirty_ = false;
};