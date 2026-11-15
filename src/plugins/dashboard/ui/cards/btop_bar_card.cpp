#include "btop_bar_card.hpp"
#include "../icon_helper.hpp"

#include <QPainter>
#include <QPaintEvent>

namespace dashboard {

static QColor lerpColor(const QColor &a, const QColor &b, double t) {
    t = qBound(0.0, t, 1.0);
    return QColor(
        qRound(a.red() + (b.red() - a.red()) * t),
        qRound(a.green() + (b.green() - a.green()) * t),
        qRound(a.blue() + (b.blue() - a.blue()) * t));
}

static QColor btopThermalColor(double pct) {
    const QColor green(0x34, 0xc7, 0x59);
    const QColor yellow(0xf5, 0xc4, 0x51);
    const QColor orange(0xff, 0x95, 0x00);
    const QColor red(0xff, 0x3b, 0x30);
    pct = qBound(0.0, pct, 100.0);
    if (pct < 50.0) {
        return lerpColor(green, yellow, pct / 50.0);
    }
    if (pct < 80.0) {
        return lerpColor(yellow, orange, (pct - 50.0) / 30.0);
    }
    return lerpColor(orange, red, (pct - 80.0) / 20.0);
}

BtopBarCard::BtopBarCard(const ThemeConfig &theme,
                         const QJsonObject &params,
                         QWidget *parent)
    : CardWidget(theme, params, parent) {
    source_ = params.value(QStringLiteral("source")).toString();
    detailSource_ = params.value(QStringLiteral("detail_source")).toString();
    label_ = params.value(QStringLiteral("label")).toString(source_);
    iconName_ = params.value(QStringLiteral("icon")).toString();
    unit_ = params.value(QStringLiteral("unit")).toString(QStringLiteral("%"));
    maxValue_ = qMax(0.001, params.value(QStringLiteral("max_value")).toDouble(100.0));
    showGraph_ = params.value(QStringLiteral("show_graph")).toBool(true);
    showBar_ = params.value(QStringLiteral("show_bar")).toBool(true);
    fontSize_ = qMax(6, params.value(QStringLiteral("font_size")).toInt(theme.fontSize));

    if (params.contains(QStringLiteral("color"))) {
        barColor_ = QColor(params.value(QStringLiteral("color")).toString());
    }

    if (!iconName_.isEmpty()) {
        iconPixmap_ = IconHelper::renderSvg(iconName_, 12, theme_.accent);
    }
}

void BtopBarCard::refresh(const QVariantMap &snapshot) {
    const double raw = snapshot.value(source_, 0.0).toDouble();
    currentPct_ = qBound(0.0, (raw / maxValue_) * 100.0, 100.0);

    if (!detailSource_.isEmpty() && snapshot.contains(detailSource_)) {
        currentDetail_ = snapshot.value(detailSource_).toString();
    } else if (unit_ == QLatin1String("%")) {
        currentDetail_ = QStringLiteral("%1%").arg(qRound(currentPct_));
    } else {
        currentDetail_ = QStringLiteral("%1%2").arg(raw, 0, 'f', 1).arg(unit_);
    }

    history_.append(currentPct_);
    if (history_.size() > 64) {
        history_.removeFirst();
    }
    update();
}

void BtopBarCard::paintEvent(QPaintEvent *) {
    QPainter p(this);
    const QRect r = rect();
    if (r.width() < 16 || r.height() < 14) {
        return;
    }

    const int headerH = qMax(14, fontSize_ + 2);
    const int barH = showBar_ ? 7 : 0;
    const int gap = 2;

    // 1. Header: icon + label on left, detail on right
    int x = r.left();
    if (!iconPixmap_.isNull()) {
        p.drawPixmap(x, r.top() + (headerH - iconPixmap_.height()) / 2, iconPixmap_);
        x += iconPixmap_.width() + 4;
    }

    const QFont labelFont = makeFont(fontSize_, true);
    const QFont detailFont = makeFont(fontSize_, false);

    p.setFont(detailFont);
    const int detailW = p.fontMetrics().horizontalAdvance(currentDetail_);
    p.setPen(btopThermalColor(currentPct_));
    p.drawText(QRect(r.right() - detailW, r.top(), detailW + 1, headerH),
               Qt::AlignRight | Qt::AlignVCenter, currentDetail_);

    p.setFont(labelFont);
    p.setPen(theme_.foreground);
    const int availLabelW = qMax(0, r.right() - detailW - 4 - x);
    const QRect labelRect(x, r.top(), availLabelW, headerH);
    p.save();
    p.setClipRect(labelRect);
    p.drawText(QRect(x, r.top(), availLabelW + 64, headerH), Qt::AlignLeft | Qt::AlignVCenter, label_);
    p.restore();

    // 2. Rolling btop-style sparkline graph
    int yTop = r.top() + headerH + gap;
    const int graphH = r.bottom() - yTop - (showBar_ ? (barH + gap) : 0);
    if (showGraph_ && graphH >= 6) {
        const QRect graphRect(r.left(), yTop, r.width(), graphH);
        p.fillRect(graphRect, QColor(10, 15, 22, 160));

        const int colW = 2;
        const int colStep = 3;
        const int maxCols = qMax(1, graphRect.width() / colStep);
        const int startIdx = qMax(0, int(history_.size()) - maxCols);

        int gx = graphRect.right() - ((int(history_.size()) - startIdx) * colStep) + 1;
        if (gx < graphRect.left()) {
            gx = graphRect.left();
        }
        for (int i = startIdx; i < history_.size(); ++i) {
            const double val = history_[i];
            int h = qRound((val / 100.0) * graphRect.height());
            if (val > 0.5 && h == 0) {
                h = 1;
            }
            if (h > 0) {
                const QColor c = barColor_.isValid() ? barColor_ : btopThermalColor(val);
                p.fillRect(gx, graphRect.bottom() - h + 1, colW, h, c);
            }
            gx += colStep;
        }
        yTop += graphH + gap;
    }

    // 3. Bottom segmented btop bar
    if (showBar_) {
        const int barY = r.bottom() - barH + 1;
        const int segW = 3;
        const int segStep = 4;
        const int segCount = qMax(1, r.width() / segStep);
        const int litCount = qRound((currentPct_ / 100.0) * segCount);
        const QColor trackColor(30, 41, 54);

        for (int i = 0; i < segCount; ++i) {
            const int sx = r.left() + i * segStep;
            const double segPct = ((i + 1) * 100.0) / segCount;
            const QColor c = (i < litCount)
                                 ? (barColor_.isValid() ? barColor_ : btopThermalColor(segPct))
                                 : trackColor;
            p.fillRect(sx, barY, segW, barH, c);
        }
    }
}

}  // namespace dashboard
