/**
 * @file DashboardPage.cpp
 * @brief Implementation of DashboardPage v2 (with chart + events table, Qt6 namespace)
 */

#include "DashboardPage.h"
#include <QHeaderView>
#include <QFont>
#include <QJsonArray>
#include <algorithm>

DashboardPage::DashboardPage(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void DashboardPage::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    // === Header ===
    auto* header = new QLabel("📊 System Dashboard");
    QFont hfont; hfont.setPointSize(18); hfont.setBold(true);
    header->setFont(hfont);
    header->setStyleSheet("color:white;");
    mainLayout->addWidget(header);
    mainLayout->addSpacing(10);

    // === Cards section (4 горизонтальні адаптивні) ===
    auto* cardsLayout = new QHBoxLayout;
    cardsLayout->setSpacing(20);

    lblCameras = new QLabel("0");
    lblDetections = new QLabel("0");
    lblAlerts = new QLabel("0");
    lblAIStatus = new QLabel("Inactive");

    cardsLayout->addWidget(createCard("Active Cameras 📷", lblCameras, QColor("#3B82F6")));
    cardsLayout->addWidget(createCard("Detections Today 👁‍🗨", lblDetections, QColor("#10B981")));
    cardsLayout->addWidget(createCard("Alerts ⚠️", lblAlerts, QColor("#FACC15")));
    cardsLayout->addWidget(createCard("AI Engine 🧠", lblAIStatus, QColor("#8B5CF6")));

    // Додай stretch, щоб розтягувалось гарно по ширині
    cardsLayout->addStretch();

    mainLayout->addLayout(cardsLayout);

    // === Chart ===
    setupChart();
    mainLayout->addWidget(chartView, 1);

    // === Recent events ===
    setupTable();
    mainLayout->addWidget(tableEvents, 2);

    setStyleSheet(R"(
        QWidget {
            background-color: #121212;
            color: #ddd;
        }
        QHeaderView::section {
            background:#1f1f1f;
            color:#bbb;
            border:none;
            padding:6px;
        }
        QTableWidget {
            background:#181818;
            alternate-background-color:#1c1c1c;
            gridline-color:#333;
            selection-background-color:#2563EB;
            selection-color:white;
        }
    )");
}


// === Stats Cards ===
QFrame* DashboardPage::createCard(const QString& title, QLabel* valueLbl, const QColor& color)
{
    auto* card = new QFrame;
    card->setFrameShape(QFrame::StyledPanel);
    card->setMinimumHeight(120);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); // 🔹 тягнеться по ширині

    card->setStyleSheet(R"(
        QFrame {
            background-color:#1E1E1E;
            border-radius:12px;
            border:1px solid #2a2a2a;
        }
        QLabel { color:#ccc; }
    )");

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(6);

    auto* titleLbl = new QLabel(title);
    QFont tf; tf.setPointSize(11);
    titleLbl->setFont(tf);
    titleLbl->setStyleSheet("color:#999;");

    QFont vf; vf.setPointSize(24); vf.setBold(true);
    valueLbl->setFont(vf);
    valueLbl->setStyleSheet(QString("color:%1;").arg(color.name()));

    layout->addWidget(titleLbl);
    layout->addWidget(valueLbl);
    layout->addStretch();

    return card;
}

// === Chart ===
void DashboardPage::setupChart()
{
    auto* series = new QLineSeries();
    series->setColor(QColor("#3B82F6"));

    // initialize with zeros (24 hours)
    for (int i = 0; i < 24; ++i)
        series->append(qreal(i), 0.0);

    auto* chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Detections by Hour");
    chart->setTheme(QChart::ChartThemeDark);
    chart->legend()->hide();
    chart->setBackgroundBrush(QColor("#1A1A1A"));

    auto* axisX = new QValueAxis();
    axisX->setTitleText("Hour");
    axisX->setRange(0, 23);
    axisX->setTickCount(13);

    auto* axisY = new QValueAxis();
    axisY->setTitleText("Detections");
    axisY->setRange(0, 100);

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(250);
}

// === Table ===
void DashboardPage::setupTable()
{
    tableEvents = new QTableWidget(0, 3);
    QStringList headers = { "Time", "Event", "Camera" };
    tableEvents->setHorizontalHeaderLabels(headers);
    tableEvents->verticalHeader()->hide();
    tableEvents->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableEvents->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableEvents->setAlternatingRowColors(true);
    tableEvents->horizontalHeader()->setStretchLastSection(true);
    tableEvents->setMinimumHeight(220);
}

// === Data Updates ===
void DashboardPage::applyDashboardData(const QJsonObject& json)
{
    updateStats(
        json.value(QStringLiteral("cameras")).toInt(),
        json.value(QStringLiteral("detectionsToday")).toInt(),
        json.value(QStringLiteral("alerts")).toInt(),
        json.value(QStringLiteral("aiActive")).toBool());

    QList<int> series(24, 0);
    const auto detectionsByHour = json.value(QStringLiteral("detectionsByHour")).toArray();
    for (int i = 0; i < detectionsByHour.size() && i < series.size(); ++i)
        series[i] = detectionsByHour.at(i).toInt();
    updateActivityChart(series);

    QList<QStringList> rows;
    const auto recentEvents = json.value(QStringLiteral("recentEvents")).toArray();
    rows.reserve(recentEvents.size());
    for (const auto& ev : recentEvents) {
        const auto o = ev.toObject();
        rows.append({
            o.value(QStringLiteral("time")).toString(),
            o.value(QStringLiteral("event")).toString(),
            o.value(QStringLiteral("camera")).toString()
            });
    }
    updateRecentEvents(rows);
}

void DashboardPage::updateStats(int cameras, int detections, int alerts, bool aiActive)
{
    lblCameras->setText(QString::number(cameras));
    lblDetections->setText(QString::number(detections));
    lblAlerts->setText(QString::number(alerts));
    lblAIStatus->setText(aiActive ? "Active" : "Stopped");
}

void DashboardPage::updateActivityChart(const QList<int>& values)
{
    if (!chartView) return;
    auto chart = chartView->chart();
    auto* series = qobject_cast<QLineSeries*>(chart->series().first());
    if (!series) return;

    series->clear();
    int maxVal = 0;
    for (int i = 0; i < values.size(); ++i) {
        maxVal = std::max(maxVal, values[i]);
        series->append(qreal(i), qreal(values[i]));
    }

    // adjust Y axis to current data range
    if (!chart->axes(Qt::Vertical).isEmpty()) {
        auto* axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());
        if (axisY) {
            const int upper = std::max(5, maxVal + 2);
            axisY->setRange(0, upper);
        }
    }
}

void DashboardPage::updateRecentEvents(const QList<QStringList>& rows)
{
    tableEvents->clearContents();
    tableEvents->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        for (int j = 0; j < rows[i].size(); ++j)
            tableEvents->setItem(i, j, new QTableWidgetItem(rows[i][j]));
    }
}
