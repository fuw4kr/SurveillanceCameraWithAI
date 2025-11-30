#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QTableWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "ui/pages/dashboardpage.h"

TEST(DashboardPageTest, ApplyDashboardDataUpdatesUi)
{
    DashboardPage page;

    QJsonObject json;
    json.insert(QStringLiteral("cameras"), 5);
    json.insert(QStringLiteral("detections"), 42);
    json.insert(QStringLiteral("alerts"), 3);
    json.insert(QStringLiteral("ai_active"), true);

    QJsonArray activity;
    for (int i = 0; i < 24; ++i)
        activity.append(i);
    json.insert(QStringLiteral("activity"), activity);

    QJsonArray events;
    QJsonObject ev1;
    ev1.insert(QStringLiteral("time"), QStringLiteral("10:00"));
    ev1.insert(QStringLiteral("label"), QStringLiteral("Face detected"));
    ev1.insert(QStringLiteral("camera"), QStringLiteral("Cam1"));
    events.append(ev1);
    json.insert(QStringLiteral("events"), events);

    page.applyDashboardData(json);

    auto* lblCameras = page.findChild<QLabel*>(QStringLiteral("lblCameras"));
    auto* lblDetections = page.findChild<QLabel*>(QStringLiteral("lblDetections"));
    auto* lblAlerts = page.findChild<QLabel*>(QStringLiteral("lblAlerts"));
    auto* lblAIStatus = page.findChild<QLabel*>(QStringLiteral("lblAIStatus"));
    ASSERT_NE(lblCameras, nullptr);
    ASSERT_NE(lblDetections, nullptr);
    ASSERT_NE(lblAlerts, nullptr);
    ASSERT_NE(lblAIStatus, nullptr);

    EXPECT_EQ(lblCameras->text(), QStringLiteral("5"));
    EXPECT_EQ(lblDetections->text(), QStringLiteral("42"));
    EXPECT_EQ(lblAlerts->text(), QStringLiteral("3"));
    EXPECT_EQ(lblAIStatus->text(), QStringLiteral("Running"));

    auto* chartView = page.findChild<QtCharts::QChartView*>(QStringLiteral("dashboardChart"));
    ASSERT_NE(chartView, nullptr);
    auto* series = qobject_cast<QtCharts::QLineSeries*>(chartView->chart()->series().first());
    ASSERT_NE(series, nullptr);
    EXPECT_EQ(series->count(), 24);
    auto* axisY = qobject_cast<QtCharts::QValueAxis*>(chartView->chart()->axisY());
    ASSERT_NE(axisY, nullptr);
    EXPECT_GE(axisY->max(), 23);

    auto* table = page.findChild<QTableWidget*>(QStringLiteral("eventsTable"));
    ASSERT_NE(table, nullptr);
    ASSERT_EQ(table->rowCount(), 1);
    EXPECT_EQ(table->item(0, 0)->text(), QStringLiteral("10:00"));
    EXPECT_EQ(table->item(0, 1)->text(), QStringLiteral("Face detected"));
    EXPECT_EQ(table->item(0, 2)->text(), QStringLiteral("Cam1"));
}

TEST(DashboardPageTest, UpdateStatsSetsInactive)
{
    DashboardPage page;
    page.updateStats(1, 2, 3, false);

    auto* lblAIStatus = page.findChild<QLabel*>(QStringLiteral("lblAIStatus"));
    ASSERT_NE(lblAIStatus, nullptr);
    EXPECT_EQ(lblAIStatus->text(), QStringLiteral("Stopped"));
}

TEST(DashboardPageTest, UpdateActivityChartRescalesYAxis)
{
    DashboardPage page;
    QList<int> values(24, 0);
    values[5] = 200;

    page.updateActivityChart(values);

    auto* chartView = page.findChild<QtCharts::QChartView*>(QStringLiteral("dashboardChart"));
    ASSERT_NE(chartView, nullptr);
    auto* series = qobject_cast<QtCharts::QLineSeries*>(chartView->chart()->series().first());
    ASSERT_NE(series, nullptr);
    EXPECT_EQ(series->count(), 24);

    auto* axisY = qobject_cast<QtCharts::QValueAxis*>(chartView->chart()->axisY());
    ASSERT_NE(axisY, nullptr);
    EXPECT_GE(axisY->max(), 205);
}

TEST(DashboardPageTest, UpdateRecentEventsShowsPlaceholderWhenEmpty)
{
    DashboardPage page;
    page.updateRecentEvents({});

    auto* table = page.findChild<QTableWidget*>(QStringLiteral("eventsTable"));
    ASSERT_NE(table, nullptr);
    ASSERT_EQ(table->rowCount(), 1);
    ASSERT_NE(table->item(0, 0), nullptr);
    EXPECT_EQ(table->item(0, 0)->text(), QStringLiteral("No events"));
}

TEST(DashboardPageTest, UpdateRecentEventsPopulatesRows)
{
    DashboardPage page;
    QList<QStringList> rows;
    rows.append({ QStringLiteral("12:00"), QStringLiteral("Event A"), QStringLiteral("CamA") });
    rows.append({ QStringLiteral("13:00"), QStringLiteral("Event B"), QStringLiteral("CamB") });
    page.updateRecentEvents(rows);

    auto* table = page.findChild<QTableWidget*>(QStringLiteral("eventsTable"));
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->rowCount(), 2);
    EXPECT_EQ(table->item(1, 1)->text(), QStringLiteral("Event B"));
}
