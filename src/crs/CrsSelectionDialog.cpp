#include "crs/CrsSelectionDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "crs/CrsAuthorityService.h"

namespace
{
constexpr int kCatalogEntryRole = Qt::UserRole + 100;

void applyCrsDialogStyle(QDialog* dialog)
{
    if (dialog == nullptr) {
        return;
    }

    dialog->setStyleSheet(QStringLiteral(
        "QDialog { background-color: #f3f7fb; }"
        "QFrame#crsCard { background-color: rgba(255,255,255,0.96); border: 1px solid rgba(148,163,184,0.22); border-radius: 14px; }"
        "QLabel#crsSectionTitle { color: #0f172a; font-size: 14px; font-weight: 700; }"
        "QLabel#crsFieldLabel { color: #64748b; font-size: 12px; font-weight: 600; }"
        "QLabel#crsFieldValue { color: #0f172a; font-size: 13px; font-weight: 500; }"
        "QLineEdit, QComboBox, QTreeWidget, QTableWidget, QPlainTextEdit {"
        "background-color: #ffffff; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 8px;"
        "selection-background-color: #dbeafe; selection-color: #0f172a; }"
        "QLineEdit, QComboBox { min-height: 28px; padding: 2px 8px; }"
        "QHeaderView::section { background-color: #e2e8f0; color: #0f172a; padding: 5px 8px; border: 1px solid #cbd5e1; font-weight: 600; }"
        "QPushButton { min-width: 88px; padding: 7px 14px; border-radius: 9px; border: 1px solid rgba(37,99,235,0.18); background-color: #ffffff; color: #0f172a; font-weight: 600; }"
        "QPushButton:hover { background-color: #eef4ff; border-color: #93c5fd; }"
        "QPushButton:pressed { background-color: #dbeafe; border-color: #60a5fa; }"
        "QPushButton:default { background-color: #e0ecff; color: #1d4ed8; border-color: #93c5fd; }"
        "QPushButton:default:hover { background-color: #d4e4ff; }"
        "QPushButton:disabled { background-color: #f1f5f9; color: #94a3b8; border-color: #dbe3ee; }"
        "QCheckBox { color: #334155; }"
        "QPlainTextEdit { padding: 6px; }"));
}

QFrame* createCard(QWidget* parent)
{
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("crsCard"));
    return frame;
}
}

namespace lasviewer::crs
{
CrsSelectionDialog::CrsSelectionDialog(QWidget* parent)
    : QDialog(parent)
{
    buildUi();
    refreshRecentTable();
    refreshCatalogTree();
    updateSelectionSummary();
}

void CrsSelectionDialog::setAllowedKindFilter(CoordinateSystemKindFilter kindFilter)
{
    allowedKindFilter_ = kindFilter;
    if (quickFilterComboBox_ != nullptr) {
        quickFilterComboBox_->setCurrentIndex(static_cast<int>(kindFilter));
        quickFilterComboBox_->setEnabled(kindFilter == CoordinateSystemKindFilter::Any);
    }
    refreshRecentTable();
    refreshCatalogTree();
}

void CrsSelectionDialog::setCurrentCoordinateSystem(const CoordinateSystemRef& crs)
{
    selectedCoordinateSystem_ = crs;
    normalizeSelectedCoordinateSystem();
    hasSelectedEntry_ = catalogService_.findByCode(crs.authName, crs.code, &selectedEntry_);
    updateSelectionSummary();
}

CoordinateSystemRef CrsSelectionDialog::selectedCoordinateSystem() const
{
    return selectedCoordinateSystem_;
}

CoordinateSystemKindFilter CrsSelectionDialog::activeKindFilter() const
{
    return allowedKindFilter_ == CoordinateSystemKindFilter::Any
        ? static_cast<CoordinateSystemKindFilter>(quickFilterComboBox_->currentData().toInt())
        : allowedKindFilter_;
}

void CrsSelectionDialog::buildUi()
{
    setWindowTitle(tr("Select Coordinate System"));
    setModal(true);
    resize(860, 700);
    applyCrsDialogStyle(this);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 20, 22, 20);
    rootLayout->setSpacing(14);

