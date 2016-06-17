#pragma once

#ifndef TNZ_ITEMBROWSER_INCLUDED
#define TNZ_ITEMBROWSER_INCLUDED

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

class TFilePath;

class DVAPI TItemBrowser : public TWidget {
  class Data;
  Data *m_data;

public:
  TItemBrowser(TWidget *parent, const TFilePath &rootDir,
               string name = "ItemBrowser");
  ~TItemBrowser();

  void setBorder(int dx, int dy);

  void setItemSize(const TDimension &d);
  TDimension getItemSize() const;

  virtual int getItemCount() const = 0;
  virtual void drawItem(TWidget *w, const TRect &rect, int index,
                        bool selected) const = 0;
  virtual bool isItemWide(int index) const { return false; };
  virtual void loadItems(const TFilePath &rootDir) {}

  virtual void onSelect(int index) {}

  void select(int index);
  void notifyItemCountChange();

  void configureNotify(const TDimension &);

  void update();

  virtual void onKey(int key, int index){};
  virtual wstring getTooltipString(int) { return wstring(); }
  virtual string getContextHelpReference(int) { return string(); }
};

#endif
