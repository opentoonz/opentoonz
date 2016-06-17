#pragma once

#ifndef TNZ_FXBROWSER_INCLUDED
#define TNZ_FXBROWSER_INCLUDED

#include "tw/tw.h"
#include "tw/treeview.h"
//#include "tfilepath.h"

// forward declaration
class TFx;
class TFilePath;

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//--------------------------------------------------------------------

class FxBuilder {
public:
  virtual ~FxBuilder() {}
  virtual TFx *create() = 0;
};

//--------------------------------------------------------------------

class DVAPI TFxBrowser : public TTreeView {
public:
  class DoubleClickListener {
  public:
    virtual void onDoubleClick() = 0;
    virtual ~DoubleClickListener() {}
  };

  class FxLoader;
  class MacroFxFolderItem;

private:
  string m_selectedFxId;
  DoubleClickListener *m_doubleClickListener;
  FxLoader *m_fxLoader;
  MacroFxFolderItem *m_macroFxFolderItem;

public:
  TFxBrowser(TWidget *parent, const TFilePath &fxListPath,
             const TFilePath &fxPresetFolder, string name = "fxBrowser");

  ~TFxBrowser();

  string getSelectedFxId() const;
  FxBuilder *getSelectedFxBuilder() const;

  void onSelect(TTreeViewItem *item);
  void leftButtonDoubleClick(const TMouseEvent &e);
  void rightButtonDown(const TMouseEvent &) {}
  void mouseWheel(const TMouseEvent &, int wheel);

  void setDoubleClickListener(DoubleClickListener *listener);

  void updatePresets();
};

#endif
