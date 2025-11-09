#include "framelesswindow.h"
#include <QApplication>
#include <QScreen>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QListWidget>
#include <QToolBox>
#include <QStyle>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyleOption>
#include <QDialog>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QPushButton>
#include <QLabel>

static const int RESIZE_MARGIN = 6;
static const int SNAP_EDGE = 10;

static int RESIZE_MARGIN_FOR(const QWidget *w) {
    const qreal dpr = w ? w->devicePixelRatioF() : 1.0;
    return qMax(4, int(RESIZE_MARGIN * dpr));
}

FramelessWindow::FramelessWindow(QWidget *parent)
    : QMainWindow(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setMouseTracking(true);
    unsetCursor();
    qApp->installEventFilter(this);
    snapPreview = new SnapPreviewWindow();
}

FramelessWindow::~FramelessWindow() {
    delete snapPreview;
}

void FramelessWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    if (m_uiInitialized) {
        return;
    }

    if (auto cw = centralWidget()) {
        cw->setMouseTracking(true);
        cw->unsetCursor();
    }
    QWidget *titleBar = findChild<QWidget*>("titleBar");
    if (titleBar) {
        titleBar->setMouseTracking(true);
        titleBar->unsetCursor();
    }

    for (auto *listWidget : this->findChildren<QListWidget*>()) {
        listWidget->setMouseTracking(true);
        listWidget->unsetCursor();
    }

    QToolBox *toolBox = this->findChild<QToolBox*>("toolBox");
    if(toolBox) {
        toolBox->setMouseTracking(true);
        toolBox->unsetCursor();
    }

    m_uiInitialized = true;
}

bool FramelessWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event && event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me && this->isVisible()) {
            updateCursorShape(mapFromGlobal(me->globalPos()));
            updateCursorShape(mapFromGlobal(me->globalPos()));
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void FramelessWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_F4)
        toggleFullscreenMode();
    else
        QMainWindow::keyPressEvent(event);
}

