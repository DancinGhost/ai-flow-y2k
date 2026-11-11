#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <map>
#include "core/models.hpp"
#include "core/stream_parser.hpp"

namespace aiflow::shell {

class ProxyServer : public QObject {
    Q_OBJECT

public:
    explicit ProxyServer(QObject* parent = nullptr);
    ~ProxyServer() override;

    bool startServer(quint16 port = 8765);
    void stopServer();
    bool isListening() const;
    quint16 serverPort() const;

    void setUpstreamUrl(const QString& url);
    QString upstreamUrl() const;

public slots:
    void handleAction(const aiflow::core::DomainAction& action);

signals:
    void eventGenerated(const aiflow::core::FlowEvent& event);
    void logMessage(const QString& message);
    void statusChanged(bool isRunning, quint16 port);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    void handleHttpRequest(QTcpSocket* socket, const QByteArray& requestData);
    void sendHttpResponse(QTcpSocket* socket, int statusCode, const QString& statusText,
                          const QString& contentType, const QByteArray& body);
    void sendMockStreamingResponse(QTcpSocket* socket, const QString& sessionId);

    QTcpServer* m_server = nullptr;
    quint16 m_port = 8765;
    QString m_upstreamUrl = "https://api.openai.com";
    
    // In-flight client sockets paused waiting for tool call decisions
    struct PausedClient {
        QTcpSocket* socket = nullptr;
        QString sessionId;
        QString callId;
        QByteArray bufferedHeader;
    };
    std::map<QString, PausedClient> m_pausedCalls; // key = callId
    std::map<QString, aiflow::core::ParserSessionState> m_parserStates;
};

} // namespace aiflow::shell
