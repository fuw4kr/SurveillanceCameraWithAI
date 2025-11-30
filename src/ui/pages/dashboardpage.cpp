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
#include <QBrush>
#include <QFont>
#include <QHeaderView>
#include <QJsonArray>
#include <QPixmap>
#include <QFileInfo>
#include <algorithm>

DashboardPage::DashboardPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("dashboardPageRoot");
    setupUi();
    applyTheme(currentTheme);
}

void DashboardPage::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    // === Header ===
    auto* headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(10);
    headerIcon = new QLabel;
    headerIcon->setFixedSize(32, 32);
    headerIcon->setScaledContents(true);
    headerLabel = new QLabel(tr("System Dashboard"));
    QFont hfont;
    hfont.setPointSize(18);
    hfont.setBold(true);
    headerLabel->setFont(hfont);
    headerLayout->addWidget(headerIcon);
    headerLayout->addWidget(headerLabel);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);
    mainLayout->addSpacing(10);

    // === Cards ===
    auto* cardsLayout = new QHBoxLayout;
    cardsLayout->setSpacing(20);

    lblCameras = new QLabel("0");
    lblCameras->setObjectName(QStringLiteral("lblCameras"));
    lblDetections = new QLabel("0");
    lblDetections->setObjectName(QStringLiteral("lblDetections"));
    lblAlerts = new QLabel("0");
    lblAIStatus = new QLabel(tr("Inactive"));

    statCards.clear();
    statCards.reserve(4);
    const auto appendCard = [&](const QString& title, QLabel* valueLabel, const QColor& accent, const QString& icon) {
        CardWidgets card = createCard(title, valueLabel, accent, icon);
        cardsLayout->addWidget(card.frame);
        statCards.append(card);
    };

    appendCard(tr("Active Cameras"), lblCameras, QColor("#3B82F6"), QStringLiteral("camera"));
    appendCard(tr("Detections Today"), lblDetections, QColor("#10B981"), QStringLiteral("detection"));
    appendCard(tr("Alerts"), lblAlerts, QColor("#FACC15"), QStringLiteral("alert"));
    appendCard(tr("AI Engine"), lblAIStatus, QColor("#8B5CF6"), QStringLiteral("ai"));

    cardsLayout->addStretch();
    mainLayout->addLayout(cardsLayout);

    // === Chart ===
    setupChart();
    mainLayout->addWidget(chartView, 1);

    // === Recent events ===
    setupTable();
    mainLayout->addWidget(tableEvents, 2);
}

// === Stats Cards ===
DashboardPage::CardWidgets DashboardPage::createCard(const QString& title, QLabel* valueLbl, const QColor& color, const QString& iconName)
{
    CardWidgets widgets;
    widgets.frame = new QFrame;
    widgets.frame->setFrameShape(QFrame::StyledPanel);
    widgets.frame->setMinimumHeight(120);
    widgets.frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(widgets.frame);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(6);

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(10);
    auto* iconLbl = new QLabel;
    iconLbl->setFixedSize(32, 32);
    iconLbl->setScaledContents(true);
    auto* titleLbl = new QLabel(title);
    QFont tf;
    tf.setPointSize(11);
    titleLbl->setFont(tf);
    headerRow->addWidget(iconLbl);
    headerRow->addWidget(titleLbl, 1);
    headerRow->addStretch();
    layout->addLayout(headerRow);

    QFont vf;
    vf.setPointSize(24);
    vf.setBold(true);
    valueLbl->setFont(vf);
    layout->addWidget(valueLbl);
    layout->addStretch();

    widgets.title = titleLbl;
    widgets.value = valueLbl;
    widgets.icon = iconLbl;
    widgets.accent = color;
    widgets.iconName = iconName;

    return widgets;
}

// === Chart ===
void DashboardPage::setupChart()
{
    auto* series = new QLineSeries();
    series->setColor(QColor("#3B82F6"));

    for (int i = 0; i < 24; ++i)
        series->append(qreal(i), 0.0);

    auto* chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(tr("Detections by Hour"));
    chart->setTheme(QChart::ChartThemeDark);
    chart->legend()->hide();
    chart->setBackgroundBrush(QColor("#1A1A1A"));

    auto* axisX = new QValueAxis();
    axisX->setTitleText(tr("Hour"));
    axisX->setRange(0, 23);
    axisX->setTickCount(13);

    auto* axisY = new QValueAxis();
    axisY->setTitleText(tr("Detections"));
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
    const QStringList headers = { tr("Time"), tr("Event"), tr("Camera") };
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

    if (activity.isEmpty()) {
        activity = QList<int>(24, 0);
    }

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
    if (!chartView || !chartView->chart()) return;

    auto* chart = chartView->chart();
    if (chart->series().isEmpty()) return;

    auto* series = qobject_cast<QLineSeries*>(chart->series().first());
    if (!series) return;

    series->clear();
    int maxY = 0;
    for (int i = 0; i < values.size(); ++i) {
        series->append(qreal(i), qreal(values[i]));
        maxY = std::max(maxY, values[i]);
    }

    const auto axes = chart->axes(Qt::Vertical);
    if (!axes.isEmpty()) {
        if (auto* axisY = qobject_cast<QValueAxis*>(axes.first())) {
            axisY->setRange(0, std::max(10, maxY + 2));
        }
    }
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
        for (int c = 0; c < row.size() && c < tableEvents->columnCount(); ++c)
            tableEvents->setItem(r, c, new QTableWidgetItem(row[c]));
        ++r;
    }
}