void FramelessWindow::toggleFullscreenMode() {
    static QRect savedGeometry;
    static bool titleBarWasVisible = true;

    QWidget *titleBar = findChild<QWidget*>("titleBar");

    QPropertyAnimation *fade = new QPropertyAnimation(this, "windowOpacity");
    fade->setDuration(200);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    fade->setEasingCurve(QEasingCurve::InOutQuad);

    connect(fade, &QPropertyAnimation::finished, this, [=]() mutable {
        if (!isFullscreenMode) {
            savedGeometry = geometry();

            if (titleBar && titleBar->isVisible()) {
                titleBar->hide();
                titleBarWasVisible = true;
            } else {
                titleBarWasVisible = false;
            }

            showFullScreen();
            isFullscreenMode = true;
        } else {
            showNormal();
            setGeometry(savedGeometry);

            if (titleBarWasVisible && titleBar)
                titleBar->show();

            isFullscreenMode = false;
        }

        QPropertyAnimation *fadeIn = new QPropertyAnimation(this, "windowOpacity");
        fadeIn->setDuration(200);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->setEasingCurve(QEasingCurve::InOutQuad);
        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    });

    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void FramelessWindow::setResizeCursor(FramelessWindow::ResizeRegion region)
{
    Qt::CursorShape newShape = Qt::ArrowCursor;
    switch (region) {
        case TopLeft:
        case BottomRight:
            newShape = Qt::SizeFDiagCursor;
        break;
        case TopRight:
        case BottomLeft:
            newShape = Qt::SizeBDiagCursor;
        break;
        case Left:
        case Right:
            newShape = Qt::SizeHorCursor;
        break;
        case Top:
        case Bottom:
            newShape = Qt::SizeVerCursor;
        break;
        default:
            newShape = Qt::ArrowCursor;
        break;
    }
    setCursor(newShape);
}

FramelessWindow::ResizeRegion FramelessWindow::detectResizeRegion(const QPoint &pos) {
    const int x = pos.x(), y = pos.y(), w = width(), h = height();
    const bool left = x <= RESIZE_MARGIN;
    const bool right = x >= w - RESIZE_MARGIN;
    const bool top = y <= RESIZE_MARGIN;
    const bool bottom = y >= h - RESIZE_MARGIN;

    if (top && left) return TopLeft;
    if (top && right) return TopRight;
    if (bottom && left) return BottomLeft;
    if (bottom && right) return BottomRight;
    if (top) return Top;
    if (bottom) return Bottom;
    if (left) return Left;
    if (right) return Right;
    return None;
}

void FramelessWindow::updateCursorShape(const QPoint &pos)
{
    if (isMaximized) {
        if (cursor().shape() != Qt::ArrowCursor) {
            unsetCursor();
        }
        return;
    }
    if (isDragging || isResizing)
        return;

    ResizeRegion region = detectResizeRegion(pos);
    Qt::CursorShape newShape = Qt::ArrowCursor;

    switch (region) {
        case TopLeft:
        case BottomRight:
            newShape = Qt::SizeFDiagCursor;
        break;
        case TopRight:
        case BottomLeft:
            newShape = Qt::SizeBDiagCursor;
        break;
        case Left:
        case Right:
            newShape = Qt::SizeHorCursor;
        break;
        case Top:
        case Bottom:
            newShape = Qt::SizeVerCursor;
        break;
        default:
            newShape = Qt::ArrowCursor;
        break;
    }

    if (cursor().shape() != newShape) {
        setCursor(newShape);
    }
}

void FramelessWindow::mousePressEvent(QMouseEvent *event)
{
    if (isFullscreenMode)
        return QMainWindow::mousePressEvent(event);
    if (event->button() == Qt::LeftButton) {
        currentResizeRegion = isMaximized ? None : detectResizeRegion(event->pos());
        if (currentResizeRegion != None) {
            isResizing = true;
            grabMouse();
            event->accept();
            return;
        }

        QWidget *titleBar = findChild<QWidget*>("titleBar");
        if (titleBar && titleBar->rect().contains(titleBar->mapFromParent(event->pos()))) {
            isDragging = true;
            dragOffset = event->globalPos() - frameGeometry().topLeft();
            grabMouse();
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void FramelessWindow::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint globalPos = event->globalPos();

    if (isFullscreenMode)
        return QMainWindow::mouseMoveEvent(event);

    if (isResizing && !isMaximized) {
        setResizeCursor(currentResizeRegion);
        QRect geom = geometry();

        switch (currentResizeRegion) {
            case Left:        geom.setLeft(globalPos.x()); break;
            case Right:       geom.setRight(globalPos.x()); break;
            case Top:         geom.setTop(globalPos.y()); break;
            case Bottom:      geom.setBottom(globalPos.y()); break;
            case TopLeft:     geom.setTop(globalPos.y()); geom.setLeft(globalPos.x()); break;
            case TopRight:    geom.setTop(globalPos.y()); geom.setRight(globalPos.x()); break;
            case BottomLeft:  geom.setBottom(globalPos.y()); geom.setLeft(globalPos.x()); break;
            case BottomRight: geom.setBottom(globalPos.y()); geom.setRight(globalPos.x()); break;
            default: break;
        }

        geom = geom.normalized();
        if (geom.width()  < minimumWidth())  geom.setWidth(minimumWidth());
        if (geom.height() < minimumHeight()) geom.setHeight(minimumHeight());
        setGeometry(geom);
        event->accept();
        return;
    }

    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        QScreen *screen = QGuiApplication::screenAt(globalPos);
        if (!screen)
            screen = QGuiApplication::primaryScreen();

        QRect avail = screen->availableGeometry();

        if (isMaximized) {
            const double ratio = double(event->pos().x()) / width();
            const int newW = int(avail.width()  * 0.8);
            const int newH = int(avail.height() * 0.75);
            const int newX = globalPos.x() - int(newW * ratio);
            const int newY = avail.y() + RESIZE_MARGIN * 3;

            savedGeometryBeforeMaximize = QRect(newX, newY, newW, newH);
            setGeometry(savedGeometryBeforeMaximize);
            isMaximized = false;
            emit windowMaximizedChanged(false);
            dragOffset = QPoint(int(newW * ratio), RESIZE_MARGIN * 3);
        }

        QPoint newTopLeft = globalPos - dragOffset;

        const int topLimit = avail.y();
        const int bottomLimit = avail.bottom();

        if (newTopLeft.y() < topLimit)
            newTopLeft.setY(topLimit);

        if (globalPos.y() > bottomLimit) {
            int overflow = globalPos.y() - bottomLimit;
            newTopLeft.setY(newTopLeft.y() - overflow);
        }

        move(newTopLeft);

        const int edge = SNAP_EDGE;
        SnapPreviewWindow::SnapType snap = SnapPreviewWindow::SnapType::None;
        if (globalPos.y() <= avail.y() + edge)
            snap = SnapPreviewWindow::SnapType::Top;
        else if (globalPos.x() <= avail.x() + edge)
            snap = SnapPreviewWindow::SnapType::Left;
        else if (globalPos.x() >= avail.x() + avail.width() - edge)
            snap = SnapPreviewWindow::SnapType::Right;

        snapPreview->showPreview(snap, screen);
        event->accept();
        return;
    }

    QMainWindow::mouseMoveEvent(event);
    updateCursorShape(event->pos());
}

void FramelessWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        releaseMouse();

        if (snapPreview && snapPreview->currentType() != SnapPreviewWindow::SnapType::None) {
            QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
            if (!screen) screen = QGuiApplication::primaryScreen();
            QRect avail = screen->availableGeometry();

            switch (snapPreview->currentType()) {
                case SnapPreviewWindow::SnapType::Top:
                    setGeometry(avail);
                isMaximized = true;
                break;
                case SnapPreviewWindow::SnapType::Left:
                    setGeometry(avail.x(), avail.y(), avail.width()/2, avail.height());
                isMaximized = false;
                break;
                case SnapPreviewWindow::SnapType::Right:
                    setGeometry(avail.x() + avail.width()/2, avail.y(),
                                avail.width()/2, avail.height());
                isMaximized = false;
                break;
                default: break;
            }
            emit windowMaximizedChanged(isMaximized);
        }

        isDragging = false;
        isResizing = false;
        currentResizeRegion = None;
        if (snapPreview) snapPreview->hidePreview();
        unsetCursor();
        updateCursorShape(mapFromGlobal(QCursor::pos()));
    }
    QMainWindow::mouseReleaseEvent(event);
}

void FramelessWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    QWidget *titleBar = findChild<QWidget*>("titleBar");
    if (isFullscreenMode)
        return QMainWindow::mouseDoubleClickEvent(event);
    if (titleBar && titleBar->rect().contains(titleBar->mapFromParent(event->pos()))) {
        toggleMaximizeRestore();
        event->accept();
        return;
    }
    QMainWindow::mouseDoubleClickEvent(event);
}

void FramelessWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    QWidget *titleBar = findChild<QWidget*>("titleBar");
    if (titleBar)
        titleBar->setGeometry(0, 0, width(), 36);
}

void FramelessWindow::enterEvent(QEnterEvent *event)
{
    QMainWindow::enterEvent(event);
    setCursor(Qt::ArrowCursor);
}

void FramelessWindow::leaveEvent(QEvent *event)
{
    QMainWindow::leaveEvent(event);
    setCursor(Qt::ArrowCursor);
}

void FramelessWindow::toggleMaximizeRestore()
{
    if (isMaximized) {
        setGeometry(savedGeometryBeforeMaximize);
        isMaximized = false;
    } else {
        savedGeometryBeforeMaximize = geometry();
        QScreen *screen = QGuiApplication::screenAt(frameGeometry().center());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        setGeometry(screen->availableGeometry());
        isMaximized = true;
    }
    emit windowMaximizedChanged(isMaximized);
}

//=======================================================
//                 FramelessDialog
//=======================================================

