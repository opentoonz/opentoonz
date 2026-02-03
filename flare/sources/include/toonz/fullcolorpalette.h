#pragma once

#ifndef FULLCOLOR_PALETTE
#define FULLCOLOR_PALETTE

#include <QObject>
#include "tcommon.h"
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
class TPalette;
class ToonzScene;

class DVAPI FullColorPalette final : public QObject {
  Q_OBJECT

  TPalette *m_palette;
  const TFilePath m_fullcolorPalettePath;

  FullColorPalette();

public:
  static FullColorPalette *instance();
  ~FullColorPalette();
  void clear();
  TPalette *getPalette(ToonzScene *scene);
  void savePalette(ToonzScene *scene);
  bool isFullColorPalette(TPalette *palette) { return m_palette == palette; }
  const TFilePath getPath() const { return m_fullcolorPalettePath; }
};

#endif  // FULLCOLOR_PALETTE
