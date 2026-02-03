#pragma once

#ifndef TNZ_LABEL_INCLUDED
#define TNZ_LABEL_INCLUDED

#include "tw/tw.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TLabel : public TWidget {
  wstring m_text;
  bool m_border;
  Alignment m_alignment;

public:
  TLabel(TWidget *parent, string name = "label");
  void draw();

  void setText(string s);
  void setText(wstring s);

  void setBorder(bool b) { m_border = b; };
  void setAlignment(Alignment a) { m_alignment = a; };
};

#endif
