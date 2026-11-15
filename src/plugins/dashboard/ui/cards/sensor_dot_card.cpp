#include "sensor_dot_card.hpp"
#include "../icon_helper.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>

namespace dashboard {

SensorDotCard::SensorDotCard(const ThemeConfig &theme,
                             const QJsonObject &params,
                             QWidget *parent)
    : CardWidget(theme, params, parent) {
    entity_ = params.value(QStringLiteral("entity")).toString();
    source_ = params.value(QStringLiteral("source")).toString();
    label_ = params.value(QStringLiteral("label")).toString(entity_);
    overflowMode_ = params.value(QStringLiteral("overflow")).toString(QStringLiteral("clip")).toLower();
    defaultState_ = params.value(QStringLiteral("default_state")).toString(QStringLiteral("unknown")).toLower();

    if (params.contains(QStringLiteral("on_color"))) {
        onColor_ = QColor(params.value(QStringLiteral("on_color")).toString());
    }
    if (params.contains(QStringLiteral("off_color"))) {
        offColor_ = QColor(params.value(QStringLiteral("off_color")).toString());
    }
    if (params.contains(QStringLiteral("unknown_color"))) {
        unknownColor_ = QColor(params.value(QStringLiteral("unknown_color")).toString());
    }
    dotSize_ = qBound(4, params.value(QStringLiteral("dot_size")).toInt(5), 12);
    fontSize_ = qBound(6, params.value(QStringLiteral("font_size")).toInt(theme.fontSize), 24);

    currentState_ = defaultState_;
}

void SensorDotCard::refresh(const QVariantMap &snapshot) {
    QString key = source_;
    if (key.isEmpty() && !entity_.isEmpty()) {
        key = QStringLiteral("ha.") + entity_;
    }

    QString nextState = defaultState_;
    if (!key.isEmpty() && snapshot.contains(key)) {
        const QVariant v = snapshot.value(key);
        if (v.typeId() == QMetaType::Bool) {
            nextState = v.toBool() ? QStringLiteral("on") : QStringLiteral("off");
        } else {
            nextState = v.toString().trimmed().toLower();
        }
    }

    bool changed = (nextState != currentState_);
    currentState_ = nextState;

    if (overflowMode_ == QLatin1String("scroll")) {
        const QFont f = makeFont(fontSize_, false);
        const QFontMetrics fm(f);
        const int availW = qMax(1, width() - dotSize_ - 2);
        const int textW = fm.horizontalAdvance(label_);
        if (textW > availW) {
            const int maxOff = textW - availW;
            scrollOffset_ += scrollDir_ * 4;
            if (scrollOffset_ >= maxOff) {
                scrollOffset_ = maxOff;
                scrollDir_ = -1;
            } else if (scrollOffset_ <= 0) {
                scrollOffset_ = 0;
                scrollDir_ = 1;
            }
            changed = true;
        } else if (scrollOffset_ != 0) {
            scrollOffset_ = 0;
            changed = true;
        }
    }
    if (changed) {
        update();
    }
}

void SensorDotCard::paintEvent(QPaintEvent *) {
    QPainter p(this);
    const QRect r = rect();
    if (r.width() < 10 || r.height() < 8) {
        return;
    }

    QColor ledColor = unknownColor_;
    if (currentState_ == QLatin1String("on") || currentState_ == QLatin1String("true") || currentState_ == QLatin1String("1")) {
        ledColor = onColor_;
    } else if (currentState_ == QLatin1String("off") || currentState_ == QLatin1String("false") || currentState_ == QLatin1String("0")) {
        ledColor = offColor_;
    }

    // Glowing LED dot on left
    const double cy = r.top() + r.height() / 2.0;
    const double cx = r.left() + dotSize_ / 2.0;
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor halo = ledColor;
    halo.setAlpha(70);
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(QPointF(cx, cy), dotSize_ / 2.0 + 1.5, dotSize_ / 2.0 + 1.5);
    p.setBrush(ledColor);
    p.drawEllipse(QPointF(cx, cy), dotSize_ / 2.0, dotSize_ / 2.0);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Label text nearby with overflow handling
    const int textX = r.left() + dotSize_ + 2;
    const int textW = qMax(0, r.right() - textX + 1);
    if (textW <= 0) {
        return;
    }

    const QFont f = makeFont(fontSize_, false);
    p.setFont(f);
    p.setPen(theme_.foreground);

    const QRect textRect(textX, r.top(), textW, r.height());
    p.save();
    p.setClipRect(textRect);
    if (overflowMode_ == QLatin1String("scroll")) {
        p.drawText(QRect(textX - scrollOffset_, r.top(), textW + scrollOffset_ + 32, r.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, label_);
    } else if (overflowMode_ == QLatin1String("clip")) {
        p.drawText(QRect(textX, r.top(), textW + 64, r.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, label_);
    } else {
        const QString elided = p.fontMetrics().elidedText(label_, Qt::ElideRight, textW);
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elided);
    }
    p.restore();
}

}  // namespace dashboard
