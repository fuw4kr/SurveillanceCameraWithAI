/**
 * @file DashboardPage.h
 * @brief Dashboard overview page with KPI cards, charts, and recent events.
 *
 * Consumes JSON data from the dashboard API to render counters, an hourly
 * activity line chart, and a table of recent events.
 *
 * @example
 * DashboardPage* page = new DashboardPage(this);
 * page->applyDashboardData(jsonPayload);
 */

#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QTableWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QJsonObject>

class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the dashboard layout and initializes charts/tables.
     * @param parent Optional parent widget.
     */
    explicit DashboardPage(QWidget* parent = nullptr);

    /**
     * @brief Applies server-provided dashboard JSON to update all widgets.
     * @param json Dashboard payload including stats, activity, and events.
     */
    void applyDashboardData(const QJsonObject& json);
    /**
     * @brief Updates KPI cards for cameras/detections/alerts/AI status.
     * @param cameras Count of cameras.
     * @param detections Count of detections.
     * @param alerts Count of alerts.
     * @param aiActive Whether AI processing is active.
     */
    void updateStats(int cameras, int detections, int alerts, bool aiActive);
    /**
     * @brief Renders 24-hour activity chart from hourly values.
     * @param values List of 24 integers.
     */
    void updateActivityChart(const QList<int>& values); // 24 points = hours
    /**
     * @brief Refreshes the recent events table.
     * @param rows Rows in order: time, label, camera.
     */
    void updateRecentEvents(const QList<QStringList>& rows); // {{"11:25","Unknown face","Cam#2"}, ...}

private:
    QLabel* lblCameras;
    QLabel* lblDetections;
    QLabel* lblAlerts;
    QLabel* lblAIStatus;
    QChartView* chartView;
    QTableWidget* tableEvents;

    void setupUi();
    QFrame* createCard(const QString& title, QLabel* valueLbl, const QColor& color);
    void setupChart();
    void setupTable();
};

#endif // DASHBOARDPAGE_H
