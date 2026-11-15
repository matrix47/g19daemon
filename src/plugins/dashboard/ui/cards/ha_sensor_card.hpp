#pragma once

#include "card_widget.hpp"
#include "../icon_helper.hpp"
#include <QElapsedTimer>

namespace dashboard {

class HaSensorCard : public CardWidget {
    Q_OBJECT
public:
    HaSensorCard(const ThemeConfig &theme,
                 const QJsonObject &params,
                 QWidget *parent = nullptr);

    void refresh(const QVariantMap &snapshot) override;

private:
    QString entity_;
    QString prefix_;
    QString unit_;
    QString iconName_;
    int precision_ = 1;
    int iconSize_ = 12;
    int minUpdateMs_ = 10000;

    IconTextRow row_;
    QElapsedTimer throttleTimer_;
    QString lastText_;
};

}  // namespace dashboard
