#include "devicemanager.h"
#include "device.h"
#include "systemtime.h"
#include "log.h"
#include "basicoffsetmeasurement.h"

#include <QTcpServer>

DeviceManager::DeviceManager()
{
    connect(&m_samples, &Samples::signalSendTimeSample,
            this, &DeviceManager::slotSendTimeSample);
    connect(&m_samples, &Samples::signalSampleRunStatusUpdate,
            this, &DeviceManager::slotSampleRunStatusUpdate);
}


Device* DeviceManager::findDevice(const QString& name)
{
    for(auto device : m_deviceDeque)
    {
        if (device->m_name == name)
        {
            return device;
        }
    }
    return nullptr;
}


const DeviceDeque &DeviceManager::getDevices() const
{
    return m_deviceDeque;
}


bool DeviceManager::activeClients() const
{
    return m_activeClients.size();
}


void DeviceManager::process(const MulticastRxPacketPtr rx)
{
    QString from = rx->value("from");
    QString command = rx->value("command");

    if (command == "connect")
    {
        trace->info("got connect request from '{}' at {}", from.toStdString(), rx->value("endpoint").toStdString());
        if (findDevice(from))
        {
            trace->warn("device '{}' already registered - connection request ignored", from.toStdString());
            return;
        }
        Device* newDevice = new Device(this, from);
        m_deviceDeque.append(newDevice);

        connect(newDevice, &Device::signalRequestSamples, &m_samples, &Samples::slotRequestSamples);
        connect(newDevice, &Device::signalConnectionLost, this, &DeviceManager::slotConnectionLost);

        QJsonObject json;
        json["to"] = from;
        json["command"] = "serveraddress";
        json["tcpaddress"] = newDevice->m_serverAddress;
        json["tcpport"] = QString::number(newDevice->m_server->serverPort());
        MulticastTxPacket udp(json);
        emit signalMulticastTx(udp);
    }
}


void DeviceManager::slotSendTimeSample(const QString& client)
{
    Device* device = findDevice(client);
    if (device)
    {
        device->sendServerTimeUDP();
    }
    else
    {
        trace->warn("internal error #0080");
    }
}


void DeviceManager::slotSampleRunStatusUpdate(QString client, bool active)
{
    if (active)
    {
        m_activeClients.append(client);
    }
    else
    {
        m_activeClients.removeAll(client);

        Device* device = findDevice(client);
        if (device)
        {
            device->getClientOffset();
        }
        if (!m_activeClients.size())
        {
            emit signalIdle();
        }
    }
}


void DeviceManager::slotConnectionLost(const QString& client)
{
    for (int i = 0; i < m_deviceDeque.size(); i++)
    {
        if (m_deviceDeque.at(i)->m_name == client)
        {
            m_samples.removeClient(client);
            Device* device = m_deviceDeque.takeAt(i);
            device->deleteLater();
            trace->warn(RED "{} connection lost, removing client. Clients connected: {}" RESET,
                        client.toStdString(),
                        m_deviceDeque.size());
            return;
        }
    }

    trace->error("got connection lost on unknown client '{}'", client.toStdString());
}