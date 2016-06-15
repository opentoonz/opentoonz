#pragma once

#ifndef TW_INCLUDED
#define TW_INCLUDED

#include "traster.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TMouseEvent;
class TCursor;
class TDragDropListener;

//-------------------------------------------------------------

class DVAPI TGuiColor {
  int m_red, m_green, m_blue;
  // mutable void* m_imp;
  // bool  m_isStock;
  // int m_shade;

  // TUINT32 *m_index;
  // mutable bool m_allocated;
public:
  // static const int m_maxShade;

  TGuiColor(int r, int g, int b);
  TGuiColor();
  ~TGuiColor();

  // TGuiColor(const TGuiColor&);
  // TGuiColor& operator=(const TGuiColor&);

  // TUINT32 getIndex(int shade=0) const;
  // TUINT32 getRGB(int shade=0) const;

  int red() const { return m_red; }
  int green() const { return m_green; }
  int blue() const { return m_blue; }

  void computeShade(int index, int &r, int &g, int &b) const;

  // void* imp() const {return m_imp;}
  // void imp(void* imp) const { m_imp = imp; }
  // bool isStock() const { return m_isStock; }

protected:
  void initStockColor();
};

//-------------------------------------------------------------

class TWidgetImp;

// class TTarget {};
// class TTimerTarget {};

class TWidget;
typedef std::vector<TWidget *> TWidgetList;

//-------------------------------------------------------------

