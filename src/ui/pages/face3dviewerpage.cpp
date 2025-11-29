/**
 * @file face3dviewerpage.cpp
 * @brief Implements the face 3D viewer page that reconstructs meshes from images.
 */
#include "face3dviewerpage.h"

#include "../widgets/faceMeshView.h"
#include "../../core/ServerSyncManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageReader>
#include <QLabel>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include <opencv2/imgproc.hpp>

namespace {
const QSize kPreviewSize(280, 280);
const QSize kIconSize(80, 80);

QImage safeLoadImage(const QString& path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    return reader.read();
}

}

Face3DViewerPage::Face3DViewerPage(AIProcessor* processor, ServerSyncManager* sync, QWidget* parent)
    : QWidget(parent)
    , aiProcessor(processor)
    , serverSync(sync)
{
    imageLoader = new QNetworkAccessManager(this);
    buildUi();
    attemptModelLoad();
    handleRefreshProfiles();

    if (aiProcessor) {
        connect(aiProcessor, &AIProcessor::faceDatabaseChanged,
            this, &Face3DViewerPage::handleDatabaseChanged, Qt::QueuedConnection);
    }
    if (serverSync) {
        connect(serverSync, &ServerSyncManager::personsUpdated,
            this, &Face3DViewerPage::setRemotePersons);
    }
}

void Face3DViewerPage::buildUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    auto* sidebar = new QVBoxLayout;
    sidebar->setSpacing(12);

    QLabel* title = new QLabel(tr("3D Face Viewer"), this);
    title->setStyleSheet("font-size:20px; font-weight:600; color:#e2e8f0;");
    sidebar->addWidget(title);

    QLabel* subtitle = new QLabel(
        tr("Select any enrolled person or load a photo to rebuild a 3D landmark cloud using the 1k3d68 model."),
        this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet("color:#94a3b8;");
    sidebar->addWidget(subtitle);

    loadImageButton = new QPushButton(tr("Load Image..."), this);
    refreshButton = new QPushButton(tr("Refresh Faces"), this);
    loadImageButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    refreshButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QHBoxLayout* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    buttonRow->addWidget(loadImageButton);
    buttonRow->addWidget(refreshButton);
    sidebar->addLayout(buttonRow);

    previewLabel = new QLabel(this);
    previewLabel->setFixedSize(kPreviewSize);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet("background:#0f172a; border:1px solid #1f2937; border-radius:12px; color:#475569;");
    previewLabel->setText(tr("No image"));
    sidebar->addWidget(previewLabel, 0, Qt::AlignHCenter);

    profileList = new QListWidget(this);
    profileList->setViewMode(QListView::IconMode);
    profileList->setIconSize(kIconSize);
    profileList->setGridSize(QSize(120, 140));
    profileList->setResizeMode(QListView::Adjust);
    profileList->setMovement(QListView::Static);
    profileList->setSelectionMode(QAbstractItemView::SingleSelection);
    profileList->setSpacing(12);
    profileList->setStyleSheet("QListWidget { background:#0a0f1f; border:1px solid #1f2937; border-radius:12px; }"
                               "QListWidget::item { color:#e2e8f0; }");
    sidebar->addWidget(profileList, 1);

    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    statusLabel->setStyleSheet("color:#cbd5f5;");
    sidebar->addWidget(statusLabel);

    mainLayout->addLayout(sidebar, 1);

    meshView = new FaceMeshView(this);
    meshView->setMinimumSize(QSize(500, 400));
    mainLayout->addWidget(meshView, 2);

    connect(loadImageButton, &QPushButton::clicked, this, &Face3DViewerPage::handleLoadImage);
    connect(refreshButton, &QPushButton::clicked, this, &Face3DViewerPage::handleRefreshProfiles);
    connect(profileList, &QListWidget::itemSelectionChanged, this, &Face3DViewerPage::handleProfileSelection);
}

