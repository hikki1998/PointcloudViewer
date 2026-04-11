#include "gui/SpanProfileDock.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include "gui/ProfilePlotWidget.h"

SpanProfileDock::SpanProfileDock(QWidget* parent)
    : QDockWidget(parent)
{
    setObjectName(QStringLiteral("spanProfileDock"));
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    setMinimumHeight(220);

    auto* surface = new QWidget(this);
    surface->setObjectName(QStringLiteral("spanProfileSurface"));
    auto* profileLayout = new QVBoxLayout(surface);
    profileLayout->setContentsMargins(12, 12, 12, 12);
    profileLayout->setSpacing(8);

    titleLabel_ = new QLabel(surface);
    titleLabel_->setObjectName(QStringLiteral("spanProfileTitleLabel"));
    titleLabel_->setStyleSheet(QStringLiteral(
        "QLabel#spanProfileTitleLabel {"
        "font-size: 14px;"
        "font-weight: 600;"
        "color: #0f172a;"
        "}"));

    subtitleLabel_ = new QLabel(surface);
    subtitleLabel_->setObjectName(QStringLiteral("spanProfileSubtitleLabel"));
    subtitleLabel_->setWordWrap(true);
    subtitleLabel_->setStyleSheet(QStringLiteral("color: #64748b;"));

    plotWidget_ = new ProfilePlotWidget(surface);
    plotWidget_->setObjectName(QStringLiteral("spanProfilePlotWidget"));
    plotWidget_->setStyleSheet(QStringLiteral(
        "ProfilePlotWidget#spanProfilePlotWidget {"
        "background: transparent;"
        "}"));

    profileLayout->addWidget(titleLabel_);
    profileLayout->addWidget(subtitleLabel_);
    profileLayout->addWidget(plotWidget_, 1);

    surface->setStyleSheet(QStringLiteral(
        "QWidget#spanProfileSurface {"
        "background-color: #eef4fb;"
        "border-top: 1px solid #d7e2f0;"
        "}"));

    setWidget(surface);
    retranslateUi();
}

void SpanProfileDock::retranslateUi()
{
    setWindowTitle(tr("Span Profile"));
    if (titleLabel_ != nullptr) {
        titleLabel_->setText(tr("Measured corridor profile"));
    }
    if (subtitleLabel_ != nullptr) {
        subtitleLabel_->setText(tr("The profile updates from the current measurement path, highlights clearance segments below the threshold, and overlays nearby towers and issues."));
    }
}

ProfilePlotWidget* SpanProfileDock::plotWidget() const
{
    return plotWidget_;
}
