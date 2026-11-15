#include "ha_sensor_card.hpp"
#include "../icon_helper.hpp"

#include <QHBoxLayout>

namespace dashboard {

HaSensorCard::HaSensorCard(const ThemeConfig &theme,
                           const QJsonObject &params,
                           QWidget *parent)
    : CardWidget(theme, params, parent) {
    entity_ = params.value(QStringLiteral("entity")).toString();
    prefix_ = params.value(QStringLiteral("prefix")).toString();
    unit_ = params.value(QStringLiteral("unit")).toString();
    iconName_ = params.value(QStringLiteral("icon")).toString(QStringLiteral("home"));
    precision_ = params.value(QStringLiteral("precision")).toInt(1);
    iconSize_ = params.value(QStringLiteral("icon_size")).toInt(12);
    minUpdateMs_ = qMax(0, params.value(QStringLiteral("min_update_ms")).toInt(10000));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    row_ = IconHelper::createIconTextRow(this,
                                         makeFont(params.value(QStringLiteral("font_size")).toInt(theme.fontSize), params.value(QStringLiteral("bold")).toBool(false)),
                                         theme.foreground,
                                         params.value(QStringLiteral("spacing")).toInt(3));
    layout->addWidget(row_.container, 1);

    refresh({});
}

void HaSensorCard::refresh(const QVariantMap &snapshot) {
    if (throttleTimer_.isValid() && minUpdateMs_ > 0 && throttleTimer_.elapsed() < minUpdateMs_) {
        return;
    }

    const QString key = QStringLiteral("ha.") + entity_;
    QString valueText = QStringLiteral("--");

    if (!entity_.isEmpty() && snapshot.contains(key)) {
        const QString raw = snapshot.value(key).toString().trimmed();
        if (!raw.isEmpty() && raw != QLatin1String("unavailable") && raw != QLatin1String("unknown")) {
            bool ok = false;
            const double num = raw.toDouble(&ok);
            const QString u = unit_.isEmpty()
                                  ? snapshot.value(key + QStringLiteral(".unit_of_measurement")).toString()
                                  : unit_;
            if (ok) {
                valueText = QStringLiteral("%1%2").arg(num, 0, 'f', precision_).arg(u);
            } else {
                valueText = raw + u;
            }
        }
    }

    const QString fullText = prefix_.isEmpty() ? valueText : (prefix_ + QLatin1Char(' ') + valueText);
    if (fullText != lastText_) {
        lastText_ = fullText;
        IconHelper::updateIconTextRow(row_, iconName_, iconSize_, theme_.accent, fullText);
    }

    if (snapshot.value(QStringLiteral("ha._connected"), false).toBool()) {
        throttleTimer_.restart();
    }
}

}  // namespace dashboard