    auto* headerCard = createCard(this);
    auto* headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(18, 16, 18, 16);
    headerLayout->setSpacing(10);
    filterEdit_ = new QLineEdit(headerCard);
    filterEdit_->setPlaceholderText(tr("Filter by name, EPSG code, or alias"));
    quickFilterComboBox_ = new QComboBox(headerCard);
    quickFilterComboBox_->addItem(tr("All"), static_cast<int>(CoordinateSystemKindFilter::Any));
    quickFilterComboBox_->addItem(tr("Geographic"), static_cast<int>(CoordinateSystemKindFilter::Geographic));
    quickFilterComboBox_->addItem(tr("Projected"), static_cast<int>(CoordinateSystemKindFilter::Projected));
    quickFilterComboBox_->setMinimumWidth(160);
    headerLayout->addWidget(filterEdit_, 1);
    headerLayout->addWidget(quickFilterComboBox_);
    rootLayout->addWidget(headerCard);

    auto* recentCard = createCard(this);
    auto* recentLayout = new QVBoxLayout(recentCard);
    recentLayout->setContentsMargins(18, 14, 18, 16);
    recentLayout->setSpacing(8);
    auto* recentTitle = new QLabel(tr("Recently Used Coordinate Systems"), recentCard);
    recentTitle->setObjectName(QStringLiteral("crsSectionTitle"));
    recentLayout->addWidget(recentTitle);
    recentTableWidget_ = new QTableWidget(recentCard);
    recentTableWidget_->setColumnCount(2);
    recentTableWidget_->setHorizontalHeaderLabels({ tr("Coordinate System"), tr("Authority ID") });
    recentTableWidget_->verticalHeader()->setVisible(false);
    recentTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    recentTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    recentTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recentTableWidget_->setAlternatingRowColors(true);
    recentTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    recentTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    recentTableWidget_->setMinimumHeight(170);
    recentLayout->addWidget(recentTableWidget_);
    rootLayout->addWidget(recentCard, 1);

    auto* catalogCard = createCard(this);
    auto* catalogLayout = new QVBoxLayout(catalogCard);
    catalogLayout->setContentsMargins(18, 14, 18, 16);
    catalogLayout->setSpacing(8);
    auto* headerRow = new QHBoxLayout();
    auto* catalogTitle = new QLabel(tr("Common Coordinate Systems"), catalogCard);
    catalogTitle->setObjectName(QStringLiteral("crsSectionTitle"));
    showDeprecatedCheckBox_ = new QCheckBox(tr("Show deprecated coordinate systems"), catalogCard);
    headerRow->addWidget(catalogTitle);
    headerRow->addStretch(1);
    headerRow->addWidget(showDeprecatedCheckBox_);
    catalogLayout->addLayout(headerRow);
    catalogTreeWidget_ = new QTreeWidget(catalogCard);
    catalogTreeWidget_->setColumnCount(2);
    catalogTreeWidget_->setHeaderLabels({ tr("Coordinate System"), tr("Authority ID") });
    catalogTreeWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    catalogTreeWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    catalogTreeWidget_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    catalogTreeWidget_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    catalogTreeWidget_->setMinimumHeight(240);
    catalogLayout->addWidget(catalogTreeWidget_);
    rootLayout->addWidget(catalogCard, 2);

