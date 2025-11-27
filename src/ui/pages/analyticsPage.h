#ifndef ANALYTICSPAGE_H
#define ANALYTICSPAGE_H

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
    explicit AnalyticsPage(CameraManager* manager, AIProcessor* processor, QWidget* parent = nullptr);

private slots:
    void onCameraChanged(int index);
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
