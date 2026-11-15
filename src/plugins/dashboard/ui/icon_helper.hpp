#pragma once

#include <QColor>
#include <QFont>
#include <QLabel>
#include <QPixmap>
#include <QString>

namespace dashboard {

struct IconTextRow {
    QWidget *container = nullptr;
    QLabel *iconLabel = nullptr;
    QLabel *textLabel = nullptr;
};

class IconHelper {
public:
    static QFont::HintingPreference parseHinting(const QString &hinting);
    static QFont makeFont(const QString &family,
                          int pixelSize,
                          bool bold = false,
                          const QString &hinting = QStringLiteral("none"));

    static QString resolveIconPath(const QString &nameOrPath);
    static QPixmap renderSvg(const QString &nameOrPath, int sizePx, const QColor &tint);

    // Creates a centered horizontal icon + plain-text label pair without HTML/RichText overhead.
    static IconTextRow createIconTextRow(QWidget *parent,
                                         const QFont &font,
                                         const QColor &textColor,
                                         int spacing = 3);
    static void updateIconTextRow(const IconTextRow &row,
                                  const QString &iconNameOrPath,
                                  int iconSizePx,
                                  const QColor &iconTint,
                                  const QString &text);
};

}  // namespace dashboard
