#include "faceDatabasePage.h"

#include "../../core/ServerSyncManager.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListView>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>

namespace {
const QSize kCardIconSize(96, 96);
const QSize kDetailPreviewSize(240, 240);
}

FaceDatabasePage::FaceDatabasePage(ServerSyncManager* sync, QWidget* parent)
    : QWidget(parent)
    , serverSync(sync)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* header = new QLabel(tr("Cloud Face Directory"), this);
    header->setStyleSheet(QStringLiteral("font-size:20px; font-weight:600; color:#f8fafc;"));
    mainLayout->addWidget(header);

    auto* subtitle = new QLabel(
        tr("Browse and manage all people stored on the server. Rename them to real names or delete obsolete entries."),
        this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(QStringLiteral("color:#94a3b8;"));
    mainLayout->addWidget(subtitle);

    QHBoxLayout* toolbar = new QHBoxLayout;
    toolbar->setSpacing(8);
    searchInput = new QLineEdit(this);
    searchInput->setPlaceholderText(tr("Filter by name..."));
    refreshButton = new QPushButton(tr("Sync now"), this);
    toolbar->addWidget(searchInput, 1);
    toolbar->addWidget(refreshButton, 0, Qt::AlignRight);
    mainLayout->addLayout(toolbar);

    QHBoxLayout* contentLayout = new QHBoxLayout;
    contentLayout->setSpacing(16);

    QVBoxLayout* galleryLayout = new QVBoxLayout;
    galleryLayout->setSpacing(8);
    gallery = new QListWidget(this);
    gallery->setViewMode(QListView::IconMode);
    gallery->setIconSize(kCardIconSize);
    gallery->setGridSize(QSize(140, 160));
    gallery->setResizeMode(QListView::Adjust);
    gallery->setMovement(QListView::Static);
    gallery->setWordWrap(true);
    gallery->setSpacing(16);
    gallery->setSelectionMode(QAbstractItemView::ExtendedSelection);
    gallery->setStyleSheet(QStringLiteral("QListWidget { background:#0f172a; border:1px solid #1f2937; border-radius:12px; }"));
    galleryLayout->addWidget(gallery, 1);

    infoLabel = new QLabel(this);
    infoLabel->setStyleSheet(QStringLiteral("color:#cbd5f5;"));
    galleryLayout->addWidget(infoLabel);

    contentLayout->addLayout(galleryLayout, 2);

    auto* detailPanel = new QFrame(this);
    detailPanel->setObjectName(QStringLiteral("faceDetailPanel"));
    detailPanel->setStyleSheet(QStringLiteral(
        "QFrame#faceDetailPanel { background:#0b1121; border:1px solid #1f2937; border-radius:12px; }"
        "QLineEdit { background:#020617; border:1px solid #1e293b; border-radius:8px; padding:6px 10px; color:#e2e8f0; }"
        "QPushButton { background:#2563eb; color:white; border:none; border-radius:8px; padding:8px 12px; font-weight:600; }"
        "QPushButton:disabled { background:#1e293b; color:#94a3b8; }"
        "QLabel { color:#e2e8f0; }"));

    QVBoxLayout* detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(16, 16, 16, 16);
    detailLayout->setSpacing(12);

    previewLabel = new QLabel(detailPanel);
    previewLabel->setFixedSize(kDetailPreviewSize);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet(QStringLiteral("background:#111827; border:1px dashed #1f2937; border-radius:12px; color:#64748b;"));
    previewLabel->setText(tr("Select a person"));
    detailLayout->addWidget(previewLabel, 0, Qt::AlignHCenter);

    samplesLabel = new QLabel(tr("No selection"), detailPanel);
    samplesLabel->setStyleSheet(QStringLiteral("color:#94a3b8;"));
    detailLayout->addWidget(samplesLabel, 0, Qt::AlignHCenter);

    nameEdit = new QLineEdit(detailPanel);
    nameEdit->setPlaceholderText(tr("Display name (e.g. Ivan Director)"));
    detailLayout->addWidget(nameEdit);

    QHBoxLayout* actionRow = new QHBoxLayout;
    actionRow->setSpacing(8);
    renameButton = new QPushButton(tr("Rename"), detailPanel);
    deleteButton = new QPushButton(tr("Delete"), detailPanel);
    actionRow->addWidget(renameButton);
    actionRow->addWidget(deleteButton);
    detailLayout->addLayout(actionRow);

    statusLabel = new QLabel(detailPanel);
    statusLabel->setStyleSheet(QStringLiteral("color:#a7f3d0; font-size:12px;"));
    statusLabel->setWordWrap(true);
    detailLayout->addWidget(statusLabel);

    contentLayout->addWidget(detailPanel, 1);
    mainLayout->addLayout(contentLayout, 1);

    connect(refreshButton, &QPushButton::clicked, this, &FaceDatabasePage::handleRefreshClicked);
    connect(searchInput, &QLineEdit::textChanged, this, &FaceDatabasePage::handleSearchChanged);
    connect(gallery, &QListWidget::itemSelectionChanged, this, &FaceDatabasePage::handleSelectionChanged);
    connect(renameButton, &QPushButton::clicked, this, &FaceDatabasePage::handleRename);
    connect(deleteButton, &QPushButton::clicked, this, &FaceDatabasePage::handleDelete);
    connect(nameEdit, &QLineEdit::textChanged, this, &FaceDatabasePage::handleNameEdited);

    renameButton->setEnabled(false);
    deleteButton->setEnabled(false);
    setStatusMessage(QString());
}

