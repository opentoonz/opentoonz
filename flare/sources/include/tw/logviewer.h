#pragma once

#ifndef TNZ_LOGVIEWER_INCLUDED
#define TNZ_LOGVIEWER_INCLUDED

#include "tw/popup.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TLogViewer : public TWidget {
  class Data;
  Data *m_data;

public:
  TLogViewer(TWidget *parent, string name = "logViewer");
  ~TLogViewer();

  void configureNotify(const TDimension &);
  void draw();

  void activate(bool on);
};

class DVAPI TLogViewerPopup : public TPopup {
  TLogViewer *m_viewer;

public:
  TLogViewerPopup(TWidget *parent, string name = "logViewerPopup");
  ~TLogViewerPopup();

  void configureNotify(const TDimension &);
  TDimension getPreferredSize() const;
};

#endif
