#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMargins>
#include <QString>
#include <QVector>
#include <Qt>

namespace dashboard {

struct SizeSpec {
    enum class Mode {
        Stretch,
        FixedPx,
        Percent
    };

    Mode mode = Mode::Stretch;
    double value = 1.0;

    static SizeSpec parse(const QJsonValue &val);
    static SizeSpec parse(const QJsonValue &val, const SizeSpec &fallback);
};

struct ThemeConfig {
    QColor background = QColor("#0a0f16");
    QColor foreground = QColor("#e6edf3");
    QColor muted = QColor("#7d8fa3");
    QColor accent = QColor("#00a5db");
    QString fontFamily = QStringLiteral("Terminus (TTF)");
    int fontSize = 12;
    QString fontHinting = QStringLiteral("none");  // "none", "default", "vertical", "full"
    int outerMargin = 4;
    int rowGap = 4;
    QString defaultNinePatch = QStringLiteral(":/dashboard/frames/card.9.png");
};

struct HomeAssistantConfig {
    QString url;
    bool tlsVerify = true;
    QString token;
    QString tokenEnv = QStringLiteral("HA_TOKEN");
    QString tokenFile = QStringLiteral("~/.config/G19Daemon/ha_token");
};

struct ColumnConfig {
    SizeSpec width{SizeSpec::Mode::Stretch, 1.0};
    SizeSpec cardWidth{SizeSpec::Mode::Stretch, 0.0};   // 0 => fill cell
    SizeSpec cardHeight{SizeSpec::Mode::Stretch, 0.0};  // 0 => fill cell
    Qt::Alignment alignment = Qt::AlignCenter;
    bool hasAlignment = false;
    QString ninePatch;      // empty => inherit theme.defaultNinePatch, "none" => no frame
    QMargins padding{0, 0, 0, 0};
    QColor background;      // invalid => none / ninepatch handles it
    QString cardType;
    QJsonObject cardParams;
};

struct RowConfig {
    SizeSpec height{SizeSpec::Mode::Stretch, 1.0};
    int gap = 4;
    QVector<ColumnConfig> columns;
};

struct PageConfig {
    QString name = QStringLiteral("Main");
    QVector<RowConfig> rows;
};

struct DashboardConfig {
    QString pluginId = QStringLiteral("dashboard");
    QString pluginName = QStringLiteral("Dashboard");
    int refreshMs = 1000;
    ThemeConfig theme;
    HomeAssistantConfig homeAssistant;
    QVector<PageConfig> pages;

    static bool loadFromFile(const QString &path, DashboardConfig &out, QString *error = nullptr);
    static bool loadFromJson(const QByteArray &jsonBytes, DashboardConfig &out, QString *error = nullptr);
};

Qt::Alignment parseAlignment(const QString &text);
QMargins parseMargins(const QJsonValue &val);

}  // namespace dashboard
