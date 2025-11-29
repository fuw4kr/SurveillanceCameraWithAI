
#ifndef FACE3DVIEWERPAGE_H
#define FACE3DVIEWERPAGE_H

#include "../../core/AIProcessor.h"
#include "../../core/ServerTypes.h"
#include "../../core/faceReconstructionModel.h"

#include <opencv2/core.hpp>

#include <QHash>
#include <QImage>
#include <QListWidget>
#include <QPixmap>
#include <QSet>
#include <QSize>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class FaceMeshView;
class ServerSyncManager;
class QNetworkAccessManager;
class QNetworkAccessManager;
class ServerSyncManager;

class Face3DViewerPage : public QWidget
{
    Q_OBJECT

public:
    explicit Face3DViewerPage(AIProcessor* processor, ServerSyncManager* sync, QWidget* parent = nullptr);

private slots:
    void handleLoadImage();
    void handleRefreshProfiles();
    void handleProfileSelection();
    void handleDatabaseChanged();
    void setRemotePersons(const QList<PersonRecord>& persons);

private:
    void buildUi();
    void attemptModelLoad();
    QString locateModel() const;
    QVector<AIProcessor::FaceProfile> fetchProfiles() const;
    void rebuildProfileList();
    void rebuildRemoteList();
    void rebuildLocalList();
    void runReconstruction(const QImage& image, const QString& label);
    void setStatus(const QString& text, bool isError = false);
    void applyImageForReconstruction(const QImage& image, const QString& label);
    static QPixmap buildPreviewPixmap(const QImage& image, const QSize& target);
    static QPixmap buildInitialsPixmap(const QString& label, const QSize& target);
    static cv::Mat toBgrMat(const QImage& image);
    AIProcessor::FaceProfile profileById(const QString& id) const;
    PersonRecord remotePersonById(const QString& id) const;
    QString resolveImageUrl(const QString& imageUrl) const;
    void ensureRemoteAvatar(const PersonRecord& person);
    void refreshRemoteIcon(const QString& personId);

    AIProcessor* aiProcessor = nullptr;
    ServerSyncManager* serverSync = nullptr;
    FaceReconstructionModel reconModel;
    QVector<AIProcessor::FaceProfile> cachedProfiles;
    QList<PersonRecord> remotePersons;

    QListWidget* profileList = nullptr;
    QLabel* previewLabel = nullptr;
    QLabel* statusLabel = nullptr;
    QPushButton* loadImageButton = nullptr;
    QPushButton* refreshButton = nullptr;
    FaceMeshView* meshView = nullptr;
    QNetworkAccessManager* imageLoader = nullptr;

    QHash<QString, QImage> remoteImages;
    QSet<QString> pendingDownloads;

    QImage currentImage;
    QString currentLabel;
    bool modelReady = false;
};

#endif // FACE3DVIEWERPAGE_H