FramelessDialog::FramelessDialog(QWidget *parent, bool enableShadow)
    : QDialog(parent), shadowEnabled(enableShadow)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setMouseTracking(true);
    qApp->installEventFilter(this);
    if (shadowEnabled)
        applyShadowEffect();
}

void FramelessDialog::applyShadowEffect()
{
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 150));
    setGraphicsEffect(shadow);
}

void FramelessDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    if (m_uiInitialized)
        return;

    if (auto cw = this->findChild<QWidget*>("qt_scrollarea_viewport"))
        cw->setMouseTracking(true);

    if (auto cw = this->findChild<QWidget*>("centralwidget"))
        cw->setMouseTracking(true);

    QWidget *titleBar = findChild<QWidget*>("titleBar");
    if (titleBar) {
        titleBar->setMouseTracking(true);
        titleBar->unsetCursor();
    }

    for (auto *listWidget : this->findChildren<QListWidget*>()) {
        listWidget->setMouseTracking(true);
        listWidget->unsetCursor();
    }

    for (auto *toolBox : this->findChildren<QToolBox*>()) {
        toolBox->setMouseTracking(true);
        toolBox->unsetCursor();
    }

    m_uiInitialized = true;
}

bool FramelessDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event && event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me && this->isVisible()) {
            updateCursorShape(mapFromGlobal(me->globalPos()));
        }
    }
    return QDialog::eventFilter(obj, event);
}

FramelessDialog::ResizeRegion FramelessDialog::detectResizeRegion(const QPoint &pos)
{
    if (!resizeEnabled) {
        unsetCursor();
        return None;
    }
    const int x = pos.x(), y = pos.y(), w = width(), h = height();
    const bool left   = x <= RESIZE_MARGIN;
    const bool right  = x >= w - RESIZE_MARGIN;
    const bool top    = y <= RESIZE_MARGIN;
    const bool bottom = y >= h - RESIZE_MARGIN;

    if (top && left)   return TopLeft;
    if (top && right)  return TopRight;
    if (bottom && left)  return BottomLeft;
    if (bottom && right) return BottomRight;
    if (top)    return Top;
    if (bottom) return Bottom;
    if (left)   return Left;
    if (right)  return Right;
    return None;
}

void FramelessDialog::updateCursorShape(const QPoint &pos)
{
    if (!resizeEnabled) {
        unsetCursor();
        return;
    }
    if (isDragging || isResizing)
        return;

    static ResizeRegion lastRegion = None;
    ResizeRegion region = detectResizeRegion(pos);

    if (region == lastRegion)
        return;
    lastRegion = region;

    Qt::CursorShape newShape = Qt::ArrowCursor;
    switch (region) {
        case TopLeft:
        case BottomRight: newShape = Qt::SizeFDiagCursor; break;
        case TopRight:
        case BottomLeft:  newShape = Qt::SizeBDiagCursor; break;
        case Left:
        case Right:       newShape = Qt::SizeHorCursor;   break;
        case Top:
        case Bottom:      newShape = Qt::SizeVerCursor;   break;
        default:          newShape = Qt::ArrowCursor;     break;
    }
    if (cursor().shape() != newShape)
        setCursor(newShape);
}

void FramelessDialog::mousePressEvent(QMouseEvent *event)
{
    if (!resizeEnabled) {
        currentResizeRegion = None;
    }

    if (event->button() != Qt::LeftButton)
        return QDialog::mousePressEvent(event);

    currentResizeRegion = detectResizeRegion(event->pos());
    if (currentResizeRegion != None) {
        isResizing = true;
        grabMouse();
        event->accept();
        return;
    }

    QWidget *titleBar = findChild<QWidget*>("titleBar");
    if (titleBar && titleBar->rect().contains(titleBar->mapFrom(this, event->pos()))) {
        isDragging = true;
        dragOffset = event->globalPos() - frameGeometry().topLeft();
        grabMouse();
        event->accept();
        return;
    }

    QDialog::mousePressEvent(event);
}

