#pragma once

#include "../../gscreen.hpp"
#include "../../plugininterface.hpp"

#include <QFileSystemWatcher>
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

namespace yad {
class PosixPlatform;
class YadEngine;
}  // namespace yad

class YadPlugin : public QObject, public PluginInterface {
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "yad")

public:
    explicit YadPlugin(QObject *parent = nullptr);
    ~YadPlugin() override;

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
    void onNetPoll();
    void onRenderTick();
    void scheduleReload();
    void reloadConfig();

private:
    void loadConfig(bool isInitial);
    void armWatchers(const QString &cfgDir);
    void pushFrame();

    Gscreen *screen_ = nullptr;
    QTimer *netTimer_ = nullptr;
    QTimer *renderTimer_ = nullptr;
    QTimer *reloadDebounce_ = nullptr;
    QFileSystemWatcher *watcher_ = nullptr;
    QString activeConfigPath_;
    bool isActive_ = false;

    std::unique_ptr<yad::PosixPlatform> platform_;
    std::unique_ptr<yad::YadEngine> engine_;
};