void FaceDatabasePage::setRemotePersons(const QList<PersonRecord>& persons)
{
    remotePersons = persons;
    rebuildGallery();
    if (infoLabel)
        infoLabel->setText(tr("Server profiles: %1").arg(remotePersons.size()));
    qInfo() << "[FaceDatabase]" << "Remote directory updated:" << remotePersons.size() << "records";
}

void FaceDatabasePage::handleRefreshClicked()
{
    setStatusMessage(tr("Requesting sync..."));
    emit requestCloudRefresh();
}

void FaceDatabasePage::rebuildGallery()
{
    if (!gallery)
        return;

    const QString filter = currentFilter.trimmed().toLower();
    const QStringList previousSelection = selectedIds();
    updatingSelection = true;
    gallery->clear();

    for (const auto& person : remotePersons) {
        if (!filter.isEmpty() && !person.name.toLower().contains(filter))
            continue;
        auto* item = new QListWidgetItem;
        item->setData(Qt::UserRole, person.id);
        const QString display = person.name.isEmpty() ? tr("Unnamed") : person.name;
        item->setText(display);
        item->setToolTip(tr("%1 (%2)").arg(display, person.role.isEmpty() ? tr("No role") : person.role));
        item->setIcon(QIcon(buildFacePixmap(person, kCardIconSize)));
        gallery->addItem(item);
        if (previousSelection.contains(person.id))
            item->setSelected(true);
    }

    updatingSelection = false;
    updateDetailPanel();
}

QStringList FaceDatabasePage::selectedIds() const
{
    QStringList ids;
    if (!gallery)
        return ids;
    const auto items = gallery->selectedItems();
    ids.reserve(items.size());
    for (auto* item : items)
        ids.append(item->data(Qt::UserRole).toString());
    return ids;
}

QString FaceDatabasePage::selectedPrimaryId() const
{
    if (!gallery)
        return {};
    if (auto* current = gallery->currentItem())
        return current->data(Qt::UserRole).toString();
    const QStringList ids = selectedIds();
    return ids.isEmpty() ? QString() : ids.first();
}

PersonRecord FaceDatabasePage::personById(const QString& id) const
{
    for (const auto& person : remotePersons) {
        if (person.id == id)
            return person;
    }
    return {};
}

