#include "layout_schema.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>

namespace dashboard {

SizeSpec SizeSpec::parse(const QJsonValue &val) {
    return parse(val, SizeSpec{});
}

SizeSpec SizeSpec::parse(const QJsonValue &val, const SizeSpec &fallback) {
    if (val.isNull() || val.isUndefined()) {
        return fallback;
    }
    if (val.isDouble()) {
        double d = val.toDouble();
        if (d > 0) {
            return {Mode::FixedPx, d};
        }
        return fallback;
    }
    if (val.isString()) {
        const QString s = val.toString().trimmed().toLower();
        if (s.isEmpty() || s == QLatin1String("auto")) {
            return {Mode::Stretch, 1.0};
        }
        if (s.endsWith(QLatin1Char('%'))) {
            bool ok = false;
            double v = s.left(s.size() - 1).toDouble(&ok);
            if (ok && v > 0) {
                return {Mode::Percent, v};
            }
        }
        if (s.endsWith(QLatin1Char('*'))) {
            QString num = s.left(s.size() - 1).trimmed();
            if (num.isEmpty()) {
                return {Mode::Stretch, 1.0};
            }
            bool ok = false;
            double v = num.toDouble(&ok);
            if (ok && v > 0) {
                return {Mode::Stretch, v};
            }
        }
        QString px = s;
        if (px.endsWith(QLatin1String("px"))) {
            px.chop(2);
        }
        bool ok = false;
        double v = px.toDouble(&ok);
        if (ok && v > 0) {
            return {Mode::FixedPx, v};
        }
    }
    return fallback;
}

Qt::Alignment parseAlignment(const QString &text) {
    Qt::Alignment align;
    const QStringList tokens = text.toLower().split(QRegularExpression(QStringLiteral("[|,\\s]+")), Qt::SkipEmptyParts);
    for (const QString &tok : tokens) {
        if (tok == QLatin1String("left")) {
            align |= Qt::AlignLeft;
        } else if (tok == QLatin1String("right")) {
            align |= Qt::AlignRight;
        } else if (tok == QLatin1String("hcenter")) {
            align |= Qt::AlignHCenter;
        } else if (tok == QLatin1String("top")) {
            align |= Qt::AlignTop;
        } else if (tok == QLatin1String("bottom")) {
            align |= Qt::AlignBottom;
        } else if (tok == QLatin1String("vcenter")) {
            align |= Qt::AlignVCenter;
        } else if (tok == QLatin1String("center")) {
            align |= Qt::AlignCenter;
        }
    }
    if (align == Qt::Alignment{}) {
        return Qt::AlignCenter;
    }
    if ((align & Qt::AlignHorizontal_Mask) == 0) {
        align |= Qt::AlignHCenter;
    }
    if ((align & Qt::AlignVertical_Mask) == 0) {
        align |= Qt::AlignVCenter;
    }
    return align;
}

QMargins parseMargins(const QJsonValue &val) {
    if (val.isDouble()) {
        int m = val.toInt();
        return QMargins(m, m, m, m);
    }
    if (val.isArray()) {
        const QJsonArray arr = val.toArray();
        if (arr.size() == 4) {
            // CSS order: [top, right, bottom, left] -> QMargins(left, top, right, bottom)
            return QMargins(arr[3].toInt(), arr[0].toInt(), arr[1].toInt(), arr[2].toInt());
        }
        if (arr.size() == 2) {
            int v = arr[0].toInt();
            int h = arr[1].toInt();
            return QMargins(h, v, h, v);
        }
    }
    return QMargins(0, 0, 0, 0);
}

static QVector<RowConfig> parseRows(const QJsonArray &rowsArr) {
    QVector<RowConfig> rows;
    rows.reserve(rowsArr.size());

    for (const QJsonValue &rv : rowsArr) {
        if (!rv.isObject()) {
            continue;
        }
        const QJsonObject ro = rv.toObject();
        RowConfig row;
        row.height = SizeSpec::parse(ro.value(QStringLiteral("height")), {SizeSpec::Mode::Stretch, 1.0});
        row.gap = ro.value(QStringLiteral("gap")).toInt(4);

        const QJsonArray colsArr = ro.value(QStringLiteral("columns")).toArray();
        row.columns.reserve(colsArr.size());
        for (const QJsonValue &cv : colsArr) {
            if (!cv.isObject()) {
                continue;
            }
            const QJsonObject co = cv.toObject();
            ColumnConfig col;
            col.width = SizeSpec::parse(co.value(QStringLiteral("width")), {SizeSpec::Mode::Stretch, 1.0});
            col.cardWidth = SizeSpec::parse(co.value(QStringLiteral("card_width")), {SizeSpec::Mode::Stretch, 0.0});
            col.cardHeight = SizeSpec::parse(co.value(QStringLiteral("card_height")), {SizeSpec::Mode::Stretch, 0.0});

            if (co.contains(QStringLiteral("alignment"))) {
                col.hasAlignment = true;
                col.alignment = parseAlignment(co.value(QStringLiteral("alignment")).toString());
            }

            if (co.contains(QStringLiteral("ninepatch"))) {
                col.ninePatch = co.value(QStringLiteral("ninepatch")).toString();
            }
            if (co.contains(QStringLiteral("padding"))) {
                col.padding = parseMargins(co.value(QStringLiteral("padding")));
            }
            if (co.contains(QStringLiteral("background"))) {
                col.background = QColor(co.value(QStringLiteral("background")).toString());
            }

            col.cardType = co.value(QStringLiteral("card_type")).toString(QStringLiteral("spacer"));
            col.cardParams = co.value(QStringLiteral("card_params")).toObject();
            row.columns.append(col);
        }
        rows.append(row);
    }
    return rows;
}

bool DashboardConfig::loadFromFile(const QString &path, DashboardConfig &out, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Failed to open config '%1': %2").arg(path, file.errorString());
        }
        return false;
    }
    return loadFromJson(file.readAll(), out, error);
}

