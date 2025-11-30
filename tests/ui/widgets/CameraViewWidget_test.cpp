#include <gtest/gtest.h>

#include <QCheckBox>
#include <QImage>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QtTest/QSignalSpy>

#include "ui/widgets/cameraViewWidget.h"

TEST(CameraViewWidgetTest, UpdateFrameSetsOnlineStatus)
{
    CameraViewWidget widget(1);
    QImage frame(20, 20, QImage::Format_RGB32);
    frame.fill(Qt::red);

    widget.updateFrame(frame);
    QMetaObject::invokeMethod(&widget, "refreshStatus", Qt::DirectConnection);

    auto* status = widget.findChild<QLabel*>(QStringLiteral("statusLabel"));
    ASSERT_NE(status, nullptr);
    EXPECT_TRUE(status->text().startsWith(QStringLiteral("Online")));

    auto* preview = widget.findChild<QLabel*>(QStringLiteral("previewLabel"));
    ASSERT_NE(preview, nullptr);
    ASSERT_NE(preview->pixmap(), nullptr);
}

TEST(CameraViewWidgetTest, UpdateFrameIgnoredWhenDisabled)
{
    CameraViewWidget widget(5);
    widget.setCameraActive(false);
    QImage frame(30, 30, QImage::Format_RGB32);
    frame.fill(Qt::green);
    widget.updateFrame(frame);

    auto* preview = widget.findChild<QLabel*>(QStringLiteral("previewLabel"));
    ASSERT_NE(preview, nullptr);
    EXPECT_TRUE(preview->pixmap().isNull());
}

TEST(CameraViewWidgetTest, SetCameraActiveDisablesPreview)
{
    CameraViewWidget widget(2);
    QImage frame(10, 10, QImage::Format_RGB32);
    frame.fill(Qt::blue);
    widget.updateFrame(frame);

    widget.setCameraActive(false);

    auto* status = widget.findChild<QLabel*>(QStringLiteral("statusLabel"));
    auto* toggle = widget.findChild<QPushButton*>(QStringLiteral("toggleButton"));
    auto* preview = widget.findChild<QLabel*>(QStringLiteral("previewLabel"));
    ASSERT_NE(status, nullptr);
    ASSERT_NE(toggle, nullptr);
    ASSERT_NE(preview, nullptr);

    EXPECT_EQ(status->text(), QStringLiteral("Disabled"));
    EXPECT_EQ(toggle->text(), QStringLiteral("Enable"));
    EXPECT_TRUE(preview->pixmap().isNull());
}

TEST(CameraViewWidgetTest, SetOnlineClearsFrameWhenOffline)
{
    CameraViewWidget widget(6);
    QImage frame(12, 12, QImage::Format_RGB32);
    frame.fill(Qt::yellow);
    widget.updateFrame(frame);

    widget.setOnline(false);

    auto* preview = widget.findChild<QLabel*>(QStringLiteral("previewLabel"));
    auto* status = widget.findChild<QLabel*>(QStringLiteral("statusLabel"));
    ASSERT_NE(preview, nullptr);
    ASSERT_NE(status, nullptr);
    EXPECT_TRUE(preview->pixmap().isNull());
    EXPECT_EQ(status->text(), QStringLiteral("Offline"));
}

TEST(CameraViewWidgetTest, AudioVisibilityResetsState)
{
    CameraViewWidget widget(3);
    widget.setAudioChecked(true);
    widget.setAudioVisible(false);

    auto* audio = widget.findChild<QCheckBox*>(QStringLiteral("audioCheck"));
    ASSERT_NE(audio, nullptr);
    EXPECT_FALSE(audio->isVisible());
    EXPECT_FALSE(audio->isChecked());
    EXPECT_FALSE(audio->isEnabled());

    widget.setAudioVisible(true);
    EXPECT_TRUE(audio->isVisible());
    EXPECT_TRUE(audio->isEnabled());
}

TEST(CameraViewWidgetTest, ToggleEmitsSignal)
{
    CameraViewWidget widget(4);
    QSignalSpy spy(&widget, &CameraViewWidget::toggleRequested);
    QMetaObject::invokeMethod(&widget, "handleToggle", Qt::DirectConnection);

    ASSERT_EQ(spy.count(), 1);
    const QList<QVariant> args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toInt(), 4);
    EXPECT_TRUE(args.at(1).toBool());
}
