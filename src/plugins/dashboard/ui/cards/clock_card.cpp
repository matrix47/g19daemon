#include "clock_card.hpp"
#include "../icon_helper.hpp"

#include <QDateTime>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace dashboard {

ClockCard::ClockCard(const ThemeConfig &theme,
                     const QJsonObject &params,
                     QWidget *parent)
    : CardWidget(theme, params, parent) {
    timeFormat_ = params.contains(QStringLiteral("time_format"))
                      ? params.value(QStringLiteral("time_format")).toString()
                      : QStringLiteral("HH:mm:ss");
    dateFormat_ = params.contains(QStringLiteral("date_format"))
                      ? params.value(QStringLiteral("date_format")).toString()
                      : QStringLiteral("ddd dd MMM");
    showIcon_ = params.value(QStringLiteral("show_icon")).toBool(true);
    iconName_ = params.value(QStringLiteral("icon")).toString(QStringLiteral("clock"));
    iconSize_ = params.value(QStringLiteral("icon_size")).toInt(14);

    const int timeSize = params.value(QStringLiteral("time_size")).toInt(qMax(16, theme.fontSize));
    const int dateSize = params.value(QStringLiteral("date_size")).toInt(theme.fontSize);
    const QColor timeColor(params.value(QStringLiteral("color")).toString(theme.foreground.name()));
    const QColor dateColor(params.value(QStringLiteral("date_color")).toString(theme.muted.name()));
    const Qt::Alignment textAlign = params.contains(QStringLiteral("text_alignment"))
                                        ? parseAlignment(params.value(QStringLiteral("text_alignment")).toString())
                                        : (Qt::AlignLeft | Qt::AlignVCenter);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    if (showIcon_ && !iconName_.isEmpty()) {
        iconLabel_ = new QLabel(this);
        iconLabel_->setPixmap(IconHelper::renderSvg(iconName_, iconSize_, theme.accent));
        iconLabel_->setFixedSize(iconSize_, iconSize_);
        root->addWidget(iconLabel_, 0, Qt::AlignVCenter);
    }

    auto *textCol = new QVBoxLayout();
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(0);

    if (!timeFormat_.isEmpty()) {
        timeLabel_ = new QLabel(this);
        timeLabel_->setFont(makeFont(timeSize, true));
        timeLabel_->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(timeColor.name()));
        timeLabel_->setAlignment(textAlign);
        textCol->addWidget(timeLabel_);
    }

    if (!dateFormat_.isEmpty()) {
        dateLabel_ = new QLabel(this);
        dateLabel_->setFont(makeFont(dateSize, false));
        dateLabel_->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(dateColor.name()));
        dateLabel_->setAlignment(textAlign);
        textCol->addWidget(dateLabel_);
    }

    root->addLayout(textCol, 1);
    refresh({});
}

void ClockCard::refresh(const QVariantMap &) {
    const QDateTime now = QDateTime::currentDateTime();
    if (timeLabel_) {
        timeLabel_->setText(now.toString(timeFormat_));
    }
    if (dateLabel_) {
        dateLabel_->setText(now.toString(dateFormat_));
    }
}

}  // namespace dashboard