bool DashboardConfig::loadFromJson(const QByteArray &jsonBytes, DashboardConfig &out, QString *error) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QStringLiteral("JSON parse error at offset %1: %2")
                         .arg(parseError.offset)
                         .arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = doc.object();
    DashboardConfig cfg;

    if (root.contains(QStringLiteral("plugin")) && root.value(QStringLiteral("plugin")).isObject()) {
        const QJsonObject po = root.value(QStringLiteral("plugin")).toObject();
        cfg.pluginId = po.value(QStringLiteral("id")).toString(cfg.pluginId);
        cfg.pluginName = po.value(QStringLiteral("name")).toString(cfg.pluginName);
    }

    cfg.refreshMs = qMax(100, root.value(QStringLiteral("refresh_ms")).toInt(1000));

    if (root.contains(QStringLiteral("theme")) && root.value(QStringLiteral("theme")).isObject()) {
        const QJsonObject to = root.value(QStringLiteral("theme")).toObject();
        if (to.contains(QStringLiteral("background"))) {
            cfg.theme.background = QColor(to.value(QStringLiteral("background")).toString());
        }
        if (to.contains(QStringLiteral("foreground"))) {
            cfg.theme.foreground = QColor(to.value(QStringLiteral("foreground")).toString());
        }
        if (to.contains(QStringLiteral("muted"))) {
            cfg.theme.muted = QColor(to.value(QStringLiteral("muted")).toString());
        }
        if (to.contains(QStringLiteral("accent"))) {
            cfg.theme.accent = QColor(to.value(QStringLiteral("accent")).toString());
        }
        cfg.theme.fontFamily = to.value(QStringLiteral("font_family")).toString(cfg.theme.fontFamily);
        cfg.theme.fontSize = to.value(QStringLiteral("font_size")).toInt(cfg.theme.fontSize);
        if (to.contains(QStringLiteral("font_hinting")) || to.contains(QStringLiteral("hinting"))) {
            const QJsonValue hv = to.contains(QStringLiteral("font_hinting"))
                                      ? to.value(QStringLiteral("font_hinting"))
                                      : to.value(QStringLiteral("hinting"));
            if (hv.isBool()) {
                cfg.theme.fontHinting = hv.toBool() ? QStringLiteral("full") : QStringLiteral("none");
            } else if (hv.isString()) {
                cfg.theme.fontHinting = hv.toString().trimmed().toLower();
            }
        }
        cfg.theme.outerMargin = to.value(QStringLiteral("outer_margin")).toInt(cfg.theme.outerMargin);
        cfg.theme.rowGap = to.value(QStringLiteral("row_gap")).toInt(cfg.theme.rowGap);
        cfg.theme.defaultNinePatch = to.value(QStringLiteral("default_ninepatch")).toString(cfg.theme.defaultNinePatch);
    }

    if (root.contains(QStringLiteral("home_assistant")) && root.value(QStringLiteral("home_assistant")).isObject()) {
        const QJsonObject ha = root.value(QStringLiteral("home_assistant")).toObject();
        cfg.homeAssistant.url = ha.value(QStringLiteral("url")).toString();
        cfg.homeAssistant.tlsVerify = ha.value(QStringLiteral("tls_verify")).toBool(true);
        cfg.homeAssistant.token = ha.value(QStringLiteral("token")).toString();
        cfg.homeAssistant.tokenEnv = ha.value(QStringLiteral("token_env")).toString(cfg.homeAssistant.tokenEnv);
        cfg.homeAssistant.tokenFile = ha.value(QStringLiteral("token_file")).toString(cfg.homeAssistant.tokenFile);
    }

    if (root.contains(QStringLiteral("pages")) && root.value(QStringLiteral("pages")).isArray()) {
        const QJsonArray pagesArr = root.value(QStringLiteral("pages")).toArray();
        for (const QJsonValue &pv : pagesArr) {
            if (!pv.isObject()) {
                continue;
            }
            const QJsonObject po = pv.toObject();
            PageConfig page;
            page.name = po.value(QStringLiteral("name")).toString(QStringLiteral("Page"));
            page.rows = parseRows(po.value(QStringLiteral("rows")).toArray());
            cfg.pages.append(page);
        }
    } else if (root.contains(QStringLiteral("rows")) && root.value(QStringLiteral("rows")).isArray()) {
        PageConfig page;
        page.name = cfg.pluginName;
        page.rows = parseRows(root.value(QStringLiteral("rows")).toArray());
        cfg.pages.append(page);
    }

    out = cfg;
    return true;
}

}  // namespace dashboard
