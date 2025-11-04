#ifndef SNAPPREVIEWWINDOW_H
#define SNAPPREVIEWWINDOW_H

#include <QWidget>
#include <QScreen>
#include <QTimer>
#include <QPainter>

class SnapPreviewWindow : public QWidget
{
    Q_OBJECT
public:
    enum class SnapType { None, Top, Left, Right };

    explicit SnapPreviewWindow(QWidget *parent = nullptr);

    void showPreview(SnapType type, QScreen *screen);
    void hidePreview();
    SnapType currentType() const { return currentSnap; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    SnapType currentSnap = SnapType::None;
    QColor borderColor = QColor(59,130,246,200);
    QColor fillColor = QColor(59,130,246,40);
    QRect lastRect;
};

#endif // SNAPPREVIEWWINDOW_H
