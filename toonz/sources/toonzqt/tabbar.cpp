

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

    if (selectedIndex == i) {
      tabRectangle = tabRectangle.adjusted(0, -2, 0, 0);
      if (!m_pixmaps[(i * 2) + 1]) continue;
      p.drawPixmap(tabRectangle.x(), tabRectangle.y(), m_pixmaps[(i * 2) + 1]);
    } else {
      if (!m_pixmaps[(i * 2)]) continue;
      p.drawPixmap(tabRectangle.x(), tabRectangle.y(), m_pixmaps[(i * 2)]);
    }
  }
}

//-----------------------------------------------------------------------------

void TabBar::addIconTab(const char *iconPrefixName, const QString &tooltip) {
  QString normal = QString(":Resources/") + iconPrefixName + "_off.svg";
  QString over   = QString(":Resources/") + iconPrefixName + "_on.svg";

  int indexTab = addTab("");
  setTabToolTip(indexTab, tooltip);
  m_pixmaps.push_back(QPixmap(normal));
  m_pixmaps.push_back(QPixmap(over));
}

//-----------------------------------------------------------------------------

void TabBar::addSimpleTab(const QString &text) {
  addTab(text);
  m_pixmaps.push_back(QPixmap(""));
  m_pixmaps.push_back(QPixmap(""));
}
//-----------------------------------------------------------------------------

void TabBar::clearTabBar() {
  int i, n = count();
  for (i = 0; i < n; i++) removeTab(0);
  if (!m_pixmaps.empty()) m_pixmaps.clear();
}
