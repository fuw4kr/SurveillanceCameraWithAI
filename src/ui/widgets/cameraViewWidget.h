#ifndef CAMERAVIEWWIDGET_H
#define CAMERAVIEWWIDGET_H

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QImage>

class CameraViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CameraViewWidget(int id, QWidget* parent = nullptr);

    void setTitle(const QString& text);
    void updateFrame(const QImage& image);
    void setStatusText(const QString& text);
    void setOnline(bool online);
    void setAudioChecked(bool enabled);
    void setCameraActive(bool enabled);
    void setAudioVisible(bool visible);

signals:
    void toggleRequested(int id, bool enable);
    void audioToggled(int id, bool enable);
    void removeRequested(int id);

private slots:
    void handleToggle();
    void handleAudio(bool checked);
    void refreshStatus();
    void handleRemove();

private:
    void resizeEvent(QResizeEvent* event) override;
    void setupUi();
    void updatePreviewPixmap();

    int cameraId;
    bool cameraEnabled = true;
    bool online = false;

    QLabel* titleLabel = nullptr;
    QLabel* previewLabel = nullptr;
    QLabel* statusLabel = nullptr;
    QPushButton* toggleButton = nullptr;
    QCheckBox* audioCheck = nullptr;
    QPushButton* removeButton = nullptr;

    QTimer heartbeatTimer;
    qint64 lastFrameMs = 0;
    QSize previewSize{ 320, 180 };
    QImage lastFrame;
};

#endif // CAMERAVIEWWIDGET_H
