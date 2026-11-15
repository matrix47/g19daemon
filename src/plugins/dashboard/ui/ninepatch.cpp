#include "ninepatch.hpp"

#include <QHash>

namespace dashboard {

static bool isMarkerPixel(QRgb px) {
    return qAlpha(px) > 128 && qRed(px) < 64 && qGreen(px) < 64 && qBlue(px) < 64;
}

static QVector<NinePatch::Span> buildSpans(const QVector<bool> &markers) {
    QVector<NinePatch::Span> spans;
    if (markers.isEmpty()) {
        return spans;
    }

    bool current = markers[0];
    int start = 0;
    for (int i = 1; i < markers.size(); ++i) {
        if (markers[i] != current) {
            spans.append({start, i - start, current});
            current = markers[i];
            start = i;
        }
    }
    spans.append({start, static_cast<int>(markers.size()) - start, current});
    return spans;
}

static void extractContentBounds(const QVector<bool> &markers, int &before, int &after) {
    int first = -1;
    int last = -1;
    for (int i = 0; i < markers.size(); ++i) {
        if (markers[i]) {
            if (first == -1) {
                first = i;
            }
            last = i;
        }
    }
    if (first == -1) {
        before = 0;
        after = 0;
    } else {
        before = first;
        after = (markers.size() - 1) - last;
    }
}

NinePatch NinePatch::load(const QString &path) {
    static QHash<QString, NinePatch> cache;
    if (cache.contains(path)) {
        return cache.value(path);
    }

    NinePatch np;
    QImage raw(path);
    if (raw.isNull()) {
        return np;
    }

    raw = raw.convertToFormat(QImage::Format_ARGB32);

    const bool isNinePatchFile = path.endsWith(QLatin1String(".9.png"), Qt::CaseInsensitive);
    if (!isNinePatchFile || raw.width() < 3 || raw.height() < 3) {
        np.image_ = raw;
        np.xSpans_ = {{0, raw.width(), true}};
        np.ySpans_ = {{0, raw.height(), true}};
        cache.insert(path, np);
        return np;
    }

    const int w = raw.width() - 2;
    const int h = raw.height() - 2;

    QVector<bool> xStretch(w, false);
    QVector<bool> yStretch(h, false);
    QVector<bool> xContent(w, false);
    QVector<bool> yContent(h, false);

    bool anyXStretch = false;
    bool anyYStretch = false;
    bool anyXContent = false;
    bool anyYContent = false;

    for (int x = 0; x < w; ++x) {
        xStretch[x] = isMarkerPixel(raw.pixel(x + 1, 0));
        anyXStretch |= xStretch[x];
        xContent[x] = isMarkerPixel(raw.pixel(x + 1, raw.height() - 1));
        anyXContent |= xContent[x];
    }
    for (int y = 0; y < h; ++y) {
        yStretch[y] = isMarkerPixel(raw.pixel(0, y + 1));
        anyYStretch |= yStretch[y];
        yContent[y] = isMarkerPixel(raw.pixel(raw.width() - 1, y + 1));
        anyYContent |= yContent[y];
    }

    if (!anyXStretch) {
        xStretch.fill(true);
    }
    if (!anyYStretch) {
        yStretch.fill(true);
    }

    np.image_ = raw.copy(1, 1, w, h);
    np.xSpans_ = buildSpans(xStretch);
    np.ySpans_ = buildSpans(yStretch);

    int padLeft = 0, padRight = 0, padTop = 0, padBottom = 0;
    if (anyXContent) {
        extractContentBounds(xContent, padLeft, padRight);
    } else {
        extractContentBounds(xStretch, padLeft, padRight);
    }
    if (anyYContent) {
        extractContentBounds(yContent, padTop, padBottom);
    } else {
        extractContentBounds(yStretch, padTop, padBottom);
    }

    np.contentMargins_ = QMargins(padLeft, padTop, padRight, padBottom);

    int sliceLeft = 0, sliceRight = 0, sliceTop = 0, sliceBottom = 0;
    extractContentBounds(xStretch, sliceLeft, sliceRight);
    extractContentBounds(yStretch, sliceTop, sliceBottom);
    np.sliceMargins_ = QMargins(sliceLeft, sliceTop, sliceRight, sliceBottom);

    cache.insert(path, np);
    return np;
}

static QVector<int> computeTargetSizes(const QVector<NinePatch::Span> &spans, int targetTotal) {
    QVector<int> sizes(spans.size(), 0);
    int fixedTotal = 0;
    int stretchSrcTotal = 0;

    for (int i = 0; i < spans.size(); ++i) {
        if (spans[i].stretch) {
            stretchSrcTotal += spans[i].length;
        } else {
            fixedTotal += spans[i].length;
            sizes[i] = spans[i].length;
        }
    }

    const int remaining = qMax(0, targetTotal - fixedTotal);
    if (stretchSrcTotal <= 0) {
        return sizes;
    }

    int assigned = 0;
    int lastStretchIdx = -1;
    for (int i = 0; i < spans.size(); ++i) {
        if (spans[i].stretch) {
            lastStretchIdx = i;
            const int s = (remaining * spans[i].length) / stretchSrcTotal;
            sizes[i] = s;
            assigned += s;
        }
    }
    if (lastStretchIdx >= 0) {
        sizes[lastStretchIdx] += (remaining - assigned);
    }
    return sizes;
}

void NinePatch::paint(QPainter *painter, const QRect &targetRect) const {
    if (image_.isNull() || targetRect.width() <= 0 || targetRect.height() <= 0) {
        return;
    }

    const QVector<int> dstWidths = computeTargetSizes(xSpans_, targetRect.width());
    const QVector<int> dstHeights = computeTargetSizes(ySpans_, targetRect.height());

    int dstY = targetRect.y();
    for (int yi = 0; yi < ySpans_.size(); ++yi) {
        const int dstH = dstHeights[yi];
        if (dstH <= 0) {
            continue;
        }
        int dstX = targetRect.x();
        for (int xi = 0; xi < xSpans_.size(); ++xi) {
            const int dstW = dstWidths[xi];
            if (dstW > 0) {
                const QRect srcRect(xSpans_[xi].start, ySpans_[yi].start, xSpans_[xi].length, ySpans_[yi].length);
                const QRect dstRect(dstX, dstY, dstW, dstH);
                painter->drawImage(dstRect, image_, srcRect);
            }
            dstX += dstW;
        }
        dstY += dstH;
    }
}

}  // namespace dashboard
