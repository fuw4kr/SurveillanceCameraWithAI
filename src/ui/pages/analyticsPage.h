#ifndef ANALYTICSPAGE_H
#define ANALYTICSPAGE_H

/**
 * @file analyticsPage.h
 * @brief UI page that visualizes detection analytics and attendance metrics.
 *
 * Presents charts, tables, and rollups for detected faces/objects and aggregates
 * per-camera statistics over time. Receives processed frames from AIProcessor to
 * refresh visuals.
 *
 * @example
 * auto* page = new AnalyticsPage(manager, processor, this);
 * layout->addWidget(page);
 */

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QImage>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QDate>
#include <QDateTime>
#include <QSize>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QAbstractAxis>

#include "../../core/cameraManager.h"
#include "../../core/AIProcessor.h"

struct AttendanceRow {
    QString name;
    QDateTime firstSeen;
    QDateTime lastSeen;
    int detectionCount = 0;
};

class AnalyticsPage : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Builds the analytics dashboard and connects to camera/AI sources.
     * @param manager Camera manager used to register available streams.
     * @param processor AI processor emitting detections for aggregation.
     * @param parent Optional parent widget.
     */
    explicit AnalyticsPage(CameraManager* manager, AIProcessor* processor, QWidget* parent = nullptr);

private slots:
    /**
     * @brief Handles camera selection changes to scope statistics.
     * @param index Selected combo index.
     */
    void onCameraChanged(int index);
    /**
     * @brief Consumes processed frames to update detections and charts.
     * @param id Camera id.
     * @param annotated Annotated frame image.
     * @param detections Detection list.
     * @param sourceSize Original frame size.
     */
    void handleProcessedFrame(int id, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize);

private:
    void buildUi();
    void updateDetections(const QVector<Detection>& detections);
    void updateStats(const QVector<Detection>& detections);
    void recordAnalytics(const QVector<Detection>& detections);
    void resetStatistics(const QDate& day);
    void rebuildHourlySeries();
    void rebuildCohortSeries();
    void rebuildAttendanceTable();
    void updateInsightCards();
    void ensureCameraRegistered(int id);
    QVector<int> buildBaselineCurve() const;
    double computeAveragePresenceHours() const;
    double computeMedianPresenceHours() const;
    int busiestHour() const;

    AIProcessor* aiProcessor = nullptr;

    QComboBox* cameraCombo = nullptr;
    QLabel* previewLabel = nullptr;
    QLabel* statsLabel = nullptr;
    QListWidget* detectionList = nullptr;
    QChartView* activityChartView = nullptr;
    QChartView* cohortChartView = nullptr;
    QChart* activityChart = nullptr;
    QChart* cohortChart = nullptr;
    QBarSeries* activitySeries = nullptr;
    QBarSet* todaySet = nullptr;
    QBarSet* baselineSet = nullptr;
    QPieSeries* cohortSeries = nullptr;
    QTableWidget* attendanceTable = nullptr;
    QLabel* occupancyLabel = nullptr;
    QLabel* anomalyLabel = nullptr;
    QLabel* dwellLabel = nullptr;

    int currentCameraId = -1;
    QVector<int> hourlyCounts;
    QVector<int> baselineHourlyCounts;
    QHash<QString, AttendanceRow> attendance;
    QSet<int> knownCameraIds;
    QDate currentStatsDate;
    int knownDetectionsToday = 0;
    int unknownDetectionsToday = 0;
    QVector<int> rollingFaceWindow;
    const int maxRollingSamples = 72;
};

#endif // ANALYTICSPAGE_H
