#include "snappreviewwindow.h"
#include <QGuiApplication>

SnapPreviewWindow::SnapPreviewWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    hide();
}

void SnapPreviewWindow::showPreview(SnapType type, QScreen *screen)
{
    if (!screen) screen = QGuiApplication::primaryScreen();

    QRect screenRect = screen->geometry();
    QRect target;

    switch (type) {
        case SnapType::Top:
            target = screenRect;
        break;
        case SnapType::Left:
            target = QRect(screenRect.x(), screenRect.y(),
                           screenRect.width()/2, screenRect.height());
        break;
        case SnapType::Right:
            target = QRect(screenRect.x() + screenRect.width()/2, screenRect.y(),
                           screenRect.width()/2, screenRect.height());
        break;
        default:
            hidePreview();
        return;
    }

    currentSnap = type;
    lastRect = target;
    setGeometry(target);
    show();
    raise();
    update();
}

void SnapPreviewWindow::hidePreview()
{
    hide();
    currentSnap = SnapType::None;
}

void SnapPreviewWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(fillColor);
    QPen pen(borderColor, 3);
    p.setPen(pen);
    p.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 10, 10);
}
