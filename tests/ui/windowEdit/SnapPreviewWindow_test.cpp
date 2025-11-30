#include <gtest/gtest.h>

#include <QGuiApplication>
#include <QScreen>

#include "ui/windowEdit/snapPreviewWindow.h"

TEST(SnapPreviewWindowTest, LeftSnapUsesHalfScreen)
{
    SnapPreviewWindow window;
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen)
        GTEST_SKIP() << "No primary screen available";

    window.showPreview(SnapPreviewWindow::SnapType::Left, screen);

    EXPECT_EQ(window.currentType(), SnapPreviewWindow::SnapType::Left);
    const QRect screenRect = screen->geometry();
    EXPECT_EQ(window.geometry().width(), screenRect.width() / 2);
    EXPECT_EQ(window.geometry().height(), screenRect.height());
    EXPECT_TRUE(window.isVisible());
}

TEST(SnapPreviewWindowTest, RightSnapUsesHalfScreen)
{
    SnapPreviewWindow window;
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen)
        GTEST_SKIP() << "No primary screen available";

    window.showPreview(SnapPreviewWindow::SnapType::Right, screen);

    EXPECT_EQ(window.currentType(), SnapPreviewWindow::SnapType::Right);
    const QRect screenRect = screen->geometry();
    EXPECT_EQ(window.geometry().width(), screenRect.width() / 2);
    EXPECT_EQ(window.geometry().height(), screenRect.height());
    EXPECT_EQ(window.geometry().x(), screenRect.x() + screenRect.width() / 2);
    EXPECT_TRUE(window.isVisible());
}

TEST(SnapPreviewWindowTest, NoneHidesPreview)
{
    SnapPreviewWindow window;
    window.showPreview(SnapPreviewWindow::SnapType::None, nullptr);
    EXPECT_EQ(window.currentType(), SnapPreviewWindow::SnapType::None);
    EXPECT_FALSE(window.isVisible());
}
