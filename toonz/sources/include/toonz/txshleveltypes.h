#pragma once

#ifndef TXSHLEVELTYPES_INCLUDED
#define TXSHLEVELTYPES_INCLUDED

enum TXshLevelType {
  UNKNOWN_XSHLEVEL     = 0x0,  // Unknown level type
  NO_XSHLEVEL          = 0x1,  // Empty level type (as in, empty xsheet cells)
  LEVELCOLUMN_XSHLEVEL = 0x2,  // Xsheet-stackable image level type

  TZI_TYPE   = 0x4,
  PLI_TYPE   = 0x8,
  TZP_TYPE   = 0x10,
  OVL_TYPE   = 0x20,
  CHILD_TYPE = 0x40,

  FULLCOLOR_TYPE = TZI_TYPE | OVL_TYPE,
  RASTER_TYPE    = FULLCOLOR_TYPE | TZP_TYPE,

  TZI_XSHLEVEL   = TZI_TYPE | LEVELCOLUMN_XSHLEVEL,  // Scan level
  PLI_XSHLEVEL   = PLI_TYPE | LEVELCOLUMN_XSHLEVEL,  // ToonzVector
  TZP_XSHLEVEL   = TZP_TYPE | LEVELCOLUMN_XSHLEVEL,  // ToonzRaster
  OVL_XSHLEVEL   = OVL_TYPE | LEVELCOLUMN_XSHLEVEL,  // Raster
  CHILD_XSHLEVEL = CHILD_TYPE | LEVELCOLUMN_XSHLEVEL,

  ZERARYFX_XSHLEVEL = 1 << 7,
  PLT_XSHLEVEL      = 2 << 7,
  SND_XSHLEVEL      = 3 << 7,
  SND_TXT_XSHLEVEL  = 4 << 7,
  MESH_XSHLEVEL     = 5 << 7
};

#endif
