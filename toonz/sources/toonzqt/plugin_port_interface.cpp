#include "trasterfx.h"
#include "plugin_port_interface.h"

int is_connected(toonz_port_handle_t port, int *is_connected) {
  if (!port) {
    return TOONZ_ERROR_INVALID_HANDLE;
  }
  if (!is_connected) {
    return TOONZ_ERROR_NULL;
  }
  *is_connected = reinterpret_cast<TRasterFxPort *>(port)->isConnected();
  return TOONZ_OK;
}

int get_fx(toonz_port_handle_t port, toonz_fxnode_handle_t *fxnode) {
  if (!port) {
    return TOONZ_ERROR_INVALID_HANDLE;
  }
  if (!fxnode) {
    return TOONZ_ERROR_NULL;
  }
  *fxnode = reinterpret_cast<TRasterFxPort *>(port)->getFx();
  return TOONZ_OK;
}
