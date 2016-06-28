

#include "toonzqt/styleindexlineedit.h"
#include "toonzqt/gutil.h"
#include "toonz/tpalettehandle.h"
#include "tcolorstyles.h"
#include "tpalette.h"

#include <QPainter>

using namespace DVGui;

//--------------------------------------------------------------------

StyleIndexLineEdit::StyleIndexLineEdit() : m_pltHandle(0) {
  // style index will not be more than 4096, but a longer text 
  // "current" may be input instead of style id + chip width + margin 
  setMaximumWidth(fontMetrics().width("current") + 30);
  setFixedHeight(20);
}

//--------------------------------------------------------------------

StyleIndexLineEdit::~StyleIndexLineEdit() {}

//--------------------------------------------------------------------

void StyleIndexLineEdit::paintEvent(QPaintEvent *pe) {
  QLineEdit::paintEvent(pe);

  TColorStyle *style;
  if (QString("current").contains(text()))
    style = m_pltHandle->getStyle();
  else {
    int index = text().toInt();
    style     = m_pltHandle->getPalette()->getStyle(index);
  }

  if (style) {
    QPainter p(this);
    int w = width();
    QRect chipRect(w - 18, 3, 14, 14);
    TRaster32P icon = style->getIcon(qsize2Dimension(chipRect.size()));
    p.drawPixmap(chipRect.left(), chipRect.top(), rasterToQPixmap(icon));
    p.setPen(Qt::black);
    p.drawRect(chipRect);
  }
}
