#pragma once

#include "card_widget.hpp"
#include "../icon_helper.hpp"
#include <QElapsedTimer>

namespace dashboard {

class WeatherCard : public CardWidget {
    Q_OBJECT
public:
    WeatherCard(const ThemeConfig &theme,
                const QJsonObject &params,
                QWidget *parent = nullptr);

    void refresh(const QVariantMap &snapshot) override;

private:
    QString entity_;
    QString conditionEntity_;
    QString unit_;
    int precision_ = 1;
    int iconSize_ = 14;
    int minUpdateMs_ = 10000;

    IconTextRow row_;
    QElapsedTimer throttleTimer_;
    QString lastText_;
    QString lastIcon_;
    QColor lastTint_;
};

}  // namespace dashboard
