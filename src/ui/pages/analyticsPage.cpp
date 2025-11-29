/**
 * @file analyticsPage.cpp
 * @brief Implements the analytics dashboard with charts, tables, and insights.
 */
#include "analyticsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QTableWidgetItem>
#include <QList>
#include <QPixmap>
#include <QIcon>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QFrame>
#include <QtCharts/QLegend>

#include <QtMath>
#include <cmath>
#include <algorithm>

namespace {
const QSize kPreviewSize(640, 360);

QString formatTime(const QDateTime& dt)
{
    if (!dt.isValid())
        return QStringLiteral("—");
    return dt.time().toString(QStringLiteral("HH:mm"));
}
}

AnalyticsPage::AnalyticsPage(CameraManager* manager, AIProcessor* processor, QWidget* parent)
    : QWidget(parent)
    , aiProcessor(processor)
{
    Q_UNUSED(manager);
    hourlyCounts = QVector<int>(24, 0);
    baselineHourlyCounts = buildBaselineCurve();

    buildUi();
    resetStatistics(QDate::currentDate());

    if (aiProcessor) {
        connect(aiProcessor, &AIProcessor::frameProcessed,
            this, &AnalyticsPage::handleProcessedFrame, Qt::QueuedConnection);
    }
}

void AnalyticsPage::buildUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    outerLayout->addWidget(scrollArea);

    QWidget* content = new QWidget;
    scrollArea->setWidget(content);

    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel(tr("AI Analytics & BI Dashboard"));
    title->setStyleSheet("font-size:20px; font-weight:600;");

    cameraCombo = new QComboBox;
    cameraCombo->setMinimumWidth(220);
    connect(cameraCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AnalyticsPage::onCameraChanged);

    header->addWidget(title);
    header->addStretch();
    header->addWidget(new QLabel(tr("Камера:")));
    header->addWidget(cameraCombo);
    layout->addLayout(header);

    auto createCard = [](const QString& titleText) -> QLabel* {
        auto* card = new QLabel;
        card->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        card->setMinimumHeight(90);
        card->setWordWrap(true);
        card->setStyleSheet(R"(
            QLabel {
                background:#0f172a;
                border:1px solid #1e293b;
                border-radius:12px;
                padding:12px;
                color:#e2e8f0;
                font-size:13px;
            }
        )");
        card->setText(QStringLiteral("<b>%1</b><br><span style='color:#94a3b8;'>—</span>").arg(titleText));
        return card;
    };

    occupancyLabel = createCard(tr("Заповнюваність офісу"));
    anomalyLabel = createCard(tr("Аномалії трафіку"));
    dwellLabel = createCard(tr("Середній час перебування"));

    auto* cardLayout = new QHBoxLayout;
    cardLayout->setSpacing(12);
    cardLayout->addWidget(occupancyLabel);
    cardLayout->addWidget(anomalyLabel);
    cardLayout->addWidget(dwellLabel);
    layout->addLayout(cardLayout);

    activityChartView = new QChartView(this);
    activityChartView->setMinimumHeight(320);
    activityChart = new QChart;
    activitySeries = new QBarSeries(this);
    todaySet = new QBarSet(tr("Поточний день"));
    baselineSet = new QBarSet(tr("Історичний тренд"));
    activitySeries->append(baselineSet);
    activitySeries->append(todaySet);
    activityChart->addSeries(activitySeries);
    activityChart->setAnimationOptions(QChart::SeriesAnimations);
    activityChart->setTitle(tr("Графік активності по годинах"));
    activityChart->legend()->setAlignment(Qt::AlignBottom);
    activityChart->setMargins(QMargins(8, 16, 8, 8));

    QStringList hours;
    for (int h = 0; h < 24; ++h)
        hours << QStringLiteral("%1").arg(h, 2, 10, QLatin1Char('0'));
    auto* axisX = new QBarCategoryAxis;
    axisX->append(hours);
    auto* axisY = new QValueAxis;
    axisY->setLabelFormat("%d");
    axisY->setTitleText(tr("Людей за годину"));
    axisY->setRange(0, 60);
    activityChart->addAxis(axisX, Qt::AlignBottom);
    activityChart->addAxis(axisY, Qt::AlignLeft);
    activitySeries->attachAxis(axisX);
    activitySeries->attachAxis(axisY);
    activityChartView->setChart(activityChart);
    activityChartView->setRenderHint(QPainter::Antialiasing);

    cohortChartView = new QChartView(this);
    cohortChartView->setMinimumHeight(320);
    cohortChart = new QChart;
    cohortSeries = new QPieSeries(this);
    cohortChart->addSeries(cohortSeries);
    cohortChart->setTitle(tr("Унікальні vs постійні"));
    cohortChart->legend()->setAlignment(Qt::AlignBottom);
    cohortChartView->setChart(cohortChart);
    cohortChartView->setRenderHint(QPainter::Antialiasing);

    auto* chartsLayout = new QHBoxLayout;
    chartsLayout->setSpacing(12);
    chartsLayout->addWidget(activityChartView, 2);
    chartsLayout->addWidget(cohortChartView, 1);
    layout->addLayout(chartsLayout);

    auto* tableHeader = new QLabel(tr("Облік робочого часу"));
    tableHeader->setStyleSheet("font-weight:600; font-size:16px;");
    layout->addWidget(tableHeader);

    attendanceTable = new QTableWidget;
    attendanceTable->setColumnCount(4);
    attendanceTable->setHorizontalHeaderLabels(
        { tr("Ім'я"), tr("Час приходу"), tr("Час виходу"), tr("Годин в офісі") });
    attendanceTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    attendanceTable->verticalHeader()->setVisible(false);
    attendanceTable->setEditTriggers(QTableWidget::NoEditTriggers);
    attendanceTable->setSelectionMode(QTableWidget::NoSelection);
    attendanceTable->setStyleSheet(R"(
        QTableWidget {
            background:#020617;
            border:1px solid #1e293b;
            border-radius:10px;
            color:#e2e8f0;
        }
        QHeaderView::section {
            background:#0f172a;
            color:#cbd5f5;
            border:none;
            padding:6px;
        }
    )");
    layout->addWidget(attendanceTable);

    statsLabel = new QLabel(tr("Немає даних"));
    statsLabel->setStyleSheet("color:#e5e7eb; font-weight:500;");
    layout->addWidget(statsLabel);

    previewLabel = new QLabel;
    previewLabel->setMinimumSize(kPreviewSize);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet("background:#050505; border:1px solid #1f2937; border-radius:8px;");
    previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    detectionList = new QListWidget;
    detectionList->setMinimumWidth(260);
    detectionList->setIconSize(QSize(64, 64));
    detectionList->setStyleSheet(R"(
        QListWidget {
            background:#111827;
            border:1px solid #1f2937;
            border-radius:6px;
            color:#e5e7eb;
        }
    )");

    auto* liveLayout = new QHBoxLayout;
    liveLayout->setSpacing(12);
    liveLayout->addWidget(previewLabel, 2);
    liveLayout->addWidget(detectionList, 1);
    layout->addLayout(liveLayout, 1);

    for (int h = 0; h < 24; ++h) {
        *todaySet << hourlyCounts[h];
        *baselineSet << baselineHourlyCounts[h];
    }
    rebuildCohortSeries();
    rebuildAttendanceTable();
    updateInsightCards();
}

