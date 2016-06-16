#pragma once

//=========================================================
//=========================================================
//=========================================================
//
// OBSOLETO: eliminare
//
//=========================================================
//=========================================================
//=========================================================

#ifdef OBSOLETO

#ifndef T_IMAGELOCATION_INCLUDED
#define T_IMAGELOCATION_INCLUDED

#include "tfilepath.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TStageObjectSpline;

//
// TImageLocation
//
class DVAPI TImageLocation {
public:
  int m_row, m_col;
  TFrameId m_fid;
  // TStageObjectId m_pid;
  wstring m_levelName;
  TStageObjectSpline *m_spline;

  enum Type { None, XsheetImage, LevelStripImage, PathImage };

  Type m_type;

  TImageLocation();
  TImageLocation(int row, int col);  // XsheetImage
  // TImageLocation(const TStageObjectId &pid); // PathImage
  TImageLocation(TStageObjectSpline *spline);  // PathImage
  TImageLocation(std::wstring levelName,
                 const TFrameId &fid);  // LevelStripImage
};

#endif
#endif
