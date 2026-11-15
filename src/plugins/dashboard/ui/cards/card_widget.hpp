#pragma once

#include "../../config/layout_schema.hpp"
#include "../icon_helper.hpp"
#include <QJsonObject>
#include <QVariantMap>
#include <QWidget>

namespace dashboard {

class CardWidget : public QWidget {
    Q_OBJECT
public:
    explicit CardWidget(const ThemeConfig &theme,
                        const QJsonObject &params,
                        QWidget *parent = nullptr)
        : QWidget(parent), theme_(theme), params_(params) {
        setAttribute(Qt::WA_TranslucentBackground);
    }

    ~CardWidget() override = default;

    virtual void refresh(const QVariantMap &snapshot) = 0;

protected:
    QString fontHintingSpec() const {
        const bool hasKey = params_.contains(QStringLiteral("font_hinting")) || params_.contains(QStringLiteral("hinting"));
        if (!hasKey) {
            return theme_.fontHinting;
        }
        const QJsonValue hv = params_.contains(QStringLiteral("font_hinting"))
                                  ? params_.value(QStringLiteral("font_hinting"))
                                  : params_.value(QStringLiteral("hinting"));
        if (hv.isBool()) {
            return hv.toBool() ? QStringLiteral("full") : QStringLiteral("none");
        }
        return hv.toString(theme_.fontHinting);
    }

    QFont makeFont(int pixelSize, bool bold = false) const {
        return IconHelper::makeFont(theme_.fontFamily, pixelSize, bold, fontHintingSpec());
    }

    ThemeConfig theme_;
    QJsonObject params_;
};

}  // namespace dashboard
