#pragma once

#ifndef IMAGE_GROUPING_INCLUDED
#define IMAGE_GROUPING_INCLUDED

#include "tcommon.h"
#include <QObject>

class StrokeSelection;
class QMenu;

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TGroupCommand final : public QObject {
  Q_OBJECT
public:
  enum {
    NONE     = 0,
    FRONT    = 1,
    FORWARD  = 2,
    BACKWARD = 4,
    BACK     = 8,
    GROUP    = 16,
    UNGROUP  = 32
  };

  StrokeSelection *m_sel;

  TGroupCommand() : m_sel(0) {}

  void setSelection(StrokeSelection *sel) { m_sel = sel; }
  UCHAR getGroupingOptions();

  void addMenuItems(QMenu *menu);

  void back() {
    if (!(getGroupingOptions() & BACK)) return;
    moveGroup(BACK);
  }
  void backward() {
    if (!(getGroupingOptions() & BACKWARD)) return;
    moveGroup(BACKWARD);
  }
  void front() {
    if (!(getGroupingOptions() & FRONT)) return;
    moveGroup(FRONT);
  }
  void forward() {
    if (!(getGroupingOptions() & FORWARD)) return;
    moveGroup(FORWARD);
  }
  void group();
  void ungroup();
  void enterGroup();
  void exitGroup();

private:
  void moveGroup(UCHAR moveType);
};

#endif
