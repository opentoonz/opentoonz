#pragma once

#ifndef STYLE_PICKER_H
#define STYLE_PICKER_H

// #include "timage.h"
#include "tcommon.h"
#include "tpalette.h"

class TStroke;
class QWidget;

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI StylePicker {
  TImageP m_image;
  TPaletteP m_palette;
  const QWidget *m_widget;

public:
  StylePicker(const QWidget *parent) : m_widget(parent) {}

  // usa come palette la palette dell'immagine
  StylePicker(const QWidget *parent, const TImageP &image);

  // palette esterna (ad es. se image e' di tipo raster)
  StylePicker(const QWidget *parent, const TImageP &image,
              const TPaletteP &palette);

  // pickStyleId(point, radius)
  //
  // point e' espresso in inches (eventualmente utilizzando il dpi
  // dell'immagine)
  // point == (0,0) indica il centro dell'immagine
  //
  // immagini tzp:
  //   ritorna l'indice del colore che si trova nel pixel individuato da point
  //   (radius viene ignorato)
  //   se tone<maxtone/2 ritorna l'inchiostro altrimenti il paint
  //
  // immagini pli
  //   ritorna l'indice del colorstyle dello stroke che si trova
  //   piu' vicini a point (ma non oltre radius) o dell'area fillata che
  //   comprende point
  //
  // immagine fullcolor
  //   ritorna l'indice del colorstyle della palette fornita nel costruttore
  //   il cui maincolor e' piu' vicino al colore del pixel individuato da point
  //   (radius viene ignorato)
  //
  // Nota: se non trova niente ritorna -1

  /*-- (StylePickerTool内で)LineとAreaを切り替えてPickできる。mode: 0=Area,
   * 1=Line, 2=Line&Areas(default)  --*/
  int pickStyleId(const TPointD &point, double radius, double scale2,
                  int mode = 2) const;

  /*--- Toonz Raster LevelのToneを拾う。 ---*/
  int pickTone(const TPointD &pos) const;

  // per pli come sopra, ma ritorna il maincolor
  // per tzp e fullcolor ritorna il colore effettivo del pixel
  TPixel32 pickColor(const TPointD &point, double radius, double scale2) const;
  TPixel64 pickColor16(const TPointD &point, double radius,
                       double scale2) const;
  TPixelF pickColor32F(const TPointD &point, double radius,
                       double scale2) const;
  TPixel32 pickAverageColor(const TRectD &rect) const;
  TPixel64 pickAverageColor16(const TRectD &rect) const;
  TPixelF pickAverageColor32F(const TRectD &rect) const;

  // ritorna il colore medio presente nell'area della finestra corrente openGL
  TPixel32 pickColor(const TRectD &area) const;

  // ritorna il colore medio presente nell'area interna all stroke della
  // finestra corrente openGL
  TPixel32 pickColor(TStroke *stroke) const;

  // helper function. conversione di coordinate:
  // p e' nel sist. di rif. descritto sopra
  // il valore di ritorno si riferisce all'eventuale raster: (0,0)==leftbottom
  TPoint getRasterPoint(const TPointD &p) const;
};
#endif
