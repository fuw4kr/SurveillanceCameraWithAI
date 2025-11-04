#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

#include "windowEdit/framelesswindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public FramelessWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    private slots:
        void processFrame();

private:
    Ui::MainWindow *ui;
    QTimer *timer;
    cv::VideoCapture camera;
    cv::dnn::Net net;
    bool isPaused = true;
    std::chrono::steady_clock::time_point lastFaceTime, startTime;
    double recordingSeconds = 0.0;
    cv::VideoWriter video;
    void setupTitleBar();

    void updateMaximizeIcon(bool maxed);
    QString getCurrentTimeString() const;
    void drawFaces(cv::Mat &frame, const std::vector<cv::Rect> &faces);
};

#endif //MAINWINDOW_H
