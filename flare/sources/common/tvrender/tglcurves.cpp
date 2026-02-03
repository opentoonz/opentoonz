

#include "tgl.h"
//#include "tvectorgl.h"
//#include "tstroke.h"
#include "tstrokeprop.h"
//#include "tpalette.h"
#include "tvectorrenderdata.h"
//#include "tpalette.h"
//#include "tcolorstyles.h"

//=============================================================================
#ifdef SPOSTATOINTGLREGIONS
void tglDraw(const TVectorRenderData &rd, const TStroke *s) {
  assert(s);
  if (!s) return;

  const TStroke &stroke = *s;

  // initialize information for aaliasing
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);

  // it's necessary to do line function of pixel size ?
  glLineWidth((float)1.0);

  assert(rd.m_palette);
  TStrokeProp *prop = stroke.getProp(/*rd.m_palette*/);
  /////questo codice stava dentro tstroke::getprop/////////
  TColorStyle *style =
      rd.m_palette->getStyle(stroke->getStyle() /*m_imp->m_styleId*/);
  if (!style->isStrokeStyle() || style->isEnabled() == false) {
    prop = 0;
  } else {
    if (!prop || style != prop->getColorStyle()) {
      stroke->setProp(style->makeStrokeProp(stroke));
      prop = stroke->getProp();
    }
  }
  ///
  //---------------------
  if (prop) prop->draw(rd);
  //---------------------

  tglColor(TPixel32::White);

  glDisable(GL_BLEND);
  glDisable(GL_LINE_SMOOTH);
}
#endif
//-----------------------------------------------------------------------------
