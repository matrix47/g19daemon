#pragma once

#include "../config/layout_schema.hpp"
#include "../providers/ha_client.hpp"
#include "../providers/system_sensors.hpp"
#include "../ui/dashboard_view.hpp"

#include <QComboBox>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QTimer>

namespace dashboard {

class PreviewWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit PreviewWindow(const QString &configPath, QWidget *parent = nullptr);
    ~PreviewWindow() override;

private slots:
    void loadConfig();
    void renderFrame();
    void onScaleChanged(int index);

private:
    QString configPath_;
    DashboardView *view_ = nullptr;
    SystemSensors sensors_;
    HaClient *haClient_ = nullptr;
    DashboardConfig config_;
    QFileSystemWatcher *watcher_ = nullptr;
    QTimer *timer_ = nullptr;

    QLabel *canvasLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QComboBox *scaleBox_ = nullptr;
    int scale_ = 2;
};

}  // namespace dashboard
