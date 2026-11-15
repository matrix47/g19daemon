#include "icon_helper.hpp"

#include <QFile>
#include <QFontDatabase>
#include <QHash>
#include <QHBoxLayout>
#include <QPainter>
#include <QSvgRenderer>

namespace dashboard {

static void ensureTerminusLoaded() {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    const QStringList candidates = {
        QStringLiteral("/usr/share/fonts/TTF/TerminusTTF.ttf"),
        QStringLiteral("/usr/share/fonts/TTF/TerminusTTF-Bold.ttf"),
        QStringLiteral("/usr/share/fonts/TTF/TerminusTTF-Italic.ttf")
    };
    for (const QString &path : candidates) {
        if (QFile::exists(path)) {
            QFontDatabase::addApplicationFont(path);
        }
    }
}

QFont::HintingPreference IconHelper::parseHinting(const QString &hinting) {
    const QString h = hinting.trimmed().toLower();
    if (h == QLatin1String("full") || h == QLatin1String("on") || h == QLatin1String("true")) {
        return QFont::PreferFullHinting;
    }
    if (h == QLatin1String("vertical")) {
        return QFont::PreferVerticalHinting;
    }
    if (h == QLatin1String("default")) {
        return QFont::PreferDefaultHinting;
    }
    return QFont::PreferNoHinting;
}

QFont IconHelper::makeFont(const QString &family, int pixelSize, bool bold, const QString &hinting) {
    ensureTerminusLoaded();

    QFont font(family.isEmpty() ? QStringLiteral("Terminus (TTF)") : family);
    font.setPixelSize(qMax(6, pixelSize));
    font.setBold(bold);
    font.setHintingPreference(parseHinting(hinting));
    if (font.family().contains(QStringLiteral("Terminus"), Qt::CaseInsensitive) && pixelSize <= 20) {
        font.setStyleStrategy(QFont::NoAntialias);
    }
    return font;
}

QString IconHelper::resolveIconPath(const QString &nameOrPath) {
    if (nameOrPath.isEmpty()) {
        return {};
    }
    if (nameOrPath.startsWith(QLatin1String(":/")) || nameOrPath.startsWith(QLatin1Char('/'))) {
        return nameOrPath;
    }
    QString clean = nameOrPath;
    if (!clean.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
        clean += QStringLiteral(".svg");
    }
    return QStringLiteral(":/dashboard/icons/") + clean;
}

QPixmap IconHelper::renderSvg(const QString &nameOrPath, int sizePx, const QColor &tint) {
    const QString path = resolveIconPath(nameOrPath);
    if (path.isEmpty() || sizePx <= 0) {
        return {};
    }

    const QString key = QStringLiteral("%1|%2|%3").arg(path).arg(sizePx).arg(tint.rgba(), 0, 16);
    static QHash<QString, QPixmap> cache;
    if (cache.contains(key)) {
        return cache.value(key);
    }

    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
        return {};
    }

    QImage img(sizePx, sizePx, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&p, QRectF(0, 0, sizePx, sizePx));
        if (tint.isValid()) {
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(img.rect(), tint);
        }
    }

    QPixmap pm = QPixmap::fromImage(img);
    cache.insert(key, pm);
    return pm;
}

IconTextRow IconHelper::createIconTextRow(QWidget *parent,
                                           const QFont &font,
                                           const QColor &textColor,
                                           int spacing) {
    IconTextRow row;
    row.container = new QWidget(parent);
    row.container->setAttribute(Qt::WA_TranslucentBackground);

    auto *layout = new QHBoxLayout(row.container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(qMax(0, spacing));

    row.iconLabel = new QLabel(row.container);
    row.iconLabel->setAlignment(Qt::AlignCenter);
    row.iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));
    row.iconLabel->setVisible(false);

    row.textLabel = new QLabel(row.container);
    row.textLabel->setTextFormat(Qt::PlainText);
    row.textLabel->setFont(font);
    row.textLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(textColor.name()));
    row.textLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    layout->addStretch(1);
    layout->addWidget(row.iconLabel, 0, Qt::AlignVCenter);
    layout->addWidget(row.textLabel, 0, Qt::AlignVCenter);
    layout->addStretch(1);
    return row;
}

void IconHelper::updateIconTextRow(const IconTextRow &row,
                                   const QString &iconNameOrPath,
                                   int iconSizePx,
                                   const QColor &iconTint,
                                   const QString &text) {
    if (row.iconLabel) {
        const QPixmap pm = renderSvg(iconNameOrPath, iconSizePx, iconTint);
        if (pm.isNull()) {
            row.iconLabel->clear();
            row.iconLabel->setVisible(false);
        } else {
            row.iconLabel->setPixmap(pm);
            row.iconLabel->setFixedSize(pm.size());
            row.iconLabel->setVisible(true);
        }
    }
    if (row.textLabel) {
        row.textLabel->setText(text);
    }
}

}  // namespace dashboard
