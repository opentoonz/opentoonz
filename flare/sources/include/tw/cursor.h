#pragma once

#ifndef TNZ_CURSOR_INCLUDED
#define TNZ_CURSOR_INCLUDED

//#include "tfilepath.h"
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

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

// forward decls
class TFilePath;
class TMouse;
class TCursor;

class DVAPI TCursorFactory {
protected:
  TCursorFactory() {}

  // typedef vector<TCursor*> CursorList;
  // static CursorList m_cursorList;
  typedef TCursor *CursorList[100];
  static CursorList m_cursorList;

  static bool m_init;

public:
  enum {
    CURSOR_NONE,                   // no cursor...
    CURSOR_DEFAULT = CURSOR_NONE,  // window class cursor...
    CURSOR_ARROW,
    CURSOR_HAND,
    CURSOR_HOURGLASS,
    CURSOR_NO,
    //#ifdef WIN32
    CURSOR_DUMMY,
//#endif
#ifndef WIN32
    CURSOR_DND,
    CURSOR_QUESTION,
#endif
    // ....
    NUM_STOCK_CURSORS,
    LAST_STOCK_CURSOR = NUM_STOCK_CURSORS - 1
  };

  // public API
  static void defineCursor(TWidget::CursorIndex new_cursor_id,
                           const TFilePath &cursor_filename);
  static void defineCursorAs(TWidget::CursorIndex new_cursor_id,
                             TWidget::CursorIndex source);

protected:
  friend class TMouse;
  static TCursor *getCursor(TWidget::CursorIndex cursor_id);
  // PD methods
  static TCursor *loadCursorFromFile(const TFilePath &cursor_filename);
  static void init();
};

class DVAPI TMouse {
public:
  ~TMouse() {}

  // PD methods
  void pushCursor(TWidget::CursorIndex id);
  void popCursor();

  void setCursor(TWidget::CursorIndex id);

  enum CursorMode { MODE_NORMAL, MODE_HOURGLASS };
  void setCursorMode(CursorMode mode);
  // CursorMode getCursorMode();

private:
  void setCursor(TCursor *);
  TWidget::CursorIndex m_currentCursorId;
  // to enforce singleton...
  TMouse();

protected:
  CursorMode m_cursorMode;

public:
  static TMouse theMouse;
};

class DVAPI THourglassCursor {
public:
  THourglassCursor() { TMouse::theMouse.setCursorMode(TMouse::MODE_HOURGLASS); }
  ~THourglassCursor() { TMouse::theMouse.setCursorMode(TMouse::MODE_NORMAL); }
};

#ifdef WIN32
#pragma warning(pop)
#endif

#endif
