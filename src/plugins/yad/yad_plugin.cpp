#include "yad_plugin.hpp"
#include "../../g19device.hpp"

#include "yad/engine.hpp"
#include "yad/posix_platform.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <algorithm>

static void initYadResources() {
    Q_INIT_RESOURCE(yad);
}

YadPlugin::YadPlugin(QObject *parent) : QObject(parent) {
    initYadResources();

    screen_ = new Gscreen(QImage(QStringLiteral(":/yad/icon.png")), QStringLiteral("YAD"));
    platform_ = std::make_unique<yad::PosixPlatform>(true);
    engine_ = std::make_unique<yad::YadEngine>(*platform_, 320, 240, yad::PixelFormat::RGB32);

    watcher_ = new QFileSystemWatcher(this);
    connect(watcher_, &QFileSystemWatcher::fileChanged, this, &YadPlugin::scheduleReload);
    connect(watcher_, &QFileSystemWatcher::directoryChanged, this, &YadPlugin::scheduleReload);

    reloadDebounce_ = new QTimer(this);
    reloadDebounce_->setSingleShot(true);
    reloadDebounce_->setInterval(100);
    connect(reloadDebounce_, &QTimer::timeout, this, &YadPlugin::reloadConfig);

    stepTimer_ = new QTimer(this);
    stepTimer_->setTimerType(Qt::PreciseTimer);
    stepTimer_->setInterval(50);
    connect(stepTimer_, &QTimer::timeout, this, &YadPlugin::onStep);

    loadConfig(true);
}

YadPlugin::~YadPlugin() {
    if (reloadDebounce_) reloadDebounce_->stop();
    if (stepTimer_) stepTimer_->stop();
    engine_.reset();
    platform_.reset();
    delete screen_;
}

void YadPlugin::armWatchers(const QString &cfgDir) {
    if (QDir(cfgDir).exists() && !watcher_->directories().contains(cfgDir)) {
        watcher_->addPath(cfgDir);
    }
    if (!activeConfigPath_.isEmpty() && QFile::exists(activeConfigPath_) &&
        !watcher_->files().contains(activeConfigPath_)) {
        watcher_->addPath(activeConfigPath_);
    }
}

void YadPlugin::scheduleReload() {
    if (reloadDebounce_) reloadDebounce_->start();
}

void YadPlugin::loadConfig(bool isInitial) {
    const QString cfgDir = QDir::homePath() + QStringLiteral("/.config/G19Daemon");
    const QString yadCfg = cfgDir + QStringLiteral("/yad.json");
    const QString dashCfg = cfgDir + QStringLiteral("/dashboard.json");
    const QString installedCfg = QStringLiteral(YAD_INSTALLED_CONFIG_PATH);
    const QString fallbackCfg = QStringLiteral(YAD_FALLBACK_CONFIG_PATH);
    const QString defaultCfg = QFile::exists(installedCfg) ? installedCfg : fallbackCfg;

    QString chosen;
    if (QFile::exists(yadCfg)) chosen = yadCfg;
    else if (QFile::exists(dashCfg)) chosen = dashCfg;
    else chosen = defaultCfg;

    activeConfigPath_ = chosen;
    armWatchers(cfgDir);

    std::string err;
    if (!engine_->loadConfigFromFile(activeConfigPath_.toStdString(), &err)) {
        qWarning() << "YadPlugin config load failed:" << QString::fromStdString(err);
        if (isInitial && activeConfigPath_ != defaultCfg && QFile::exists(defaultCfg)) {
            engine_->loadConfigFromFile(defaultCfg.toStdString(), &err);
        } else {
            return;
        }
    }
}

void YadPlugin::reloadConfig() {
    loadConfig(false);
    if (isActive_) {
        const uint64_t now = platform_->nowMs();
        const uint64_t interval = static_cast<uint64_t>(engine_->getIntervalMs());
        nextRenderMs_ = now + interval;
        pushFrame(engine_->getFrame(true));
        engine_->step();
    }
}

QString YadPlugin::getName() {
    return QStringLiteral("YAD");
}

QImage YadPlugin::getIcon() {
    return QImage(QStringLiteral(":/yad/menu_icon.png"));
}

bool YadPlugin::isPopup() {
    return false;
}

QObject *YadPlugin::getQObject() {
    return this;
}

void YadPlugin::setActive(bool active) {
    isActive_ = active;
    if (active) {
        const uint64_t now = platform_->nowMs();
        const uint64_t interval = static_cast<uint64_t>(engine_->getIntervalMs());
        nextRenderMs_ = now + interval;
        pushFrame(engine_->getFrame(true));
        engine_->step();
        stepTimer_->start();
    } else {
        stepTimer_->stop();
    }
}

void YadPlugin::lKeys(int keys) {
    if (!isActive_) return;
    if (keys & G19_KEY_LRIGHT) {
        engine_->nextPage();
        nextRenderMs_ = platform_->nowMs() + static_cast<uint64_t>(engine_->getIntervalMs());
        pushFrame(engine_->getFrame(false));
        engine_->step();
    } else if (keys & G19_KEY_LLEFT) {
        engine_->prevPage();
        nextRenderMs_ = platform_->nowMs() + static_cast<uint64_t>(engine_->getIntervalMs());
        pushFrame(engine_->getFrame(false));
        engine_->step();
    } else if (keys & G19_KEY_LOK) {
        nextRenderMs_ = platform_->nowMs() + static_cast<uint64_t>(engine_->getIntervalMs());
        pushFrame(engine_->getFrame(true));
        engine_->step();
    }
}

void YadPlugin::mKeys(int) {}

void YadPlugin::onStep() {
    if (!isActive_) return;
    engine_->step();

    const uint64_t now = platform_->nowMs();
    const uint64_t interval = static_cast<uint64_t>(engine_->getIntervalMs());
    if (now >= nextRenderMs_) {
        pushFrame(engine_->getFrame(true));
        nextRenderMs_ += interval;
        if (now >= nextRenderMs_ + interval) {
            nextRenderMs_ = now + interval;
        }
    }
}

void YadPlugin::pushFrame(const yad::Framebuffer &fb) {
    if (!isActive_) return;
    const QImage frame(reinterpret_cast<const uchar *>(fb.data32()),
                       fb.width(),
                       fb.height(),
                       fb.strideBytes(),
                       QImage::Format_RGB32);
    QPainter *p = screen_->beginFullScreen();
    p->drawImage(0, 0, frame);
    screen_->end();
    emit doAction(displayFullScreen, screen_);
}
