#include "gui/ApplicationLogDock.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include "logging/ApplicationLogger.h"

namespace
{
struct LogVisualStyle
{
    QString token;
    QString accentColor;
    QString badgeBackground;
    QString badgeForeground;
    QString messageColor;
};

LogVisualStyle logVisualStyleForLevel(lasviewer::logging::LogLevel level)
{
    switch (level) {
    case lasviewer::logging::LogLevel::Warning:
        return {
            QStringLiteral("WARN"),
            QStringLiteral("#d97706"),
            QStringLiteral("#fef3c7"),
            QStringLiteral("#92400e"),
            QStringLiteral("#78350f")
        };
    case lasviewer::logging::LogLevel::Error:
        return {
            QStringLiteral("ERROR"),
            QStringLiteral("#dc2626"),
            QStringLiteral("#fee2e2"),
            QStringLiteral("#991b1b"),
            QStringLiteral("#7f1d1d")
        };
    case lasviewer::logging::LogLevel::Info:
    default:
        return {
            QStringLiteral("INFO"),
            QStringLiteral("#2563eb"),
            QStringLiteral("#dbeafe"),
            QStringLiteral("#1d4ed8"),
            QStringLiteral("#0f172a")
        };
    }
}

QString highlightLogKeyword(const QString& text, const QString& keyword)
{
    const QString normalizedKeyword = keyword.trimmed();
    if (normalizedKeyword.isEmpty()) {
        return text.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br/>"));
    }

    QString html;
    html.reserve(text.size() + 48);

    int cursor = 0;
    while (cursor < text.size()) {
        const int hitIndex = text.indexOf(normalizedKeyword, cursor, Qt::CaseInsensitive);
        if (hitIndex < 0) {
            html += text.mid(cursor).toHtmlEscaped();
            break;
        }

        html += text.mid(cursor, hitIndex - cursor).toHtmlEscaped();
        const QString hitText = text.mid(hitIndex, normalizedKeyword.size());
        html += QStringLiteral("<span style='background:#fde68a; color:#111827; border-radius:3px; padding:0 2px;'>%1</span>")
            .arg(hitText.toHtmlEscaped());
        cursor = hitIndex + normalizedKeyword.size();
    }

    return html.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));
}

QString logEntryHtml(const lasviewer::logging::LogEntry& entry, const QString& keyword)
{
    const LogVisualStyle style = logVisualStyleForLevel(entry.level);
    const QString timestamp = QDateTime::fromMSecsSinceEpoch(entry.timestampMs).toString(QStringLiteral("HH:mm:ss"));
    const QString moduleText = entry.module.trimmed().isEmpty() ? QStringLiteral("APP") : entry.module;
    const QString moduleHtml = moduleText.toHtmlEscaped();
    const QString messageHtml = highlightLogKeyword(entry.message, keyword);

    return QStringLiteral(
        "<div style='margin:0 0 10px 0; padding:10px 12px; border-left:4px solid %1; "
        "background:#ffffff; border-radius:9px; border:1px solid #d9e5f2;'>"
        "<div>"
        "<span style='color:#64748b; font-family:Consolas, \"Courier New\", monospace; font-size:12px;'>%2</span>"
        "<span style='display:inline-block; padding:2px 8px; border-radius:999px; "
        "margin-left:8px; background:%3; color:%4; font-family:Consolas, \"Courier New\", monospace; font-size:11px; font-weight:700;'>%5</span>"
        "<span style='display:inline-block; padding:2px 8px; border-radius:999px; "
        "margin-left:8px; background:#eef2f7; color:#334155; font-size:11px; font-weight:600;'>%6</span>"
        "</div>"
        "<div style='margin-top:7px; color:%7; font-size:13px; line-height:1.55;'>%8</div>"
        "</div>")
        .arg(style.accentColor)
        .arg(timestamp)
        .arg(style.badgeBackground)
        .arg(style.badgeForeground)
        .arg(style.token)
        .arg(moduleHtml)
        .arg(style.messageColor)
        .arg(messageHtml);
}
}

