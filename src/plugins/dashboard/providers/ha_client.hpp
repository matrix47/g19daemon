#pragma once

#include "../config/layout_schema.hpp"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QWebSocket>

namespace dashboard {

class HaClient : public QObject {
    Q_OBJECT
public:
    explicit HaClient(QObject *parent = nullptr);
    ~HaClient() override;

    void configure(const HomeAssistantConfig &config, const QSet<QString> &watchedEntities);
    void stop();

    bool isConnected() const { return connected_; }
    QString lastError() const { return lastError_; }

    void mergeIntoSnapshot(QVariantMap &outSnapshot) const;

    // Ready for Phase 4 touch actions (buttons, toggles, sliders)
    void callService(const QString &domain,
                     const QString &service,
                     const QString &entityId,
                     const QJsonObject &serviceData = {});

    static QSet<QString> extractWatchedEntities(const DashboardConfig &config);

signals:
    void stateUpdated();
    void connectionChanged(bool connected, const QString &reason);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onSslErrors(const QList<QSslError> &errors);
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onReconnectTimer();
    void onPingTimer();

private:
    QString resolveToken(const HomeAssistantConfig &config) const;
    QUrl buildWebSocketUrl(const QString &rawUrl) const;
    void connectSocket();
    void scheduleReconnect();
    void applyEntityState(const QJsonObject &stateObj);

    HomeAssistantConfig config_;
    QSet<QString> watchedEntities_;
    QString resolvedToken_;
    QUrl wsUrl_;

    QWebSocket *socket_ = nullptr;
    QTimer *reconnectTimer_ = nullptr;
    QTimer *pingTimer_ = nullptr;

    bool started_ = false;
    bool authenticated_ = false;
    bool connected_ = false;
    bool authFailed_ = false;
    int reconnectDelayMs_ = 2000;
    int nextMsgId_ = 1;
    int getStatesMsgId_ = -1;
    QString lastError_;

    QVariantMap states_;
};

}  // namespace dashboard
