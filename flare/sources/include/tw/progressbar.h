#pragma once

#ifndef PROGRESSBAR_INCLUDED
#define PROGRESSBAR_INCLUDED

#include "tw/popup.h"

class DVAPI TProgressBar : public TPopup {
  class Imp;
  Imp *m_data;

public:
  TProgressBar(TWidget *parent, string name = "progress bar",
               wstring text = L"");
  ~TProgressBar();

  void popup();
  // se ritorna false vuol dire che l'utente ha premuto il bottone
  bool changeFraction(int num, int den);

  void closePopup();

  void configureNotify(const TDimension &d);

  TDimension getPreferredSize() const;
  void setPreferredSize(const TDimension &d);
  void setPreferredSize(int lx, int ly) {
    setPreferredSize(TDimension(lx, ly));
  }

  bool onNcPaint(bool is_active, const TDimension &window_size,
                 const TRect &caption_rect);
};

#endif
