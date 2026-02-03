#include "plugin_tile_interface.h"
#include "ttile.h"

int tile_interface_get_raw_address_unsafe(toonz_tile_handle_t handle,
                                          void **address) {
  if (!handle || !address) return -1;  // inval
  TTile *tile = reinterpret_cast<TTile *>(handle);

  tile->getRaster()->lock();
  *address = tile->getRaster()->getRawData();

  return 0;
}

int tile_interface_safen(toonz_tile_handle_t handle) {
  if (!handle) {
    return -1;  // inval
  }

  TTile *tile = reinterpret_cast<TTile *>(handle);
  tile->getRaster()->unlock();

  return 0;
}

int tile_interface_get_raw_stride(toonz_tile_handle_t handle, int *stride) {
  if (!handle || !stride) return -1;  // inval
  TTile *tile = reinterpret_cast<TTile *>(handle);

  //	return stride in byte, not in pixel
  *stride = tile->getRaster()->getWrap() * tile->getRaster()->getPixelSize();
  return 0;
}

int tile_interface_get_element_type(toonz_tile_handle_t handle, int *element) {
  if (!handle || !element) return -1;  // inval
  TTile *tile     = reinterpret_cast<TTile *>(handle);
  TRasterP raster = tile->getRaster();

  if (false)
    ;
  else if (TRaster32P(raster))
    *element = TOONZ_TILE_TYPE_32P;
  else if (TRaster64P(raster))
    *element = TOONZ_TILE_TYPE_64P;
  else if (TRasterGR8P(raster))
    *element = TOONZ_TILE_TYPE_GR8P;
  else if (TRasterGR16P(raster))
    *element = TOONZ_TILE_TYPE_GR16P;
  else if (TRasterGRDP(raster))
    *element = TOONZ_TILE_TYPE_GRDP;
  else if (TRasterYUV422P(raster))
    *element = TOONZ_TILE_TYPE_YUV422P;
  else {
    *element = TOONZ_TILE_TYPE_NONE;
    return -1;
  }

  return 0;
}

int tile_interface_copy_rect(toonz_tile_handle_t handle, int left, int top,
                             int width, int height, void *dst, int dststride) {
  if (!handle || !dst || !dststride) return -1;  // inval
  if (!width || !height) return 0;               // do nothing

  TTile *tile     = reinterpret_cast<TTile *>(handle);
  TRasterP raster = tile->getRaster();

  if (!(0 <= left && left + width <= raster->getLx() && 0 <= top &&
        top + height <= raster->getLy())) {
    return -1;
  }

  for (int y = 0; y < height; y++) {
    memcpy((UCHAR *)dst + y * dststride, raster->getRawData(left, top + y),
           width * raster->getPixelSize());
  }

  return 0;
}

int tile_interface_create_from(toonz_tile_handle_t handle,
                               toonz_tile_handle_t *newhandle) {
  if (!handle || !newhandle) return -1;  // inval
  TTile *tile    = reinterpret_cast<TTile *>(handle);
  TTile *newtile = new TTile(tile->getRaster());
  *newhandle     = reinterpret_cast<toonz_tile_handle_t *>(newtile);

  return 0;
}

int tile_interface_create(toonz_tile_handle_t *newhandle) {
  if (!newhandle) return -1;  // inval
  *newhandle = reinterpret_cast<toonz_tile_handle_t *>(new TTile());

  return 0;
}

int tile_interface_destroy(toonz_tile_handle_t handle) {
  if (!handle) return -1;  // inval
  TTile *tile = reinterpret_cast<TTile *>(handle);

  delete tile;

  return 0;
}

int tile_interface_get_rectangle(toonz_tile_handle_t handle,
                                 toonz_rect_t *rect) {
  if (!handle || !rect) {
    return -1;  // inval
  }

  TTile *tile = reinterpret_cast<TTile *>(handle);
  rect->x0    = tile->m_pos.x;
  rect->y0    = tile->m_pos.y;
  rect->x1    = tile->m_pos.x + tile->getRaster()->getLx();
  rect->y1    = tile->m_pos.y + tile->getRaster()->getLy();
  return 0;
}
