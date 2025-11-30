/**
 * @file dashboardpage.cpp
 * @brief Implements the dashboard page with KPI cards, activity chart, and events table.
 *
 * Consumes JSON payloads from the dashboard endpoint to populate summary stats,
 * render a 24-hour activity chart, and list recent events.
 *
 * @example
 * DashboardPage page;
 * page.applyDashboardData(dashboardJson);
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
    auto* header = new QLabel("?? System Dashboard");
    QFont hfont; hfont.setPointSize(18); hfont.setBold(true);
    header->setFont(hfont);
    header->setStyleSheet("color:white;");
    mainLayout->addWidget(header);
    mainLayout->addSpacing(10);

    // === Cards section (4 ��ਧ��⠫�? ����⨢�?) ===
    auto* cardsLayout = new QHBoxLayout;
    cardsLayout->setSpacing(20);

    lblCameras = new QLabel("0");
    lblCameras->setObjectName(QStringLiteral("lblCameras"));
    lblDetections = new QLabel("0");
    lblDetections->setObjectName(QStringLiteral("lblDetections"));
    lblAlerts = new QLabel("0");
    lblAlerts->setObjectName(QStringLiteral("lblAlerts"));
    lblAIStatus = new QLabel("Inactive");
    lblAIStatus->setObjectName(QStringLiteral("lblAIStatus"));

    cardsLayout->addWidget(createCard("Active Cameras ??", lblCameras, QColor("#3B82F6")));
    cardsLayout->addWidget(createCard("Detections Today ?????", lblDetections, QColor("#10B981")));
    cardsLayout->addWidget(createCard("Alerts ??", lblAlerts, QColor("#FACC15")));
    cardsLayout->addWidget(createCard("AI Engine ??", lblAIStatus, QColor("#8B5CF6")));

    // ����� stretch, 鮡 ஧��㢠���� ��୮ �� �ਭ?
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
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); // ?? ������ �� �ਭ?

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
    chartView->setObjectName(QStringLiteral("dashboardChart"));
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(250);
}

// === Table ===
void DashboardPage::setupTable()
{
    tableEvents = new QTableWidget(0, 3);
    tableEvents->setObjectName(QStringLiteral("eventsTable"));
    QStringList headers = { "Time", "Event", "Camera" };
    tableEvents->setHorizontalHeaderLabels(headers);
    tableEvents->verticalHeader()->hide();
    tableEvents->setAlternatingRowColors(true);
    tableEvents->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableEvents->setSelectionMode(QAbstractItemView::SingleSelection);
    tableEvents->horizontalHeader()->setStretchLastSection(true);
    tableEvents->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableEvents->setMinimumHeight(220);
}

void DashboardPage::applyDashboardData(const QJsonObject& json)
{
    const int cameras = json.value("cameras").toInt();
    const int detections = json.value("detections").toInt();
    const int alerts = json.value("alerts").toInt();
    const bool aiActive = json.value("ai_active").toBool();
    updateStats(cameras, detections, alerts, aiActive);

    QList<int> activity;
    const QJsonArray activityArray = json.value("activity").toArray();
    for (const auto& val : activityArray)
        activity.append(val.toInt());
    if (activity.size() == 24)
        updateActivityChart(activity);

    QList<QStringList> rows;
    const QJsonArray events = json.value("events").toArray();
    for (const auto& ev : events) {
        const QJsonObject obj = ev.toObject();
        const QString time = obj.value("time").toString();
        const QString label = obj.value("label").toString();
        const QString camera = obj.value("camera").toString();
        rows.append({ time, label, camera });
    }
    updateRecentEvents(rows);
}

void DashboardPage::updateStats(int cameras, int detections, int alerts, bool aiActive)
{
    lblCameras->setText(QString::number(cameras));
    lblDetections->setText(QString::number(detections));
    lblAlerts->setText(QString::number(alerts));
    lblAIStatus->setText(aiActive ? tr("Running") : tr("Stopped"));
}

void DashboardPage::updateActivityChart(const QList<int>& values)
{
    auto* chart = chartView->chart();
    auto* series = qobject_cast<QLineSeries*>(chart->series().first());
    if (!series)
        return;

    series->clear();
    int maxY = 0;
    for (int i = 0; i < values.size(); ++i) {
        series->append(qreal(i), qreal(values[i]));
        maxY = std::max(maxY, values[i]);
    }

    auto* axisY = qobject_cast<QValueAxis*>(chart->axisY());
    if (axisY)
        axisY->setRange(0, std::max(10, maxY + 5));
}

void DashboardPage::updateRecentEvents(const QList<QStringList>& rows)
{
    if (rows.isEmpty()) {
        tableEvents->setRowCount(1);
        tableEvents->setSpan(0, 0, 1, tableEvents->columnCount());
        auto* placeholder = new QTableWidgetItem(tr("No events"));
        placeholder->setFlags(placeholder->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEditable);
        tableEvents->setItem(0, 0, placeholder);
        return;
    }

    tableEvents->clearSpans();
    tableEvents->setRowCount(rows.size());
    int r = 0;
    for (const QStringList& row : rows) {
        for (int c = 0; c < row.size() && c < tableEvents->columnCount(); ++c) {
            tableEvents->setItem(r, c, new QTableWidgetItem(row[c]));
        }
        ++r;
    }
}
