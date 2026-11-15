#include "dashboard_view.hpp"
#include "cards/btop_bar_card.hpp"
#include "cards/clock_card.hpp"
#include "cards/ha_sensor_card.hpp"
#include "cards/net_card.hpp"
#include "cards/placeholder_card.hpp"
#include "cards/sensor_dot_card.hpp"
#include "cards/weather_card.hpp"
#include "ninepatch.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace dashboard {

class NinePatchFrame : public QFrame {
public:
    NinePatchFrame(const NinePatch &np,
                   const QColor &bgColor,
                   const QMargins &extraPadding,
                   QWidget *parent = nullptr)
        : QFrame(parent), np_(np), bgColor_(bgColor) {
        setAttribute(Qt::WA_TranslucentBackground);
        const QMargins m = np_.contentMargins() + extraPadding;
        setContentsMargins(m);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter p(this);
        if (bgColor_.isValid()) {
            p.fillRect(rect(), bgColor_);
        }
        if (np_.isValid()) {
            np_.paint(&p, rect());
        }
        QFrame::paintEvent(event);
    }

private:
    NinePatch np_;
    QColor bgColor_;
};

static int resolvePixels(const SizeSpec &spec, int availableTotal) {
    switch (spec.mode) {
        case SizeSpec::Mode::FixedPx:
            return qMax(1, qRound(spec.value));
        case SizeSpec::Mode::Percent:
            return qMax(1, qRound((availableTotal * spec.value) / 100.0));
        case SizeSpec::Mode::Stretch:
        default:
            return 0;
    }
}

DashboardView::DashboardView(const QSize &size) : size_(size) {}

DashboardView::~DashboardView() {
    delete root_;
    root_ = nullptr;
}

void DashboardView::setConfig(const DashboardConfig &config) {
    config_ = config;
    if (currentPage_ >= config_.pages.size()) {
        currentPage_ = 0;
    }
    rebuildPage();
}

void DashboardView::setPage(int index) {
    if (config_.pages.isEmpty()) {
        return;
    }
    const int wrapped = ((index % config_.pages.size()) + config_.pages.size()) % config_.pages.size();
    if (wrapped != currentPage_) {
        currentPage_ = wrapped;
        rebuildPage();
    }
}

void DashboardView::nextPage() {
    setPage(currentPage_ + 1);
}

void DashboardView::prevPage() {
    setPage(currentPage_ - 1);
}

CardWidget *DashboardView::createCard(const ColumnConfig &col, QWidget *parent) {
    const QString t = col.cardType.toLower();
    if (t == QLatin1String("clock")) {
        return new ClockCard(config_.theme, col.cardParams, parent);
    }
    if (t == QLatin1String("weather")) {
        return new WeatherCard(config_.theme, col.cardParams, parent);
    }
    if (t == QLatin1String("btop_bar")) {
        return new BtopBarCard(config_.theme, col.cardParams, parent);
    }
    if (t == QLatin1String("net_activity")) {
        return new NetCard(config_.theme, col.cardParams, parent);
    }
    if (t == QLatin1String("ha_binary") || t == QLatin1String("sensor_dot")) {
        return new SensorDotCard(config_.theme, col.cardParams, parent);
    }
    if (t == QLatin1String("ha_sensor")) {
        return new HaSensorCard(config_.theme, col.cardParams, parent);
    }
    if (t == QLatin1String("spacer")) {
        return nullptr;
    }
    return new PlaceholderCard(col.cardType, config_.theme, col.cardParams, parent);
}

