#include "placeholder_card.hpp"
#include "../icon_helper.hpp"

#include <QJsonArray>
#include <QVBoxLayout>

namespace dashboard {

PlaceholderCard::PlaceholderCard(const QString &cardType,
                                 const ThemeConfig &theme,
                                 const QJsonObject &params,
                                 QWidget *parent)
    : CardWidget(theme, params, parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    QString label = params.value(QStringLiteral("label")).toString();
    QString icon = params.value(QStringLiteral("icon")).toString();
    QString sub = cardType;

    if (label.isEmpty() && params.contains(QStringLiteral("items"))) {
        const QJsonArray items = params.value(QStringLiteral("items")).toArray();
        if (!items.isEmpty()) {
            const QJsonObject first = items[0].toObject();
            label = first.value(QStringLiteral("label")).toString(cardType);
            icon = first.value(QStringLiteral("icon")).toString(QStringLiteral("home"));
            sub = QStringLiteral("%1 (%2 entities)").arg(cardType).arg(items.size());
        }
    } else if (label.isEmpty() && params.contains(QStringLiteral("interfaces"))) {
        const QJsonArray ifaces = params.value(QStringLiteral("interfaces")).toArray();
        QStringList names;
        for (const QJsonValue &v : ifaces) {
            names << v.toObject().value(QStringLiteral("label")).toString();
        }
        label = names.join(QStringLiteral(" / "));
        icon = QStringLiteral("lan");
    }

    if (label.isEmpty()) {
        label = cardType;
    }

    const int fontSize = params.value(QStringLiteral("font_size")).toInt(theme.fontSize);
    const bool bold = params.value(QStringLiteral("bold")).toBool(true);
    const int iconSize = params.value(QStringLiteral("icon_size")).toInt(fontSize);
    const bool showIcon = params.value(QStringLiteral("show_icon")).toBool(true);

    titleRow_ = IconHelper::createIconTextRow(this,
                                              makeFont(fontSize, bold),
                                              theme.foreground,
                                              params.value(QStringLiteral("spacing")).toInt(3));
    IconHelper::updateIconTextRow(titleRow_, showIcon ? icon : QString(), iconSize, theme.accent, label);
    layout->addWidget(titleRow_.container);

    if (params.value(QStringLiteral("show_sub")).toBool(false)) {
        subLabel_ = new QLabel(sub, this);
        subLabel_->setFont(makeFont(qMax(10, theme.fontSize - 2), false));
        subLabel_->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(theme.muted.name()));
        layout->addWidget(subLabel_);
    }
}

void PlaceholderCard::refresh(const QVariantMap &) {}

}  // namespace dashboard