ApplicationLogDock::ApplicationLogDock(QWidget* parent)
    : QDockWidget(parent)
{
    setObjectName(QStringLiteral("applicationLogDock"));
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    setFeatures(
        QDockWidget::DockWidgetClosable
        | QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable);
    setMinimumHeight(180);
    setMaximumHeight(460);

    auto* logSurface = new QWidget(this);
    logSurface->setObjectName(QStringLiteral("applicationLogSurface"));
    auto* logSurfaceLayout = new QVBoxLayout(logSurface);
    logSurfaceLayout->setContentsMargins(10, 10, 10, 10);
    logSurfaceLayout->setSpacing(8);

    auto* logControlsRow = new QWidget(logSurface);
    logControlsRow->setObjectName(QStringLiteral("applicationLogToolsRow"));
    auto* logControlsLayout = new QHBoxLayout(logControlsRow);
    logControlsLayout->setContentsMargins(8, 8, 8, 8);
    logControlsLayout->setSpacing(8);

    logLevelFilterComboBox_ = new QComboBox(logControlsRow);
    logLevelFilterComboBox_->setObjectName(QStringLiteral("applicationLogLevelFilter"));
    logLevelFilterComboBox_->setMinimumWidth(138);
    logLevelFilterComboBox_->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    logSearchLineEdit_ = new QLineEdit(logControlsRow);
    logSearchLineEdit_->setObjectName(QStringLiteral("applicationLogSearchEdit"));
    logSearchLineEdit_->setClearButtonEnabled(true);
    logSearchLineEdit_->setMinimumWidth(240);

    logAutoScrollCheckBox_ = new QCheckBox(logControlsRow);
    logAutoScrollCheckBox_->setObjectName(QStringLiteral("applicationLogAutoScrollCheck"));
    logAutoScrollCheckBox_->setChecked(true);

    logClearButton_ = new QPushButton(logControlsRow);
    logClearButton_->setObjectName(QStringLiteral("applicationLogClearButton"));

    logExportButton_ = new QPushButton(logControlsRow);
    logExportButton_->setObjectName(QStringLiteral("applicationLogExportButton"));

    logStatsLabel_ = new QLabel(logControlsRow);
    logStatsLabel_->setObjectName(QStringLiteral("applicationLogStatsLabel"));
    logStatsLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    logStatsLabel_->setMinimumWidth(180);

    logControlsLayout->addWidget(logLevelFilterComboBox_, 0);
    logControlsLayout->addWidget(logSearchLineEdit_, 1);
    logControlsLayout->addWidget(logAutoScrollCheckBox_, 0);
    logControlsLayout->addWidget(logClearButton_, 0);
    logControlsLayout->addWidget(logExportButton_, 0);
    logControlsLayout->addStretch(1);
    logControlsLayout->addWidget(logStatsLabel_, 0);

    logTextEdit_ = new QTextEdit(logSurface);
    logTextEdit_->setReadOnly(true);
    logTextEdit_->setUndoRedoEnabled(false);
    logTextEdit_->setAcceptRichText(true);
    logTextEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
    logTextEdit_->document()->setMaximumBlockCount(0);
    logTextEdit_->document()->setDocumentMargin(14);
    logTextEdit_->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "border: 1px solid #d6e1ee;"
        "border-radius: 8px;"
        "selection-background-color: #bfdbfe;"
        "selection-color: #0f172a;"
        "font-family: 'Segoe UI', 'Microsoft YaHei UI';"
        "font-size: 13px;"
        "}"));

    logSurfaceLayout->addWidget(logControlsRow, 0);
    logSurfaceLayout->addWidget(logTextEdit_, 1);

    logSurface->setStyleSheet(QStringLiteral(
        "QWidget#applicationLogSurface {"
        "background-color: #edf3fb;"
        "border-top: 1px solid #d6e2ef;"
        "}"
        "QWidget#applicationLogToolsRow {"
        "background: #f8fbff;"
        "border: 1px solid #d5e3f2;"
        "border-radius: 8px;"
        "}"
        "QComboBox#applicationLogLevelFilter,"
        "QLineEdit#applicationLogSearchEdit {"
        "background: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #c8d6e8;"
        "border-radius: 5px;"
        "padding: 4px 8px;"
        "font-size: 13px;"
        "min-height: 28px;"
        "}"
        "QCheckBox#applicationLogAutoScrollCheck {"
        "color: #334155;"
        "font-size: 13px;"
        "font-weight: 600;"
        "}"
        "QPushButton#applicationLogClearButton,"
        "QPushButton#applicationLogExportButton {"
        "background: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #c8d6e8;"
        "border-radius: 5px;"
        "padding: 5px 12px;"
        "font-size: 13px;"
        "font-weight: 600;"
        "min-height: 28px;"
        "}"
        "QPushButton#applicationLogClearButton:hover,"
        "QPushButton#applicationLogExportButton:hover {"
        "background: #eff6ff;"
        "border-color: #9fb8d6;"
        "}"
        "QLabel#applicationLogStatsLabel {"
        "color: #64748b;"
        "font-size: 12px;"
        "font-weight: 600;"
        "}"));

    setWidget(logSurface);

    retranslateUi();

    connect(logLevelFilterComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshEntries();
        emit filterStateChanged();
    });
    connect(logSearchLineEdit_, &QLineEdit::textChanged, this, [this](const QString&) {
        refreshEntries();
        emit filterStateChanged();
    });
    connect(logAutoScrollCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && logTextEdit_ != nullptr) {
            if (QScrollBar* scrollBar = logTextEdit_->verticalScrollBar()) {
                scrollBar->setValue(scrollBar->maximum());
            }
        }
        emit autoScrollToggled(checked);
    });
    connect(logClearButton_, &QPushButton::clicked, this, [this]() {
        lasviewer::logging::ApplicationLogger::instance().clear();
        emit entriesClearedByUser();
    });
    connect(logExportButton_, &QPushButton::clicked, this, &ApplicationLogDock::exportRequested);
    connect(
        &lasviewer::logging::ApplicationLogger::instance(),
        &lasviewer::logging::ApplicationLogger::entryAdded,
        this,
        &ApplicationLogDock::refreshEntries);
    connect(
        &lasviewer::logging::ApplicationLogger::instance(),
        &lasviewer::logging::ApplicationLogger::entriesCleared,
        this,
        &ApplicationLogDock::refreshEntries);

    refreshEntries();
}

