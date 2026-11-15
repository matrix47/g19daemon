#include "dashboard_plugin_base.hpp"
#include "../../g19device.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPainter>

static void initDashboardResources() {
    Q_INIT_RESOURCE(dashboard);
}

namespace dashboard {

DashboardPluginBase::DashboardPluginBase(const QString &pluginId,
                                         const QString &pluginName,
                                         const QString &embeddedConfigPath,
                                         QObject *parent)
    : QObject(parent),
      pluginId_(pluginId),
      pluginName_(pluginName),
      embeddedConfigPath_(embeddedConfigPath) {
    initDashboardResources();

    overrideConfigPath_ =
        QDir::homePath() + QStringLiteral("/.config/G19Daemon/%1.json").arg(pluginId_);

    screen_ = new Gscreen(QImage(QStringLiteral(":/dashboard/icon.png")), pluginName_);
    view_ = new DashboardView(QSize(320, 240));
    haClient_ = new HaClient(this);
    connect(haClient_, &HaClient::stateUpdated, this, [this]() {
        if (isActive_) {
            paint();
        }
    });
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &DashboardPluginBase::onTimer);

    watcher_ = new QFileSystemWatcher(this);
    connect(watcher_, &QFileSystemWatcher::fileChanged, this, &DashboardPluginBase::reloadConfig);

    loadConfig();
}

DashboardPluginBase::~DashboardPluginBase() {
    if (timer_) {
        timer_->stop();
    }
    delete view_;
    delete screen_;
}

void DashboardPluginBase::loadConfig() {
    QString loadPath = embeddedConfigPath_;
    if (QFile::exists(overrideConfigPath_)) {
        loadPath = overrideConfigPath_;
        if (!watcher_->files().contains(overrideConfigPath_)) {
            watcher_->addPath(overrideConfigPath_);
        }
    }

    DashboardConfig cfg;
    QString err;
    if (!DashboardConfig::loadFromFile(loadPath, cfg, &err)) {
        qWarning() << "Dashboard config load failed:" << err;
        if (loadPath != embeddedConfigPath_) {
            DashboardConfig::loadFromFile(embeddedConfigPath_, cfg, &err);
        }
    }

    config_ = cfg;
    if (!config_.pluginName.isEmpty() && pluginName_.isEmpty()) {
        pluginName_ = config_.pluginName;
    }
    view_->setConfig(config_);
    timer_->setInterval(config_.refreshMs);
    haClient_->configure(config_.homeAssistant, HaClient::extractWatchedEntities(config_));
}

void DashboardPluginBase::reloadConfig() {
    loadConfig();
    if (isActive_) {
        paint();
    }
}

QString DashboardPluginBase::getName() {
    return pluginName_;
}

QImage DashboardPluginBase::getIcon() {
    return QImage(QStringLiteral(":/dashboard/menu_icon.png"));
}

bool DashboardPluginBase::isPopup() {
    return false;
}

QObject *DashboardPluginBase::getQObject() {
    return this;
}

void DashboardPluginBase::setActive(bool active) {
    isActive_ = active;
    if (active) {
        paint();
        timer_->start(config_.refreshMs);
    } else {
        timer_->stop();
    }
}

void DashboardPluginBase::lKeys(int keys) {
    if (!isActive_) {
        return;
    }
    if (keys & G19_KEY_LRIGHT) {
        view_->nextPage();
        paint();
    } else if (keys & G19_KEY_LLEFT) {
        view_->prevPage();
        paint();
    } else if (keys & G19_KEY_LOK) {
        paint();
    }
}

void DashboardPluginBase::mKeys(int) {}

void DashboardPluginBase::onTimer() {
    paint();
}

void DashboardPluginBase::paint() {
    if (!isActive_) {
        return;
    }

    QPainter *p = screen_->beginFullScreen();
    p->fillRect(0, 0, 320, 240, config_.theme.background);
    QVariantMap snapshot;
    sensors_.sample(snapshot);
    haClient_->mergeIntoSnapshot(snapshot);
    view_->render(p, snapshot);
    screen_->end();

    emit doAction(displayFullScreen, screen_);
}

}  // namespace dashboard
