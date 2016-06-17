#pragma once

#ifndef TNZ_FILEBROWSER_INCLUDED
#define TNZ_FILEBROWSER_INCLUDED

#include "tw/treeview.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TFileBrowser : public TTreeView {
public:
  TFileBrowser(TWidget *parent, string name = "fileBrowser");
};

#endif
