#include "preview_window.hpp"
#include "../config/layout_schema.hpp"
#include "../providers/ha_client.hpp"
#include "../providers/system_sensors.hpp"
#include "../ui/dashboard_view.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>

int main(int argc, char *argv[]) {
    Q_INIT_RESOURCE(dashboard);
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("G19 Dashboard Preview Harness"));
    parser.addHelpOption();
    QCommandLineOption configOpt(QStringList() << QStringLiteral("c") << QStringLiteral("config"),
                                 QStringLiteral("Path to dashboard JSON config"),
                                 QStringLiteral("file"));
    QCommandLineOption dumpOpt(QStringList() << QStringLiteral("d") << QStringLiteral("dump"),
                               QStringLiteral("Render one 320x240 frame to PNG and exit"),
                               QStringLiteral("png_file"));
    QCommandLineOption waitOpt(QStringLiteral("wait-ms"),
                               QStringLiteral("Wait milliseconds for async feeds before dumping"),
                               QStringLiteral("ms"),
                               QStringLiteral("200"));
    parser.addOption(configOpt);
    parser.addOption(dumpOpt);
    parser.addOption(waitOpt);
    parser.process(app);

    QString configPath = parser.value(configOpt);
    if (configPath.isEmpty()) {
        configPath = QStringLiteral(":/dashboard/default_dashboard.json");
    }

    if (parser.isSet(dumpOpt)) {
        dashboard::DashboardConfig cfg;
        QString err;
        if (!dashboard::DashboardConfig::loadFromFile(configPath, cfg, &err)) {
            qCritical() << "Failed to load config:" << err;
            return 1;
        }
        dashboard::DashboardView view(QSize(320, 240));
        view.setConfig(cfg);
        dashboard::SystemSensors sensors;
        dashboard::HaClient haClient;
        haClient.configure(cfg.homeAssistant, dashboard::HaClient::extractWatchedEntities(cfg));

        QEventLoop loop;
        QTimer::singleShot(qMax(50, parser.value(waitOpt).toInt()), &loop, &QEventLoop::quit);
        loop.exec();

        QVariantMap snapshot;
        sensors.sample(snapshot);
        haClient.mergeIntoSnapshot(snapshot);
        const QImage img = view.renderToImage(snapshot);
        const QString outPath = parser.value(dumpOpt);
        if (!img.save(outPath)) {
            qCritical() << "Failed to save image to:" << outPath;
            return 1;
        }
        return 0;
    }

    dashboard::PreviewWindow win(configPath);
    win.show();
    return app.exec();
}
