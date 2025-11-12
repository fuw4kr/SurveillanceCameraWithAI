#ifndef AUDIOSTREAMPLAYER_H
#define AUDIOSTREAMPLAYER_H

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

class AudioStreamPlayer : public QThread
{
    Q_OBJECT
public:
    explicit AudioStreamPlayer(const QString& url, QObject* parent = nullptr);
    ~AudioStreamPlayer();

    void stop();

protected:
    void run() override;

private:
    QString streamUrl;
    std::atomic<bool> running{ false };

    QAudioSink* audioSink = nullptr;
    QIODevice* audioDevice = nullptr;
};

#endif // AUDIOSTREAMPLAYER_H
