
#ifndef FACE3DVIEWERPAGE_H
#define FACE3DVIEWERPAGE_H

#include "../../core/AIProcessor.h"
#include "../../core/faceReconstructionModel.h"

#include <opencv2/core.hpp>

#include <QImage>
#include <QListWidget>
#include <QPixmap>
#include <QSize>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class FaceMeshView;

class Face3DViewerPage : public QWidget
{
    Q_OBJECT

public:
    explicit Face3DViewerPage(AIProcessor* processor, QWidget* parent = nullptr);

private slots:
    void handleLoadImage();
    void handleRefreshProfiles();
    void handleProfileSelection();
    void handleDatabaseChanged();

private:
    void buildUi();
    void attemptModelLoad();
    QString locateModel() const;
    QVector<AIProcessor::FaceProfile> fetchProfiles() const;
    void rebuildProfileList();
    void runReconstruction(const QImage& image, const QString& label);
    void setStatus(const QString& text, bool isError = false);
    static QPixmap buildPreviewPixmap(const QImage& image, const QSize& target);
    static cv::Mat toBgrMat(const QImage& image);
    AIProcessor::FaceProfile profileById(const QString& id) const;

    AIProcessor* aiProcessor = nullptr;
    FaceReconstructionModel reconModel;
    QVector<AIProcessor::FaceProfile> cachedProfiles;

    QListWidget* profileList = nullptr;
    QLabel* previewLabel = nullptr;
    QLabel* statusLabel = nullptr;
    QPushButton* loadImageButton = nullptr;
    QPushButton* refreshButton = nullptr;
    FaceMeshView* meshView = nullptr;

    QImage currentImage;
    QString currentLabel;
    bool modelReady = false;
};

#endif // FACE3DVIEWERPAGE_H