void Face3DViewerPage::attemptModelLoad()
{
    const QString modelPath = locateModel();
    if (modelPath.isEmpty()) {
        setStatus(tr("1k3d68.onnx not found in assets/models. Please copy the model first."), true);
        return;
    }

    if (!reconModel.loadModel(modelPath)) {
        setStatus(tr("Failed to load 3D model: %1").arg(reconModel.lastError()), true);
        return;
    }

    modelReady = true;
    setStatus(tr("Model %1 ready (%2x%3 input).")
                  .arg(QFileInfo(modelPath).fileName())
                  .arg(reconModel.inputResolution().width())
                  .arg(reconModel.inputResolution().height()));
}

QString Face3DViewerPage::locateModel() const
{
    const QString binDir = QCoreApplication::applicationDirPath();
    const QStringList relativeCandidates = {
        QStringLiteral("/assets/models/1k3d68.onnx"),
        QStringLiteral("/assets/models/buffalo_s/1k3d68.onnx"),
        QStringLiteral("/assets/1k3d68.onnx")
    };
    const QStringList prefixes { QString(), QStringLiteral("/.."), QStringLiteral("/../.."), QStringLiteral("/../../..") };

    for (const QString& prefix : prefixes) {
        for (const QString& rel : relativeCandidates) {
            const QString candidate = QDir::cleanPath(binDir + prefix + rel);
            if (QFileInfo::exists(candidate))
                return QFileInfo(candidate).absoluteFilePath();
        }
    }
    return {};
}

void Face3DViewerPage::handleLoadImage()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select face photo"),
        QDir::homePath(), tr("Images (*.png *.jpg *.jpeg *.bmp);;All Files (*.*)"));
    if (path.isEmpty())
        return;

    QImage image = safeLoadImage(path);
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Image"), tr("Unable to read %1").arg(path));
        return;
    }

    applyImageForReconstruction(image, QFileInfo(path).fileName());
}

void Face3DViewerPage::handleRefreshProfiles()
{
    if (serverSync) {
        setStatus(tr("Requesting latest server faces..."));
        serverSync->requestImmediatePersonsRefresh();
        return;
    }

    cachedProfiles = fetchProfiles();
    rebuildProfileList();
    if (cachedProfiles.isEmpty())
        setStatus(tr("Local face database is empty. Add faces to see them here."));
}

void Face3DViewerPage::handleProfileSelection()
{
    auto* item = profileList->currentItem();
    if (!item)
        return;

    const QString id = item->data(Qt::UserRole).toString();
    if (id.isEmpty())
        return;
    if (!remotePersons.isEmpty()) {
        const PersonRecord person = remotePersonById(id);
        if (person.id.isEmpty())
            return;
        const QString label = person.name.isEmpty() ? person.id : person.name;
        if (remoteImages.contains(person.id)) {
            applyImageForReconstruction(remoteImages.value(person.id), label);
        } else {
            ensureRemoteAvatar(person);
            setStatus(tr("Downloading photo for %1...").arg(label));
        }
        return;
    }

    const AIProcessor::FaceProfile profile = profileById(id);
    if (profile.id.isEmpty())
        return;

    QImage image = safeLoadImage(profile.previewPath);
    if (image.isNull()) {
        setStatus(tr("Preview file for %1 is missing (%2)").arg(profile.name, profile.previewPath), true);
        return;
    }
    applyImageForReconstruction(image, profile.name);
}

void Face3DViewerPage::handleDatabaseChanged()
{
    if (!serverSync)
        handleRefreshProfiles();
}

void Face3DViewerPage::setRemotePersons(const QList<PersonRecord>& persons)
{
    remotePersons = persons;
    QSet<QString> valid;
    for (const auto& person : remotePersons) {
        valid.insert(person.id);
        ensureRemoteAvatar(person);
    }
    // drop cached images that no longer exist
    for (auto it = remoteImages.begin(); it != remoteImages.end();) {
        if (!valid.contains(it.key()))
            it = remoteImages.erase(it);
        else
            ++it;
    }
    rebuildProfileList();
    if (!remotePersons.isEmpty())
        setStatus(tr("Loaded %1 server faces.").arg(remotePersons.size()));
}

