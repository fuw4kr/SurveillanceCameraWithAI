#include "AudioStreamPlayer.h"
#include <QDebug>
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QThread>

extern "C" {
#include <libavutil/channel_layout.h>
}

AudioStreamPlayer::AudioStreamPlayer(const QString& url, QObject* parent)
    : QThread(parent), streamUrl(url)
{
    av_log_set_level(AV_LOG_QUIET); // вимикаємо FFmpeg spam
}

AudioStreamPlayer::~AudioStreamPlayer()
{
    stop();
}

void AudioStreamPlayer::stop()
{
    running = false;
    wait(500);
    if (audioSink) {
        audioSink->stop();
        delete audioSink;
        audioSink = nullptr;
    }
}

void AudioStreamPlayer::run()
{
    AVFormatContext* fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, streamUrl.toStdString().c_str(), nullptr, nullptr) < 0) {
        qWarning() << "❌ Failed to open audio stream:" << streamUrl;
        return;
    }

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        qWarning() << "❌ Failed to find stream info";
        avformat_close_input(&fmtCtx);
        return;
    }

    // === Знаходимо перший аудіо-потік ===
    int audioStreamIndex = -1;
    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
            break;
        }
    }

    if (audioStreamIndex == -1) {
        qWarning() << "❌ No audio stream found";
        avformat_close_input(&fmtCtx);
        return;
    }

    AVCodecParameters* codecPar = fmtCtx->streams[audioStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        qWarning() << "❌ Unsupported audio codec";
        avformat_close_input(&fmtCtx);
        return;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codecCtx, codecPar) < 0) {
        qWarning() << "❌ Failed to copy codec parameters";
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return;
    }

    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        qWarning() << "❌ Failed to open codec";
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return;
    }

    // === Ініціалізація ресемплера ===
    SwrContext* swrCtx = swr_alloc();
    if (!swrCtx) {
        qWarning() << "❌ Failed to alloc SwrContext";
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return;
    }

    // 🧠 Новий API FFmpeg 6+: channel_layout → ch_layout
    av_opt_set_chlayout(swrCtx, "in_chlayout", &codecCtx->ch_layout, 0);
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, 2); // стерео
    av_opt_set_chlayout(swrCtx, "out_chlayout", &outLayout, 0);

    av_opt_set_int(swrCtx, "in_sample_rate", codecCtx->sample_rate, 0);
    av_opt_set_int(swrCtx, "out_sample_rate", codecCtx->sample_rate, 0);
    av_opt_set_sample_fmt(swrCtx, "in_sample_fmt", codecCtx->sample_fmt, 0);
    av_opt_set_sample_fmt(swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if (swr_init(swrCtx) < 0) {
        qWarning() << "❌ Failed to init SwrContext";
        swr_free(&swrCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return;
    }

    // === Qt-аудіо вихід ===
    QAudioFormat fmt;
    fmt.setSampleRate(codecCtx->sample_rate);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);

    audioSink = new QAudioSink(fmt);
    audioDevice = audioSink->start();

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    running = true;
    qDebug() << "🎧 Audio thread started for" << streamUrl;

    while (running && av_read_frame(fmtCtx, packet) >= 0) {
        if (packet->stream_index != audioStreamIndex) {
            av_packet_unref(packet);
            continue;
        }

        if (avcodec_send_packet(codecCtx, packet) < 0) {
            av_packet_unref(packet);
            continue;
        }

        while (avcodec_receive_frame(codecCtx, frame) >= 0) {
            const int dstNbSamples = av_rescale_rnd(
                swr_get_delay(swrCtx, codecCtx->sample_rate) + frame->nb_samples,
                codecCtx->sample_rate, codecCtx->sample_rate, AV_ROUND_UP);

            int outBufferSize = av_samples_get_buffer_size(
                nullptr, 2, dstNbSamples, AV_SAMPLE_FMT_S16, 1);
            uint8_t* outBuffer = (uint8_t*)av_malloc(outBufferSize);

            swr_convert(swrCtx, &outBuffer, dstNbSamples,
                (const uint8_t**)frame->extended_data, frame->nb_samples);

            // 🔊 Відтворення через Qt
            if (audioDevice)
                audioDevice->write((const char*)outBuffer, outBufferSize);

            av_free(outBuffer);
        }

        av_packet_unref(packet);
    }

    qDebug() << "🛑 Audio thread stopped:" << streamUrl;

    // === Очищення ===
    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swrCtx);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);
}
