#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QImage>
#include <QLabel>
#include <QWidget>
#include <QVBoxLayout>

class ScreenCastServer : public QObject
{
    Q_OBJECT
public:
    explicit ScreenCastServer(QObject *parent = nullptr);
    ~ScreenCastServer();

    bool startCasting(quint16 port = 4244);
    void stopCasting();

private slots:
    void onNewConnection();
    void captureAndSend();
    void onDisconnected();

private:
    QTcpServer *m_tcpServer;
    QTcpSocket *m_clientSocket;
    QTimer *m_captureTimer;
};

class ScreenCastClient : public QObject
{
    Q_OBJECT
public:
    explicit ScreenCastClient(QObject *parent = nullptr);
    ~ScreenCastClient();

    void connectToServer(const QString &host, quint16 port = 4244);
    void disconnectFromServer();

signals:
    void frameReceived(const QImage &image);

private slots:
    void onReadyRead();

private:
    QTcpSocket *m_socket;
    qint32 m_expectedSize;
};

class ScreenViewerWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ScreenViewerWindow(QWidget *parent = nullptr);
    ~ScreenViewerWindow();

    void startViewing(const QString &host);

private slots:
    void updateFrame(const QImage &image);

private:
    QLabel *m_imageLabel;
    ScreenCastClient *m_client;
};
