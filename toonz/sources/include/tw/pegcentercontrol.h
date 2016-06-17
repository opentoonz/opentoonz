#pragma once

#ifndef PEGCENTERCONTROL_INCLUDED
#define PEGCENTERCONTROL_INCLUDED

#include "tw/tw.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TPegCenterControl : public TWidget {
  TUINT32 m_status, m_oldStatus;
  int m_clickPos;
  bool m_dragging;

protected:
public:
  TPegCenterControl(TWidget *parent, string name = "canvas");
  ~TPegCenterControl();

  virtual void draw();

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDrag(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);

  void configureNotify(const TDimension &d);

  void setStatus(TUINT32 status);
  TUINT32 getStatus() const;
};

#endif
