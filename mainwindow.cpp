#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QImage>
#include <QPixmap>
#include <QDateTime>
#include <QMessageBox>
#include <filesystem>
#include <iostream>


using namespace cv;
using namespace std;
using namespace std::chrono;
using namespace std::filesystem;

MainWindow::MainWindow(QWidget *parent)
    : FramelessWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Smart Camera (DNN)");

    setupTitleBar();
    snapPreview = new SnapPreviewWindow(this);
    connect(this, &FramelessWindow::windowMaximizedChanged, this, &MainWindow::updateMaximizeIcon);

    string modelFile = "C:/dev/opencv/models/face_detector/res10_300x300_ssd_iter_140000.caffemodel";
    string configFile = "C:/dev/opencv/models/face_detector/deploy.prototxt";

    net = dnn::readNetFromCaffe(configFile, modelFile);
    if (net.empty()) {
        QMessageBox::critical(this, "Error", "Could not load DNN model!");
        return;
    }

    camera.open(1);
    if (!camera.isOpened()) {
        QMessageBox::critical(this, "Error", "Cannot open camera!");
        return;
    }

    camera.set(cv::CAP_PROP_BUFFERSIZE, 1);

    if (!exists("records")) create_directory("records");

    string filename = "records/smart_record_dnn.avi";
    int frame_width = (int)camera.get(CAP_PROP_FRAME_WIDTH);
    int frame_height = (int)camera.get(CAP_PROP_FRAME_HEIGHT);
    video.open(filename, VideoWriter::fourcc('M','J','P','G'), 20, Size(frame_width, frame_height));

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::processFrame);
    timer->start(30);

    lastFaceTime = steady_clock::now();
    startTime = steady_clock::now();
}

MainWindow::~MainWindow() {
    if (snapPreview) {
        snapPreview->hidePreview();
        snapPreview->deleteLater();
    }
    camera.release();
    video.release();
    delete ui;
}

void MainWindow::setupTitleBar() {
    ui->titleBar->setMinimumHeight(36);
    ui->titleBar->setMaximumHeight(36);
    connect(ui->btnClose,    &QPushButton::clicked, this, &QWidget::close);
    connect(ui->btnMinimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(ui->btnMaximize, &QPushButton::clicked, this, &MainWindow::toggleMaximizeRestore);

    if (auto *v = qobject_cast<QVBoxLayout*>(ui->centralwidget->layout())) {
        v->setContentsMargins(0,0,0,0);
        v->setSpacing(0);
    }
}

void MainWindow::updateMaximizeIcon(bool maxed) {
    //bool isLight = (AppSettings::theme() == AppSettings::Theme::Light);
    bool isLight = true;
    QString path;
    if (maxed)
        path = isLight
            ? ":/resources/icons/icons-for-window/minimize-black.png"
            : ":/resources/icons/icons-for-window/minimize-white.png";
    else
        path = isLight
            ? ":/resources/icons/icons-for-window/maximize-black.png"
            : ":/resources/icons/icons-for-window/maximize-white.png";

    ui->btnMaximize->setIcon(QIcon(path));
}

QString MainWindow::getCurrentTimeString() const {
    return QTime::currentTime().toString("HH:mm:ss");
}

void MainWindow::processFrame() {
    Mat frame;
    camera >> frame;
    if (frame.empty()) return;

    flip(frame, frame, 1);

    Mat blob = dnn::blobFromImage(frame, 1.0, Size(300, 300),
                                  Scalar(104.0, 177.0, 123.0), false, false);
    net.setInput(blob);
    Mat detections = net.forward();

    Mat detectionMat(detections.size[2], detections.size[3], CV_32F, detections.ptr<float>());
    vector<Rect> faces;

    for (int i = 0; i < detectionMat.rows; i++) {
        float confidence = detectionMat.at<float>(i, 2);
        if (confidence > 0.5f) {
            int x1 = (int)(detectionMat.at<float>(i, 3) * frame.cols);
            int y1 = (int)(detectionMat.at<float>(i, 4) * frame.rows);
            int x2 = (int)(detectionMat.at<float>(i, 5) * frame.cols);
            int y2 = (int)(detectionMat.at<float>(i, 6) * frame.rows);
            faces.emplace_back(Point(x1, y1), Point(x2, y2));
        }
    }

    if (!faces.empty()) {
        if (isPaused) {
            isPaused = false;
            startTime = steady_clock::now() - duration_cast<steady_clock::duration>(duration<double>(recordingSeconds));
        }
        lastFaceTime = steady_clock::now();
    } else {
        if (!isPaused && duration_cast<seconds>(steady_clock::now() - lastFaceTime).count() > 2) {
            isPaused = true;
        }
    }

    if (!isPaused) {
        recordingSeconds = duration<double>(steady_clock::now() - startTime).count();
        video.write(frame);
    }

    drawFaces(frame, faces);

    QImage img((uchar*)frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
    ui->labelCamera->setPixmap(QPixmap::fromImage(img));
}

void MainWindow::drawFaces(Mat &frame, const vector<Rect> &faces) {
    for (const auto &face : faces)
        rectangle(frame, face, Scalar(0, 255, 0), 2);

    if (!isPaused) {
        putText(frame, "REC " + getCurrentTimeString().toStdString(),
                Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 0, 255), 2);
    } else {
        putText(frame, "PAUSED", Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 255, 255), 2);
    }
}
