#pragma once

#include <QDialog>

#include "crs/CrsCatalogService.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace lasviewer::crs
{
class LASVIEWERCRS_EXPORT CrsSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CrsSelectionDialog(QWidget* parent = nullptr);

    void setAllowedKindFilter(CoordinateSystemKindFilter kindFilter);
    void setCurrentCoordinateSystem(const CoordinateSystemRef& crs);
    CoordinateSystemRef selectedCoordinateSystem() const;

private:
    void buildUi();
    void refreshRecentTable();
    void refreshCatalogTree();
    void updateSelectionSummary();
    void normalizeSelectedCoordinateSystem();
    void applySelection(const CrsCatalogEntry& entry);
    void acceptCurrentSelection();
    bool treeItemEntry(const QTreeWidgetItem* item, CrsCatalogEntry* entry) const;
    bool recentRowEntry(int row, CrsCatalogEntry* entry) const;
    QString previewText(const CrsCatalogEntry& entry) const;
    CoordinateSystemKindFilter activeKindFilter() const;

    CrsCatalogService catalogService_;
    CoordinateSystemKindFilter allowedKindFilter_ = CoordinateSystemKindFilter::Any;
    CoordinateSystemRef selectedCoordinateSystem_;
    CrsCatalogEntry selectedEntry_;
    bool hasSelectedEntry_ = false;

    QLineEdit* filterEdit_ = nullptr;
    QComboBox* quickFilterComboBox_ = nullptr;
    QTableWidget* recentTableWidget_ = nullptr;
    QTreeWidget* catalogTreeWidget_ = nullptr;
    QCheckBox* showDeprecatedCheckBox_ = nullptr;
    QLabel* selectedNameValueLabel_ = nullptr;
    QLabel* selectedCodeValueLabel_ = nullptr;
    QLabel* selectedTypeValueLabel_ = nullptr;
    QLabel* selectedSummaryValueLabel_ = nullptr;
    QPlainTextEdit* previewTextEdit_ = nullptr;
};
}