void FramelessDialog::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint globalPos = event->globalPos();

    if (isResizing) {
        QRect geom = geometry();
        switch (currentResizeRegion) {
            case Left:        geom.setLeft(globalPos.x()); break;
            case Right:       geom.setRight(globalPos.x()); break;
            case Top:         geom.setTop(globalPos.y()); break;
            case Bottom:      geom.setBottom(globalPos.y()); break;
            case TopLeft:     geom.setTop(globalPos.y()); geom.setLeft(globalPos.x()); break;
            case TopRight:    geom.setTop(globalPos.y()); geom.setRight(globalPos.x()); break;
            case BottomLeft:  geom.setBottom(globalPos.y()); geom.setLeft(globalPos.x()); break;
            case BottomRight: geom.setBottom(globalPos.y()); geom.setRight(globalPos.x()); break;
            default: break;
        }

        geom = geom.normalized();

        if (geom.width()  < minimumWidth())  geom.setWidth(minimumWidth());
        if (geom.height() < minimumHeight()) geom.setHeight(minimumHeight());

        setGeometry(geom);
        event->accept();
        return;
    }

    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        QScreen *screen = QGuiApplication::screenAt(globalPos);
        if (!screen)
            screen = QGuiApplication::primaryScreen();

        QRect avail = screen->availableGeometry();
        QPoint newTopLeft = globalPos - dragOffset;

        if (newTopLeft.y() < avail.y())
            newTopLeft.setY(avail.y());
        if (globalPos.y() > avail.bottom()) {
            int overflow = globalPos.y() - avail.bottom();
            newTopLeft.setY(newTopLeft.y() - overflow);
        }

        move(newTopLeft);
        event->accept();
        return;
    }

    updateCursorShape(event->pos());
    QDialog::mouseMoveEvent(event);
}

void FramelessDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        releaseMouse();
        isDragging = false;
        isResizing = false;
        currentResizeRegion = None;
    }
    QDialog::mouseReleaseEvent(event);
}

void FramelessDialog::resizeEvent(QResizeEvent *event)
{
    if (!resizeEnabled) {
        unsetCursor();
        return;
    }
    QDialog::resizeEvent(event);
    QWidget *titleBar = findChild<QWidget*>("titleBar");
    if (titleBar)
        titleBar->setGeometry(0, 0, width(), 36);
}

void FramelessDialog::enterEvent(QEnterEvent *event)
{
    QDialog::enterEvent(event);
    setCursor(Qt::ArrowCursor);
}

void FramelessDialog::leaveEvent(QEvent *event)
{
    QDialog::leaveEvent(event);
    setCursor(Qt::ArrowCursor);
}

void FramelessDialog::showEventFade(QShowEvent *event)
{
    QDialog::showEvent(event);
    setWindowOpacity(0.0);
    auto *fadeIn = new QPropertyAnimation(this, "windowOpacity");
    fadeIn->setDuration(200);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::InOutQuad);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
}

void FramelessDialog::closeEvent(QCloseEvent *event)
{
    auto *fadeOut = new QPropertyAnimation(this, "windowOpacity");
    fadeOut->setDuration(180);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InOutQuad);

    connect(fadeOut, &QPropertyAnimation::finished, this, [this, event]() {
        event->accept();
        QDialog::close();
    });

    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
    event->ignore();
}

