#pragma once

#include "card_widget.hpp"
#include <QColor>
#include <QPixmap>
#include <QVector>

namespace dashboard {

class BtopBarCard : public CardWidget {
    Q_OBJECT
public:
    BtopBarCard(const ThemeConfig &theme,
                const QJsonObject &params,
                QWidget *parent = nullptr);

    void refresh(const QVariantMap &snapshot) override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString source_;
    QString detailSource_;
    QString label_;
    QString iconName_;
    QString unit_;
    double maxValue_ = 100.0;
    bool showGraph_ = true;
    bool showBar_ = true;
    int fontSize_ = 12;
    QColor barColor_;  // invalid => btop thermal gradient

    QPixmap iconPixmap_;
    double currentPct_ = 0.0;
    QString currentDetail_;
    QVector<double> history_;
};

}  // namespace dashboard
