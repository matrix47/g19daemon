#include "ha_client.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcessEnvironment>

namespace dashboard {

HaClient::HaClient(QObject *parent) : QObject(parent) {
    socket_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    reconnectTimer_ = new QTimer(this);
    reconnectTimer_->setSingleShot(true);
    pingTimer_ = new QTimer(this);
    pingTimer_->setInterval(30000);

    connect(socket_, &QWebSocket::connected, this, &HaClient::onConnected);
    connect(socket_, &QWebSocket::disconnected, this, &HaClient::onDisconnected);
    connect(socket_, &QWebSocket::textMessageReceived, this, &HaClient::onTextMessageReceived);
    connect(socket_, &QWebSocket::sslErrors, this, &HaClient::onSslErrors);
    connect(socket_, &QWebSocket::errorOccurred, this, &HaClient::onErrorOccurred);
    connect(reconnectTimer_, &QTimer::timeout, this, &HaClient::onReconnectTimer);
    connect(pingTimer_, &QTimer::timeout, this, &HaClient::onPingTimer);
}

HaClient::~HaClient() {
    stop();
}

QSet<QString> HaClient::extractWatchedEntities(const DashboardConfig &config) {
    QSet<QString> entities;
    for (const PageConfig &page : config.pages) {
        for (const RowConfig &row : page.rows) {
            for (const ColumnConfig &col : row.columns) {
                const QJsonObject &p = col.cardParams;
                const QString e = p.value(QStringLiteral("entity")).toString();
                if (!e.isEmpty()) {
                    entities.insert(e);
                }
                const QString ce = p.value(QStringLiteral("condition_entity")).toString();
                if (!ce.isEmpty()) {
                    entities.insert(ce);
                }
                const QJsonArray items = p.value(QStringLiteral("items")).toArray();
                for (const QJsonValue &iv : items) {
                    const QString ie = iv.toObject().value(QStringLiteral("entity")).toString();
                    if (!ie.isEmpty()) {
                        entities.insert(ie);
                    }
                }
            }
        }
    }
    return entities;
}

QString HaClient::resolveToken(const HomeAssistantConfig &config) const {
    if (!config.token.isEmpty()) {
        return config.token.trimmed();
    }
    if (!config.tokenEnv.isEmpty()) {
        const QString envVal = QProcessEnvironment::systemEnvironment().value(config.tokenEnv).trimmed();
        if (!envVal.isEmpty()) {
            return envVal;
        }
    }
    if (!config.tokenFile.isEmpty()) {
        QString path = config.tokenFile;
        if (path.startsWith(QStringLiteral("~/"))) {
            path = QDir::homePath() + path.mid(1);
        }
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QByteArray raw = f.readAll().trimmed();
            if (path.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
                const QJsonDocument doc = QJsonDocument::fromJson(raw);
                if (doc.isObject()) {
                    return doc.object().value(QStringLiteral("token")).toString().trimmed();
                }
            }
            return QString::fromUtf8(raw).trimmed();
        }
    }
    return {};
}

QUrl HaClient::buildWebSocketUrl(const QString &rawUrl) const {
    if (rawUrl.isEmpty()) {
        return {};
    }
    QUrl u(rawUrl.trimmed());
    if (u.scheme() == QLatin1String("https")) {
        u.setScheme(QStringLiteral("wss"));
    } else if (u.scheme() == QLatin1String("http")) {
        u.setScheme(QStringLiteral("ws"));
    }
    if (u.path().isEmpty() || u.path() == QLatin1String("/")) {
        u.setPath(QStringLiteral("/api/websocket"));
    }
    return u;
}

void HaClient::configure(const HomeAssistantConfig &config, const QSet<QString> &watchedEntities) {
    const bool connectionChangedNeeded =
        (config_.url != config.url) ||
        (config_.tlsVerify != config.tlsVerify) ||
        (config_.token != config.token) ||
        (config_.tokenEnv != config.tokenEnv) ||
        (config_.tokenFile != config.tokenFile);

    config_ = config;
    watchedEntities_ = watchedEntities;
    resolvedToken_ = resolveToken(config);
    wsUrl_ = buildWebSocketUrl(config.url);
    authFailed_ = false;

    if (!wsUrl_.isValid() || resolvedToken_.isEmpty()) {
        stop();
        lastError_ = resolvedToken_.isEmpty()
                         ? QStringLiteral("No Home Assistant token configured")
                         : QStringLiteral("Invalid Home Assistant URL");
        return;
    }

    if (!started_ || connectionChangedNeeded) {
        stop();
        started_ = true;
        reconnectDelayMs_ = 2000;
        connectSocket();
    }
}

void HaClient::stop() {
    started_ = false;
    authenticated_ = false;
    connected_ = false;
    reconnectTimer_->stop();
    pingTimer_->stop();
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->close();
    }
}

void HaClient::connectSocket() {
    if (!started_ || !wsUrl_.isValid() || authFailed_) {
        return;
    }
    authenticated_ = false;
    connected_ = false;
    socket_->open(wsUrl_);
}

void HaClient::scheduleReconnect() {
    if (!started_ || authFailed_) {
        return;
    }
    if (!reconnectTimer_->isActive()) {
        reconnectTimer_->start(reconnectDelayMs_);
        reconnectDelayMs_ = qMin(30000, reconnectDelayMs_ * 2);
    }
}

void HaClient::onReconnectTimer() {
    connectSocket();
}

void HaClient::onPingTimer() {
    if (connected_) {
        socket_->ping();
    }
}

void HaClient::onConnected() {
    lastError_.clear();
}

