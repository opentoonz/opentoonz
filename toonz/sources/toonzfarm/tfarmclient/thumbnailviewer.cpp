

#include "thumbnailviewer.h"
#include "thumbnail.h"
#include "tfilepath.h"
#include "tsystem.h"

#include "tw/textfield.h"
#include "tw/colors.h"
#include "tw/event.h"
#include "tw/dragdrop.h"
#include "tw/keycodes.h"
#include "tw/mainshell.h"

#include "tw/message.h"

using namespace TwConsts;

//===================================================================

class ThumbnailViewer::NameField : public TTextField {
  Thumbnail *m_thumbnail;

public:
  NameField(TWidget *parent) : TTextField(parent), m_thumbnail(0) {}
  void onFocusChange(bool on) {
    if (!on) close();
  }

  void close() {
    if (m_thumbnail) {
      try {
        m_thumbnail->setName(toString(getText()));
      } catch (...) {
        TMessage::error("unable to change name");
      }
    }
    hide();
  }

  void keyDown(int key, unsigned long flags, const TPoint &p) {
    if (key == TK_Return)
      close();
    else
      TTextField::keyDown(key, flags, p);
  }

  void setThumbnail(Thumbnail *thumbnail) {
    m_thumbnail = thumbnail;
    setText(m_thumbnail->getName());
    selectAll();
  }
};

//===================================================================

ThumbnailViewer::ThumbnailViewer(TWidget *parent, string name)
    : TScrollView(parent, name)
    , m_selectedItemIndex(-1)
    , m_itemSize(94, 100)
    , m_itemSpace(10, 10)
    , m_margins(10, 10)
    , m_playButtonBox(TPoint(85, 2), TDimension(10, 10))
    , m_iconBox(TPoint(2 + 4 - 3, 20 + 4), TDimension(96 - 8, 78 - 8))
    , m_textBox(TPoint(2 + 4 + 2 + 10, 2 + 2), TDimension(65, 18))
    , m_flag(false)
    , m_playing(false)
    , m_loading(false)
    , m_timerActive(false)
    , m_dragDropArmed(false)
    , m_oldPos(0, 0) {
  setBackgroundColor(White);
  m_nameField = new NameField(this);
  m_nameField->hide();
}

//-------------------------------------------------------------------

ThumbnailViewer::~ThumbnailViewer() { clearPointerContainer(m_items); }

//-------------------------------------------------------------------

int ThumbnailViewer::getColumnCount() const {
  int columnWidth    = m_itemSize.lx + m_itemSpace.lx;
  int availableWidth = getLx() - m_margins.lx * 2 + m_itemSpace.lx;
  return tmax(1, (availableWidth) / columnWidth);
}

//-------------------------------------------------------------------

TPoint ThumbnailViewer::getItemPos(int index) {
  int columnCount = getColumnCount();
  int row         = index / columnCount;
  int col         = index % columnCount;
  int x           = m_margins.lx + col * (m_itemSize.lx + m_itemSpace.lx);
  int y           = getLy() - m_margins.ly - m_itemSize.ly -
          row * (m_itemSize.ly + m_itemSpace.ly);
  return TPoint(x, y);
}

//-------------------------------------------------------------------

TRect ThumbnailViewer::getItemBounds(int index) {
  return TRect(getItemPos(index), m_itemSize);
}

//-------------------------------------------------------------------

int ThumbnailViewer::findItem(const TPoint &pos) {
  int columnCount = getColumnCount();
  int col         = (pos.x - m_margins.lx) / (m_itemSize.lx + m_itemSpace.lx);
  int row         = (getLy() - m_margins.ly - m_itemSpace.ly - pos.y) /
            (m_itemSize.ly + m_itemSpace.ly);
  int index = row * columnCount + col;
  if (0 <= index && index < (int)m_items.size() &&
      getItemBounds(index).contains(pos))
    return index;
  else
    return -1;
}

//-------------------------------------------------------------------

int ThumbnailViewer::getItemCount() const { return m_items.size(); }

//-------------------------------------------------------------------

Thumbnail *ThumbnailViewer::getItem(int index) const {
  assert(0 <= index && index < (int)m_items.size());
  return m_items[index];
}

//-------------------------------------------------------------------

void ThumbnailViewer::configureNotify(const TDimension &d) {
  TScrollView::configureNotify(d);
  // m_nameField->setGeometry(TPoint(10,10), TDimension(100,30));
  updateContentSize();
  invalidate();
}

//-------------------------------------------------------------------

void ThumbnailViewer::drawFrame(int index) {
  TPoint base = getItemPos(index);
  assert(0 <= index && index < (int)m_items.size());
  assert(m_items[index]->getRaster());
  assert(m_iconBox.getSize() == m_items[index]->getRaster()->getSize());
  rectwrite(m_items[index]->getRaster(), base + m_iconBox.getP00());
}

