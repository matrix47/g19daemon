#pragma once

#include "card_widget.hpp"
#include <QPixmap>

namespace dashboard {

class NetCard : public CardWidget {
    Q_OBJECT
public:
    NetCard(const ThemeConfig &theme,
            const QJsonObject &params,
            QWidget *parent = nullptr);

    void refresh(const QVariantMap &snapshot) override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString iface_;
    QString label_;
    QString iconName_;
    double maxBps_ = 0.0;  // 0 => auto-scale with smooth decay
    int fontSize_ = 12;

    QPixmap iconPixmap_;
    bool isUp_ = false;
    double rxBps_ = 0.0;
    double txBps_ = 0.0;
    double autoPeakBps_ = 1024.0 * 1024.0;  // 1 MB/s floor
};

}  // namespace dashboard