void HaClient::onDisconnected() {
    const bool wasConnected = connected_;
    connected_ = false;
    authenticated_ = false;
    pingTimer_->stop();
    if (wasConnected) {
        emit connectionChanged(false, QStringLiteral("disconnected"));
    }
    scheduleReconnect();
}

void HaClient::onSslErrors(const QList<QSslError> &errors) {
    if (!config_.tlsVerify) {
        socket_->ignoreSslErrors();
        return;
    }
    QStringList msgs;
    for (const QSslError &e : errors) {
        msgs << e.errorString();
    }
    lastError_ = msgs.join(QStringLiteral("; "));
    qWarning() << "HaClient SSL error:" << lastError_;
}

void HaClient::onErrorOccurred(QAbstractSocket::SocketError) {
    lastError_ = socket_->errorString();
    qWarning() << "HaClient socket error:" << lastError_;
    if (socket_->state() == QAbstractSocket::UnconnectedState) {
        scheduleReconnect();
    }
}

void HaClient::onTextMessageReceived(const QString &message) {
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        return;
    }
    const QJsonObject obj = doc.object();
    const QString type = obj.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("auth_required")) {
        QJsonObject auth;
        auth.insert(QStringLiteral("type"), QStringLiteral("auth"));
        auth.insert(QStringLiteral("access_token"), resolvedToken_);
        socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(auth).toJson(QJsonDocument::Compact)));
        return;
    }

    if (type == QLatin1String("auth_ok")) {
        authenticated_ = true;
        connected_ = true;
        reconnectDelayMs_ = 2000;
        pingTimer_->start();
        emit connectionChanged(true, QStringLiteral("ok"));

        // 1. Fetch current states once
        getStatesMsgId_ = nextMsgId_++;
        QJsonObject getStates;
        getStates.insert(QStringLiteral("id"), getStatesMsgId_);
        getStates.insert(QStringLiteral("type"), QStringLiteral("get_states"));
        socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(getStates).toJson(QJsonDocument::Compact)));

        // 2. Subscribe to real-time state_changed events
        QJsonObject sub;
        sub.insert(QStringLiteral("id"), nextMsgId_++);
        sub.insert(QStringLiteral("type"), QStringLiteral("subscribe_events"));
        sub.insert(QStringLiteral("event_type"), QStringLiteral("state_changed"));
        socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(sub).toJson(QJsonDocument::Compact)));
        return;
    }

    if (type == QLatin1String("auth_invalid")) {
        authFailed_ = true;
        lastError_ = obj.value(QStringLiteral("message")).toString(QStringLiteral("Authentication invalid"));
        qWarning() << "HaClient auth failed:" << lastError_;
        emit connectionChanged(false, lastError_);
        socket_->close();
        return;
    }

    if (type == QLatin1String("result")) {
        const int id = obj.value(QStringLiteral("id")).toInt(-1);
        if (id == getStatesMsgId_ && obj.value(QStringLiteral("success")).toBool(false)) {
            const QJsonArray arr = obj.value(QStringLiteral("result")).toArray();
            for (const QJsonValue &v : arr) {
                if (v.isObject()) {
                    applyEntityState(v.toObject());
                }
            }
            emit stateUpdated();
        }
        return;
    }

    if (type == QLatin1String("event")) {
        const QJsonObject ev = obj.value(QStringLiteral("event")).toObject();
        if (ev.value(QStringLiteral("event_type")).toString() == QLatin1String("state_changed")) {
            const QJsonObject data = ev.value(QStringLiteral("data")).toObject();
            const QString entityId = data.value(QStringLiteral("entity_id")).toString();
            if (!watchedEntities_.isEmpty() && !watchedEntities_.contains(entityId)) {
                return;
            }
            const QJsonObject newState = data.value(QStringLiteral("new_state")).toObject();
            if (!newState.isEmpty()) {
                applyEntityState(newState);
                emit stateUpdated();
            }
        }
    }
}

void HaClient::applyEntityState(const QJsonObject &stateObj) {
    const QString entityId = stateObj.value(QStringLiteral("entity_id")).toString();
    if (entityId.isEmpty()) {
        return;
    }
    if (!watchedEntities_.isEmpty() && !watchedEntities_.contains(entityId)) {
        return;
    }

    const QString baseKey = QStringLiteral("ha.") + entityId;
    states_.insert(baseKey, stateObj.value(QStringLiteral("state")).toVariant());

    const QJsonObject attrs = stateObj.value(QStringLiteral("attributes")).toObject();
    for (auto it = attrs.constBegin(); it != attrs.constEnd(); ++it) {
        states_.insert(baseKey + QLatin1Char('.') + it.key(), it.value().toVariant());
    }
}

void HaClient::mergeIntoSnapshot(QVariantMap &outSnapshot) const {
    outSnapshot.insert(QStringLiteral("ha._connected"), connected_);
    for (auto it = states_.constBegin(); it != states_.constEnd(); ++it) {
        outSnapshot.insert(it.key(), it.value());
    }
}

void HaClient::callService(const QString &domain,
                           const QString &service,
                           const QString &entityId,
                           const QJsonObject &serviceData) {
    if (!connected_ || !authenticated_) {
        return;
    }
    QJsonObject msg;
    msg.insert(QStringLiteral("id"), nextMsgId_++);
    msg.insert(QStringLiteral("type"), QStringLiteral("call_service"));
    msg.insert(QStringLiteral("domain"), domain);
    msg.insert(QStringLiteral("service"), service);
    if (!entityId.isEmpty()) {
        QJsonObject target;
        target.insert(QStringLiteral("entity_id"), entityId);
        msg.insert(QStringLiteral("target"), target);
    }
    if (!serviceData.isEmpty()) {
        msg.insert(QStringLiteral("service_data"), serviceData);
    }
    socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact)));
}

}  // namespace dashboard
