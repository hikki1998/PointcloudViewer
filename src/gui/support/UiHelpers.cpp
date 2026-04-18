#include "gui/support/UiHelpers.h"

#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QToolButton>

namespace
{
QString lightDialogPushButtonStyleSheet()
{
    return QStringLiteral(
        "QPushButton {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "padding: 6px 14px;"
        "min-width: 84px;"
        "}"
        "QPushButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QPushButton:pressed {"
        "background-color: #dbeafe;"
        "border-color: #60a5fa;"
        "}"
        "QPushButton:default {"
        "background-color: #e0ecff;"
        "color: #1d4ed8;"
        "border-color: #93c5fd;"
        "}"
        "QPushButton:default:hover {"
        "background-color: #d4e4ff;"
        "}"
        "QPushButton:disabled {"
        "background-color: #f1f5f9;"
        "color: #94a3b8;"
        "border-color: #dbe3ee;"
        "}");
}

QString lightDialogToolButtonStyleSheet()
{
    return QStringLiteral(
        "QToolButton {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "padding: 3px 8px;"
        "}"
        "QToolButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QToolButton:pressed {"
        "background-color: #dbeafe;"
        "border-color: #60a5fa;"
        "}"
        "QToolButton:disabled {"
        "background-color: #f1f5f9;"
        "color: #94a3b8;"
        "border-color: #dbe3ee;"
        "}");
}

QString styledDialogStyleSheet()
{
    return QStringLiteral(
        "QDialog, QFileDialog, QFileDialog QWidget {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "}"
        "QFileDialog QFrame, QFileDialog QStackedWidget, QFileDialog QSplitter {"
        "background-color: #f8fafc;"
        "}"
        "QFileDialog QLabel {"
        "color: #0f172a;"
        "}"
        "QFileDialog QLineEdit,"
        "QFileDialog QComboBox,"
        "QFileDialog QListView,"
        "QFileDialog QTreeView,"
        "QFileDialog QAbstractItemView,"
        "QFileDialog QSpinBox {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "}"
        "QFileDialog QLineEdit, QFileDialog QComboBox {"
        "min-height: 26px;"
        "padding: 2px 8px;"
        "}"
        "QFileDialog QPushButton {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "min-height: 28px;"
        "padding: 4px 10px;"
        "}"
        "QFileDialog QPushButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QFileDialog QPushButton:pressed {"
        "background-color: #dbeafe;"
        "}"
        "QFileDialog QPushButton:default {"
        "background-color: #e0ecff;"
        "color: #1d4ed8;"
        "border-color: #93c5fd;"
        "}"
        "QFileDialog QPushButton:default:hover {"
        "background-color: #d4e4ff;"
        "}"
        "QFileDialog QPushButton:disabled {"
        "background-color: #f1f5f9;"
        "border-color: #e2e8f0;"
        "color: #94a3b8;"
        "}"
        "QFileDialog QHeaderView::section {"
        "background-color: #e2e8f0;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "padding: 4px 8px;"
        "font-weight: 600;"
        "}"
        "QFileDialog QHeaderView::section:hover {"
        "background-color: #dbeafe;"
        "color: #0f172a;"
        "}"
        "QFileDialog QHeaderView::section:pressed {"
        "background-color: #bfdbfe;"
        "color: #0f172a;"
        "}"
        "QFileDialog QToolButton {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "padding: 3px 8px;"
        "}"
        "QFileDialog QToolButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QFileDialog QToolButton:pressed {"
        "background-color: #dbeafe;"
        "}");
}
}

