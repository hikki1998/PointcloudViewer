#include "crs/ProjectCoordinateSystemsDialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>

#include "crs/CrsAuthorityService.h"
#include "crs/CrsSelectionDialog.h"

namespace
{
void applyProjectDialogStyle(QDialog* dialog)
{
    if (dialog == nullptr) {
        return;
    }

    dialog->setStyleSheet(QStringLiteral(
        "QDialog { background-color: #f3f7fb; }"
        "QFrame#projectCrsCard { background-color: rgba(255,255,255,0.96); border: 1px solid rgba(148,163,184,0.22); border-radius: 16px; }"
        "QLabel#projectCrsTitle { color: #0f172a; font-size: 16px; font-weight: 700; }"
        "QLabel#projectCrsLabel { color: #64748b; font-size: 12px; font-weight: 600; }"
        "QLabel#projectCrsValue { color: #0f172a; font-size: 13px; font-weight: 500; }"
        "QPushButton { min-width: 88px; padding: 7px 14px; border-radius: 9px; border: 1px solid rgba(37,99,235,0.18); background-color: #e0ecff; color: #1d4ed8; font-weight: 600; }"
        "QPushButton:hover { background-color: #d4e4ff; }"));
}

QFrame* createProjectCard(QWidget* parent)
{
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("projectCrsCard"));
    return frame;
}
}

namespace lasviewer::crs
{
ProjectCoordinateSystemsDialog::ProjectCoordinateSystemsDialog(QWidget* parent)
    : QDialog(parent)
{
    buildUi();
    refreshUi();
}

void ProjectCoordinateSystemsDialog::setCoordinateSystems(const ProjectCoordinateSystems& coordinateSystems)
{
    coordinateSystems_ = coordinateSystems;
    if (coordinateSystems_.geographicCrs.code <= 0) {
        coordinateSystems_.geographicCrs = defaultGeographicCoordinateSystem();
    }
    refreshUi();
}

ProjectCoordinateSystems ProjectCoordinateSystemsDialog::coordinateSystems() const
{
    return coordinateSystems_;
}

void ProjectCoordinateSystemsDialog::buildUi()
{
    setWindowTitle(tr("Project Coordinate Systems"));
    setModal(true);
    resize(760, 520);
    applyProjectDialogStyle(this);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 22, 24, 20);
    rootLayout->setSpacing(16);

    auto* introCard = createProjectCard(this);
    auto* introLayout = new QVBoxLayout(introCard);
    introLayout->setContentsMargins(20, 18, 20, 18);
    auto* titleLabel = new QLabel(tr("Project Coordinate Systems"), introCard);
    titleLabel->setObjectName(QStringLiteral("projectCrsTitle"));
    auto* subtitleLabel = new QLabel(
        tr("Define the projected working CRS for point cloud coordinates and the geographic CRS used for longitude/latitude interoperability."),
        introCard);
    subtitleLabel->setObjectName(QStringLiteral("projectCrsValue"));
    subtitleLabel->setWordWrap(true);
    introLayout->addWidget(titleLabel);
    introLayout->addWidget(subtitleLabel);
    rootLayout->addWidget(introCard);

    auto buildSection = [this, rootLayout](
                            const QString& title,
                            QLabel*& nameValue,
                            QLabel*& codeValue,
                            QLabel*& typeValue,
                            QLabel*& summaryValue,
                            const std::function<void()>& chooseHandler,
                            const std::function<void()>& secondaryHandler,
                            const QString& secondaryText) {
        auto* card = createProjectCard(this);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(20, 18, 20, 18);
        layout->setSpacing(10);
        auto* headerLayout = new QHBoxLayout();
        auto* titleLabel = new QLabel(title, card);
        titleLabel->setObjectName(QStringLiteral("projectCrsTitle"));
        auto* chooseButton = new QPushButton(tr("Select..."), card);
        auto* secondaryButton = new QPushButton(secondaryText, card);
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch(1);
        headerLayout->addWidget(chooseButton);
        headerLayout->addWidget(secondaryButton);
        layout->addLayout(headerLayout);

        auto* grid = new QGridLayout();
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(8);
        auto addField = [card, grid](int row, const QString& labelText, QLabel*& valueLabel) {
            auto* label = new QLabel(labelText, card);
            label->setObjectName(QStringLiteral("projectCrsLabel"));
            valueLabel = new QLabel(card);
            valueLabel->setObjectName(QStringLiteral("projectCrsValue"));
            valueLabel->setWordWrap(true);
            valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            grid->addWidget(label, row, 0);
            grid->addWidget(valueLabel, row, 1);
        };
        addField(0, tr("Name"), nameValue);
        addField(1, tr("Authority ID"), codeValue);
        addField(2, tr("Type"), typeValue);
        addField(3, tr("Summary"), summaryValue);
        grid->setColumnStretch(1, 1);
        layout->addLayout(grid);
        rootLayout->addWidget(card);

        connect(chooseButton, &QPushButton::clicked, this, chooseHandler);
        connect(secondaryButton, &QPushButton::clicked, this, secondaryHandler);
    };

