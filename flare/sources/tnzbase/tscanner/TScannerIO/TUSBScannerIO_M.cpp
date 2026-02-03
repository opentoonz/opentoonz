

#include "TUSBScannerIO.h"
#include "tsystem.h"
#include <iostream>
#include <sstream>

//#define HAS_LIBUSB

#if defined(HAS_LIBUSB)
#include <USB.h>
#endif

#include <errno.h>
using namespace std;
class TUSBScannerIOPD {
public:
  TUSBScannerIOPD();
  struct usb_device *m_epson;
  struct usb_dev_handle *m_handle;
  int m_epR;
  int m_epW;
  bool m_trace;
};

static pthread_t T = 0;
//#define TRACE cout << __PRETTY_FUNCTION__ << endl;
#define TRACE

TUSBScannerIOPD::TUSBScannerIOPD()
    : m_epson(0), m_handle(0), m_epR(0), m_epW(0), m_trace(false) {
  /*initialize libusb*/
  TRACE

  if (T == 0) T = pthread_self();

#if defined(HAS_LIBUSB)
  usb_set_debug(9);

  usb_init();

  usb_find_busses();
  usb_find_devices();
#endif
}

namespace {
void buf2printable(const unsigned char *buffer, const int size,
                   stringstream &os) {
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

TUSBScannerIO::TUSBScannerIO() : m_data(new TUSBScannerIOPD()) { TRACE }

//-----------------------------------------------------------------------------
namespace {

#if defined(HAS_LIBUSB)
// looking for an Epson device
struct usb_device *doCheck(struct usb_device *dev, int level) {
  if (!dev) return 0;

  cout << "checking idVendor = " << std::hex << dev->descriptor.idVendor
       << endl;
  if (dev->descriptor.idVendor == 0x04b8) return dev;
  for (int i = 0; i < dev->num_children; i++)
    if (doCheck(dev->children[i], level + 1)) return dev->children[i];
  return 0;
}

#endif
};

bool TUSBScannerIO::open() {
#if defined(HAS_LIBUSB)
  for (struct usb_bus *bus = usb_busses; bus; bus = bus->next) {
    if (bus->root_dev) {
      m_data->m_epson = doCheck(bus->root_dev, 0);
      if (m_data->m_epson) break;
    } else {
      struct usb_device *dev;
      for (dev = bus->devices; dev; dev = dev->next)
        m_data->m_epson = doCheck(dev, 0);
      if (m_data->m_epson) break;
    }
  }
  if (!m_data->m_epson) return false;
  cout << "found" << endl;
  m_data->m_handle = usb_open(m_data->m_epson);
  if (!m_data->m_handle) return false;
  cout << "opened" << endl;
  m_data->m_epR = m_data->m_epson->config[0]
                      .interface[0]
                      .altsetting[0]
                      .endpoint[0]
                      .bEndpointAddress;
  m_data->m_epW = m_data->m_epson->config[0]
                      .interface[0]
                      .altsetting[0]
                      .endpoint[1]
                      .bEndpointAddress;

  int rc;
  rc = usb_set_configuration(m_data->m_handle,
                             m_data->m_epson->config[0].bConfigurationValue);
  cout << "rc (config) = " << rc << endl;
  int ifc = 0;
  rc      = usb_claim_interface(m_data->m_handle, ifc);
  if ((rc == -EBUSY) || (rc == -ENOMEM)) return false;
  cout << "rc (claim) = " << rc << endl;
  cout << "gotit!" << endl;
  return true;
#else
  return false;
#endif
}

//-----------------------------------------------------------------------------

void TUSBScannerIO::close() {
  TRACE
#if defined(HAS_LIBUSB)
  if (m_data->m_handle) {
    usb_release_interface(m_data->m_handle, 0);
    usb_close(m_data->m_handle);
  }
  m_data->m_handle = 0;
  m_data->m_epson  = 0;
#endif
}

//-----------------------------------------------------------------------------

int TUSBScannerIO::receive(unsigned char *buffer, int size) {
  TRACE

#if defined(HAS_LIBUSB)

  int count;
  count = usb_bulk_read(m_data->m_handle, m_data->m_epR, (char *)buffer, size,
                        30 * 1000);

  if (m_data->m_trace) {
    stringstream os;
    os << "receive: size=" << size << " got = " << count << " buf=";
    buf2printable(buffer, count, os);
    os << '\n' << '\0';
    TSystem::outputDebug(os.str());
  }
  return count;

#else
  return 0;
#endif
}

//-----------------------------------------------------------------------------

int TUSBScannerIO::send(unsigned char *buffer, int size) {
  TRACE

#if defined(HAS_LIBUSB)
  int count;
  if (T != pthread_self()) {
    cout << "called from another thead" << endl;
    return 0;
  }

  count = usb_bulk_write(m_data->m_handle, m_data->m_epW, (char *)buffer, size,
                         30 * 1000);
  if (m_data->m_trace) {
    stringstream os;
    os << "send: size=" << size << " wrote = " << count << " buf=";
    buf2printable(buffer, size, os);
    os << '\n' << '\0';
    TSystem::outputDebug(os.str());
  }
  return count;
#else
  return 0;
#endif
}

//-----------------------------------------------------------------------------

void TUSBScannerIO::trace(bool on) {
  TRACE
  m_data->m_trace = on;
}

//-----------------------------------------------------------------------------

TUSBScannerIO::~TUSBScannerIO() { TRACE }
