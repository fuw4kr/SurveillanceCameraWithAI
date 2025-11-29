#include "faceDatabasePage.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QMetaObject>
#include <QIcon>
#include <QDebug>
#include <QImage>
#include <QTableWidget>
#include <algorithm>

namespace {
const QSize kCardIconSize(96, 96);
const QSize kDetailPreviewSize(240, 240);
}

FaceDatabasePage::FaceDatabasePage(AIProcessor* processor, QWidget* parent)
    : QWidget(parent)
    , aiProcessor(processor)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QLabel* header = new QLabel(tr("Face Database Manager"), this);
    header->setStyleSheet("font-size:20px; font-weight:600; color:#f8fafc;");
    mainLayout->addWidget(header);

    QLabel* subtitle = new QLabel(
        tr("Review all enrolled faces, rename them to real people, delete mistakes and merge duplicates."),
        this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet("color:#94a3b8;");
    mainLayout->addWidget(subtitle);

    QHBoxLayout* toolbar = new QHBoxLayout;
    toolbar->setSpacing(8);
    searchInput = new QLineEdit(this);
    searchInput->setPlaceholderText(tr("Filter by name..."));
    refreshButton = new QPushButton(tr("Refresh"), this);
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
    gallery->setStyleSheet("QListWidget { background:#0f172a; border:1px solid #1f2937; border-radius:12px; }");
    galleryLayout->addWidget(gallery, 1);

    infoLabel = new QLabel(this);
    infoLabel->setStyleSheet("color:#cbd5f5;");
    galleryLayout->addWidget(infoLabel);

    contentLayout->addLayout(galleryLayout, 2);

    QFrame* detailPanel = new QFrame(this);
    detailPanel->setObjectName("faceDetailPanel");
    detailPanel->setStyleSheet(R"(
        QFrame#faceDetailPanel {
            background:#0b1121;
            border:1px solid #1f2937;
            border-radius:12px;
        }
        QLineEdit { background:#020617; border:1px solid #1e293b; border-radius:8px; padding:6px 10px; color:#e2e8f0; }
        QPushButton {
            background:#2563eb;
            color:white;
            border:none;
            border-radius:8px;
            padding:8px 12px;
            font-weight:600;
        }
        QPushButton:disabled {
            background:#1e293b;
            color:#94a3b8;
        }
        QLabel { color:#e2e8f0; }
    )");

    QVBoxLayout* detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(16, 16, 16, 16);
    detailLayout->setSpacing(12);

    previewLabel = new QLabel(detailPanel);
    previewLabel->setFixedSize(kDetailPreviewSize);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet("background:#111827; border:1px dashed #1f2937; border-radius:12px; color:#64748b;");
    previewLabel->setText(tr("Select a profile"));
    detailLayout->addWidget(previewLabel, 0, Qt::AlignHCenter);

    samplesLabel = new QLabel(tr("No selection"), detailPanel);
    samplesLabel->setStyleSheet("color:#94a3b8;");
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

    mergeButton = new QPushButton(tr("Merge Selected"), detailPanel);
    detailLayout->addWidget(mergeButton);

    statusLabel = new QLabel(detailPanel);
    statusLabel->setStyleSheet("color:#a7f3d0; font-size:12px;");
    statusLabel->setWordWrap(true);
    detailLayout->addWidget(statusLabel);

    contentLayout->addWidget(detailPanel, 1);
    mainLayout->addLayout(contentLayout, 1);

    QGroupBox* cloudGroup = new QGroupBox(tr("Cloud Directory (remote persons)"), this);
    QVBoxLayout* cloudLayout = new QVBoxLayout(cloudGroup);
    cloudLayout->setContentsMargins(12, 12, 12, 12);
    cloudLayout->setSpacing(8);

    QHBoxLayout* cloudToolbar = new QHBoxLayout;
    cloudToolbar->setSpacing(8);
    remoteStatusLabel = new QLabel(tr("Waiting for sync..."), cloudGroup);
    remoteRefreshButton = new QPushButton(tr("Reload"), cloudGroup);
    cloudToolbar->addWidget(remoteStatusLabel, 1);
    cloudToolbar->addWidget(remoteRefreshButton, 0, Qt::AlignRight);
    cloudLayout->addLayout(cloudToolbar);

    remotePersonsTable = new QTableWidget(0, 5, cloudGroup);
    QStringList remoteHeaders = { tr("Name"), tr("Role"), tr("Authorized"), tr("Last Seen"), tr("Registered") };
    remotePersonsTable->setHorizontalHeaderLabels(remoteHeaders);
    remotePersonsTable->horizontalHeader()->setStretchLastSection(true);
    remotePersonsTable->verticalHeader()->hide();
    remotePersonsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    remotePersonsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    remotePersonsTable->setAlternatingRowColors(true);
    remotePersonsTable->setStyleSheet(QStringLiteral(
        "QTableWidget { background:#0f172a; border:1px solid #1f2937; border-radius:10px; }"
        "QHeaderView::section { background:#1e293b; color:#e2e8f0; border:none; }"));
    cloudLayout->addWidget(remotePersonsTable);
    mainLayout->addWidget(cloudGroup);

    if (aiProcessor) {
        connect(aiProcessor, &AIProcessor::faceDatabaseChanged,
            this, &FaceDatabasePage::handleFaceDatabaseChanged, Qt::QueuedConnection);
    }

    connect(refreshButton, &QPushButton::clicked, this, &FaceDatabasePage::refreshProfiles);
    connect(searchInput, &QLineEdit::textChanged, this, &FaceDatabasePage::handleSearchChanged);
    connect(gallery, &QListWidget::itemSelectionChanged, this, &FaceDatabasePage::handleSelectionChanged);
    connect(renameButton, &QPushButton::clicked, this, &FaceDatabasePage::handleRename);
    connect(deleteButton, &QPushButton::clicked, this, &FaceDatabasePage::handleDelete);
    connect(mergeButton, &QPushButton::clicked, this, &FaceDatabasePage::handleMerge);
    connect(nameEdit, &QLineEdit::textChanged, this, &FaceDatabasePage::handleNameEdited);
    connect(remoteRefreshButton, &QPushButton::clicked, this, &FaceDatabasePage::requestCloudRefresh);

    renameButton->setEnabled(false);
    deleteButton->setEnabled(false);
    mergeButton->setEnabled(false);
    setStatusMessage(QString());

    refreshProfiles();
}

void FaceDatabasePage::setRemotePersons(const QList<PersonRecord>& persons)
{
    remotePersons = persons;
    updateRemotePersonsTable();
    qInfo() << "[FaceDatabase]" << "Remote directory updated:" << remotePersons.size() << "records";
}

QVector<AIProcessor::FaceProfile> FaceDatabasePage::fetchProfiles() const
{
    QVector<AIProcessor::FaceProfile> profiles;
    if (!aiProcessor)
        return profiles;

    const bool ok = QMetaObject::invokeMethod(aiProcessor, "listFaceProfiles",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(QVector<AIProcessor::FaceProfile>, profiles));
    if (!ok)
        qWarning() << "Failed to retrieve face database snapshot";
    return profiles;
}

void FaceDatabasePage::refreshProfiles()
{
    if (!aiProcessor) {
        cachedProfiles.clear();
    } else {
        cachedProfiles = fetchProfiles();
    }
    rebuildGallery();
    if (!cachedProfiles.isEmpty())
        setStatusMessage(tr("Loaded %1 face profiles").arg(cachedProfiles.size()));
    else
        setStatusMessage(tr("No enrolled faces yet. They'll appear here automatically."));
    qInfo() << "[FaceDatabase]" << "Reloaded local profiles:" << cachedProfiles.size();
}

void FaceDatabasePage::rebuildGallery()
{
    if (!gallery)
        return;

    const QString filter = currentFilter.trimmed().toLower();
    const QStringList previousSelection = selectedIds();
    updatingSelection = true;
    gallery->clear();

    for (const auto& profile : cachedProfiles) {
        if (!filter.isEmpty() && !profile.name.toLower().contains(filter))
            continue;
        auto* item = new QListWidgetItem;
        item->setData(Qt::UserRole, profile.id);
        item->setText(profile.name);
        item->setToolTip(profile.name);
        item->setIcon(QIcon(buildFacePixmap(profile, kCardIconSize)));
        gallery->addItem(item);
        if (previousSelection.contains(profile.id))
            item->setSelected(true);
    }

    updatingSelection = false;
    const int total = cachedProfiles.size();
    const int shown = gallery->count();
    if (infoLabel)
        infoLabel->setText(tr("%1 saved faces (showing %2)").arg(total).arg(shown));
    updateDetailPanel();
}

QStringList FaceDatabasePage::selectedIds() const
{
    QStringList ids;
    if (!gallery)
        return ids;
    const auto items = gallery->selectedItems();
    ids.reserve(items.size());
    for (auto* item : items) {
        ids.append(item->data(Qt::UserRole).toString());
    }
    return ids;
}

QString FaceDatabasePage::selectedPrimaryId() const
{
    if (!gallery)
        return {};
    if (QListWidgetItem* current = gallery->currentItem())
        return current->data(Qt::UserRole).toString();
    const QStringList ids = selectedIds();
    return ids.isEmpty() ? QString() : ids.first();
}

AIProcessor::FaceProfile FaceDatabasePage::profileForId(const QString& id) const
{
    for (const auto& profile : cachedProfiles) {
        if (profile.id == id)
            return profile;
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
            previewLabel->setText(tr("%1 faces selected").arg(ids.size()));
            samplesLabel->setText(tr("Multiple selection"));
        } else {
            previewLabel->setPixmap(QPixmap());
            previewLabel->setText(tr("Select a profile"));
            samplesLabel->setText(tr("No selection"));
        }
        nameEdit->clear();
    } else {
        const auto profile = profileForId(ids.first());
        QPixmap pix = buildFacePixmap(profile, kDetailPreviewSize);
        previewLabel->setPixmap(pix);
        previewLabel->setText(QString());
        samplesLabel->setText(tr("Samples captured: %1").arg(std::max(1, profile.sampleCount)));
        const QSignalBlocker blocker(nameEdit);
        nameEdit->setText(profile.name);
    }

    renameButton->setEnabled(single && !nameEdit->text().trimmed().isEmpty());
    deleteButton->setEnabled(hasSelection);
    mergeButton->setEnabled(ids.size() >= 2);
}

