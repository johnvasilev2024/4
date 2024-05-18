//
//

#include <QNetworkProxy>
#include <QNetworkInterface>
#include <QHostAddress>
#include "evercamregistrator.h"
#include "registratorcommands.h"

//----------------------------------------------------------------------------------------------------------------------

EvercamRegistrator::EvercamRegistrator(
        const QString &name,
        const QString &ip,
        const QString &mac,
        quint16 statusPort,
        quint16 motionClientPort,
        int pollInterval,
        int pollTimeout,
        QObject *parent
)
    : QObject(parent)
    , name_(name)
    , ip_(ip)
    , mac_(mac)
    , statusPort_(statusPort)
    , motionClientPort_(motionClientPort)
    , socket_(nullptr)
    , statusCheckTimer_(this)
    , timeoutTimer_(this)
{
    init(pollInterval, pollTimeout);
}

EvercamRegistrator::~EvercamRegistrator()
{
    delete socket_;
}

QString EvercamRegistrator::name() const
{
    return name_;
}

QString EvercamRegistrator::ip() const
{
    return ip_;
}

QString EvercamRegistrator::mac() const
{
    return mac_;
}

QString EvercamRegistrator::wol() const
{
    const auto localhost = QHostAddress(QHostAddress::LocalHost);

    for (auto address: QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost) {
            // FIXME TODO check if this is correct interface
            return address.toString();
        }
    }

    return QString();
}

quint16 EvercamRegistrator::motionClientPort() const
{
    return motionClientPort_;
}

QList<QString> EvercamRegistrator::camerasSerials() const
{
    QList<QString> result;

    for (auto serial : statuses_.keys()) {
        result.append(serial);
    }

    return result;
}

EvercamStatus EvercamRegistrator::status(const QString &serial)
{
    if (serial.isEmpty())
        return statuses_.isEmpty() ? EvercamStatus() : statuses_.first().first;

    return statuses_.contains(serial) ? statuses_[serial].first : EvercamStatus();
}

bool EvercamRegistrator::sendCommand(const QString &command, const QString &params)
{
    if (socket_->state() != QAbstractSocket::ConnectedState) {
        return false;
    }

    QString request =
        params.isEmpty()
        ? tr("GET /%1 HTTP/1.1").arg(command)
        : tr("GET /%1?%2 HTTP/1.1").arg(command).arg(params);

    socket_->write(request.toUtf8());
    socket_->flush();

    //qDebug() << Q_FUNC_INFO << request;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

void EvercamRegistrator::init(int pollInterval, int pollTimeout)
{
    socket_ = new QTcpSocket(this);
    socket_->setProxy(QNetworkProxy::NoProxy);
    socket_->connectToHost(ip_, statusPort_);

    connect(socket_, &QIODevice::readyRead, this, [=]() {
        auto data = socket_->readAll();
        processRegistratorReply(data);
    });

    connect(socket_, &QTcpSocket::stateChanged, this, [=](auto state) {
        if (state == QAbstractSocket::UnconnectedState) {
            QTimer::singleShot(1000, this, [=] { // FIXME reconnect delay from params
                if (socket_) {
                    socket_->connectToHost(ip_, statusPort_);
                }
            });
        }
    });

    connect(this, &EvercamRegistrator::statusReceived, this, [=](const QString &serial, const EvercamStatus &status) {
        if (statuses_.contains(serial)) {
            // we knew about this camera previously, mark it as checked
            statuses_[serial] = {status, true};
        } else {
            // camera has appeared, mark it as unchecked
            statuses_[serial] = {status, false};
            qDebug() << "appeared" << serial;
            emit camerasUpdated();
        }
        //timeoutTimer_.start();
    });

    timeoutTimer_.setSingleShot(true);
    timeoutTimer_.setInterval(pollTimeout);
    connect(&timeoutTimer_, &QTimer::timeout, this, [=]() {
        qDebug() << "TIMEOUT";
        statuses_.clear();
        emit camerasUpdated();
    });

    statusCheckTimer_.setSingleShot(false);
    statusCheckTimer_.start(pollInterval);
    connect(&statusCheckTimer_, &QTimer::timeout, this, [=]() {
        for (auto serial : statuses_.keys()) {
            statuses_[serial].second = false; // mark camera as unchecked
        }
        if (sendCommand(RegistratorCommands::getStatus)) {
            timeoutTimer_.start();
        }
    });
}

void EvercamRegistrator::checkCamerasListChange()
{
    bool isUpdated = false;

    for (auto serial : statuses_.keys()) {
        if (!statuses_[serial].second) {
            statuses_.remove(serial);
            isUpdated = true;
        }
    }

    if (isUpdated) {
        emit camerasUpdated();
    }
}

void EvercamRegistrator::processRegistratorReply(const QByteArray &data)
{
    QByteArray copy(data);

    EvercamStatus status(copy);
    int start = status.start();
    int end = status.end();

    while (start >= 0 && end > start) {
        int serial = status.serial();
        if (serial > 0) {
            emit statusReceived(QString::number(serial), status);
        }
        copy = copy.mid(end);
        status = EvercamStatus(copy);
        start = status.start();
        end = status.end();
    }
}

//----------------------------------------------------------------------------------------------------------------------
