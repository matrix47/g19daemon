#include "preview_window.hpp"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QPixmap>
#include <QVBoxLayout>

namespace dashboard {

PreviewWindow::PreviewWindow(const QString &configPath, QWidget *parent)
    : QMainWindow(parent), configPath_(configPath) {
    setWindowTitle(QStringLiteral("G19 Dashboard Preview"));

    view_ = new DashboardView(QSize(320, 240));
    haClient_ = new HaClient(this);
    connect(haClient_, &HaClient::stateUpdated, this, &PreviewWindow::renderFrame);
    connect(haClient_, &HaClient::connectionChanged, this, [this](bool ok, const QString &reason) {
        statusLabel_->setText(QStringLiteral("%1 | HA: %2")
                                  .arg(configPath_, ok ? QStringLiteral("connected") : reason));
    });

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);

    auto *toolbar = new QHBoxLayout();
    auto *prevBtn = new QPushButton(QStringLiteral("< Prev"), central);
    auto *nextBtn = new QPushButton(QStringLiteral("Next >"), central);
    auto *reloadBtn = new QPushButton(QStringLiteral("Reload"), central);
    scaleBox_ = new QComboBox(central);
    scaleBox_->addItem(QStringLiteral("1x (320x240)"), 1);
    scaleBox_->addItem(QStringLiteral("2x (640x480)"), 2);
    scaleBox_->addItem(QStringLiteral("3x (960x720)"), 3);
    scaleBox_->setCurrentIndex(1);

    toolbar->addWidget(prevBtn);
    toolbar->addWidget(nextBtn);
    toolbar->addWidget(reloadBtn);
    toolbar->addStretch(1);
    toolbar->addWidget(new QLabel(QStringLiteral("Zoom:"), central));
    toolbar->addWidget(scaleBox_);
    root->addLayout(toolbar);

    canvasLabel_ = new QLabel(central);
    canvasLabel_->setAlignment(Qt::AlignCenter);
    canvasLabel_->setStyleSheet(QStringLiteral("background: #111; border: 1px solid #333;"));
    root->addWidget(canvasLabel_, 1);

    statusLabel_ = new QLabel(central);
    root->addWidget(statusLabel_);

    setCentralWidget(central);

    watcher_ = new QFileSystemWatcher(this);
    if (QFileInfo::exists(configPath_)) {
        watcher_->addPath(configPath_);
    }

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &PreviewWindow::renderFrame);
    connect(watcher_, &QFileSystemWatcher::fileChanged, this, &PreviewWindow::loadConfig);
    connect(reloadBtn, &QPushButton::clicked, this, &PreviewWindow::loadConfig);
    connect(prevBtn, &QPushButton::clicked, this, [this]() {
        view_->prevPage();
        renderFrame();
    });
    connect(nextBtn, &QPushButton::clicked, this, [this]() {
        view_->nextPage();
        renderFrame();
    });
    connect(scaleBox_, &QComboBox::currentIndexChanged, this, &PreviewWindow::onScaleChanged);

    loadConfig();
    timer_->start(config_.refreshMs);
}

PreviewWindow::~PreviewWindow() {
    delete view_;
}

void PreviewWindow::loadConfig() {
    if (QFileInfo::exists(configPath_) && !watcher_->files().contains(configPath_)) {
        watcher_->addPath(configPath_);
    }
    DashboardConfig cfg;
    QString err;
    if (DashboardConfig::loadFromFile(configPath_, cfg, &err)) {
        config_ = cfg;
        view_->setConfig(config_);
        timer_->setInterval(config_.refreshMs);
        haClient_->configure(config_.homeAssistant, HaClient::extractWatchedEntities(config_));
        const QString haStatus = haClient_->isConnected()
                                     ? QStringLiteral("connected")
                                     : (haClient_->lastError().isEmpty() ? QStringLiteral("connecting...") : haClient_->lastError());
        statusLabel_->setText(QStringLiteral("%1 (%2p) | HA: %3")
                                  .arg(configPath_)
                                  .arg(config_.pages.size())
                                  .arg(haStatus));
    } else {
        statusLabel_->setText(QStringLiteral("Error: %1").arg(err));
    }
    renderFrame();
}

void PreviewWindow::onScaleChanged(int) {
    scale_ = scaleBox_->currentData().toInt();
    renderFrame();
}

void PreviewWindow::renderFrame() {
    QVariantMap snapshot;
    sensors_.sample(snapshot);
    haClient_->mergeIntoSnapshot(snapshot);
    const QImage frame = view_->renderToImage(snapshot);
    QPixmap pm = QPixmap::fromImage(frame);
    if (scale_ > 1) {
        pm = pm.scaled(frame.size() * scale_, Qt::KeepAspectRatio, Qt::FastTransformation);
    }
    canvasLabel_->setPixmap(pm);
    canvasLabel_->setMinimumSize(pm.size() + QSize(8, 8));
    adjustSize();
}

}  // namespace dashboard