void AnalyticsPage::onCameraChanged(int index)
{
    if (index < 0)
        return;
    currentCameraId = cameraCombo->itemData(index).toInt();
    if (aiProcessor)
        aiProcessor->resetBackground();
    detectionList->clear();
    statsLabel->setText(tr("Очікуємо кадри..."));
}

void AnalyticsPage::handleProcessedFrame(int id, const QImage& annotated,
    const QVector<Detection>& detections, const QSize& sourceSize)
{
    Q_UNUSED(sourceSize);
    ensureCameraRegistered(id);

    if (currentCameraId == -1)
        currentCameraId = id;

    if (id != currentCameraId)
        return;

    if (previewLabel) {
        QPixmap pixmap;
        if (!annotated.isNull()) {
            pixmap = QPixmap::fromImage(annotated).scaled(
                previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        previewLabel->setPixmap(pixmap);
    }

    updateDetections(detections);
}

void AnalyticsPage::updateDetections(const QVector<Detection>& detections)
{
    recordAnalytics(detections);

    detectionList->clear();
    for (const Detection& d : detections) {
        const QString category = d.category.isEmpty() ? d.label : d.category;
        const QString displayLabel = d.label.isEmpty() ? category : d.label;
        const QString entry = QString("%1 [%2]  |  conf:%3  |  (%4,%5,%6,%7)")
                                  .arg(displayLabel)
                                  .arg(category.isEmpty() ? tr("Unknown") : category)
                                  .arg(QString::number(d.confidence, 'f', 2))
                                  .arg(QString::number(d.rect.x()))
                                  .arg(QString::number(d.rect.y()))
                                  .arg(QString::number(d.rect.width()))
                                  .arg(QString::number(d.rect.height()));

        auto* item = new QListWidgetItem(entry);
        if (!d.previewPath.isEmpty()) {
            QPixmap pixmap(d.previewPath);
            if (!pixmap.isNull()) {
                const QPixmap thumb = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                item->setIcon(QIcon(thumb));
            }
        }
        detectionList->addItem(item);
    }

    if (detections.isEmpty())
        statsLabel->setText(tr("Немає детекцій"));
    else
        updateStats(detections);
}

void AnalyticsPage::ensureCameraRegistered(int id)
{
    if (!cameraCombo || knownCameraIds.contains(id))
        return;

    knownCameraIds.insert(id);
    const QString label = tr("Camera %1").arg(id);
    {
        QSignalBlocker blocker(cameraCombo);
        cameraCombo->addItem(label, id);
    }

    if (currentCameraId == -1) {
        currentCameraId = id;
        const int comboIndex = cameraCombo->findData(id);
        if (comboIndex >= 0) {
            QSignalBlocker blocker(cameraCombo);
            cameraCombo->setCurrentIndex(comboIndex);
        }
        statsLabel->setText(tr("Аналітика для %1").arg(label));
    }
}

void AnalyticsPage::updateStats(const QVector<Detection>& detections)
{
    int faceCount = 0;
    int objectCount = 0;
    int personCount = 0;
    for (const Detection& d : detections) {
        const QString category = d.category.isEmpty() ? d.label : d.category;
        if (category.compare("Face", Qt::CaseInsensitive) == 0)
            ++faceCount;
        else if (category.compare("Object", Qt::CaseInsensitive) == 0)
            ++objectCount;
        else if (category.compare("Person", Qt::CaseInsensitive) == 0)
            ++personCount;
    }

    const int totalKnown = attendance.size();
    const int peak = busiestHour();
    const QString peakLabel = peak >= 0
        ? QStringLiteral("%1:00").arg(peak, 2, 10, QLatin1Char('0'))
        : QStringLiteral("—");

    statsLabel->setText(tr("Faces:%1   Persons:%2   Objects:%3   |   Унікальних сьогодні: %4   |   Пік: %5")
                            .arg(faceCount)
                            .arg(personCount)
                            .arg(objectCount)
                            .arg(totalKnown)
                            .arg(peakLabel));
}

void AnalyticsPage::recordAnalytics(const QVector<Detection>& detections)
{
    if (detections.isEmpty())
        return;

    const QDateTime now = QDateTime::currentDateTime();
    if (!currentStatsDate.isValid() || currentStatsDate != now.date())
        resetStatistics(now.date());

    int facesThisFrame = 0;
    int knownThisFrame = 0;
    int unknownThisFrame = 0;
    const int hour = now.time().hour();

    for (const Detection& d : detections) {
        const QString category = d.category.isEmpty() ? d.label : d.category;
        if (category.compare("Face", Qt::CaseInsensitive) != 0)
            continue;

        ++facesThisFrame;
        const QString label = d.label.trimmed();
        const bool isKnown = !label.isEmpty() && label.compare("Face", Qt::CaseInsensitive) != 0;
        if (isKnown) {
            ++knownThisFrame;
            AttendanceRow& row = attendance[label];
            row.name = label;
            if (!row.firstSeen.isValid())
                row.firstSeen = now;
            row.lastSeen = now;
            row.detectionCount += 1;
        } else {
            ++unknownThisFrame;
        }
    }

    if (facesThisFrame == 0)
        return;

    hourlyCounts[hour] += facesThisFrame;
    knownDetectionsToday += knownThisFrame;
    unknownDetectionsToday += unknownThisFrame;

    rollingFaceWindow.append(facesThisFrame);
    if (rollingFaceWindow.size() > maxRollingSamples)
        rollingFaceWindow.removeFirst();

    rebuildHourlySeries();
    rebuildCohortSeries();
    rebuildAttendanceTable();
    updateInsightCards();
}

void AnalyticsPage::resetStatistics(const QDate& day)
{
    currentStatsDate = day;
    std::fill(hourlyCounts.begin(), hourlyCounts.end(), 0);
    knownDetectionsToday = 0;
    unknownDetectionsToday = 0;
    attendance.clear();
    rollingFaceWindow.clear();
    rebuildHourlySeries();
    rebuildCohortSeries();
    rebuildAttendanceTable();
    updateInsightCards();
}

void AnalyticsPage::rebuildHourlySeries()
{
    if (!todaySet || !baselineSet)
        return;

    const int size = hourlyCounts.size();
    if (todaySet->count() != size || baselineSet->count() != size)
        return;

    for (int i = 0; i < size; ++i)
        todaySet->replace(i, hourlyCounts[i]);
    for (int i = 0; i < size; ++i)
        baselineSet->replace(i, baselineHourlyCounts.value(i));

    int maxValue = 0;
    for (int v : hourlyCounts)
        maxValue = std::max(maxValue, v);
    for (int v : baselineHourlyCounts)
        maxValue = std::max(maxValue, v);
    maxValue = std::max(10, ((maxValue + 4) / 5) * 5);

    if (activityChart) {
        const auto axes = activityChart->axes(Qt::Vertical);
        if (!axes.isEmpty()) {
            if (auto* valueAxis = qobject_cast<QValueAxis*>(axes.first()))
                valueAxis->setRange(0, maxValue);
        }
    }
}

void AnalyticsPage::rebuildCohortSeries()
{
    if (!cohortSeries)
        return;

    cohortSeries->clear();
    const qreal known = knownDetectionsToday;
    const qreal unknown = unknownDetectionsToday;
    if (known <= 0 && unknown <= 0) {
        cohortSeries->append(tr("Очікуємо події"), 1.0);
        return;
    }

    auto* knownSlice = cohortSeries->append(tr("Співробітники"), known);
    auto* unknownSlice = cohortSeries->append(tr("Нові відвідувачі"), unknown);
    if (knownSlice) {
        knownSlice->setBrush(QColor("#34d399"));
        knownSlice->setLabelVisible(true);
    }
    if (unknownSlice) {
        unknownSlice->setBrush(QColor("#f87171"));
        unknownSlice->setLabelVisible(true);
    }
}

void AnalyticsPage::rebuildAttendanceTable()
{
    if (!attendanceTable)
        return;

    QList<AttendanceRow> entries = attendance.values();
    std::sort(entries.begin(), entries.end(), [](const AttendanceRow& a, const AttendanceRow& b) {
        return a.firstSeen < b.firstSeen;
    });

    attendanceTable->setRowCount(entries.size());
    for (int rowIndex = 0; rowIndex < entries.size(); ++rowIndex) {
        const auto& row = entries[rowIndex];
        const QString arrival = formatTime(row.firstSeen);
        const QString departure = formatTime(row.lastSeen);
        const double hours = row.firstSeen.isValid()
            ? std::max(0.0, static_cast<double>(row.firstSeen.secsTo(row.lastSeen)) / 3600.0)
            : 0.0;

        attendanceTable->setItem(rowIndex, 0, new QTableWidgetItem(row.name));
        attendanceTable->setItem(rowIndex, 1, new QTableWidgetItem(arrival));
        attendanceTable->setItem(rowIndex, 2, new QTableWidgetItem(departure));
        attendanceTable->setItem(rowIndex, 3, new QTableWidgetItem(QString::number(hours, 'f', 2)));
    }
}

void AnalyticsPage::updateInsightCards()
{
    const int uniqueKnown = attendance.size();
    const int officeCapacity = 40;
    const double occupancy = officeCapacity > 0
        ? std::min(100.0, (static_cast<double>(uniqueKnown) / officeCapacity) * 100.0)
        : 0.0;
    if (occupancyLabel) {
        occupancyLabel->setText(
            tr("<b>Заповнюваність офісу</b><br>"
               "<span style='font-size:26px;color:#fef3c7;'>%1%</span><br>"
               "<span style='color:#94a3b8;'>%2 / %3 очікуваних</span>")
                .arg(QString::number(occupancy, 'f', 1))
                .arg(uniqueKnown)
                .arg(officeCapacity));
    }

    const double avgPresence = computeAveragePresenceHours();
    const double medianPresence = computeMedianPresenceHours();
    if (dwellLabel) {
        dwellLabel->setText(
            tr("<b>Середній час перебування</b><br>"
               "<span style='font-size:26px;color:#bae6fd;'>%1 год.</span><br>"
               "<span style='color:#94a3b8;'>Медіана: %2 год.</span>")
                .arg(QString::number(avgPresence, 'f', 2))
                .arg(QString::number(medianPresence, 'f', 2)));
    }

    if (anomalyLabel) {
        const int sampleCount = rollingFaceWindow.size();
        double mean = 0.0;
        if (sampleCount > 0) {
            for (int value : rollingFaceWindow)
                mean += value;
            mean /= sampleCount;
        }
        double variance = 0.0;
        if (sampleCount > 1) {
            for (int value : rollingFaceWindow)
                variance += std::pow(value - mean, 2);
            variance /= (sampleCount - 1);
        }
        const double stddev = std::sqrt(std::max(0.0, variance));
        const double latest = rollingFaceWindow.isEmpty() ? 0.0 : rollingFaceWindow.last();
        const double z = (stddev > 1e-3) ? (latest - mean) / stddev : 0.0;
        const double ciHalfWidth = (sampleCount > 1)
            ? 1.96 * (stddev / std::sqrt(sampleCount))
            : 0.0;
        anomalyLabel->setText(
            tr("<b>Аномалії трафіку</b><br>"
               "<span style='font-size:26px;color:#ddd6fe;'>Z=%1</span><br>"
               "<span style='color:#94a3b8;'>95% CI: [%2 ; %3]</span>")
                .arg(QString::number(z, 'f', 2))
                .arg(QString::number(mean - ciHalfWidth, 'f', 2))
                .arg(QString::number(mean + ciHalfWidth, 'f', 2)));
    }
}

QVector<int> AnalyticsPage::buildBaselineCurve() const
{
    QVector<int> values(24, 0);
    for (int hour = 0; hour < 24; ++hour) {
        const double morningPeak = std::exp(-0.5 * std::pow((hour - 9.0) / 1.8, 2.0));
        const double eveningPeak = std::exp(-0.5 * std::pow((hour - 18.0) / 2.3, 2.0));
        const double noise = 0.3;
        values[hour] = qBound(1, static_cast<int>(std::round(4 + 30 * (morningPeak + eveningPeak + noise))), 60);
    }
    return values;
}

double AnalyticsPage::computeAveragePresenceHours() const
{
    if (attendance.isEmpty())
        return 0.0;

    double total = 0.0;
    int count = 0;
    for (const auto& row : attendance) {
        if (!row.firstSeen.isValid() || !row.lastSeen.isValid())
            continue;
        total += std::max(0.0, static_cast<double>(row.firstSeen.secsTo(row.lastSeen)) / 3600.0);
        ++count;
    }
    return count > 0 ? total / count : 0.0;
}

double AnalyticsPage::computeMedianPresenceHours() const
{
    QList<double> samples;
    for (const auto& row : attendance) {
        if (!row.firstSeen.isValid() || !row.lastSeen.isValid())
            continue;
        samples.append(std::max(0.0, static_cast<double>(row.firstSeen.secsTo(row.lastSeen)) / 3600.0));
    }
    if (samples.isEmpty())
        return 0.0;

    std::sort(samples.begin(), samples.end());
    const int mid = samples.size() / 2;
    if (samples.size() % 2 == 0)
        return (samples[mid - 1] + samples[mid]) / 2.0;
    return samples[mid];
}

int AnalyticsPage::busiestHour() const
{
    int maxHour = -1;
    int maxValue = -1;
    for (int h = 0; h < hourlyCounts.size(); ++h) {
        if (hourlyCounts[h] > maxValue) {
            maxValue = hourlyCounts[h];
            maxHour = h;
        }
    }
    return maxHour;
}
