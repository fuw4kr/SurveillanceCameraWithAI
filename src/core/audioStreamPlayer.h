#ifndef AUDIOSTREAMPLAYER_H
#define AUDIOSTREAMPLAYER_H

/**
 * @file audioStreamPlayer.h
 * @brief FFmpeg-powered audio streaming thread that plays remote sources.
 *
 * Spawns a background thread to pull audio from an URL (e.g., RTSP) using FFmpeg,
 * resamples as needed, and outputs via Qt Multimedia's `QAudioSink`. Designed to
 * pair with RTSP video playback so audio can be toggled independently.
 *
 * @example
 * auto* player = new AudioStreamPlayer("rtsp://example/stream");
 * player->start();
 * // ...
 * player->stop();
 */

#include <QObject>
#include <QThread>
#include <QAudioSink>
#include <QIODevice>
#include <atomic>
#include <string>

// FFmpeg
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

/**
 * @brief Background audio player that decodes streams with FFmpeg and outputs via QAudioSink.
 *
 * Inherits QThread to encapsulate decoding and playback while exposing a simple stop
 * method to halt streaming.
 */
class AudioStreamPlayer : public QThread
{
    Q_OBJECT
public:
    /**
     * @brief Creates an audio player bound to a streaming URL.
     * @param url Network or file URL to decode (e.g., RTSP).
     * @param parent Optional QObject parent.
     * @throws std::bad_alloc If internal allocations fail.
     * @example AudioStreamPlayer player("rtsp://cam/audio");
     */
    explicit AudioStreamPlayer(const QString& url, QObject* parent = nullptr);
    /**
     * @brief Stops playback and joins the decoding thread on destruction.
     * @return void
     * @throws None
     * @example delete player;
     */
    ~AudioStreamPlayer();

    /**
     * @brief Requests the decoding loop to terminate and waits for thread completion.
     * @return void
     * @throws None
     * @example player->stop();
     */
    void stop();

protected:
    /**
     * @brief Main decoding loop that reads, resamples, and feeds audio frames to QAudioSink.
     * @return void
     * @throws None (errors are handled internally and stop the loop)
     * @example start(); // run() executes on the thread start
     */
    void run() override;

private:
    QString streamUrl;
    std::atomic<bool> running{ false };

    QAudioSink* audioSink = nullptr;
    QIODevice* audioDevice = nullptr;
};

#endif // AUDIOSTREAMPLAYER_H
