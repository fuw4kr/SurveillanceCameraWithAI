#ifndef CAMERAVIEWWIDGET_H
#define CAMERAVIEWWIDGET_H

/**
 * @file cameraViewWidget.h
 * @brief Lightweight camera tile used inside the CamerasPage grid.
 *
 * Shows a live preview, status indicator, and controls to toggle camera and audio.
 *
 * @example
 * auto* tile = new CameraViewWidget(id, name, this);
 * connect(tile, &CameraViewWidget::toggleRequested, ...);
 */

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

/**
 * @brief Compact camera tile with preview and enable/audio controls.
 */
class CameraViewWidget : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a camera tile bound to an id and name.
     * @param id Camera identifier.
     * @param name Display name.
     * @param parent Optional parent widget.
     */
    explicit CameraViewWidget(int id, const QString& name, QWidget* parent = nullptr);

    /**
     * @brief Updates the preview image and heartbeat timer.
     * @param img Latest frame image.
     */
    void updateFrame(const QImage& img);  // ?? ��������� �����
    /**
     * @brief Marks the camera offline/online and updates status visuals.
     * @param off True to show offline state.
     */
    void setOffline(bool off);            // ?? �ਬ�ᮢ� �䫠��

signals:
    /**
     * @brief Emitted when the enable/disable button is clicked.
     * @param id Camera identifier.
     * @param enable True to enable.
     */
    void toggleRequested(int id, bool enable);
    /**
     * @brief Emitted when the audio checkbox is toggled.
     * @param id Camera identifier.
     * @param enable True when checked.
     */
    void audioToggled(int id, bool enable);

private slots:
    void onToggleClicked();
    void onAudioToggled(bool checked);
    void checkActivity(); // ?? ⠩��ୠ ��ॢ?ઠ ������

private:
    int id;
    bool active;
    QLabel* title;
    QLabel* preview;
    QLabel* statusIndicator;  // ?? ?������� �⠭�
    QPushButton* btnToggle;
    QCheckBox* chkAudio;

    QTimer activityTimer;     // ? ��ॢ?ઠ ��⨢����?
    qint64 lastFrameTime = 0; // �� ��⠭�쮣� ����� (��)
};

#endif // CAMERAVIEWWIDGET_H
