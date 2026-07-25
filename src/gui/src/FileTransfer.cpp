#include "FileTransfer.h"
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

// ---------------- FileTransferServer ----------------

FileTransferServer::FileTransferServer(QObject *parent)
    : QObject(parent), m_tcpServer(new QTcpServer(this)), m_clientSocket(nullptr), m_incomingFile(nullptr), m_expectedSize(0), m_receivedSize(0)
{
    connect(m_tcpServer, &QTcpServer::newConnection, this, &FileTransferServer::onNewConnection);
}

FileTransferServer::~FileTransferServer()
{
    stopServer();
}

bool FileTransferServer::startServer(quint16 port)
{
    return m_tcpServer->listen(QHostAddress::Any, port);
}

void FileTransferServer::stopServer()
{
    if (m_tcpServer->isListening()) {
        m_tcpServer->close();
    }
}

void FileTransferServer::onNewConnection()
{
    if (m_clientSocket) {
        // Reject multiple simultaneous transfers for simplicity in MVP
        QTcpSocket *tempSocket = m_tcpServer->nextPendingConnection();
        tempSocket->disconnectFromHost();
        tempSocket->deleteLater();
        return;
    }

    m_clientSocket = m_tcpServer->nextPendingConnection();
    connect(m_clientSocket, &QTcpSocket::readyRead, this, &FileTransferServer::onReadyRead);
    connect(m_clientSocket, &QTcpSocket::disconnected, this, &FileTransferServer::onDisconnected);
    
    m_expectedSize = 0;
    m_receivedSize = 0;
    m_fileName.clear();
}

void FileTransferServer::onReadyRead()
{
    QDataStream in(m_clientSocket);
    in.setVersion(QDataStream::Qt_5_9); // Match Qt version roughly

    if (m_expectedSize == 0) {
        if (m_clientSocket->bytesAvailable() < sizeof(qint64) + sizeof(qint32)) {
            return;
        }
        
        in >> m_expectedSize;
        in >> m_fileName;

        QString savePath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + QDir::separator() + m_fileName;
        m_incomingFile = new QFile(savePath, this);
        if (!m_incomingFile->open(QIODevice::WriteOnly)) {
            qWarning() << "Could not open file for writing:" << savePath;
            m_clientSocket->disconnectFromHost();
            return;
        }
    }

    if (m_incomingFile && m_incomingFile->isOpen()) {
        QByteArray buffer = m_clientSocket->readAll();
        m_incomingFile->write(buffer);
        m_receivedSize += buffer.size();

        if (m_receivedSize >= m_expectedSize) {
            // Transfer complete
            m_incomingFile->close();
            m_incomingFile->deleteLater();
            m_incomingFile = nullptr;
            m_clientSocket->disconnectFromHost();
        }
    }
}

void FileTransferServer::onDisconnected()
{
    if (m_incomingFile && m_incomingFile->isOpen()) {
        m_incomingFile->close();
        m_incomingFile->deleteLater();
        m_incomingFile = nullptr;
    }
    m_clientSocket->deleteLater();
    m_clientSocket = nullptr;
}


// ---------------- FileTransferClient ----------------

FileTransferClient::FileTransferClient(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this)), m_outgoingFile(nullptr), m_totalSize(0), m_sentSize(0)
{
    connect(m_socket, &QTcpSocket::connected, this, &FileTransferClient::onConnected);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &FileTransferClient::onBytesWritten);
    connect(m_socket, &QTcpSocket::disconnected, this, &FileTransferClient::onDisconnected);
}

FileTransferClient::~FileTransferClient()
{
    if (m_socket->isOpen()) {
        m_socket->close();
    }
}

void FileTransferClient::sendFile(const QString &host, quint16 port, const QString &filePath)
{
    if (m_outgoingFile && m_outgoingFile->isOpen()) {
        qWarning() << "Transfer already in progress.";
        return;
    }

    m_outgoingFile = new QFile(filePath, this);
    if (!m_outgoingFile->open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open file for reading:" << filePath;
        m_outgoingFile->deleteLater();
        m_outgoingFile = nullptr;
        return;
    }

    m_totalSize = m_outgoingFile->size();
    m_sentSize = 0;
    
    m_socket->connectToHost(host, port);
}

void FileTransferClient::onConnected()
{
    QDataStream out(m_socket);
    out.setVersion(QDataStream::Qt_5_9);
    
    QFileInfo fileInfo(*m_outgoingFile);
    out << m_totalSize;
    out << fileInfo.fileName();

    // Start writing first chunk
    QByteArray buffer = m_outgoingFile->read(65536);
    m_socket->write(buffer);
}

void FileTransferClient::onBytesWritten(qint64 bytes)
{
    m_sentSize += bytes;
    if (m_sentSize < m_totalSize && m_outgoingFile && !m_outgoingFile->atEnd()) {
        QByteArray buffer = m_outgoingFile->read(65536);
        m_socket->write(buffer);
    } else if (m_sentSize >= m_totalSize) {
        m_socket->disconnectFromHost();
    }
}

void FileTransferClient::onDisconnected()
{
    if (m_outgoingFile) {
        m_outgoingFile->close();
        m_outgoingFile->deleteLater();
        m_outgoingFile = nullptr;
    }
}