void FaceDatabasePage::updateDetailPanel()
{
    if (!previewLabel || !nameEdit)
        return;
    const QStringList ids = selectedIds();
    const bool single = ids.size() == 1;
    const bool hasSelection = !ids.isEmpty();

    if (!single) {
        QSignalBlocker blocker(nameEdit);
        if (hasSelection) {
            previewLabel->setPixmap(QPixmap());
            previewLabel->setText(tr("%1 people selected").arg(ids.size()));
            samplesLabel->setText(tr("Multiple selection"));
        } else {
            previewLabel->setPixmap(QPixmap());
            previewLabel->setText(tr("Select a person"));
            samplesLabel->setText(tr("No selection"));
        }
        nameEdit->clear();
    } else {
        const auto person = personById(ids.first());
        QPixmap pix = buildFacePixmap(person, kDetailPreviewSize);
        previewLabel->setPixmap(pix);
        previewLabel->setText(QString());
        QString meta = tr("Role: %1").arg(person.role.isEmpty() ? tr("Unknown") : person.role);
        meta += tr(" • Access: %1").arg(person.authorized ? tr("Allowed") : tr("Restricted"));
        if (person.lastSeen.isValid())
            meta += tr("\nLast seen: %1").arg(person.lastSeen.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        samplesLabel->setText(meta);
        const QSignalBlocker blocker(nameEdit);
        nameEdit->setText(person.name);
    }

    renameButton->setEnabled(single && !nameEdit->text().trimmed().isEmpty());
    deleteButton->setEnabled(hasSelection);
}

QPixmap FaceDatabasePage::buildFacePixmap(const PersonRecord& person, const QSize& size) const
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect rect = pixmap.rect();
    painter.setBrush(QColor("#1f2937"));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect, 12, 12);

    QPen pen(QColor("#cbd5f5"));
    painter.setPen(pen);
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(std::max(10, size.height() / 3));
    painter.setFont(font);
    const QString initial = person.name.isEmpty()
        ? QStringLiteral("?")
        : person.name.left(1).toUpper();
    painter.drawText(rect, Qt::AlignCenter, initial);
    return pixmap;
}

void FaceDatabasePage::handleSearchChanged(const QString& text)
{
    currentFilter = text;
    rebuildGallery();
}

void FaceDatabasePage::handleSelectionChanged()
{
    if (updatingSelection)
        return;
    updateDetailPanel();
}

void FaceDatabasePage::handleRename()
{
    if (!serverSync)
        return;
    const QStringList ids = selectedIds();
    if (ids.size() != 1)
        return;
    const QString newName = nameEdit->text().trimmed();
    if (newName.isEmpty())
        return;
    const PersonRecord person = personById(ids.first());
    if (person.id.isEmpty())
        return;

    serverSync->renamePerson(person.id, newName);
    setStatusMessage(tr("Renaming \"%1\"...").arg(person.name));
}

void FaceDatabasePage::handleDelete()
{
    if (!serverSync)
        return;
    const QStringList ids = selectedIds();
    if (ids.isEmpty())
        return;
    const auto response = QMessageBox::question(this, tr("Delete People"),
        tr("Remove %1 selected entr%2 from the server?")
            .arg(ids.size())
            .arg(ids.size() == 1 ? "y" : "ies"));
    if (response != QMessageBox::Yes)
        return;
    for (const QString& id : ids)
        serverSync->deletePerson(id);
    setStatusMessage(tr("Delete requested for %1 entr%2").arg(ids.size()).arg(ids.size() == 1 ? "y" : "ies"));
}

void FaceDatabasePage::handleNameEdited(const QString&)
{
    const bool single = selectedIds().size() == 1;
    renameButton->setEnabled(single && !nameEdit->text().trimmed().isEmpty());
}

void FaceDatabasePage::setStatusMessage(const QString& text, bool isError)
{
    if (!statusLabel)
        return;
    if (text.isEmpty()) {
        statusLabel->clear();
        return;
    }
    statusLabel->setStyleSheet(isError ? QStringLiteral("color:#f87171; font-size:12px;")
                                       : QStringLiteral("color:#a7f3d0; font-size:12px;"));
    statusLabel->setText(text);
}