//=======================================================
//                 FramelessMessageBox
//=======================================================
FramelessMessageBox::FramelessMessageBox(const QString &title,
                                         const QString &message,
                                         QMessageBox::Icon icon,
                                         QWidget *parent)
    : FramelessDialog(parent, false)
{
    setResizeEnabled(false);
    setMinimumSize(380, 200);
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    QWidget *background = new QWidget(this);
    background->setObjectName("background");

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(background);

    QVBoxLayout *mainLayout = new QVBoxLayout(background);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    titleLabel = new QLabel(title);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    titleLabel->setWordWrap(true);
    titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(titleLabel);

    // === іконка + текст ===
    QHBoxLayout *hLayout = new QHBoxLayout;
    iconLabel = new QLabel;
    messageLabel = new QLabel(message);
    messageLabel->setWordWrap(true);
    messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QPixmap iconPixmap;
    switch (icon) {
        case QMessageBox::Warning:
            iconPixmap = style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(48, 48);
            break;
        case QMessageBox::Critical:
            iconPixmap = style()->standardIcon(QStyle::SP_MessageBoxCritical).pixmap(48, 48);
            break;
        case QMessageBox::Question:
            iconPixmap = style()->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(48, 48);
            break;
        case QMessageBox::Information:
        default:
            iconPixmap = style()->standardIcon(QStyle::SP_MessageBoxInformation).pixmap(48, 48);
            break;
    }

    iconLabel->setPixmap(iconPixmap);
    iconLabel->setAlignment(Qt::AlignTop);

    hLayout->addWidget(iconLabel, 0, Qt::AlignTop);
    hLayout->addWidget(messageLabel, 1);
    mainLayout->addLayout(hLayout);

    // === кнопки ===
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    okButton = new QPushButton(tr("OK"));
    okButton->setCursor(Qt::PointingHandCursor);
    connect(okButton, &QPushButton::clicked, this, &FramelessMessageBox::onOkClicked);
    btnLayout->addWidget(okButton);

    if (icon == QMessageBox::Question || icon == QMessageBox::Critical) {
        cancelButton = new QPushButton(tr("Cancel"));
        cancelButton->setCursor(Qt::PointingHandCursor);
        connect(cancelButton, &QPushButton::clicked, this, &FramelessMessageBox::onCancelClicked);
        btnLayout->addWidget(cancelButton);
    }

    mainLayout->addLayout(btnLayout);
}

// ======================================================
//                     Slots
// ======================================================

void FramelessMessageBox::onOkClicked()
{
    resultCode = QMessageBox::Accepted;
    accept();
}

void FramelessMessageBox::onCancelClicked()
{
    resultCode = QMessageBox::Rejected;
    reject();
}

// ======================================================
//                 Exec з fade-анімацією
// ======================================================

int FramelessMessageBox::exec()
{
    setWindowOpacity(0.0);
    QPropertyAnimation *fadeIn = new QPropertyAnimation(this, "windowOpacity");
    fadeIn->setDuration(200);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::InOutQuad);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    QDialog::show();
    QDialog::raise();
    QDialog::activateWindow();

    int res = QDialog::exec();

    QPropertyAnimation *fadeOut = new QPropertyAnimation(this, "windowOpacity");
    fadeOut->setDuration(150);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InOutQuad);
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);

    return resultCode = (res == QDialog::Accepted
                         ? QMessageBox::Accepted
                         : QMessageBox::Rejected);
}

// ======================================================
//                Drag + highlight border (опційно)
// ======================================================

void FramelessMessageBox::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *child = childAt(event->pos());
        if (!qobject_cast<QPushButton*>(child)) {
            isDragging = true;
            dragOffset = event->globalPos() - frameGeometry().topLeft();
            event->accept();
            return;
        }
    }
    QDialog::mousePressEvent(event);
}

void FramelessMessageBox::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - dragOffset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void FramelessMessageBox::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        isDragging = false;
    QDialog::mouseReleaseEvent(event);
}

// ======================================================
//        Статичні Qt-стилі (зручні виклики)
// ======================================================

void FramelessMessageBox::information(QWidget *parent,
                                      const QString &title,
                                      const QString &message)
{
    FramelessMessageBox box(title, message, QMessageBox::Information, parent);
    box.exec();
}

void FramelessMessageBox::warning(QWidget *parent,
                                  const QString &title,
                                  const QString &message)
{
    FramelessMessageBox box(title, message, QMessageBox::Warning, parent);
    box.exec();
}

bool FramelessMessageBox::question(QWidget *parent,
                                   const QString &title,
                                   const QString &message)
{
    FramelessMessageBox box(title, message, QMessageBox::Question, parent);
    int res = box.exec();
    return res == QMessageBox::Accepted;
}

void FramelessMessageBox::critical(QWidget *parent,
                                   const QString &title,
                                   const QString &message)
{
    FramelessMessageBox box(title, message, QMessageBox::Critical, parent);
    box.exec();
}