QPixmap FaceDatabasePage::buildFacePixmap(const AIProcessor::FaceProfile& profile, const QSize& size) const
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect rect = pixmap.rect();
    painter.setBrush(QColor("#1f2937"));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect, 12, 12);

    bool drewImage = false;
    if (!profile.previewPath.isEmpty()) {
        QImage image(profile.previewPath);
        if (!image.isNull()) {
            QImage scaled = image.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            QRect targetRect = rect.adjusted(4, 4, -4, -4);
            QPainterPath path;
            path.addRoundedRect(targetRect, 10, 10);
            painter.save();
            painter.setClipPath(path);
            painter.drawImage(targetRect, scaled);
            painter.restore();
            drewImage = true;
        }
    }

    if (!drewImage) {
        QPen pen(QColor("#cbd5f5"));
        painter.setPen(pen);
        QFont font = painter.font();
        font.setBold(true);
        font.setPointSize(std::max(10, size.height() / 3));
        painter.setFont(font);
        const QString initial = profile.name.isEmpty()
            ? QStringLiteral("?")
            : profile.name.left(1).toUpper();
        painter.drawText(rect, Qt::AlignCenter, initial);
    }
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
    const QStringList ids = selectedIds();
    if (ids.size() != 1 || !aiProcessor)
        return;
    const QString newName = nameEdit->text().trimmed();
    if (newName.isEmpty())
        return;

    bool success = false;
    const QString id = ids.first();
    if (!QMetaObject::invokeMethod(aiProcessor, "renameFaceProfile",
            Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(bool, success),
            Q_ARG(QString, id),
            Q_ARG(QString, newName))) {
        success = false;
    }

    if (!success) {
        setStatusMessage(tr("Failed to rename profile"), true);
        return;
    }

    setStatusMessage(tr("Saved \"%1\"").arg(newName));
    refreshProfiles();
    qInfo() << "[FaceDatabase]" << "Profile" << id << "renamed to" << newName;
}

