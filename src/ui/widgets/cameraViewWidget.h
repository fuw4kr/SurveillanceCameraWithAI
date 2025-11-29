#ifndef CAMERAVIEWWIDGET_H
#define CAMERAVIEWWIDGET_H

/**
 * @file cameraViewWidget.h
 * @brief Declarative UI widget for displaying a camera stream tile.
 *
 * Renders a live preview with title, status text, audio toggle, and enable/disable
 * controls. Emits signals so the hosting page can turn cameras on/off, toggle
 * audio, or remove a stream. The widget monitors the freshness of the last frame
 * to auto-mark cameras as online or offline.
 *
 * @example
 * auto* view = new CameraViewWidget(1, parent);
 * view->setTitle("Entrance");
 * view->setAudioVisible(true);
 * connect(view, &CameraViewWidget::toggleRequested, this, &Controller::onToggleCamera);
 */

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QImage>

/**
 * @brief Live camera tile with preview, status, and audio controls.
 *
 * Maintains the latest frame, scales it to the available preview area, and emits
 * user intent through signals for enable/disable, audio toggling, and removal.
 */
class CameraViewWidget : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Creates a camera view bound to a logical camera identifier.
     * @param id Unique camera identifier used in emitted signals.
     * @param parent Optional Qt parent widget.
     * @throws std::bad_alloc If UI elements cannot be allocated.
     * @example CameraViewWidget* tile = new CameraViewWidget(0, this);
     */
    explicit CameraViewWidget(int id, QWidget* parent = nullptr);

    /**
     * @brief Sets the display title in the header.
     * @param text Title string (e.g., camera name/location).
     * @return void
     * @throws None
     * @example setTitle("Lobby Camera");
     */
    void setTitle(const QString& text);
    /**
     * @brief Updates the preview with a new frame and refreshes online status.
     * @param image Latest frame image to display.
     * @return void
     * @throws None
     * @example updateFrame(frameImage);
     */
    void updateFrame(const QImage& image);
    /**
     * @brief Overrides the footer status text.
     * @param text Human-readable status.
     * @return void
     * @throws None
     * @example setStatusText(tr("Reconnecting..."));
     */
    void setStatusText(const QString& text);
    /**
     * @brief Marks the camera online/offline and clears preview when offline.
     * @param online True when frames are current; false clears preview.
     * @return void
     * @throws None
     * @example setOnline(true);
     */
    void setOnline(bool online);
    /**
     * @brief Sets the audio checkbox state programmatically.
     * @param enabled True if audio is enabled.
     * @return void
     * @throws None
     * @example setAudioChecked(false);
     */
    void setAudioChecked(bool enabled);
    /**
     * @brief Enables or disables the camera tile, updating UI affordances.
     * @param enabled True to allow preview/audio; false grays out the tile.
     * @return void
     * @throws None
     * @example setCameraActive(true);
     */
    void setCameraActive(bool enabled);
    /**
     * @brief Shows or hides the audio checkbox and resets state when hidden.
     * @param visible True to show audio control.
     * @return void
     * @throws None
     * @example setAudioVisible(true);
     */
    void setAudioVisible(bool visible);

signals:
    /**
     * @brief Emitted when the user toggles camera enablement.
     * @param id Camera identifier.
     * @param enable True to enable, false to disable.
     */
    void toggleRequested(int id, bool enable);
    /**
     * @brief Emitted when the audio checkbox changes state.
     * @param id Camera identifier.
     * @param enable True when audio is enabled.
     */
    void audioToggled(int id, bool enable);
    /**
     * @brief Emitted when the remove button is pressed.
     * @param id Camera identifier to remove.
     */
    void removeRequested(int id);

private slots:
    /**
     * @brief Handles enable/disable button clicks and emits toggleRequested.
     * @return void
     * @throws None
     * @example handleToggle();
     */
    void handleToggle();
    /**
     * @brief Emits audioToggled when the audio checkbox is toggled.
     * @param checked Checkbox state.
     * @return void
     * @throws None
     * @example handleAudio(true);
     */
    void handleAudio(bool checked);
    /**
     * @brief Refreshes online status based on last frame timestamp.
     * @return void
     * @throws None
     * @example refreshStatus();
     */
    void refreshStatus();
    /**
     * @brief Emits removeRequested when the remove button is clicked.
     * @return void
     * @throws None
     * @example handleRemove();
     */
    void handleRemove();

private:
    /**
     * @brief Rescales the preview on resize to maintain aspect ratio.
     * @param event Resize event passed from Qt.
     * @return void
     * @throws None
     * @example resizeEvent(event);
     */
    void resizeEvent(QResizeEvent* event) override;
    /**
     * @brief Builds the widget layout, styles, and connections.
     * @return void
     * @throws std::bad_alloc If layout/widget creation fails.
     * @example setupUi();
     */
    void setupUi();
    /**
     * @brief Scales the last received frame into the preview label.
     * @return void
     * @throws None
     * @example updatePreviewPixmap();
     */
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
