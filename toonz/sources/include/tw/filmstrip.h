#pragma once

#ifndef FILMSTRIP_INCLUDED
#define FILMSTRIP_INCLUDED

#include "tw/tw.h"
//#include "tw/scrollbar.h"
#include "tlevel_io.h"
#include "tthread.h"

//=========================================================
// forward declaration

class TScrollbar;

//=========================================================

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TFilmStripPanel : public TWidget {
protected:
  TPoint m_lastPos;
  int m_offset;
  TScrollbar *m_scrollbar;
  TLevelP m_level;

  TUINT32 m_loaderId;
  TLevelReaderP m_levelReader;

  bool m_loaded;
  TThread::Mutex m_mutex;

  friend class FilmStripLoader;

public:
  static const int m_xsize;
  static const int m_ysize;
  static const int m_frameHeight;

public:
  TFilmStripPanel(TWidget *parent, string name = "FilmStripPanel");
  ~TFilmStripPanel();

  void repaint();
  void draw();

  void leftButtonDown(const TMouseEvent &);
  void leftButtonDrag(const TMouseEvent &);
  void leftButtonUp(const TMouseEvent &);

  void onDrop(string s);
  void onTimer(int);

  void setScrollbar(TScrollbar *sb);

  void setLevel(TLevelP level);
  TLevelP getLevel() { return m_level; };
  void onScrollbarMove(int value);

  void updateScrollbar();
};

class DVAPI TFilmStrip : public TWidget {
  TFilmStripPanel *m_panel;
  TScrollbar *m_scrollbar;

public:
  TFilmStrip(TWidget *parent, string name = "FilmStrip");

  void configureNotify(const TDimension &d);
  void drop(string s) { m_panel->onDrop(s); };
};

#endif
