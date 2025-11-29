/**
 * @file DashboardPage.h
 * @brief Dashboard overview page (v2) with charts and recent events table.
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
    explicit DashboardPage(QWidget* parent = nullptr);

    void applyDashboardData(const QJsonObject& json);
    void updateStats(int cameras, int detections, int alerts, bool aiActive);
    void updateActivityChart(const QList<int>& values); // 24 points = hours
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
