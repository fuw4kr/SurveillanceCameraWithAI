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
#include <QtCharts/QAbstractAxis>
#include <QJsonObject>
#include <QVector>
#include <QColor>

class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    enum class Theme {
        Light,
        Dark
    };

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
    /**
     * @brief Applies the current application theme so custom colors/icons stay in sync.
     * @param theme Main window theme selection.
     */
    void applyTheme(Theme theme);

private:
    struct CardWidgets {
        QFrame* frame = nullptr;
        QLabel* title = nullptr;
        QLabel* value = nullptr;
        QLabel* icon = nullptr;
        QColor accent;
        QString iconName;
    };

    QLabel* lblCameras;
    QLabel* lblDetections;
    QLabel* lblAlerts;
    QLabel* lblAIStatus;
    QChartView* chartView;
    QTableWidget* tableEvents;
    QLabel* headerIcon = nullptr;
    QLabel* headerLabel = nullptr;
    QVector<CardWidgets> statCards;
    Theme currentTheme = Theme::Dark;

    void setupUi();
    CardWidgets createCard(const QString& title, QLabel* valueLbl, const QColor& color, const QString& iconName);
    void setupChart();
    void setupTable();
    void refreshIcons();
    void refreshCardStyles();
    void refreshChartTheme();
    QString themedIconPath(const QString& base) const;
};

#endif // DASHBOARDPAGE_H