void ApplicationLogDock::retranslateUi()
{
    setWindowTitle(tr("Application Log"));

    const int selectedFilter = selectedFilterLevel();
    if (logLevelFilterComboBox_ != nullptr) {
        const QSignalBlocker blocker(logLevelFilterComboBox_);
        logLevelFilterComboBox_->clear();
        logLevelFilterComboBox_->addItem(tr("All levels"), -1);
        logLevelFilterComboBox_->addItem(tr("Info"), static_cast<int>(lasviewer::logging::LogLevel::Info));
        logLevelFilterComboBox_->addItem(tr("Warning"), static_cast<int>(lasviewer::logging::LogLevel::Warning));
        logLevelFilterComboBox_->addItem(tr("Error"), static_cast<int>(lasviewer::logging::LogLevel::Error));

        const int index = logLevelFilterComboBox_->findData(selectedFilter);
        logLevelFilterComboBox_->setCurrentIndex(index >= 0 ? index : 0);
    }
    if (logSearchLineEdit_ != nullptr) {
        logSearchLineEdit_->setPlaceholderText(tr("Search module or message"));
    }
    if (logAutoScrollCheckBox_ != nullptr) {
        logAutoScrollCheckBox_->setText(tr("Auto-scroll"));
    }
    if (logClearButton_ != nullptr) {
        logClearButton_->setText(tr("Clear"));
    }
    if (logExportButton_ != nullptr) {
        logExportButton_->setText(tr("Export"));
    }

    refreshEntries();
}

