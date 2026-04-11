#include "gui/ProfileClassificationDock.h"

#include <QVBoxLayout>
#include <QWidget>

ProfileClassificationDock::ProfileClassificationDock(QWidget* parent)
    : QDockWidget(parent)
{
    setObjectName(QStringLiteral("profileClassificationDock"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(320);

    surface_ = new QWidget(this);
    surface_->setObjectName(QStringLiteral("profileClassificationSurface"));
    auto* surfaceLayout = new QVBoxLayout(surface_);
    surfaceLayout->setContentsMargins(10, 10, 10, 10);
    surfaceLayout->setSpacing(8);
    surfaceLayout->addStretch(1);
    surface_->setStyleSheet(QStringLiteral(
        "QWidget#profileClassificationSurface {"
        "background-color: #f3f7fc;"
        "border: 1px solid #d6e0eb;"
        "border-radius: 10px;"
        "}"
        "QGroupBox {"
        "font-weight: 600;"
        "background-color: #ffffff;"
        "color: #1f2937;"
        "border: 1px solid #d6dde8;"
        "border-radius: 8px;"
        "margin-top: 8px;"
        "padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 8px;"
        "padding: 0 4px;"
        "background-color: #f3f7fc;"
        "color: #334155;"
        "}"
        "QLabel {"
        "color: #1f2937;"
        "}"
        "QListWidget {"
        "background-color: #ffffff;"
        "border: 1px solid #d6dde8;"
        "border-radius: 6px;"
        "alternate-background-color: #f8fafc;"
        "color: #0f172a;"
        "}"
        "QListWidget::item:selected {"
        "background-color: #dbeafe;"
        "color: #0f172a;"
        "}"
        "QScrollBar:vertical {"
        "background: #edf2f7;"
        "width: 12px;"
        "margin: 2px;"
        "border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical {"
        "background: #94a3b8;"
        "min-height: 28px;"
        "border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "background: #64748b;"
        "}"
        "QScrollBar:horizontal {"
        "background: #edf2f7;"
        "height: 12px;"
        "margin: 2px;"
        "border-radius: 6px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "background: #94a3b8;"
        "min-width: 28px;"
        "border-radius: 6px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "background: #64748b;"
        "}"
        "QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page {"
        "background: transparent;"
        "border: none;"
        "}"
        "QPushButton {"
        "background-color: #ffffff;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "padding: 6px 10px;"
        "color: #0f172a;"
        "}"
        "QPushButton:hover {"
        "border-color: #94a3b8;"
        "background-color: #f8fafc;"
        "}"
        "QPushButton:disabled {"
        "background-color: #f1f5f9;"
        "border-color: #e2e8f0;"
        "color: #94a3b8;"
        "}"
        "QComboBox {"
        "background-color: #ffffff;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "min-height: 30px;"
        "padding: 4px 10px;"
        "padding-right: 26px;"
        "color: #0f172a;"
        "}"
        "QComboBox:hover {"
        "border-color: #94a3b8;"
        "}"
        "QComboBox::drop-down {"
        "subcontrol-origin: padding;"
        "subcontrol-position: top right;"
        "width: 22px;"
        "border: none;"
        "}"
        "QComboBox QAbstractItemView {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "padding: 4px 0;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "min-height: 24px;"
        "padding: 4px 10px;"
        "}"));

    setWidget(surface_);
    retranslateUi();
}

void ProfileClassificationDock::retranslateUi()
{
    setWindowTitle(tr("Profile Classification"));
}

void ProfileClassificationDock::setContentWidget(QWidget* widget)
{
    if (surface_ == nullptr || widget == contentWidget_) {
        return;
    }

    auto* surfaceLayout = qobject_cast<QVBoxLayout*>(surface_->layout());
    if (surfaceLayout == nullptr) {
        return;
    }

    if (contentWidget_ != nullptr) {
        surfaceLayout->removeWidget(contentWidget_);
    }

    contentWidget_ = widget;
    if (contentWidget_ != nullptr) {
        surfaceLayout->insertWidget(0, contentWidget_);
    }
}
