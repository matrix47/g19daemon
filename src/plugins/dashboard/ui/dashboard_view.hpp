#pragma once

#include "../config/layout_schema.hpp"
#include "cards/card_widget.hpp"
#include <QImage>
#include <QPainter>
#include <QSize>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

namespace dashboard {

class DashboardView {
public:
    explicit DashboardView(const QSize &size = QSize(320, 240));
    ~DashboardView();

    void setConfig(const DashboardConfig &config);
    const DashboardConfig &config() const { return config_; }

    int pageCount() const { return config_.pages.size(); }
    int currentPage() const { return currentPage_; }
    void setPage(int index);
    void nextPage();
    void prevPage();

    void refresh(const QVariantMap &snapshot);
    void render(QPainter *painter, const QVariantMap &snapshot);
    QImage renderToImage(const QVariantMap &snapshot);

    QWidget *rootWidget() const { return root_; }

private:
    void rebuildPage();
    CardWidget *createCard(const ColumnConfig &col, QWidget *parent);

    QSize size_;
    DashboardConfig config_;
    int currentPage_ = 0;

    QWidget *root_ = nullptr;
    QVector<CardWidget *> cards_;
};

}  // namespace dashboard