void DashboardPage::applyTheme(Theme theme)
{
    currentTheme = theme;
    const bool dark = (theme == Theme::Dark);
    const QString background = dark ? "#121212" : "#f9fafb";
    const QString textColor = dark ? "#e5e7eb" : "#111827";
    const QString headerBg = dark ? "#1f1f1f" : "#e5e7eb";
    const QString headerText = dark ? "#cbd5f5" : "#1f2937";
    const QString tableBg = dark ? "#181818" : "#ffffff";
    const QString tableAlt = dark ? "#1c1c1c" : "#f1f5f9";
    const QString gridColor = dark ? "#333333" : "#d1d5db";
    const QString selectionBg = dark ? "#2563EB" : "#bfdbfe";
    const QString selectionText = dark ? "#ffffff" : "#0f172a";

    const QString stylesheet = QString(R"(
        QWidget#dashboardPageRoot {
            background-color:%1;
            color:%2;
        }
        QWidget#dashboardPageRoot QHeaderView::section {
            background:%3;
            color:%4;
            border:none;
            padding:6px;
        }
        QWidget#dashboardPageRoot QTableWidget {
            background:%5;
            alternate-background-color:%6;
            gridline-color:%7;
            selection-background-color:%8;
            selection-color:%9;
        }
    )").arg(background, textColor, headerBg, headerText, tableBg, tableAlt, gridColor, selectionBg, selectionText);
    setStyleSheet(stylesheet);

    if (headerLabel)
        headerLabel->setStyleSheet(QStringLiteral("color:%1;").arg(textColor));

    refreshCardStyles();
    refreshIcons();
    refreshChartTheme();
}

void DashboardPage::refreshIcons()
{
    const auto applyPixmap = [&](QLabel* label, const QString& name, const QSize& size) {
        if (!label)
            return;
        const QPixmap pix(themedIconPath(name));
        if (!pix.isNull())
            label->setPixmap(pix.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    };

    applyPixmap(headerIcon, QStringLiteral("appicon"), QSize(32, 32));
    for (auto& card : statCards)
        applyPixmap(card.icon, card.iconName, QSize(28, 28));
}

void DashboardPage::refreshCardStyles()
{
    const bool dark = (currentTheme == Theme::Dark);
    const QString cardBg = dark ? "#1E1E1E" : "#ffffff";
    const QString cardBorder = dark ? "#2a2a2a" : "#e5e7eb";
    const QString titleColor = dark ? "#9ca3af" : "#6b7280";

    for (auto& card : statCards) {
        if (!card.frame)
            continue;
        card.frame->setStyleSheet(QStringLiteral(
            "QFrame { background-color:%1; border-radius:12px; border:1px solid %2; }")
            .arg(cardBg, cardBorder));
        if (card.title)
            card.title->setStyleSheet(QStringLiteral("color:%1;").arg(titleColor));
        if (card.value)
            card.value->setStyleSheet(QStringLiteral("color:%1;").arg(card.accent.name()));
    }
}

void DashboardPage::refreshChartTheme()
{
    if (!chartView || !chartView->chart())
        return;
    QChart* chart = chartView->chart();
    const bool dark = (currentTheme == Theme::Dark);
    chart->setTheme(dark ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    chart->setBackgroundBrush(QColor(dark ? "#1A1A1A" : "#ffffff"));
    const QColor axisLabel = QColor(dark ? "#e5e7eb" : "#111827");
    const QColor axisLine = QColor(dark ? "#374151" : "#94a3b8");
    const QColor grid = QColor(dark ? "#374151" : "#d1d5db");
    for (QAbstractAxis* axis : chart->axes()) {
        if (auto* valueAxis = qobject_cast<QValueAxis*>(axis)) {
            valueAxis->setLabelsColor(axisLabel);
            valueAxis->setTitleBrush(QBrush(axisLabel));
            valueAxis->setLinePenColor(axisLine);
            valueAxis->setGridLineColor(grid);
        }
    }

    if (!chart->series().isEmpty()) {
        if (auto* line = qobject_cast<QLineSeries*>(chart->series().first()))
            line->setColor(QColor(dark ? "#3B82F6" : "#2563EB"));
    }
}

QString DashboardPage::themedIconPath(const QString& base) const
{
    const bool useDarkGlyph = (currentTheme == Theme::Light);
    const QString suffix = useDarkGlyph ? QStringLiteral("_dark.png") : QStringLiteral("_light.png");
    const QString themedPath = QStringLiteral(":/resources/icons/") + base + suffix;
    if (QFileInfo::exists(themedPath))
        return themedPath;
    const QString fallback = QStringLiteral(":/resources/icons/") + base + QStringLiteral(".png");
    return fallback;
}
