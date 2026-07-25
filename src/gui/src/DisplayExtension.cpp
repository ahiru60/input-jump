#include "DisplayExtension.h"
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QBuffer>
#include <QDataStream>
#include <QDebug>

// ---------------- ScreenCastServer ----------------

ScreenCastServer::ScreenCastServer(QObject *parent)
    : QObject(parent), m_tcpServer(new QTcpServer(this)), m_clientSocket(nullptr), m_captureTimer(new QTimer(this))
{
    connect(m_tcpServer, &QTcpServer::newConnection, this, &ScreenCastServer::onNewConnection);
    connect(m_captureTimer, &QTimer::timeout, this, &ScreenCastServer::captureAndSend);
}

ScreenCastServer::~ScreenCastServer()
{
    stopCasting();
}

bool ScreenCastServer::startCasting(quint16 port)
{
    return m_tcpServer->listen(QHostAddress::Any, port);
}

void ScreenCastServer::stopCasting()
{
    m_captureTimer->stop();
    if (m_tcpServer->isListening()) {
        m_tcpServer->close();
    }
}

void ScreenCastServer::onNewConnection()
{
    if (m_clientSocket) {
        QTcpSocket *tempSocket = m_tcpServer->nextPendingConnection();
        tempSocket->disconnectFromHost();
        tempSocket->deleteLater();
        return;
    }

    m_clientSocket = m_tcpServer->nextPendingConnection();
    connect(m_clientSocket, &QTcpSocket::disconnected, this, &ScreenCastServer::onDisconnected);
    
    // Start capturing at roughly 30 FPS
    m_captureTimer->start(33);
}

void ScreenCastServer::captureAndSend()
{
    if (!m_clientSocket || m_clientSocket->state() != QAbstractSocket::ConnectedState) return;

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QPixmap pixmap = screen->grabWindow(0);
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    // Compress to JPEG for faster transmission
    pixmap.save(&buffer, "JPG", 50); 
    
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_9);
    
    out << (qint32)ba.size();
    block.append(ba);

    m_clientSocket->write(block);
}

void ScreenCastServer::onDisconnected()
{
    m_captureTimer->stop();
    m_clientSocket->deleteLater();
    m_clientSocket = nullptr;
}


// ---------------- ScreenCastClient ----------------

ScreenCastClient::ScreenCastClient(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this)), m_expectedSize(0)
{
    connect(m_socket, &QTcpSocket::readyRead, this, &ScreenCastClient::onReadyRead);
}

ScreenCastClient::~ScreenCastClient()
{
    disconnectFromServer();
}

void ScreenCastClient::connectToServer(const QString &host, quint16 port)
{
    m_socket->connectToHost(host, port);
}

void ScreenCastClient::disconnectFromServer()
{
    if (m_socket->isOpen()) {
        m_socket->close();
    }
}

void ScreenCastClient::onReadyRead()
{
    QDataStream in(m_socket);
    in.setVersion(QDataStream::Qt_5_9);

    while (true) {
        if (m_expectedSize == 0) {
            if (m_socket->bytesAvailable() < sizeof(qint32)) {
                return;
            }
            in >> m_expectedSize;
        }

        if (m_socket->bytesAvailable() < m_expectedSize) {
            return;
        }

        QByteArray ba = m_socket->read(m_expectedSize);
        m_expectedSize = 0;

        QImage image;
        if (image.loadFromData(ba, "JPG")) {
            emit frameReceived(image);
        }
    }
}


// ---------------- ScreenViewerWindow ----------------

ScreenViewerWindow::ScreenViewerWindow(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint), m_imageLabel(new QLabel(this)), m_client(new ScreenCastClient(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_imageLabel);
    
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet("background-color: black;");
    
    connect(m_client, &ScreenCastClient::frameReceived, this, &ScreenViewerWindow::updateFrame);
}

ScreenViewerWindow::~ScreenViewerWindow()
{
}

void ScreenViewerWindow::startViewing(const QString &host)
{
    m_client->connectToServer(host, 4244);
    showFullScreen();
}

void ScreenViewerWindow::updateFrame(const QImage &image)
{
    m_imageLabel->setPixmap(QPixmap::fromImage(image).scaled(m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
