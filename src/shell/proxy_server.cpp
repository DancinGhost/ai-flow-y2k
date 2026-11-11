#include "shell/proxy_server.hpp"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

namespace aiflow::shell {

ProxyServer::ProxyServer(QObject* parent)
    : QObject(parent), m_server(new QTcpServer(this)) {
    connect(m_server, &QTcpServer::newConnection, this, &ProxyServer::onNewConnection);
}

ProxyServer::~ProxyServer() {
    stopServer();
}

bool ProxyServer::startServer(quint16 port) {
    m_port = port;
    if (m_server->isListening()) {
        m_server->close();
    }
    bool ok = m_server->listen(QHostAddress::LocalHost, m_port);
    if (ok) {
        emit statusChanged(true, m_port);
        emit logMessage(QString("Proxy listening on http://127.0.0.1:%1").arg(m_port));
    } else {
        emit statusChanged(false, 0);
        emit logMessage(QString("Proxy failed to bind port %1: %2").arg(m_port).arg(m_server->errorString()));
    }
    return ok;
}

void ProxyServer::stopServer() {
    if (m_server->isListening()) {
        m_server->close();
        emit statusChanged(false, 0);
        emit logMessage("Proxy stopped.");
    }
}

bool ProxyServer::isListening() const {
    return m_server && m_server->isListening();
}

quint16 ProxyServer::serverPort() const {
    return m_port;
}

void ProxyServer::setUpstreamUrl(const QString& url) {
    m_upstreamUrl = url;
}

QString ProxyServer::upstreamUrl() const {
    return m_upstreamUrl;
}

void ProxyServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &ProxyServer::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &ProxyServer::onClientDisconnected);
    }
}

void ProxyServer::onClientReadyRead() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    handleHttpRequest(socket, data);
}

void ProxyServer::onClientDisconnected() {
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    socket->deleteLater();
}

