#ifndef FRAMELESSWINDOW_H
#define FRAMELESSWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QGraphicsDropShadowEffect>
#include "snapPreviewWindow.h"

/*
 .h
#include "windowEdit/framelesswindow.h"
private:
    void setupTitleBar();
    void updateMaximizeIcon(bool maxed);
class YOURCLASS : public FramelessWindow


.cpp
 VOID

void MainWindow::updateMaximizeIcon(bool maxed) {
    bool isLight = (AppSettings::theme() == AppSettings::Theme::Light);

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

CONSTRUCTOR:
setupTitleBar();
snapPreview = new SnapPreviewWindow(this);
connect(this, &FramelessWindow::windowMaximizedChanged, this, &MainWindow::updateMaximizeIcon);

DESTRUCTOR:
if (snapPreview) {
    snapPreview->hidePreview();
    snapPreview->deleteLater();
}

IN UI -> QWidget#titleBar:  QLabels: IconName, titleLabel;  horizontalLayout: (QPushButtons: btnMinimize, btnMaximize, btnClose);
 */

class FramelessWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit FramelessWindow(QWidget *parent = nullptr);
    ~FramelessWindow() override;

    signals:
        void windowMaximizedChanged(bool maximized);

protected:
    enum ResizeRegion {
        None, Left, Right, Top, Bottom,
        TopLeft, TopRight, BottomLeft, BottomRight
    };
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    ResizeRegion detectResizeRegion(const QPoint &pos);
    void updateCursorShape(const QPoint &pos);
    void toggleMaximizeRestore();
    void toggleFullscreenMode();
    bool eventFilter(QObject *obj, QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *e) override;
    void showEvent(QShowEvent *event) override;

    bool isFullscreenMode = false;
    bool m_uiInitialized = false;
    bool isMaximized = false;
    bool isDragging = false;
    bool isResizing = false;
    QPoint dragOffset;
    QRect savedGeometryBeforeMaximize;
    ResizeRegion currentResizeRegion = None;
    SnapPreviewWindow *snapPreview = nullptr;
private:
    void setResizeCursor(ResizeRegion region);
};

/*
.h
#include "../windowEdit/framelessWindow.h"

class YOURCLASS : public FramelessDialog

private:
    void setupTitleBar();

.cpp

void MainWindow::updateMaximizeIcon(bool maxed) {
    bool isLight = (AppSettings::theme() == AppSettings::Theme::Light);

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

void YOURCLASS::setupTitleBar() {
    ui->titleBar->setMinimumHeight(36);
    ui->titleBar->setMaximumHeight(36);
    connect(ui->btnClose,    &QPushButton::clicked, this, &QWidget::close);
}

CONSTRUCTOR:

YOURCLASS::YOURCLASS(QWidget *parent)
    : FramelessDialog(parent),     // prev: QDialog
      ui(new Ui::YOURCLASS)

setupTitleBar();
IN UI -> QWidget#titleBar:  QLabels: IconName, titleLabel;  horizontalLayout: (QPushButtons: btnClose);

 */

class FramelessDialog : public QDialog {
    Q_OBJECT
public:
    explicit FramelessDialog(QWidget *parent, bool enableShadow = true);
    ~FramelessDialog() override = default;

    void setResizeEnabled(bool enabled) { resizeEnabled = enabled; }
    void setShadowEnabled(bool enabled) { shadowEnabled = enabled; }

private:
    enum ResizeRegion {
        None, Left, Right, Top, Bottom,
        TopLeft, TopRight, BottomLeft, BottomRight
    };
    bool shadowEnabled = true;
    bool resizeEnabled = true;
    bool m_uiInitialized = false;
    bool isDragging = false;
    bool isResizing = false;
    QPoint dragOffset;
    ResizeRegion currentResizeRegion = None;

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void applyShadowEffect();

    ResizeRegion detectResizeRegion(const QPoint &pos);
    void updateCursorShape(const QPoint &pos);
    bool eventFilter(QObject *obj, QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void showEventFade(QShowEvent *event);
};

/*
 EXAMPLE
    before:
       QMessageBox::warning(this, "Warning", "Task title cannot be empty!");
    after:
        // 1. Info
        FramelessMessageBox::information(this, "Saved", "Task saved successfully!");

        // 2. Warn
        FramelessMessageBox::warning(this, "Warning", "Task title cannot be empty!");

        // 3. Question (return bool)
        if (FramelessMessageBox::question(this, "Delete Task", "Are you sure you want to delete this task?")) {
            deleteTask();
        }

        // 4. Error
        FramelessMessageBox::critical(this, "Error", "Database connection failed!");

 */

class FramelessMessageBox : public FramelessDialog
{
    Q_OBJECT
public:
    explicit FramelessMessageBox(const QString &title,
                                 const QString &message,
                                 QMessageBox::Icon icon = QMessageBox::Information,
                                 QWidget *parent = nullptr);

    int exec() override;

    static void information(QWidget *parent,
                            const QString &title,
                            const QString &message);

    static void warning(QWidget *parent,
                        const QString &title,
                        const QString &message);

    static bool question(QWidget *parent,
                         const QString &title,
                         const QString &message);

    static void critical(QWidget *parent,
                         const QString &title,
                         const QString &message);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    private slots:
        void onOkClicked();
    void onCancelClicked();

private:
    QLabel *titleLabel = nullptr;
    QLabel *iconLabel = nullptr;
    QLabel *messageLabel = nullptr;
    QPushButton *okButton = nullptr;
    QPushButton *cancelButton = nullptr;
    QGraphicsDropShadowEffect *shadowEffect = nullptr;

    bool isDragging = false;
    QPoint dragOffset;
    int resultCode = QMessageBox::Rejected;
};

#endif // FRAMELESSDIALOG_H