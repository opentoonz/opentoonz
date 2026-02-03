

#include "textlist.h"
#include "tw/colors.h"
#include "tw/event.h"
#include "tw/scrollbar.h"
#include "tw/keycodes.h"

#include <vector>
#include <map>
#include <string>

using namespace std;
using namespace TwConsts;

namespace {
const int rowHeight = 20;  // 15
};

//==============================================================================

class TTextList::Data {
public:
  Data(TWidget *w)
      : m_w(w)
      , m_selAction(0)
      , m_dblClickAction(0)
      , m_scrollbar(new TScrollbar(w))
      , m_yoffset(0) {}

  ~Data() {
    if (m_selAction) delete m_selAction;

    if (m_dblClickAction) delete m_dblClickAction;

    std::map<string, TTextListItem *>::iterator it = m_items.begin();
    for (; it != m_items.end(); ++it) delete it->second;
  }

  int posToItem(const TPoint &p);
  void updateScrollBarStatus() {
    if (!m_scrollbar) return;
    unsigned int ly = m_w->getLy();

    if ((m_items.size() * rowHeight) > ly) {
      m_scrollbar->setValue(m_yoffset, 0, (m_items.size() + 1) * rowHeight - ly,
                            ly);
      m_scrollbar->show();  // m_scrollbar->setValue(m_yoffset,0, yrange-ly,
                            // ly);
    } else {
      m_yoffset = 0;
      m_scrollbar->hide();  // m_scrollbar->setValue(0,0, 0, 0);
    }
    m_scrollbar->invalidate();
  }

  TWidget *m_w;
  std::map<string, TTextListItem *> m_items;
  vector<string> m_selectedItems;

  TGenericTextListAction *m_selAction;
  TGenericTextListAction *m_dblClickAction;
  TScrollbar *m_scrollbar;
  int m_yoffset;
};

//------------------------------------------------------------------------------

int TTextList::Data::posToItem(const TPoint &p) {
  /*  TDimension d = getSize();
int i = getItemCount()-1;

10 +
for(int y=10; y<d.ly && i>=0; y+=rowHeight, --i)
*/

  int y    = m_w->getSize().ly - p.y + m_yoffset;
  int item = y / rowHeight;
  return item;
}

//==============================================================================

TTextListItem::TTextListItem(const string &id, const string &caption)
    : m_id(id), m_caption(caption) {}

//==============================================================================

TTextList::TTextList(TWidget *parent, string name)
    : TWidget(parent, name), m_data(0) {
  m_data = new Data(this);
  m_data->m_scrollbar->setAction(
      new TScrollbarAction<TTextList>(this, &TTextList::scrollTo));
}

//------------------------------------------------------------------------------

TTextList::~TTextList() { delete m_data; }

//------------------------------------------------------------------------------

void TTextList::addItem(TTextListItem *item) {
  std::map<string, TTextListItem *>::iterator it =
      m_data->m_items.find(item->getId());

  if (it == m_data->m_items.end()) {
    m_data->m_items.insert(make_pair(item->getId(), item));
    m_data->updateScrollBarStatus();
  }
}

//------------------------------------------------------------------------------

void TTextList::removeItem(const string &itemId) {
  std::map<string, TTextListItem *>::iterator it = m_data->m_items.find(itemId);
  if (it != m_data->m_items.end()) {
    m_data->m_items.erase(it);
    m_data->updateScrollBarStatus();
  }
}

//------------------------------------------------------------------------------

void TTextList::clearAll() {
  m_data->m_items.clear();
  m_data->m_selectedItems.clear();
  invalidate();
  m_data->updateScrollBarStatus();
}

//------------------------------------------------------------------------------

int TTextList::getItemCount() const { return m_data->m_items.size(); }

//------------------------------------------------------------------------------

TTextListItem *TTextList::getItem(int i) const {
  if (i >= 0 && i < (int)m_data->m_items.size()) {
    std::map<string, TTextListItem *>::iterator it = m_data->m_items.begin();
    advance(it, i);
    return it->second;
  }
  return 0;
}

//------------------------------------------------------------------------------

int TTextList::itemToIndex(const string &itemId) {
  std::map<string, TTextListItem *>::iterator it = m_data->m_items.find(itemId);

  if (it == m_data->m_items.end())
    return -1;
  else
    return distance(m_data->m_items.begin(), it);
}

//------------------------------------------------------------------------------

int TTextList::getSelectedItemCount() const {
  return m_data->m_selectedItems.size();
}

//------------------------------------------------------------------------------

TTextListItem *TTextList::getSelectedItem(int i) const {
  if (i < 0 || (int)m_data->m_selectedItems.size() <= i) return 0;

  string itemId = m_data->m_selectedItems[i];
  std::map<string, TTextListItem *>::iterator it = m_data->m_items.find(itemId);
  if (it != m_data->m_items.end())
    return it->second;
  else
    return 0;
}

//------------------------------------------------------------------------------