//-------------------------------------------------------------------

void ThumbnailViewer::drawItem(int index) {
  TPoint base = getItemPos(index);
  if (index < 0 || index >= (int)m_items.size()) {
    setColor(getBackgroundColor());
    fillRect(TRect(base, m_itemSize));
    return;
  }

  // setColor(Black);
  // fillRect(TRect(base, m_itemSize));

  Thumbnail *item = m_items[index];
  bool selected   = item != 0 && index == m_selectedItemIndex;

  TRect r0(base, m_itemSize);
  TRect r1 = m_iconBox + base;
  TRect r2 = m_textBox + base;

  setColor(selected ? Cyan : getBackgroundColor());
  fillRect(r0.x0, r1.y0, r1.x0 - 1, r1.y1);  // sinistra
  fillRect(r1.x1 + 1, r1.y0, r0.x1, r1.y1);  // destra
  fillRect(r0.x0, r0.y0, r0.x1, r1.y0 - 1);  // sotto
  // fillRect(r1.x0,r1.y1+1,r1.x1,r0.y1); // sopra
  TRect topLine(r0.x0, r1.y1 + 1, r0.x1, r0.y1);
  TRect sceneLine = topLine;
  sceneLine.y0    = sceneLine.y1 - 3;
  topLine.y1      = sceneLine.y0 - 1;
  fillRect(topLine);

  if (item->getType() == Thumbnail::SCENE) {
    setColor(Black);
    drawRect(sceneLine);
    setColor(White);
    fillRect(sceneLine.enlarge(-1));
    int x;
    setColor(Black);
    for (x = sceneLine.x0 + 5; x + 5 < sceneLine.x1; x += 10) {
      int y = sceneLine.y0 + 1;
      drawLine(x, y, x + 4, y);
      y++;
      x++;
      drawLine(x, y, x + 4, y);
    }
  } else {
    setColor(White);
    fillRect(sceneLine);
  }

  string name   = item->getName();
  int maxWidth  = r2.getLx() - 4;
  TDimension ts = getTextSize(name);
  if (ts.lx > maxWidth) {
    do {
      name.pop_back();
      ts = getTextSize(name);
    } while (name.size() > 1 && ts.lx > maxWidth);
  }

  TPoint textOrigin = r2.getP00() + TPoint(2 - 10, (r2.getLy() - ts.ly) / 2);

  setColor(Black);
  string fname          = name;
  string type           = item->getPath().getType();
  if (type != "") fname = fname + "." + type;

  fname = item->getPath().getLevelName();
  drawText(textOrigin, fname);

  if (selected) {
    TRect r = m_playButtonBox + base;
    setColor(Blue);
    if (m_playing) {
      int x0 = r.x0, x3 = r.x1;
      int x1 = (x0 + x3) / 2 - 1;
      int x2 = x3 - (x1 - x0);
      drawRect(x0, r.y0, x1, r.y1);
      drawRect(x2, r.y0, x3, r.y1);
    } else {
      vector<TPoint> pts;
      int d = (r.y1 - r.y0 + 1) / 2;
      int x = r.x0;
      int y = r.y0;
      pts.push_back(TPoint(x, y));
      pts.push_back(TPoint(x + d, y + d));
      pts.push_back(TPoint(x, y + d * 2));
      fillPolygon(Cyan, Blue, &pts[0], 3);
    }
  }
  assert(0 <= index && index < (int)m_items.size());
  assert(m_items[index]->getRaster());
  assert(m_iconBox.getSize() == m_items[index]->getRaster()->getSize());
  rectwrite(m_items[index]->getRaster(), base + m_iconBox.getP00());
}

//-------------------------------------------------------------------

