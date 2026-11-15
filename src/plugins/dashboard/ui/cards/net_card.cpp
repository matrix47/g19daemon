#include "net_card.hpp"
#include "../icon_helper.hpp"

#include <QPainter>
#include <QPaintEvent>

namespace dashboard {

static QString formatRate(double bps) {
    if (bps >= 1024.0 * 1024.0 * 1024.0) {
        return QStringLiteral("%1G").arg(bps / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    }
    if (bps >= 1024.0 * 1024.0) {
        return QStringLiteral("%1M").arg(bps / (1024.0 * 1024.0), 0, 'f', 1);
    }
    if (bps >= 1024.0) {
        return QStringLiteral("%1K").arg(bps / 1024.0, 0, 'f', 0);
    }
    return QStringLiteral("%1B").arg(qRound(bps));
}

NetCard::NetCard(const ThemeConfig &theme,
                 const QJsonObject &params,
                 QWidget *parent)
    : CardWidget(theme, params, parent) {
    iface_ = params.value(QStringLiteral("iface")).toString(QStringLiteral("eth0"));
    label_ = params.value(QStringLiteral("label")).toString(iface_);
    iconName_ = params.value(QStringLiteral("icon")).toString(QStringLiteral("lan"));
    fontSize_ = qMax(6, params.value(QStringLiteral("font_size")).toInt(theme.fontSize));
    const double maxMbps = params.value(QStringLiteral("max_mbps")).toDouble(0.0);
    if (maxMbps > 0.0) {
        maxBps_ = (maxMbps * 1000.0 * 1000.0) / 8.0;
    }
    if (!iconName_.isEmpty()) {
        iconPixmap_ = IconHelper::renderSvg(iconName_, 12, theme_.accent);
    }
}

void NetCard::refresh(const QVariantMap &snapshot) {
    const QString prefix = QStringLiteral("net.") + iface_ + QLatin1Char('.');
    isUp_ = snapshot.value(prefix + QStringLiteral("up"), false).toBool();
    rxBps_ = snapshot.value(prefix + QStringLiteral("rx_bps"), 0.0).toDouble();
    txBps_ = snapshot.value(prefix + QStringLiteral("tx_bps"), 0.0).toDouble();

    if (maxBps_ <= 0.0) {
        const double curMax = qMax(rxBps_, txBps_);
        autoPeakBps_ = qMax(128.0 * 1024.0, qMax(curMax, autoPeakBps_ * 0.92));
    }
    update();
}

static void drawSegmentedRow(QPainter &p,
                             const QRect &rowRect,
                             const QString &prefixGlyph,
                             const QString &rateText,
                             double ratio,
                             const QColor &barColor,
                             const QColor &textColor,
                             const QFont &font) {
    p.setFont(font);
    const int prefixW = qMax(10, p.fontMetrics().horizontalAdvance(prefixGlyph) + 2);
    const int rateW = qMax(40, p.fontMetrics().horizontalAdvance(rateText) + 2);
    const int barX = rowRect.left() + prefixW;
    const int barW = qMax(10, rowRect.width() - prefixW - rateW - 2);
    const int barH = 6;
    const int barY = rowRect.top() + (rowRect.height() - barH) / 2;

    p.setPen(barColor);
    p.drawText(QRect(rowRect.left(), rowRect.top(), prefixW, rowRect.height()),
               Qt::AlignLeft | Qt::AlignVCenter, prefixGlyph);

    const int segW = 2;
    const int segStep = 3;
    const int segCount = qMax(1, barW / segStep);
    const int litCount = qBound(0, qRound(ratio * segCount), segCount);
    const QColor track(30, 41, 54);
    for (int i = 0; i < segCount; ++i) {
        p.fillRect(barX + i * segStep, barY, segW, barH, i < litCount ? barColor : track);
    }

    p.setPen(textColor);
    p.drawText(QRect(rowRect.right() - rateW + 1, rowRect.top(), rateW, rowRect.height()),
               Qt::AlignRight | Qt::AlignVCenter, rateText);
}

void NetCard::paintEvent(QPaintEvent *) {
    QPainter p(this);
    const QRect r = rect();
    if (r.width() < 40 || r.height() < 24) {
        return;
    }

    const int headerH = qMax(14, fontSize_ + 2);
    int x = r.left();
    if (!iconPixmap_.isNull()) {
        p.drawPixmap(x, r.top() + (headerH - iconPixmap_.height()) / 2, iconPixmap_);
        x += iconPixmap_.width() + 4;
    }

    p.setFont(makeFont(fontSize_, true));
    p.setPen(theme_.foreground);
    p.drawText(QRect(x, r.top(), r.width() - (x - r.left()) - 12, headerH),
               Qt::AlignLeft | Qt::AlignVCenter, label_);

    // Up/Down status dot in top-right
    const QColor stateColor = isUp_ ? QColor(0x34, 0xc7, 0x59) : QColor(0xff, 0x3b, 0x30);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(stateColor);
    p.drawEllipse(QPointF(r.right() - 4, r.top() + headerH / 2.0), 3.0, 3.0);
    p.setRenderHint(QPainter::Antialiasing, false);

    const double scaleMax = (maxBps_ > 0.0) ? maxBps_ : autoPeakBps_;
    const double rxRatio = qBound(0.0, rxBps_ / scaleMax, 1.0);
    const double txRatio = qBound(0.0, txBps_ / scaleMax, 1.0);

    const int bodyY = r.top() + headerH + 2;
    const int rowH = qMax(10, (r.bottom() - bodyY + 1) / 2);
    const QFont rateFont = makeFont(fontSize_, false);

    drawSegmentedRow(p, QRect(r.left(), bodyY, r.width(), rowH),
                     QStringLiteral("↓"), formatRate(rxBps_), rxRatio,
                     QColor(0x2e, 0xd5, 0x73), theme_.foreground, rateFont);

    drawSegmentedRow(p, QRect(r.left(), bodyY + rowH, r.width(), r.bottom() - (bodyY + rowH) + 1),
                     QStringLiteral("↑"), formatRate(txBps_), txRatio,
                     QColor(0x00, 0xa5, 0xdb), theme_.foreground, rateFont);
}

}  // namespace dashboard