string TTextList::getSelectedItemId(int i) const {
  if ((int)m_data->m_selectedItems.size() <= i) return "";
  assert(i >= 0 && i < (int)m_data->m_selectedItems.size());
  return m_data->m_selectedItems[i];
}
//------------------------------------------------------------------------------

void TTextList::select(int i, bool on) {
  assert(i >= 0 && i < (int)m_data->m_items.size());

  std::map<string, TTextListItem *>::iterator it = m_data->m_items.begin();
  advance(it, i);

  string id = it->first;
  vector<string>::iterator it2 =
      find(m_data->m_selectedItems.begin(), m_data->m_selectedItems.end(), id);

  if (on) {
    if (it2 == m_data->m_selectedItems.end())
      m_data->m_selectedItems.push_back(id);
  } else {
    if (it2 != m_data->m_selectedItems.end())
      m_data->m_selectedItems.erase(it2);
  }

  if (m_data->m_selAction) m_data->m_selAction->sendCommand(i);

  invalidate();
}

//------------------------------------------------------------------------------

void TTextList::select(const string &itemId, bool on) {
  std::map<string, TTextListItem *>::iterator it = m_data->m_items.find(itemId);

  if (it != m_data->m_items.end()) {
    int i = distance(m_data->m_items.begin(), it);
    select(i, on);
  }
}

//------------------------------------------------------------------------------

void TTextList::unselectAll() {
  m_data->m_selectedItems.clear();
  invalidate();
}

//------------------------------------------------------------------------------

bool TTextList::isSelected(int i) const {
  assert(i >= 0 && i < (int)m_data->m_items.size());

  std::map<string, TTextListItem *>::iterator it = m_data->m_items.begin();
  advance(it, i);

  string id = it->first;

  vector<string>::iterator it2 =
      find(m_data->m_selectedItems.begin(), m_data->m_selectedItems.end(), id);

  return it2 != m_data->m_selectedItems.end();
}

//------------------------------------------------------------------------------

bool TTextList::isSelected(const string &item) const {
  assert(false);
  return false;
}

//------------------------------------------------------------------------------

void TTextList::scrollTo(int y) {
  y = (y / rowHeight) * rowHeight;
  if (m_data->m_yoffset == y) return;
  m_data->m_yoffset = y;
  m_data->updateScrollBarStatus();
  invalidate();
}

//------------------------------------------------------------------------------

void TTextList::draw() {
  drawRect(TRect(TPoint(0, 0), getSize()));
  TDimension d = getSize();

  int y = d.ly - 1 - rowHeight;
  for (int i = m_data->m_yoffset / rowHeight; i < getItemCount() && y >= 0;
       ++i, y -= rowHeight) {
    if (isSelected(i)) {
      setColor(Blue, 2);
      fillRect(2, y - 2, getSize().lx - 1 - 2, y + rowHeight - 1 - 2);
    }

    setColor(Black);
    drawText(TPoint(10, y), getItem(i)->getCaption());
  }
}

//------------------------------------------------------------------------------

void TTextList::setSelAction(TGenericTextListAction *action) {
  if (m_data->m_selAction) delete m_data->m_selAction;

  m_data->m_selAction = action;
}

//------------------------------------------------------------------------------

void TTextList::setDblClickAction(TGenericTextListAction *action) {
  if (m_data->m_dblClickAction) delete m_data->m_dblClickAction;

  m_data->m_dblClickAction = action;
}

//------------------------------------------------------------------------------

void TTextList::configureNotify(const TDimension &d) {
  m_data->m_scrollbar->setGeometry(d.lx - 20, 1, d.lx - 2, d.ly - 2);
  m_data->updateScrollBarStatus();
}

//------------------------------------------------------------------------------

void TTextList::leftButtonDown(const TMouseEvent &e) {
  int i = m_data->posToItem(e.m_pos);
  if (i >= 0 && i < (int)m_data->m_items.size()) {
    if (!e.isShiftPressed()) unselectAll();
    select(i, !isSelected(i));
  }
}

//------------------------------------------------------------------------------

void TTextList::leftButtonDoubleClick(const TMouseEvent &e) {
  int i = m_data->posToItem(e.m_pos);
  if (i >= 0 && i < (int)m_data->m_items.size()) {
    if (m_data->m_dblClickAction) m_data->m_dblClickAction->sendCommand(i);
  }
}

//------------------------------------------------------------------------------

void TTextList::keyDown(int key, unsigned long mod, const TPoint &pos) {
  if ((key == TK_UpArrow) || (key == TK_DownArrow)) {
    int lastSelected = -1;
    for (int i                        = 0; i < getItemCount(); ++i)
      if (isSelected(i)) lastSelected = i;
    if (lastSelected == -1) return;
    int newSelected = (key == TK_UpArrow) ? lastSelected - 1 : lastSelected + 1;

    if (newSelected >= 0 && newSelected < (int)m_data->m_items.size()) {
      //   if (!e.isShiftPressed())
      unselectAll();
      select(newSelected, !isSelected(newSelected));
    }
  } else
    TWidget::keyDown(key, mod, pos);
}