namespace lasviewer::gui
{
void enforceLightDialogButtonStyles(QWidget* root)
{
    if (root == nullptr) {
        return;
    }

    const QString pushButtonStyle = lightDialogPushButtonStyleSheet();
    const QString toolButtonStyle = lightDialogToolButtonStyleSheet();

    for (QPushButton* button : root->findChildren<QPushButton*>()) {
        if (button != nullptr) {
            button->setStyleSheet(pushButtonStyle);
        }
    }
    for (QToolButton* button : root->findChildren<QToolButton*>()) {
        if (button != nullptr) {
            button->setStyleSheet(toolButtonStyle);
        }
    }
}

void applyStyledDialogPalette(QDialog* dialog)
{
    if (dialog == nullptr) {
        return;
    }

    dialog->setStyleSheet(styledDialogStyleSheet());

    QPalette palette = dialog->palette();
    palette.setColor(QPalette::Window, QColor(248, 250, 252));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(241, 245, 249));
    palette.setColor(QPalette::WindowText, QColor(15, 23, 42));
    palette.setColor(QPalette::Text, QColor(15, 23, 42));
    palette.setColor(QPalette::Button, QColor(255, 255, 255));
    palette.setColor(QPalette::ButtonText, QColor(15, 23, 42));
    palette.setColor(QPalette::Highlight, QColor(219, 234, 254));
    palette.setColor(QPalette::HighlightedText, QColor(15, 23, 42));
    dialog->setPalette(palette);
    enforceLightDialogButtonStyles(dialog);
}

void setFormFieldLabel(QFormLayout* layout, QWidget* field, const QString& text)
{
    if (layout == nullptr || field == nullptr) {
        return;
    }
    if (auto* label = qobject_cast<QLabel*>(layout->labelForField(field))) {
        label->setText(text);
    }
}

QString showStyledOpenFileNameDialog(
    QWidget* parent,
    const QString& title,
    const QString& initialPath,
    const QString& filter)
{
    QFileDialog dialog(parent, title, initialPath, filter);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    applyStyledDialogPalette(&dialog);

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return QString();
    }
    return dialog.selectedFiles().constFirst();
}

QStringList showStyledOpenFileNamesDialog(
    QWidget* parent,
    const QString& title,
    const QString& initialPath,
    const QString& filter)
{
    QFileDialog dialog(parent, title, initialPath, filter);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    applyStyledDialogPalette(&dialog);

    if (dialog.exec() != QDialog::Accepted) {
        return QStringList();
    }
    return dialog.selectedFiles();
}

QString showStyledSaveFileNameDialog(
    QWidget* parent,
    const QString& title,
    const QString& initialPath,
    const QString& filter)
{
    QFileDialog dialog(parent, title, initialPath, filter);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    applyStyledDialogPalette(&dialog);

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return QString();
    }
    return dialog.selectedFiles().constFirst();
}

QString showStyledExistingDirectoryDialog(
    QWidget* parent,
    const QString& title,
    const QString& initialPath)
{
    QFileDialog dialog(parent, title, initialPath);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    applyStyledDialogPalette(&dialog);

    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        return QString();
    }
    return dialog.selectedFiles().constFirst();
}

QMessageBox::StandardButton showLightStyledMessageBox(
    QWidget* parent,
    QMessageBox::Icon icon,
    const QString& title,
    const QString& text,
    QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    QMessageBox messageBox(icon, title, text, buttons, parent);
    messageBox.setStyleSheet(QStringLiteral(
        "QMessageBox {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "}"
        "QMessageBox QLabel {"
        "color: #0f172a;"
        "}"
        "QMessageBox QPushButton {"
        "background-color: #ffffff;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "padding: 6px 14px;"
        "color: #0f172a;"
        "min-width: 84px;"
        "}"
        "QMessageBox QPushButton:hover {"
        "background-color: #f1f5f9;"
        "border-color: #94a3b8;"
        "}"
        "QMessageBox QPushButton:pressed {"
        "background-color: #e2e8f0;"
        "border-color: #94a3b8;"
        "}"
        "QMessageBox QPushButton:default {"
        "background-color: #e0ecff;"
        "color: #1d4ed8;"
        "border-color: #93c5fd;"
        "}"
        "QMessageBox QPushButton:default:hover {"
        "background-color: #d4e4ff;"
        "}"));
    enforceLightDialogButtonStyles(&messageBox);
    if (defaultButton != QMessageBox::NoButton) {
        messageBox.setDefaultButton(defaultButton);
    }
    return static_cast<QMessageBox::StandardButton>(messageBox.exec());
}
}