void ApplicationLogDock::refreshEntries()
{
    if (logTextEdit_ == nullptr) {
        return;
    }

    const QVector<lasviewer::logging::LogEntry> entries = lasviewer::logging::ApplicationLogger::instance().entries();
    const int selectedLevel = selectedFilterLevel();
    const QString keyword = searchKeyword().trimmed();

    totalEntryCount_ = entries.size();
    visibleEntryCount_ = 0;

    QString html;
    html.reserve(entries.size() * 240);
    for (const lasviewer::logging::LogEntry& entry : entries) {
        if (selectedLevel >= 0 && static_cast<int>(entry.level) != selectedLevel) {
            continue;
        }

        const LogVisualStyle style = logVisualStyleForLevel(entry.level);
        if (!keyword.isEmpty()
            && !entry.message.contains(keyword, Qt::CaseInsensitive)
            && !entry.module.contains(keyword, Qt::CaseInsensitive)
            && !style.token.contains(keyword, Qt::CaseInsensitive)) {
            continue;
        }

        html += logEntryHtml(entry, keyword);
        ++visibleEntryCount_;
    }

    if (visibleEntryCount_ == 0) {
        const QString emptyText = entries.isEmpty()
            ? tr("No log entries yet.")
            : tr("No log entries match current filters.");
        html = QStringLiteral(
            "<div style='margin:20px 16px; padding:12px; border:1px dashed #c6d4e6; border-radius:8px; "
            "background:#ffffff; color:#64748b; font-size:13px;'>%1</div>")
                .arg(emptyText.toHtmlEscaped());
    }

    logTextEdit_->setHtml(html);

    if (logStatsLabel_ != nullptr) {
        logStatsLabel_->setText(tr("%1 shown / %2 total")
            .arg(QLocale().toString(visibleEntryCount_))
            .arg(QLocale().toString(totalEntryCount_)));
    }

    if (autoScrollEnabled()) {
        if (QScrollBar* scrollBar = logTextEdit_->verticalScrollBar()) {
            scrollBar->setValue(scrollBar->maximum());
        }
    }
}

int ApplicationLogDock::totalEntryCount() const
{
    return totalEntryCount_;
}

int ApplicationLogDock::visibleEntryCount() const
{
    return visibleEntryCount_;
}

int ApplicationLogDock::selectedFilterLevel() const
{
    if (logLevelFilterComboBox_ == nullptr) {
        return -1;
    }

    const QVariant currentData = logLevelFilterComboBox_->currentData();
    return currentData.isValid() ? currentData.toInt() : -1;
}

void ApplicationLogDock::setSelectedFilterLevel(int level)
{
    if (logLevelFilterComboBox_ == nullptr) {
        return;
    }

    const int index = logLevelFilterComboBox_->findData(level);
    logLevelFilterComboBox_->setCurrentIndex(index >= 0 ? index : 0);
}

QString ApplicationLogDock::searchKeyword() const
{
    return logSearchLineEdit_ != nullptr ? logSearchLineEdit_->text() : QString();
}

void ApplicationLogDock::setSearchKeyword(const QString& keyword)
{
    if (logSearchLineEdit_ != nullptr) {
        logSearchLineEdit_->setText(keyword);
    }
}

bool ApplicationLogDock::autoScrollEnabled() const
{
    return logAutoScrollCheckBox_ == nullptr || logAutoScrollCheckBox_->isChecked();
}

void ApplicationLogDock::setAutoScrollEnabled(bool enabled)
{
    if (logAutoScrollCheckBox_ != nullptr) {
        logAutoScrollCheckBox_->setChecked(enabled);
    }
}

QLineEdit* ApplicationLogDock::searchLineEdit() const
{
    return logSearchLineEdit_;
}

QTextEdit* ApplicationLogDock::logTextEdit() const
{
    return logTextEdit_;
}
