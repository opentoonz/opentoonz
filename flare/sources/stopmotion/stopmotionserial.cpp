#include "stopmotionserial.h"
#include "stopmotion.h"
#include <QSerialPort>
#include <QSerialPortInfo>
//=============================================================================
//=============================================================================

StopMotionSerial::StopMotionSerial() { m_serialPort = new QSerialPort(this); }
StopMotionSerial::~StopMotionSerial() {}

//-----------------------------------------------------------------

QStringList StopMotionSerial::getAvailableSerialPorts() {
  QList<QSerialPortInfo> list = QSerialPortInfo::availablePorts();
  QStringList ports;
  ports.push_back(tr("No Device"));
  int size = list.size();
  for (int i = 0; i < size; i++) {
    std::string desc  = list.at(i).description().toStdString();
    std::string port  = list.at(i).portName().toStdString();
    std::string maker = list.at(i).manufacturer().toStdString();
    ports.push_back(list.at(i).portName());
    if (list.at(i).hasProductIdentifier()) {
      std::string id =
          QString::number(list.at(i).productIdentifier()).toStdString();
    }
    if (list.at(i).hasVendorIdentifier()) {
      std::string id =
          QString::number(list.at(i).vendorIdentifier()).toStdString();
    }
  }
  return ports;
}

//-----------------------------------------------------------------

bool StopMotionSerial::setSerialPort(QString port) {
  if (m_serialPort->isOpen()) m_serialPort->close();
  QList<QSerialPortInfo> list = QSerialPortInfo::availablePorts();
  QStringList ports;
  bool success = false;
  int size     = list.size();
  for (int i = 0; i < size; i++) {
    if (port == list.at(i).portName()) {
      m_serialPort->setPort(list.at(i));
    }
  }
  // m_serialPort->setBaudRate(9600);
  bool connected = m_serialPort->open(QIODevice::ReadWrite);
  if (connected) {
    success = m_serialPort->setBaudRate(QSerialPort::Baud9600) &
              m_serialPort->setParity(QSerialPort::NoParity) &
              m_serialPort->setDataBits(QSerialPort::Data8) &
              m_serialPort->setStopBits(QSerialPort::OneStop) &
              m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
    std::string fNumber =
        std::to_string(StopMotion::instance()->getXSheetFrameNumber());
    char const* fNumberChar = fNumber.c_str();
    m_serialPort->write(fNumberChar);
    m_serialPort->waitForBytesWritten(10);
    QByteArray DataReceive = m_serialPort->readLine();
    while (m_serialPort->waitForReadyRead(30)) {
      DataReceive += m_serialPort->readLine();
    }
    QByteArray response = DataReceive;
    QString s(response);
    std::string text = response.toStdString();
    //}
  }
  return success;
}

//-----------------------------------------------------------------

void StopMotionSerial::sendSerialData() {
  if (m_serialPort->isOpen()) {
    std::string fNumber =
        std::to_string(StopMotion::instance()->getXSheetFrameNumber());
    char const* fNumberChar = fNumber.c_str();
    m_serialPort->write(fNumberChar);
    m_serialPort->waitForBytesWritten(10);
    QByteArray DataReceive = m_serialPort->readLine();
    while (m_serialPort->waitForReadyRead(30)) {
      DataReceive += m_serialPort->readLine();
    }
    QByteArray response = DataReceive;
    QString s(response);
    std::string text = response.toStdString();
  }

  // for not data sending is not implemented yet, just the frame number.
  // These lines are here to be a reference for using column data as movement.
  // TDoubleParam *param =
  // TApp::instance()->getCurrentScene()->getScene()->getXsheet()->getStageObjectTree()->getStageObject(0)->getParam(TStageObject::T_X);
  // double value =
  // TApp::instance()->getCurrentScene()->getScene()->getXsheet()->getStageObjectTree()->getStageObject(0)->getParam(TStageObject::T_X,
  // m_xSheetFrameNumber - 1);
  // QString isCam =
  // TApp::instance()->getCurrentScene()->getScene()->getXsheet()->getStageObjectTree()->getStageObject(0)->getId().isCamera()
  // ? "yep" : "nope";
  // std::string name =
  // TApp::instance()->getCurrentScene()->getScene()->getXsheet()->getStageObjectTree()->getStageObject(0)->getName();
  // TMeasure *measure =param->getMeasure();
  // const TUnit *unit = measure->getCurrentUnit();
  // double newValue = unit->convertTo(value);
}
