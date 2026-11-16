#include "../dashboard/config/layout_schema.hpp"
#include "../dashboard/providers/system_sensors.hpp"
#include "../dashboard/ui/dashboard_view.hpp"
#include "yad/config.hpp"
#include "yad/view.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QImage>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

int main(int argc, char *argv[]) {
    Q_INIT_RESOURCE(dashboard);
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Compare Qt dashboard renderer vs libyad renderer on identical state"));
    parser.addHelpOption();
    QCommandLineOption cfgOpt(QStringList() << QStringLiteral("c") << QStringLiteral("config"),
                              QStringLiteral("Dashboard JSON config"),
                              QStringLiteral("file"),
                              QStringLiteral(YAD_COMPARE_DEFAULT_CONFIG));
    QCommandLineOption outDirOpt(QStringList() << QStringLiteral("o") << QStringLiteral("out-dir"),
                                 QStringLiteral("Optional directory to save qt.png, yad.png, diff.png"),
                                 QStringLiteral("dir"));
    QCommandLineOption synthOpt(QStringLiteral("synthetic"),
                                QStringLiteral("Include fixed synthetic HA/weather values in snapshot"));
    parser.addOption(cfgOpt);
    parser.addOption(outDirOpt);
    parser.addOption(synthOpt);
    parser.process(app);

    const QString cfgPath = parser.value(cfgOpt);

    dashboard::DashboardConfig qtCfg;
    QString qErr;
    if (!dashboard::DashboardConfig::loadFromFile(cfgPath, qtCfg, &qErr)) {
        std::fprintf(stderr, "Qt config error: %s\n", qErr.toUtf8().constData());
        return 1;
    }

    yad::DashboardConfig yadCfg;
    std::string yErr;
    if (!yad::DashboardConfig::loadFromFile(cfgPath.toStdString(), yadCfg, &yErr)) {
        std::fprintf(stderr, "YAD config error: %s\n", yErr.c_str());
        return 1;
    }

    dashboard::DashboardView qtView(QSize(320, 240));
    qtView.setConfig(qtCfg);

    yad::DashboardView yadView(320, 240, yad::PixelFormat::RGB32);
    yadView.setConfig(yadCfg);

    // Capture one real local snapshot so both renderers see identical values
    dashboard::SystemSensors sensors;
    QVariantMap qSnap;
    sensors.sample(qSnap);
    yad::Snapshot ySnap;
    for (auto it = qSnap.constBegin(); it != qSnap.constEnd(); ++it) {
        const std::string k = it.key().toStdString();
        const QVariant &v = it.value();
        if (v.typeId() == QMetaType::Bool) ySnap.set(k, v.toBool());
        else if (v.typeId() == QMetaType::Double || v.typeId() == QMetaType::Int || v.typeId() == QMetaType::UInt) {
            ySnap.set(k, v.toDouble());
        } else {
            ySnap.set(k, v.toString().toStdString());
        }
    }

    if (parser.isSet(synthOpt)) {
        auto putStr = [&](const char *k, const char *v) { qSnap.insert(k, QString::fromUtf8(v)); ySnap.set(k, v); };
        auto putNum = [&](const char *k, double v) { qSnap.insert(k, v); ySnap.set(k, v); };
        qSnap.insert(QStringLiteral("ha._connected"), true);
        ySnap.set("ha._connected", true);
        putStr("ha.weather.home", "partlycloudy");
        putNum("ha.weather.home.temperature", 12.0);
        putStr("ha.weather.home.temperature_unit", "°C");
    }

    std::time_t nowSec = 0;
    QImage qtImg;
    while (true) {
        nowSec = std::time(nullptr);
        qtImg = qtView.renderToImage(qSnap);
        yadView.render(ySnap, 1000, nowSec);
        if (std::time(nullptr) == nowSec) break;
    }
    qtImg = qtImg.convertToFormat(QImage::Format_RGB32);

    const QImage yadImg(reinterpret_cast<const uchar *>(yadView.pixels32()),
                        320, 240, 320 * 4, QImage::Format_RGB32);
    QImage diffImg(320, 240, QImage::Format_Grayscale8);

    int exact = 0;
    int near2 = 0;
    int over8 = 0;
    const int total = 320 * 240;

    for (int y = 0; y < 240; ++y) {
        const QRgb *qa = reinterpret_cast<const QRgb *>(qtImg.constScanLine(y));
        const QRgb *ya = reinterpret_cast<const QRgb *>(yadImg.constScanLine(y));
        uchar *da = diffImg.scanLine(y);
        for (int x = 0; x < 320; ++x) {
            const int dr = std::abs(qRed(qa[x]) - qRed(ya[x]));
            const int dg = std::abs(qGreen(qa[x]) - qGreen(ya[x]));
            const int db = std::abs(qBlue(qa[x]) - qBlue(ya[x]));
            const int md = std::max({dr, dg, db});
            if (md == 0) ++exact;
            if (md <= 2) ++near2;
            if (md > 8) ++over8;
            da[x] = static_cast<uchar>(std::min(255, md * 4));
        }
    }

    std::printf("Comparison (%s):\n", cfgPath.toUtf8().constData());
    std::printf("  Total px:        %d\n", total);
    std::printf("  Exact (diff==0): %d (%.2f%%)\n", exact, (100.0 * exact) / total);
    std::printf("  Near  (diff<=2): %d (%.2f%%)\n", near2, (100.0 * near2) / total);
    std::printf("  Large (diff>8):  %d (%.2f%%)\n", over8, (100.0 * over8) / total);

    const QString outDir = parser.value(outDirOpt);
    if (!outDir.isEmpty()) {
        QDir().mkpath(outDir);
        qtImg.save(outDir + QStringLiteral("/qt.png"));
        yadImg.save(outDir + QStringLiteral("/yad.png"));
        diffImg.save(outDir + QStringLiteral("/diff.png"));
        std::printf("Saved qt.png, yad.png, diff.png to %s\n", outDir.toUtf8().constData());
    }

    return 0;
}
