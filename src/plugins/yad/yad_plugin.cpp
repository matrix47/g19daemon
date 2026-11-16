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

    netTimer_ = new QTimer(this);
    netTimer_->setInterval(50);
    connect(netTimer_, &QTimer::timeout, this, &YadPlugin::onNetPoll);

    renderTimer_ = new QTimer(this);
    renderTimer_->setTimerType(Qt::PreciseTimer);
    renderTimer_->setInterval(1000);
    connect(renderTimer_, &QTimer::timeout, this, &YadPlugin::onRenderTick);

    loadConfig(true);
}

YadPlugin::~YadPlugin() {
    if (reloadDebounce_) reloadDebounce_->stop();
    if (netTimer_) netTimer_->stop();
    if (renderTimer_) renderTimer_->stop();
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

    renderTimer_->setInterval(std::max(100, engine_->config().refreshMs));
}

void YadPlugin::reloadConfig() {
    loadConfig(false);
    if (isActive_) {
        engine_->sampleAndRender();
        pushFrame();
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
        engine_->pollNetwork();
        engine_->sampleAndRender();
        pushFrame();
        netTimer_->start();
        renderTimer_->start();
    } else {
        netTimer_->stop();
        renderTimer_->stop();
    }
}

void YadPlugin::lKeys(int keys) {
    if (!isActive_) return;
    if (keys & G19_KEY_LRIGHT) {
        engine_->nextPage();
        engine_->renderNow();
        pushFrame();
    } else if (keys & G19_KEY_LLEFT) {
        engine_->prevPage();
        engine_->renderNow();
        pushFrame();
    } else if (keys & G19_KEY_LOK) {
        engine_->sampleAndRender();
        pushFrame();
    }
}

void YadPlugin::mKeys(int) {}

void YadPlugin::onNetPoll() {
    if (!isActive_) return;
    engine_->pollNetwork();
}

void YadPlugin::onRenderTick() {
    if (!isActive_) return;
    engine_->sampleAndRender();
    pushFrame();
}

void YadPlugin::pushFrame() {
    if (!isActive_) return;
    const QImage frame(reinterpret_cast<const uchar *>(engine_->pixels32()),
                       engine_->width(),
                       engine_->height(),
                       engine_->width() * static_cast<int>(sizeof(uint32_t)),
                       QImage::Format_RGB32);
    QPainter *p = screen_->beginFullScreen();
    p->drawImage(0, 0, frame);
    screen_->end();
    emit doAction(displayFullScreen, screen_);
}