void FaceDatabasePage::handleDelete()
{
    const QStringList ids = selectedIds();
    if (ids.isEmpty() || !aiProcessor)
        return;

    const auto response = QMessageBox::question(this, tr("Delete Faces"),
        tr("Remove %1 selected entr%2 from the database?")
        .arg(ids.size())
        .arg(ids.size() == 1 ? "y" : "ies"));
    if (response != QMessageBox::Yes)
        return;

    bool allOk = true;
    for (const QString& id : ids) {
        bool success = false;
        if (!QMetaObject::invokeMethod(aiProcessor, "deleteFaceProfile",
                Qt::BlockingQueuedConnection,
                Q_RETURN_ARG(bool, success),
                Q_ARG(QString, id))) {
            success = false;
        }
        allOk = allOk && success;
    }

    refreshProfiles();
    if (allOk)
        setStatusMessage(tr("Deleted %1 profile(s)").arg(ids.size()));
    else
        setStatusMessage(tr("Some entries could not be deleted"), true);
    qInfo() << "[FaceDatabase]" << "Delete requested for" << ids.size() << "profile(s). success=" << allOk;
}

void FaceDatabasePage::handleMerge()
{
    const QStringList ids = selectedIds();
    if (ids.size() < 2 || !aiProcessor)
        return;

    const QString primaryId = selectedPrimaryId();
    if (primaryId.isEmpty())
        return;

    QStringList duplicates = ids;
    duplicates.removeAll(primaryId);
    const auto primaryProfile = profileForId(primaryId);
    const auto response = QMessageBox::question(this, tr("Merge Faces"),
        tr("Merge %1 selected faces into \"%2\"? "
           "Embeddings will be averaged so the recognizer learns from all samples.")
        .arg(ids.size())
        .arg(primaryProfile.name));
    if (response != QMessageBox::Yes)
        return;

    bool success = false;
    if (!QMetaObject::invokeMethod(aiProcessor, "mergeFaceProfiles",
            Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(bool, success),
            Q_ARG(QString, primaryId),
            Q_ARG(QStringList, duplicates))) {
        success = false;
    }

    if (!success) {
        setStatusMessage(tr("Failed to merge faces"), true);
        return;
    }

    setStatusMessage(tr("Merged %1 faces into \"%2\"").arg(ids.size()).arg(primaryProfile.name));
    refreshProfiles();
    qInfo() << "[FaceDatabase]" << "Merged" << ids.size() << "profiles into" << primaryProfile.name;
}

