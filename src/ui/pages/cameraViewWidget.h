#ifndef CAMERAVIEWWIDGET_H
#define CAMERAVIEWWIDGET_H

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

class CameraViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CameraViewWidget(int id, const QString& name, QWidget* parent = nullptr);

    void updateFrame(const QImage& img);  // 🔹 оновлення кадру
    void setOffline(bool off);            // 🔹 примусово офлайн

signals:
    void toggleRequested(int id, bool enable);
    void audioToggled(int id, bool enable);

private slots:
    void onToggleClicked();
    void onAudioToggled(bool checked);
    void checkActivity(); // 🔹 таймерна перевірка статусу

private:
    int id;
    bool active;
    QLabel* title;
    QLabel* preview;
    QLabel* statusIndicator;  // 🔵 індикатор стану
    QPushButton* btnToggle;
    QCheckBox* chkAudio;

    QTimer activityTimer;     // ⏱ перевірка активності
    qint64 lastFrameTime = 0; // час останнього кадру (мс)
};

#endif // CAMERAVIEWWIDGET_H
