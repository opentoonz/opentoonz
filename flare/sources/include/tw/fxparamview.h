#pragma once

#ifndef TNZ_FXPARAMVIEW_INCLUDED
#define TNZ_FXPARAMVIEW_INCLUDED

#include "tw/tw.h"
#include "tfx.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TFx;
class TGenericCommandAction;
class TFxParamViewData;

class DVAPI TFxParamView : public TWidget, public TFxObserver {
  TFxParamViewData *m_data;

public:
  TFxParamView(TWidget *parent, string name = "fxParamView");
  ~TFxParamView();

  void setFx(TFx *fx, const wstring &fxCaption);

  // TFxObserver methods
  void onChange(const TFxChange &);
  void onChange(const TFxParamAdded &change);
  void onChange(const TFxParamRemoved &change);

  void configureNotify(const TDimension &d);
  void repaint();

  void select(TParam *param, bool on);

  // PROVVISORIO
  void addAction(TGenericCommandAction *action);
  void triggerActions();

  void setCurrentFrame(int frame);
  int getCurrentFrame() const;
};

#endif
