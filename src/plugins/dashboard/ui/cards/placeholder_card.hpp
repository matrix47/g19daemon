#pragma once

#include "card_widget.hpp"
#include "../icon_helper.hpp"
#include <QLabel>

namespace dashboard {

class PlaceholderCard : public CardWidget {
    Q_OBJECT
public:
    PlaceholderCard(const QString &cardType,
                    const ThemeConfig &theme,
                    const QJsonObject &params,
                    QWidget *parent = nullptr);

    void refresh(const QVariantMap &snapshot) override;

private:
    IconTextRow titleRow_;
    QLabel *subLabel_ = nullptr;
};

}  // namespace dashboard