class DVAPI TWidget
// : public TTarget, public TTimerTarget
{
public:
  enum Alignment { BEGIN, CENTER, END };
  enum FocusHandling { IGNORE_FOCUS, STOP_FOCUS, LISTEN_TO_FOCUS };

protected:
  TRect m_placement;
  int m_xoff, m_yoff;

  bool m_mapped;  // e' arrivato il MapNotify
  bool m_hidden;  // il widget non vuole mostrarsi
  bool m_childrenChanged;

  // TTimer *m_timer;

  TWidget *m_parent, *m_sonKeeper;
  std::string m_name, m_contextHelpReference;
  std::wstring m_shortHelp, m_longHelp;

  int m_cursorIdx;

  TWidgetList *m_sons;
  TWidgetImp *m_imp;

  friend class TWidgetImp;

  TGuiColor m_backgroundColor, m_foregroundColor;
  bool m_popupMode, m_menuMode, m_fixedSize, m_fullColor, m_openGL;
  bool m_enabled, m_menuRelated;

  TDragDropListener *m_dragDropListener;

  std::string m_fontName;
  int m_fontSize;
  int m_fontOrientation;

  static TWidget *m_selectionOwner;
  static TWidget *m_focusOwner;

  // not implemented (cannot copy and assign widget)
private:
  TWidget(const TWidget &);
  TWidget &operator=(const TWidget &);

protected:
  virtual void create();
  virtual void setDecoration();
  virtual void destroyImp();
  void createSons();

public:
  TWidget(TWidget *parent = 0, std::string name = "unnamed");
  virtual ~TWidget();

  //
  // geometry
  //
  const TRect &getGeometry() const { return m_placement; }
  TPoint getPosition() const { return m_placement.getP00(); }
  virtual TPoint getHotSpot() const { return TPoint(0, 0); }
  TDimension getSize() const { return m_placement.getSize(); }
  TRect getBounds() const { return TRect(TPoint(), getSize()); }
  int getLx() const { return m_placement.getLx(); }
  int getLy() const { return m_placement.getLy(); }

  inline void setPlacement(const TRect &rect) { m_placement = rect; }

  inline void setGeometry(int x0, int y0, int x1, int y1) {
    setGeometry(TRect(x0, y0, x1, y1));
  }

  virtual void setGeometry(const TRect &rect);
  void setGeometry(const TPoint &pos, const TDimension &size) {
    setGeometry(TRect(pos, size));
  }
  inline void setSize(int w, int h) {
    setGeometry(getPosition(), TDimension(w, h));
  }

  inline void setSize(const TDimension &size) { setSize(size.lx, size.ly); }
  inline void setPosition(int x, int y) {
    setGeometry(TPoint(x, y), getSize());
  }
  inline void setPosition(const TPoint &pt) { setGeometry(pt, getSize()); }
  inline bool contains(int x, int y) const { return contains(TPoint(x, y)); }
  inline bool contains(const TPoint &p) const {
    return m_placement.contains(p);
  }

  TPoint getScreenPosition() const;

  //
  // name, parent & sons
  //
  inline TWidget *getParent() const { return m_parent; }
  inline std::string getName() const { return m_name; }
  virtual void setParent(TWidget *p);
  virtual void addSon(TWidget *son);
  virtual void removeSon(TWidget *son);
  inline int getSonCount() const { return m_sons->size(); }
  inline TWidget *getSon(int i) const {
    return i < 0 || i >= (int)(m_sons->size()) ? 0 : (*m_sons)[i];
  }

  //
  // help messages
  //
  std::wstring getLongHelp() const;
  std::wstring getShortHelp() const;

  void setHelp(std::string shortHelp, std::string longHelp);
  void setHelp(std::wstring shortHelp, std::wstring longHelp);

  virtual std::string getContextHelpReference(const TPoint &) {
    return m_contextHelpReference;
  }

  void setContextHelpReference(std::string s) { m_contextHelpReference = s; }

  // void delegateHelpToParent();

  virtual std::wstring getTooltipString(const TPoint &) {
    return getShortHelp();
  }

  //
  // misc
  //
  inline bool isHidden() const { return m_hidden; }
  inline bool isMapped() const { return m_mapped; }

  void enable();
  void disable();
  inline bool isEnabled() const { return m_enabled; }

  void show();
  void hide();

  // n.b. non chiamare map/unmap, ma show/hide
  virtual void map();
  virtual void unmap();

  virtual void doModal();

  virtual void undock();
  virtual void dock(TWidget *parent);
  virtual TWidget *findSubWidget(const TPoint &pt);

  virtual void repaint();

  void sendRepaint() { sendRepaint(getBounds()); }
  void sendRepaint(const TRect &);

  virtual void paste();

  virtual void startDragAndDrop(std::string stringToDrop);

  typedef unsigned int CursorIndex;

  inline CursorIndex getCurrentCursor() { return m_cursorIdx; }
  void changeCursor(CursorIndex new_cur);

  TWidgetImp *getImp() { return m_imp; }

  void startTimer(TINT32 period);
  void stopTimer();

  virtual void getSelection();

  virtual void onFocusChange(bool on);

  virtual bool getFocus() { return false; }
  virtual void circulateFocus();

  virtual void copyRegion(const TPoint &dst, const TPoint &src,
                          const TDimension &size);

  //
  // draw
  //
  void flush();

  virtual void draw() {}
  virtual void invalidate();
  virtual void invalidate(const TRect &rect);

  void getClipRect(TRect &rect);

  virtual void clear();

  virtual void drawText(const TPoint &p, std::string text);
  virtual void drawText(const TPoint &p, std::wstring text);

  virtual void drawText(const TRect &r, std::string text,
                        Alignment alignment = CENTER);
  virtual void drawText(const TRect &r, std::wstring text,
                        Alignment alignment = CENTER);
  virtual void drawMultiLineText(const TRect &r, std::string text);
  virtual void drawMultiLineText(const TRect &r, std::wstring text);

  virtual TDimension getTextSize(std::string text);
  virtual TDimension getTextSize(std::wstring text);

  virtual TDimension getTextSize(std::string text, std::string font,
                                 int fontSize);
  virtual TDimension getTextSize(std::wstring text, std::string font,
                                 int fontSize);

  inline void drawPoint(int x, int y) { drawLine(x, y, x, y); }

  virtual void drawLine(const TPoint &p0, const TPoint &p1);
  inline void drawLine(int x0, int y0, int x1, int y1) {
    drawLine(TPoint(x0, y0), TPoint(x1, y1));
  }
  virtual void drawVLine(const TPoint &p0, int length) {
    drawLine(p0, p0 + TPoint(0, length - 1));
  }
  virtual void drawHLine(const TPoint &p0, int length) {
    drawLine(p0, p0 + TPoint(length - 1, 0));
  }

  virtual void drawDottedLine(const TPoint &p0, const TPoint &p1);
  inline void drawDottedLine(int x0, int y0, int x1, int y1) {
    drawDottedLine(TPoint(x0, y0), TPoint(x1, y1));
  }

  virtual void drawRect(const TRect &);
  virtual void fillRect(const TRect &);

  virtual void drawRect(int x0, int y0, int x1, int y1) {
    drawRect(TRect(x0, y0, x1, y1));
  }
  virtual void fillRect(int x0, int y0, int x1, int y1) {
    fillRect(TRect(x0, y0, x1, y1));
  }

  virtual void draw3DRect(const TRect &, bool pressed = false, int border = 1);
  virtual void fill3DRect(const TRect &, bool pressed = false, int border = 1);

  virtual void drawDottedRect(const TRect &);
  virtual void drawDottedRect(int x0, int y0, int x1, int y1) {
    drawDottedRect(TRect(x0, y0, x1, y1));
  }

  virtual void fillPolygon(const TGuiColor &fillColor,
                           const TGuiColor &borderColor, const TPoint points[],
                           int pointCount);

  virtual void clear(const TRect &);

  virtual void drawImage(int index, const TPoint &);

  virtual void setColor(const TGuiColor &, int shade = 0);
  virtual void setBackgroundColor(const TGuiColor &);

  TGuiColor getBackgroundColor() const { return m_backgroundColor; }
  TGuiColor getForegroundColor() const { return m_foregroundColor; }

  static const TGuiColor White;
  static const TGuiColor Black;

  virtual void rectwrite(const TRaster32P &, const TPoint &);

  virtual void setFontSize(int size);
  virtual void setFont(std::string name, int size = 0, int orientation = 0);

  //
  // events
  //

  virtual void enter(const TPoint &) {}
  virtual void leave(const TPoint &) {}

  virtual void mouseMove(const TMouseEvent &) {}
  virtual void leftButtonDown(const TMouseEvent &) {}
  virtual void leftButtonDrag(const TMouseEvent &) {}
  virtual void leftButtonUp(const TMouseEvent &) {}
  virtual void rightButtonDown(const TMouseEvent &) {}
  virtual void rightButtonDrag(const TMouseEvent &) {}
  virtual void rightButtonUp(const TMouseEvent &) {}
  virtual void middleButtonDown(const TMouseEvent &) {}
  virtual void middleButtonDrag(const TMouseEvent &) {}
  virtual void middleButtonUp(const TMouseEvent &) {}
  virtual void leftButtonDoubleClick(const TMouseEvent &) {}
  virtual void middleButtonDoubleClick(const TMouseEvent &) {}
  virtual void rightButtonDoubleClick(const TMouseEvent &) {}

  virtual void mouseWheel(const TMouseEvent &, int wheel) {}

  virtual void keyDown(int, TUINT32, const TPoint &) {}

  virtual void configureNotify(const TDimension &) {}

  virtual void onPaste(std::string) {}

  // virtual void close() {unmap();}

  virtual void onDrop(std::string) {}

  virtual std::string getToolTip() { return ""; }

  virtual void onTimer(int) {}

  virtual void onChildrenChanged() {}

  virtual bool onNcPaint(bool is_active, const TDimension &window_size,
                         const TRect &caption_rect);

public:
  static void setFocusOwner(TWidget *w);
  static TWidget *getFocusOwner();

  virtual FocusHandling getFocusHandling() const { return IGNORE_FOCUS; }

  static void placeInteractively(TRect &);

  // brutto brutto qui
  virtual void maximize(TWidget *) {}
  virtual bool isMaximized(TWidget *) { return false; }

  //
  // drag & drop
  //
  // virtual bool acceptDrop() {return false;}
  void enableDropTarget(TDragDropListener *dragDropListener);
  TDragDropListener *getDragDropListener() const { return m_dragDropListener; }

  static void yield();  // processa tutti gli eventi in attesa
  static void setMode(int mode);

  // misc
  TPoint getAbsolutePosition() const;
  static TPoint getMouseAbsolutePosition();
  TPoint getMouseRelativePosition() const;

  // brutto. serve per filtrare gli eventi durante il menu event loop
  bool isMenuRelated() const { return m_menuRelated; }

  // se non altro e' brutto il nome
  // NB: il nome e' brutto... ora questo e' anche sbagliato
  static void processAllPendingMessages();  // questa processa solo i WM_PAINT

  static void processAllPendingMessages2();  // questa processa tutto

  // mah? serve a evitare il circuito invalidate() -> repaint(); chiama
  // direttamente
  // repaint() (creando l'opportuno GC su windows)
  // void paintRegion(const TRect &rect);

  static TDimension getScreenSize();
  static void getScreenPlacement(std::vector<TRectI> &);
};

#endif
