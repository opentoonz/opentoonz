#pragma once

#ifndef PLUGIN_TILE_INTERFACE
#define PLUGIN_TILE_INTERFACE

#include "toonz_hostif.h"

int tile_interface_get_raw_address_unsafe(toonz_tile_handle_t handle,
                                          void **address);
int tile_interface_get_raw_stride(toonz_tile_handle_t handle, int *stride);
int tile_interface_get_element_type(toonz_tile_handle_t handle, int *element);
int tile_interface_copy_rect(toonz_tile_handle_t handle, int left, int top,
                             int width, int height, void *dst, int dststride);
int tile_interface_create_from(toonz_tile_handle_t handle,
                               toonz_tile_handle_t *newhandle);
int tile_interface_create(toonz_tile_handle_t *newhandle);
int tile_interface_destroy(toonz_tile_handle_t handle);
int tile_interface_get_rectangle(toonz_tile_handle_t handle, toonz_rect_t *);
int tile_interface_safen(toonz_tile_handle_t handle);

#endif