    buildSection(
        tr("Point Cloud CRS"),
        pointCloudNameValueLabel_,
        pointCloudCodeValueLabel_,
        pointCloudTypeValueLabel_,
        pointCloudSummaryValueLabel_,
        [this]() { choosePointCloudCrs(); },
        [this]() {
            coordinateSystems_.pointCloudCrs = CoordinateSystemRef();
            coordinateSystems_.pointCloudCrs.kind = CoordinateSystemKind::Projected;
            refreshUi();
        },
        tr("Clear"));

    buildSection(
        tr("Geographic CRS"),
        geographicNameValueLabel_,
        geographicCodeValueLabel_,
        geographicTypeValueLabel_,
        geographicSummaryValueLabel_,
        [this]() { chooseGeographicCrs(); },
        [this]() {
            coordinateSystems_.geographicCrs = defaultGeographicCoordinateSystem();
            refreshUi();
        },
        tr("Reset WGS84"));

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttonBox);
}

void ProjectCoordinateSystemsDialog::refreshUi()
{
    const auto fill = [this](const CoordinateSystemRef& crs, QLabel* name, QLabel* code, QLabel* type, QLabel* summary) {
        name->setText(crs.code > 0 ? CrsAuthorityService::displayName(crs) : tr("Unset"));
        code->setText(CrsAuthorityService::authorityText(crs));
        type->setText(coordinateSystemKindDisplayName(crs.kind));
        summary->setText(summaryFor(crs));
    };

    fill(coordinateSystems_.pointCloudCrs, pointCloudNameValueLabel_, pointCloudCodeValueLabel_, pointCloudTypeValueLabel_, pointCloudSummaryValueLabel_);
    fill(coordinateSystems_.geographicCrs.code > 0 ? coordinateSystems_.geographicCrs : defaultGeographicCoordinateSystem(),
        geographicNameValueLabel_, geographicCodeValueLabel_, geographicTypeValueLabel_, geographicSummaryValueLabel_);
}

QString ProjectCoordinateSystemsDialog::summaryFor(const CoordinateSystemRef& crs) const
{
    if (crs.code <= 0) {
        return tr("No coordinate system is assigned.");
    }
    CrsCatalogEntry entry;
    if (catalogService_.findByCode(crs.authName, crs.code, &entry)) {
        return entry.summary;
    }
    return tr("Stored project CRS without a matching common catalog entry.");
}

void ProjectCoordinateSystemsDialog::choosePointCloudCrs()
{
    CrsSelectionDialog dialog(this);
    dialog.setAllowedKindFilter(CoordinateSystemKindFilter::Projected);
    dialog.setCurrentCoordinateSystem(coordinateSystems_.pointCloudCrs);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    coordinateSystems_.pointCloudCrs = dialog.selectedCoordinateSystem();
    coordinateSystems_.pointCloudCrs.kind = CoordinateSystemKind::Projected;
    refreshUi();
}

void ProjectCoordinateSystemsDialog::chooseGeographicCrs()
{
    CrsSelectionDialog dialog(this);
    dialog.setAllowedKindFilter(CoordinateSystemKindFilter::Geographic);
    dialog.setCurrentCoordinateSystem(
        coordinateSystems_.geographicCrs.code > 0
            ? coordinateSystems_.geographicCrs
            : defaultGeographicCoordinateSystem());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    coordinateSystems_.geographicCrs = dialog.selectedCoordinateSystem();
    coordinateSystems_.geographicCrs.kind = CoordinateSystemKind::Geographic;
    refreshUi();
}
}
