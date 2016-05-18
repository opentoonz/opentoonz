#pragma once

#ifndef PLUGIN_PORT_INTERFACE
#define PLUGIN_PORT_INTERFACE

#include "toonz_hostif.h"

int is_connected(toonz_port_handle_t port, int *is_connected);
int get_fx(toonz_port_handle_t port, toonz_fxnode_handle_t *fxnode);

#endif
