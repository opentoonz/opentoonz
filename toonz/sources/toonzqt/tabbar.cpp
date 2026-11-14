

#include "toonzqt/tabbar.h"
#include "toonzqt/dvdialog.h"

#include <QPainter>

using namespace DVGui;

//=============================================================================
// TabBar
//-----------------------------------------------------------------------------

TabBar::TabBar(QWidget *parent) : QTabBar(parent) {}

//-----------------------------------------------------------------------------

TabBar::~TabBar() { m_pixmaps.clear(); }

//-----------------------------------------------------------------------------

void TabBar::paintEvent(QPaintEvent *event) {
  QTabBar::paintEvent(event);

  QPainter p(this);

  // Paint tab Icons
  int tabCount      = count();
  int selectedIndex = currentIndex();
  for (int i = 0; i < tabCount; i++) {
    QRect tabRectangle = tabRect(i).adjusted(2, 1, -4, 0);

    // Written by Claude Opus 5.5, requires maintainer code review.
    // Guard against tabs without a pixmap pair (issue #6917): reading past
    // the end of m_pixmaps crashed in QPixmap::isNull().
    int pixmapIndex = i * 2;
    if (selectedIndex == i) {
      tabRectangle = tabRectangle.adjusted(0, -2, 0, 0);
      pixmapIndex++;
    }
    if (pixmapIndex >= (int)m_pixmaps.size() || m_pixmaps[pixmapIndex].isNull())
      continue;
    p.drawPixmap(tabRectangle.x(), tabRectangle.y(), m_pixmaps[pixmapIndex]);
  }
}

//-----------------------------------------------------------------------------

void TabBar::addIconTab(const char *iconPrefixName, const QString &tooltip) {
  QString normal = QString(":Resources/") + iconPrefixName + "_off.svg";
  QString over   = QString(":Resources/") + iconPrefixName + "_on.svg";

  // Push the pixmaps first so a repaint triggered by addTab() never sees
  // more tabs than pixmap pairs.
  m_pixmaps.push_back(QPixmap(normal));
  m_pixmaps.push_back(QPixmap(over));
  int indexTab = addTab("");
  setTabToolTip(indexTab, tooltip);
}

//-----------------------------------------------------------------------------

void TabBar::addSimpleTab(const QString &text) {
  m_pixmaps.push_back(QPixmap(""));
  m_pixmaps.push_back(QPixmap(""));
  addTab(text);
}
//-----------------------------------------------------------------------------

void TabBar::clearTabBar() {
  int i, n = count();
  for (i = 0; i < n; i++) removeTab(0);
  if (!m_pixmaps.empty()) m_pixmaps.clear();
}
