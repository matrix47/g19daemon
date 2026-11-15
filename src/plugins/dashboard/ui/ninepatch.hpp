#pragma once

#include <QImage>
#include <QMargins>
#include <QPainter>
#include <QRect>
#include <QString>
#include <QVector>

namespace dashboard {

class NinePatch {
public:
    struct Span {
        int start = 0;
        int length = 0;
        bool stretch = false;
    };

    NinePatch() = default;

    static NinePatch load(const QString &path);

    bool isValid() const { return !image_.isNull(); }
    QMargins contentMargins() const { return contentMargins_; }
    QMargins sliceMargins() const { return sliceMargins_; }

    void paint(QPainter *painter, const QRect &targetRect) const;

private:
    QImage image_;
    QVector<Span> xSpans_;
    QVector<Span> ySpans_;
    QMargins contentMargins_{0, 0, 0, 0};
    QMargins sliceMargins_{0, 0, 0, 0};
};

}  // namespace dashboard