QVector<AIProcessor::FaceProfile> Face3DViewerPage::fetchProfiles() const
{
    QVector<AIProcessor::FaceProfile> profiles;
    if (!aiProcessor)
        return profiles;

    const bool ok = QMetaObject::invokeMethod(aiProcessor, "listFaceProfiles",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(QVector<AIProcessor::FaceProfile>, profiles));
    if (!ok)
        qWarning() << "Face3DViewerPage: failed to fetch profiles";
    return profiles;
}

void Face3DViewerPage::rebuildProfileList()
{
    if (!profileList)
        return;
    profileList->clear();
    if (!remotePersons.isEmpty()) {
        rebuildRemoteList();
        return;
    }
    rebuildLocalList();
}

void Face3DViewerPage::applyImageForReconstruction(const QImage& image, const QString& label)
{
    if (image.isNull())
        return;
    currentImage = image;
    currentLabel = label;
    if (previewLabel) {
        previewLabel->setPixmap(buildPreviewPixmap(image, kPreviewSize));
        previewLabel->setText(QString());
    }
    runReconstruction(image, label);
}
void Face3DViewerPage::runReconstruction(const QImage& image, const QString& label)
{
    if (!modelReady) {
        setStatus(tr("3D model is not ready yet."), true);
        return;
    }

    cv::Mat face = toBgrMat(image);
    QString error;
    const QVector<QVector3D> points = reconModel.reconstruct(face, &error);
    if (points.isEmpty()) {
        meshView->clear();
        setStatus(tr("Unable to build 3D face for %1: %2").arg(label, error), true);
        return;
    }

    meshView->setPoints(points);
    setStatus(tr("Rendered %1 points for %2").arg(points.size()).arg(label));
}

void Face3DViewerPage::setStatus(const QString& text, bool isError)
{
    if (!statusLabel)
        return;
    statusLabel->setText(text);
    if (isError)
        statusLabel->setStyleSheet("color:#f87171;");
    else
        statusLabel->setStyleSheet("color:#cbd5f5;");
}

QPixmap Face3DViewerPage::buildPreviewPixmap(const QImage& image, const QSize& target)
{
    if (image.isNull())
        return QPixmap();
    const QImage scaled = image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap pixmap(target);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QRect drawRect(QPoint(0, 0), scaled.size());
    drawRect.moveCenter(QRect(QPoint(0, 0), target).center());
    painter.drawImage(drawRect, scaled);
    return pixmap;
}

QPixmap Face3DViewerPage::buildInitialsPixmap(const QString& label, const QSize& target)
{
    QPixmap pixmap(target);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor("#1f2937"));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(pixmap.rect(), 10, 10);
    painter.setPen(QColor("#cbd5f5"));
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(std::max(10, target.height() / 3));
    painter.setFont(font);
    const QString letter = label.isEmpty() ? QStringLiteral("?") : label.left(1).toUpper();
    painter.drawText(pixmap.rect(), Qt::AlignCenter, letter);
    return pixmap;
}

cv::Mat Face3DViewerPage::toBgrMat(const QImage& image)
{
    if (image.isNull())
        return {};
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3, const_cast<uchar*>(rgb.bits()), rgb.bytesPerLine());
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
    return bgr;
}

AIProcessor::FaceProfile Face3DViewerPage::profileById(const QString& id) const
{
    for (const auto& profile : cachedProfiles) {
        if (profile.id == id)
            return profile;
    }
    return {};
}

PersonRecord Face3DViewerPage::remotePersonById(const QString& id) const
{
    for (const auto& person : remotePersons) {
        if (person.id == id)
            return person;
    }
    return {};
}

