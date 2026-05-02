#include "SerialWorker.h"

SerialWorker::SerialWorker(QObject *parent)
    : QObject(parent)
{
    // TODO: Initialize QSerialPort; connect readyRead to internal parser slot
}

SerialWorker::~SerialWorker()
{
    // TODO: Close port if open before destruction
}