void ProxyServer::sendHttpResponse(QTcpSocket* socket, int statusCode, const QString& statusText,
                                   const QString& contentType, const QByteArray& body) {
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;

    QByteArray resp;
    resp.append(QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    resp.append("Server: ai-flow-y2k-proxy/1.0\r\n");
    resp.append(QString("Content-Type: %1\r\n").arg(contentType).toUtf8());
    resp.append(QString("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    resp.append("Access-Control-Allow-Origin: *\r\n");
    resp.append("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
    resp.append("Access-Control-Allow-Headers: Content-Type, Authorization\r\n");
    resp.append("Connection: close\r\n\r\n");
    resp.append(body);

    socket->write(resp);
    socket->flush();
    socket->disconnectFromHost();
}

void ProxyServer::handleHttpRequest(QTcpSocket* socket, const QByteArray& requestData) {
    QString req = QString::fromUtf8(requestData);
    QStringList lines = req.split("\r\n");
    if (lines.isEmpty()) return;

    QString reqLine = lines.first();
    QStringList parts = reqLine.split(" ");
    if (parts.size() < 2) return;

    QString method = parts[0].toUpper();
    QString path = parts[1];

    if (method == "OPTIONS") {
        sendHttpResponse(socket, 200, "OK", "text/plain", "");
        return;
    }

    if (path == "/" || path == "/health" || path == "/status") {
        QJsonObject info;
        info["status"] = "online";
        info["proxy"] = "ai-flow-y2k";
        info["version"] = "1.0.0";
        info["theme_support"] = "vaporwave, digital_rain, frutiger_aero";
        info["port"] = m_port;
        sendHttpResponse(socket, 200, "OK", "application/json", QJsonDocument(info).toJson());
        return;
    }

    // Extract JSON Body
    int headerEnd = requestData.indexOf("\r\n\r\n");
    QByteArray body = (headerEnd != -1) ? requestData.mid(headerEnd + 4) : QByteArray();

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QString sessionId = QString("session_%1").arg(now);

    emit logMessage(QString("Incoming %1 %2 (%3 bytes)").arg(method, path).arg(body.size()));

    // Parse Model and Messages
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    QString modelName = "default-model";

    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("model")) {
            modelName = obj["model"].toString();
        }
    }

    // Emit Session Started event
    emit eventGenerated(aiflow::core::events::SessionStarted{
        sessionId.toStdString(),
        modelName.toStdString(),
        "AI Agent Client",
        now
    });

    // Check if client expects streaming SSE or standard JSON
    bool isStream = false;
    if (doc.isObject() && doc.object().contains("stream")) {
        isStream = doc.object()["stream"].toBool();
    }

    // For local interception and testing demonstration, simulate rich agent response
    sendMockStreamingResponse(socket, sessionId);
}

void ProxyServer::sendMockStreamingResponse(QTcpSocket* socket, const QString& sessionId) {
    // Generate headers for SSE stream
    QByteArray header;
    header.append("HTTP/1.1 200 OK\r\n");
    header.append("Content-Type: text/event-stream\r\n");
    header.append("Cache-Control: no-cache\r\n");
    header.append("Connection: keep-alive\r\n");
    header.append("Access-Control-Allow-Origin: *\r\n\r\n");

    socket->write(header);
    socket->flush();

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 1. Send Thinking Block
    emit eventGenerated(aiflow::core::events::ThinkingDelta{
        sessionId.toStdString(),
        "Evaluating incoming user query...\nI should inspect the target directory structure to locate configuration files.",
        now
    });
    emit eventGenerated(aiflow::core::events::ThinkingFinished{sessionId.toStdString(), now + 100});

    // 2. Emit Tool Call
    aiflow::core::ToolCall call;
    call.id = QString("call_%1").arg(now).toStdString();
    call.toolName = "execute_bash";
    call.rawArguments = "{\"command\": \"cat /etc/os-release && ls -la /tmp\"}";
    call.timestampMs = now + 150;

    // Buffer socket if approval is required
    PausedClient client;
    client.socket = socket;
    client.sessionId = sessionId;
    client.callId = QString::fromStdString(call.id);
    m_pausedCalls[client.callId] = client;

    emit eventGenerated(aiflow::core::events::ToolCallDetected{
        sessionId.toStdString(),
        call
    });
}

void ProxyServer::handleAction(const aiflow::core::DomainAction& action) {
    std::visit([&](const auto& act) {
        using T = std::decay_t<decltype(act)>;

        if constexpr (std::is_same_v<T, aiflow::core::actions::PauseResponseStream>) {
            emit logMessage(QString("Stream paused: Call %1 awaiting user approval.").arg(QString::fromStdString(act.callId)));
        }
        else if constexpr (std::is_same_v<T, aiflow::core::actions::ResumeResponseStream>) {
            QString callId = QString::fromStdString(act.callId);
            auto it = m_pausedCalls.find(callId);
            if (it != m_pausedCalls.end()) {
                QTcpSocket* sock = it->second.socket;
                if (sock && sock->state() == QAbstractSocket::ConnectedState) {
                    // Send approved tool call payload SSE chunk
                    QJsonObject chunk;
                    chunk["id"] = "chatcmpl-" + callId;
                    chunk["object"] = "chat.completion.chunk";
                    QJsonObject delta;
                    delta["content"] = QString("[Tool Approved: %1]").arg(QString::fromStdString(act.transformedPayload));
                    QJsonObject choice;
                    choice["delta"] = delta;
                    QJsonArray choices;
                    choices.append(choice);
                    chunk["choices"] = choices;

                    QByteArray data = "data: " + QJsonDocument(chunk).toJson(QJsonDocument::Compact) + "\n\n";
                    sock->write(data);
                    sock->write("data: [DONE]\n\n");
                    sock->flush();
                    sock->disconnectFromHost();
                }
                m_pausedCalls.erase(it);
                emit logMessage(QString("Resumed stream for call %1.").arg(callId));
            }
        }
        else if constexpr (std::is_same_v<T, aiflow::core::actions::EmitToolRejection>) {
            QString callId = QString::fromStdString(act.callId);
            auto it = m_pausedCalls.find(callId);
            if (it != m_pausedCalls.end()) {
                QTcpSocket* sock = it->second.socket;
                if (sock && sock->state() == QAbstractSocket::ConnectedState) {
                    QJsonObject chunk;
                    chunk["id"] = "chatcmpl-" + callId;
                    chunk["error"] = QString("Execution blocked by user policy: %1").arg(QString::fromStdString(act.errorMessage));

                    QByteArray data = "data: " + QJsonDocument(chunk).toJson(QJsonDocument::Compact) + "\n\n";
                    sock->write(data);
                    sock->write("data: [DONE]\n\n");
                    sock->flush();
                    sock->disconnectFromHost();
                }
                m_pausedCalls.erase(it);
                emit logMessage(QString("Rejected call %1.").arg(callId));
            }
        }
    }, action);
}

} // namespace aiflow::shell
