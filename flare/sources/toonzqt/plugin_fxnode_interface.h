#pragma once

#ifndef PLUGIN_FXNODE_INTERFACE
#define PLUGIN_FXNODE_INTERFACE

#include "toonz_hostif.h"

int fxnode_get_bbox(toonz_fxnode_handle_t fxnode,
                    const toonz_rendering_setting_t *, double frame,
                    toonz_rect_t *rect, int *ret);
int fxnode_can_handle(toonz_fxnode_handle_t fxnode,
                      const toonz_rendering_setting_t *, double frame,
                      int *ret);
int fxnode_get_input_port_count(toonz_fxnode_handle_t fxnode, int *count);
int fxnode_get_input_port(toonz_fxnode_handle_t fxnode, int index,
                          toonz_port_handle_t *port);
int fxnode_compute_to_tile(toonz_fxnode_handle_t fxnode,
                           const toonz_rendering_setting_t *rendering_setting,
                           double frame, const toonz_rect_t *rect,
                           toonz_tile_handle_t intile,
                           toonz_tile_handle_t tile);

#endif
