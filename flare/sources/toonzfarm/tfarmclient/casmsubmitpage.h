#pragma once

#ifndef CASMSUBMITPAGE_H
#define CASMSUBMITPAGE_H

#include "tabPage.h"

class CasmSubmitPage : public TabPage {
public:
  CasmSubmitPage(TWidget *parent);
  ~CasmSubmitPage();

  void configureNotify(const TDimension &size);

  void onActivate();
  void onDeactivate();

private:
  class Data;
  Data *m_data;
};

#endif
