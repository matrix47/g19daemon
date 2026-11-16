#pragma once

#include "../../gscreen.hpp"
#include "../../plugininterface.hpp"
#include "config/layout_schema.hpp"
#include "providers/ha_client.hpp"
#include "providers/system_sensors.hpp"
#include "ui/dashboard_view.hpp"

#include <QFileSystemWatcher>
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>

namespace dashboard {

class DashboardPluginBase : public QObject, public PluginInterface {
    Q_OBJECT
    Q_INTERFACES(PluginInterface)

public:
    DashboardPluginBase(const QString &pluginId,
                        const QString &pluginName,
                        const QString &embeddedConfigPath,
                        QObject *parent = nullptr);
    ~DashboardPluginBase() override;

    void lKeys(int keys) override;
    QString getName() override;
    QImage getIcon() override;
    void setActive(bool active) override;
    bool isPopup() override;
    QObject *getQObject() override;
    void mKeys(int keys) override;

signals:
    void doAction(gAction action, void *data);

private slots:
    void onTimer();
    void scheduleReload();
    void reloadConfig();

private:
    void loadConfig(bool isInitial);
    void armWatchers();
    void paint();

    QString pluginId_;
    QString pluginName_;
    QString embeddedConfigPath_;
    QString overrideConfigPath_;

    Gscreen *screen_ = nullptr;
    DashboardView *view_ = nullptr;
    SystemSensors sensors_;
    HaClient *haClient_ = nullptr;
    DashboardConfig config_;
    QTimer *timer_ = nullptr;
    QTimer *reloadDebounce_ = nullptr;
    QFileSystemWatcher *watcher_ = nullptr;
    bool isActive_ = false;
};

}  // namespace dashboard
