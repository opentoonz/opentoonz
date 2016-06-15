#pragma once

#ifndef TNZ_STYLECONTROL_INCLUDED
#define TNZ_STYLECONTROL_INCLUDED

#include "tw/action.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TColorStyle;
class TPalette;
class TLevel;
class TFilePath;

class DVAPI TStyleControl : public TWidget, public TCommandSource {
  class Data;
  Data *m_data;

public:
  enum {
    COLOR_PAGE    = 0x01,
    TEXTURE_PAGE  = 0x02,
    SPECIAL_PAGE  = 0x04,
    CUSTOM_PAGE   = 0x08,
    SETTINGS_PAGE = 0x10,

    NO_PAGES  = 0x00,
    ALL_PAGES = 0x1F
  };

  enum { ALL_STYLES = 0x01, VECTOR_SYLES_ONLY = 0x02 };

  TStyleControl(TWidget *parent, const TFilePath &rootDir,
                int styleFilter = ALL_STYLES);
  ~TStyleControl();

  void setActivePages(unsigned long pageMask);

  void configureNotify(const TDimension &);
  void draw();

  void setStyle(TPalette *palette, int styleIndex);

  const TColorStyle *getCurrentStyle() const;
  int getCurrentStyleIndex() const;
  TPalette *getPalette() const;

  virtual void onChangeStyleParam(bool dragging) {}
  virtual void onChangeStyleNumericParam(int paramIndex, bool dragging) {
    onChangeStyleParam(dragging);
  }

  void refreshCurrentPage();

  void setAutoApply(bool on);
  bool getAutoApply() const;
};

#endif