QString Face3DViewerPage::resolveImageUrl(const QString& imageUrl) const
{
    if (imageUrl.isEmpty())
        return {};
    QUrl url(imageUrl);
    if (url.scheme().isEmpty() && serverSync) {
        const QUrl base = serverSync->baseUrl();
        if (base.isValid())
            url = base.resolved(QUrl(imageUrl));
    }
    return url.isValid() ? url.toString() : QString();
}

void Face3DViewerPage::ensureRemoteAvatar(const PersonRecord& person)
{
    if (!imageLoader || person.id.isEmpty())
        return;
    if (remoteImages.contains(person.id) || pendingDownloads.contains(person.id))
        return;
    const QString urlString = resolveImageUrl(person.imageUrl);
    if (urlString.isEmpty())
        return;
    QNetworkReply* reply = imageLoader->get(QNetworkRequest(QUrl(urlString)));
    pendingDownloads.insert(person.id);
    reply->setProperty("personId", person.id);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QString personId = reply->property("personId").toString();
        pendingDownloads.remove(personId);
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[Face3DViewer]" << "Avatar download failed for" << personId << reply->errorString();
            reply->deleteLater();
            return;
        }
        QImage image;
        image.loadFromData(reply->readAll());
        reply->deleteLater();
        if (image.isNull()) {
            qWarning() << "[Face3DViewer]" << "Invalid avatar data for" << personId;
            return;
        }
        remoteImages.insert(personId, image);
        refreshRemoteIcon(personId);
        if (profileList && profileList->currentItem()
            && profileList->currentItem()->data(Qt::UserRole).toString() == personId) {
            const PersonRecord person = remotePersonById(personId);
            const QString label = person.name.isEmpty() ? personId : person.name;
            applyImageForReconstruction(image, label);
        }
    });
}

void Face3DViewerPage::refreshRemoteIcon(const QString& personId)
{
    if (!profileList)
        return;
    for (int i = 0; i < profileList->count(); ++i) {
        auto* item = profileList->item(i);
        if (item->data(Qt::UserRole).toString() != personId)
            continue;
        const PersonRecord person = remotePersonById(personId);
        const QString display = person.name.isEmpty() ? tr("Unnamed") : person.name;
        if (remoteImages.contains(personId))
            item->setIcon(QIcon(buildPreviewPixmap(remoteImages.value(personId), kIconSize)));
        else
            item->setIcon(QIcon(buildInitialsPixmap(display, kIconSize)));
    }
}
void Face3DViewerPage::rebuildRemoteList()
{
    if (!profileList)
        return;
    for (const auto& person : remotePersons) {
        auto* item = new QListWidgetItem;
        item->setData(Qt::UserRole, person.id);
        const QString display = person.name.isEmpty() ? tr("Unnamed") : person.name;
        item->setText(tr("%1\nID: %2").arg(display, person.id));
        if (remoteImages.contains(person.id)) {
            item->setIcon(QIcon(buildPreviewPixmap(remoteImages.value(person.id), kIconSize)));
        } else {
            item->setIcon(QIcon(buildInitialsPixmap(display, kIconSize)));
        }
        profileList->addItem(item);
        ensureRemoteAvatar(person);
    }
}

void Face3DViewerPage::rebuildLocalList()
{
    if (!profileList)
        return;
    if (cachedProfiles.isEmpty())
        cachedProfiles = fetchProfiles();
    for (const auto& profile : cachedProfiles) {
        auto* item = new QListWidgetItem;
        item->setData(Qt::UserRole, profile.id);
        item->setText(tr("%1\nID: %2").arg(profile.name, profile.id));
        QImage image = safeLoadImage(profile.previewPath);
        if (!image.isNull())
            item->setIcon(QIcon(buildPreviewPixmap(image, kIconSize)));
        else
            item->setIcon(QIcon(buildInitialsPixmap(profile.name, kIconSize)));
        profileList->addItem(item);
    }
}


