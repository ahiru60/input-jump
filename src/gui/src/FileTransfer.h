#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QDataStream>
#include <QFileInfo>

class FileTransferServer : public QObject
{
    Q_OBJECT
public:
    explicit FileTransferServer(QObject *parent = nullptr);
    ~FileTransferServer();

    bool startServer(quint16 port = 4243);
    void stopServer();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QTcpServer *m_tcpServer;
    QTcpSocket *m_clientSocket;
    QFile *m_incomingFile;
    qint64 m_expectedSize;
    qint64 m_receivedSize;
    QString m_fileName;
};

class FileTransferClient : public QObject
{
    Q_OBJECT
public:
    explicit FileTransferClient(QObject *parent = nullptr);
    ~FileTransferClient();

    void sendFile(const QString &host, quint16 port, const QString &filePath);

private slots:
    void onConnected();
    void onBytesWritten(qint64 bytes);
    void onDisconnected();

private:
    QTcpSocket *m_socket;
    QFile *m_outgoingFile;
    qint64 m_totalSize;
    qint64 m_sentSize;
};
