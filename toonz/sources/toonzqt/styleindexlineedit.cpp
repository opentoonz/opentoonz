

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
  int currentWidth = std::max(fontMetrics().width("current"),
                              fontMetrics().width(tr("current")));
  setMaximumWidth(currentWidth + 30);
  setFixedHeight(20);
}

//--------------------------------------------------------------------

StyleIndexLineEdit::~StyleIndexLineEdit() {}

//--------------------------------------------------------------------

void StyleIndexLineEdit::paintEvent(QPaintEvent *pe) {
  QLineEdit::paintEvent(pe);

  if (!m_pltHandle->getPalette()) return;

  TColorStyle *style;
  // Aware of both "current" and translated string
  if (QString("current").contains(text()) || tr("current").contains(text()))
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
