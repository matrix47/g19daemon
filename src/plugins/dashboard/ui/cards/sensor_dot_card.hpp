#pragma once

#include "card_widget.hpp"
#include <QColor>

namespace dashboard {

class SensorDotCard : public CardWidget {
    Q_OBJECT
public:
    SensorDotCard(const ThemeConfig &theme,
                  const QJsonObject &params,
                  QWidget *parent = nullptr);

    void refresh(const QVariantMap &snapshot) override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString entity_;
    QString source_;
    QString label_;
    QString overflowMode_;  // "clip" (default), "ellipsis", "scroll"
    QString defaultState_;
    QColor onColor_{0x34, 0xc7, 0x59};
    QColor offColor_{0xff, 0x3b, 0x30};
    QColor unknownColor_{0xff, 0x9f, 0x0a};
    int dotSize_ = 6;
    int fontSize_ = 12;

    QString currentState_;
    int scrollOffset_ = 0;
    int scrollDir_ = 1;
};

}  // namespace dashboard