void FaceDatabasePage::handleNameEdited(const QString&)
{
    const bool single = selectedIds().size() == 1;
    renameButton->setEnabled(single && !nameEdit->text().trimmed().isEmpty());
}

void FaceDatabasePage::handleFaceDatabaseChanged()
{
    refreshProfiles();
}

void FaceDatabasePage::setStatusMessage(const QString& text, bool isError)
{
    if (!statusLabel)
        return;
    if (text.isEmpty())
        statusLabel->clear();
    else {
        statusLabel->setStyleSheet(isError ? "color:#f87171; font-size:12px;"
                                           : "color:#a7f3d0; font-size:12px;");
        statusLabel->setText(text);
    }
}

void FaceDatabasePage::updateRemotePersonsTable()
{
    if (!remotePersonsTable)
        return;
    remotePersonsTable->setRowCount(remotePersons.size());
    const QString yesText = tr("Yes");
    const QString noText = tr("No");

    for (int row = 0; row < remotePersons.size(); ++row) {
        const auto& person = remotePersons.at(row);
        auto setItem = [&](int column, const QString& value) {
            auto* item = new QTableWidgetItem(value);
            remotePersonsTable->setItem(row, column, item);
        };
        setItem(0, person.name.isEmpty() ? tr("Unnamed") : person.name);
        setItem(1, person.role.isEmpty() ? tr("Unknown") : person.role);
        setItem(2, person.authorized ? yesText : noText);
        const QString lastSeen = person.lastSeen.isValid()
            ? person.lastSeen.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : tr("Never");
        setItem(3, lastSeen);
        const QString registered = person.registeredAt.isValid()
            ? person.registeredAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
            : QString();
        setItem(4, registered);
    }

    if (remoteStatusLabel)
        remoteStatusLabel->setText(tr("Cloud persons: %1").arg(remotePersons.size()));
}
