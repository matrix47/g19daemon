#pragma once

#include "card_widget.hpp"
#include <QLabel>

namespace dashboard {

class ClockCard : public CardWidget {
    Q_OBJECT
public:
    ClockCard(const ThemeConfig &theme,
              const QJsonObject &params,
              QWidget *parent = nullptr);

    void refresh(const QVariantMap &snapshot) override;

private:
    QString timeFormat_;
    QString dateFormat_;
    bool showIcon_ = true;
    QString iconName_;
    int iconSize_ = 16;

    QLabel *iconLabel_ = nullptr;
    QLabel *timeLabel_ = nullptr;
    QLabel *dateLabel_ = nullptr;
};

}  // namespace dashboard
