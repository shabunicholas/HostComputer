#include "modbusmaster.h"

ModbusRtuMaster::ModbusRtuMaster(QIODevice *transport, QObject *parent):
                 QObject(parent),transport_(transport)
{

}

ModbusRtuMaster::~ModbusRtuMaster()
{

}

void ModbusRtuMaster::setTransport(QIODevice *transport)
{

}

QIODevice *ModbusRtuMaster::transport() const
{

}

void ModbusRtuMaster::setConfig(const ModbusRtuMaster::Config &config)
{

}

ModbusRtuMaster::Config ModbusRtuMaster::config() const
{

}

ModbusRtuMaster::Result ModbusRtuMaster::readHoldingRegisters(quint8 slave, quint16 start, quint16 count, QVector<quint16> &out, quint8 *exceptionCode)
{

}

ModbusRtuMaster::Result ModbusRtuMaster::readInputRegisters(quint8 slave, quint16 start, quint16 count, QVector<quint16> &out, quint8 *exceptionCode)
{

}

ModbusRtuMaster::Result ModbusRtuMaster::writeSingleRegister(quint8 slave, quint16 address, quint16 value, quint8 *exceptionCode)
{

}

ModbusRtuMaster::Result ModbusRtuMaster::writeMultipleRegisters(quint8 slave, quint16 start, const QVector<quint16> &values, quint8 *exceptionCode)
{

}

void ModbusRtuMaster::resetStatistics()
{

}

QString ModbusRtuMaster::resultText(ModbusRtuMaster::Result result)
{

}