void ThumbnailViewer::drawBackground() {
  int itemCount   = m_items.size();
  int columnCount = getColumnCount();
  int rowCount    = (itemCount + columnCount - 1) / columnCount;

  TRect clipRect;
  getClipRect(clipRect);  // coordinate window
  clipRect -= TPoint(m_xoff, m_yoff);
  TRect rect;
  rect.x0 = m_margins.lx;
  rect.x1 = m_margins.lx + (m_itemSize.lx + m_itemSpace.lx) * columnCount -
            m_itemSpace.lx - 1;
  rect.y1 = getLy() - m_margins.ly - 1;
  rect.y0 = rect.y1 - (m_itemSize.ly + m_itemSpace.ly) * rowCount +
            m_itemSpace.ly + 1;

  setColor(getBackgroundColor());

  // alto
  if (rect.y1 + 1 <= clipRect.y1)
    fillRect(clipRect.x0, rect.y1 + 1, clipRect.x1, clipRect.y1);

  // basso
  if (clipRect.y0 <= rect.y0 - 1)
    fillRect(clipRect.x0, clipRect.y0, clipRect.x1, rect.y0 - 1);

  int y0 = tmax(clipRect.y0, rect.y0);
  int y1 = tmin(clipRect.y1, rect.y1);
  if (y0 <= y1) {
    // sinistra
    if (clipRect.x0 <= rect.x0 - 1) fillRect(clipRect.x0, y0, rect.x0 - 1, y1);

    // destra
    if (rect.x1 + 1 <= clipRect.x1) fillRect(rect.x1 + 1, y0, clipRect.x1, y1);
  }

  // interlinee verticali
  for (int r = 0; r < rowCount; r++) {
    int y1 = rect.y1 - (m_itemSize.ly + m_itemSpace.ly) * r + m_itemSpace.ly;
    int y0 = y1 - m_itemSpace.ly + 1;
    fillRect(clipRect.x0, y0, clipRect.x1, y1);
  }
  for (int c = 0; c < columnCount - 1; c++) {
    int x0 =
        m_margins.lx + (m_itemSize.lx + m_itemSpace.lx) * c + m_itemSize.lx;
    int x1 = x0 + m_itemSpace.lx - 1;
    fillRect(x0, clipRect.y0, x1, clipRect.y1);
  }

  // ultima riga
  c = itemCount % columnCount;
  if (c != 0) {
    TRect box = getItemBounds(itemCount - 1);
    if (box.y0 <= clipRect.y1 && box.y1 >= clipRect.y0 &&
        box.x1 + 1 <= clipRect.x1)
      fillRect(box.x1 + 1, box.y0, clipRect.x1, box.y1);
  }
}

//-------------------------------------------------------------------

void ThumbnailViewer::repaint() {
  if (m_flag) {
    if (m_selectedItemIndex) drawFrame(m_selectedItemIndex);
  } else {
    // m_loadingDirectory = false;
    // setColor(Black);
    // fillRect(getBounds());

    drawBackground();

    TRect clipRect;
    getClipRect(clipRect);  // coordinate window
    clipRect -= TPoint(m_xoff, m_yoff);

    bool loading = false;
    for (int i = 0; i < (int)m_items.size(); i++) {
      TRect bounds = getItemBounds(i);
      if (bounds.overlaps(clipRect)) {
        drawItem(i);
        if (m_items[i]->isIconLoaded() == false) loading = true;
      }
    }
    if (loading) {
      m_loading = true;
      if (!m_timerActive) {
        m_timerActive = true;
        startTimer(200);
      }
    }
  }
}

//-------------------------------------------------------------------

void ThumbnailViewer::select(int index) {
  assert(index == -1 || 0 <= index && index < (int)m_items.size());
  m_playing = false;
  if (m_timerActive) {
    m_timerActive = false;
    stopTimer();
  }
  Thumbnail *thumbnail;
  if (m_selectedItemIndex >= 0) {
    thumbnail = m_items[m_selectedItemIndex];
    thumbnail->setPlaying(false);
  }
  m_selectedItemIndex = index;
  onSelect(m_selectedItemIndex);
}

//-------------------------------------------------------------------

void ThumbnailViewer::startPlaying() {
  if (m_playing) return;
  if (m_selectedItemIndex < 0) return;
  m_playing = true;
  assert(0 <= m_selectedItemIndex && m_selectedItemIndex < (int)m_items.size());
  m_items[m_selectedItemIndex]->setPlaying(true);
  if (!m_timerActive) {
    m_timerActive = true;
    startTimer(200);
  }
}

//-------------------------------------------------------------------

void ThumbnailViewer::stopPlaying() {
  if (!m_playing) return;
  m_playing = false;
  if (m_selectedItemIndex >= 0) {
    assert(0 <= m_selectedItemIndex &&
           m_selectedItemIndex < (int)m_items.size());
    m_items[m_selectedItemIndex]->gotoFrame(0);
    m_items[m_selectedItemIndex]->setPlaying(false);
  }
  if (m_timerActive) {
    m_timerActive = false;
    stopTimer();
  }
}

//-------------------------------------------------------------------

void ThumbnailViewer::leftButtonDown(const TMouseEvent &e) {
  TPoint pos      = e.m_pos - TPoint(m_xoff, m_yoff);
  m_oldPos        = e.m_pos;
  m_dragDropArmed = false;

  m_nameField->hide();
  //  if(setGeometry(m_textBox + bounds.getP00());
  //           m_nameField->setText(m_items[index]->getName());
  //           m_nameField->show();

  int index = findItem(pos);
  if (index >= 0) {
    if (m_selectedItemIndex == index) {
      TRect bounds = getItemBounds(index);
      pos -= bounds.getP00();
      if (m_playButtonBox.contains(pos)) {
        if (!m_playing)
          startPlaying();
        else
          stopPlaying();
        invalidate();
      } else if (m_textBox.contains(pos)) {
        stopPlaying();
        m_nameField->setGeometry(m_textBox + bounds.getP00() + TPoint(-10, 0));
        m_nameField->setThumbnail(m_items[index]);
        m_nameField->show();
        setFocusOwner(m_nameField);
      } else {
        m_dragDropArmed = true;
      }
    } else {
      select(index);
      m_dragDropArmed = true;
    }
  } else
    select(-1);
  invalidate();
}