void DashboardView::rebuildPage() {
    cards_.clear();
    delete root_;
    root_ = new QWidget();
    root_->setFixedSize(size_);
    root_->setAutoFillBackground(true);

    QPalette pal = root_->palette();
    pal.setColor(QPalette::Window, config_.theme.background);
    root_->setPalette(pal);

    auto *rowsLayout = new QVBoxLayout(root_);
    const int margin = config_.theme.outerMargin;
    const int rowGap = config_.theme.rowGap;
    rowsLayout->setContentsMargins(margin, margin, margin, margin);
    rowsLayout->setSpacing(rowGap);

    if (config_.pages.isEmpty()) {
        return;
    }

    const PageConfig &page = config_.pages[currentPage_];
    const int availH = qMax(1, size_.height() - 2 * margin - qMax(0, int(page.rows.size()) - 1) * rowGap);
    const int availRowW = qMax(1, size_.width() - 2 * margin);
    bool hasStretchRow = false;

    for (const RowConfig &row : page.rows) {
        auto *rowWidget = new QWidget(root_);
        rowWidget->setAttribute(Qt::WA_TranslucentBackground);

        int stretchRow = 0;
        if (row.height.mode == SizeSpec::Mode::Stretch) {
            stretchRow = qMax(1, qRound(row.height.value));
            hasStretchRow = true;
        } else {
            rowWidget->setFixedHeight(resolvePixels(row.height, availH));
        }

        auto *colsLayout = new QHBoxLayout(rowWidget);
        colsLayout->setContentsMargins(0, 0, 0, 0);
        colsLayout->setSpacing(row.gap);

        const int availColW = qMax(1, availRowW - qMax(0, int(row.columns.size()) - 1) * row.gap);

        for (const ColumnConfig &col : row.columns) {
            auto *cellWidget = new QWidget(rowWidget);
            cellWidget->setAttribute(Qt::WA_TranslucentBackground);

            int stretchCol = 0;
            if (col.width.mode == SizeSpec::Mode::Stretch) {
                stretchCol = qMax(1, qRound(col.width.value));
            } else {
                cellWidget->setFixedWidth(resolvePixels(col.width, availColW));
            }

            auto *cellLayout = new QHBoxLayout(cellWidget);
            cellLayout->setContentsMargins(0, 0, 0, 0);
            cellLayout->setSpacing(0);

            QString npPath = col.ninePatch;
            if (npPath.isEmpty()) {
                npPath = config_.theme.defaultNinePatch;
            }
            NinePatch np;
            if (!npPath.isEmpty() && npPath.compare(QLatin1String("none"), Qt::CaseInsensitive) != 0) {
                np = NinePatch::load(npPath);
            }

            auto *frame = new NinePatchFrame(np, col.background, col.padding, cellWidget);
            const bool customCardW = col.cardWidth.value > 0;
            const bool customCardH = col.cardHeight.value > 0;
            if (customCardW) {
                const int cw = resolvePixels(col.cardWidth, availColW);
                frame->setFixedWidth(cw);
            }
            if (customCardH) {
                const int ch = resolvePixels(col.cardHeight, availH);
                frame->setFixedHeight(ch);
            }

            auto *frameLayout = new QVBoxLayout(frame);
            frameLayout->setContentsMargins(0, 0, 0, 0);
            frameLayout->setSpacing(0);

            if (CardWidget *card = createCard(col, frame)) {
                cards_.append(card);
                if (col.hasAlignment) {
                    frameLayout->addWidget(card, 0, col.alignment);
                } else {
                    frameLayout->addWidget(card, 1);
                }
            }

            if ((customCardW || customCardH) && col.hasAlignment) {
                cellLayout->addWidget(frame, 0, col.alignment);
            } else {
                cellLayout->addWidget(frame, 1);
            }

            colsLayout->addWidget(cellWidget, stretchCol);
        }

        rowsLayout->addWidget(rowWidget, stretchRow);
    }

    if (!hasStretchRow) {
        rowsLayout->addStretch(1);
    }

    root_->ensurePolished();
    if (root_->layout()) {
        root_->layout()->activate();
    }
}

void DashboardView::refresh(const QVariantMap &snapshot) {
    for (CardWidget *card : cards_) {
        if (card) {
            card->refresh(snapshot);
        }
    }
}

void DashboardView::render(QPainter *painter, const QVariantMap &snapshot) {
    if (!root_ || !painter) {
        return;
    }
    refresh(snapshot);
    root_->ensurePolished();
    if (root_->layout()) {
        root_->layout()->activate();
    }
    root_->render(painter);
}

QImage DashboardView::renderToImage(const QVariantMap &snapshot) {
    QImage img(size_, QImage::Format_RGB32);
    img.fill(config_.theme.background);
    QPainter p(&img);
    render(&p, snapshot);
    return img;
}

}  // namespace dashboard
