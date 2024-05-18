//
//

#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QMap>
#include "evercamstatus.h"


class EvercamRegistrator : public QObject
{
    Q_OBJECT

public:
    explicit EvercamRegistrator(
        const QString &name,
        const QString &ip,
        const QString &mac,
        quint16 statusPort,
        quint16 motionClientPort,
        int pollInterval,
        int pollTimeout,
        QObject *parent = nullptr
    );
    virtual ~EvercamRegistrator();
    QString name() const;
    QString ip() const;
    QString mac() const;
    QString wol() const;
    quint16 motionClientPort() const;
    QList<QString> camerasSerials() const;
    EvercamStatus status(const QString &serial);
    bool sendCommand(const QString &command, const QString &params = QString());

signals:
    void camerasUpdated();
    void statusReceived(const QString &serial, const EvercamStatus &status);

private:
    void init(int pollInterval, int pollTimeout);
    void checkCamerasListChange();
    void processRegistratorReply(const QByteArray &data);

private:
    QString name_;
    QString ip_;
    QString mac_;
    quint16 statusPort_;
    quint16 motionClientPort_;
    QTcpSocket *socket_;
    QTimer statusCheckTimer_;
    QTimer timeoutTimer_;
    QMap<QString, QPair<EvercamStatus, bool>> statuses_;
};
