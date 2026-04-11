#pragma once

#include <QMessageBox>
#include <QString>
#include <QStringList>

class QDialog;
class QFormLayout;
class QWidget;

namespace lasviewer::gui
{
void enforceLightDialogButtonStyles(QWidget* root);
void applyStyledDialogPalette(QDialog* dialog);
void setFormFieldLabel(QFormLayout* layout, QWidget* field, const QString& text);

QString showStyledOpenFileNameDialog(
    QWidget* parent,
    const QString& title,
    const QString& initialPath,
    const QString& filter);

QStringList showStyledOpenFileNamesDialog(
    QWidget* parent,
    const QString& title,
    const QString& initialPath,
    const QString& filter);

QString showStyledSaveFileNameDialog(
    QWidget* parent,
    const QString& title,
    const QString& initialPath,
    const QString& filter);

QMessageBox::StandardButton showLightStyledMessageBox(
    QWidget* parent,
    QMessageBox::Icon icon,
    const QString& title,
    const QString& text,
    QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
}