//-------------------------------------------------------------------

void ThumbnailViewer::leftButtonDrag(const TMouseEvent &e) {
  if (!m_dragDropArmed || m_selectedItemIndex < 0) return;
  if (norm2(e.m_pos - m_oldPos) < 100) return;
  m_dragDropArmed = false;

  Thumbnail *item = m_items[m_selectedItemIndex];
  TFilePath path  = item->getPath();
  if (path.getWideString() != toWideString("")) {
    TDropSource dropSource;
    std::vector<std::string> v;
    v.push_back(toString(path.getWideString()));
    TDataObject data(v);
    dropSource.doDragDrop(data);
  }
}

//-------------------------------------------------------------------

void ThumbnailViewer::leftButtonUp(const TMouseEvent &e) {
  m_dragDropArmed = false;
}

//-------------------------------------------------------------------

void ThumbnailViewer::leftButtonDoubleClick(const TMouseEvent &e) {
  TPoint pos = e.m_pos - TPoint(m_xoff, m_yoff);
  m_nameField->hide();
  int index = findItem(pos);
  if (index >= 0) onDoubleClick(index);
}

//-------------------------------------------------------------------

void ThumbnailViewer::onTimer(int) {
  stopTimer();
  m_timerActive = false;
  if (m_playing && m_selectedItemIndex >= 0) {
    Thumbnail *thumbnail = m_items[m_selectedItemIndex];
    bool ret             = thumbnail->nextFrame();
    if (ret) {
      m_timerActive = true;
      startTimer(100);
    } else {
      thumbnail->gotoFrame(0);
      thumbnail->setPlaying(false);
      m_playing = false;
    }
    invalidate();
  } else if (m_loading) {
    for (int i = 0; i < (int)m_items.size(); i++)
      if (!m_items[i]->isIconLoaded()) {
        m_items[i]->loadIcon();
        invalidate();
        return;
      }
  }
}

//-------------------------------------------------------------------

void ThumbnailViewer::addItem(Thumbnail *item) { m_items.push_back(item); }

//-------------------------------------------------------------------

void ThumbnailViewer::removeItem(Thumbnail *item) {
  std::vector<Thumbnail *>::iterator it =
      std::find(m_items.begin(), m_items.end(), item);
  if (it != m_items.end()) m_items.erase(it);

  m_selectedItemIndex = -1;
}

//-------------------------------------------------------------------

void ThumbnailViewer::clearItems() {
  clearPointerContainer(m_items);
  m_selectedItemIndex = -1;
  m_playing           = false;
  m_loading           = false;
  if (m_timerActive) {
    stopTimer();
    m_timerActive = false;
  }
}

//-------------------------------------------------------------------

void ThumbnailViewer::loadDirectory(const TFilePath &dirPath,
                                    const vector<string> &fileTypes) {
  clearItems();
  TFilePathSet fps;

  try {
    fps = TSystem::readDirectory(dirPath);
  } catch (...) {
    TMessage::error(toString(dirPath.getWideString()) +
                    ": can't read directory");
    updateContentSize();
    return;
  }

  fps = TSystem::packLevelNames(fps);
  fps.sort();

  for (TFilePathSet::iterator it = fps.begin(); it != fps.end(); it++) {
    try {
      TFilePath fp = *it;
      if (fp.getType() == "bmp" && fp.getName().find("_icon") != string::npos)
        continue;
      if (std::find(fileTypes.begin(), fileTypes.end(), fp.getType()) ==
          fileTypes.end())
        continue;
      Thumbnail *item = FileThumbnail::create(m_iconBox.getSize(), fp);
      if (item) m_items.push_back(item);
    } catch (...) {
      TMessage::error(toString(it->getWideString()) + ": can't read thumbnail");
    }
  }
  updateContentSize();
}

//-------------------------------------------------------------------

void ThumbnailViewer::updateContentSize() {
  int itemCount   = m_items.size();
  int columnCount = getColumnCount();
  int rowCount    = (itemCount + columnCount - 1) / columnCount;

  TRect rect;
  int yrange = rowCount * (m_itemSize.ly + m_itemSpace.ly) - m_itemSpace.ly +
               m_margins.ly * 2;
  setContentSize(TDimension(getLx(), yrange));
}

//-------------------------------------------------------------------

void ThumbnailViewer::scrollPage(int y) { doScrollTo_y(tmax(y, 0)); }
