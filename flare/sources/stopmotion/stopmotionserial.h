#pragma once

#ifndef STOPMOTIONSERIAL_H
#define STOPMOTIONSERIAL_H

#include <QObject>

class QSerialPort;

//=============================================================================
// StopMotionSerial
//-----------------------------------------------------------------------------

class StopMotionSerial : public QObject {
  Q_OBJECT

public:
  StopMotionSerial();
  ~StopMotionSerial();

  QStringList m_controlBaudRates, m_serialDevices;

  QSerialPort* m_serialPort;

  // motion control
  QStringList getAvailableSerialPorts();
  bool setSerialPort(QString port);
  void sendSerialData();
};
#endif  // STOPMOTIONSERIAL_H