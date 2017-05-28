

#include "TUSBScannerIO.h"
#include "tsystem.h"

#include <windows.h>

#include <sstream>

class TUSBScannerIOPD {
public:
  TUSBScannerIOPD() : m_handle(INVALID_HANDLE_VALUE), m_trace(false) {}
  HANDLE m_handle;
  bool m_trace;
};

//-----------------------------------------------------------------------------
namespace {
void buf2printable(const unsigned char *buffer, const int size,
                   std::stringstream &os) {
  int i = 0;
  if ((size == 2) && (buffer[0] == 0x1b)) {
    os << "ESC ";
    char c = buffer[1];
    if (isprint(c)) os << c << " ";
    return;
  }
  os << std::hex;
  for (; i < std::min(size, 0x40); ++i) {
    char c = buffer[i];
    os << "0x" << (unsigned int)c << " ";
  }
  if (i < size) os << "...";
}
}

//-----------------------------------------------------------------------------

TUSBScannerIO::TUSBScannerIO() : m_data(new TUSBScannerIOPD()) {}

//-----------------------------------------------------------------------------

bool TUSBScannerIO::open() {
  m_data->m_handle = CreateFile("\\\\.\\usbscan0", GENERIC_WRITE | GENERIC_READ,
                                FILE_SHARE_WRITE | FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, 0, NULL);
  if (m_data->m_handle == INVALID_HANDLE_VALUE) return false;
  return true;
}

//-----------------------------------------------------------------------------

void TUSBScannerIO::close() {
  if (m_data->m_handle && (m_data->m_handle != INVALID_HANDLE_VALUE))
    CloseHandle(m_data->m_handle);
}

//-----------------------------------------------------------------------------

int TUSBScannerIO::receive(unsigned char *buffer, int size) {
  int bytesLeft      = size;
  unsigned char *ptr = buffer;
  DWORD count;

  static int m_maxReadSize = 0x10000;

  do {
    int bytesToRead = bytesLeft;
    notMoreThan<int>(m_maxReadSize, bytesToRead);

    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(OVERLAPPED));
    overlapped.hEvent = CreateEvent(NULL,  // pointertosecurityattributes,
                                    // WIN95ignoresthisparameter
                                    FALSE,  // automaticreset
                                    FALSE,  // initializetonotsignaled
                                    NULL);  // pointertotheevent-objectname

    ReadFile(m_data->m_handle, ptr, bytesToRead, &count, &overlapped);
    DWORD waitRC = WaitForSingleObject(overlapped.hEvent, INFINITE);
    if (m_data->m_trace) {
      std::stringstream os;
      os << "receive: size=" << size << " got = " << count << " buf=";
      buf2printable(ptr, count, os);
      os << '\n' << '\0';
      TSystem::outputDebug(os.str());
    }

    if (count != bytesToRead) return 0;

    ptr += count;
    bytesLeft = bytesLeft - count;
  } while (bytesLeft);

  return size;
}

//-----------------------------------------------------------------------------

int TUSBScannerIO::send(unsigned char *buffer, int size) {
  int bytesLeft = size;
  DWORD count;
  static int m_maxWriteSize = 64;
  // bytesLeft = 64;
  do {
    int bytesToWrite = bytesLeft;
    notMoreThan<int>(m_maxWriteSize, bytesToWrite);
    WriteFile(m_data->m_handle, buffer, bytesToWrite, &count, 0);

    if (m_data->m_trace) {
      std::stringstream os;
      os << "send: size=" << size << " wrote = " << count << " buf=";
      buf2printable(buffer, size, os);
      os << '\n' << '\0';
      TSystem::outputDebug(os.str());
    }

    if (count != bytesToWrite) return 0;

    // ptr += count;
    bytesLeft = bytesLeft - count;
  } while (bytesLeft);

  return size;
}

//-----------------------------------------------------------------------------

void TUSBScannerIO::trace(bool on) { m_data->m_trace = on; }

//-----------------------------------------------------------------------------

TUSBScannerIO::~TUSBScannerIO() {}