    auto* summaryCard = createCard(this);
    auto* summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(18, 14, 18, 16);
    summaryLayout->setSpacing(10);
    auto* summaryTitle = new QLabel(tr("Selected Coordinate System"), summaryCard);
    summaryTitle->setObjectName(QStringLiteral("crsSectionTitle"));
    summaryLayout->addWidget(summaryTitle);
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(8);
    auto addField = [summaryCard, grid](int row, const QString& labelText, QLabel*& valueLabel) {
        auto* label = new QLabel(labelText, summaryCard);
        label->setObjectName(QStringLiteral("crsFieldLabel"));
        valueLabel = new QLabel(summaryCard);
        valueLabel->setObjectName(QStringLiteral("crsFieldValue"));
        valueLabel->setWordWrap(true);
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(label, row, 0);
        grid->addWidget(valueLabel, row, 1);
    };
    addField(0, tr("Name"), selectedNameValueLabel_);
    addField(1, tr("Authority ID"), selectedCodeValueLabel_);
    addField(2, tr("Type"), selectedTypeValueLabel_);
    addField(3, tr("Summary"), selectedSummaryValueLabel_);
    grid->setColumnStretch(1, 1);
    summaryLayout->addLayout(grid);
    previewTextEdit_ = new QPlainTextEdit(summaryCard);
    previewTextEdit_->setReadOnly(true);
    previewTextEdit_->setMinimumHeight(130);
    summaryLayout->addWidget(previewTextEdit_);
    rootLayout->addWidget(summaryCard, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    rootLayout->addWidget(buttonBox);

    connect(filterEdit_, &QLineEdit::textChanged, this, [this](const QString&) { refreshRecentTable(); refreshCatalogTree(); });
    connect(quickFilterComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { refreshRecentTable(); refreshCatalogTree(); });
    connect(showDeprecatedCheckBox_, &QCheckBox::toggled, this, [this](bool) { refreshRecentTable(); refreshCatalogTree(); });
    connect(recentTableWidget_, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        CrsCatalogEntry entry;
        if (recentRowEntry(row, &entry)) {
            applySelection(entry);
        }
    });
    connect(recentTableWidget_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        CrsCatalogEntry entry;
        if (recentRowEntry(row, &entry)) {
            applySelection(entry);
            acceptCurrentSelection();
        }
    });
    connect(catalogTreeWidget_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
        CrsCatalogEntry entry;
        if (treeItemEntry(current, &entry)) {
            applySelection(entry);
        }
    });
    connect(catalogTreeWidget_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        CrsCatalogEntry entry;
        if (treeItemEntry(item, &entry)) {
            applySelection(entry);
            acceptCurrentSelection();
        }
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() { acceptCurrentSelection(); });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CrsSelectionDialog::refreshRecentTable()
{
    const QList<CrsCatalogEntry> entries = catalogService_.recentEntries(activeKindFilter());
    recentTableWidget_->clearContents();
    recentTableWidget_->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        const CrsCatalogEntry& entry = entries.at(row);
        auto* nameItem = new QTableWidgetItem(entry.reference.displayName);
        auto* codeItem = new QTableWidgetItem(coordinateSystemCodeText(entry.reference));
        nameItem->setData(kCatalogEntryRole, coordinateSystemRefToJson(entry.reference));
        codeItem->setData(kCatalogEntryRole, coordinateSystemRefToJson(entry.reference));
        recentTableWidget_->setItem(row, 0, nameItem);
        recentTableWidget_->setItem(row, 1, codeItem);
    }
}

void CrsSelectionDialog::refreshCatalogTree()
{
    const QList<CrsCatalogEntry> entries = catalogService_.filter(
        filterEdit_->text(),
        activeKindFilter(),
        showDeprecatedCheckBox_->isChecked());
    catalogTreeWidget_->clear();

    QTreeWidgetItem* geographicRoot = nullptr;
    QTreeWidgetItem* projectedRoot = nullptr;
    for (const CrsCatalogEntry& entry : entries) {
        QTreeWidgetItem*& rootItem = entry.reference.kind == CoordinateSystemKind::Geographic
            ? geographicRoot
            : projectedRoot;
        if (rootItem == nullptr) {
            rootItem = new QTreeWidgetItem(catalogTreeWidget_, QStringList {
                entry.reference.kind == CoordinateSystemKind::Geographic
                    ? tr("Geographic Coordinate Systems")
                    : tr("Projected Coordinate Systems")
            });
            rootItem->setExpanded(true);
        }
        auto* child = new QTreeWidgetItem(rootItem, QStringList {
            entry.reference.displayName,
            coordinateSystemCodeText(entry.reference)
        });
        child->setData(0, kCatalogEntryRole, coordinateSystemRefToJson(entry.reference));
        child->setToolTip(0, entry.summary);
    }
}

