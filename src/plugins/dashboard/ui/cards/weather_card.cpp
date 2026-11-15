#include "weather_card.hpp"
#include "../icon_helper.hpp"

#include <QHBoxLayout>

namespace dashboard {

struct WeatherVisual {
    QString icon;
    QColor tint;
};

static WeatherVisual weatherConditionVisual(const QString &cond, const QColor &fallbackTint) {
    const QString c = cond.trimmed().toLower();
    if (c == QLatin1String("sunny")) {
        return {QStringLiteral("weather_sunny"), QColor(0xf5, 0xc4, 0x51)};
    }
    if (c == QLatin1String("clear-night")) {
        return {QStringLiteral("weather_night"), QColor(0xa8, 0xc7, 0xfa)};
    }
    if (c == QLatin1String("partlycloudy")) {
        return {QStringLiteral("weather_partlycloudy"), QColor(0xf5, 0xc4, 0x51)};
    }
    if (c == QLatin1String("cloudy")) {
        return {QStringLiteral("weather_cloudy"), QColor(0x9f, 0xb3, 0xc8)};
    }
    if (c == QLatin1String("rainy") || c == QLatin1String("pouring")) {
        return {QStringLiteral("weather_rainy"), QColor(0x00, 0xa5, 0xdb)};
    }
    if (c.contains(QLatin1String("lightning"))) {
        return {QStringLiteral("weather_lightning"), QColor(0xff, 0xd1, 0x66)};
    }
    if (c.contains(QLatin1String("snow")) || c == QLatin1String("hail")) {
        return {QStringLiteral("weather_snowy"), QColor(0xdc, 0xee, 0xff)};
    }
    if (c == QLatin1String("fog")) {
        return {QStringLiteral("weather_fog"), QColor(0x9f, 0xb3, 0xc8)};
    }
    if (c.contains(QLatin1String("wind"))) {
        return {QStringLiteral("weather_windy"), QColor(0x64, 0xd2, 0xff)};
    }
    return {QStringLiteral("weather"), fallbackTint};
}

WeatherCard::WeatherCard(const ThemeConfig &theme,
                         const QJsonObject &params,
                         QWidget *parent)
    : CardWidget(theme, params, parent) {
    entity_ = params.value(QStringLiteral("entity")).toString(QStringLiteral("weather.home"));
    conditionEntity_ = params.value(QStringLiteral("condition_entity")).toString();
    unit_ = params.value(QStringLiteral("unit")).toString(QStringLiteral("°C"));
    precision_ = params.value(QStringLiteral("precision")).toInt(1);
    const int fontSize = params.value(QStringLiteral("font_size")).toInt(theme.fontSize);
    const bool bold = params.value(QStringLiteral("bold")).toBool(false);
    iconSize_ = params.value(QStringLiteral("icon_size")).toInt(fontSize);
    minUpdateMs_ = qMax(0, params.value(QStringLiteral("min_update_ms")).toInt(10000));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    row_ = IconHelper::createIconTextRow(this,
                                         makeFont(fontSize, bold),
                                         theme.foreground,
                                         params.value(QStringLiteral("spacing")).toInt(3));
    layout->addWidget(row_.container, 1);

    refresh({});
}

void WeatherCard::refresh(const QVariantMap &snapshot) {
    if (throttleTimer_.isValid() && minUpdateMs_ > 0 && throttleTimer_.elapsed() < minUpdateMs_) {
        return;
    }

    QString text = QStringLiteral("--") + unit_;
    QString condition;

    const QString tempAttrKey = QStringLiteral("ha.") + entity_ + QStringLiteral(".temperature");
    const QString stateKey = QStringLiteral("ha.") + entity_;

    if (snapshot.contains(tempAttrKey)) {
        bool ok = false;
        double temp = snapshot.value(tempAttrKey).toDouble(&ok);
        if (ok) {
            const QString srcUnit = snapshot.value(stateKey + QStringLiteral(".temperature_unit")).toString();
            if (srcUnit == QLatin1String("°F") && unit_ == QLatin1String("°C")) {
                temp = (temp - 32.0) * 5.0 / 9.0;
            }
            text = QStringLiteral("%1%2").arg(temp, 0, 'f', precision_).arg(unit_);
        }
        condition = snapshot.value(stateKey).toString();
    } else if (snapshot.contains(stateKey)) {
        const QString raw = snapshot.value(stateKey).toString().trimmed();
        bool ok = false;
        double val = raw.toDouble(&ok);
        if (ok) {
            const QString srcUnit = snapshot.value(stateKey + QStringLiteral(".unit_of_measurement")).toString();
            if (srcUnit == QLatin1String("°F") && unit_ == QLatin1String("°C")) {
                val = (val - 32.0) * 5.0 / 9.0;
            }
            text = QStringLiteral("%1%2").arg(val, 0, 'f', precision_).arg(unit_);
        }
    }

    if (!conditionEntity_.isEmpty()) {
        condition = snapshot.value(QStringLiteral("ha.") + conditionEntity_).toString();
    }

    const WeatherVisual vis = weatherConditionVisual(condition, theme_.accent);
    if (text != lastText_ || vis.icon != lastIcon_ || vis.tint != lastTint_) {
        lastText_ = text;
        lastIcon_ = vis.icon;
        lastTint_ = vis.tint;
        IconHelper::updateIconTextRow(row_, vis.icon, iconSize_, vis.tint, text);
    }

    if (snapshot.value(QStringLiteral("ha._connected"), false).toBool()) {
        throttleTimer_.restart();
    }
}

}  // namespace dashboard