void CrsSelectionDialog::updateSelectionSummary()
{
    selectedNameValueLabel_->setText(selectedCoordinateSystem_.code <= 0
        ? tr("Not selected")
        : CrsAuthorityService::displayName(selectedCoordinateSystem_));
    selectedCodeValueLabel_->setText(CrsAuthorityService::authorityText(selectedCoordinateSystem_));
    selectedTypeValueLabel_->setText(coordinateSystemKindDisplayName(selectedCoordinateSystem_.kind));
    if (hasSelectedEntry_) {
        selectedSummaryValueLabel_->setText(selectedEntry_.summary);
        previewTextEdit_->setPlainText(previewText(selectedEntry_));
    } else {
        selectedSummaryValueLabel_->setText(tr("No common CRS entry is selected."));
        previewTextEdit_->setPlainText(selectedCoordinateSystem_.wkt.trimmed().isEmpty()
            ? coordinateSystemCodeText(selectedCoordinateSystem_)
            : selectedCoordinateSystem_.wkt);
    }

    if (QDialogButtonBox* buttonBox = findChild<QDialogButtonBox*>()) {
        if (QPushButton* okButton = buttonBox->button(QDialogButtonBox::Ok)) {
            okButton->setEnabled(selectedCoordinateSystem_.code > 0);
        }
    }
}

void CrsSelectionDialog::normalizeSelectedCoordinateSystem()
{
    if (selectedCoordinateSystem_.code <= 0 && selectedCoordinateSystem_.wkt.trimmed().isEmpty()) {
        return;
    }

    CoordinateSystemRef normalized;
    if (CrsAuthorityService::normalizeCoordinateSystem(selectedCoordinateSystem_, &normalized, nullptr)) {
        selectedCoordinateSystem_ = normalized;
    }
}

void CrsSelectionDialog::applySelection(const CrsCatalogEntry& entry)
{
    selectedCoordinateSystem_ = entry.reference;
    selectedEntry_ = entry;
    hasSelectedEntry_ = true;
    normalizeSelectedCoordinateSystem();
    updateSelectionSummary();
}

void CrsSelectionDialog::acceptCurrentSelection()
{
    if (selectedCoordinateSystem_.code <= 0) {
        return;
    }
    normalizeSelectedCoordinateSystem();
    catalogService_.markRecentlyUsed(selectedCoordinateSystem_);
    accept();
}

bool CrsSelectionDialog::treeItemEntry(const QTreeWidgetItem* item, CrsCatalogEntry* entry) const
{
    if (item == nullptr || item->childCount() > 0) {
        return false;
    }
    const CoordinateSystemRef ref = coordinateSystemRefFromJson(item->data(0, kCatalogEntryRole).toJsonObject());
    return catalogService_.findByCode(ref.authName, ref.code, entry);
}

bool CrsSelectionDialog::recentRowEntry(int row, CrsCatalogEntry* entry) const
{
    if (row < 0) {
        return false;
    }
    const QTableWidgetItem* item = recentTableWidget_->item(row, 0);
    if (item == nullptr) {
        return false;
    }
    const CoordinateSystemRef ref = coordinateSystemRefFromJson(item->data(kCatalogEntryRole).toJsonObject());
    return catalogService_.findByCode(ref.authName, ref.code, entry);
}

QString CrsSelectionDialog::previewText(const CrsCatalogEntry& entry) const
{
    const QString wkt = CrsAuthorityService::exportWkt(entry.reference);
    if (!wkt.trimmed().isEmpty()) {
        return wkt;
    }
    QStringList lines;
    lines.append(CrsAuthorityService::displayName(entry.reference));
    lines.append(CrsAuthorityService::authorityText(entry.reference));
    lines.append(coordinateSystemKindDisplayName(entry.reference.kind));
    lines.append(entry.summary);
    if (!entry.aliases.isEmpty()) {
        lines.append(tr("Aliases: %1").arg(entry.aliases.join(QStringLiteral(", "))));
    }
    return lines.join(QStringLiteral("\n"));
}
}
